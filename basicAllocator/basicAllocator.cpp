#include <string>
#include <stdint.h>
#include <sstream>
#include <map>
#include <fstream>
#include <set>
#include <list>
#include <assert.h>
#include <iostream>

using namespace std; 

typedef int64_t INT64;
typedef uint64_t ADDRINT;
typedef unsigned int UINT32;

#define NPOS -1

class CBlock
{
public:	
	CBlock() { _size = 0; }
	int Add(ADDRINT obj) { ++_size; _elements.insert(obj);}

public:	
	UINT32 _size;
	ADDRINT _rep;
	set<ADDRINT> _elements;	
};

UINT32 g_blockSize = 32;
UINT32 g_capacity = g_blockSize/4;

// szInputFile: graph file,  szOutFile: blocks file
std::string g_szInputFile;
std::string g_szOutFile;

// graph loaded from szInputFile
map<ADDRINT, map<ADDRINT, map<ADDRINT, ADDRINT> > > g_graph;
// blocks to output 
map<ADDRINT, list<CBlock *> > g_Blocks;

int Run();
int BuildGraph();
int DoAllocate();
int Dump();


int main(int argc, char *argv[])
{
	if( argc < 2)
	{
		cerr << "Usage: allocate graphFile" << endl;
		return -1;
	}

	g_szInputFile = argv[1];
	g_szOutFile = g_szInputFile.substr(0,g_szInputFile.rfind('.')) + ".alloc";	
	
	Run();
	return 0;
}

int Run() 
{
    BuildGraph();
	DoAllocate();	
	Dump();
	
	return 0;
}


int BuildGraph() 
{
    ifstream inf;
	
	string szLine;
	ADDRINT nFunc;
	ADDRINT obj1;
	
	inf.open(g_szInputFile.c_str() );
	while(inf.good() )
	{
		getline(inf, szLine);
		if( szLine.size() < 2 )
			continue;
		if( szLine.find("###") == 0 )  // a new function
		{
			stringstream ss(szLine.substr(3));
			ss >> nFunc;
			continue;
		}
		UINT32 index = szLine.find(":");
		if( index != NPOS )  // a new obj1
		{
			string szObj1 = szLine.substr(0, index);
			stringstream ss(szObj1);
			ss >> obj1;
			continue;
		}
		
		while( (index=szLine.find(";")) != NPOS)
		{
			string szRecord = szLine.substr(0,index);
			stringstream ss(szRecord);
			ADDRINT obj2, cost;
			ss >> obj2 >> cost;				
			g_graph[nFunc][obj1][obj2] = g_graph[nFunc][obj2][obj1] = cost;					
			szLine = szLine.substr(index+1);
			if( szLine.size() < 2 )
				break;
		}
	}
	
	inf.close();
	return 0;
}


int Dump()
{
    ofstream os;
    os.open(g_szOutFile.c_str());
	map<ADDRINT, list<CBlock *> >::iterator I = g_Blocks.begin(), E = g_Blocks.end();
	for( ; I != E; ++ I)
	{
		ADDRINT nFunc = I->first;
		os << endl << "###" << nFunc <<  endl;
		list<CBlock *>::iterator J = I->second.begin(), E1 = I->second.end();
		for(; J != E1; ++ J)
		{
			CBlock *pBlock = *J;
			if( I->first == 0)
				os << endl << pBlock->_rep << ":\t";
			else
				os << endl << (int) pBlock->_rep << ":\t";
			set<ADDRINT>::iterator K = pBlock->_elements.begin(), E2 = pBlock->_elements.end();
			for(; K != E2; ++ K)
			{
				if( nFunc == 0)
					os << *K << "; ";
				else 
					os << (int)*K << "; ";
			}
			os << endl;
			delete pBlock;
		}
	}
    os.close();
    // cerr << "Generating " << g_szOutFile << endl;
    return 0;
}

int AdjacentTable(ADDRINT nFunc, map<ADDRINT, set<ADDRINT> > &Atable)
{
	map<ADDRINT, map<ADDRINT, ADDRINT> > &fGraph = g_graph[nFunc];
	std::map<ADDRINT, map<ADDRINT, ADDRINT> >::iterator I = fGraph.begin(), E = fGraph.end();
	for(; I != E; ++ I)
	{
		std::map<ADDRINT, map<ADDRINT, ADDRINT> >::iterator J = fGraph.begin();
		for(; J != E; ++ J)
		{
			if( I != J)
			{
				Atable[I->first].insert(J->first);
				Atable[J->first].insert(I->first);
			}
		}
	}
	
	return 0;
}

