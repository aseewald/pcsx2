/*  ZeroGS KOSMOS
 *  Copyright (C) 2005-2006 zerofrog@gmail.com
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "GS.h"
#include "Util.h"
#include "ZZoglMem.h"
//#include "zerogs.h"
#include "targets.h"
#include "x86.h"

#ifndef ZZNORMAL_MEMORY
// works only when base is a power of 2

//#define ROUND_UPPOW2(val, base)	(((val)+(base-1))&~(base-1))
//#define ROUND_DOWNPOW2(val, base)	((val)&~(base-1))
//#define MOD_POW2(val, base) ((val)&(base-1))

u32 g_blockTable32[4][8] = {
    {  0,  1,  4,  5, 16, 17, 20, 21},
    {  2,  3,  6,  7, 18, 19, 22, 23},
    {  8,  9, 12, 13, 24, 25, 28, 29},
    { 10, 11, 14, 15, 26, 27, 30, 31}
};

u32 g_blockTable32Z[4][8] = {
    { 24, 25, 28, 29,  8,  9, 12, 13},
    { 26, 27, 30, 31, 10, 11, 14, 15},
    { 16, 17, 20, 21,  0,  1,  4,  5},
    { 18, 19, 22, 23,  2,  3,  6,  7}
};

u32 g_blockTable16[8][4] = {
    {  0,  2,  8, 10 },
    {  1,  3,  9, 11 },
    {  4,  6, 12, 14 },
    {  5,  7, 13, 15 },
    { 16, 18, 24, 26 },
    { 17, 19, 25, 27 },
    { 20, 22, 28, 30 },
    { 21, 23, 29, 31 }
};

u32 g_blockTable16S[8][4] = {
    {  0,  2, 16, 18 },
    {  1,  3, 17, 19 },
    {  8, 10, 24, 26 },
    {  9, 11, 25, 27 },
    {  4,  6, 20, 22 },
    {  5,  7, 21, 23 },
    { 12, 14, 28, 30 },
    { 13, 15, 29, 31 }
};

u32 g_blockTable16Z[8][4] = {
    { 24, 26, 16, 18 },
    { 25, 27, 17, 19 },
    { 28, 30, 20, 22 },
    { 29, 31, 21, 23 },
    {  8, 10,  0,  2 },
    {  9, 11,  1,  3 },
    { 12, 14,  4,  6 },
    { 13, 15,  5,  7 }
};

u32 g_blockTable16SZ[8][4] = {
    { 24, 26,  8, 10 },
    { 25, 27,  9, 11 },
    { 16, 18,  0,  2 },
    { 17, 19,  1,  3 },
    { 28, 30, 12, 14 },
    { 29, 31, 13, 15 },
    { 20, 22,  4,  6 },
    { 21, 23,  5,  7 }
};

u32 g_blockTable8[4][8] = {
    {  0,  1,  4,  5, 16, 17, 20, 21},
    {  2,  3,  6,  7, 18, 19, 22, 23},
    {  8,  9, 12, 13, 24, 25, 28, 29},
    { 10, 11, 14, 15, 26, 27, 30, 31}
};

u32 g_blockTable4[8][4] = {
    {  0,  2,  8, 10 },
    {  1,  3,  9, 11 },
    {  4,  6, 12, 14 },
    {  5,  7, 13, 15 },
    { 16, 18, 24, 26 },
    { 17, 19, 25, 27 },
    { 20, 22, 28, 30 },
    { 21, 23, 29, 31 }
};

u32 g_columnTable32[8][8] = {
    {  0,  1,  4,  5,  8,  9, 12, 13 },
    {  2,  3,  6,  7, 10, 11, 14, 15 },
    { 16, 17, 20, 21, 24, 25, 28, 29 },
    { 18, 19, 22, 23, 26, 27, 30, 31 },
    { 32, 33, 36, 37, 40, 41, 44, 45 },
    { 34, 35, 38, 39, 42, 43, 46, 47 },
    { 48, 49, 52, 53, 56, 57, 60, 61 },
    { 50, 51, 54, 55, 58, 59, 62, 63 },
};

u32 g_columnTable16[8][16] = {
    {   0,   2,   8,  10,  16,  18,  24,  26, 
        1,   3,   9,  11,  17,  19,  25,  27 },
    {   4,   6,  12,  14,  20,  22,  28,  30, 
        5,   7,  13,  15,  21,  23,  29,  31 },
    {  32,  34,  40,  42,  48,  50,  56,  58,
       33,  35,  41,  43,  49,  51,  57,  59 },
    {  36,  38,  44,  46,  52,  54,  60,  62,
       37,  39,  45,  47,  53,  55,  61,  63 },
    {  64,  66,  72,  74,  80,  82,  88,  90,
       65,  67,  73,  75,  81,  83,  89,  91 },
    {  68,  70,  76,  78,  84,  86,  92,  94,
       69,  71,  77,  79,  85,  87,  93,  95 },
    {  96,  98, 104, 106, 112, 114, 120, 122,
       97,  99, 105, 107, 113, 115, 121, 123 },
    { 100, 102, 108, 110, 116, 118, 124, 126,
      101, 103, 109, 111, 117, 119, 125, 127 },
};

u32 g_columnTable8[16][16] = {
	{   0,   4,  16,  20,  32,  36,  48,  52,   // column 0
        2,   6,  18,  22,  34,  38,  50,  54 },
    {   8,  12,  24,  28,  40,  44,  56,  60,
       10,  14,  26,  30,  42,  46,  58,  62 },
    {  33,  37,  49,  53,   1,   5,  17,  21,
       35,  39,  51,  55,   3,   7,  19,  23 },
    {  41,  45,  57,  61,   9,  13,  25,  29,
	  43,  47,  59,  63,  11,  15,  27,  31 },
	{  96, 100, 112, 116,  64,  68,  80,  84,   // column 1
       98, 102, 114, 118,  66,  70,  82,  86 },
    { 104, 108, 120, 124,  72,  76,  88,  92, 
      106, 110, 122, 126,  74,  78,  90,  94 },
    {  65,  69,  81,  85,  97, 101, 113, 117,
       67,  71,  83,  87,  99, 103, 115, 119 },
    {  73,  77,  89,  93, 105, 109, 121, 125,
       75,  79,  91,  95, 107, 111, 123, 127 },
	{ 128, 132, 144, 148, 160, 164, 176, 180,   // column 2
      130, 134, 146, 150, 162, 166, 178, 182 },
    { 136, 140, 152, 156, 168, 172, 184, 188,
      138, 142, 154, 158, 170, 174, 186, 190 },
    { 161, 165, 177, 181, 129, 133, 145, 149,
      163, 167, 179, 183, 131, 135, 147, 151 },
    { 169, 173, 185, 189, 137, 141, 153, 157,
      171, 175, 187, 191, 139, 143, 155, 159 },
	{ 224, 228, 240, 244, 192, 196, 208, 212,   // column 3
      226, 230, 242, 246, 194, 198, 210, 214 },
    { 232, 236, 248, 252, 200, 204, 216, 220,
      234, 238, 250, 254, 202, 206, 218, 222 },
    { 193, 197, 209, 213, 225, 229, 241, 245,
      195, 199, 211, 215, 227, 231, 243, 247 },
    { 201, 205, 217, 221, 233, 237, 249, 253,
      203, 207, 219, 223, 235, 239, 251, 255 },
};

u32 g_columnTable4[16][32] = {
	{   0,   8,  32,  40,  64,  72,  96, 104,   // column 0
        2,  10,  34,  42,  66,  74,  98, 106,
        4,  12,  36,  44,  68,  76, 100, 108,
        6,  14,  38,  46,  70,  78, 102, 110 },
    {  16,  24,  48,  56,  80,  88, 112, 120,
       18,  26,  50,  58,  82,  90, 114, 122,
       20,  28,  52,  60,  84,  92, 116, 124,
       22,  30,  54,  62,  86,  94, 118, 126 },
    {  65,  73,  97, 105,   1,   9,  33,  41,
       67,  75,  99, 107,   3,  11,  35,  43,
       69,  77, 101, 109,   5,  13,  37,  45, 
       71,  79, 103, 111,   7,  15,  39,  47 },
    {  81,  89, 113, 121,  17,  25,  49,  57,
       83,  91, 115, 123,  19,  27,  51,  59,
       85,  93, 117, 125,  21,  29,  53,  61,
       87,  95, 119, 127,  23,  31,  55,  63 },
	{ 192, 200, 224, 232, 128, 136, 160, 168,   // column 1
      194, 202, 226, 234, 130, 138, 162, 170,
      196, 204, 228, 236, 132, 140, 164, 172,
      198, 206, 230, 238, 134, 142, 166, 174 },
    { 208, 216, 240, 248, 144, 152, 176, 184,
      210, 218, 242, 250, 146, 154, 178, 186,
      212, 220, 244, 252, 148, 156, 180, 188,
      214, 222, 246, 254, 150, 158, 182, 190 },
    { 129, 137, 161, 169, 193, 201, 225, 233,
      131, 139, 163, 171, 195, 203, 227, 235,
      133, 141, 165, 173, 197, 205, 229, 237, 
      135, 143, 167, 175, 199, 207, 231, 239 },
    { 145, 153, 177, 185, 209, 217, 241, 249,
      147, 155, 179, 187, 211, 219, 243, 251,
      149, 157, 181, 189, 213, 221, 245, 253,
      151, 159, 183, 191, 215, 223, 247, 255 },
	{ 256, 264, 288, 296, 320, 328, 352, 360,   // column 2
      258, 266, 290, 298, 322, 330, 354, 362,
      260, 268, 292, 300, 324, 332, 356, 364,
      262, 270, 294, 302, 326, 334, 358, 366 },
    { 272, 280, 304, 312, 336, 344, 368, 376,
      274, 282, 306, 314, 338, 346, 370, 378,
      276, 284, 308, 316, 340, 348, 372, 380,
      278, 286, 310, 318, 342, 350, 374, 382 },
    { 321, 329, 353, 361, 257, 265, 289, 297,
      323, 331, 355, 363, 259, 267, 291, 299,
      325, 333, 357, 365, 261, 269, 293, 301, 
      327, 335, 359, 367, 263, 271, 295, 303 },
    { 337, 345, 369, 377, 273, 281, 305, 313,
      339, 347, 371, 379, 275, 283, 307, 315,
      341, 349, 373, 381, 277, 285, 309, 317,
      343, 351, 375, 383, 279, 287, 311, 319 },
	{ 448, 456, 480, 488, 384, 392, 416, 424,   // column 3
      450, 458, 482, 490, 386, 394, 418, 426,
      452, 460, 484, 492, 388, 396, 420, 428,
      454, 462, 486, 494, 390, 398, 422, 430 },
    { 464, 472, 496, 504, 400, 408, 432, 440,
      466, 474, 498, 506, 402, 410, 434, 442,
      468, 476, 500, 508, 404, 412, 436, 444,
      470, 478, 502, 510, 406, 414, 438, 446 },
    { 385, 393, 417, 425, 449, 457, 481, 489,
      387, 395, 419, 427, 451, 459, 483, 491,
      389, 397, 421, 429, 453, 461, 485, 493, 
      391, 399, 423, 431, 455, 463, 487, 495 },
    { 401, 409, 433, 441, 465, 473, 497, 505,
      403, 411, 435, 443, 467, 475, 499, 507,
      405, 413, 437, 445, 469, 477, 501, 509,
      407, 415, 439, 447, 471, 479, 503, 511 },
};

u32 g_pageTable32[32][64];
u32 g_pageTable32Z[32][64];
u32 g_pageTable16[64][64];
u32 g_pageTable16S[64][64];
u32 g_pageTable16Z[64][64];
u32 g_pageTable16SZ[64][64];
u32 g_pageTable8[64][128];
u32 g_pageTable4[128][128];

//maxium PSM is 58, so our arrays have 58 + 1 = 59 elements

// This table is used for fasr access to memory storage data. Field meaning is following:
// 0 -- the number (1 << [psm][0]) is number of pixels per storage format. It's  0 if stored 1 pixel, 1 for 2 pixels (16-bit), 2 for 4 pixels (PSMT8) and 3 for 8 (PSMT4)
// 5 -- is 3 - [psm][0]. Just for speed
// 3, 4 -- size-1 of pageTable for psm. It used to clump x, y otside boundaries.
// 1, 2 -- the number (1 << [psm][1]) and (1 << [psm[2]]) is also size of pageTable. So [psm][3] = (1 << [psm][1]) - 1
//	Also note, that [psm][1] =  5 + ([psm][0] + 1) / 2, and [psm][2] = 6 + [psm][0] / 2.
// 6 -- pixel mask, (1 << [psm][5]) - 1, if be used to word, it leave only bytes for pixel formay
// 7 -- starting position of data in word, PSMT8H, 4HL, 4HH are stored data not from the begining.
u32 ZZ_DT[MAX_PSM][TABLE_WIDTH] = {
	{0, 5, 6,  31,  63, 3, 0xffffffff, 0}, // 0 PSMCT32	
	{0, 5, 6,  31,  63, 3, 0x00ffffff, 0}, // 1 PSMCT24
	{1, 6, 6,  63,  63, 2, 0x0000ffff, 0}, // 2 PSMCT16
	{0, }, // 3
	{0, }, // 4
	{0, }, // 5
	{0, }, // 6
	{0, }, // 7
	{0, }, // 8
	{0, }, // 9
	{1, 6, 6,  63,  63, 2, 0x0000ffff, 0}, // 10 PSMCT16S
 	{0, }, // 11
	{0, }, // 12
	{0, }, // 13
	{0, }, // 14
	{0, }, // 15
	{0, }, // 16
	{0, }, // 17
	{0, }, // 18
	{2, 6, 7,  63, 127, 1, 0x000000ff, 0}, // 19 PSMT8
	{3, 7, 7, 127, 127, 0, 0x0000000f, 0}, // 20 PSMT4
	{0, }, // 21
	{0, }, // 22
	{0, }, // 23
	{0, }, // 24
	{0, }, // 25
	{0, }, // 26
	{0, 5, 6,  31,  63, 3, 0x000000ff, 24}, // 27 PSMT8H
	{0, }, // 28
	{0, }, // 29
	{0, }, // 30
	{0, }, // 31
	{0, }, // 32
	{0, }, // 33
	{0, }, // 34
	{0, }, // 35
	{0, 5, 6,  31,  63, 3, 0x0000000f, 24}, // 36 PSMT4HL
	{0, }, // 37
	{0, }, // 38
	{0, }, // 39
	{0, }, // 40
	{0, }, // 41
	{0, }, // 42
	{0, }, // 43
	{0, 5, 6,  31,  63, 3, 0x0000000f, 28}, // 44 PSMT4HH
	{0, }, // 45
	{0, }, // 46
	{0, }, // 47
	{0, 5, 6,  31,  63, 3, 0xffffffff, 0}, // 48 PSMCT32Z
	{0, 5, 6,  31,  63, 3, 0x00ffffff, 0}, // 49 PSMCT24Z
	{1, 6, 6,  63,  63, 2, 0x0000ffff, 0}, // 50 PSMCT16Z
	{0, }, // 51
	{0, }, // 52
	{0, }, // 53
	{0, }, // 54
	{0, }, // 55
	{0, }, // 56
	{0, }, // 57
	{1, 6, 6,  63,  63, 2, 0x0000ffff, 0}, // 58 PSMCT16SZ
	{0, }, // 59
	{0, }, // 60
	{0, }, // 61
	{0, }, // 62
	{0, }, // 63
};


//maxium PSM is 58, so our arrays have 58 + 1 = 59 elements
u32** g_pageTable[MAX_PSM] = {NULL,};
u32** g_blockTable[MAX_PSM] = {NULL, };
u32** g_columnTable[MAX_PSM] = {NULL, };
u32 g_pageTable2[MAX_PSM][127][127] = {0, };
u32** g_pageTableNew[MAX_PSM] = {NULL,};

// return array of pointer of array string,
// We SHOULD do memory allocation for u32** -- otherwize we have a lot of trouble!
// if bw and bh are set correctly, as dimensions of table, than array have pointers
// to table rows, so array[i][j] = table[i][j];
inline u32** InitTable(int bh, int bw, u32* table) {
	u32** array = (u32**)malloc(bh * sizeof(u32*));
	for (int i = 0; i < bh; i++) {
		array[i] = &table[i * bw];
	}
	return array;
}

// initialize dynamic arrays (u32**) for each regular psm.
inline void SetTable(int psm) {
	switch (psm) {
		case PSMCT32:
			g_pageTable[psm]   = InitTable( 32,  64, &g_pageTable32[0][0]);
			g_blockTable[psm]  = InitTable(  4,   8, &g_blockTable32[0][0]);
			g_columnTable[psm] = InitTable(  8,   8, &g_columnTable32[0][0]);
			break;
		case PSMCT24:
			g_pageTable[psm]   = g_pageTable[PSMCT32];;
			g_blockTable[psm]  = InitTable(  4,   8, &g_blockTable32[0][0]);
			g_columnTable[psm] = InitTable(  8,   8, &g_columnTable32[0][0]);
			break;
		case PSMCT16:
			g_pageTable[psm]   = InitTable( 64,  64, &g_pageTable16[0][0]);
			g_blockTable[psm]  = InitTable(  8,   4, &g_blockTable16[0][0]);
			g_columnTable[psm] = InitTable(  8,  16, &g_columnTable16[0][0]);
			break;
		case PSMCT16S:
			g_pageTable[psm]   = InitTable( 64,  64, &g_pageTable16S[0][0]);
			g_blockTable[psm]  = InitTable(  8,   4, &g_blockTable16S[0][0]);
			g_columnTable[psm] = InitTable(  8,  16, &g_columnTable16[0][0]);
			break;
		case PSMT8:
			g_pageTable[psm]   = InitTable( 64, 128, &g_pageTable8[0][0]);
			g_blockTable[psm]  = InitTable(  4,   8, &g_blockTable8[0][0]);
			g_columnTable[psm] = InitTable( 16,  16, &g_columnTable8[0][0]);
			break;
		case PSMT8H:
			g_pageTable[psm]   = g_pageTable[PSMCT32];
			g_blockTable[psm]  = InitTable(  4,   8, &g_blockTable8[0][0]);
			g_columnTable[psm] = InitTable( 16,  16, &g_columnTable8[0][0]);
			break;
		case PSMT4:
			g_pageTable[psm]   = InitTable(128, 128, &g_pageTable4[0][0]);
			g_blockTable[psm]  = InitTable(  8,   4, &g_blockTable4[0][0]);
			g_columnTable[psm] = InitTable( 16,  32, &g_columnTable4[0][0]);
			break;			
		case PSMT4HL:
		case PSMT4HH:
			g_pageTable[psm]   = g_pageTable[PSMCT32];
			g_blockTable[psm]  = InitTable(  8,   4, &g_blockTable4[0][0]);
			g_columnTable[psm] = InitTable( 16,  32, &g_columnTable4[0][0]);
			break;
		case PSMT32Z:
			g_pageTable[psm]   = InitTable( 32,  64, &g_pageTable32Z[0][0]);
			g_blockTable[psm]  = InitTable(  4,   8, &g_blockTable32Z[0][0]);
			g_columnTable[psm] = InitTable(  8,   8, &g_columnTable32[0][0]);
		case PSMT24Z:
			g_pageTable[psm]   = g_pageTable[PSMT32Z]; ;
			g_blockTable[psm]  = InitTable(  4,   8, &g_blockTable32Z[0][0]);
			g_columnTable[psm] = InitTable(  8,   8, &g_columnTable32[0][0]);
			break;
		case PSMT16Z:
			g_pageTable[psm]   = InitTable( 64,  64, &g_pageTable16Z[0][0]);
			g_blockTable[psm]  = InitTable(  8,   4, &g_blockTable16Z[0][0]);
			g_columnTable[psm] = InitTable(  8,  16, &g_columnTable16[0][0]);
			break;
		case PSMT16SZ:
			g_pageTable[psm]   = InitTable( 64,  64, &g_pageTable16SZ[0][0]);
			g_blockTable[psm]  = InitTable(  8,   4, &g_blockTable16SZ[0][0]);
			g_columnTable[psm] = InitTable(  8,  16, &g_columnTable16[0][0]);
			break;
	}
}

// Afther this function array's with u32** are memory set and filled. 
void FillBlockTables() {
	for (int i = 0; i < MAX_PSM; i++) 
		SetTable(i);
}

// Deallocate memory for u32** arrays.
void DestroyBlockTables() {
	for (int i = 0; i < MAX_PSM; i++) {
		if (g_pageTable[i] != NULL && (i != PSMT8H && i != PSMT4HL && i != PSMT4HH && i != PSMCT24 && i != PSMT24Z))
			free(g_pageTable[i]);
		if (g_blockTable[i] != NULL)
		      	free(g_blockTable[i]);
		if (g_columnTable[i] != NULL)
			free(g_columnTable[i]);
	}
}

void FillNewPageTable() {
	u32 address;
	u32 shift;
	int k = 0;
	for (int psm = 0; psm < MAX_PSM; psm ++)
		if (allowed_psm[psm]) {
			for (u32 i = 0; i < 127; i++)
				for(u32 j = 0; j < 127; j++) {
					address = g_pageTable[psm][i & ZZ_DT[psm][3]][j & ZZ_DT[psm][4]];
					shift = (((address << ZZ_DT[psm][5]) & 0x7 ) << 3)+ ZZ_DT[psm][7]; 				// last part is for 8H, 4HL and 4HH -- they have data from 24 and 28 byte
					g_pageTable2[k][i][j] = (address >> ZZ_DT[psm][0]) + (shift << 16); 			// now lower 16 byte of page table is 32-bit aligned address, and upper -- 
																	// shift.
				}
			g_pageTableNew[psm]   = InitTable( 128,  128, &g_pageTable2[k][0][0]);
			k++;;
		}
}

BLOCK m_Blocks[MAX_PSM]; // do so blocks are indexable
static PCSX2_ALIGNED16(u32 tempblock[64]);

#define DSTPSM gs.dstbuf.psm

#define START_HOSTLOCAL() \
	assert( gs.imageTransfer == 0 ); \
	u8* pstart = g_pbyGSMemory + gs.dstbuf.bp*256; \
	\
	const u8* pendbuf = (const u8*)pbyMem + nQWordSize*4; \
	int i = gs.imageY, j = gs.imageX; \

#define END_HOSTLOCAL() \
End: \
	if( i >= gs.imageEndY ) { \
		assert( gs.imageTransfer == -1 || i == gs.imageEndY ); \
		gs.imageTransfer = -1; \
		/*int start, end; \
		ZeroGS::GetRectMemAddress(start, end, gs.dstbuf.psm, gs.trxpos.dx, gs.trxpos.dy, gs.imageWnew, gs.imageHnew, gs.dstbuf.bp, gs.dstbuf.bw); \
		ZeroGS::g_MemTargs.ClearRange(start, end);*/ \
	} \
	else { \
		/* update new params */ \
		gs.imageY = i; \
		gs.imageX = j; \
	} \

