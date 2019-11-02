/*
 * This file is for estimating refresh schemes for data layout re-arranged programs
 * Input:
 * 1) the encoded trace which distinguishes between instructions, stack/global/heap read/write
 * 2) the data map as allocation results
 * Output: the optimized results
 */

#include "pin.H"

#include <iostream>
#include <fstream>
#include <string>
#include <set>
#include <map>
#include <sstream>
#include "cacheL1.H"
#include "volatileCache.H"

using namespace std;

#define UINT64 ADDRINT
#define UINT32 unsigned int

/* ===================================================================== */
/* Commandline Switches */
/* ===================================================================== */

KNOB<string> KnobTraceFile(KNOB_MODE_WRITEONCE,    "pintool",
    "it", "trace", "specify the input trace file");
KNOB<string> KnobDatamapFile(KNOB_MODE_WRITEONCE,    "pintool",
    "id", "datamap", "specify the input data map file");
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE,    "pintool",
    "o", "stats", "specify the output stats file");
KNOB<UINT32> KnobCacheSize(KNOB_MODE_WRITEONCE, "pintool",
    "c","32", "cache size in kilobytes");
KNOB<UINT32> KnobLineSize(KNOB_MODE_WRITEONCE, "pintool",
    "b","32", "cache block size in bytes");
KNOB<UINT32> KnobAssociativity(KNOB_MODE_WRITEONCE, "pintool",
    "a","4", "cache associativity (1 for direct mapped)");
KNOB<bool> KnobEnableTrace(KNOB_MODE_WRITEONCE, "pintool",
	"t", "0", "enbale trace output");
KNOB<UINT32> KnobRetent(KNOB_MODE_WRITEONCE, "pintool",
	"r", "53000", "retention time" );
KNOB<UINT32> KnobMemLat(KNOB_MODE_WRITEONCE, "pintool",
	"m", "300", "memory latency" );
KNOB<int> KnobOptiHw(KNOB_MODE_WRITEONCE, "pintool",
	"hw", "2", "hardware optimization: Lihai, Xieyuan, Jason");
KNOB<int> KnobWriteLatency(KNOB_MODE_WRITEONCE, "pintool",
	"wl", "4", "write latency: 3,4,5,6,7,8");

/* ===================================================================== */
/* Data structure                                                  */
/* ===================================================================== */
typedef struct FuncRecord
{
	string _name;
	//string _image;
	ADDRINT _startAddr;
	ADDRINT _endAddr;
} FuncRec;

typedef struct ActiveRecord
{
	FuncRec *_fr;
	ADDRINT _esp;
	ADDRINT _ebp;
	ADDRINT _subValue;
	ADDRINT _func;
}ActiveRec;

/* ===================================================================== */
/* Global variables */
/* ===================================================================== */
extern ADDRINT g_EndOfImage;
extern ADDRINT g_CurrentEsp;
extern UINT32 g_BlockSize;
set<string> g_UserFuncs;
map<string, ADDRINT> g_hFuncs;
map<ADDRINT, FuncRec *> g_hFunc2Recs;
map<ADDRINT, ActiveRec *> g_hEsp2ARs;

map<ADDRINT, map<ADDRINT, ADDRINT> > g_DataMap;

ifstream g_traceFile;
ifstream g_datamapFile;
ofstream g_outputFile;

map<ADDRINT, int> g_hFunc2Esp;
set<UINT32> g_largeFuncSet;
map<ADDRINT, UINT32> g_hCurrentFunc;

ADDRINT RefreshCycle;
UINT32 g_memoryLatency;

// latency
const ADDRINT g_rLatL1 = 2;
ADDRINT g_wLatL1 = 4;

ADDRINT g_testCounter = 0;
// trace output
namespace Graph
{
	struct Object
	{
		int _object;
		ADDRINT _cycle;
	};
	struct Global
	{
		ADDRINT _addr;
		ADDRINT _cycle;
	};
	list<Global> g_gTrace;
	map<ADDRINT, map<ADDRINT, ADDRINT> > g_gGraph;
	map<ADDRINT, list<Object> > g_trace;       // function->object->cycle
	map<ADDRINT, map<int, map<int, ADDRINT> > > g_graph;   // function->object->object->cost
	