int UpdateGraph(map<ADDRINT, map<ADDRINT, ADDRINT> > &fGraph, 
    map<ADDRINT, set<ADDRINT> > &Atable, CBlock *pBlock, ADDRINT obj)
{
	ADDRINT repObj = pBlock->_rep;
	std::set<ADDRINT> &neighbors = Atable[obj]; 
	set<ADDRINT>::iterator I = neighbors.begin(), E = neighbors.end();
	for(; I != E; ++ I)
	{
		fGraph[repObj][*I] = min(fGraph[repObj][*I], fGraph[obj][*I]);
		fGraph[*I][repObj] = fGraph[repObj][*I];
		
		Atable[*I].erase(obj);		
	}
	return 0;
}

bool FindBest(const set<ADDRINT> &allocated, ADDRINT &obj, 
    map<ADDRINT, ADDRINT> &costM, set<ADDRINT> &neighbors)
{
	bool bFound = false;
	ADDRINT best;
	ADDRINT bestCost;
	std::set<ADDRINT>::iterator I = neighbors.begin(), E = neighbors.end();
	for(; I != E; ++ I)
	{
		if( allocated.find(*I) != allocated.end())
			continue;
		ADDRINT cost = costM[*I];
		if( !bFound || cost < bestCost)
		{
			bFound = true;
			best = *I;
			bestCost = cost;
		}
	}	
	obj = best;
	return bFound;
}

int AddAllocate(set<ADDRINT> &allocated, CBlock *pBlock, ADDRINT obj, 
    map<ADDRINT, map<ADDRINT, ADDRINT> > &fGraph, map<ADDRINT, set<ADDRINT> > &Atable)
{
	if( pBlock->_size == g_capacity)    // if this block is full now
	{		
		return 0;
	}
	ADDRINT obj2;
	bool bFound = FindBest(allocated, obj2, fGraph[obj], Atable[obj]);
	if( !bFound )   // allocation ends
		return 1;
	pBlock->Add(obj2);
	allocated.insert(obj2);
	UpdateGraph(fGraph, Atable, pBlock, obj2);	
	return AddAllocate(allocated, pBlock, obj, fGraph, Atable);		
}


int DoAllocate() 
{
    map<ADDRINT, map<ADDRINT, map<ADDRINT, ADDRINT> > >::iterator i_p = g_graph.begin(), i_e = g_graph.end();
	for(; i_p != i_e; ++ i_p )
	{
		ADDRINT nFunc = i_p->first;
		// cerr << endl << "For ###" << nFunc << endl;
		int L = g_blockSize/4;
		int nBlocks = (g_graph[nFunc].size() + L - 1)/L;
		map<ADDRINT, map<ADDRINT, ADDRINT> > &fGraph = g_graph[nFunc];
		list<CBlock *> &blocks = g_Blocks[nFunc];
		
		// make use of adjacent table for efficiency
		map<ADDRINT, set<ADDRINT> > Atable;
		AdjacentTable(nFunc, Atable);
		
		// initialize the block list to be allocated
		set<ADDRINT> allocated;
		for(int i = 0; i < nBlocks; ++ i)
        {
            CBlock *cb = new CBlock();
            blocks.push_back(cb);
        }
		
		std::map<ADDRINT, std::set<ADDRINT> >::iterator I = Atable.begin(), E = Atable.end();		
		for(; I != E; ++ I)
		{
			ADDRINT obj1 = I->first;
			if( allocated.find(obj1) != allocated.end())
				continue;
			// the represent object is allcoated ?? No, alloate it
			// skip allocated && non-represent objects
			CBlock *pBlock = blocks.front();
			if( pBlock->_size != 0)       // skip non-empty blocks
				break;
			blocks.pop_front();
			blocks.push_back(pBlock);
			
			pBlock->Add(obj1);
			allocated.insert(obj1);
			pBlock->_rep = obj1;	

			int bOver = AddAllocate(allocated, pBlock, obj1, fGraph, Atable);	
			if( bOver)
				break;	
		}            
	}
	return 0;
}
