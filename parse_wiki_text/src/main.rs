use anyhow::{anyhow, Context, Result};
use clap::Parser;
use parse_wiki_text::{
    Configuration, DefinitionListItem, DefinitionListItemType, ListItem, Node, Output, Parameter,
    TableCaption, TableCell, TableCellType, TableRow,
};
use serde_json::{json, Value};
use std::fs;
use std::io::{self, BufRead, BufWriter, Write};
use std::path::{Path, PathBuf};
use std::time::Instant;

#[derive(Debug, Parser)]
#[command(
    name = "parse-wiki-text-export",
    version,
    about = "Export parse_wiki_text AST as encoded strings with original input"
)]
struct Args {
    #[arg(value_name = "FILE", help = "Plain wikitext files to parse and export")]
    input_files: Vec<PathBuf>,

    #[arg(
        long = "input-json",
        value_name = "JSON_FILE",
        help = "JSON sample file(s) containing wikitext strings"
    )]
    input_json_files: Vec<PathBuf>,

    #[arg(
        long = "wikitext-dir",
        value_name = "DIR",
        help = "Directory of wikitext files (top-level files only, no subdirectories)"
    )]
    wikitext_dir: Option<PathBuf>,

    #[arg(
        long = "jsonl-stdin",
        default_value_t = false,
        help = "Read JSONL records from STDIN and write augmented JSONL to STDOUT"
    )]
    jsonl_stdin: bool,

    #[arg(
        long,
        value_name = "OUT_DIR",
        help = "Optional output directory (defaults to the input file directory)"
    )]
    out_dir: Option<PathBuf>,

    #[arg(
        long,
        default_value = "/tmp/wikitext_perf_pwt.txt",
        value_name = "PERF_LOG",
        help = "Performance log path"
    )]
    perf_log: PathBuf,

    #[arg(
        long,
        default_value = "pwt",
        value_name = "SUITE",
        help = "Suite label used in perf logging"
    )]
    suite: String,
}

#[derive(Debug, Clone)]
struct SampleInput {
    label: String,
    wikitext: String,
}

#[derive(Debug, Clone, Copy)]
struct Timing {
    parse_ms: u128,
    encode_ms: u128,
}

fn main() -> Result<()> {
    let args = Args::parse();

    if args.jsonl_stdin {
        process_jsonl_stdin(&args)?;
        return Ok(());
    }

    if args.input_files.is_empty() && args.input_json_files.is_empty() && args.wikitext_dir.is_none() {
        return Err(anyhow!(
            "no inputs provided; pass FILE arguments and/or --input-json files and/or --wikitext-dir or --jsonl-stdin"
        ));
    }

    if let Some(out_dir) = &args.out_dir {
        fs::create_dir_all(out_dir)
            .with_context(|| format!("failed to create output directory: {}", out_dir.display()))?;
    }

    let mut pass_count = 0usize;
    let mut fail_count = 0usize;

    for plain_file in &args.input_files {
        match process_plain_file(plain_file, &args) {
            Ok(_) => pass_count += 1,
            Err(e) => {
                fail_count += 1;
                eprintln!("FAIL [{}]: {}", plain_file.display(), e);
            }
        }
    }

    for json_file in &args.input_json_files {
        match process_json_suite_file(json_file, &args) {
            Ok(_) => pass_count += 1,
            Err(e) => {
                fail_count += 1;
                eprintln!("FAIL [{}]: {}", json_file.display(), e);
            }
        }
    }

    if let Some(dir) = &args.wikitext_dir {
        match process_wikitext_dir(dir, &args) {
            Ok(n) => pass_count += n,
            Err(e) => {
                fail_count += 1;
                eprintln!("FAIL [{}]: {}", dir.display(), e);
            }
        }
    }

    println!(
        "Completed parse_wiki_text export: {} passed, {} failed",
        pass_count, fail_count
    );

    if fail_count > 0 {
        return Err(anyhow!("one or more exports failed"));
    }

    Ok(())
}

