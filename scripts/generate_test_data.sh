#!/bin/bash

set -e

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${YELLOW}Generating test CSV files...${NC}"

# Small file (1K rows)
echo "Creating small.csv (1K rows)..."
python3 -c "
import csv
with open('small.csv', 'w', newline='') as f:
    w = csv.writer(f)
    w.writerow(['id', 'name', 'email', 'address', 'phone', 'date', 'amount'])
    for i in range(1000):
        w.writerow([i, f'Person {i}', f'person{i}@email.com',
                   f'{i} Main St, City {i%100}', f'555-{i:04d}',
                   f'2024-{(i%12)+1:02d}-{(i%28)+1:02d}', f'{i*1.23:.2f}'])
"

# Medium file (100K rows)
echo "Creating medium.csv (100K rows)..."
python3 -c "
import csv
with open('medium.csv', 'w', newline='') as f:
    w = csv.writer(f)
    w.writerow(['id', 'name', 'email', 'address', 'phone', 'date', 'amount'])
    for i in range(100000):
        w.writerow([i, f'Person {i}', f'person{i}@email.com',
                   f'{i} Main St, City {i%100}', f'555-{i:04d}',
                   f'2024-{(i%12)+1:02d}-{(i%28)+1:02d}', f'{i*1.23:.2f}'])
"

# Large file (1M rows) - optional based on parameter
if [ "$1" = "--large" ] || [ "$1" = "--all" ]; then
    echo "Creating large.csv (1M rows)..."
    python3 -c "
import csv
with open('large.csv', 'w', newline='') as f:
    w = csv.writer(f)
    w.writerow(['id', 'name', 'email', 'address', 'phone', 'date', 'amount'])
    for i in range(1000000):
        w.writerow([i, f'Person {i}', f'person{i}@email.com',
                   f'{i} Main St, City {i%100}', f'555-{i:04d}',
                   f'2024-{(i%12)+1:02d}-{(i%28)+1:02d}', f'{i*1.23:.2f}'])
"
fi

# Fixtures directory for JS benchmarks
if [ "$2" = "--fixtures" ]; then
    mkdir -p fixtures
    echo "Creating fixtures/data.csv (100K rows)..."
    python3 -c "
import csv
with open('fixtures/data.csv', 'w', newline='') as f:
    w = csv.writer(f)
    w.writerow(['id', 'name', 'email', 'address', 'phone', 'date', 'amount'])
    for i in range(100000):
        w.writerow([i, f'Person {i}', f'person{i}@email.com',
                   f'{i} Main St, City {i%100}', f'555-{i:04d}',
                   f'2024-{(i%12)+1:02d}-{(i%28)+1:02d}', f'{i*1.23:.2f}'])
"
fi

echo -e "${GREEN}Test files created:${NC}"
ls -lh *.csv 2>/dev/null || true
if [ -d fixtures ]; then
    ls -lh fixtures/*.csv 2>/dev/null || true
fi
