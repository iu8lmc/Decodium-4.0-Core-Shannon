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

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>

#include "dbgprintf.h"
#include "qpc_fwht.h"
#include "np_qpc.h"

// static constant / functions

static const float knorm = 1.0f / QPC_Q;

static float* pdf_conv(float *dst, float* pdf1, float* pdf2)
{
    // convolution between two pdf

    float fwht_pdf1[QPC_Q];
    float fwht_pdf2[QPC_Q];
    int k;

    qpc_fwht(fwht_pdf1, pdf1);
    qpc_fwht(fwht_pdf2, pdf2);

    for (k = 0; k < QPC_Q; k++)
        fwht_pdf1[k] *= fwht_pdf2[k];

    qpc_fwht(dst, fwht_pdf1);

    for (k = 0; k < QPC_Q; k++)
        dst[k] *= knorm;

    return dst;
}
static void   pdfarray_conv(float* dstarray, float* pdf1array, float* pdf2array, int numrows)
{
    int k;

    // convolutions between rows of pdfs

    for (k = 0; k < numrows; k++) {
        pdf_conv(dstarray, pdf1array, pdf2array);
        dstarray  += QPC_Q;
        pdf1array += QPC_Q;
        pdf2array += QPC_Q;
    }

}

static float _pdfuniform[QPC_Q];
static const float* _pdf_uniform1();
static const float* _pdf_uniform0();
typedef const float*(*ptr_pdfuniform)(void);

static ptr_pdfuniform _ptr_pdf_uniform = _pdf_uniform0;

static const float* _pdf_uniform1() 
{
    return _pdfuniform;
};
static const float* _pdf_uniform0() 
{
    // compute uniform pdf once for all
    int k;
    for (k = 0; k < QPC_Q; k++)
        _pdfuniform[k] = knorm;

    // next call to _qpc_pdfuniform
    // will be handled directly by _pqc_pdfuniform1
    _ptr_pdf_uniform = _pdf_uniform1;

    return _pdfuniform;
};
static const float*  pdf_uniform()
{
    return _ptr_pdf_uniform();
}

static float * pdf_mul(float *dst, float* pdf1, float* pdf2)
{
    int k;
    float v;
    float norm = 0;
    for (k = 0; k < QPC_Q; k++) {
        v = pdf1[k] * pdf2[k];
        dst[k] = v;
        norm += v;
    }
    // if norm of the result is not positive
    // return in dst a uniform distribution
    if (norm <= 0)
        memcpy(dst, pdf_uniform(), QPC_Q * sizeof(float));
    else {
        norm = 1.0f / norm;
        for (k = 0; k < QPC_Q; k++)
            dst[k] = dst[k] * norm;
    }


    return dst;
}
static void pdfarray_mul(float* dstarray, float* pdf1array, float* pdf2array, int numrows)
{
    int k;

    // products between rows of pdfs

    for (k = 0; k < numrows; k++) {
        pdf_mul(dstarray, pdf1array, pdf2array);
        dstarray += QPC_Q;
        pdf1array += QPC_Q;
        pdf2array += QPC_Q;
    }
}

static float* pdf_convhard(float* dst, const float* pdf, unsigned char hd)
{
    // convolution between a pdf and a hard-decision feedback

    int k;
    for (k=0;k<QPC_Q;k++) 
        dst[k] = pdf[k^hd];

    return dst;
}
static void pdfarray_convhard(float* dstarray, const float* pdfarray, const unsigned char *hdarray, int numrows)
{
    int k;

    // hard convolutions between rows

    for (k = 0; k < numrows; k++) {
        pdf_convhard(dstarray, pdfarray, hdarray[k]);
        dstarray += QPC_Q;
        pdfarray += QPC_Q;
    }
}

static unsigned char pdf_max(const float* pdf)
{
    int k;

    unsigned char imax = 0;
    float pdfmax = pdf[0];

    for (k=1;k<QPC_Q;k++)
        if (pdf[k] > pdfmax) {
            pdfmax = pdf[k];
            imax = k;
        }

    return imax;
}

// local stack functions ---------------------------------------
static float _qpc_stack[QPC_N * QPC_Q * 2];
static float* _qpc_stack_base = _qpc_stack;

static float* _qpc_stack_push(int numfloats)
{
    float* addr = _qpc_stack_base;
    _qpc_stack_base += numfloats;
    return addr;
}
static void _qpc_stack_pop(int numfloats)
{
    _qpc_stack_base -= numfloats;
}

