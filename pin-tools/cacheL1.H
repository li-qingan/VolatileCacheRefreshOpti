/*BEGIN_LEGAL
Intel Open Source License

Copyright (c) 2002-2011 Intel Corporation. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.  Redistributions
in binary form must reproduce the above copyright notice, this list of
conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.  Neither the name of
the Intel Corporation nor the names of its contributors may be used to
endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */
//
// @ORIGINAL_AUTHOR: Artur Klauser
//

/*! @file
 *  This file contains a configurable cache class
 */

#ifndef PIN_CACHEL1_H
#define PIN_CACHEL1_H

#include <list>
#include <iostream>
#include <fstream>
#include "tool.H"

using namespace std;





extern ADDRINT g_CurrentCycle;
extern UINT32 g_memoryLatency;

/*!
 *  @brief Cache tag - self clearing on creation
 */
class CACHE_TAG
{
  private:
    ADDRINT _tag;

  public:
    CACHE_TAG(ADDRINT tag = 0) { _tag = tag; }
    bool operator==(const CACHE_TAG &right) const { return _tag == right._tag; }
    operator ADDRINT() const { return _tag; }
};


/*!
 * Everything related to cache sets
 */
namespace CACHE_SET
{

/*!
 *  @brief Cache set direct mapped
 */
class DIRECT_MAPPED
{
  private:
    CACHE_TAG _tag;

  public:
    DIRECT_MAPPED(UINT32 associativity = 1) { ASSERTX(associativity == 1); }

    VOID SetAssociativity(UINT32 associativity) { ASSERTX(associativity == 1); }
    UINT32 GetAssociativity(UINT32 associativity) { return 1; }

    UINT32 Find(CACHE_TAG tag) { return(_tag == tag); }
    VOID Replace(CACHE_TAG tag) { _tag = tag; }
};

/*!
 *  @brief Cache set with Least-Recent-Use replacement
 */
template <UINT32 MAX_ASSOCIATIVITY = 4>
class LRU_CACHE_SET
{
  private:
    CACHE_TAG _tags[MAX_ASSOCIATIVITY];
    UINT32 _tagsLastIndex;
    UINT32 _nextReplaceIndex;

    bool _bDirty[MAX_ASSOCIATIVITY];
	bool _bValid[MAX_ASSOCIATIVITY];  // to do: no need for one-level cache
	
	UINT32 _readPenalty;
	UINT32 _writePenalty;

  public:
  std::list<UINT32> _tagsLRU;         // LRU list
  public:
    LRU_CACHE_SET(UINT32 associativity = MAX_ASSOCIATIVITY)
      : _tagsLastIndex(associativity - 1)
    {
        ASSERTX(associativity <= MAX_ASSOCIATIVITY);
        _nextReplaceIndex = _tagsLastIndex;        
    }
	
	VOID SetPenalty(UINT32 rPenalty, UINT32 wPenalty)
	{
		_readPenalty = rPenalty;
		_writePenalty = wPenalty;
	}

    VOID SetAssociativity(UINT32 associativity)
    {
        ASSERTX(associativity <= MAX_ASSOCIATIVITY);
        _tagsLastIndex = associativity - 1;
        _nextReplaceIndex = _tagsLastIndex;

	for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
            _tags[index] = CACHE_TAG(0);
            _tagsLRU.push_front(index);     // initial the lru list

	    _bDirty[index] = false;
        }
    }
    UINT32 GetAssociativity(UINT32 associativity) { return _tagsLastIndex + 1; }

    UINT32 Find(CACHE_TAG tag, UINT32 &lineIndex, int &nWriteback)
    {
        bool result = true;

        for (INT32 index = _tagsLastIndex; index >= 0; index--)
        {
            // this is an ugly micro-optimization, but it does cause a
            // tighter assembly loop for ARM that way ...
            if(_tags[index] == tag)
			  {
			  lineIndex = index;
			  goto end;
			  }
        }
        result = false;

        end: return result;
    }

  void HitLRU(UINT32 lineIndex, bool bWrite)
  {
	  g_CurrentCycle += bWrite? _writePenalty: _readPenalty;
        if( _tagsLRU.back() == lineIndex )
            return;

        std::list<UINT32>::iterator I = _tagsLRU.begin(), E = _tagsLRU.end();
        for( ; I != E; ++ I)
            if( *I == lineIndex)
                break;
        _tagsLRU.erase(I);
        _tagsLRU.push_back(lineIndex);

        if (bWrite)
            _bDirty[lineIndex] = true;
    }

  int Replace(CACHE_TAG tag, bool bWrite, int nArea )
  {
      UINT32 lineIndex = _tagsLRU.front();
	  
	  g_CurrentCycle += bWrite? _writePenalty: _readPenalty;

      _tags[lineIndex] = tag;
      _tagsLRU.pop_front();
      _tagsLRU.push_back(lineIndex);
      _bDirty[lineIndex] = false;

      if( bWrite)
         _bDirty[lineIndex] = true;
	  return 0;
  }

