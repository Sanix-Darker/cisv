"""
CISV - High-performance CSV parser with SIMD optimizations

This setup.py compiles the native C library during installation,
optimizing for the user's specific CPU.
"""

import os
import subprocess
import sys
from pathlib import Path
from setuptools import setup, find_packages
from setuptools.command.build_py import build_py
from setuptools.command.develop import develop
from setuptools.command.egg_info import egg_info


def get_version():
    """Read version from __init__.py"""
    init_path = Path(__file__).parent / "cisv" / "__init__.py"
    with open(init_path) as f:
        for line in f:
            if line.startswith("__version__"):
                return line.split("=")[1].strip().strip("'\"")
    return "0.0.0"


def build_native_library():
    """Build the native C library."""
    # Find the core directory (relative to this setup.py)
    setup_dir = Path(__file__).parent.resolve()

    # Check multiple possible locations for core
    possible_core_dirs = [
        setup_dir / ".." / ".." / "core",  # When in bindings/python/
        setup_dir / "core",                 # When core is copied to sdist
    ]

    core_dir = None
    for candidate in possible_core_dirs:
        if candidate.exists() and (candidate / "Makefile").exists():
            core_dir = candidate.resolve()
            break

    if core_dir is None:
        print("WARNING: Could not find core directory with Makefile")
        print("Checked locations:")
        for candidate in possible_core_dirs:
            print(f"  - {candidate}")
        print("The native library will not be built.")
        print("You may need to build it manually: cd core && make shared")
        return None

    print(f"Building native library in: {core_dir}")

    # Build the shared library
    try:
        # Clean and build
        subprocess.check_call(["make", "clean"], cwd=core_dir)
        subprocess.check_call(["make", "shared"], cwd=core_dir)

        # Find the built library
        build_dir = core_dir / "build"
        for lib_name in ["libcisv.so", "libcisv.dylib"]:
            lib_path = build_dir / lib_name
            if lib_path.exists():
                print(f"Successfully built: {lib_path}")
                return lib_path

        print("WARNING: Build succeeded but library not found")
        return None

    except subprocess.CalledProcessError as e:
        print(f"WARNING: Failed to build native library: {e}")
        print("You may need to install build tools (gcc, make)")
        return None
    except FileNotFoundError:
        print("WARNING: 'make' not found. Please install build-essential or equivalent.")
        return None


def copy_library_to_package(lib_path):
    """Copy the built library to the package directory."""
    if lib_path is None:
        return

    setup_dir = Path(__file__).parent.resolve()
    libs_dir = setup_dir / "cisv" / "libs"
    libs_dir.mkdir(parents=True, exist_ok=True)

    dest_path = libs_dir / lib_path.name

    import shutil
    shutil.copy2(lib_path, dest_path)
    print(f"Copied library to: {dest_path}")


class BuildPyCommand(build_py):
    """Custom build command that compiles the native library first."""

    def run(self):
        lib_path = build_native_library()
        copy_library_to_package(lib_path)
        super().run()


class DevelopCommand(develop):
    """Custom develop command that compiles the native library first."""

    def run(self):
        lib_path = build_native_library()
        copy_library_to_package(lib_path)
        super().run()


class EggInfoCommand(egg_info):
    """Custom egg_info command that compiles the native library first."""

    def run(self):
        lib_path = build_native_library()
        copy_library_to_package(lib_path)
        super().run()


# Read long description from README
readme_path = Path(__file__).parent / "README.md"
long_description = ""
if readme_path.exists():
    with open(readme_path, encoding="utf-8") as f:
        long_description = f.read()


setup(
    name="cisv",
    version=get_version(),
    description="High-performance CSV parser with SIMD optimizations (AVX-512/AVX2)",
    long_description=long_description,
    long_description_content_type="text/markdown",
    author="Sanix Darker",
    author_email="s4nixd@gmail.com",
    url="https://github.com/sanix-darker/cisv",
    project_urls={
        "Homepage": "https://github.com/sanix-darker/cisv",
        "Documentation": "https://github.com/sanix-darker/cisv#readme",
        "Repository": "https://github.com/sanix-darker/cisv",
        "Issues": "https://github.com/sanix-darker/cisv/issues",
    },
    packages=find_packages(),
    package_data={
        "cisv": ["libs/*.so", "libs/*.dylib", "libs/*.dll"],
    },
    include_package_data=True,
    python_requires=">=3.8",
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: MIT License",
        "Operating System :: POSIX :: Linux",
        "Operating System :: MacOS",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Programming Language :: Python :: 3.12",
        "Programming Language :: Python :: 3.13",
        "Topic :: Software Development :: Libraries",
        "Topic :: Text Processing",
    ],
    keywords=["csv", "parser", "simd", "avx", "performance", "fast", "high-performance"],
    cmdclass={
        "build_py": BuildPyCommand,
        "develop": DevelopCommand,
        "egg_info": EggInfoCommand,
    },
    extras_require={
        "dev": ["pytest", "pytest-benchmark"],
    },
)
