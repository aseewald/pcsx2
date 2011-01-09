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

#ifndef __ZZOGL_MEM_H__
#define __ZZOGL_MEM_H__

#include <assert.h>
#include <vector>
#include "GS.h"
#include "Util.h"

#ifndef ZZNORMAL_MEMORY
// works only when base is a power of 2
#define ROUND_UPPOW2(val, base)	(((val)+(base-1))&~(base-1))
#define ROUND_DOWNPOW2(val, base)	((val)&~(base-1))
#define MOD_POW2(val, base) ((val)&(base-1))

// d3d texture dims
#define BLOCK_TEXWIDTH	128
#define BLOCK_TEXHEIGHT 512

// rest not visible externally
struct BLOCK
{
	BLOCK() { memset(this, 0, sizeof(BLOCK)); }

	// shader constants for this block
	float4 vTexBlock;
	float4 vTexDims;
	int width, height;	// dims of one page in pixels
	int bpp;
	int colwidth, colheight;
	u32** pageTable;	// offset inside each page
	u32** blockTable;
	u32** columnTable;

	// Nobody use this, so we better remove it.
//	u32 (*getPixelAddress)(int x, int y, u32 bp, u32 bw);
//	u32 (*getPixelAddress_0)(int x, int y, u32 bw);
//	void (*writePixel)(void* pmem, int x, int y, u32 pixel, u32 bp, u32 bw);
//	void (*writePixel_0)(void* pmem, int x, int y, u32 pixel, u32 bw);
//	u32 (*readPixel)(const void* pmem, int x, int y, u32 bp, u32 bw);
//	u32 (*readPixel_0)(const void* pmem, int x, int y, u32 bw);
	int (*TransferHostLocal)(const void* pbyMem, u32 nQWordSize);
	void (*TransferLocalHost)(void* pbyMem, u32 nQWordSize);

	// texture must be of dims BLOCK_TEXWIDTH and BLOCK_TEXHEIGHT
	static void FillBlocks(std::vector<char>& vBlockData, std::vector<char>& vBilinearData, int floatfmt);
};

void FillBlockTables();
void DestroyBlockTables();
void FillNewPageTable();

extern BLOCK m_Blocks[];

extern u32 g_blockTable32[4][8];
extern u32 g_blockTable32Z[4][8];
extern u32 g_blockTable16[8][4];
extern u32 g_blockTable16S[8][4];
extern u32 g_blockTable16Z[8][4];
extern u32 g_blockTable16SZ[8][4];
extern u32 g_blockTable8[4][8];
extern u32 g_blockTable4[8][4];

extern u32 g_columnTable32[8][8];
extern u32 g_columnTable16[8][16];
extern u32 g_columnTable8[16][16];
extern u32 g_columnTable4[16][32];

extern u32 g_pageTable32[32][64];
extern u32 g_pageTable32Z[32][64];
extern u32 g_pageTable16[64][64];
extern u32 g_pageTable16S[64][64];
extern u32 g_pageTable16Z[64][64];
extern u32 g_pageTable16SZ[64][64];
extern u32 g_pageTable8[64][128];
extern u32 g_pageTable4[128][128];

static __forceinline void MaskedOR(u32* dst, u32 pixel, u32 mask = 0xffffffff) {
	if (mask == 0xffffffff)
		*dst = pixel;
	else
		*dst = (*dst & (~mask)) | (pixel & mask);
}

// This two defines seems like idiotic code, but in reallity it have one, but big impartance -- this code
// made psm variable (and psm2 in second case) -- constant, so optimiser could properly pass proper function
#define PSM_SWITCHCASE(X) { \
	switch (psm) { \
		case PSMCT32: { \
     			const int psmC = PSMCT32; \
			X; } \
			break; \
		case PSMT32Z: { \
			const int psmC = PSMT32Z; \
			X; } \
			break; \
		case PSMCT24: { \
      			const int psmC = PSMCT24; \
			X; }  \
			break; \
		case PSMT24Z: { \
			const int psmC = PSMT24Z; \
			X; }  \
			break; \
		case PSMCT16: { \
      			const int psmC = PSMCT16; \
			X; }  \
			break; \
		case PSMCT16S: { \
      			const int psmC = PSMCT16S; \
			X; }  \
			break; \
		case PSMT16Z: { \
			const int psmC = PSMT16Z; \
			X; }  \
			break; \
		case PSMT16SZ: { \
			const int psmC = PSMT16SZ; \
			X; }  \
			break; \
		case PSMT8: { \
			const int psmC = PSMT8; \
			X; }  \
			break; \
		case PSMT8H: { \
			const int psmC = PSMT8H; \
			X; }  \
			break; \
		case PSMT4HH: { \
			const int psmC = PSMT4HH; \
			X; }  \
			break; \
		case PSMT4HL: { \
			const int psmC = PSMT4HL; \
			X; }  \
			break; \
		case PSMT4: { \
			const int psmC = PSMT4; \
			X; }  \
			break; \
	}\
}

#define PSM_SWITCHCASE_2(X) { \
	switch (psm) { \
		case PSMCT32: \
			if( psm2 == PSMCT32 ) 		{ const int psmC = PSMCT32, psmC1 = PSMCT32; X; } \
			else 				{ const int psmC = PSMCT32, psmC1 = PSMT32Z; X; } \
			break; \
		case PSMCT24: \
			if( psm2 == PSMCT24 ) 		{ const int psmC = PSMCT24, psmC1 = PSMCT24; X; } \
			else 				{ const int psmC = PSMCT24, psmC1 = PSMT24Z; X; } \
			break; \
		case PSMT32Z: \
			if( psm2 == PSMT32Z ) 		{ const int psmC = PSMT32Z, psmC1 = PSMCT32; X; } \
			else  				{ const int psmC = PSMT32Z, psmC1 = PSMT32Z; X; } \
			break; \
		case PSMT24Z: \
			if( psm2 == PSMCT24 ) 		{ const int psmC = PSMT24Z, psmC1 = PSMCT24; X; } \
			else 				{ const int psmC = PSMT24Z, psmC1 = PSMT24Z; X; } \
			break; \
		case PSMCT16: \
			switch(psm2) { \
				case PSMCT16: 		{ const int psmC = PSMCT16, psmC1 = PSMCT16; X; }  break; \
				case PSMCT16S:  	{ const int psmC = PSMCT16, psmC1 = PSMCT16S; X; } break; \
				case PSMT16Z: 	      	{ const int psmC = PSMCT16, psmC1 = PSMT16Z; X; }  break; \
				case PSMT16SZ: 	      	{ const int psmC = PSMCT16, psmC1 = PSMT16SZ; X; } break; \
			} \
			break; \
		case PSMCT16S: \
			switch(psm2) { \
				case PSMCT16: 		{ const int psmC = PSMCT16S, psmC1 = PSMCT16; X; }  break; \
				case PSMCT16S:  	{ const int psmC = PSMCT16S, psmC1 = PSMCT16S; X; } break; \
				case PSMT16Z: 	      	{ const int psmC = PSMCT16S, psmC1 = PSMT16Z; X; }  break; \
				case PSMT16SZ: 	      	{ const int psmC = PSMCT16S, psmC1 = PSMT16SZ; X; } break; \
			} \
			break; \
		case PSMT16Z: \
			switch(psm2) { \
				case PSMCT16: 		{ const int psmC = PSMT16Z, psmC1 = PSMCT16; X; }  break; \
				case PSMCT16S:  	{ const int psmC = PSMT16Z, psmC1 = PSMCT16S; X; } break; \
				case PSMT16Z: 	      	{ const int psmC = PSMT16Z, psmC1 = PSMT16Z; X; }  break; \
				case PSMT16SZ: 	      	{ const int psmC = PSMT16Z, psmC1 = PSMT16SZ; X; } break; \
			} \
			break; \
		case PSMT16SZ: \
			switch(psm2) { \
				case PSMCT16: 		{ const int psmC = PSMT16SZ, psmC1 = PSMCT16; X; }  break; \
				case PSMCT16S:  	{ const int psmC = PSMT16SZ, psmC1 = PSMCT16S; X; } break; \
				case PSMT16Z: 	      	{ const int psmC = PSMT16SZ, psmC1 = PSMT16Z; X; }  break; \
				case PSMT16SZ: 	      	{ const int psmC = PSMT16SZ, psmC1 = PSMT16SZ; X; } break; \
			} \
			break; \
		case PSMT8: \
			if( psm2 == PSMT8 ) 		{ const int psmC = PSMT8, psmC1 = PSMT8; X; }   \
			else		  		{ const int psmC = PSMT8, psmC1 = PSMT8H; X; }  \
			break; \
		case PSMT8H: \
			if( psm2 == PSMT8H ) 		{ const int psmC = PSMT8H, psmC1 = PSMT8; X; }  \
			else		  		{ const int psmC = PSMT8H, psmC1 = PSMT8H; X; } \
			break; \
		case PSMT4: \
			switch(psm2) { \
				case PSMT4: 		{ const int psmC = PSMT4, psmC1 = PSMT4; X; }  break; \
				case PSMT4HL:  		{ const int psmC = PSMT4, psmC1 = PSMT4HL; X; } break; \
				case PSMT4HH: 	      	{ const int psmC = PSMT4, psmC1 = PSMT4HH; X; }  break; \
			} \
			break; \
		case PSMT4HL: \
			switch(psm2) { \
				case PSMT4: 		{ const int psmC = PSMT4HL, psmC1 = PSMT4; X; }  break; \
				case PSMT4HL:  		{ const int psmC = PSMT4HL, psmC1 = PSMT4HL; X; } break; \
				case PSMT4HH: 	      	{ const int psmC = PSMT4HL, psmC1 = PSMT4HH; X; }  break; \
			} \
			break; \
		case PSMT4HH: \
  			switch(psm2) { \
				case PSMT4: 		{ const int psmC = PSMT4HH, psmC1 = PSMT4; X; }  break; \
				case PSMT4HL:  		{ const int psmC = PSMT4HH, psmC1 = PSMT4HL; X; } break; \
				case PSMT4HH: 	      	{ const int psmC = PSMT4HH, psmC1 = PSMT4HH; X; }  break; \
			} \
			break; \
		} \
}

