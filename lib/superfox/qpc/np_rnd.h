// ------------------------------------------------------------------------------
// np_rnd.h
// Functions to generate random numbers with uniform/gaussian probability distributions
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

#ifndef _np_rnd_h_
#define _np_rnd_h_

#define _CRT_RAND_S
#include <stdlib.h>

#define _USE_MATH_DEFINES 
#include <math.h>
#define M_2PI (2.0f*(float)M_PI)

#ifdef __cplusplus
extern "C" {
#endif

// generate a random array of real numbers with a gaussian distribution of given mean and stdev
void np_normrnd_real(float *dst, int nitems, float mean, float stdev);

// generate a random array of nitems complex numbers with a gaussian distribution of given mean and stdev
void np_normrnd_cpx(float* dst, int nitems, float mean, float stdev);

// generate a random array of nitems unsigned int numbers with uniform distribution
// in the range [0 .. nsetsize) 
void np_unidrnd(unsigned int* dst, int nitems, unsigned int nsetsize);

// generate a random array of nitems unsigned chars numbers with uniform distribution
// in the range [0 .. nsetsize) 
void np_unidrnd_uc(unsigned char* dst, int nitems, unsigned char nsetsize);

// generate a random array of nitems float numbers with uniform distribution
// in the range [0 .. fmax) 
void np_unifrnd(float* dst, int nitems, float fmax);


#ifdef __cplusplus
}
#endif

#endif // _np_rnd_h_

