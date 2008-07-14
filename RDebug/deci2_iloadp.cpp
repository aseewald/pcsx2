/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2002-2008  Pcsx2 Team
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "Common.h"
#include "PsxCommon.h"
#include "PsxBios2.h"
#include "deci2.h"

typedef struct tag_DECI2_ILOADP_HEADER{
	DECI2_HEADER	h;			//+00
	u8				code,		//+08	cmd
					action,		//+09
					result,		//+0A
					stamp;		//+0B
	u32				moduleId;	//+0C
} DECI2_ILOADP_HEADER;			//=10

typedef struct tag_DECI2_ILOADP_INFO{
	u16		version,		//+00
			flags;			//+02
	u32		module_address,	//+04
			text_size,		//+08
			data_size,		//+0C
			bss_size,		//+10
			_pad[3];		//+14
} DECI2_ILOADP_INFO;

void writeInfo(DECI2_ILOADP_INFO *info,
			   u16 version, u16 flags, u32 module_address,
			   u32 text_size, u32 data_size, u32 bss_size){
	info->version		=version;
	info->flags			=flags;
	info->module_address=module_address;
	info->text_size		=text_size;
	info->data_size		=data_size;
	info->bss_size		=bss_size;
	info->_pad[0]=info->_pad[1]=info->_pad[2]=0;
}

void D2_ILOADP(char *inbuffer, char *outbuffer, char *message){
	DECI2_ILOADP_HEADER	*in=(DECI2_ILOADP_HEADER*)inbuffer,
				*out=(DECI2_ILOADP_HEADER*)outbuffer;
	u8	*data=(u8*)in+sizeof(DECI2_ILOADP_HEADER);
	irxImageInfo	*iii;
	static char line[1024];

	memcpy(outbuffer, inbuffer, 128*1024);//BUFFERSIZE
	out->h.length=sizeof(DECI2_ILOADP_HEADER);
	out->code++;
	out->result=0;	//ok
	exchangeSD((DECI2_HEADER*)out);
	switch(in->code){
		case 0:
			sprintf(line, "code=START action=%d stamp=%d moduleId=0x%X", in->action, in->stamp, in->moduleId);
			break;
		case 2:
			sprintf(line, "code=REMOVE action=%d stamp=%d moduleId=0x%X", in->action, in->stamp, in->moduleId);
			break;
		case 4:
			sprintf(line, "code=LIST action=%d stamp=%d moduleId=0x%X", in->action, in->stamp, in->moduleId);
			data=(u8*)out+sizeof(DECI2_ILOADP_HEADER);
				for (iii=(irxImageInfo*)PSXM(0x800); iii; iii=iii->next?(irxImageInfo*)PSXM(iii->next):0, data+=4)
					*(u32*)data=iii->index;

			out->h.length=data-(u8*)out;
			break;
		case 6:
			sprintf(line, "code=INFO action=%d stamp=%d moduleId=0x%X", in->action, in->stamp, in->moduleId);
			data=(u8*)out+sizeof(DECI2_ILOADP_HEADER);
				for(iii=(irxImageInfo*)PSXM(0x800); iii; iii=iii->next?(irxImageInfo*)PSXM(iii->next):0)
					if (iii->index==in->moduleId){
						writeInfo((DECI2_ILOADP_INFO*)data,
							iii->version, iii->flags, iii->vaddr, iii->text_size, iii->data_size, iii->bss_size);
						data+=sizeof(DECI2_ILOADP_INFO);
						strcpy((char*)data, (char*)PSXM(iii->name));
						data+=strlen((char*)PSXM(iii->name))+4;
						data=(u8*)((int)data & 0xFFFFFFFC);
						break;
					
			}
			out->h.length=data-(u8*)out;
			break;
		case 8:
			sprintf(line, "code=WATCH action=%d stamp=%d moduleId=0x%X", in->action, in->stamp, in->moduleId);
			break;
		default:
			sprintf(line, "code=%d[unknown]", in->code);
	}
	sprintf(message, "[ILOADP %c->%c/%04X] %s", in->h.source, in->h.destination, in->h.length, line);
	writeData(outbuffer);
}