template <int psm> 
static __forceinline void setPsmtConstantsX(u8& A, u8& B, u8& C, u8& D, u8& E, u8& F, u32& G, u8& H)  { 
	switch (psm) { 
		case PSMCT32: 
		case PSMT32Z: 
			A = 5; B = 6; C = 0; D = 31; E = 63; F = 0; H = 1; G = 0xffffffff; 
			break; 

		case PSMCT24: 
		case PSMT24Z: 
			A = 5; B = 6; C = 0; D = 31; E = 63; F = 0; H = 1; G = 0xffffff; 
			break; 

		case PSMT8H: 
			A = 5; B = 6; C = 0; D = 31; E = 63; F = 24; H = 4; G = 0xff;
			break;

		case PSMT4HH: 
			A = 5; B = 6; C = 0; D = 31; E = 63; F = 28; H = 8; G = 0xf;
			break;
	       	
		case PSMT4HL: 
			A = 5; B = 6; C = 0; D = 31; E = 63; F = 24; H = 8; G = 0xf;
			break; 

		case PSMCT16: 
		case PSMT16Z: 
		case PSMCT16S: 
		case PSMT16SZ: 
			A = 6; B = 6; C = 1; D = 63; E = 63; F = 0; H = 2; G = 0xffff;
			break; 

		case PSMT8: 
			A = 6; B = 7; C = 2; D = 63; E = 127; F = 0; H = 4; G = 0xff;
			break; 

		case PSMT4: 
			A = 7; B = 7; C = 3; D = 127; E = 127; F = 0; H = 8; G = 0xf; 
			break; 
	} 
}

// PSM is u6 value, so we MUST guarantee, that we don't crush on incorrect psm.
#define MAX_PSM 64
#define TABLE_WIDTH 8

extern u32** g_pageTable[MAX_PSM];
extern u32** g_blockTable[MAX_PSM];
extern u32** g_columnTable[MAX_PSM];
extern u32 ZZ_DT[MAX_PSM][TABLE_WIDTH];
extern u32** g_pageTableNew[MAX_PSM];

#define NEWCODE
//#define OLDCODE
//#define ZEROFROG_CODE

#ifdef NEWCODE
// ------------------------------------------ get Address functions ------------------------------------
// Yes, only 1 function to all cases of life! 
// Warning! We switch bp and bw for usage of default value, so be warned! I
// t not C, it's C++, so not it.
template <int psm>
static __forceinline u32 getPixelAddress(int x, int y, u32 bw, u32 bp = 0) {
	u32 basepage;
	u32 word;

	u8 A = 0, B = 0, C = 0, D = 0, E = 0, F = 0;  u32 G = 0; u8 H= 0;
	setPsmtConstantsX<psm>(A, B, C, D, E, F, G, H) ; 
	basepage = ((y>>A) * (bw>>B)) + (x>>B); 
	word = ((bp * 64 + basepage * 2048) << C) + g_pageTable[psm][y&D][x&E];				

	return word;	
}

// It's Zerofrog's funtion. I need to eliminate them all! All access should be 32-bit aligned.
static __forceinline u32 getPixelAddress(int psm, int x, int y, u32 bw, u32 bp = 0) {
	PSM_SWITCHCASE(return getPixelAddress<psmC>(x, y, bw, bp) ;)
	return 0;
}

// This is compatibility code, for refenrece,
#define Def_getPixelAddress(psmT, psmX) \
	static __forceinline u32 getPixelAddress##psmT(int x, int y, u32 bp, u32 bw) { \
		return getPixelAddress<psmX>(x, y, bw, bp); } \
	static __forceinline u32 getPixelAddress##psmT##_0(int x, int y, u32 bw) { \
		return getPixelAddress<psmX>(x, y, bw); } \

Def_getPixelAddress(32, PSMCT32)
Def_getPixelAddress(16, PSMCT16)
Def_getPixelAddress(16S, PSMCT16S)
Def_getPixelAddress(8, PSMT8)
Def_getPixelAddress(4, PSMT4)
Def_getPixelAddress(32Z, PSMT32Z)
Def_getPixelAddress(16Z, PSMT16Z)
Def_getPixelAddress(16SZ, PSMT16SZ)

#define getPixelAddress24 getPixelAddress32
#define getPixelAddress24_0 getPixelAddress32_0
#define getPixelAddress8H getPixelAddress32
#define getPixelAddress8H_0 getPixelAddress32_0
#define getPixelAddress4HL getPixelAddress32
#define getPixelAddress4HL_0 getPixelAddress32_0
#define getPixelAddress4HH getPixelAddress32
#define getPixelAddress4HH_0 getPixelAddress32_0
#define getPixelAddress24Z getPixelAddress32Z
#define getPixelAddress24Z_0 getPixelAddress32Z_0

// Check FFX-1 (very begining) for PSMT8
// Check Tekken menu for PSMT4
// ZZ_DT[7] is needed only for PSMT8H, PSMT4HL and PSMT4HH -- at this case word contain data not from a begining.