fn process_jsonl_stdin(args: &Args) -> Result<()> {
    let stdin = io::stdin();
    let mut stdout = BufWriter::new(io::stdout().lock());

    let mut processed = 0usize;
    let mut failures = 0usize;

    for (line_no, line_result) in stdin.lock().lines().enumerate() {
        let line = line_result.with_context(|| format!("failed reading stdin line {}", line_no + 1))?;
        let trimmed = line.trim();
        if trimmed.is_empty() {
            continue;
        }

        let mut value: Value = match serde_json::from_str(trimmed) {
            Ok(v) => v,
            Err(e) => {
                failures += 1;
                eprintln!("line {}: invalid JSONL: {}", line_no + 1, e);
                continue;
            }
        };

        let label = extract_record_label(&value, processed + 1);
        let maybe_wikitext = extract_record_wikitext(&value);

        let section = if let Some(wikitext) = maybe_wikitext {
            let parse_start = Instant::now();
            let parsed = Configuration::default().parse(&wikitext);
            let parse_ms = parse_start.elapsed().as_millis();

            let encode_start = Instant::now();
            let ast_text = encode_parse_wiki_text_ast(&parsed);
            let encode_ms = encode_start.elapsed().as_millis();

            let warnings_json = warnings_to_value(&parsed);
            let section = json!({
                "parseMs": parse_ms,
                "encodeMs": encode_ms,
                "nodeCount": parsed.nodes.len(),
                "warningCount": parsed.warnings.len(),
                "ast": ast_text,
                "warnings": warnings_json
            });

            append_perf_line(
                &args.perf_log,
                &args.suite,
                processed + 1,
                &label,
                Timing { parse_ms, encode_ms },
                parsed.nodes.len(),
                Path::new("stdout"),
            )?;

            section
        } else {
            failures += 1;
            json!({
                "error": "record does not contain wikitext/input/text/content or array[3] text",
                "parseMs": 0,
                "encodeMs": 0,
                "nodeCount": 0,
                "warningCount": 0,
                "ast": "",
                "warnings": []
            })
        };

        attach_parse_section(&mut value, section);

        let out_line = serde_json::to_string(&value)
            .with_context(|| format!("failed serializing output line {}", line_no + 1))?;
        stdout
            .write_all(out_line.as_bytes())
            .with_context(|| format!("failed writing output line {}", line_no + 1))?;
        stdout
            .write_all(b"\n")
            .with_context(|| format!("failed writing newline for line {}", line_no + 1))?;

        processed += 1;
    }

    stdout.flush().context("failed flushing stdout")?;
    eprintln!(
        "jsonl-stdin completed: processed={}, failures={}",
        processed, failures
    );

    Ok(())
}

fn extract_record_wikitext(value: &Value) -> Option<String> {
    match value {
        Value::Object(map) => map
            .get("wikitext")
            .or_else(|| map.get("input"))
            .or_else(|| map.get("text"))
            .or_else(|| map.get("content"))
            .and_then(Value::as_str)
            .map(str::to_string),
        Value::Array(items) => items.get(3).and_then(Value::as_str).map(str::to_string),
        _ => None,
    }
}

fn extract_record_label(value: &Value, index: usize) -> String {
    match value {
        Value::Object(map) => map
            .get("title")
            .or_else(|| map.get("name"))
            .or_else(|| map.get("id"))
            .and_then(Value::as_str)
            .map(str::to_string)
            .unwrap_or_else(|| format!("record-{}", index)),
        Value::Array(items) => items
            .get(1)
            .and_then(Value::as_str)
            .map(str::to_string)
            .unwrap_or_else(|| format!("record-{}", index)),
        _ => format!("record-{}", index),
    }
}

fn attach_parse_section(value: &mut Value, section: Value) {
    match value {
        Value::Object(map) => {
            map.insert("parse_wiki_text".to_string(), section);
        }
        Value::Array(items) => {
            // Preserve all original array items and append/replace parse section container.
            if let Some(existing_idx) = items.iter().position(|v| {
                v.as_object()
                    .map(|o| o.contains_key("parse_wiki_text"))
                    .unwrap_or(false)
            }) {
                items[existing_idx] = json!({ "parse_wiki_text": section });
            } else {
                items.push(json!({ "parse_wiki_text": section }));
            }
        }
        _ => {
            let original = value.take();
            *value = json!({
                "record": original,
                "parse_wiki_text": section
            });
        }
    }
}

