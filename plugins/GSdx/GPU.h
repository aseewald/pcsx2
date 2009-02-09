/* 
 *	Copyright (C) 2007-2009 Gabest
 *	http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#pragma once

#pragma pack(push, 1)

#include "GS.h"

enum
{
	GPU_POLYGON = 1,
	GPU_LINE = 2,
	GPU_SPRITE = 3,
};

REG32_(GPUReg, STATUS)
	UINT32 TX:4;
	UINT32 TY:1;
	UINT32 ABR:2;
	UINT32 TP:2;
	UINT32 DTD:1;
	UINT32 DFE:1;
	UINT32 MD:1;
	UINT32 ME:1;
	UINT32 _PAD0:3;
	UINT32 WIDTH1:1;
	UINT32 WIDTH0:2;
	UINT32 HEIGHT:1;
	UINT32 ISPAL:1;
	UINT32 ISRGB24:1;
	UINT32 ISINTER:1;
	UINT32 DEN:1;
	UINT32 _PAD1:2;
	UINT32 IDLE:1;
	UINT32 IMG:1;
	UINT32 COM:1;
	UINT32 DMA:2;
	UINT32 LCF:1;
	/*
	UINT32 TX:4;
	UINT32 TY:1;
	UINT32 ABR:2;
	UINT32 TP:2;
	UINT32 DTD:1;
	UINT32 DFE:1;
	UINT32 PBW:1;
	UINT32 PBC:1;
	UINT32 _PAD0:3;
	UINT32 HRES2:1;
	UINT32 HRES1:2;
	UINT32 VRES:1;
	UINT32 ISPAL:1;
	UINT32 ISRGB24:1;
	UINT32 ISINTER:1;
	UINT32 ISSTOP:1;
	UINT32 _PAD1:1;
	UINT32 DMARDY:1;
	UINT32 IDIDLE:1;
	UINT32 DATARDY:1;
	UINT32 ISEMPTY:1;
	UINT32 TMODE:2;
	UINT32 ODE:1;
	*/
REG_END

REG32_(GPUReg, PACKET)
	UINT32 _PAD:24;
	UINT32 OPTION:5;
	UINT32 TYPE:3;
REG_END

REG32_(GPUReg, PRIM)
	UINT32 VTX:24;
	UINT32 TGE:1;
	UINT32 ABE:1;
	UINT32 TME:1;
	UINT32 _PAD2:1;
	UINT32 IIP:1;
	UINT32 TYPE:3;
REG_END

REG32_(GPUReg, POLYGON)
	UINT32 _PAD:24;
	UINT32 TGE:1;
	UINT32 ABE:1;
	UINT32 TME:1;
	UINT32 VTX:1;
	UINT32 IIP:1;
	UINT32 TYPE:3;
REG_END

REG32_(GPUReg, LINE)
	UINT32 _PAD:24;
	UINT32 ZERO1:1;
	UINT32 ABE:1;
	UINT32 ZERO2:1;
	UINT32 PLL:1;
	UINT32 IIP:1;
	UINT32 TYPE:3;
REG_END

REG32_(GPUReg, SPRITE)
	UINT32 _PAD:24;
	UINT32 ZERO:1;
	UINT32 ABE:1;
	UINT32 TME:1;
	UINT32 SIZE:2;
	UINT32 TYPE:3;
REG_END

REG32_(GPUReg, RESET)
	UINT32 _PAD:32;
REG_END

REG32_(GPUReg, DEN)
	UINT32 DEN:1;
	UINT32 _PAD:31;
REG_END

REG32_(GPUReg, DMA)
	UINT32 DMA:2;
	UINT32 _PAD:30;
REG_END

REG32_(GPUReg, DAREA)
	UINT32 X:10;
	UINT32 Y:9;
	UINT32 _PAD:13;
REG_END

REG32_(GPUReg, DHRANGE)
	UINT32 X1:12;
	UINT32 X2:12;
	UINT32 _PAD:8;
REG_END

REG32_(GPUReg, DVRANGE)
	UINT32 Y1:10;
	UINT32 Y2:11;
	UINT32 _PAD:11;
REG_END

REG32_(GPUReg, DMODE)
	UINT32 WIDTH0:2;
	UINT32 HEIGHT:1;
	UINT32 ISPAL:1;
	UINT32 ISRGB24:1;
	UINT32 ISINTER:1;
	UINT32 WIDTH1:1;
	UINT32 REVERSE:1;
	UINT32 _PAD:24;
REG_END

REG32_(GPUReg, GPUINFO)
	UINT32 PARAM:24;
	UINT32 _PAD:8;
REG_END

REG32_(GPUReg, MODE)
	UINT32 TX:4;
	UINT32 TY:1;
	UINT32 ABR:2;
	UINT32 TP:2;
	UINT32 DTD:1;
	UINT32 DFE:1;
	UINT32 _PAD:21;
REG_END

REG32_(GPUReg, MASK)
	UINT32 MD:1;
	UINT32 ME:1;
	UINT32 _PAD:30;
REG_END

REG32_(GPUReg, DRAREA)
	UINT32 X:10;
	UINT32 Y:10;
	UINT32 _PAD:12;
REG_END

REG32_(GPUReg, DROFF)
	INT32 X:11;
	INT32 Y:11;
	INT32 _PAD:10;
REG_END

REG32_(GPUReg, RGB)
	UINT32 R:8;
	UINT32 G:8;
	UINT32 B:8;
	UINT32 _PAD:8;
REG_END

REG32_(GPUReg, XY)
	INT32 X:11;
	INT32 _PAD1:5;
	INT32 Y:11;
	INT32 _PAD2:5;
REG_END

REG32_(GPUReg, UV)
	UINT32 U:8;
	UINT32 V:8;
	UINT32 _PAD:16;
REG_END

REG32_(GPUReg, TWIN)
	UINT32 TWW:5;
	UINT32 TWH:5;
	UINT32 TWX:5;
	UINT32 TWY:5;
	UINT32 _PAD:12;
REG_END

REG32_(GPUReg, CLUT)
	UINT32 _PAD1:16;
	UINT32 X:6;
	UINT32 Y:9;
	UINT32 _PAD2:1;
REG_END

REG32_SET(GPUReg)
	GPURegSTATUS STATUS;
	GPURegPACKET PACKET;
	GPURegPRIM PRIM;
	GPURegPOLYGON POLYGON;
	GPURegLINE LINE;
	GPURegSPRITE SPRITE;
	GPURegRESET RESET;
	GPURegDEN DEN;
	GPURegDMA DMA;
	GPURegDAREA DAREA;
	GPURegDHRANGE DHRANGE;
	GPURegDVRANGE DVRANGE;
	GPURegDMODE DMODE;
	GPURegGPUINFO GPUINFO;
	GPURegMODE MODE;
	GPURegMASK MASK;
	GPURegDRAREA DRAREA;
	GPURegDROFF DROFF;
	GPURegRGB RGB;
	GPURegXY XY;
	GPURegUV UV;
	GPURegTWIN TWIN;
	GPURegCLUT CLUT;
REG_SET_END

struct GPUFreezeData
{
	UINT32 version; // == 1
	UINT32 status;
	UINT32 control[256];
	UINT16 vram[1024 * 1024];
};

#pragma pack(pop)