  UINT32 NeedWriteback()
  {
      UINT32 lineIndex = _tagsLRU.front();
      if(_bDirty[lineIndex])
      {
          return _tags[lineIndex];
      }

      return 0;
  }

  UINT32 GetSpot()
  {
      return _tagsLRU.back();
  }
  
  int Fini()
  {
	  return 0;
  }
};

} // namespace CACHE_SET

namespace CACHE_ALLOC
{
    typedef enum
    {
        STORE_ALLOCATE,
        STORE_NO_ALLOCATE
    } STORE_ALLOCATION;
}

/*!
 *  @brief Generic cache base class; no allocate specialization, no cache set specialization
 */
class CACHE_BASE
{
  public:
    // types, constants
/*    typedef enum
    {
        ACCESS_TYPE_LOAD,
        ACCESS_TYPE_STORE,
        ACCESS_TYPE_NUM
    } ACCESS_TYPE;*/

    typedef enum
    {
        CACHE_TYPE_ICACHE,
        CACHE_TYPE_DCACHE,
		CACHE_TYPE_L2,
        CACHE_TYPE_NUM
    } CACHE_TYPE;

  protected:
    static const UINT32 HIT_MISS_NUM = 2;
    CACHE_STATS _access[ACCESS_BASE::ACCESS_TYPE_NUM][HIT_MISS_NUM];
    CACHE_STATS _writeback;
    const std::string _name;	
	
	UINT32 _readPenalty;
	UINT32 _writePenalty;

  private:    // input params

    const UINT32 _cacheSize;
    const UINT32 _lineSize;
    const UINT32 _associativity;

    // computed params
protected:
    const UINT32 _lineShift;
    const UINT32 _setIndexMask;


    CACHE_STATS SumAccess(bool hit) const
    {
        CACHE_STATS sum = 0;

        for (UINT32 accessType = 0; accessType < ACCESS_BASE::ACCESS_TYPE_NUM; accessType++)
        {
            sum += _access[accessType][hit];
        }

        return sum;
    }

  protected:
    UINT32 NumSets() const { return _setIndexMask + 1; }

  public:
    // constructors/destructors
    CACHE_BASE(std::string name, UINT32 cacheSize, UINT32 lineSize, UINT32 associativity);

    // accessors
    UINT32 CacheSize() const { return _cacheSize; }
    UINT32 LineSize() const { return _lineSize; }
    UINT32 Associativity() const { return _associativity; }

    //
    CACHE_STATS Hits(ACCESS_BASE::ACCESS_TYPE accessType) const { return _access[accessType][true];}
    CACHE_STATS Misses(ACCESS_BASE::ACCESS_TYPE accessType) const { return _access[accessType][false];}
    CACHE_STATS Accesses(ACCESS_BASE::ACCESS_TYPE accessType) const { return Hits(accessType) + Misses(accessType);}
    CACHE_STATS Hits() const { return SumAccess(true);}
    CACHE_STATS Misses() const { return SumAccess(false);}
    CACHE_STATS Accesses() const { return Hits() + Misses();}

    VOID SplitAddress(const ADDRINT addr, CACHE_TAG & tag, UINT32 & setIndex) const
    {
        tag = addr >> _lineShift;
        setIndex = tag & _setIndexMask;
    }

    VOID SplitAddress(const ADDRINT addr, CACHE_TAG & tag, UINT32 & setIndex, UINT32 & lineIndex) const
    {
        const UINT32 lineMask = _lineSize - 1;
        lineIndex = addr & lineMask;
        SplitAddress(addr, tag, setIndex);
    }

  virtual unsigned int AccessSingleLine(ADDRINT addr, ACCESS_BASE::ACCESS_TYPE accessType, int nArea) { cout << "AccessSingleLine in Base1\n"; return true;};

    virtual string StatsLong(string prefix = "", CACHE_TYPE = CACHE_TYPE_DCACHE) const { return "StatsLong in Base1";};
};

CACHE_BASE::CACHE_BASE(std::string name, UINT32 cacheSize, UINT32 lineSize, UINT32 associativity)
  : _name(name),
    _cacheSize(cacheSize),
    _lineSize(lineSize),
    _associativity(associativity),
    _lineShift(FloorLog2(lineSize)),
    _setIndexMask((cacheSize / (associativity * lineSize)) - 1)
{

    ASSERTX(IsPower2(_lineSize));
    ASSERTX(IsPower2(_setIndexMask + 1));


    for (UINT32 accessType = 0; accessType < ACCESS_BASE::ACCESS_TYPE_NUM; accessType++)
    {
        _access[accessType][false] = 0;
        _access[accessType][true] = 0;
    }
}

