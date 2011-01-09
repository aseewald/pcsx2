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
#include "targets.h"
#include "x86.h"

#ifndef ZZNORMAL_MEMORY

bool allowed_psm[256] = {false, };			// Sometimes we got strange unknown psm
PSM_value PSM_value_Table[64] = {PSMT_BAD_PSM, };	// for int -> PSM_value

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

		for (; j < gs.imageEnd.x - widthlimit + 1 && nSize >= widthlimit; j += widthlimit, nSize -= widthlimit) { 			
			writePixelsFromMemory<psm, true, widthlimit>(pstart, pbuf, k, j % 2048, i % 2048,  gs.dstbuf.bw);
		}
	
		assert ( gs.imageEnd.x - j < widthlimit || nSize < widthlimit);	
		if (DoOneTransmitStep<psm>(pstart, nSize, gs.imageEnd.x, pbuf, k, i, j, widthlimit)) return true;				// There are 2 reasons for finish of previous for: 1) nSize < widthlimit
																		// 2) j > gs.imageEnd.x - widthlimit + 1. We would try to write pixels up do
																		// EndX, it's no more widthlimit pixels																		
		j = gs.trxpos.dx; 
	}	

	return false;
}

// PSMT4 -- Tekken V
template <int psm, int widthlimit>
inline void TRANSMIT_HOSTLOCAL_X(u32* pbuf, int& nSize, u8* pstart, int& i, int& j, int& k, int blockheight, int startX, int pitch, int fracX) {
	if (psm != 19 && psm != 20)
		ZZLog::Error_Log("This is usable function TRANSMIT_HOSTLOCAL_X at ZZoglMem.cpp %d %d %d %d %d\n", psm, widthlimit, i, j, nSize);

	for(int tempi = 0; tempi < blockheight; ++tempi) { 
		for(j = startX; j < gs.imageEnd.x; j++, k++) { 
			writePixelMem<psm, false>((u32*)pstart, j%2048, (i + tempi)%2048, (u32*)(pbuf), k, gs.dstbuf.bw); 
		} 
		k += ( pitch - fracX ); 
	} 
} 

// transfers whole rows
#define TRANSMIT_HOSTLOCAL_Y_(psm, T, widthlimit, endY) { \
	assert( (nSize%widthlimit) == 0 && widthlimit <= 4 ); \
	if( (gs.imageEnd.x-gs.trxpos.dx)%widthlimit ) { \
		/*GS_LOG("Bad Transmission! %d %d, psm: %d\n", gs.trxpos.dx, gs.imageEnd.x, gs.dstbuf.psm);*/ \
		for(; i < endY; ++i) { \
			for(; j < gs.imageEnd.x && nSize > 0; j += 1, nSize -= 1, pbuf += 1) { \
				/* write as many pixel at one time as possible */ \
				writePixel##psm##_0(pstart, j%2048, i%2048, pbuf[0], gs.dstbuf.bw); \
			} \
		} \
	} \
	for(; i < endY; ++i) { \
		for(; j < gs.imageEnd.x && nSize > 0; j += widthlimit, nSize -= widthlimit, pbuf += widthlimit) { \
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
		if( j >= gs.imageEnd.x ) { assert(j == gs.imageEnd.x); j = gs.trxpos.dx; } \
		else { assert( gs.imageTransfer == -1 || nSize*sizeof(T)/4 == 0 ); goto End; } \
	} \
} \

// transmit until endX, don't check size since it has already been prevalidated
#define TRANSMIT_HOSTLOCAL_X_(psm, T, widthlimit, blockheight, startX) { \
	for(int tempi = 0; tempi < blockheight; ++tempi) { \
		for(j = startX; j < gs.imageEnd.x; j++, pbuf++) { \
			writePixel##psm##_0(pstart, j%2048, (i+tempi)%2048, pbuf[0], gs.dstbuf.bw); \
		} \
		pbuf += pitch-fracX; \
	} \
} \