/*
DEFINE_TRANSFERLOCAL(32, u32, 2, 32, 8, 8, _, SwizzleBlock32, PSMCT32);
DEFINE_TRANSFERLOCAL(32Z, u32, 2, 32, 8, 8, _, SwizzleBlock32, PSMT32Z);
DEFINE_TRANSFERLOCAL(24, u8, 8, 32, 8, 8, _24, SwizzleBlock24, PSMCT24);
DEFINE_TRANSFERLOCAL(24Z, u8, 8, 32, 8, 8, _24, SwizzleBlock24, PSMT24Z);
DEFINE_TRANSFERLOCAL(16, u16, 4, 16, 16, 8, _, SwizzleBlock16, PSMCT16);
DEFINE_TRANSFERLOCAL(16S, u16, 4, 16, 16, 8, _, SwizzleBlock16, PSMCT16S);
DEFINE_TRANSFERLOCAL(16Z, u16, 4, 16, 16, 8, _, SwizzleBlock16, PSMT16Z);
DEFINE_TRANSFERLOCAL(16SZ, u16, 4, 16, 16, 8, _, SwizzleBlock16, PSMT16SZ);
DEFINE_TRANSFERLOCAL(8, u8, 4, 8, 16, 16, _, SwizzleBlock8, PSMT8);
DEFINE_TRANSFERLOCAL(4, u8, 8, 4, 32, 16, _4, SwizzleBlock4, PSMT4);
DEFINE_TRANSFERLOCAL(8H, u8, 4, 32, 8, 8, _, SwizzleBlock8H, PSMT8H);
DEFINE_TRANSFERLOCAL(4HL, u8, 8, 32, 8, 8, _4, SwizzleBlock4HL, PSMT4HL);
DEFINE_TRANSFERLOCAL(4HH, u8, 8, 32, 8, 8, _4, SwizzleBlock4HH, PSMT4HH);
*/