/*!
 *  @brief Templated cache class with specific cache set allocation policies
 *
 *  All that remains to be done here is allocate and deallocate the right
 *  type of cache sets.
 */
template <class SET, UINT32 MAX_SETS, UINT32 STORE_ALLOCATION>
class CACHE1 : public CACHE_BASE
{
  private:
    SET _sets[MAX_SETS];

  // The next level cache uses write-allocation
  CACHE_BASE *m_pNextCache;
  public:
    // constructors/destructors
    CACHE1(std::string name, UINT32 cacheSize, UINT32 lineSize, UINT32 associativity)
      : CACHE_BASE(name, cacheSize, lineSize, associativity)
    {
        _writeback = 0;
        m_pNextCache = NULL;

        ASSERTX(NumSets() <= MAX_SETS);

        for (UINT32 i = 0; i < NumSets(); i++)
        {
            _sets[i].SetAssociativity(associativity);
			//_sets[i].SetPenalty(rPenalty, wPenalty);
        }
    }

    // modifiers
	VOID SetLatency(UINT32 rPenalty, UINT32 wPenalty ) 
	{ 
		for (UINT32 i = 0; i < NumSets(); i++)
     		_sets[i].SetPenalty(rPenalty, wPenalty);
	}
  VOID SetNextLevel(CACHE_BASE *pCache) { m_pNextCache = pCache; };
    /// Cache access from addr to addr+size-1
    bool Access(ADDRINT addr, UINT32 size, ACCESS_BASE::ACCESS_TYPE accessType, int nArea);
    /// Cache access at addr that does not span cache lines
    unsigned int AccessSingleLine(ADDRINT addr, ACCESS_BASE::ACCESS_TYPE accessType, int nArea);

	VOID Fini();
    string StatsLong(string prefix, CACHE_TYPE cache_type) const;
  void Dump(ofstream &os);
};

/*!
 *  @return true if all accessed cache lines hit
 */

template <class SET, UINT32 MAX_SETS, UINT32 STORE_ALLOCATION>
bool CACHE1<SET,MAX_SETS,STORE_ALLOCATION>::Access(ADDRINT addr, UINT32 size, ACCESS_BASE::ACCESS_TYPE accessType, int nArea)
{
    const ADDRINT highAddr = addr + size;

    const ADDRINT lineSize = LineSize();
    const ADDRINT notLineMask = ~(lineSize - 1);
    do
    {
       AccessSingleLine(addr, accessType, nArea);
        addr = (addr & notLineMask) + lineSize; // start of next cache line
    }
    while (addr < highAddr);

    return true;
}

/*!
 *  @return true if accessed cache line hits
 */
template <class SET, UINT32 MAX_SETS, UINT32 STORE_ALLOCATION>
unsigned int CACHE1<SET,MAX_SETS,STORE_ALLOCATION>::AccessSingleLine(ADDRINT addr, ACCESS_BASE::ACCESS_TYPE accessType, int nArea)
{
    CACHE_TAG tag;
    UINT32 setIndex;

    SplitAddress(addr, tag, setIndex);

    SET & set = _sets[setIndex];

  UINT32 lineIndex = 0;
  int nWriteBack = 0;
  bool hit = set.Find(tag, lineIndex, nWriteBack);
  unsigned int nHit = 0;
    
    //assert(STORE_ALLOCATION == CACHE_ALLOC::STORE_NO_ALLOCATE);
    // on miss, loads always allocate, stores optionally
	if( hit )
	{
	   set.HitLRU( lineIndex, accessType == ACCESS_BASE::ACCESS_TYPE_LOAD? false: true);	   
	 /*     if( accessType == ACCESS_TYPE_STORE)
		 {
			 // write hit, using write-through mode
			 if( m_pNextCache)
				 m_pNextCache->AccessSingleLine(addr, accessType, bUser, bData);
		 }*/
	}
	// miss, in write-allocation mode and read-allocation (ie. no read through), just load the block from the next level cache, and then access the
	else if (  (accessType == ACCESS_BASE::ACCESS_TYPE_LOAD || STORE_ALLOCATION == CACHE_ALLOC::STORE_ALLOCATE))
	{
		// if needing write-back
		CACHE_TAG wbTag = set.NeedWriteback();			
		if( wbTag )
		{
			++ _writeback;
			if(m_pNextCache )
			{
				ADDRINT nAddr = wbTag;
				nAddr <<= _lineShift;			
				(void)m_pNextCache->AccessSingleLine(nAddr, ACCESS_BASE::ACCESS_TYPE_STORE, nArea);    // Assume: cache line l2 size is multiple of l1            ++ _writeback;
			}
		}
		// for replace		
		if( m_pNextCache)
			nHit = m_pNextCache->AccessSingleLine(addr, ACCESS_BASE::ACCESS_TYPE_LOAD, nArea);      // read from the next level to the current
		else
			g_CurrentCycle += g_memoryLatency;
		nWriteBack += set.Replace(tag, accessType == ACCESS_BASE::ACCESS_TYPE_LOAD?  false: true, nArea);
		 
	}
	// write miss, in non-write-allocation, write into lower memory directly
	else
	{
		if( m_pNextCache)
			nHit = m_pNextCache->AccessSingleLine(addr, accessType, nArea);
	}
	_writeback += nWriteBack;
    _access[accessType][hit]++;

    // ---by qali: for debugging
   // cerr << "access " << hex << addr << ":\tin cache[" << setIndex << "][" << set.GetSpot() << "]--" << (hit?"hit":"miss") << endl;     

	unsigned int status = nHit;  // record the cache hit/miss information for the lower cache levels
	status <<= 1;
	if( hit )
		status |= 1;
	
    return status;
}


