# wikiparser-node C Tokenizer

This project contains a drop in replacement for the [wikiparser-node](https://github.com/bhsd-harry/wikiparser-node) Token class. It overrides the parse and toString to use a C based tokenizer.  It will have extra functions to assist with normalizing the data for AI.

## Purpose

Parsing the 7 million wikipedia articles is a slow process.  Using a straight C tokenizer speeds it up.  Initial testing shows the increase can be significan for processing 7 million records.  Below is a sample, but there are opportunities for additional performance enhancements.

```text
article-1 parse: 159ms 054ms, toString: 003ms 000ms
article-2 parse: 060ms 020ms, toString: 002ms 000ms
article-3 parse: 037ms 014ms, toString: 001ms 000ms
article-4 parse: 140ms 086ms, toString: 004ms 000ms
article-5 parse: 046ms 030ms, toString: 001ms 000ms
```

Along with parsing, a straight C implementation is portable.  I could be used with Python, the de-facto language for AI.

The end goal is to use this parser to normalize WikiPedia for use with AI, to create a knowledge graph and normalized text for training and RAG.

## Implementation

This is a duplicate of the functionality of [wikiparser-node](https://github.com/bhsd-harry/wikiparser-node).  There are prompts to use in vs Code Copilot to convert to C and keep parity with the JS implementation.  The goal is to not re-invent the wheel, but provide a cross language high performance tokenizer.  

## Layout

The tokenizer is in wikitexxt_tokenizer
The Node bindings is in node-api-binding