// At the begining and the end of each string we should made unaligned writes, with nSize check's. we should be sure, that all
// this pixels are inside one widthlimit space.
template <int psm>
inline bool DoOneTransmitStep(void* pstart, int& nSize, int endj, const void* pbuf, int& k, int& i, int& j, int widthlimit) {
	for (; j < endj && nSize > 0; j++, k++, nSize -= 1) { 
		writePixelMem<psm, false>((u32*)pstart, j%2048, i%2048, (u32*)(pbuf), k, gs.dstbuf.bw); 
	}
	
	return (nSize == 0);
}

// FFX have PSMT8 transmit (starting intro -- sword and hairs).
// Persona 4 texts at start are PSMCT32 (and there is also PSMCT16 transmit somwhere after).
// Tekken V have PSMCT24 and PSMT4 transfers

// This function transfer "Y" block pixels. I use little another code than Zerofrog. My code ofthenly use widthmult != 1 addition (Zerofrog's code
// have an stict condition for fast path: width of transferred data should be widthlimit multiplicate and j-EndY also should be multiplicate. But
// usual data block of 255 pixels become transfered by 1.
// I should check, maybe Unaligned_Start and Unaligned_End are othenly == 0, that I could try a fastpath -- with this block's off.
template <int psm, int widthlimit>
inline bool TRANSMIT_HOSTLOCAL_Y(u32* pbuf, int& nSize, u8* pstart, int endY, int& i, int& j, int& k) {
//	if (psm != 19 && psm != 0 && psm != 20 && psm != 1)
//		ERROR_LOG("This is usable function TRANSMIT_HOSTLOCAL_Y at ZZoglMem.cpp %d %d %d %d %d\n", psm, widthlimit, i, j, nSize);

	int q = (gs.trxpos.dx - j) % widthlimit; 
	if (DoOneTransmitStep<psm>(pstart, nSize, q, pbuf, k, i, j, widthlimit)) return true;						// After this j and dx are compatible by modyle of widthlimit
	
	int Unaligned_Start = (gs.trxpos.dx % widthlimit == 0) ? 0 : widthlimit - gs.trxpos.dx % widthlimit;					// gs.trpos.dx + Unaligned_Start is multiple of widthlimit

	for (; i < endY; ++i) {
		if (DoOneTransmitStep<psm>(pstart, nSize, j + Unaligned_Start, pbuf, k, i, j, widthlimit)) return true;			// This operation made j % widthlimit == 0.
		assert (j % widthlimit != 0);													 

		for (; j < gs.imageEndX - widthlimit + 1 && nSize >= widthlimit; j += widthlimit, nSize -= widthlimit) { 			
			writePixelsFromMemory<psm, true, widthlimit>(pstart, pbuf, k, j % 2048, i % 2048,  gs.dstbuf.bw);
		}
	
		assert ( gs.imageEndX - j < widthlimit || nSize < widthlimit);	
		if (DoOneTransmitStep<psm>(pstart, nSize, gs.imageEndX, pbuf, k, i, j, widthlimit)) return true;				// There are 2 reasons for finish of previous for: 1) nSize < widthlimit
																		// 2) j > gs.imageEndX - widthlimit + 1. We would try to write pixels up do
																		// EndX, it's no more widthlimit pixels																		
		j = gs.trxpos.dx; 
	}	

	return false;
}

