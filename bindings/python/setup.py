from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext
import subprocess
import os

class BuildExt(build_ext):
    def run(self):
        # Build the core library first
        core_dir = os.path.join(os.path.dirname(__file__), '..', '..', 'core')
        subprocess.check_call(['make', '-C', core_dir, 'shared'])
        super().run()

setup(
    name='cisv',
    version='0.0.8',
    description='High-performance CSV parser with SIMD optimizations',
    author='Sanix Darker',
    author_email='s4nixd@gmail.com',
    packages=['cisv'],
    python_requires='>=3.8',
    install_requires=[],
    extras_require={
        'dev': ['pytest', 'pytest-benchmark'],
    },
    cmdclass={'build_ext': BuildExt},
    classifiers=[
        'Development Status :: 4 - Beta',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: MIT License',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.8',
        'Programming Language :: Python :: 3.9',
        'Programming Language :: Python :: 3.10',
        'Programming Language :: Python :: 3.11',
        'Programming Language :: Python :: 3.12',
    ],
)