// This function return shift from 32-bit aligned address and shift -- number of byte in u32 order.
// so if ((u32*)mem + getPixelAddress_Aligned32) is exact location of u32, where our pixel data stored. 
// Just for remember:
// PMSCT32, 24, 32Z, 24Z, 8HH, 4HL and 4HH have ZZ_DT[psm] == 3, so shift is always 0.
// PSMCT16, 16S, 16SZ, 16Z have 		ZZ_DT[psm] == 2, so shift is 0 or 16.
// PSMT8					ZZ_DT[psm] == 1,    shift is 0, 8, 16, 24
// PSMT4					ZZ_DT[psm] == 0,    shift is 0, 4, 8, 12, 16, 20, 24, 28.

// It allow us to made a fast access to pixels in the same basepage: if x % N == 0 (N = 1, 2, 4, 8, .. 64)
// than we could guarantee that all pixels form x to x + N - 1 are in the same basepage.
template <int psm>
static __forceinline u32* getPixelBasepage(const void* pmem, int x, int y, u32 bw, u32 bp = 0) {
	u32 basepage;
	u8 A = 0, B = 0, C = 0 , D = 0, E = 0, F = 0; u32 G = 0; u8 H = 0;	
	setPsmtConstantsX<psm> (A, B, C, D, E, F, G, H);
	basepage = ((y>>A) * (bw>>B)) + (x>>B);
	return ((u32*)pmem + (bp * 64 + basepage * 2048));
}

// And this is offset for this pixels.
template <int psm>
static __forceinline u32* getPixelOffset(u32& mask, u32& shift, const void* pmem, int x, int y) {
	u32 word;

	u8 A = 0, B = 0, C = 0 , D = 0, E = 0, F = 0; u32 G = 0; u8 H = 0;
	setPsmtConstantsX<psm> (A, B, C, D, E, F, G, H);

	word = (g_pageTable[psm][y&D][x&E] << (3 - C));
	shift = ((word & 0x7) << 2) + F;
	mask &= G << shift; 

	return ((u32*)pmem + ((word & ~0x7) >> 3));
}


template <int psm>
static __forceinline u32* getPixelAddress_A32(u32& mask, u32& shift, const void* pmem, int x, int y, u32 bw, u32 bp = 0) {
	return getPixelOffset<psm>(mask, shift, getPixelBasepage<psm>(pmem, x, y, bw, bp), x, y);

}

template <int psm>
static __forceinline u32* getPixelBaseAddress_A32(const void* pmem, int x, int y, u32 bw, u32 bp = 0) {
	u32 word;
	
	u8 A = 0, B = 0, C = 0 , D = 0, E = 0, F = 0; u32 G = 0; u8 H = 0;
	setPsmtConstantsX<psm> (A, B, C, D, E, F, G, H);

	word = (g_pageTable[psm][y&D][x&E] << (3 - C));
	return ((u32*)getPixelBasepage<psm>(pmem, x, y, bw, bp) + ((word & ~0x7) >> 3));
}

// Wrapper for cases, where psm is not constant, should be avoided inside cycles
static __forceinline u32* getPixelAddress_A32(u32& mask, u32& shift, int psm, const void* pmem, int x, int y, u32 bw, u32 bp = 0) {
	PSM_SWITCHCASE( return getPixelAddress_A32<psmC>(mask, shift, pmem, x, y, bw, bp) );
}

static __forceinline u32* getClutAddress(u8* pmem, const tex0Info& tex0) {
	if (PSMT_ISHALF(tex0.cpsm))
		return (u32*)(pmem + 64 * (tex0.csa & 15) + (tex0.csa >= 16 ? 2 : 0) );
	else
		return (u32*)(pmem + 64 * (tex0.csa & 15));
}

//--------------------------------------------- Write Pixel -----------------------------------------------------------
// Set proper mask for transfering multiple bytes per word.
template <int psm>
inline u32 HandleWritemask(u32 Writemask) {
	u8 G = PSM_BITS_PER_PIXEL<psm>();
	u32 dmask = Writemask & ((1 << G) - 1);				// drop all bits in writemask, that could not be used
	u32 mask;

	switch (psm) {
		case PSMT8H:						// modes with non-zero start bit should be handled differently
			return 0xff000000;
		case PSMT4HL:
			return 0x0f000000;
		case PSMT4HH:
			return 0xf0000000;
		default:
			mask = dmask;					// 32 targets and lower

			if (G < 24) {					
				mask |= dmask << G;			// 16 targets and lower
			if (G < 16) {
				mask |= dmask << (2 * G);		// 8 targets and lower
				mask |= dmask << (3 * G);
			if (G < 8) {
				mask |= dmask << (4 * G);		// 4 targets
				mask |= dmask << (5 * G);
				mask |= dmask << (6 * G);
				mask |= dmask << (7 * G);
			}}}
			return mask;
	}
}

//push pixel data at position x,y, according psm storage format. pixel do not need to be properly masked, wrong bit's would not be used
//mask should be made according PSM.
template <int psm>
static __forceinline void writePixel(void* pmem, int x, int y, u32 pixel, u32 bw, u32 bp = 0, u32 mask = 0xffffffff) {
	u32 shift;
	u32* p = getPixelAddress_A32<psm>(mask, shift, pmem, x, y, bw, bp);

	MaskedOR (p, pixel << shift, mask);
}

static __forceinline void writePixel(int psm, void* pmem, int x, int y, u32 pixel, u32 bw, u32 bp = 0, u32 mask = 0xffffffff) {
	PSM_SWITCHCASE(writePixel<psmC>(pmem, x, y, pixel, bw, bp, mask)); 
}

// Put pixel data from memory. Pixel is p, memory start from pixel, and we should count pmove words and shift resulting word to shift 
// 24 targets could be outside of 32-bit borders.
template <int psm>
static __forceinline void pushPixelMem(u32* p, u32* pixel, int pmove, int shift, u32 mask = 0xffffffff) {
	if (psm != PSMCT24 || psm != PSMT24Z) {
		if (shift > 0)
			MaskedOR (p, (*(pixel + pmove)) << (shift), mask);
		else
			MaskedOR (p, (*(pixel + pmove)) >> (-shift), mask);
	}
	else {									// for 24 and 24Z psm data could be not-aligned by 32. Merde!
		u64 pixel64 = (*(u64*)(pixel + pmove) ) >> (-shift);		// we read more data, but for 24 targets shift always negative and resulting data is u32
		MaskedOR(p, (u32)pixel64, mask);				// drop upper part, we don't need it. all data is stored in lower part of u64 after shift

//		MaskedOR(p, (u32)((u8*)pixel + count * 3), mask);
	}
}

// use it if pixel already shifted by needed number of bytes. 
// offseted mean that we should skip basepage calculation, pmem is link to basepage'ed memory. Just a little quicker.
template <int psm, int offseted>
static __forceinline void writePixelMem(const void* pmem, int x, int y, u32* pixel, int count, u32 bw, u32 bp = 0, u32 mask = 0xffffffff) {
	u32 shift;
	u32* p;

	if (offseted)	
		p = getPixelOffset<psm>(mask, shift, pmem, x, y);
	else
		p = getPixelAddress_A32<psm>(mask, shift, pmem, x, y, bw, bp);

	int A = PSM_BITS_PER_PIXEL<psm>();

	int pmove = (count * A) >> 5;
	int pshift = (count * A) & 31;			// we assume, that if shift outside word, than user want next pixel data

	pushPixelMem<psm>(p, pixel, pmove, (int)shift - pshift, mask);
}	