// PSMT4 -- Tekken V
template <int psm, int widthlimit>
inline void TRANSMIT_HOSTLOCAL_X(u32* pbuf, int& nSize, u8* pstart, int& i, int& j, int& k, int blockheight, int startX, int pitch, int fracX) {
	if (psm != 19 && psm != 20)
		ERROR_LOG("This is usable function TRANSMIT_HOSTLOCAL_X at ZZoglMem.cpp %d %d %d %d %d\n", psm, widthlimit, i, j, nSize);

	for(int tempi = 0; tempi < blockheight; ++tempi) { 
		for(j = startX; j < gs.imageEndX; j++, k++) { 
			writePixelMem<psm, false>((u32*)pstart, j%2048, (i + tempi)%2048, (u32*)(pbuf), k, gs.dstbuf.bw); 
		} 
		k += ( pitch - fracX ); 
	} 
} 

// transfers whole rows
#define TRANSMIT_HOSTLOCAL_Y_(psm, T, widthlimit, endY) { \
	assert( (nSize%widthlimit) == 0 && widthlimit <= 4 ); \
	if( (gs.imageEndX-gs.trxpos.dx)%widthlimit ) { \
		/*GS_LOG("Bad Transmission! %d %d, psm: %d\n", gs.trxpos.dx, gs.imageEndX, DSTPSM);*/ \
		for(; i < endY; ++i) { \
			for(; j < gs.imageEndX && nSize > 0; j += 1, nSize -= 1, pbuf += 1) { \
				/* write as many pixel at one time as possible */ \
				writePixel##psm##_0(pstart, j%2048, i%2048, pbuf[0], gs.dstbuf.bw); \
			} \
		} \
	} \
	for(; i < endY; ++i) { \
		for(; j < gs.imageEndX && nSize > 0; j += widthlimit, nSize -= widthlimit, pbuf += widthlimit) { \
			/* write as many pixel at one time as possible */ \
			if( nSize < widthlimit ) goto End; \
			writePixel##psm##_0(pstart, j%2048, i%2048, pbuf[0], gs.dstbuf.bw); \
			\
			if( widthlimit > 1 ) { \
				writePixel##psm##_0(pstart, (j+1)%2048, i%2048, pbuf[1], gs.dstbuf.bw); \
				\
				if( widthlimit > 2 ) { \
					writePixel##psm##_0(pstart, (j+2)%2048, i%2048, pbuf[2], gs.dstbuf.bw); \
					\
					if( widthlimit > 3 ) { \
						writePixel##psm##_0(pstart, (j+3)%2048, i%2048, pbuf[3], gs.dstbuf.bw); \
					} \
				} \
			} \
		} \
		\
		if( j >= gs.imageEndX ) { assert(j == gs.imageEndX); j = gs.trxpos.dx; } \
		else { assert( gs.imageTransfer == -1 || nSize*sizeof(T)/4 == 0 ); goto End; } \
	} \
} \

