# Introduction
This repositoy is part of the experimental code for the paper[1].

This experiment setup is built upon Intel Pin[2].

## Layout of direcotry
- basicAllocator: 
	
	this is an example allocator with heuristic optimization (refer to [1]).
	
- myAllocator: 
	
	this is for the competitors to place the source code of their customed allocator. 
	
- output: 
	
	this is for output files of all benchmarks.
	
- pin-3.6-97554-g31f0a167d-gcc-linux (Refered as PinLibDir here): 

	this is the directory for the offical release of the Intel PIN. It doesn't exist in the repository. You should obtain and extract the .tar.gz file into the root directory first. Newer version of pin lib is likely to work (perhaps you need revising the shell script file accordingly), without guaranteen.
	
- pin-tools: 

	this is our implementation of several tools based on Intel PIN (this is a link file into the pin directory, for convenience).
	
	- volatileCache.H: a volatle cache simulator with LRU replacement policy.
	- SymbolTracer.cpp: generating the encoded trace of an executable of benchmark.
	- PairwiseGrapher.cpp: given a trace file,  it outputs the data objects' pair-wise graph (refer to [1])
	- Evaluator.cpp: a cache simulator, given a trace file and the allocation result, it outputs it outputs the evaluation of performance statistics for optimized program. 
	
- powerstone: 
	
	the source code of powerstone benchmark
	
- init.sh: 
	
	this script builds the benchmarks and pintools, generates the encoded trace, generating the pair-wise graph [refer to [1]. Note: we strongly advise not to run init.sh more than once, as the Address space layout randomization (ASLR) may cause the trace changing again and again.
	
- run.sh: 

	this script needs an argument to specify the "allocator". Given a pair-wise graph, it uses the specified allocator to generate an allocation results.
- result.sh: 
	
	a simple tool to compare the statistics among different allocators.
- pintools:

	this is a link file, linking to PinLibDir/source/tools/pintools.





The layout of the direcotry includes:

Reference:
[1] Li Q, He Y, Li J, et al. Compiler-assisted refresh minimization for volatile STT-RAM cache[J]. IEEE Transactions on Computers, 2015, 64(8): 2169-2181.
[2] https://software.intel.com/en-us/articles/pin-a-binary-instrumentation-tool-downloads


# How to propose a new allocator
## Notes
You should implement an allocator. Given a pair-wise graph, the allocator partition vertices of the graph into different groups, with regarding to:

- assumming each group can hold at most 8 vertices
- calculating the weight of each group by accumulating the weight of edges whose two-vertices are allocated into this group.
- calculating the overall weight by accumulating the weight of all groups.
- the key point is to provide an allocation or graph partition such that the overall weight is minimum.

## Step by step
1. The repository provides most code, as stated above. 
2. Download the pin\*.tar.gz from Intel Pin official site and extract into the root directory for future usage of pintools.
3. Create a link file "pintools", which links to PinLibDir/source/tools/pintools, in the root directory. This will helps both building pintools and using pintools for simulation.
4. Run "./init", to build the powerstone benchmark, to build the pin tools, and  for each benchmark program, it can
	- output X.trace: the trace of runnning X
	- output X.graph: the pair-wise graph relating data writes for X (see Ref[1])
5. Run "./run.sh ori", which invokes the cache simulator (a.trace & a null alloc -> a.ori.stats ) to evaluate the performance statistics of the original gcc generated data layout. For each benchmark program X, it:
	- input X.trace: 
	- output X.ori.stats: performance statistics
6. Run "./run.sh basicAllocator", which builds the basicAllocator. For each benchmark program, it invokes the allocator to:	- input: X.graph, the pair-wise graph named a.graph
	- output: X.basicAllocator.alloc, alloation result
	
	Then, it invokes the cache simulator to:
	
	- input: X.trace, X.basicAllocator.alloc
	- outputs: X.basicAllocator.stats, performance statistics

7. Implement your allocator and overrite the source code in ./myAllocator, provide a makefile too
8. Using "./run.sh myAllocator", which builds the allocator, invokes the allocator, and then invokes the cache simulator to evaluate the performance statistics of myAllocator. It is similar to the 6 step for basicAllocator.

9. Now, you have three .statics(X.ori.stats, X.basicAllocator.stats, X.myAllocator.stats) for each benchmark, from the 5, 6, and 8 step respectively. Theorically, a successful implementation means a.myAllocator.statics has minimum "Total refresh" and minimum "Total cycles". To check it, you may refer to: 

```
./result.sh "Total refresh"
./result.sh "Total cycles".
```