fn process_wikitext_dir(dir: &Path, args: &Args) -> Result<usize> {
    let mut entries = fs::read_dir(dir)
        .with_context(|| format!("failed reading directory: {}", dir.display()))?
        .filter_map(|entry| entry.ok())
        .filter(|entry| entry.file_type().map(|t| t.is_file()).unwrap_or(false))
        .collect::<Vec<_>>();

    entries.sort_by_key(|e| e.file_name());

    if entries.is_empty() {
        return Err(anyhow!("no files found in directory: {}", dir.display()));
    }

    let mut ok_count = 0usize;

    for (idx, entry) in entries.iter().enumerate() {
        let input_path = entry.path();
        let wikitext = fs::read_to_string(&input_path)
            .with_context(|| format!("failed reading input file: {}", input_path.display()))?;

        let parse_start = Instant::now();
        let parsed = Configuration::default().parse(&wikitext);
        let parse_ms = parse_start.elapsed().as_millis();

        let encode_start = Instant::now();
        let encoded_ast = encode_parse_wiki_text_ast(&parsed);
        let encode_ms = encode_start.elapsed().as_millis();

        let sample_label = input_path
            .file_name()
            .and_then(|s| s.to_str())
            .unwrap_or("sample")
            .to_string();

        let payload = json!({
            "sampleLabel": sample_label,
            "inputLen": wikitext.len(),
            "parse_wiki_text": encoded_ast,
            "metrics": {
                "parseMs": parse_ms,
                "encodeMs": encode_ms,
                "nodeCount": parsed.nodes.len(),
                "warningCount": parsed.warnings.len()
            }
        });

        let output_path = output_path_for_wikitext_file(&input_path, args.out_dir.as_deref());
        if let Some(parent) = output_path.parent() {
            fs::create_dir_all(parent).with_context(|| {
                format!("failed creating output directory: {}", parent.display())
            })?;
        }

        let serialized = serde_json::to_string_pretty(&payload)
            .context("failed serializing wikitext-dir output JSON")?;
        fs::write(&output_path, serialized)
            .with_context(|| format!("failed writing output file: {}", output_path.display()))?;

        append_perf_line(
            &args.perf_log,
            &args.suite,
            idx + 1,
            &sample_label,
            Timing { parse_ms, encode_ms },
            parsed.nodes.len(),
            &output_path,
        )?;

        println!(
            "OK   [{}] parse={} encode={} nodes={} -> {}",
            sample_label,
            format_ms(parse_ms),
            format_ms(encode_ms),
            parsed.nodes.len(),
            output_path.display()
        );

        ok_count += 1;
    }

    Ok(ok_count)
}

fn process_plain_file(input_path: &Path, args: &Args) -> Result<()> {
    let wikitext = fs::read_to_string(input_path)
        .with_context(|| format!("failed reading input file: {}", input_path.display()))?;

    let parse_start = Instant::now();
    let parsed = Configuration::default().parse(&wikitext);
    let parse_ms = parse_start.elapsed().as_millis();

    let encode_start = Instant::now();
    let encoded_ast = encode_parse_wiki_text_ast(&parsed);
    let encode_ms = encode_start.elapsed().as_millis();

    let payload = json!({
        "sampleLabel": input_path
            .file_name()
            .and_then(|s| s.to_str())
            .unwrap_or("sample"),
        "input": wikitext,
        "inputLen": wikitext.len(),
        "parse_wiki_text": encoded_ast,
        "metrics": {
            "parseMs": parse_ms,
            "encodeMs": encode_ms,
            "nodeCount": parsed.nodes.len(),
            "warningCount": parsed.warnings.len()
        }
    });

    let output_path = output_path_for_plain_file(input_path, args.out_dir.as_deref());
    if let Some(parent) = output_path.parent() {
        fs::create_dir_all(parent)
            .with_context(|| format!("failed creating output directory: {}", parent.display()))?;
    }

    let serialized = serde_json::to_string_pretty(&payload)
        .context("failed serializing plain-file output JSON")?;
    fs::write(&output_path, serialized)
        .with_context(|| format!("failed writing output file: {}", output_path.display()))?;

    append_perf_line(
        &args.perf_log,
        &args.suite,
        1,
        &input_path.display().to_string(),
        Timing { parse_ms, encode_ms },
        parsed.nodes.len(),
        &output_path,
    )?;

    println!(
        "OK   [{}] parse={} encode={} nodes={} -> {}",
        input_path.display(),
        format_ms(parse_ms),
        format_ms(encode_ms),
        parsed.nodes.len(),
        output_path.display()
    );

    Ok(())
}