// transmit until endX, don't check size since it has already been prevalidated
#define TRANSMIT_HOSTLOCAL_X_(psm, T, widthlimit, blockheight, startX) { \
	for(int tempi = 0; tempi < blockheight; ++tempi) { \
		for(j = startX; j < gs.imageEndX; j++, pbuf++) { \
			writePixel##psm##_0(pstart, j%2048, (i+tempi)%2048, pbuf[0], gs.dstbuf.bw); \
		} \
		pbuf += pitch-fracX; \
	} \
} \

// transfers whole rows
#define TRANSMIT_HOSTLOCAL_Y_24(psm, T, widthlimit, endY) { \
	if( widthlimit != 8 || ((gs.imageEndX-gs.trxpos.dx)%widthlimit) ) { \
		/*GS_LOG("Bad Transmission! %d %d, psm: %d\n", gs.trxpos.dx, gs.imageEndX, DSTPSM);*/ \
		for(; i < endY; ++i) { \
			for(; j < gs.imageEndX && nSize > 0; j += 1, nSize -= 1, pbuf += 3) { \
				writePixel##psm##_0(pstart, j%2048, i%2048, *(u32*)(pbuf), gs.dstbuf.bw); \
			} \
			\
			if( j >= gs.imageEndX ) { assert(gs.imageTransfer == -1 || j == gs.imageEndX); j = gs.trxpos.dx; } \
			else { assert( gs.imageTransfer == -1 || nSize == 0 ); goto End; } \
		} \
	} \
	else { \
		assert( /*(nSize%widthlimit) == 0 &&*/ widthlimit == 8 ); \
		for(; i < endY; ++i) { \
			for(; j < gs.imageEndX && nSize > 0; j += widthlimit, nSize -= widthlimit, pbuf += 3*widthlimit) { \
				if( nSize < widthlimit ) goto End; \
				/* write as many pixel at one time as possible */ \
				writePixel##psm##_0(pstart, j%2048, i%2048, *(u32*)(pbuf+0), gs.dstbuf.bw); \
				writePixel##psm##_0(pstart, (j+1)%2048, i%2048, *(u32*)(pbuf+3), gs.dstbuf.bw); \
				writePixel##psm##_0(pstart, (j+2)%2048, i%2048, *(u32*)(pbuf+6), gs.dstbuf.bw); \
				writePixel##psm##_0(pstart, (j+3)%2048, i%2048, *(u32*)(pbuf+9), gs.dstbuf.bw); \
				writePixel##psm##_0(pstart, (j+4)%2048, i%2048, *(u32*)(pbuf+12), gs.dstbuf.bw); \
				writePixel##psm##_0(pstart, (j+5)%2048, i%2048, *(u32*)(pbuf+15), gs.dstbuf.bw); \
				writePixel##psm##_0(pstart, (j+6)%2048, i%2048, *(u32*)(pbuf+18), gs.dstbuf.bw); \
				writePixel##psm##_0(pstart, (j+7)%2048, i%2048, *(u32*)(pbuf+21), gs.dstbuf.bw); \
			} \
			\
			if( j >= gs.imageEndX ) { assert(gs.imageTransfer == -1 || j == gs.imageEndX); j = gs.trxpos.dx; } \
			else { \
				if( nSize < 0 ) { \
					/* extracted too much */ \
					assert( (nSize%3)==0 && nSize > -24 ); \
					j += nSize/3; \
					nSize = 0; \
				} \
				assert( gs.imageTransfer == -1 || nSize == 0 ); \
				goto End; \
			} \
		} \
	} \
} \

// transmit until endX, don't check size since it has already been prevalidated
#define TRANSMIT_HOSTLOCAL_X_24(psm, T, widthlimit, blockheight, startX) { \
	for(int tempi = 0; tempi < blockheight; ++tempi) { \
		for(j = startX; j < gs.imageEndX; j++, pbuf += 3) { \
			writePixel##psm##_0(pstart, j%2048, (i+tempi)%2048, *(u32*)pbuf, gs.dstbuf.bw); \
		} \
		pbuf += 3*(pitch-fracX); \
	} \
} \

