#!/bin/bash
# Parallel vs Sequential Benchmark
# Measures wall clock time (not CPU time)

JOY=${JOY:-./build/joy}
export OMP_NUM_THREADS=${OMP_NUM_THREADS:-8}

echo "========================================"
echo "  PARALLEL vs SEQUENTIAL BENCHMARK"
echo "  OMP_NUM_THREADS=$OMP_NUM_THREADS"
echo "========================================"
echo ""

run_test() {
    local desc=$1
    local map_code=$2
    local pmap_code=$3
    
    echo -n "$desc: "
    
    # Run map version
    echo "$map_code" > /tmp/joy_bench.joy
    map_time=$( { time $JOY /tmp/joy_bench.joy > /dev/null 2>&1; } 2>&1 | grep real | awk '{print $2}' )
    
    # Run pmap version  
    echo "$pmap_code" > /tmp/joy_bench.joy
    pmap_time=$( { time $JOY /tmp/joy_bench.joy > /dev/null 2>&1; } 2>&1 | grep real | awk '{print $2}' )
    
    echo "map=$map_time pmap=$pmap_time"
}

echo "--- Light Work (dup *) ---"
run_test "4 elements " \
    "[1 2 3 4] [dup *] map." \
    "[1 2 3 4] [dup *] pmap."
run_test "8 elements " \
    "[1 2 3 4 5 6 7 8] [dup *] map." \
    "[1 2 3 4 5 6 7 8] [dup *] pmap."
echo ""

echo "--- Medium Work (1000 iterations) ---"
run_test "4 elements " \
    "[1 2 3 4] [1000 [dup * 2 mod] times] map." \
    "[1 2 3 4] [1000 [dup * 2 mod] times] pmap."
run_test "8 elements " \
    "[1 2 3 4 5 6 7 8] [1000 [dup * 2 mod] times] map." \
    "[1 2 3 4 5 6 7 8] [1000 [dup * 2 mod] times] pmap."
echo ""

echo "--- Heavy Work (100000 iterations) ---"
run_test "4 elements " \
    "[1 2 3 4] [100000 [dup * 2 mod] times] map." \
    "[1 2 3 4] [100000 [dup * 2 mod] times] pmap."
run_test "8 elements " \
    "[1 2 3 4 5 6 7 8] [100000 [dup * 2 mod] times] map." \
    "[1 2 3 4 5 6 7 8] [100000 [dup * 2 mod] times] pmap."
echo ""

echo "--- Very Heavy Work (1000000 iterations) ---"
run_test "4 elements " \
    "[1 2 3 4] [1000000 [dup * 2 mod] times] map." \
    "[1 2 3 4] [1000000 [dup * 2 mod] times] pmap."
run_test "8 elements " \
    "[1 2 3 4 5 6 7 8] [1000000 [dup * 2 mod] times] map." \
    "[1 2 3 4 5 6 7 8] [1000000 [dup * 2 mod] times] pmap."
echo ""

echo "--- Recursive: Sum 1..N (linrec) ---"
run_test "sum to 5000 " \
    "[5000 5000 5000 5000] [[0 =] [] [dup 1 -] [+] linrec] map." \
    "[5000 5000 5000 5000] [[0 =] [] [dup 1 -] [+] linrec] pmap."
run_test "sum to 10000" \
    "[10000 10000 10000 10000] [[0 =] [] [dup 1 -] [+] linrec] map." \
    "[10000 10000 10000 10000] [[0 =] [] [dup 1 -] [+] linrec] pmap."
echo ""

echo "========================================"
echo "  BENCHMARK COMPLETE"
echo "========================================"
echo ""
echo "Lower time = faster"
echo "pmap should be faster for heavy work"

rm -f /tmp/joy_bench.joy
