# wiki-cast Python Bindings

High-performance Python wrapper for the `wiki-cast` SIMD-accelerated wikitext parser.

## Requirements

Because this library utilizes host-native SIMD instructions (AVX2/AVX-512) for maximum parsing performance, it must be compiled from source on your machine.

- **Linux**: `gcc` or `clang` (via `build-essential`)
- **macOS**: Xcode Command Line Tools (`xcode-select --install`)
- **Windows**: Visual Studio 2019+ with C++ build tools

## Installation

You can install this package directly from GitHub using `pip`:

```bash
pip install "git+[https://github.com/YOUR_USERNAME/wiki-cast.git#subdirectory=bindings/python](https://github.com/YOUR_USERNAME/wiki-cast.git#subdirectory=bindings/python)"
```

## Development

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install --upgrade pip setuptools wheel pytest
cd bindings/python
pip install -e .
python samples/simple.py
```
