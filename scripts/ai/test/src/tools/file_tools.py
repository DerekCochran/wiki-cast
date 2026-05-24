import os
from typing import Optional
from langchain_core.tools import tool

# Set this to your repository root to restrict the agent
PROJECT_ROOT = os.path.expanduser("~/git/wikiparser-node-c-tokenizer")
JS_ROOT = os.path.join(PROJECT_ROOT, "new-js")
C_ROOT = os.path.join(PROJECT_ROOT, "src")
H_ROOT = os.path.join(PROJECT_ROOT, "include")

def _resolve_path(path: str) -> str:
    """Helper to ensure the agent stays within the project root."""
    js_path = os.path.abspath(os.path.join(JS_ROOT, path))
    if js_path.startswith(os.path.abspath(JS_ROOT)):
        return js_path
    c_path = os.path.abspath(os.path.join(C_ROOT, path))
    if c_path.startswith(os.path.abspath(C_ROOT)):
        return c_path
    h_path = os.path.abspath(os.path.join(H_ROOT, path))
    if h_path.startswith(os.path.abspath(H_ROOT)):
        return h_path
    raise ValueError(f"Invalid path: {path}. Must be within new-js, src, or include.")


@tool
def view_file_lines(filepath: str, start_line: int = 1, end_line: Optional[int] = None) -> str:
    """
    Reads a specific range of lines from a file, displaying line numbers.
    Use this to inspect code without dumping the entire file into memory.
    """
    try:
        abs_path = _resolve_path(filepath)
        if not os.path.exists(abs_path):
            return f"Error: File not found at {filepath}"
            
        with open(abs_path, 'r', encoding='utf-8') as f:
            lines = f.readlines()
            
        total_lines = len(lines)
        end = end_line if end_line is not None else total_lines
        
        # Bound checking
        start = max(1, start_line)
        end = min(total_lines, end)
        
        output = [f"--- File: {filepath} (Lines {start}-{end} of {total_lines}) ---"]
        for idx in range(start - 1, end):
            output.append(f"{idx + 1:4d}: {lines[idx].rstrip()}")
            
        return "\n".join(output)
    except Exception as e:
        return f"Error viewing file: {str(e)}"


@tool
def search_grep(search_term: str, extension_filter: Optional[str] = None) -> str:
    """
    Searches for a specific string or variable name across all files in the project.
    Equivalent to a basic 'grep' search. Optional extension_filter (e.g., '.c', '.js').
    """
    results = []
    try:
        # Create a safe search by walking the directory and reading files, rather than using subprocess
        for root, _, files in os.walk(PROJECT_ROOT):
            for file in files:
                if extension_filter and not file.endswith(extension_filter):
                    continue
                    
                full_path = os.path.join(root, file)
                rel_path = os.path.relpath(full_path, PROJECT_ROOT)
                
                try:
                    with open(full_path, 'r', encoding='utf-8', errors='ignore') as f:
                        for line_num, line in enumerate(f, 1):
                            if search_term in line:
                                results.append(f"{rel_path}:{line_num}: {line.strip()}")
                except Exception:
                    continue # Skip files that can't be read
                    
        if not results:
            return f"No matches found for '{search_term}'."
        return "\n".join(results[:100]) # Limit output so it doesn't flood context
    except Exception as e:
        return f"Error during search: {str(e)}"


@tool
def edit_file_replace(filepath: str, target_block: str, replacement_block: str) -> str:
    """
    Edits a file by searching for an exact 'target_block' of code and replacing it 
    with a 'replacement_block'. This avoids rewriting the whole file.
    The target_block must match exactly (including indentation).
    """
    try:
        abs_path = _resolve_path(filepath)
        with open(abs_path, 'r', encoding='utf-8') as f:
            content = f.read()
            
        if target_block not in content:
            return (
                f"Error: The target_block was not found exactly as written in {filepath}.\n"
                "Ensure your indentation and line breaks match perfectly. "
                "Use view_file_lines to check the exact layout."
            )

        # If target block has multiple instances, fail
        if content.count(target_block) > 1:
            return (
                f"Error: The target_block appears multiple times in {filepath}. "
                "Please make it more specific to match only one block of code."
            )

        # Perform the surgical replacement
        new_content = content.replace(target_block, replacement_block, 1)
        
        with open(abs_path, 'w', encoding='utf-8') as f:
            f.write(new_content)
            
        return f"Successfully updated {filepath}."
    except Exception as e:
        return f"Error editing file: {str(e)}"
    
@tool
def list_directory(path: str = "") -> str:
    """
    Lists files and directories at a given path relative to the project root.
    Use this to explore the project structure and find files to edit or view.
    """
    try:
        abs_path = _resolve_path(path)
        if not os.path.exists(abs_path):
            return f"Error: Path not found at {path}"
        
        entries = os.listdir(abs_path)
        if not entries:
            return f"The directory {path} is empty."
        
        output = [f"--- Directory listing for {path} ---"]
        for entry in entries:
            full_entry_path = os.path.join(abs_path, entry)
            if os.path.isdir(full_entry_path):
                output.append(f"[DIR]  {entry}")
            else:
                output.append(f"       {entry}")
                
        return "\n".join(output)
    except Exception as e:
        return f"Error listing directory: {str(e)}"