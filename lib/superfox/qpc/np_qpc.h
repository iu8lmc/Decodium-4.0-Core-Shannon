// ------------------------------------------------------------------------------
// np_qpc.h 
// Q-Ary Polar Codes encoding/decoding functions
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



#ifndef _np_qpc_h_
#define _np_qpc_h_

#define QPC_LOG2N 7             // log2(codeword length) (not punctured)
#define QPC_N (1<<QPC_LOG2N)    // codeword length (not punctured)
#define QPC_LOG2Q 7             // bits per symbol
#define QPC_Q (1<<QPC_LOG2Q)    // alphabet size
#define QPC_K 50               // number of information symbols


typedef struct {
    int n;                      // codeword length (unpunctured)
    int np;                     // codeword length (punctured)
    int k;                      // number of information symbols 
    int q;                      // alphabet size
    int xpos[QPC_N];            // info symbols mapping/demapping tables
    unsigned char f[QPC_N];     // frozen symbols values
    unsigned char fsize[QPC_N]; // frozen symbol flag (fsize==0 => frozen)
} qpccode_ds;

#ifdef __cplusplus
extern "C"
{
#endif

    void qpc_encode(unsigned char* y, const unsigned char* x);
    void qpc_decode(unsigned char* xdec, unsigned char* ydec, float* py);

    unsigned char* _qpc_encode(unsigned char* y, unsigned char* x);
    void           _qpc_decode(unsigned char* xdec, unsigned char* ydec,
                        const float* py, const unsigned char* f, const unsigned char* fsize,
                        const int numrows);

    extern qpccode_ds qpccode;

#ifdef __cplusplus
}
#endif 

#endif // _np_qpc_h_