// meant for 4bit transfers
#define TRANSMIT_HOSTLOCAL_Y_4(psm, T, widthlimit, endY) { \
	for(; i < endY; ++i) { \
		for(; j < gs.imageEndX && nSize > 0; j += widthlimit, nSize -= widthlimit) { \
			/* write as many pixel at one time as possible */ \
			writePixel##psm##_0(pstart, j%2048, i%2048, *pbuf&0x0f, gs.dstbuf.bw); \
			writePixel##psm##_0(pstart, (j+1)%2048, i%2048, *pbuf>>4, gs.dstbuf.bw); \
			pbuf++; \
			if( widthlimit > 2 ) { \
				writePixel##psm##_0(pstart, (j+2)%2048, i%2048, *pbuf&0x0f, gs.dstbuf.bw); \
				writePixel##psm##_0(pstart, (j+3)%2048, i%2048, *pbuf>>4, gs.dstbuf.bw); \
				pbuf++; \
				\
				if( widthlimit > 4 ) { \
					writePixel##psm##_0(pstart, (j+4)%2048, i%2048, *pbuf&0x0f, gs.dstbuf.bw); \
					writePixel##psm##_0(pstart, (j+5)%2048, i%2048, *pbuf>>4, gs.dstbuf.bw); \
					pbuf++; \
					\
					if( widthlimit > 6 ) { \
						writePixel##psm##_0(pstart, (j+6)%2048, i%2048, *pbuf&0x0f, gs.dstbuf.bw); \
						writePixel##psm##_0(pstart, (j+7)%2048, i%2048, *pbuf>>4, gs.dstbuf.bw); \
						pbuf++; \
					} \
				} \
			} \
		} \
		\
		if( j >= gs.imageEndX ) { j = gs.trxpos.dx; } \
		else { assert( gs.imageTransfer == -1 || (nSize/32) == 0 ); goto End; } \
	} \
} \

// transmit until endX, don't check size since it has already been prevalidated
#define TRANSMIT_HOSTLOCAL_X_4(psm, T, widthlimit, blockheight, startX) { \
	for(int tempi = 0; tempi < blockheight; ++tempi) { \
		for(j = startX; j < gs.imageEndX; j+=2, k++) { \
			writePixel##psm##_0(pstart, j%2048, (i+tempi)%2048, pbuf[k]&0x0f, gs.dstbuf.bw); \
			writePixel##psm##_0(pstart, (j+1)%2048, (i+tempi)%2048, pbuf[k]>>4, gs.dstbuf.bw); \
		} \
		k += (pitch-fracX)/2; \
	} \
} \

template <int psm>
inline int TRANSMIT_PITCH(int pitch) {
	return (PSM_BITS_PER_PIXEL<psm>() * pitch) >> 3;
}

// calculate pitch in source buffer
#define TRANSMIT_PITCH_(pitch, T) (pitch*sizeof(T))
#define TRANSMIT_PITCH_24(pitch, T) (pitch*3)
#define TRANSMIT_PITCH_4(pitch, T) (pitch/2)

// special swizzle macros
#define SwizzleBlock24(dst, src, pitch) { \
	u8* pnewsrc = src; \
	u32* pblock = tempblock; \
	\
	for(int by = 0; by < 7; ++by, pblock += 8, pnewsrc += pitch-24) { \
		for(int bx = 0; bx < 8; ++bx, pnewsrc += 3) { \
			pblock[bx] = *(u32*)pnewsrc; \
		} \
	} \
	for(int bx = 0; bx < 7; ++bx, pnewsrc += 3) { \
		/* might be 1 byte out of bounds of GS memory */ \
		pblock[bx] = *(u32*)pnewsrc; \
	} \
	/* do 3 bytes for the last copy */ \
	*((u8*)pblock+28) = pnewsrc[0]; \
	*((u8*)pblock+29) = pnewsrc[1]; \
	*((u8*)pblock+30) = pnewsrc[2]; \
	SwizzleBlock32((u8*)dst, (u8*)tempblock, 32, 0x00ffffff); \
} \

#define SwizzleBlock24u SwizzleBlock24

#define SwizzleBlock8H(dst, src, pitch) { \
	u8* pnewsrc = src; \
	u32* pblock = tempblock; \
	\
	for(int by = 0; by < 8; ++by, pblock += 8, pnewsrc += pitch) { \
		u32 u = *(u32*)pnewsrc; \
		pblock[0] = u<<24; \
		pblock[1] = u<<16; \
		pblock[2] = u<<8; \
		pblock[3] = u; \
		u = *(u32*)(pnewsrc+4); \
		pblock[4] = u<<24; \
		pblock[5] = u<<16; \
		pblock[6] = u<<8; \
		pblock[7] = u; \
	} \
	SwizzleBlock32((u8*)dst, (u8*)tempblock, 32, 0xff000000); \
} \

#define SwizzleBlock8Hu SwizzleBlock8H

#define SwizzleBlock4HH(dst, src, pitch) { \
	u8* pnewsrc = src; \
	u32* pblock = tempblock; \
	\
	for(int by = 0; by < 8; ++by, pblock += 8, pnewsrc += pitch) { \
		u32 u = *(u32*)pnewsrc; \
		pblock[0] = u<<28; \
		pblock[1] = u<<24; \
		pblock[2] = u<<20; \
		pblock[3] = u<<16; \
		pblock[4] = u<<12; \
		pblock[5] = u<<8; \
		pblock[6] = u<<4; \
		pblock[7] = u; \
	} \
	SwizzleBlock32((u8*)dst, (u8*)tempblock, 32, 0xf0000000); \
} \

#define SwizzleBlock4HHu SwizzleBlock4HH

#define SwizzleBlock4HL(dst, src, pitch) { \
	ERROR_LOG("A 0x%x 0x%x %d\n", dst, src, pitch); \
	u8* pnewsrc = src; \
	u32* pblock = tempblock; \
	\
	for(int by = 0; by < 8; ++by, pblock += 8, pnewsrc += pitch) { \
		u32 u = *(u32*)pnewsrc; \
		pblock[0] = u<<24; \
		pblock[1] = u<<20; \
		pblock[2] = u<<16; \
		pblock[3] = u<<12; \
		pblock[4] = u<<8; \
		pblock[5] = u<<4; \
		pblock[6] = u; \
		pblock[7] = u>>4; \
	} \
	SwizzleBlock32((u8*)dst, (u8*)tempblock, 32, 0x0f000000); \
} \

#define SwizzleBlock4HLu SwizzleBlock4HL

