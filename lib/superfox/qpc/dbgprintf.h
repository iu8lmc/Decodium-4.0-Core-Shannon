// ------------------------------------------------------------------------------
// dbgprintf.h
// Functions for printing debug information
// 
// (c) 2024 - Nico Palermo, IV3NWV - Microtelecom Srl, Italy
// ------------------------------------------------------------------------------
//
//    This source is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//    This file is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this source distribution.  
//    If not, see <http://www.gnu.org/licenses/>.
// ------------------------------------------------------------------------------

#ifndef _dbgprintf_h_
#define _dbgprintf_h_

// Define DBGPRINTF_ANYWAY
// in order to print debug information also 
// when _DEBUG is not defined

#define DBGPRINTF_ANYWAY

#ifdef _DEBUG
#define DBG_PRINTF
#else
#ifdef DBGPRINTF_ANYWAY
#define DBG_PRINTF
#endif 
#endif

#include <stdio.h>

// print functions for debug purposes
#ifdef DBG_PRINTF

#ifdef __cplusplus 
extern "C" {
#endif

	void dbgprintf_vector_uchar(const char* name, const unsigned char* v, int vsize);
	void dbgprintf_vector_float(const char* name, const float* v, int vsize);
	void dbgprintf_rows_float(const char* name, const float* v, int vsize, int nrows);

#ifdef __cplusplus 
}
#endif

#else

#define dbgprintf_vector_uchar(a,b)
#define dbgprintf_vector_float(a,b)
#define dbgprintf_rows_float(a, b, c, d)

#endif

#endif // _dbgprintf_h_