fn process_json_suite_file(input_path: &Path, args: &Args) -> Result<()> {
    let content = fs::read_to_string(input_path)
        .with_context(|| format!("failed reading JSON input file: {}", input_path.display()))?;
    let value: Value = serde_json::from_str(&content)
        .with_context(|| format!("JSON input is invalid: {}", input_path.display()))?;

    let suite_name = value
        .get("name")
        .and_then(Value::as_str)
        .map(str::to_string)
        .or_else(|| {
            input_path
                .file_stem()
                .and_then(|s| s.to_str())
                .map(str::to_string)
        })
        .unwrap_or_else(|| "suite".to_string());

    let mut samples = Vec::new();
    extract_samples_from_value(&value, None, &mut samples)?;

    if samples.is_empty() {
        return Err(anyhow!(
            "no samples were found in JSON input: {}",
            input_path.display()
        ));
    }

    let mut out_samples = Vec::with_capacity(samples.len());

    for (idx, sample) in samples.iter().enumerate() {
        let parse_start = Instant::now();
        let parsed = Configuration::default().parse(&sample.wikitext);
        let parse_ms = parse_start.elapsed().as_millis();

        let encode_start = Instant::now();
        let encoded_ast = encode_parse_wiki_text_ast(&parsed);
        let encode_ms = encode_start.elapsed().as_millis();

        out_samples.push(json!({
            "sampleIndex": idx + 1,
            "sampleLabel": sample.label,
            "input": sample.wikitext,
            "inputLen": sample.wikitext.len(),
            "parse_wiki_text": encoded_ast,
            "metrics": {
                "parseMs": parse_ms,
                "encodeMs": encode_ms,
                "nodeCount": parsed.nodes.len(),
                "warningCount": parsed.warnings.len()
            }
        }));

        append_perf_line(
            &args.perf_log,
            &suite_name,
            idx + 1,
            &sample.label,
            Timing { parse_ms, encode_ms },
            parsed.nodes.len(),
            &output_path_for_suite(input_path, args.out_dir.as_deref()),
        )?;
    }

    let output_path = output_path_for_suite(input_path, args.out_dir.as_deref());
    if let Some(parent) = output_path.parent() {
        fs::create_dir_all(parent)
            .with_context(|| format!("failed creating output directory: {}", parent.display()))?;
    }

    let payload = json!({
        "suite": suite_name,
        "parser": "parse_wiki_text",
        "sampleCount": out_samples.len(),
        "samples": out_samples
    });

    let serialized = serde_json::to_string_pretty(&payload)
        .context("failed serializing suite output JSON")?;
    fs::write(&output_path, serialized)
        .with_context(|| format!("failed writing output file: {}", output_path.display()))?;

    println!(
        "OK   [{}] samples={} -> {}",
        input_path.display(),
        samples.len(),
        output_path.display()
    );

    Ok(())
}

fn encode_parse_wiki_text_ast(parsed: &Output<'_>) -> String {
    // Encode parser-native AST nodes as JSON text.
    let encoded = json!({
        "nodes": parsed.nodes.iter().map(node_to_value).collect::<Vec<_>>()
    });
    serde_json::to_string(&encoded).unwrap_or_else(|_| "{\"error\":\"encode failed\"}".to_string())
}

