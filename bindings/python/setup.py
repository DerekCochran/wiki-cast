import os
import shutil
import subprocess
import sys
from setuptools import setup, Extension
from Cython.Build import cythonize
import glob

# 1. Resolve the absolute path to the core C source directory.
# Since this script lives in bindings/python/, ../../include points to wiki-cast/include/
SETUP_DIR = os.path.abspath(os.path.dirname(__file__))
CORE_INCLUDE_DIR = os.path.abspath(os.path.join(SETUP_DIR, "..", "..", "include"))
CORE_SRC_DIR = os.path.abspath(os.path.join(SETUP_DIR, "..", "..", "src"))
STAGED_SRC_DIR = os.path.join(SETUP_DIR, "build_sources", "wiki_cast_core")


def pkg_config(*packages):
    include_dirs = []
    library_dirs = []
    libraries = []
    extra_compile_args = []
    extra_link_args = []

    try:
        cflags = subprocess.check_output(["pkg-config", "--cflags", *packages], text=True).split()
        libs = subprocess.check_output(["pkg-config", "--libs", *packages], text=True).split()
    except (OSError, subprocess.CalledProcessError) as exc:
        joined = ", ".join(packages)
        raise RuntimeError(f"pkg-config could not resolve required packages: {joined}") from exc

    for token in cflags:
        if token.startswith("-I"):
            include_dirs.append(token[2:])
        else:
            extra_compile_args.append(token)

    for token in libs:
        if token.startswith("-L"):
            library_dirs.append(token[2:])
        elif token.startswith("-l"):
            libraries.append(token[2:])
        else:
            extra_link_args.append(token)

    return {
        "include_dirs": include_dirs,
        "library_dirs": library_dirs,
        "libraries": libraries,
        "extra_compile_args": extra_compile_args,
        "extra_link_args": extra_link_args,
    }

# 2. Configure maximum performance and SIMD flags based on the compiler platform
if sys.platform == "win32":
    extra_compile_args = [
        "/O2",          # Maximize speed optimizations
        "/Oi",          # Enable intrinsic functions (critical for SIMD)
        "/Ot",          # Favor fast code execution over small binary size
        "/arch:HOST",   # Target the current machine's native CPU architecture (MSVC 2019+)
    ]
else:
    extra_compile_args = [
        "-O3",            # Aggressive code optimization
        "-march=native",   # Target user's exact CPU and unlock native SIMD (AVX2/AVX-512)
        "-ffast-math",     # Breaks strict IEEE compliance for additional speed sweeps
    ]

pkg_config_flags = pkg_config("libcjson", "icu-uc")

# 3. Stage core C sources under bindings/python so editable builds never see ../../ paths.
if os.path.isdir(STAGED_SRC_DIR):
    shutil.rmtree(STAGED_SRC_DIR)

c_sources = []
for pattern in ["*.c", "parser/*.c", "util/*.c"]:
    for abs_path in glob.glob(os.path.join(CORE_SRC_DIR, pattern)):
        rel_core_path = os.path.relpath(abs_path, CORE_SRC_DIR)
        staged_path = os.path.join(STAGED_SRC_DIR, rel_core_path)
        os.makedirs(os.path.dirname(staged_path), exist_ok=True)
        shutil.copy2(abs_path, staged_path)
        c_sources.append(os.path.relpath(staged_path, SETUP_DIR))

# 3. Define the Cython extension module
extensions = [
    Extension(
        name="wiki_cast._binding",
        sources=["src/wiki_cast/_binding.pyx"] + c_sources,
        include_dirs=[CORE_INCLUDE_DIR,
                      *pkg_config_flags["include_dirs"],
                      os.path.join(CORE_SRC_DIR, "include")
                      ],
        library_dirs=pkg_config_flags["library_dirs"],
        libraries=pkg_config_flags["libraries"],
        extra_compile_args=extra_compile_args + pkg_config_flags["extra_compile_args"],
        extra_link_args=pkg_config_flags["extra_link_args"],
    )
]

# 4. Run the setup execution
setup(
    name="wiki_cast",
    version="1.0.0",
    description="High-performance SIMD-accelerated wikitext parser",
    packages=["wiki_cast"],
    package_dir={"wiki_cast": "src/wiki_cast"},
    ext_modules=cythonize(
        extensions, 
        compiler_directives={
            'language_level': "3",
            'boundscheck': False,   # Strips out Python array bounds checking for raw C speed
            'wraparound': False     # Disables Python negative indexing support inside Cython code
        }
    ),
    zip_safe=False,
)