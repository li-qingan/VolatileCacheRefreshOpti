/*
 * This file is for generating the symbolized memory trace (currently only collecting local symbols)
 * 1. collect the user functions, and each user function's instruction-start and instruction-end addresses
 * 2. for each user instruction, compare the operand's address with the function's stack base address
 * Input: the application code
 * Output: the encoded trace
 */

#include "pin.H"

#include <iostream>
#include <fstream>
#include <string>
#include <set>
#include <map>
#include <sstream>
//#include "cacheL1.H"
//#include "volatileCache.H"

using namespace std;

#define UINT64 ADDRINT

/* ===================================================================== */
/* Commandline Switches */
/* ===================================================================== */

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE,    "pintool",
    "o", "trace", "specify the output trace file");
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
	"hw", "0", "hardware optimization: Lihai, Xieyuan, Jason");

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

ofstream g_traceFile;
ofstream g_outputFile;

map<ADDRINT, int> g_hFunc2Esp;
set<UINT32> g_largeFuncSet;
map<ADDRINT, UINT32> g_hCurrentFunc;

ADDRINT RefreshCycle;
UINT32 g_memoryLatency;

// latency
const ADDRINT g_rLatL1 = 2;
const ADDRINT g_wLatL1 = 4;
const ADDRINT g_rLatL2 = 200;
const ADDRINT g_wLatL2 = 200;

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
		
	void DumpGraph(ostream &os);
}


//static ADDRINT g_prevCycle = 0;
ADDRINT g_EndOfImage = 0;
/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
    cerr <<
        "This tool generates the symbolized memory trace (currently only collecting local symbols).\n"
        "\n";

    cerr << KNOB_BASE::StringKnobSummary() << endl; 
    return -1;
}

/* ===================================================================== */
VOID LoadInst(ADDRINT addr)
{
	g_traceFile << endl << "I:" << addr;
	//cerr << "LoadInst for " << hex << addr << ": " << ++g_testCounter << endl;		
}
/* ===================================================================== */

VOID LoadSingle(ADDRINT addr)
{
	g_traceFile << endl << "R:";
	if( addr < g_EndOfImage)
		g_traceFile << "G:";
	else
		g_traceFile << "H:";
	g_traceFile << addr;  
	//cerr << "LoadSingle for " << addr << endl;
	

}
/* ===================================================================== */

VOID StoreSingle(ADDRINT addr)
{	
	g_traceFile << endl << "W:";
	if( addr < g_EndOfImage)
		g_traceFile << "G:";
	else
		g_traceFile << "H:";
	g_traceFile << addr;  
}


/* ===================================================================== */
VOID Instruction(INS ins, void * v)
{		
	INS_InsertPredicatedCall(ins, 
				IPOINT_BEFORE,  (AFUNPTR) LoadInst, 
				IARG_ADDRINT, INS_Address(ins),
				IARG_END);
	// skip stack access here for memory access
	//if( INS_IsStackRead(ins) || INS_IsStackWrite(ins) )
		//return;   
}

void OnMemory(INS ins, void *v)
{
	 if (INS_IsMemoryRead(ins))
    {
        // map sparse INS addresses to dense IDs
        //const UINT32 size = INS_MemoryReadSize(ins);      
		INS_InsertPredicatedCall(
			ins, IPOINT_BEFORE, (AFUNPTR) LoadSingle,
			IARG_MEMORYREAD_EA,
			IARG_CONTEXT,
			IARG_END);		
    }        
    else if ( INS_IsMemoryWrite(ins) )
    {
        // map sparse INS addresses to dense IDs  
		INS_InsertPredicatedCall(
			ins, IPOINT_BEFORE,  (AFUNPTR) StoreSingle,
			IARG_MEMORYWRITE_EA,
			IARG_CONTEXT,
			IARG_END);		
    }
}
/* ===================================================================== */
/* get instruction the stack base/top address                                                                 */
/* ===================================================================== */

VOID StackAccessDump(ADDRINT addr, ADDRINT nFunc, UINT32 oriDisp, bool bRead)
{	
	if( bRead)
		g_traceFile << endl << "R:";
	else
		g_traceFile << endl << "W:";
	g_traceFile << "S:";
	g_traceFile << nFunc << ":";
	g_traceFile << oriDisp << ":";
	g_traceFile << addr;  
}

