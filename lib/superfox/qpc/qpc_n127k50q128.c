// ------------------------------------------------------------------------------
// qpc_n127k50q128.c
//
// Defines the parameters of the q-ary polar code (127,50) Q=128
// for WSJT-X
// ------------------------------------------------------------------------------
// (c) 2024 - Nico Palermo, IV3NWV - Microtelecom Srl, Italy
// ------------------------------------------------------------------------------
//
//    WARNING:
//    This source is NOT free software and it is licensed only for use with WSJT-X.
//    No derived work is authorized to use this code without the written
//    permission of his author (Nico Palermo / IV3NWV) who owns all the rights
//    of this IP (intellectual property).
//
//    This file is distributed ONLY for the purpose of documenting and making of 
//    public domain the encoding scheme used in WSJT-X so that the transmitted 
//    messages can be decoded by anybody.
//    Anyway this does not imply that one could use the following tables in a 
//    derived work without an explicit and written authorization from his author.
//	  Any unauthorized use, as for any intellectual property, is simply 
//    illegal.
// -------------------------------------------------------------------------------
#include "np_qpc.h"
qpccode_ds qpccode = {
  128,        //n
  127,        //np
  50,         //k
  128,        //q
{
  1,   2,   3,   4,   5,   6,   8,   9,  10,  12,  16,  32,  17,  18,  64,  20, 
 33,  34,  24,   7,  11,  36,  13,  19,  14,  65,  40,  21,  66,  22,  35,  68, 
 25,  48,  37,  26,  72,  15,  38,  28,  41,  67,  23,  80,  42,  69,  49,  96, 
 44,  27,  70,  50,  73,  39,  29,  52,  74,  30,  56,  81,  76,  43,  82,  84, 
 97,  45,  71,  88,  98,  46, 100,  51, 104,  53,  75, 112,  54,  57,  99, 119, 
 92,  77,  58, 117,  59,  83, 106,  31,  85, 108, 115, 116, 122, 125, 124,  91, 
 61,  90,  89, 111,  78,  93,  94, 126,  86, 107, 110, 118, 121,  62, 120,  87, 
105,  55, 114,  60, 127,  63, 103, 101, 123,  95, 102,  47, 109,  79, 113,   0
},
{
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
},
{
  0,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1, 
  1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   0,   0,   0, 
  1,   1,   1,   1,   1,   1,   1,   0,   1,   1,   1,   0,   1,   0,   0,   0, 
  1,   1,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  1,   1,   1,   1,   1,   1,   0,   0,   1,   0,   0,   0,   0,   0,   0,   0, 
  1,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  1,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
}
};
