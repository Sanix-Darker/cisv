from setuptools import setup, Extension
import os

# Detect SIMD support
compile_args = []
if os.uname().machine in ['x86_64', 'amd64']:
    compile_args = ['-mavx2', '-march=native']
elif os.uname().machine.startswith('arm'):
    compile_args = ['-mfpu=neon']

module = Extension('cisv',
                   sources=['cisv_python.c'],
                   include_dirs=['../../include'],
                   extra_compile_args=compile_args + ['-O3', '-std=c11'],
                   extra_link_args=[])

setup(name='cisv-python',
      version='0.0.7',
      description='High-performance CSV parser for Python',
      ext_modules=[module],
      zip_safe=False)