// qpc encoder function (internal use) ----------------------------------------------------------
unsigned char* _qpc_encode(unsigned char* y, unsigned char* x)
{
    // Non recursive polar encoder
    // Same architecture of a fast fourier transform
    // in which the fft butteflies are replaced by the polar transform
    // butterflies

    int k, j, m;
    int groups;
    int bfypergroup;
    int stepbfy;
    int stepgroup;
    int basegroup;
    int basebfy;

    memcpy(y, x, QPC_N);

    for (k = 0; k < QPC_LOG2N; k++) {
        groups = 1 << (QPC_LOG2N - 1 - k);
        stepbfy = bfypergroup = 1 << k;
        stepgroup = stepbfy << 1;
        basegroup = 0;
        for (j = 0; j < groups; j++) {
            basebfy = basegroup;
            for (m = 0; m < bfypergroup; m++) {
                // polar transform
                y[basebfy + stepbfy] = y[basebfy + stepbfy] ^ y[basebfy];
                basebfy = basebfy + 1;
            }
            basegroup = basegroup + stepgroup;
        }
    }

    return y;
}

// qpc polar decoder (internal use )--------------------------------------------------
void _qpc_decode(unsigned char* xdec, unsigned char* ydec, const float* py, const unsigned char* f, const unsigned char* fsize, const int numrows)
{

    if (numrows == 1) {
        if (fsize[0] == 0) {
            // dbgprintf_vector_float("py", py, QPC_Q);

            // frozen symbol
            xdec[0] = pdf_max(py);
            ydec[0] = f[0];
        }
        else {
            // fsize = 1 => information symbol
            xdec[0] = pdf_max(py);
            ydec[0] = xdec[0];
        }

    }
    else {

        int k;
        int nextrows = numrows >> 1;
        int size = nextrows << QPC_LOG2Q;

        // upper block variables
        unsigned char* xdech = xdec + nextrows;
        unsigned char* ydech = ydec + nextrows;
        const unsigned char* fh = f + nextrows;
        const unsigned char* fsizeh = fsize + nextrows;

        // Step 1.
        // stack and init variables used in the recursion

        float* pyl = _qpc_stack_push(size);
        memcpy(pyl, py, size * sizeof(float));

        float* pyh = _qpc_stack_push(size);
        memcpy(pyh, py + size, size * sizeof(float));

        // Step 2. Recursion on upper block
        // Forward pdf convolutions for the upper block
        // (place in the lower part of py the convolution of lower and higher blocks)

//        float* pyh = py + size;
//        pdfarray_conv(py, pyl, pyh, nextrows); // convolution overwriting the lower block of py which is not needed  

        pdfarray_conv(pyh, pyl, pyh, nextrows); 
        _qpc_decode(xdech, ydech, pyh, fh, fsizeh, nextrows);
 
        // Step 3. compute pdfs in the lower block
        pdfarray_convhard(pyh, py+size, ydech,nextrows); // dst ptr must be different form src ptr
        pdfarray_mul(pyl, pyl, pyh, nextrows);
        // we don't need pyh anymore
        _qpc_stack_pop(size);

        // Step 4. Recursion on the lower block
        _qpc_decode(xdec, ydec, pyl, f, fsize, nextrows);
        // we don't need pyl anymore
        _qpc_stack_pop(size);

        // Step 5. Update backward results
        // xdec is already ok, we need just to update ydech
        for (k = 0; k < nextrows; k++)
            ydech[k] = ydech[k] ^ ydec[k];

    }


}

// Public encoding/decoding functions ------------------------------------------------
void qpc_encode(unsigned char* y, const unsigned char* x)
{
  
    // map information symbols
    int kk, pos;
    for (kk = 0; kk < QPC_K; kk++) {
        pos = qpccode.xpos[kk];
        qpccode.f[pos] = x[kk];
    }
    // do polar encoding
    _qpc_encode(y, qpccode.f);
}
void qpc_decode(unsigned char* xdec, unsigned char* ydec, float* py)
{
    int k;
    unsigned char x[QPC_N];

    // set the first py row with know frozen (punctured) symbol
    if (qpccode.np < qpccode.n) {
        // assume that we punctured only the first output symbol
        memset(py, 0, QPC_Q * sizeof(float));
        py[qpccode.f[0]] = 1.0f;
    }

    // decode
    _qpc_decode(x, ydec, py, qpccode.f, qpccode.fsize, QPC_N);

    // demap information symbols
    for (k = 0; k < QPC_K; k++)
        xdec[k] = x[qpccode.xpos[k]];

}