// This function push several pixels. Note, that for 32, 24, 8HH, 4HL, 4HH it's simply write (and pixel should not be properly masked), 16 do push 2 pixels (and x should be even).
// 8 push 4 pixels: 0,0; 0,1; 1,0 and 1,1. 4 push 8: 0,0; 0,1; 1,0; 1,1; 2,0, 2,1; 3,0; 3,1.
template <int psm>
static __forceinline void writePixelWord(const void* pmem, int x, int y, u32 pixel, u32 bw, u32 bp = 0, u32 mask = 0xffffffff) {
	u32 maskA = mask, shift;
	u32* p = getPixelAddress_A32<psm>(maskA, shift, pmem, x, y, bw, bp);

/*	if (PSM_NON_FULL_WORD<psm>())			
		maskA = maskA & mask;
	else
		maskA = mask;*/
	
	MaskedOR (p, pixel, mask);
}	

// ------------------------------------- Read Pixel ---------------------------------------
template <int psm>
static __forceinline u32 readPixel(const void* pmem, int x, int y, u32 bw, u32 bp = 0, u32 mask = 0xffffffff) {
	u32 shift;
	u32* p = getPixelAddress_A32<psm>(mask, shift, pmem, x, y, bw, bp);

	return ((*p & mask) >> shift);
}

static __forceinline u32 readPixel(int psm, const void* pmem, int x, int y, u32 bw, u32 bp = 0, u32 mask = 0xffffffff) {
	PSM_SWITCHCASE(return readPixel<psmC>(pmem, x, y, bw, bp, mask););
	return 0;
}	

template <int psm>
static __forceinline u32 readPixelWord(const void* pmem, int x, int y, u32 bw, u32 bp = 0, u32 mask = 0xffffffff) {
	u32 maskA = 0xffffffff, shift;
	if (PSM_NON_FULL_WORD<psm>())
		return *getPixelAddress_A32<psm>(mask, shift, pmem, x, y, bw, bp) & mask;
	else
		return *getPixelAddress_A32<psm>(maskA, shift, pmem, x, y, bw, bp) & mask;
}

template <int psm>
static __forceinline void fillMemoryFromPixels(u32* dst, const void* pmem, int& count, int x, int y, u32 bw, u32 bp = 0, u32 mask = 0xffffffff) {
	u32 pixel;

	u8 I  = PSM_BITS_PER_PIXEL<psm>(); 
	int K = count / PSM_PIXELS_STORED_PER_WORD<psm>();				// offset for pmem, count for 32, count / 2 for 16, etc.

		pixel = readPixel<psm>(pmem, x, y, bw, bp, mask);			// I prefer not to use for here. It's slow
	if (I < 32) {
		pixel += readPixel<psm>(pmem, x + 1, y, bw, bp, mask) << I;
	if (I < 16) {									// 8 and 4 targets
		pixel += readPixel<psm>(pmem, x + 2, y, bw, bp, mask) << (2 * I);
		pixel += readPixel<psm>(pmem, x + 3, y, bw, bp, mask) << (3 * I);
	if (I < 8) {									// This is for 4, 4HH and 4HL
		pixel += readPixel<psm>(pmem, x + 4, y, bw, bp, mask) << (4 * I);
		pixel += readPixel<psm>(pmem, x + 5, y, bw, bp, mask) << (5 * I);
		pixel += readPixel<psm>(pmem, x + 6, y, bw, bp, mask) << (6 * I);
		pixel += readPixel<psm>(pmem, x + 7, y, bw, bp, mask) << (7 * I);
	}}}
	
	if  (I != 24) {										
		*(dst + K) = pixel;										
	}
	else {										// 24. should have special care.
//		ERROR_LOG("special care %d\n", count);
		MaskedOR((u32*)((u8*)dst + 3 * count), pixel, 0xffffff);
	}
	count +=  PSM_PIXELS_STORED_PER_WORD<psm>();
}


// Fill count pixels form continues memory region, starting from pmem, First pixel to read have number shift in this region.
// Read no more than count pixels. We could assert, that all this pixels would be place in the same basepage 
// Shift is automaticaly increased by count (or decreased if count < 0)
template <int psm, bool offseted, int count>
static __forceinline void writePixelsFromMemory(void* dst, const void* pmem, int& shift, int x, int y, u32 bw, u32 bp = 0, u32 mask = 0xffffffff) {
	const void* base;
	if (offseted)
		base = getPixelBasepage<psm>(dst, x, y, bw, bp);
	else
		base = (const void*)dst;

	shift += count;
	writePixelMem<psm, offseted>(base, x, y, (u32*)pmem, shift - count, bw, bp, mask);				  		// I prefer not to use for here. It's slow
	if (count < 2) return;
	writePixelMem<psm, offseted>(base, x + 1, y, (u32*)pmem, shift - count + 1, bw, bp, mask);				  	
	if (count < 3) return; 	
	writePixelMem<psm, offseted>(base, x + 2, y, (u32*)pmem, shift - count + 2, bw, bp, mask);				  
	if (count < 4) return;
	writePixelMem<psm, offseted>(base, x + 3, y, (u32*)pmem, shift - count + 3, bw, bp, mask);				  
	if (count < 5) return;
	writePixelMem<psm, offseted>(base, x + 4, y, (u32*)pmem, shift - count + 4, bw, bp, mask);
	if (count < 6) return;
	writePixelMem<psm, offseted>(base, x + 5, y, (u32*)pmem, shift - count + 5, bw, bp, mask);
	if (count < 7) return;
	writePixelMem<psm, offseted>(base, x + 6, y, (u32*)pmem, shift - count + 6, bw, bp, mask);
	if (count < 8) return;
	writePixelMem<psm, offseted>(base, x + 7, y, (u32*)pmem, shift - count + 7, bw, bp, mask);				  	
}

// Use it if we don't know that starting pixel is aligned for multiple-pixel write
template <int psm, bool offseted>
static __forceinline void writeUnalignedPixelsFromMemory(void* dst, int div, const void* pmem, int& shift, int x, int y, u32 bw, u32 bp = 0, u32 mask = 0xffffffff) {
	switch (div){
		case 0: return; 											// Pixels are aligned, so we could move on
		case 1: writePixelsFromMemory<psm, offseted, 1>(dst, pmem, shift, x, y, bw, bp, mask);
			return;
		case 2: writePixelsFromMemory<psm, offseted, 2>(dst, pmem, shift, x, y, bw, bp, mask);
			return;
		case 3: writePixelsFromMemory<psm, offseted, 3>(dst, pmem, shift, x, y, bw, bp, mask);
			return;
		case 4: writePixelsFromMemory<psm, offseted, 4>(dst, pmem, shift, x, y, bw, bp, mask);
			return;
		case 5: writePixelsFromMemory<psm, offseted, 5>(dst, pmem, shift, x, y, bw, bp, mask);
			return;
		case 6: writePixelsFromMemory<psm, offseted, 6>(dst, pmem, shift, x, y, bw, bp, mask);
			return;
		case 7: writePixelsFromMemory<psm, offseted, 7>(dst, pmem, shift, x, y, bw, bp, mask);
			return;
	}
}

// This little swizzle function used to convert data form memory. z is first byte in destination block, and y is number of word, in which we look look for data.
// s is shift by number of pixels, that should be used in masking
template <int psm, int y, int z>
static __forceinline u32 BitmaskinPSM(u32* pmem, u8 x) {

	u8 H = PSM_BITCOUNT<psm>();
	u8 I = PSM_BITS_PER_PIXEL<psm>() ;							// length of bitmask in bits. 


	if (PSM_BITMODE<psm>() != 1) {								// PSMCT24 and 24Z should be handle separated, as it could pass 32-bit storage.	
		u8 k = (x & (H - 1)) * I;							// shift of PC data -- in PC we use pixels from constant position: x / H word and k is shift: x = ( x % H ) * H + k / I
												// in PS2 we use all bit position from 0 by I pixels.
	 
		u32 J = ((1 << I) - 1) << k;							// bitmask (of length ) & mask, moved by position k

		if (z > k)	
			return ((*(pmem + x/H + y)) & J) << (z - k);				// we use PX data from *mem + ynd properly shift
		else										// This formula loo little swizzled. 
			return ((*(pmem + x/H + y)) & J) >> (k - z);
	}
	else {											// only 24 targets
		u8* mem = ((u8*)pmem + (x * 3) + 4 * y); 					// Our pixel's is disaligned on 32-bit. So just use u8*.
		return *(u32*)mem;								// Mask would be handled later
	}
}