fn warnings_to_value(parsed: &Output<'_>) -> Vec<Value> {
    parsed
        .warnings
        .iter()
        .map(|w| {
            json!({
                "start": w.start,
                "end": w.end,
                "messageType": format!("{:?}", w.message),
                "message": w.message.message()
            })
        })
        .collect::<Vec<_>>()
}

fn node_to_value(node: &Node<'_>) -> Value {
    match node {
        Node::Bold { start, end } => json!({ "type": "Bold", "start": start, "end": end }),
        Node::BoldItalic { start, end } => {
            json!({ "type": "BoldItalic", "start": start, "end": end })
        }
        Node::Category {
            start,
            end,
            target,
            ordinal,
        } => json!({
            "type": "Category",
            "start": start,
            "end": end,
            "target": target,
            "ordinal": ordinal.iter().map(node_to_value).collect::<Vec<_>>()
        }),
        Node::CharacterEntity {
            start,
            end,
            character,
        } => json!({
            "type": "CharacterEntity",
            "start": start,
            "end": end,
            "character": character.to_string()
        }),
        Node::Comment { start, end } => json!({ "type": "Comment", "start": start, "end": end }),
        Node::DefinitionList { start, end, items } => json!({
            "type": "DefinitionList",
            "start": start,
            "end": end,
            "items": items.iter().map(def_list_item_to_value).collect::<Vec<_>>()
        }),
        Node::EndTag { start, end, name } => {
            json!({ "type": "EndTag", "start": start, "end": end, "name": name })
        }
        Node::ExternalLink { start, end, nodes } => json!({
            "type": "ExternalLink",
            "start": start,
            "end": end,
            "nodes": nodes.iter().map(node_to_value).collect::<Vec<_>>()
        }),
        Node::Heading {
            start,
            end,
            level,
            nodes,
        } => json!({
            "type": "Heading",
            "start": start,
            "end": end,
            "level": level,
            "nodes": nodes.iter().map(node_to_value).collect::<Vec<_>>()
        }),
        Node::HorizontalDivider { start, end } => {
            json!({ "type": "HorizontalDivider", "start": start, "end": end })
        }
        Node::Image {
            start,
            end,
            target,
            text,
        } => json!({
            "type": "Image",
            "start": start,
            "end": end,
            "target": target,
            "text": text.iter().map(node_to_value).collect::<Vec<_>>()
        }),
        Node::Italic { start, end } => json!({ "type": "Italic", "start": start, "end": end }),
        Node::Link {
            start,
            end,
            target,
            text,
        } => json!({
            "type": "Link",
            "start": start,
            "end": end,
            "target": target,
            "text": text.iter().map(node_to_value).collect::<Vec<_>>()
        }),
        Node::MagicWord { start, end } => {
            json!({ "type": "MagicWord", "start": start, "end": end })
        }
        Node::OrderedList { start, end, items } => json!({
            "type": "OrderedList",
            "start": start,
            "end": end,
            "items": items.iter().map(list_item_to_value).collect::<Vec<_>>()
        }),
        Node::ParagraphBreak { start, end } => {
            json!({ "type": "ParagraphBreak", "start": start, "end": end })
        }
        Node::Parameter {
            start,
            end,
            name,
            default,
        } => json!({
            "type": "Parameter",
            "start": start,
            "end": end,
            "name": name.iter().map(node_to_value).collect::<Vec<_>>(),
            "default": default.as_ref().map(|d| d.iter().map(node_to_value).collect::<Vec<_>>())
        }),
        Node::Preformatted { start, end, nodes } => json!({
            "type": "Preformatted",
            "start": start,
            "end": end,
            "nodes": nodes.iter().map(node_to_value).collect::<Vec<_>>()
        }),
        Node::Redirect { start, end, target } => json!({
            "type": "Redirect",
            "start": start,
            "end": end,
            "target": target
        }),
        Node::StartTag { start, end, name } => {
            json!({ "type": "StartTag", "start": start, "end": end, "name": name })
        }
        Node::Table {
            start,
            end,
            attributes,
            captions,
            rows,
        } => json!({
            "type": "Table",
            "start": start,
            "end": end,
            "attributes": attributes.iter().map(node_to_value).collect::<Vec<_>>(),
            "captions": captions.iter().map(table_caption_to_value).collect::<Vec<_>>(),
            "rows": rows.iter().map(table_row_to_value).collect::<Vec<_>>()
        }),
        Node::Tag {
            start,
            end,
            name,
            nodes,
        } => json!({
            "type": "Tag",
            "start": start,
            "end": end,
            "name": name,
            "nodes": nodes.iter().map(node_to_value).collect::<Vec<_>>()
        }),
        Node::Template {
            start,
            end,
            name,
            parameters,
        } => json!({
            "type": "Template",
            "start": start,
            "end": end,
            "name": name.iter().map(node_to_value).collect::<Vec<_>>(),
            "parameters": parameters.iter().map(template_parameter_to_value).collect::<Vec<_>>()
        }),
        Node::Text { start, end, value } => {
            json!({ "type": "Text", "start": start, "end": end, "value": value })
        }
        Node::UnorderedList { start, end, items } => json!({
            "type": "UnorderedList",
            "start": start,
            "end": end,
            "items": items.iter().map(list_item_to_value).collect::<Vec<_>>()
        }),
    }
}

