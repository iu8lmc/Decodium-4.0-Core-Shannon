// ------------------------------------------------------------------------------
// qpc_fwht.h
// Fast Walsh-Hadamard Transforms for q-ary polar codes
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

#ifndef _qpc_fwht_h_
#define _qpc_fwht_h_

#include "np_qpc.h"

#if QPC_LOG2Q==7
#define qpc_fwht(a,b) qpc_fwht128(a,b)  
#endif
#if QPC_LOG2Q==6
#define qpc_fwht(a,b) qpc_fwht64(a,b)  
#endif
#if QPC_LOG2Q==5
#define qpc_fwht(a,b) qpc_fwht32(a,b)  
#endif
#if QPC_LOG2Q==4
#define qpc_fwht(a,b) qpc_fwht16(a,b)  
#endif
// Note that it makes no sense to use fast convolutions
// for transforms that are less than 16 symbols in size.
// For such cases direct convolutions are faster.
#if QPC_LOG2Q==3
#define qpc_fwht(a,b) qpc_fwht8(a,b)  
#endif

#ifdef __cplusplus
extern "C" {
#endif 
    float* qpc_fwht8(float* y, float* x);
    float* qpc_fwht16(float* y, float* x);
    float* qpc_fwht32(float* y, float* x);
    float* qpc_fwht64(float* y, float* x);
    float* qpc_fwht128(float* y, float* x);
#ifdef __cplusplus
}
#endif 

#endif // _qpc_fwht_h_