// ------------------------
// |              Y       |
// ------------------------
// |        block     |   |
// |   aligned area   | X |
// |                  |   |
// ------------------------
// |              Y       |
// ------------------------

 
template<int psmX, int widthlimit, int blockbits, int blockwidth, int blockheight>
int TransferHostLocal(const void* pbyMem, u32 nQWordSize) { 
	START_HOSTLOCAL(); 
	
	const u8* pbuf = (const u8*)pbyMem; \
	int nLeftOver = (nQWordSize*4*2)%(TRANSMIT_PITCH<psmX>(2)); \
	int nSize = nQWordSize*4*2/TRANSMIT_PITCH<psmX>(2); \
	nSize = min(nSize, gs.imageWnew * gs.imageHnew); \
	\
	int pitch, area, fracX; \
	int endY = ROUND_UPPOW2(i, blockheight); \
	int alignedY = ROUND_DOWNPOW2(gs.imageEndY, blockheight); \
	int alignedX = ROUND_DOWNPOW2(gs.imageEndX, blockwidth); \
	bool bAligned, bCanAlign = MOD_POW2(gs.trxpos.dx, blockwidth) == 0 && (j == gs.trxpos.dx) && (alignedY > endY) && alignedX > gs.trxpos.dx; \
	\
	if( (gs.imageEndX-gs.trxpos.dx)%widthlimit ) { \
		/* hack */ \
		int testwidth = (int)nSize - (gs.imageEndY-i)*(gs.imageEndX-gs.trxpos.dx)+(j-gs.trxpos.dx); \
		if((testwidth <= widthlimit) && (testwidth >= -widthlimit)) { \
			/* don't transfer */ \
			/*DEBUG_LOG("bad texture %s: %d %d %d\n", #psm, gs.trxpos.dx, gs.imageEndX, nQWordSize);*/ \
			gs.imageTransfer = -1; \
		} \
		bCanAlign = false; \
	} \
	\
	/* first align on block boundary */ \
	if( MOD_POW2(i, blockheight) || !bCanAlign ) { \
		\
		if( !bCanAlign ) \
			endY = gs.imageEndY; /* transfer the whole image */ \
		else \
			assert( endY < gs.imageEndY); /* part of alignment condition */ \
			\
		int limit = widthlimit; \
		if( ((gs.imageEndX-gs.trxpos.dx)%widthlimit) || ((gs.imageEndX-j)%widthlimit) )  \
			/* transmit with a width of 1 */ \
			limit = 1 + (DSTPSM == 0x14); \
		/*TRANSMIT_HOSTLOCAL_Y##TransSfx(psm, T, limit, endY)*/ \
		int k = 0; \
		if (TRANSMIT_HOSTLOCAL_Y<psmX, widthlimit>((u32*)pbuf, nSize, pstart, endY, i, j, k)) goto End; \
		pbuf += TRANSMIT_PITCH<psmX>(k); \
		if( nSize == 0 || i == gs.imageEndY ) \
			goto End; \
	} \
	\
	assert( MOD_POW2(i, blockheight) == 0 && j == gs.trxpos.dx); \
	\
	/* can align! */ \
	pitch = gs.imageEndX-gs.trxpos.dx; \
	area = pitch*blockheight; \
	fracX = gs.imageEndX-alignedX; \
	\
	/* on top of checking whether pbuf is alinged, make sure that the width is at least aligned to its limits (due to bugs in pcsx2) */ \
	bAligned = !((uptr)pbuf & 0xf) && (TRANSMIT_PITCH<psmX>(pitch)&0xf) == 0; \
	\
	/* transfer aligning to blocks */ \
	for(; i < alignedY && nSize >= area; i += blockheight, nSize -= area) { \
		\
		for(int tempj = gs.trxpos.dx; tempj < alignedX; tempj += blockwidth, pbuf += TRANSMIT_PITCH<psmX>(blockwidth)) { \
			SwizzleBlock<psmX>((u32*)(pstart + getPixelAddress<psmX>(tempj, i, gs.dstbuf.bw)*blockbits/8), \
				(u32*)pbuf, TRANSMIT_PITCH<psmX>(pitch)); \
		} \
		\
		/* transfer the rest */ \
		if( alignedX < gs.imageEndX ) { \
			int k = 0; \
			TRANSMIT_HOSTLOCAL_X<psmX, widthlimit>((u32*)pbuf, nSize, pstart, i, j, k, blockheight, alignedX, pitch, fracX); \
			pbuf += TRANSMIT_PITCH<psmX>(k - alignedX + gs.trxpos.dx); \
		} \
		else pbuf += (blockheight-1)*TRANSMIT_PITCH<psmX>(pitch); \
		j = gs.trxpos.dx; \
	} \
	\
	if( TRANSMIT_PITCH<psmX>(nSize)/4 > 0 ) { \
		int k = 0; \
		TRANSMIT_HOSTLOCAL_Y<psmX, widthlimit>((u32*)pbuf, nSize, pstart, gs.imageEndY, i, j, k); \
		pbuf += TRANSMIT_PITCH<psmX>(k); \
		/* sometimes wrong sizes are sent (tekken tag) */ \
		assert( gs.imageTransfer == -1 || TRANSMIT_PITCH<psmX>(nSize)/4 <= 2 ); \
	} \
	\
	END_HOSTLOCAL(); \
	return (nSize * TRANSMIT_PITCH<psmX>(2) + nLeftOver)/2; \
} \

#define DEFINE_TRANSFERLOCAL(psm, T, widthlimit, blockbits, blockwidth, blockheight, TransSfx, swizzleblock, psmX) \
inline int TransferHostLocal##psm(const void* pbyMem, u32 nQWordSize) { \
	return TransferHostLocal<psmX, widthlimit, blockbits, blockwidth, blockheight>( pbyMem, nQWordSize); \
}


DEFINE_TRANSFERLOCAL(32, u32, 2, 32, 8, 8, _, SwizzleBlock32, PSMCT32);
DEFINE_TRANSFERLOCAL(32Z, u32, 2, 32, 8, 8, _, SwizzleBlock32, PSMT32Z);
DEFINE_TRANSFERLOCAL(24, u8, 8, 32, 8, 8, _24, SwizzleBlock24, PSMCT24);
DEFINE_TRANSFERLOCAL(24Z, u8, 8, 32, 8, 8, _24, SwizzleBlock24, PSMT24Z);
DEFINE_TRANSFERLOCAL(16, u16, 4, 16, 16, 8, _, SwizzleBlock16, PSMCT16);
DEFINE_TRANSFERLOCAL(16S, u16, 4, 16, 16, 8, _, SwizzleBlock16, PSMCT16S);
DEFINE_TRANSFERLOCAL(16Z, u16, 4, 16, 16, 8, _, SwizzleBlock16, PSMT16Z);
DEFINE_TRANSFERLOCAL(16SZ, u16, 4, 16, 16, 8, _, SwizzleBlock16, PSMT16SZ);
DEFINE_TRANSFERLOCAL(8, u8, 4, 8, 16, 16, _, SwizzleBlock8, PSMT8);
DEFINE_TRANSFERLOCAL(4, u8, 8, 4, 32, 16, _4, SwizzleBlock4, PSMT4);
DEFINE_TRANSFERLOCAL(8H, u8, 4, 32, 8, 8, _, SwizzleBlock8H, PSMT8H);
DEFINE_TRANSFERLOCAL(4HL, u8, 8, 32, 8, 8, _4, SwizzleBlock4HL, PSMT4HL);
DEFINE_TRANSFERLOCAL(4HH, u8, 8, 32, 8, 8, _4, SwizzleBlock4HH, PSMT4HH);

void TransferLocalHost32(void* pbyMem, u32 nQWordSize)
{FUNCLOG
}

void TransferLocalHost24(void* pbyMem, u32 nQWordSize)
{FUNCLOG
}

void TransferLocalHost16(void* pbyMem, u32 nQWordSize)
{FUNCLOG
}

void TransferLocalHost16S(void* pbyMem, u32 nQWordSize)
{FUNCLOG
}

void TransferLocalHost8(void* pbyMem, u32 nQWordSize)
{
}

void TransferLocalHost4(void* pbyMem, u32 nQWordSize)
{FUNCLOG
}

