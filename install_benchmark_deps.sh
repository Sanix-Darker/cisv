#!/bin/bash

# Install script for CSV benchmark tool dependencies

set -e

echo "Installing CSV parsing tool dependencies..."

# Detect OS
OS="unknown"
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="linux"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macos"
fi

# Install Rust if not present
if ! command -v cargo &> /dev/null; then
    echo "Installing Rust..."
    curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
    source $HOME/.cargo/env
else
    echo "✓ Rust already installed"
fi

# Install/Build rust-csv benchmark tools
echo "Building rust-csv benchmark tools..."
mkdir -p benchmark/rust-csv-bench
cd benchmark/rust-csv-bench

# Initialize Cargo project if needed
if [ ! -f Cargo.toml ]; then
    cargo init --name csv-bench
    cat > Cargo.toml << 'EOF'
[package]
name = "csv-bench"
version = "0.1.0"
edition = "2021"

[dependencies]
csv = "1.3"

[[bin]]
name = "csv-bench"
path = "src/main.rs"

[[bin]]
name = "csv-select"
path = "src/select.rs"

[profile.release]
lto = true
codegen-units = 1
EOF
fi

# Create count benchmark
cat > src/main.rs << 'EOF'
use std::env;
use std::error::Error;
use csv::ReaderBuilder;

fn main() -> Result<(), Box<dyn Error>> {
    let args: Vec<String> = env::args().collect();
    if args.len() < 2 {
        eprintln!("Usage: {} <file>", args[0]);
        std::process::exit(1);
    }

    let mut rdr = ReaderBuilder::new()
        .has_headers(true)
        .from_path(&args[1])?;

    let count = rdr.records().count();
    println!("{}", count);
    Ok(())
}
EOF

# Create select benchmark
cat > src/select.rs << 'EOF'
use std::env;
use std::error::Error;
use csv::{ReaderBuilder, WriterBuilder};

fn main() -> Result<(), Box<dyn Error>> {
    let args: Vec<String> = env::args().collect();
    if args.len() < 3 {
        eprintln!("Usage: {} <file> <columns>", args[0]);
        std::process::exit(1);
    }

    let columns: Vec<usize> = args[2].split(',')
        .map(|s| s.parse::<usize>().unwrap())
        .collect();

    let mut rdr = ReaderBuilder::new()
        .has_headers(true)
        .from_path(&args[1])?;

    let mut wtr = WriterBuilder::new()
        .from_writer(std::io::stdout());

    // Write headers
    let headers = rdr.headers()?.clone();
    let selected_headers: Vec<&str> = columns.iter()
        .map(|&i| headers.get(i).unwrap_or(""))
        .collect();
    wtr.write_record(&selected_headers)?;

    // Write records
    for result in rdr.records() {
        let record = result?;
        let selected: Vec<&str> = columns.iter()
            .map(|&i| record.get(i).unwrap_or(""))
            .collect();
        wtr.write_record(&selected)?;
    }

    wtr.flush()?;
    Ok(())
}
EOF

cargo build --release
cd ../..
echo "✓ rust-csv benchmark tools built"

# Install xsv
#if ! command -v xsv &> /dev/null; then
#    echo "Installing xsv..."
#    cargo install xsv
#else
#    echo "✓ xsv already installed"
#fi

# Install csvkit
if command -v pip3 &> /dev/null; then
    if ! command -v csvcut &> /dev/null; then
        echo "Installing csvkit..."
        pip3 install csvkit
    else
        echo "✓ csvkit already installed"
    fi
else
    echo "⚠ pip3 not found, skipping csvkit installation"
fi

echo ""
echo "Installation complete! Installed tools:"
echo "-----------------------------------"
[ -f "benchmark/rust-csv-bench/target/release/csv-bench" ] && echo "✓ rust-csv (benchmark tool)"
# command -v xsv &> /dev/null && echo "✓ xsv"
command -v csvcut &> /dev/null && echo "✓ csvkit"
echo ""
echo "You can now run: make benchmark-cli"