// We use this function to limit number of memory R/W. This function fill all pixels for data with coordindates x, y. inside block data.
// Only rule is x, y should be < 8 (it automatically fill all needed pixels, that lie in blockdata, but have coords more than 8).
template <int psm>
static __forceinline void fillPixelsFromMemory(u32* dst, u32* pmem, int x, int y, int pitch, u32 bw, u32 bp = 0, u32 mask = 0xffffffff) {
	u32 pixel = 0;
	const u8 H = PSM_PIXELS_PER_WORD<psm>();

	if (PSM_PIXEL_SHIFT<psm>() == 0)								// We could not use calculated constants as templated parameters.
		pixel = BitmaskinPSM<psm, 0, 0>(pmem, x);						// First pixel x,y is the common part of all psmt path's
	else {
		if (PSM_PIXEL_SHIFT<psm>() == 24) 							// 8H and 4HL have 1 pixel, but shifted to 24 bits. 4HH -- 28 bits.	
			pixel = BitmaskinPSM<psm, 0, 24>(pmem, x);					
		else
			pixel = BitmaskinPSM<psm, 0, 28>(pmem, x);
	}
	if (H > 1) {
		const u8 G = psm & 0x7;									// Bitmode, we use it for better chance of switch optimization
		int div = ( x < 4 ) ? 4 : -4;								// secondary row have shift by +4 or -4 pixels

		switch (G) {
			case 2:
				pixel |= BitmaskinPSM<psm, 4, 16>(pmem, x);
				break;
			case 3:	
				pixel |= BitmaskinPSM<psm, 2, 16>(pmem, x);			
				pixel |= BitmaskinPSM<psm, 0, 8>(pmem + 2 * pitch, x + div);
				pixel |= BitmaskinPSM<psm, 2, 24>(pmem + 2 * pitch, x + div);
				break;	
			case 4:
				pixel |= BitmaskinPSM<psm, 1, 8>(pmem, x);			
				pixel |= BitmaskinPSM<psm, 2, 16>(pmem, x);		
				pixel |= BitmaskinPSM<psm, 3, 24>(pmem, x);			

				pixel |= BitmaskinPSM<psm, 0, 4>(pmem + 2 * pitch, x + div);
				pixel |= BitmaskinPSM<psm, 1, 12>(pmem + 2 * pitch, x + div);			
				pixel |= BitmaskinPSM<psm, 2, 20>(pmem + 2 * pitch, x + div);			
				pixel |= BitmaskinPSM<psm, 3, 28>(pmem + 2 * pitch, x + div);			

				break;				
		}
	}
	writePixelWord<psm>(dst, x, y, pixel, bw, bp, HandleWritemask<psm>(mask));				// use it for 32, 24, 8H, 4HL and 4HH
}

template <int psm>
void writeWordPixel(u32* pmem, u32 pixel, u32 mask) {
	if (psm == PSMT4HH || psm == PSMT8H || psm == PSMT4HL || psm == PSMCT24 || psm == PSMT24Z)
		MaskedOR(pmem, pixel, mask);
	else	
		*pmem = pixel;
}

// Get pixel from src and put in in src. We assume, that psm of both buffers are the same and (sx-dx) & E == (sy - dy) & D == 0;
// Also in this case we could transfer the whole word
template <int psm>
void transferPixelFast(void* dst, void* src, int dx, int dy, int sx, int sy, u32 dbw, u32 sbw ) {
	u32 Dbasepage, Sbasepage;
	u32 word, mask = 0xffffffff;

	u8 A = 0, B = 0, C = 0 , D = 0, E = 0, F = 0; u32 G = 0; u8 H = 0;
	setPsmtConstantsX<psm> (A, B, C, D, E, F, G, H);
	assert ( (sx-dx) & E == (sy - dy) & D; (sy - dy) & D == 0; );

	Dbasepage = ((dy>>A) * (dbw>>B)) + (dx>>B);
	Sbasepage = ((sy>>A) * (sbw>>B)) + (sx>>B);

	word = (g_pageTable[psm][sy&D][sx&E] >> C);

	u32* dstp = (u32*)dst + Dbasepage * 2048 + word;
	u32* srcp = (u32*)src + Sbasepage * 2048 + word;

	writeWordPixel<psm>(dstp, *srcp, G << F);
}

// if we could not guarantee, that buffer suize shared same page Table address
template <int psm>
void transferPixel(void* dst, void* src, int dx, int dy, int sx, int sy, u32 dbw, u32 sbw ) {
	u32 mask = 0xffffffff, shift;
	u32* dstp = getPixelAddress_A32<psm>(mask, shift, dst, dx, dy, dbw);
	u32* srcp = getPixelAddress_A32<psm>(mask, shift, src, sx, sy, sbw);
	writeWordPixel<psm>(dstp, *srcp, mask);								// write whole word
}

#define Def_getReadWrite(psmT, psmX) \
	static __forceinline void writePixel##psmT(void* pmem, int x, int y, u32 pixel, u32 bp, u32 bw) { \
		writePixel<psmX>(pmem, x, y, pixel, bw, bp); } \
	static __forceinline u32 readPixel##psmT(const void* pmem, int x, int y, u32 bp, u32 bw) { \
		return readPixel<psmX>(pmem, x, y, bw, bp); } \
	static __forceinline void writePixel##psmT##_0(void* pmem, int x, int y, u32 pixel, u32 bw) { \
		writePixel<psmX>(pmem, x, y, pixel, bw); } \
	static __forceinline u32 readPixel##psmT##_0(const void* pmem, int x, int y, u32 bw) { \
		return readPixel<psmX>(pmem, x, y, bw); }  

Def_getReadWrite(32, PSMCT32); 
Def_getReadWrite(24, PSMCT24); 
Def_getReadWrite(16, PSMCT16); 
Def_getReadWrite(16S, PSMCT16); 
Def_getReadWrite(8, PSMT8); 
Def_getReadWrite(8H, PSMT8H); 
Def_getReadWrite(4, PSMT4); 
Def_getReadWrite(4HH, PSMT4HH); 
Def_getReadWrite(4HL, PSMT4HL); 
Def_getReadWrite(32Z, PSMCT32); 
Def_getReadWrite(24Z, PSMCT24);  
Def_getReadWrite(16Z, PSMCT16); 
Def_getReadWrite(16SZ, PSMCT16);

#endif // NEWCODE

//---------------------------------------------------------------------------------------------------------------------------------------------
#ifdef ZEROFROG_CODE

static __forceinline u32 getPixelAddress32(int x, int y, u32 bp, u32 bw) {
	u32 basepage = ((y>>5) * (bw>>6)) + (x>>6);
	u32 word = bp * 64 + basepage * 2048 + g_pageTable32[y&31][x&63];
	//assert (word < 0x100000);
	//word = min(word, 0xfffff);
	return word;
}