	void DumpGraph(ostream &os);
}


//static ADDRINT g_prevCycle = 0;

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
    cerr <<
        "This tool represents a cache simulator.\n"
        "\n";

    cerr << KNOB_BASE::StringKnobSummary() << endl; 
    return -1;
}

namespace DL1
{
    const UINT32 max_sets = 16 * KILO; // cacheSize / (lineSize * associativity);
    const UINT32 max_associativity = 16; // associativity;
    const CACHE_ALLOC::STORE_ALLOCATION allocation = CACHE_ALLOC::STORE_ALLOCATE;
	
	typedef CACHE1<CACHE_SET::Volatile_LRU_CACHE_SET<max_associativity>, max_sets, allocation> CACHE;
}

DL1::CACHE* dl1 = NULL;

namespace IL1
{
    const UINT32 max_sets = 16 * KILO; // cacheSize / (lineSize * associativity);
    const UINT32 max_associativity = 16; // associativity;
    const CACHE_ALLOC::STORE_ALLOCATION allocation = CACHE_ALLOC::STORE_ALLOCATE;
	
	typedef CACHE1<CACHE_SET::Volatile_LRU_CACHE_SET<max_associativity>, max_sets, allocation> CACHE;
}

IL1::CACHE* il1 = NULL;


/* ===================================================================== */
VOID LoadInst(ADDRINT addr)
{
	//cerr << "LoadInst for " << hex << addr << ": " << ++g_testCounter << endl;

	(void)il1->AccessSingleLine(addr, ACCESS_BASE::ACCESS_TYPE_LOAD, 0);
		
}
/* ===================================================================== */

VOID LoadSingle(ADDRINT addr, int nArea)
{
	//cerr << "LoadSingle for " << addr << endl;
	map<ADDRINT, ADDRINT> &dataMap = g_DataMap[0];
	map<ADDRINT, ADDRINT>::iterator d_p = dataMap.find(addr);
	if( d_p != dataMap.end() )
	{
		(void)dl1->AccessSingleLine(d_p->second, ACCESS_BASE::ACCESS_TYPE_LOAD, nArea);
		//cerr <<hex << endl << "mapping:\t" << addr << " -> " << d_p->second ;
	}
	else
		(void)dl1->AccessSingleLine(addr, ACCESS_BASE::ACCESS_TYPE_LOAD, nArea);
}
/* ===================================================================== */

VOID StoreSingle(ADDRINT addr, int nArea)
{	
	map<ADDRINT, ADDRINT> &dataMap = g_DataMap[0];
	map<ADDRINT, ADDRINT>::iterator d_p = dataMap.find(addr);
	if( d_p != dataMap.end() )
	{
		(void)dl1->AccessSingleLine(d_p->second, ACCESS_BASE::ACCESS_TYPE_STORE, nArea);
		//cout <<hex << endl << "-global:\t" << addr << " -> " << d_p->second ;
	}
	else
		(void)dl1->AccessSingleLine(addr, ACCESS_BASE::ACCESS_TYPE_STORE, nArea);
	//cout <<hex << endl << "mapping-global:\t" << addr ;
}


VOID OnStackAccess( ADDRINT nFunc, int oriDisp, ADDRINT oriAddr, bool bRead)
{
	ADDRINT addr = oriAddr;
	map<ADDRINT, ADDRINT> dataMap = g_DataMap[nFunc];
	map<ADDRINT, ADDRINT>::iterator d_p = dataMap.find(oriDisp);
	if( d_p != dataMap.end() )
	{
		int disp2 = (int) d_p->second;
		int disp1 = (int) oriDisp;
		//cout << hex << endl << "map-stack:\t" << oriAddr << "(" << disp1 << ") -> " << oriAddr+disp2-disp1 << "(" << disp2 << ")";
		
		addr = oriAddr + disp2 - disp1;		
	}
	//cout << hex << endl << "map-stack in func: " << nFunc << ":\t" << oriAddr << " -> " << addr ;
	//cerr << endl << addr << "(" << disp1 << ") -> " << addr+disp2-disp1 << "(" << disp2 << ")";
	if( bRead)
		(void)dl1->AccessSingleLine(addr, ACCESS_BASE::ACCESS_TYPE_LOAD, 0);
	else
		(void)dl1->AccessSingleLine(addr, ACCESS_BASE::ACCESS_TYPE_STORE, 0);	
}
/* ===================================================================== */
/* get user functions' instruction-start and instruction-end addresses   
 * trace format:
 * 'I:addr': instruction execution on addr
 * 'R:S:4200208:12:14073623860994': memory read on the stack of function-ptr (4200408) with offset (12) and address (140...)
 * 'W:addr': memory read on addr
 * ===================================================================== */