void TransferLocalHost8H(void* pbyMem, u32 nQWordSize)
{FUNCLOG
}

void TransferLocalHost4HL(void* pbyMem, u32 nQWordSize)
{FUNCLOG
}

void TransferLocalHost4HH(void* pbyMem, u32 nQWordSize)
{
}

void TransferLocalHost32Z(void* pbyMem, u32 nQWordSize)
{FUNCLOG
}

void TransferLocalHost24Z(void* pbyMem, u32 nQWordSize)
{FUNCLOG
}

void TransferLocalHost16Z(void* pbyMem, u32 nQWordSize)
{FUNCLOG
}

void TransferLocalHost16SZ(void* pbyMem, u32 nQWordSize)
{FUNCLOG
}

inline void FILL_BLOCK(BLOCK& b, int floatfmt, vector<char>& vBlockData, vector<char>& vBilinearData, int ox, int oy, int psmX) { 
	int bw = ZZ_DT[psmX][4] + 1;
	int bh = ZZ_DT[psmX][3] + 1;
	int mult = 1 << ZZ_DT[psmX][0];

	b.vTexDims = float4 (BLOCK_TEXWIDTH/(float)(bw), BLOCK_TEXHEIGHT/(float)(bh), 0, 0); 
	b.vTexBlock = float4( (float)bw/BLOCK_TEXWIDTH, (float)bh/BLOCK_TEXHEIGHT, ((float)ox+0.2f)/BLOCK_TEXWIDTH, ((float)oy+0.05f)/BLOCK_TEXHEIGHT); 
	b.width = bw; 
	b.height = bh; 
	b.colwidth = bh / 4; 
	b.colheight = bw / 8; 
	b.bpp = 32/mult; 
	
	b.pageTable = g_pageTable[psmX]; 
	b.blockTable = g_blockTable[psmX]; 
	b.columnTable = g_columnTable[psmX]; 
	assert( sizeof(g_pageTable[psmX]) == bw*bh*sizeof(g_pageTable[psmX][0][0]) ); 
	float* psrcf = (float*)&vBlockData[0] + ox + oy * BLOCK_TEXWIDTH; 
	u16* psrcw = (u16*)&vBlockData[0] + ox + oy * BLOCK_TEXWIDTH; 
	for(int i = 0; i < bh; ++i) { 
		for(int j = 0; j < bw; ++j) { 
			/* fill the table */ 
			u32 u = g_blockTable[psmX][(i / b.colheight)][(j / b.colwidth)] * 64 * mult + g_columnTable[psmX][i%b.colheight][j%b.colwidth]; 
			b.pageTable[i][j] = u; 
			if( floatfmt ) { 
				psrcf[i*BLOCK_TEXWIDTH+j] = (float)(u) / (float)(GPU_TEXWIDTH*mult); 
			} 
			else { 
				psrcw[i*BLOCK_TEXWIDTH+j] = u; 
			} 
		} 
	} 
	
	if( floatfmt ) { 
		float4* psrcv = (float4*)&vBilinearData[0] + ox + oy * BLOCK_TEXWIDTH; 
		for(int i = 0; i < bh; ++i) { 
			for(int j = 0; j < bw; ++j) { 
				float4* pv = &psrcv[i*BLOCK_TEXWIDTH+j]; 
				pv->x = psrcf[i*BLOCK_TEXWIDTH+j]; 
				pv->y = psrcf[i*BLOCK_TEXWIDTH+((j+1)%bw)]; 
				pv->z = psrcf[((i+1)%bh)*BLOCK_TEXWIDTH+j]; 
				pv->w = psrcf[((i+1)%bh)*BLOCK_TEXWIDTH+((j+1)%bw)]; 
			} 
		} 
	} 
}


#define FILL_BLOCK_FUNCIONS(psm) { \
	b.TransferHostLocal = TransferHostLocal##psm; \
	b.TransferLocalHost = TransferLocalHost##psm; \
} \

void BLOCK::FillBlocks(vector<char>& vBlockData, vector<char>& vBilinearData, int floatfmt)
{
	FUNCLOG
	vBlockData.resize(BLOCK_TEXWIDTH * BLOCK_TEXHEIGHT * (floatfmt ? 4 : 2));
	if( floatfmt )
		vBilinearData.resize(BLOCK_TEXWIDTH * BLOCK_TEXHEIGHT * sizeof(float4));

	int i, j;
	BLOCK b;
	float* psrcf = NULL;
	u16* psrcw = NULL;
	float4* psrcv = NULL;

	memset(m_Blocks, 0, sizeof(m_Blocks));

	// 32
	FILL_BLOCK(b, floatfmt,  vBlockData, vBilinearData, 0, 0, PSMCT32);
	FILL_BLOCK_FUNCIONS(32);
	m_Blocks[PSMCT32] = b;

	// 24 (same as 32 except write/readPixel are different)
	FILL_BLOCK_FUNCIONS(24);
	 m_Blocks[PSMCT24] = b;

	// 8H (same as 32 except write/readPixel are different)
	FILL_BLOCK_FUNCIONS(8H);	 
	m_Blocks[PSMT8H] = b;

	FILL_BLOCK_FUNCIONS(4HL);
	m_Blocks[PSMT4HL] = b;

	FILL_BLOCK_FUNCIONS(4HH);
	m_Blocks[PSMT4HH] = b;

	// 32z
	FILL_BLOCK(b, floatfmt, vBlockData, vBilinearData, 64, 0, PSMT32Z);
	FILL_BLOCK_FUNCIONS(32Z);
	m_Blocks[PSMT32Z] = b;

	// 24Z (same as 32Z except write/readPixel are different)
	FILL_BLOCK_FUNCIONS(24Z);	 
	m_Blocks[PSMT24Z] = b;

	// 16
	FILL_BLOCK(b, floatfmt,  vBlockData, vBilinearData,  0, 32, PSMCT16);
	FILL_BLOCK_FUNCIONS(16);
	m_Blocks[PSMCT16] = b;

	// 16s
	FILL_BLOCK(b, floatfmt,  vBlockData, vBilinearData,  64, 32, PSMCT16S);
	FILL_BLOCK_FUNCIONS(16S);
	m_Blocks[PSMCT16S] = b;

	// 16z
	FILL_BLOCK(b, floatfmt,  vBlockData, vBilinearData,  0, 96, PSMT16Z);
	FILL_BLOCK_FUNCIONS(16Z);
	m_Blocks[PSMT16Z] = b;

	// 16sz
	FILL_BLOCK(b, floatfmt,  vBlockData, vBilinearData, 64, 96, PSMT16SZ);
	FILL_BLOCK_FUNCIONS(16SZ);
	m_Blocks[PSMT16SZ] = b;

	// 8
	FILL_BLOCK(b, floatfmt,  vBlockData, vBilinearData,  0, 160, PSMT8);
	FILL_BLOCK_FUNCIONS(8);
	m_Blocks[PSMT8] = b;

	// 4
	FILL_BLOCK(b, floatfmt,  vBlockData, vBilinearData,  0, 224, PSMT4);
	FILL_BLOCK_FUNCIONS(4);
	m_Blocks[PSMT4] = b;
}

#endif