static __forceinline u32 getPixelAddress32_0(int x, int y, u32 bw) {
	u32 basepage = ((y>>5) * (bw>>6)) + (x>>6);
	u32 word = basepage * 2048 + g_pageTable32[y&31][x&63];
	//assert (word < 0x100000);
	//word = min(word, 0xfffff);
	return word;
}

#define getPixelAddress24 getPixelAddress32
#define getPixelAddress24_0 getPixelAddress32_0
#define getPixelAddress8H getPixelAddress32
#define getPixelAddress8H_0 getPixelAddress32_0
#define getPixelAddress4HL getPixelAddress32
#define getPixelAddress4HL_0 getPixelAddress32_0
#define getPixelAddress4HH getPixelAddress32
#define getPixelAddress4HH_0 getPixelAddress32_0

static __forceinline u32 getPixelAddress16(int x, int y, u32 bp, u32 bw) {
	u32 basepage = ((y>>6) * (bw>>6)) + (x>>6);
	u32 word = bp * 128 + basepage * 4096 + g_pageTable16[y&63][x&63];
	//assert (word < 0x200000);
	//word = min(word, 0x1fffff);
	return word;
}

static __forceinline u32 getPixelAddress16_0(int x, int y, u32 bw) {
	u32 basepage = ((y>>6) * (bw>>6)) + (x>>6);
	u32 word = basepage * 4096 + g_pageTable16[y&63][x&63];
	//assert (word < 0x200000);
	//word = min(word, 0x1fffff);
	return word;
}

static __forceinline u32 getPixelAddress16S(int x, int y, u32 bp, u32 bw) {
	u32 basepage = ((y>>6) * (bw>>6)) + (x>>6);
	u32 word = bp * 128 + basepage * 4096 + g_pageTable16S[y&63][x&63];
	//assert (word < 0x200000);
	//word = min(word, 0x1fffff);
	return word;
}

static __forceinline u32 getPixelAddress16S_0(int x, int y, u32 bw) {
	u32 basepage = ((y>>6) * (bw>>6)) + (x>>6);
	u32 word = basepage * 4096 + g_pageTable16S[y&63][x&63];
	//assert (word < 0x200000);
	//word = min(word, 0x1fffff);
	return word;
}

static __forceinline u32 getPixelAddress8(int x, int y, u32 bp, u32 bw) {
	u32 basepage = ((y>>6) * ((bw+127)>>7)) + (x>>7);
	u32 word = bp * 256 + basepage * 8192 + g_pageTable8[y&63][x&127];
	//assert (word < 0x400000);
	//word = min(word, 0x3fffff);
	return word;
}

static __forceinline u32 getPixelAddress8_0(int x, int y, u32 bw) {
	u32 basepage = ((y>>6) * ((bw+127)>>7)) + (x>>7);
	u32 word = basepage * 8192 + g_pageTable8[y&63][x&127];
	//assert (word < 0x400000);
	//word = min(word, 0x3fffff);
	return word;
}

static __forceinline u32 getPixelAddress4(int x, int y, u32 bp, u32 bw) {
	u32 basepage = ((y>>7) * ((bw+127)>>7)) + (x>>7);
	u32 word = bp * 512 + basepage * 16384 + g_pageTable4[y&127][x&127];
	//assert (word < 0x800000);
	//word = min(word, 0x7fffff);
	return word;
}

static __forceinline u32 getPixelAddress4_0(int x, int y, u32 bw) {
	u32 basepage = ((y>>7) * ((bw+127)>>7)) + (x>>7);
	u32 word = basepage * 16384 + g_pageTable4[y&127][x&127];
	//assert (word < 0x800000);
	//word = min(word, 0x7fffff);
	return word;
}

static __forceinline u32 getPixelAddress32Z(int x, int y, u32 bp, u32 bw) {
	u32 basepage = ((y>>5) * (bw>>6)) + (x>>6);
	u32 word = bp * 64 + basepage * 2048 + g_pageTable32Z[y&31][x&63];
	//assert (word < 0x100000);
	//word = min(word, 0xfffff);
	return word;
}

static __forceinline u32 getPixelAddress32Z_0(int x, int y, u32 bw) {
	u32 basepage = ((y>>5) * (bw>>6)) + (x>>6);
	u32 word = basepage * 2048 + g_pageTable32Z[y&31][x&63];
	//assert (word < 0x100000);
	//word = min(word, 0xfffff);
	return word;
}

#define getPixelAddress24Z getPixelAddress32Z
#define getPixelAddress24Z_0 getPixelAddress32Z_0

static __forceinline u32 getPixelAddress16Z(int x, int y, u32 bp, u32 bw) {
	u32 basepage = ((y>>6) * (bw>>6)) + (x>>6);
	u32 word = bp * 128 + basepage * 4096 + g_pageTable16Z[y&63][x&63];
	//assert (word < 0x200000);
	//word = min(word, 0x1fffff);
	return word;
}

static __forceinline u32 getPixelAddress16Z_0(int x, int y, u32 bw) {
	u32 basepage = ((y>>6) * (bw>>6)) + (x>>6);
	u32 word = basepage * 4096 + g_pageTable16Z[y&63][x&63];
	//assert (word < 0x200000);
	//word = min(word, 0x1fffff);
	return word;
}

static __forceinline u32 getPixelAddress16SZ(int x, int y, u32 bp, u32 bw) {
	u32 basepage = ((y>>6) * (bw>>6)) + (x>>6);
	u32 word = bp * 128 + basepage * 4096 + g_pageTable16SZ[y&63][x&63];
	//assert (word < 0x200000);
	//word = min(word, 0x1fffff);
	return word;
}

static __forceinline u32 getPixelAddress16SZ_0(int x, int y, u32 bw) {
	u32 basepage = ((y>>6) * (bw>>6)) + (x>>6);
	u32 word = basepage * 4096 + g_pageTable16SZ[y&63][x&63];
	//assert (word < 0x200000);
	//word = min(word, 0x1fffff);
	return word;
}

static __forceinline void writePixel32(void* pmem, int x, int y, u32 pixel, u32 bp, u32 bw) {
	((u32*)pmem)[getPixelAddress32(x, y, bp, bw)] = pixel;
}

static __forceinline void writePixel24(void* pmem, int x, int y, u32 pixel, u32 bp, u32 bw) {
	u8 *buf = (u8*)&((u32*)pmem)[getPixelAddress32(x, y, bp, bw)];
	u8 *pix = (u8*)&pixel;
	buf[0] = pix[0]; buf[1] = pix[1]; buf[2] = pix[2];
}

static __forceinline void writePixel16(void* pmem, int x, int y, u32 pixel, u32 bp, u32 bw) {
	((u16*)pmem)[getPixelAddress16(x, y, bp, bw)] = pixel;
}

static __forceinline void writePixel16S(void* pmem, int x, int y, u32 pixel, u32 bp, u32 bw) {
	((u16*)pmem)[getPixelAddress16S(x, y, bp, bw)] = pixel;
}

static __forceinline void writePixel8(void* pmem, int x, int y, u32 pixel, u32 bp, u32 bw) {
	((u8*)pmem)[getPixelAddress8(x, y, bp, bw)] = pixel;
}

static __forceinline void writePixel8H(void* pmem, int x, int y, u32 pixel, u32 bp, u32 bw) {
	((u8*)pmem)[4*getPixelAddress32(x, y, bp, bw)+3] = pixel;
}

static __forceinline void writePixel4(void* pmem, int x, int y, u32 pixel, u32 bp, u32 bw) {
	u32 addr = getPixelAddress4(x, y, bp, bw);
	u8 pix = ((u8*)pmem)[addr/2];
	if (addr & 0x1) ((u8*)pmem)[addr/2] = (pix & 0x0f) | (pixel << 4);
	else ((u8*)pmem)[addr/2] = (pix & 0xf0) | (pixel);
}