fn def_list_item_to_value(item: &DefinitionListItem<'_>) -> Value {
    json!({
        "start": item.start,
        "end": item.end,
        "type": match item.type_ {
            DefinitionListItemType::Details => "Details",
            DefinitionListItemType::Term => "Term",
        },
        "nodes": item.nodes.iter().map(node_to_value).collect::<Vec<_>>()
    })
}

fn list_item_to_value(item: &ListItem<'_>) -> Value {
    json!({
        "start": item.start,
        "end": item.end,
        "nodes": item.nodes.iter().map(node_to_value).collect::<Vec<_>>()
    })
}

fn template_parameter_to_value(parameter: &Parameter<'_>) -> Value {
    json!({
        "start": parameter.start,
        "end": parameter.end,
        "name": parameter.name.as_ref().map(|n| n.iter().map(node_to_value).collect::<Vec<_>>()),
        "value": parameter.value.iter().map(node_to_value).collect::<Vec<_>>()
    })
}

fn table_caption_to_value(caption: &TableCaption<'_>) -> Value {
    json!({
        "start": caption.start,
        "end": caption.end,
        "attributes": caption.attributes.as_ref().map(|a| a.iter().map(node_to_value).collect::<Vec<_>>()),
        "content": caption.content.iter().map(node_to_value).collect::<Vec<_>>()
    })
}

fn table_row_to_value(row: &TableRow<'_>) -> Value {
    json!({
        "start": row.start,
        "end": row.end,
        "attributes": row.attributes.iter().map(node_to_value).collect::<Vec<_>>(),
        "cells": row.cells.iter().map(table_cell_to_value).collect::<Vec<_>>()
    })
}

fn table_cell_to_value(cell: &TableCell<'_>) -> Value {
    json!({
        "start": cell.start,
        "end": cell.end,
        "cellType": match cell.type_ {
            TableCellType::Heading => "Heading",
            TableCellType::Ordinary => "Ordinary",
        },
        "attributes": cell.attributes.as_ref().map(|a| a.iter().map(node_to_value).collect::<Vec<_>>()),
        "content": cell.content.iter().map(node_to_value).collect::<Vec<_>>()
    })
}