template <class SET, UINT32 MAX_SETS, UINT32 STORE_ALLOCATION>
void CACHE1<SET,MAX_SETS,STORE_ALLOCATION>::Fini()
{
	for(unsigned int i=0; i < NumSets(); ++ i)
		_writeback += _sets[i].Fini();
}

template <class SET, UINT32 MAX_SETS, UINT32 STORE_ALLOCATION>
string CACHE1<SET,MAX_SETS,STORE_ALLOCATION>::StatsLong(string prefix, CACHE_TYPE cache_type) const
{
    const UINT32 headerWidth = 19;
    const UINT32 numberWidth = 12;

    string out;

	out += "==============================================\n";
    out += prefix + _name;
	out += "("+ mydecstr(CacheSize(), 7) + "," + mydecstr(LineSize(), 2) + "," + mydecstr(Associativity(), 2) + ")\n";

    if (cache_type == CACHE_TYPE_ICACHE)	
		out += prefix + "Instruction cache:\n";
	else if( cache_type == CACHE_TYPE_DCACHE)
		out += prefix + "Data cache:\n";
	else if( cache_type == CACHE_TYPE_L2)
		out +=prefix + "L2:\n";
		
   for (UINT32 i = 0; i < ACCESS_BASE::ACCESS_TYPE_NUM; i++)
   {
	   const ACCESS_BASE::ACCESS_TYPE accessType = (ACCESS_BASE::ACCESS_TYPE)i;

	   std::string type(accessType == ACCESS_BASE::ACCESS_TYPE_LOAD? "Load" : "Store");

	   out += prefix + ljstr(type + "-Hits\t      ", headerWidth)
			  + mydecstr(Hits(accessType), numberWidth)  +
			  "\t  " + fltstr(100.0 * Hits(accessType) / Accesses(accessType), 2, 6) + "%\n";

	   out += prefix + ljstr(type + "-Misses\t    ", headerWidth)
			  + mydecstr(Misses(accessType), numberWidth) +
			  "\t  " +fltstr(100.0 * Misses(accessType) / Accesses(accessType), 2, 6) + "%\n";

	   out += prefix + ljstr(type + "-Accesses\t  ", headerWidth)
			  + mydecstr(Accesses(accessType), numberWidth) +
			  "\t  " +fltstr(100.0 * Accesses(accessType) / Accesses(accessType), 2, 6) + "%\n";

	   out += prefix + "\n";
   }


    out += prefix + ljstr("Total-Hits\t      ", headerWidth)
           + mydecstr(Hits(), numberWidth) +
           "\t  " +fltstr(100.0 * Hits() / Accesses(), 2, 6) + "%\n";

    out += prefix + ljstr("Total-Misses\t    ", headerWidth)
           + mydecstr(Misses(), numberWidth) +
           "\t  " +fltstr(100.0 * Misses() / Accesses(), 2, 6) + "%\n";

    out += prefix + ljstr("Total-Accesses\t  ", headerWidth)
           + mydecstr(Accesses(), numberWidth) +
           "\t  " +fltstr(100.0 * Accesses() / Accesses(), 2, 6) + "%\n";
    out += "\n";

    out += prefix + ljstr("Total-Writeback\t      ", headerWidth)
           + mydecstr(_writeback, numberWidth) +
           "\t  " +fltstr(100.0 * _writeback / Accesses(), 2, 6) + "%\n";
    return out;
}

template <class SET, UINT32 MAX_SETS, UINT32 STORE_ALLOCATION>
void CACHE1<SET,MAX_SETS,STORE_ALLOCATION>::Dump(ofstream &os)
{
   os << endl << endl << StatsLong("##", CACHE_BASE::CACHE_TYPE_DCACHE);
}



// define shortcuts
#endif // PIN_CACHE1_H