static __forceinline void writePixel4HL(void* pmem, int x, int y, u32 pixel, u32 bp, u32 bw) {
	u8 *p = (u8*)pmem + 4*getPixelAddress4HL(x, y, bp, bw)+3;
	*p = (*p & 0xf0) | pixel;
}

static __forceinline void writePixel4HH(void* pmem, int x, int y, u32 pixel, u32 bp, u32 bw) {
	u8 *p = (u8*)pmem + 4*getPixelAddress4HH(x, y, bp, bw)+3;
	*p = (*p & 0x0f) | (pixel<<4);
}

static __forceinline void writePixel32Z(void* pmem, int x, int y, u32 pixel, u32 bp, u32 bw) {
	((u32*)pmem)[getPixelAddress32Z(x, y, bp, bw)] = pixel;
}

static __forceinline void writePixel24Z(void* pmem, int x, int y, u32 pixel, u32 bp, u32 bw) {
	u8 *buf = (u8*)pmem + 4*getPixelAddress32Z(x, y, bp, bw);
	u8 *pix = (u8*)&pixel;
	buf[0] = pix[0]; buf[1] = pix[1]; buf[2] = pix[2];
}

static __forceinline void writePixel16Z(void* pmem, int x, int y, u32 pixel, u32 bp, u32 bw) {
	((u16*)pmem)[getPixelAddress16Z(x, y, bp, bw)] = pixel;
}

static __forceinline void writePixel16SZ(void* pmem, int x, int y, u32 pixel, u32 bp, u32 bw) {
	((u16*)pmem)[getPixelAddress16SZ(x, y, bp, bw)] = pixel;
}


///////////////

static __forceinline u32  readPixel32(const void* pmem, int x, int y, u32 bp, u32 bw) {
	return ((const u32*)pmem)[getPixelAddress32(x, y, bp, bw)];
}

static __forceinline u32  readPixel24(const void* pmem, int x, int y, u32 bp, u32 bw) {
	return ((const u32*)pmem)[getPixelAddress32(x, y, bp, bw)] & 0xffffff;
}

static __forceinline u32  readPixel16(const void* pmem, int x, int y, u32 bp, u32 bw) {
	return ((const u16*)pmem)[getPixelAddress16(x, y, bp, bw)];
}

static __forceinline u32  readPixel16S(const void* pmem, int x, int y, u32 bp, u32 bw) {
	return ((const u16*)pmem)[getPixelAddress16S(x, y, bp, bw)];
}

static __forceinline u32  readPixel8(const void* pmem, int x, int y, u32 bp, u32 bw) {
	return ((const u8*)pmem)[getPixelAddress8(x, y, bp, bw)];
}

static __forceinline u32  readPixel8H(const void* pmem, int x, int y, u32 bp, u32 bw) {
	return ((const u8*)pmem)[4*getPixelAddress32(x, y, bp, bw) + 3];
}

static __forceinline u32  readPixel4(const void* pmem, int x, int y, u32 bp, u32 bw) {
	u32 addr = getPixelAddress4(x, y, bp, bw);
	u8 pix = ((const u8*)pmem)[addr/2];
	if (addr & 0x1)
		 return pix >> 4;
	else return pix & 0xf;
}

static __forceinline u32  readPixel4HL(const void* pmem, int x, int y, u32 bp, u32 bw) {
	const u8 *p = (const u8*)pmem+4*getPixelAddress4HL(x, y, bp, bw)+3;
	return *p & 0x0f;
}

static __forceinline u32  readPixel4HH(const void* pmem, int x, int y, u32 bp, u32 bw) {
	const u8 *p = (const u8*)pmem+4*getPixelAddress4HH(x, y, bp, bw) + 3;
	return *p >> 4;
}

///////////////

static __forceinline u32  readPixel32Z(const void* pmem, int x, int y, u32 bp, u32 bw) {
	return ((const u32*)pmem)[getPixelAddress32Z(x, y, bp, bw)];
}

static __forceinline u32  readPixel24Z(const void* pmem, int x, int y, u32 bp, u32 bw) {
	return ((const u32*)pmem)[getPixelAddress32Z(x, y, bp, bw)] & 0xffffff;
}

static __forceinline u32  readPixel16Z(const void* pmem, int x, int y, u32 bp, u32 bw) {
	return ((const u16*)pmem)[getPixelAddress16Z(x, y, bp, bw)];
}

static __forceinline u32  readPixel16SZ(const void* pmem, int x, int y, u32 bp, u32 bw) {
	return ((const u16*)pmem)[getPixelAddress16SZ(x, y, bp, bw)];
}

///////////////////////////////
// Functions that take 0 bps //
///////////////////////////////

static __forceinline void writePixel32_0(void* pmem, int x, int y, u32 pixel, u32 bw) {
	((u32*)pmem)[getPixelAddress32_0(x, y, bw)] = pixel;
}

static __forceinline void writePixel24_0(void* pmem, int x, int y, u32 pixel, u32 bw) {
	u8 *buf = (u8*)&((u32*)pmem)[getPixelAddress32_0(x, y, bw)];
	u8 *pix = (u8*)&pixel;
#if defined(_MSC_VER) && defined(__x86_64__)
	memcpy(buf, pix, 3);
#else
	buf[0] = pix[0]; buf[1] = pix[1]; buf[2] = pix[2];
#endif
}

static __forceinline void writePixel16_0(void* pmem, int x, int y, u32 pixel, u32 bw) {
	((u16*)pmem)[getPixelAddress16_0(x, y, bw)] = pixel;
}

static __forceinline void writePixel16S_0(void* pmem, int x, int y, u32 pixel, u32 bw) {
	((u16*)pmem)[getPixelAddress16S_0(x, y, bw)] = pixel;
}

static __forceinline void writePixel8_0(void* pmem, int x, int y, u32 pixel, u32 bw) {
	((u8*)pmem)[getPixelAddress8_0(x, y, bw)] = pixel;
}

static __forceinline void writePixel8H_0(void* pmem, int x, int y, u32 pixel, u32 bw) {
	((u8*)pmem)[4*getPixelAddress32_0(x, y, bw)+3] = pixel;
}

static __forceinline void writePixel4_0(void* pmem, int x, int y, u32 pixel, u32 bw) {
	u32 addr = getPixelAddress4_0(x, y, bw);
	u8 pix = ((u8*)pmem)[addr/2];
	if (addr & 0x1) ((u8*)pmem)[addr/2] = (pix & 0x0f) | (pixel << 4);
	else ((u8*)pmem)[addr/2] = (pix & 0xf0) | (pixel);
}

static __forceinline void writePixel4HL_0(void* pmem, int x, int y, u32 pixel, u32 bw) {
	u8 *p = (u8*)pmem + 4*getPixelAddress4HL_0(x, y, bw)+3;
	*p = (*p & 0xf0) | pixel;
}

static __forceinline void writePixel4HH_0(void* pmem, int x, int y, u32 pixel, u32 bw) {
	u8 *p = (u8*)pmem + 4*getPixelAddress4HH_0(x, y, bw)+3;
	*p = (*p & 0x0f) | (pixel<<4);
}

static __forceinline void writePixel32Z_0(void* pmem, int x, int y, u32 pixel, u32 bw) {
	((u32*)pmem)[getPixelAddress32Z_0(x, y, bw)] = pixel;
}

static __forceinline void writePixel24Z_0(void* pmem, int x, int y, u32 pixel, u32 bw) {
	u8 *buf = (u8*)pmem + 4*getPixelAddress32Z_0(x, y, bw);
	u8 *pix = (u8*)&pixel;
#if defined(_MSC_VER) && defined(__x86_64__)
	memcpy(buf, pix, 3);
#else
	buf[0] = pix[0]; buf[1] = pix[1]; buf[2] = pix[2];
#endif
}

