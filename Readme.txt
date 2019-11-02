The following description is for completeness.

=============Overall Introduction========================
This experiment setup is part of the code for the paper[1].
This experiment setup is built upon Intel Pin[2].
The layout of the direcotry includes:
1) basicAllocator: this is an example allocator with heuristic optimization (refer to [1]).
2) myAllocator: this is for the competitors to place the source code of their allocator. 
3) output: this is for output files of all benchmarks.
4) pin-3.6-97554-g31f0a167d-gcc-linux: this is for the offical release of the Intel PIN(you should extract the .tar.gz file into the root directory first).
5) pintools: this is our implementation of several tools based on Intel PIN(this is a link file into the pin directory, for convenience).
	a) volatileCache.H: a volatle cache simulator with LRU replacement policy.
	b) SymbolTracer.cpp: generating the encoded trace of an executable of benchmark.
	c) PairwiseGrapher.cpp: given a trace file,  it outputs the data objects' pair-wise graph (refer to [1])
	d) Evaluator.cpp: a cache simulator, given a trace file and the allocation result, it outputs it outputs the evaluation of performance statistics for optimized program. 
	
(6) powerstone: the source of the benchmarks
(7) init.sh: this script builds the benchmarks and pintools, generates the encoded trace, generating the pair-wise graph [refer to [1]. Note: we strongly advise not to run init.sh more than once, as the Address space layout randomization (ASLR) may cause the trace changing again and again.
(8) run.sh: this script needs an argument to specify the "allocator". Given a pair-wise graph, it uses the specified allocator to generate an allocation results.
(9) result.sh: a simple tool to compare the statistics among different allocators.

Reference:
[1] Li Q, He Y, Li J, et al. Compiler-assisted refresh minimization for volatile STT-RAM cache[J]. IEEE Transactions on Computers, 2015, 64(8): 2169-2181.
[2] https://software.intel.com/en-us/articles/pin-a-binary-instrumentation-tool-downloads


=============How to propose a new allocator========================
1. Task
You should implement an allocator. Given a pair-wise graph, the allocator partition vertices of the graph into different groups, with regarding to:
1) assumming each group can hold at most 8 vertices
2) calculating the weight of each group by accumulating the weight of edges whose two-vertices are allocated into this group.
3) calculating the overall weight bu accumulating the weight of all groups.
4) the key point is to provide an allocation or graph partition such that the overall weight is minimum.

2. Programming
1) The download repository provides most code, the trace file (a.trace) as well as the pair-wise graph (a.graph) for each benchmark from powerstone. You should extract the pin*.tar.gz file into the root directory for future using of pintools.
2) Using "./run.sh ori", which invokes the cache simulator (a.trace & a null alloc -> a.ori.stats ) to evaluate the performance statistics of the original gcc generated data layout.
3) Using "./run.sh basicAllocator", which builds the basicAllocator, invokes the allocator (pair-wise graph named a.graph -> alloation result named a.basicAllocator.alloc), and then invokes the cache simulator (a.trace & a.basicAllocator.alloc -> a.basicAllocator.stats ) to evaluate the performance statistics.
4) Implement your allocator and place the source code in ./myAllocator, provide a makefile too
5) Using "./run.sh myAllocator", which builds the allocator, invokes the allocator (pair-wise graph named a.graph -> alloation result named a.myAllocator.alloc), and then invokes the cache simulator (.trace & .myAllocator.alloc -> .myAllocator.stats ) to evaluate the performance statistics.

Now, you have three .statics(a.ori.stats, a.basicAllocator.stats, a.myAllocator.stats) for each benchmark, from 1), 2), and 4) respectively. Theorically, a successful implementation means a.myAllocator.statics has minimum "Total refresh" and minimum "Total cycles". To check it, you may refer to: 
>./result.sh "Total refresh"
or
>./result.sh "Total cycles".