VOID Image(VOID *v)
{
	int nArea = 0;     // 0 for stack, 1 for global, 2 for heap
	// int nHead = 0;
	g_traceFile.open(KnobTraceFile.Value().c_str() );	
	if(!g_traceFile.good())
		cerr << "Failed to open " << KnobTraceFile.Value().c_str();
	string szLine;
	while(g_traceFile.good())
	{
		getline(g_traceFile, szLine);
		if( szLine.size() < 1)
			continue;
		if( 'I' == szLine[0] )
		{
			ADDRINT addr;
			string szAddr = szLine.substr(2);
			stringstream ss(szAddr);
			ss >> addr;
			
			LoadInst(addr);			
		}
		else if( 'R' == szLine[0] || 'W' == szLine[0])
		{
			bool bRead = ('R' == szLine[0]);
			// R:S:4200208:12:14073623860994
			if( 'S' == szLine[2] )
			{
				UINT32 index1 = szLine.find(':', 4);
				string szStr = szLine.substr(4, index1-4);
				stringstream ss1(szStr);
				ADDRINT nFunc;
				ss1 >> nFunc;
				
				
				UINT32 index2 = szLine.find(':', index1+1);
				szStr=szLine.substr(index1+1, index2-index1-1);
				int disp;
				stringstream ss2(szStr);
				ss2 >> disp;
				
				szStr = szLine.substr(index2+1);
				ADDRINT addr;
				stringstream ss3(szStr);
				ss3 >> addr;
				
				OnStackAccess(nFunc, disp, addr, bRead);			
			}
			// W:G:6962120
			else if( 'G' == szLine[2])
			{
				nArea = 1;
				ADDRINT addr;
				string szAddr = szLine.substr(4);
				stringstream ss(szAddr);
				ss >> addr;
				if( bRead)
					LoadSingle(addr, nArea);	
				else
					StoreSingle(addr, nArea);
			}
			else if( 'H' == szLine[2] )
			{
				nArea = 2;
				ADDRINT addr;
				string szAddr = szLine.substr(4);
				stringstream ss(szAddr);
				ss >> addr;
				
				if(bRead)
					LoadSingle(addr, nArea);	
				else
					StoreSingle(addr, nArea);
			}
		}
		else if( '#' == szLine[0])
		{
			stringstream ss(szLine.substr(1));
			ss >> g_EndOfImage;
		}
	}	
}

/* ===================================================================== */

VOID Fini(int code, VOID * v)
{
	// Finalize the work
	dl1->Fini();
	//char buf[256];
	//sprintf(buf, "%u",KnobOptiHw.Value());
	
	//string szOutFile = KnobOutputFile.Value() +"_" + buf;
	g_outputFile.open(KnobOutputFile.Value().c_str() );	
	if(!g_outputFile.good())
		cerr << "Failed to open " << KnobOutputFile.Value().c_str();
	g_outputFile << "#Parameters:\n";
	g_outputFile << "L1 read/write latency:\t" << g_rLatL1 << "/" << g_wLatL1 << " cycle" << endl;
	g_outputFile << "Memory read/write latency:\t" << g_memoryLatency << " cycle" << endl;
	g_outputFile << il1->StatsLong("#", CACHE_BASE::CACHE_TYPE_ICACHE);
	g_outputFile << dl1->StatsLong("#", CACHE_BASE::CACHE_TYPE_DCACHE);	
	CACHE_SET::DumpRefresh(g_outputFile);
	g_outputFile.close();
	g_traceFile.close();
	
}