static __forceinline void writePixel16Z_0(void* pmem, int x, int y, u32 pixel, u32 bw) {
	((u16*)pmem)[getPixelAddress16Z_0(x, y, bw)] = pixel;
}

static __forceinline void writePixel16SZ_0(void* pmem, int x, int y, u32 pixel, u32 bw) {
	((u16*)pmem)[getPixelAddress16SZ_0(x, y, bw)] = pixel;
}


///////////////

static __forceinline u32  readPixel32_0(const void* pmem, int x, int y, u32 bw) {
	return ((const u32*)pmem)[getPixelAddress32_0(x, y, bw)];
}

static __forceinline u32  readPixel24_0(const void* pmem, int x, int y, u32 bw) {
	return ((const u32*)pmem)[getPixelAddress32_0(x, y, bw)] & 0xffffff;
}

static __forceinline u32  readPixel16_0(const void* pmem, int x, int y, u32 bw) {
	return ((const u16*)pmem)[getPixelAddress16_0(x, y, bw)];
}

static __forceinline u32  readPixel16S_0(const void* pmem, int x, int y, u32 bw) {
	return ((const u16*)pmem)[getPixelAddress16S_0(x, y, bw)];
}

static __forceinline u32  readPixel8_0(const void* pmem, int x, int y, u32 bw) {
	return ((const u8*)pmem)[getPixelAddress8_0(x, y, bw)];
}

static __forceinline u32  readPixel8H_0(const void* pmem, int x, int y, u32 bw) {
	return ((const u8*)pmem)[4*getPixelAddress32_0(x, y, bw) + 3];
}

static __forceinline u32  readPixel4_0(const void* pmem, int x, int y, u32 bw) {
	u32 addr = getPixelAddress4_0(x, y, bw);
	u8 pix = ((const u8*)pmem)[addr/2];
	if (addr & 0x1)
		 return pix >> 4;
	else return pix & 0xf;
}

static __forceinline u32  readPixel4HL_0(const void* pmem, int x, int y, u32 bw) {
	const u8 *p = (const u8*)pmem+4*getPixelAddress4HL_0(x, y, bw)+3;
	return *p & 0x0f;
}

static __forceinline u32  readPixel4HH_0(const void* pmem, int x, int y, u32 bw) {
	const u8 *p = (const u8*)pmem+4*getPixelAddress4HH_0(x, y, bw) + 3;
	return *p >> 4;
}

///////////////

static __forceinline u32  readPixel32Z_0(const void* pmem, int x, int y, u32 bw) {
	return ((const u32*)pmem)[getPixelAddress32Z_0(x, y, bw)];
}

static __forceinline u32  readPixel24Z_0(const void* pmem, int x, int y, u32 bw) {
	return ((const u32*)pmem)[getPixelAddress32Z_0(x, y, bw)] & 0xffffff;
}

static __forceinline u32  readPixel16Z_0(const void* pmem, int x, int y, u32 bw) {
	return ((const u16*)pmem)[getPixelAddress16Z_0(x, y, bw)];
}

static __forceinline u32  readPixel16SZ_0(const void* pmem, int x, int y, u32 bw) {
	return ((const u16*)pmem)[getPixelAddress16SZ_0(x, y, bw)];
}

static __forceinline u32 getPixelAddress(int psm, int x, int y, u32 bw, u32 bp = 0) {
	switch (psm) {
		case PSMCT32: 
		case PSMCT24:
		case PSMT8H:  
		case PSMT4HH:  
		case PSMT4HL: 
			return getPixelAddress32(x, y, bp, bw);
			break; 
		case PSMT32Z: 
		case PSMT24Z:
			return getPixelAddress32Z(x, y, bp, bw);
			break;  
		case PSMCT16:   
			return getPixelAddress16(x, y, bp, bw);
			break; 
		case PSMCT16S: 
       			return getPixelAddress16S(x, y, bp, bw);
			break; 
		case PSMT16Z:  
      			return getPixelAddress16Z(x, y, bp, bw);
			break; 
		case PSMT16SZ: 
       			return getPixelAddress16SZ(x, y, bp, bw);
			break; 
		case PSMT8:  
    			return getPixelAddress8(x, y, bp, bw);
			break; 
		case PSMT4:  
    			return getPixelAddress4(x, y, bp, bw);
			break; 
	}
}

static __forceinline u32  readPixel(int psm, const void* pmem, int x, int y, u32 bw, u32 bp = 0) {
	switch (psm) {
		case PSMCT32: 
			return readPixel32(pmem, x, y, bp, bw);
			break; 
		case PSMCT24:
			return readPixel24(pmem, x, y, bp, bw);
			break; 
		case PSMT32Z: 
			return readPixel32Z(pmem, x, y, bp, bw);
			break; 
		case PSMT24Z: 
			return readPixel24Z(pmem, x, y, bp, bw);
			break; 
		case PSMT8H: 
			return readPixel8H(pmem, x, y, bp, bw);
			break; 		
		case PSMT4HH: 
			return readPixel4HH(pmem, x, y, bp, bw);
			break; 
		case PSMT4HL: 
			return readPixel4HL(pmem, x, y, bp, bw);
			break; 
		case PSMCT16:   
			return readPixel16(pmem, x, y, bp, bw);
			break; 
		case PSMCT16S: 
       			return readPixel16S(pmem, x, y, bp, bw);
			break; 
		case PSMT16Z:  
      			return readPixel16Z(pmem, x, y, bp, bw);
			break; 
		case PSMT16SZ: 
       			return readPixel16SZ(pmem, x, y, bp, bw);
			break; 
		case PSMT8:  
    			return readPixel8(pmem, x, y, bp, bw);
			break; 
		case PSMT4:  
    			return readPixel4(pmem, x, y, bp, bw);
			break; 
	}
}

static __forceinline void writePixel(int psm, void* pmem, int x, int y, int pixel, u32 bw, u32 bp = 0) {
	switch (psm) {
		case PSMCT32: 
			writePixel32(pmem, x, y, pixel, bp, bw);
			break; 
		case PSMCT24:
			writePixel24(pmem, x, y, pixel, bp, bw);
			break; 
		case PSMT32Z: 
			writePixel32Z(pmem, x, y, pixel, bp, bw);
			break; 
		case PSMT24Z: 
			writePixel24Z(pmem, x, y, pixel, bp, bw);
			break; 
		case PSMT8H: 
			writePixel8H(pmem, x, y, pixel, bp, bw);
			break; 		
		case PSMT4HH: 
			writePixel4HH(pmem, x, y, pixel, bp, bw);
			break; 
		case PSMT4HL: 
			writePixel4HL(pmem, x, y, pixel, bp, bw);
			break; 
		case PSMCT16:   
			writePixel16(pmem, x, y, pixel, bp, bw);
			break; 
		case PSMCT16S: 
       			writePixel16S(pmem, x, y, pixel, bp, bw);
			break; 
		case PSMT16Z:  
      			writePixel16Z(pmem, x, y, pixel, bp, bw);
			break; 
		case PSMT16SZ: 
       			writePixel16SZ(pmem, x, y, pixel, bp, bw);
			break; 
		case PSMT8:  
    			writePixel8(pmem, x, y, pixel, bp, bw);
			break; 
		case PSMT4:  
    			writePixel4(pmem, x, y, pixel, bp, bw);
			break; 
	}
}



#endif // ZEROFROG_CODE
#endif // Zeydlitz's code

#endif /* __ZZOGL_MEM_H__ */
