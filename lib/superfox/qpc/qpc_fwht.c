// ------------------------------------------------------------------------------
// qpc_fwht.c
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

#include "qpc_fwht.h"

static void   _qpc_sumdiff8_16(float* y, float* t)
{
    y[0] = t[0] + t[16];
    y[16] = t[0] - t[16];

    y[1] = t[1] + t[17];
    y[17] = t[1] - t[17];

    y[2] = t[2] + t[18];
    y[18] = t[2] - t[18];

    y[3] = t[3] + t[19];
    y[19] = t[3] - t[19];

    y[4] = t[4] + t[20];
    y[20] = t[4] - t[20];

    y[5] = t[5] + t[21];
    y[21] = t[5] - t[21];

    y[6] = t[6] + t[22];
    y[22] = t[6] - t[22];

    y[7] = t[7] + t[23];
    y[23] = t[7] - t[23];

}
static void   _qpc_sumdiff8_32(float* y, float* t)
{
    y[0] = t[0] + t[32];
    y[32] = t[0] - t[32];

    y[1] = t[1] + t[33];
    y[33] = t[1] - t[33];

    y[2] = t[2] + t[34];
    y[34] = t[2] - t[34];

    y[3] = t[3] + t[35];
    y[35] = t[3] - t[35];

    y[4] = t[4] + t[36];
    y[36] = t[4] - t[36];

    y[5] = t[5] + t[37];
    y[37] = t[5] - t[37];

    y[6] = t[6] + t[38];
    y[38] = t[6] - t[38];

    y[7] = t[7] + t[39];
    y[39] = t[7] - t[39];

}
static void   _qpc_sumdiff8_64(float* y, float* t)
{
    y[0] = t[0] + t[64];
    y[64] = t[0] - t[64];

    y[1] = t[1] + t[65];
    y[65] = t[1] - t[65];

    y[2] = t[2] + t[66];
    y[66] = t[2] - t[66];

    y[3] = t[3] + t[67];
    y[67] = t[3] - t[67];

    y[4] = t[4] + t[68];
    y[68] = t[4] - t[68];

    y[5] = t[5] + t[69];
    y[69] = t[5] - t[69];

    y[6] = t[6] + t[70];
    y[70] = t[6] - t[70];

    y[7] = t[7] + t[71];
    y[71] = t[7] - t[71];

}

float* qpc_fwht8(float* y, float* x)
{
    float t[8];

    // first stage
    y[0] = x[0] + x[1];
    y[1] = x[0] - x[1];

    y[2] = x[2] + x[3];
    y[3] = x[2] - x[3];

    y[4] = x[4] + x[5];
    y[5] = x[4] - x[5];

    y[6] = x[6] + x[7];
    y[7] = x[6] - x[7];

    // second stage
    t[0] = y[0] + y[2];
    t[2] = y[0] - y[2];

    t[1] = y[1] + y[3];
    t[3] = y[1] - y[3];

    t[4] = y[4] + y[6];
    t[6] = y[4] - y[6];

    t[5] = y[5] + y[7];
    t[7] = y[5] - y[7];

    // third stage
    y[0] = t[0] + t[4];
    y[4] = t[0] - t[4];

    y[1] = t[1] + t[5];
    y[5] = t[1] - t[5];

    y[2] = t[2] + t[6];
    y[6] = t[2] - t[6];

    y[3] = t[3] + t[7];
    y[7] = t[3] - t[7];

    return y;
}
float* qpc_fwht16(float* y, float* x)
{
    float t[16];

    qpc_fwht8(t, x);
    qpc_fwht8(t + 8, x + 8);

    y[0] = t[0] + t[8];
    y[8] = t[0] - t[8];

    y[1] = t[1] + t[9];
    y[9] = t[1] - t[9];

    y[2] = t[2] + t[10];
    y[10] = t[2] - t[10];

    y[3] = t[3] + t[11];
    y[11] = t[3] - t[11];

    y[4] = t[4] + t[12];
    y[12] = t[4] - t[12];

    y[5] = t[5] + t[13];
    y[13] = t[5] - t[13];

    y[6] = t[6] + t[14];
    y[14] = t[6] - t[14];

    y[7] = t[7] + t[15];
    y[15] = t[7] - t[15];

    return y;

}
float* qpc_fwht32(float* y, float* x)
{
    float t[32];

    qpc_fwht16(t, x);
    qpc_fwht16(t + 16, x + 16);

    _qpc_sumdiff8_16(y, t);
    _qpc_sumdiff8_16(y + 8, t + 8);

    return y;
}
float* qpc_fwht64(float* y, float* x)
{
    float t[64];

    qpc_fwht32(t, x);
    qpc_fwht32(t + 32, x + 32);

    _qpc_sumdiff8_32(y, t);
    _qpc_sumdiff8_32(y + 8, t + 8);
    _qpc_sumdiff8_32(y + 16, t + 16);
    _qpc_sumdiff8_32(y + 24, t + 24);

    return y;
}
float* qpc_fwht128(float* y, float* x)
{
    float t[128];

    qpc_fwht64(t, x);
    qpc_fwht64(t + 64, x + 64);

    _qpc_sumdiff8_64(y, t);
    _qpc_sumdiff8_64(y + 8, t + 8);
    _qpc_sumdiff8_64(y + 16, t + 16);
    _qpc_sumdiff8_64(y + 24, t + 24);
    _qpc_sumdiff8_64(y + 32, t + 32);
    _qpc_sumdiff8_64(y + 40, t + 40);
    _qpc_sumdiff8_64(y + 48, t + 48);
    _qpc_sumdiff8_64(y + 56, t + 56);

    return y;
}

// functions over pdfs used by the decoder -----------------------------------------------------------




