# CISV

![License](https://img.shields.io/badge/license-MIT-blue)
![Build](https://img.shields.io/badge/build-passing-brightgreen)
![Size](https://deno.bundlejs.com/badge?q=spring-easing)
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/Sanix-Darker/cisv)

<!-- Service is down fow now ![Downloads](https://badgen.net/npm/dw/cisv) -->

> # DISCLAIMER
>
> This csv parser does not covers all quotes/comments edge cases, it is meant for now to be just extremly fast, thus not PROD ready yet.

Cisv is a csv parser on steroids... literally.
It's a high-performance CSV parser/writer leveraging SIMD instructions and zero-copy memory mapping. Available as both a Node.js native addon and standalone CLI tool with extensive configuration options.

I wrote about basics in a blog post, you can read here :https://sanixdk.xyz/blogs/how-i-accidentally-created-the-fastest-csv-parser-ever-made.

## CLI BENCHMARKS WITH DOCKER

```bash
$ docker build -t cisv-benchmark .
```

To run them... choosing some specs for the container to size resources, you can :

```bash
$ docker run --rm      \
    --cpus="2.0"       \
    --memory="4g"      \
    --memory-swap="4g" \
    --cpu-shares=1024  \
    --security-opt     \
    seccomp=unconfined \
    cisv-benchmark
```
**NOTE:** To have more details on benchmarks, check [BENCHMARK-LIST](./BENCHMARKS.md).


### CLI TOOL (FROM SOURCE)
```bash
git clone https://github.com/sanix-darker/cisv
cd cisv
make cli
sudo make install-cli
```

### CLI
```bash
# Basic parsing
cisv_bin data.csv

# Parse TSV file
cisv_bin -d $'\t' data.tsv

# Parse with custom quote and trim
cisv_bin -q "'" -t data.csv

# Skip comment lines
cisv_bin -m '#' config.csv
```

## CLI USAGE

### PARSING OPTIONS

```bash
cisv_bin [OPTIONS] [FILE]

General Options:
  -h, --help              Show help message
  -v, --version           Show version
  -o, --output FILE       Write to FILE instead of stdout
  -b, --benchmark         Run benchmark mode

Configuration Options:
  -d, --delimiter DELIM   Field delimiter (default: ,)
  -q, --quote CHAR        Quote character (default: ")
  -e, --escape CHAR       Escape character (default: RFC4180 style)
  -m, --comment CHAR      Comment character (default: none)
  -t, --trim              Trim whitespace from fields
  -r, --relaxed           Use relaxed parsing rules
  --skip-empty            Skip empty lines
  --skip-errors           Skip lines with parse errors
  --max-row SIZE          Maximum row size in bytes
  --from-line N           Start from line N (1-based)
  --to-line N             Stop at line N

Processing Options:
  -s, --select COLS       Select columns (comma-separated indices)
  -c, --count             Show only row count
  --head N                Show first N rows
  --tail N                Show last N rows
```

### EXAMPLES

```bash
# Parse TSV file
cisv_bin -d $'\t' data.tsv

# Parse CSV with semicolon delimiter and single quotes
cisv_bin -d ';' -q "'" european.csv

# Skip comment lines starting with #
cisv_bin -m '#' config.csv

# Trim whitespace and skip empty lines
cisv_bin -t --skip-empty messy.csv

# Parse lines 100-1000 only
cisv_bin --from-line 100 --to-line 1000 large.csv

# Select specific columns
cisv_bin -s 0,2,5,7 data.csv

# Count rows with specific configuration
cisv_bin -c -d $'\t' --skip-empty data.tsv

# Benchmark with custom delimiter
cisv_bin -b -d ';' european.csv
```

### WRITING

```bash
cisv_bin write [OPTIONS]

Options:
  -g, --generate N       Generate N rows of test data
  -o, --output FILE      Output file
  -d, --delimiter DELIM  Field delimiter
  -Q, --quote-all        Quote all fields
  -r, --crlf             Use CRLF line endings
  -n, --null TEXT        Null representation
  -b, --benchmark        Benchmark mode
```

## TECHNICAL ARCHITECTURE

- **SIMD Processing**: AVX-512 (64-byte vectors) or AVX2 (32-byte vectors) for parallel processing
- **Memory Mapping**: Direct kernel-to-userspace zero-copy with `mmap()`
- **Optimized Buffering**: 1MB ring buffer sized for L3 cache efficiency
- **Compiler Optimizations**: LTO and architecture-specific tuning with `-march=native`
- **Configurable Parsing**: RFC 4180 compliant with extensive customization options

## FEATURES (PROS)

- RFC 4180 compliant with configurable extensions
- Handles quoted fields with embedded delimiters
- Support for multiple CSV dialects (TSV, PSV, etc.)
- Comment line support
- Field trimming and empty line handling
- Line range parsing for large files
- Streaming API for unlimited file sizes
- Safe fallback for non-x86 architectures
- High-performance CSV writer with SIMD optimization
- Row counting without full parsing

## LIMITATIONS

- Linux/Unix support only (optimized for x86_64 CPU)
- Windows support planned for future release

## LICENSE

MIT © [sanix-darker](https://github.com/sanix-darker)

## ACKNOWLEDGMENTS

Inspired by:
- [simdjson](https://github.com/simdjson/simdjson) - Parsing gigabytes of JSON per second
- [xsv](https://github.com/BurntSushi/xsv) - Fast CSV command line toolkit
- [rust-csv](https://github.com/BurntSushi/rust-csv) - CSV parser for Rust
