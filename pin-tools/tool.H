#ifndef TOOL_H
#define TOOL_H

#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string>
#include <sstream>

using namespace std;

// if not use the pin library environment
#define PIN_LIB

#ifndef PIN_LIB

typedef uint8_t  UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef uint32_t UINT;
typedef uint64_t UINT64;

typedef int8_t  INT8;
typedef int16_t INT16;
typedef int32_t INT32;
typedef int64_t INT64;
typedef uint64_t ADDRINT;


static inline string ljstr(const string& s, UINT32 width, char padding = ' ')
{
    string ostr(width, padding);
    ostr.replace(0,s.length(),s);
    return ostr;
}

static inline string fltstr(double val, UINT32 prec =0, UINT32 width = 0)
{
  char buf[512];
  
  sprintf(buf, "%*.*f", prec, width, val);
  string szValue(buf);
  return szValue;
}
#endif



/*!
 *  @brief Checks if n is a power of 2.
 *  @returns true if n is power of 2
 */
static inline bool IsPower2(UINT32 n)
{
    return ((n & (n - 1)) == 0);
}

/*!
 *  @brief Computes floor(log2(n))
 *  Works by finding position of MSB set.
 *  @returns -1 if n == 0.
 */
static inline INT32 FloorLog2(UINT32 n)
{
    INT32 p = 0;

    if (n == 0) return -1;

    if (n & 0xffff0000) { p += 16; n >>= 16; }
    if (n & 0x0000ff00)  { p +=  8; n >>=  8; }
    if (n & 0x000000f0) { p +=  4; n >>=  4; }
    if (n & 0x0000000c) { p +=  2; n >>=  2; }
    if (n & 0x00000002) { p +=  1; }

    return p;
}

/*!
 *  @brief Computes floor(log2(n))
 *  Works by finding position of MSB set.
 *  @returns -1 if n == 0.
 */
static inline INT32 CeilLog2(UINT32 n)
{
    return FloorLog2(n - 1) + 1;
}
#endif


#define KILO 1024

typedef UINT64 CACHE_STATS; // type of cache hit/miss counters

namespace ACCESS_BASE
{
	typedef enum
    {
        ACCESS_TYPE_LOAD,
        ACCESS_TYPE_STORE,
        ACCESS_TYPE_NUM
    } ACCESS_TYPE;
	
	inline ADDRINT Addr2Tag(ADDRINT nAddr, ADDRINT nCacheLine) { return nAddr/nCacheLine; }
}
static inline string mydecstr(UINT64 v, UINT32 w)
{
    ostringstream o;
    o.width(w);
    o << v;
    string str(o.str());
    return str;
}


