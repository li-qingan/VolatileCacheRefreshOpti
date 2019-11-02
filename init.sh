#!/bin/bash

POWERSTONE="./powerstone"
PINTOOLS="./pintools"


# 1. Build powerstone bechmarks
echo 'Building benchmarks...'
echo 'cd $POWERSTONE && make && cd ..'
cd $POWERSTONE && make && cd ..

echo ""

# 2. Build pintools
echo 'Building pin tools'
echo 'export PIN_ROOT=$(pwd)/pin-3.6-97554-g31f0a167d-gcc-linux'
export PIN_ROOT=$(pwd)/pin-3.6-97554-g31f0a167d-gcc-linux
echo 'export PATH=$PIN_ROOT:$PATH'
export PATH=$PIN_ROOT:$PATH
echo $PATH
echo ""
echo 'cd $PINTOOLS && make && cd ..'
cd $PINTOOLS && make && cd ..

echo ""

SYMBOL_TRACER="$PINTOOLS/obj-intel64/SymbolTracer.so"
PairwiseGrapher="$PINTOOLS/obj-intel64/PairwiseGrapher.so"
Evaluator="$PINTOOLS/obj-intel64/Evaluator.so"
BENCHMARKS_DIR=$POWERSTONE
BENCHMARKS=("adpcm" "bcnt" "blit" "crc" "engine" "fir" "g3fax" "pocsag" "qurt" "ucbqsort")
#BENCHMARKS=("adpcm")
OUTPUT_DIR="./output"

echo "mkdir -p $OUTPUT_DIR"
mkdir -p $OUTPUT_DIR

# 3. preparing for each benchmark
for BENCHMARK in ${BENCHMARKS[@]}
do
    echo "=======preparing for $BENCHMARK...========"
    BENCH_OUTDIR=$OUTPUT_DIR/$BENCHMARK
    echo "mkdir -p $BENCH_OUTDIR"
    mkdir -p $BENCH_OUTDIR

    TRACE_FILE=$BENCH_OUTDIR/$BENCHMARK.trace
    GRAPH_FILE=$BENCH_OUTDIR/$BENCHMARK.graph
    PARTITIONED_GRAPH_FILE=$BENCH_OUTDIR/$BENCHMARK.alloc
    STATS_FILE=$BENCH_OUTDIR/$BENCHMARK.stats
    OPTI_STATS_FILE=$BENCH_OUTDIR/${BENCHMARK}"."${ALLOCATOR}".stats"
    BENCHMARK_FILE=$BENCHMARKS_DIR/$BENCHMARK

    # collect trace information
    echo ">>Collecting trace information..."
    echo "pin -t $SYMBOL_TRACER -o $TRACE_FILE -- $BENCHMARK_FILE"
    pin -t $SYMBOL_TRACER -o $TRACE_FILE -- $BENCHMARK_FILE
    echo ""

    # generate pairwise graph 
    echo ">>Generating pairwise graph"
    echo "pin -t $PairwiseGrapher -it $TRACE_FILE  -og $GRAPH_FILE -- $BENCHMARK_FILE"
    pin -t $PairwiseGrapher -it $TRACE_FILE  -og $GRAPH_FILE -- $BENCHMARK_FILE
    echo ""
   
done