// read the allolcation results
void ReadMap(ifstream &inf)
{
	ADDRINT nFunc;
	ADDRINT rep;
	string szLine;
	while(inf.good() )
	{
		getline(inf, szLine);
		if( szLine.size() < 4 )
			continue;
		if( szLine.find("###") == 0 )
		{
			stringstream ss(szLine.substr(3));
			//cout << "line:\t" << szLine << endl;
			ss >> nFunc;
		//	cout << dec << "read func in map: " << nFunc << endl;
			continue;
		}
		// read the representation data objects of a memory block
		UINT32 index = szLine.find(":");
		if( index != 0xffffffff )
		{
			string szRep = szLine.substr(0,index);
			stringstream ss(szRep);
			ss >> rep;
			//continue;
		}
		
		// read the other data objects of a memory block
		while( (index=szLine.find(";") ) != 0xffffffff)
		{
			string szObj = szLine.substr(0,index);
			ADDRINT obj;
			stringstream ss(szObj);
			ss >> obj;
			g_DataMap[nFunc][obj] = rep;
			//if( obj != rep) cout << hex << obj << " >> " << rep << "\t";
			szLine = szLine.substr(index+1);
			if( szLine.size() < 4)
				break;
		}
	} 
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char *argv[])
{
    //PIN_InitSymbols();

    if( PIN_Init(argc,argv) )
    {
        return Usage();
    }    
	
	g_datamapFile.open(KnobDatamapFile.Value().c_str());
	if( !g_datamapFile.good())
		cerr << "Failed to open " << KnobDatamapFile.Value() << endl;
	ReadMap(g_datamapFile);
	g_datamapFile.close();

	g_wLatL1 = KnobWriteLatency.Value();
	dl1 = new DL1::CACHE("L1 Data Cache", 
		KnobCacheSize.Value() * KILO,
		KnobLineSize.Value(),
		KnobAssociativity.Value());
	dl1->SetLatency(g_rLatL1, g_wLatL1);
	il1 = new IL1::CACHE("L1 Instruction Cache", 
		32 * KILO, 
		64,
		4);
	il1->SetLatency(g_rLatL1,g_wLatL1);
	
	opti_hardware = KnobOptiHw.Value();
	g_BlockSize = KnobLineSize.Value();
	RefreshCycle = KnobRetent.Value()/4*4;
	g_memoryLatency = KnobMemLat.Value();	
	
	Image(0);
	Fini(0,0);
    
    return 0;
}

void Graph::DumpGraph(ostream &os)
{
	os << "###0" << endl;
	map<ADDRINT, map<ADDRINT, ADDRINT> >::iterator i2i_p = g_gGraph.begin(), i2i_e = g_gGraph.end();
	for(; i2i_p != i2i_e; ++ i2i_p)
	{
		os << i2i_p->first << ":" << endl;
		map<ADDRINT, ADDRINT>::iterator i_p = i2i_p->second.begin(), i_e = i2i_p->second.end();
		int i = 0;
		for(; i_p != i_e; ++ i_p)
		{
			if( i_p->second != 0)
			{
				++ i;
				os << i_p->first << "  " << i_p->second << ";\t";
				if( i %6 == 0)
					os << endl;
			}
		}
		os << endl;
	}
	map<ADDRINT, map<int, map<int, ADDRINT> > >::iterator i2i2i_p = g_graph.begin(), i2i2i_e = g_graph.end();
	for(; i2i2i_p != i2i2i_e; ++ i2i2i_p)
	{
		os << "###" << i2i2i_p->first << endl;
		map<int, map<int, ADDRINT> >::iterator i2i_p = i2i2i_p->second.begin(), i2i_e = i2i2i_p->second.end();
		for(; i2i_p != i2i_e; ++ i2i_p)
		{
			os << i2i_p->first << ":" << endl;
			map<int, ADDRINT>::iterator i_p = i2i_p->second.begin(), i_e = i2i_p->second.end();
			int i = 0;
			for(; i_p != i_e; ++ i_p)
			{
				if( i_p->second != 0)
				{
					++ i;
					os << i_p->first << "  " << i_p->second << ";\t";
					if( i %6 == 0)
						os << endl;
				}
			}
			os << endl;			
		}
	}
}
