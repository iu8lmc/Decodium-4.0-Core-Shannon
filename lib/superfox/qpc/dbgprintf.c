// ------------------------------------------------------------------------------
// dbgprintf.c
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

#include "dbgprintf.h"

// print functions for debug purposes
void dbgprintf_vector_uchar(const char* name, const unsigned char* v, int vsize)
{
    int k;

    printf("%s=", name);
    for (k = 0; k < vsize; k++) {
        if ((k & 0x0F) == 0)
            printf("\n");
        printf("%02X ", v[k]);
    }
    printf("\n");
}

void dbgprintf_vector_float(const char* name, const float* v, int vsize)
{
    int k;

    printf("%s=", name);
    for (k = 0; k < vsize; k++) {
        if ((k & 0x07) == 0)
            printf("\n");
        printf("%10.3f ", v[k]);
    }
    printf("\n");
}

void dbgprintf_rows_float(const char* name, const float* v, int vsize, int nrows)
{
    int k;
    int j;

    printf("%s=\n", name);
    for (j = 0; j < nrows; j++) {
        printf("r%d:", j);
        for (k = 0; k < vsize; k++) {
            if ((k & 0x07) == 0)
                printf("\n");
            printf("%7.3f ", v[k]);
        }
        printf("\n");
        v += vsize;
    }
}
