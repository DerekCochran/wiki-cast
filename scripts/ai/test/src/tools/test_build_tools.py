# Build and test tools that call external processes, like compilers and test runners. These are more powerful but also more dangerous, so be careful with how you use them!

import os
import subprocess
from typing import Optional
from langchain_core.tools import tool

@tool
def run_tests() -> str:
    """Runs the node tests for the wikiparser-c application and returns results."""
    # Your actual subprocess execution logic here
    result = subprocess.run(
        ["node", "~/github/wikiparser-node-c-tokenizer/bindings/node/test/run_parsoid.js"],
        capture_output=True, text=True
    )
    return result.stdout + result.stderr

@tool
def compile_code() -> str:
    """Compiles the wikiparser-c application and returns any compilation errors."""
    return "No compilation errors." # Implement actual compiler call