// transfers whole rows
#define TRANSMIT_HOSTLOCAL_Y_24(psm, T, widthlimit, endY) { \
	if( widthlimit != 8 || ((gs.imageEnd.x-gs.trxpos.dx)%widthlimit) ) { \
		/*GS_LOG("Bad Transmission! %d %d, psm: %d\n", gs.trxpos.dx, gs.imageEnd.x, gs.dstbuf.psm);*/ \
		for(; i < endY; ++i) { \
			for(; j < gs.imageEnd.x && nSize > 0; j += 1, nSize -= 1, pbuf += 3) { \
				writePixel##psm##_0(pstart, j%2048, i%2048, *(u32*)(pbuf), gs.dstbuf.bw); \
			} \
			\
			if( j >= gs.imageEnd.x ) { assert(gs.imageTransfer == -1 || j == gs.imageEnd.x); j = gs.trxpos.dx; } \
			else { assert( gs.imageTransfer == -1 || nSize == 0 ); goto End; } \
		} \
	} \
	else { \
		assert( /*(nSize%widthlimit) == 0 &&*/ widthlimit == 8 ); \
		for(; i < endY; ++i) { \
			for(; j < gs.imageEnd.x && nSize > 0; j += widthlimit, nSize -= widthlimit, pbuf += 3*widthlimit) { \
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
			if( j >= gs.imageEnd.x ) { assert(gs.imageTransfer == -1 || j == gs.imageEnd.x); j = gs.trxpos.dx; } \
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
		for(j = startX; j < gs.imageEnd.x; j++, pbuf += 3) { \
			writePixel##psm##_0(pstart, j%2048, (i+tempi)%2048, *(u32*)pbuf, gs.dstbuf.bw); \
		} \
		pbuf += 3*(pitch-fracX); \
	} \
} \

// meant for 4bit transfers
#define TRANSMIT_HOSTLOCAL_Y_4(psm, T, widthlimit, endY) { \
	for(; i < endY; ++i) { \
		for(; j < gs.imageEnd.x && nSize > 0; j += widthlimit, nSize -= widthlimit) { \
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
		if( j >= gs.imageEnd.x ) { j = gs.trxpos.dx; } \
		else { assert( gs.imageTransfer == -1 || (nSize/32) == 0 ); goto End; } \
	} \
} \

// transmit until endX, don't check size since it has already been prevalidated
#define TRANSMIT_HOSTLOCAL_X_4(psm, T, widthlimit, blockheight, startX) { \
	for(int tempi = 0; tempi < blockheight; ++tempi) { \
		for(j = startX; j < gs.imageEnd.x; j+=2, k++) { \
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
int TransferHostLocal(const void* pbyMem, u32 nQWordSize) 
{ 
	assert( gs.imageTransfer == 0 );
	u8* pstart = g_pbyGSMemory + gs.dstbuf.bp*256;

	const u8* pendbuf = (const u8*)pbyMem + nQWordSize*4;
	int i = gs.image.y, j = gs.image.x;
	
	const u8* pbuf = (const u8*)pbyMem;
	int nLeftOver = (nQWordSize*4*2)%(TRANSMIT_PITCH<psmX>(2));
	int nSize = nQWordSize*4*2/TRANSMIT_PITCH<psmX>(2);
	nSize = min(nSize, gs.imageNew.w * gs.imageNew.h);

	int pitch, area, fracX;
	int endY = ROUND_UPPOW2(i, blockheight);
	int alignedY = ROUND_DOWNPOW2(gs.imageEnd.y, blockheight);
	int alignedX = ROUND_DOWNPOW2(gs.imageEnd.x, blockwidth);
	bool bAligned, bCanAlign = MOD_POW2(gs.trxpos.dx, blockwidth) == 0 && (j == gs.trxpos.dx) && (alignedY > endY) && alignedX > gs.trxpos.dx;

	if( (gs.imageEnd.x - gs.trxpos.dx) % widthlimit ) {
		/* hack */
		int testwidth = (int)nSize - (gs.imageEnd.y - i) * (gs.imageEnd.x - gs.trxpos.dx) + (j - gs.trxpos.dx);
		if((testwidth <= widthlimit) && (testwidth >= -widthlimit)) {
			/* don't transfer */
			/*DEBUG_LOG("bad texture %s: %d %d %d\n", #psm, gs.trxpos.dx, gs.imageEnd.x, nQWordSize);*/
			gs.imageTransfer = -1;
		}
		bCanAlign = false;
	}

	/* first align on block boundary */
	if( MOD_POW2(i, blockheight) || !bCanAlign ) {
	
		if( !bCanAlign )
			endY = gs.imageEnd.y; /* transfer the whole image */
		else
			assert( endY < gs.imageEnd.y); /* part of alignment condition */
		
		int limit = widthlimit;
		if( ((gs.imageEnd.x-gs.trxpos.dx)%widthlimit) || ((gs.imageEnd.x-j)%widthlimit) ) 
			/* transmit with a width of 1 */
			limit = 1 + (gs.dstbuf.psm == 0x14);
		/*TRANSMIT_HOSTLOCAL_Y##TransSfx(psm, T, limit, endY)*/
		int k = 0;
		if (TRANSMIT_HOSTLOCAL_Y<psmX, widthlimit>((u32*)pbuf, nSize, pstart, endY, i, j, k)) goto End;
		pbuf += TRANSMIT_PITCH<psmX>(k);
		if( nSize == 0 || i == gs.imageEnd.y )
			goto End;
	}

	assert( MOD_POW2(i, blockheight) == 0 && j == gs.trxpos.dx);

	/* can align! */
	pitch = gs.imageEnd.x-gs.trxpos.dx;
	area = pitch*blockheight;
	fracX = gs.imageEnd.x-alignedX;

	/* on top of checking whether pbuf is alinged, make sure that the width is at least aligned to its limits (due to bugs in pcsx2) */
	bAligned = !((uptr)pbuf & 0xf) && (TRANSMIT_PITCH<psmX>(pitch)&0xf) == 0;
	
	/* transfer aligning to blocks */
	for(; i < alignedY && nSize >= area; i += blockheight, nSize -= area) {
	
		for(int tempj = gs.trxpos.dx; tempj < alignedX; tempj += blockwidth, pbuf += TRANSMIT_PITCH<psmX>(blockwidth)) {
			SwizzleBlock<psmX>((u32*)(pstart + getPixelAddress<psmX>(tempj, i, gs.dstbuf.bw)*blockbits/8),
				(u32*)pbuf, TRANSMIT_PITCH<psmX>(pitch));
		}
	
		/* transfer the rest */
		if( alignedX < gs.imageEnd.x ) {
			int k = 0;
			TRANSMIT_HOSTLOCAL_X<psmX, widthlimit>((u32*)pbuf, nSize, pstart, i, j, k, blockheight, alignedX, pitch, fracX);
			pbuf += TRANSMIT_PITCH<psmX>(k - alignedX + gs.trxpos.dx);
		}
		else pbuf += (blockheight-1)*TRANSMIT_PITCH<psmX>(pitch);
		j = gs.trxpos.dx;
	}

	if( TRANSMIT_PITCH<psmX>(nSize)/4 > 0 ) {
		int k = 0;
		TRANSMIT_HOSTLOCAL_Y<psmX, widthlimit>((u32*)pbuf, nSize, pstart, gs.imageEnd.y, i, j, k);
		pbuf += TRANSMIT_PITCH<psmX>(k);
		/* sometimes wrong sizes are sent (tekken tag) */
		assert( gs.imageTransfer == -1 || TRANSMIT_PITCH<psmX>(nSize)/4 <= 2 );
	}
	
End:
	if( i >= gs.imageEnd.y ) {
		assert( gs.imageTransfer == -1 || i == gs.imageEnd.y );
		gs.imageTransfer = -1;
		/*int start, end;
		ZeroGS::GetRectMemAddress(start, end, gs.dstbuf.psm, gs.trxpos.dx, gs.trxpos.dy, gs.imageWnew, gs.imageHnew, gs.dstbuf.bp, gs.dstbuf.bw);
		ZeroGS::g_MemTargs.ClearRange(start, end);*/
	}
	else {
		/* update new params */
		gs.image.y = i;
		gs.image.x = j;
	}
	
	return (nSize * TRANSMIT_PITCH<psmX>(2) + nLeftOver)/2;
}

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