VOID OnStackAccess(INS ins, ADDRINT nFunc)
{
	bool bRead = INS_IsStackRead(ins);
	ADDRINT disp = INS_MemoryDisplacement(ins);
	//RTN rtn = INS_Rtn(ins);
	//ADDRINT nFunc = RTN_Address(rtn);
	
	
	if( bRead )	
		INS_InsertCall(ins,
			IPOINT_BEFORE, AFUNPTR(StackAccessDump),
			IARG_MEMORYREAD_EA,	
			IARG_ADDRINT, nFunc,
			IARG_UINT32, disp,
			IARG_BOOL, bRead,
			//IARG_BOOL, bEsp,
			IARG_END);				
	else
		INS_InsertCall(ins,
			IPOINT_BEFORE, AFUNPTR(StackAccessDump),
			IARG_MEMORYWRITE_EA,	
			IARG_ADDRINT, nFunc,
			IARG_UINT32, disp,
			IARG_BOOL, bRead,
			//IARG_BOOL, bEsp,
			IARG_END);	
}
/* ===================================================================== */
/* get user functions' instruction-start and instruction-end addresses                                                                 */
/* ===================================================================== */
VOID Image(IMG img, VOID *v)
{
	g_EndOfImage = IMG_HighAddress(img);
	// cerr << endl << "End of image:\t" << IMG_Name(img) << "@" << hex << g_EndOfImage <<dec << endl;
	g_traceFile << endl << "#" << g_EndOfImage;
	for( SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec) )
	{
		for( RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn) )
		{			
			RTN_Open(rtn);
			ADDRINT nFunc = RTN_Address(rtn);
			//cout << endl << dec << nFunc << ":" << RTN_Name(rtn).c_str() << endl;
			bool bLargeFunc = false;
			// 1. track the change of stack frame of user functions, by searching "sub $24, esp"
			for( INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins) )
			{
				//cout << hex << INS_Address(ins) << endl;	
				bool bTrack = false;					
				if( !bLargeFunc
					&& INS_Opcode(ins) == XED_ICLASS_SUB &&
					INS_OperandIsImmediate( ins, 1) &&
					INS_OperandIsReg( ins ,0) && 
					INS_OperandReg( ins, 0) == REG_STACK_PTR )
				{				
					// treat functions with stack less than 1x block size as small functions
					UINT32 nOffset = (UINT32) INS_OperandImmediate(ins, 1);
					//cerr << endl << "Offset:\t" << nOffset << endl;
					if( nOffset >= KnobLineSize.Value() * 1 )
					{
						//g_largeFuncSet.insert(i2s_p->second);	
						bLargeFunc = true;
					}						
				}
	
				// 2. track stack access by user functions
				// instruction accesses memory relative to ESP or EBP, the latter may be used
				// as a general register and thus mislead the judgement
				if( bLargeFunc && (INS_IsStackRead(ins) || INS_IsStackWrite(ins) ) )
				{
					bool bEsp = false;
					ADDRINT disp = INS_MemoryDisplacement(ins);
					if( INS_MemoryBaseReg(ins) == REG_STACK_PTR )
						bEsp = true;
					if(disp != 0 && bEsp)
						bTrack = true;
				}
				
						
										
				// track stack write access by ESP+0x23					
				Instruction(ins, v);
				if( bTrack )
				{																			
					OnStackAccess(ins, nFunc);			

				}		
				else
				{						
					OnMemory(ins, v);
				}
			}
			RTN_Close(rtn);
		}
	}	
}

/* ===================================================================== */

VOID Fini(int code, VOID * v)
{
	// Finalize the work

	g_traceFile.close();	
}
/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char *argv[])
{
    PIN_InitSymbols();

    if( PIN_Init(argc,argv) )
    {
        return Usage();
    }    	
    
	g_traceFile.open(KnobOutputFile.Value().c_str() );
	if(!g_traceFile.good())
		cerr << "Failed to open " << KnobOutputFile.Value().c_str();
	
	// 1. Collect user functions from a external file
	//GetUserFunction();
	// 2. Collect the start address of user functions
	IMG_AddInstrumentFunction(Image, 0);
	// 3. Collect dynamic stack base address when function-calling
	// 4. Deal with each instruction	
    //INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    // Never returns

    PIN_StartProgram();
    
    return 0;
}