fn extract_samples_from_value(
    value: &Value,
    inherited_label: Option<&str>,
    out: &mut Vec<SampleInput>,
) -> Result<()> {
    match value {
        Value::String(s) => {
            out.push(SampleInput {
                label: inherited_label.unwrap_or("sample").to_string(),
                wikitext: s.clone(),
            });
        }
        Value::Array(items) => {
            for (idx, item) in items.iter().enumerate() {
                let label = format!("{}", idx + 1);
                extract_samples_from_value(item, Some(&label), out)?;
            }
        }
        Value::Object(map) => {
            if let Some(samples) = map.get("samples") {
                extract_samples_from_value(samples, inherited_label, out)?;
                return Ok(());
            }

            let display_label = map
                .get("sampleLabel")
                .or_else(|| map.get("name"))
                .or_else(|| map.get("title"))
                .or_else(|| map.get("id"))
                .and_then(Value::as_str)
                .or(inherited_label)
                .unwrap_or("sample")
                .to_string();

            let text = map
                .get("wikitext")
                .or_else(|| map.get("input"))
                .or_else(|| map.get("text"))
                .or_else(|| map.get("content"))
                .and_then(Value::as_str);

            if let Some(wikitext) = text {
                out.push(SampleInput {
                    label: display_label,
                    wikitext: wikitext.to_string(),
                });
                return Ok(());
            }

            for (k, v) in map {
                let child_label = format!("{}:{}", display_label, k);
                extract_samples_from_value(v, Some(&child_label), out)?;
            }
        }
        _ => {}
    }

    Ok(())
}

fn output_path_for_plain_file(input_path: &Path, out_dir: Option<&Path>) -> PathBuf {
    let base_dir = out_dir
        .map(Path::to_path_buf)
        .or_else(|| input_path.parent().map(Path::to_path_buf))
        .unwrap_or_else(|| PathBuf::from("."));
    let filename = input_path
        .file_name()
        .and_then(|s| s.to_str())
        .unwrap_or("sample");
    base_dir.join(format!("{}.pwt.json", filename))
}

fn output_path_for_suite(input_path: &Path, out_dir: Option<&Path>) -> PathBuf {
    let base_dir = out_dir
        .map(Path::to_path_buf)
        .or_else(|| input_path.parent().map(Path::to_path_buf))
        .unwrap_or_else(|| PathBuf::from("."));
    let stem = input_path
        .file_stem()
        .and_then(|s| s.to_str())
        .unwrap_or("suite");
    base_dir.join(format!("{}.pwt.json", stem))
}

fn output_path_for_wikitext_file(input_path: &Path, out_dir: Option<&Path>) -> PathBuf {
    let base_dir = out_dir
        .map(Path::to_path_buf)
        .or_else(|| input_path.parent().map(Path::to_path_buf))
        .unwrap_or_else(|| PathBuf::from("."));
    let filename = input_path
        .file_name()
        .and_then(|s| s.to_str())
        .unwrap_or("sample");
    base_dir.join(format!("{}.pwt.json", filename))
}

fn append_perf_line(
    perf_log: &Path,
    suite: &str,
    sample_index: usize,
    label: &str,
    timing: Timing,
    node_count: usize,
    output_path: &Path,
) -> Result<()> {
    let perf_label = format!("{}-{}", sanitize_name(suite), sample_index);
    let line = format!(
        "{} ({}) parse: {}, encode: {}, nodes: {}, output: {}\n",
        perf_label,
        sanitize_name(label),
        format_ms(timing.parse_ms),
        format_ms(timing.encode_ms),
        node_count,
        output_path.display()
    );

    if let Some(parent) = perf_log.parent() {
        fs::create_dir_all(parent)
            .with_context(|| format!("failed creating perf log directory: {}", parent.display()))?;
    }

    fs::OpenOptions::new()
        .create(true)
        .append(true)
        .open(perf_log)
        .with_context(|| format!("failed opening perf log: {}", perf_log.display()))?
        .write_all(line.as_bytes())
        .with_context(|| format!("failed writing perf log: {}", perf_log.display()))?;

    Ok(())
}

fn sanitize_name(input: &str) -> String {
    let mut out = String::new();
    for c in input.chars() {
        if c.is_ascii_alphanumeric() || c == '.' || c == '_' || c == '-' {
            out.push(c);
        } else {
            out.push('_');
        }
    }

    let trimmed = out.trim_matches('_').to_string();
    if trimmed.is_empty() {
        "sample".to_string()
    } else {
        trimmed.chars().take(80).collect()
    }
}

fn format_ms(ms: u128) -> String {
    let s = ms.to_string();
    if s.len() >= 3 {
        format!("{}ms", s)
    } else {
        format!("{}ms", format!("{:0>3}", s))
    }
}
