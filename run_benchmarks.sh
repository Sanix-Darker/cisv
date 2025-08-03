#!/bin/bash
set -e

echo "=== CISV Benchmark Suite ==="
echo "Date: $(date)"
echo "CPU: $(lscpu | grep 'Model name' | cut -d: -f2 | xargs)"
echo "Memory: $(free -h | grep Mem | awk '{print $2}')"
echo "Kernel: $(uname -r)"
echo "=========================================="
echo ""

cd /home/benchmark/cisv

# Check what benchmark to run based on command line argument
case "${1:-all}" in
    parser|read)
        echo "Running CSV Parser Benchmarks..."
        chmod +x benchmark_cli_reader.sh
        ./benchmark_cli_reader.sh
        ;;
    writer|write)
        echo "Running CSV Writer Benchmarks..."
        chmod +x benchmark_cli_writer.sh
        ./benchmark_cli_writer.sh
        ;;
    memory)
        echo "Running Memory Tests..."
        echo "id,name,value" > test.csv
        echo "1,test,100" >> test.csv
        echo "2,demo,200" >> test.csv
        valgrind --leak-check=full --show-leak-kinds=all ./cisv --version
        valgrind --leak-check=full ./cisv -c test.csv
        valgrind --leak-check=full ./cisv write -g 100 -o test_write.csv
        rm -f test.csv test_write.csv
        ;;
    all|*)
        echo "Running all benchmarks..."

        # Memory tests first
        echo "=== Memory Leak Tests ==="
        $0 memory
        echo ""

        # Parser benchmarks
        echo "=== CSV Parser Benchmarks ==="
        $0 parser
        echo ""

        # Writer benchmarks
        echo "=== CSV Writer Benchmarks ==="
        $0 writer
        ;;
esac
