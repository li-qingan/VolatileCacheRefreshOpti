#!/bin/bash

# args checking
if [ $# -lt 1 ]
then
	echo "Argument error!"
	echo "Ex: ./run.sh allocator_Dir"
	exit 1
fi

DefaultAlloc="ori"

# 1. Build Allocator
ALLOCATOR=
echo "Building $1..."
if [[ ! -d "$1" ]]; then  
    ALLOCATOR=$DefaultAlloc
    echo "Allocator: $ALLOCATOR not exist! Use the original data layout, output in .ori.stats!"
    read -p "Press any key to continue."　
    touch  
else
    ALLOCATOR=$1
    cd $ALLOCATOR && make && cd ..
fi

echo ""


SYMBOL_TRACER="./pintools/obj-intel64/SymbolTracer.so"
PairwiseGrapher="./pintools/obj-intel64/PairwiseGrapher.so"
Evaluator="./pintools/obj-intel64/Evaluator.so"
BENCHMARKS_DIR="./powerstone"
BENCHMARKS=("adpcm" "bcnt" "blit" "crc" "engine" "fir" "g3fax" "pocsag" "qurt" "ucbqsort")
#BENCHMARKS=("adpcm"  )
OUTPUT_DIR="./output"

echo "mkdir -p $OUTPUT_DIR"
mkdir -p $OUTPUT_DIR
echo 'export PIN_ROOT=$(pwd)/pin-3.6-97554-g31f0a167d-gcc-linux'
export PIN_ROOT=$(pwd)/pin-3.6-97554-g31f0a167d-gcc-linux
echo 'export PATH=$PIN_ROOT:$PATH'
export PATH=$PIN_ROOT:$PATH
echo $PATH
echo ""
echo ""

# 2. evaluating  each benchmark
for BENCHMARK in ${BENCHMARKS[@]}
do
    echo "=======Evaluating on $BENCHMARK...========"
    BENCH_OUTDIR=$OUTPUT_DIR/$BENCHMARK
    echo "mkdir -p $BENCH_OUTDIR"
    mkdir -p $BENCH_OUTDIR

    TRACE_FILE=$BENCH_OUTDIR/$BENCHMARK.trace
    GRAPH_FILE=$BENCH_OUTDIR/$BENCHMARK.graph
    PARTITIONED_GRAPH_FILE=$BENCH_OUTDIR/${BENCHMARK}".alloc"
    STATS_FILE=$BENCH_OUTDIR/$BENCHMARK.stats
    OPTI_STATS_FILE=$BENCH_OUTDIR/${BENCHMARK}"."${ALLOCATOR}".stats"
    BENCHMARK_FILE=$BENCHMARKS_DIR/$BENCHMARK

    # graph partitioning via allocator
    if [[ $ALLOCATOR == $DefaultAlloc ]]; then
        >$PARTITIONED_GRAPH_FILE
        echo ">>Evaluate on the original data layout of program..."
    else
        echo ">>Partitioning graph via allocator..."
        echo "$ALLOCATOR/$ALLOCATOR $GRAPH_FILE"
        $ALLOCATOR/$ALLOCATOR $GRAPH_FILE
        echo ">>Evaluate on the data layout re-arrganged program..."
    fi
    echo ""

    # evaluate on the data layout re-arrganged program
    
    echo "pin -t $Evaluator -it $TRACE_FILE -o $OPTI_STATS_FILE -id $PARTITIONED_GRAPH_FILE  -- $BENCHMARK_FILE" 
    pin -t $Evaluator -it $TRACE_FILE -o $OPTI_STATS_FILE -id $PARTITIONED_GRAPH_FILE  -- $BENCHMARK_FILE
    echo ""
done


