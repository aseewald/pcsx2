/* SPU2-X, A plugin for Emulating the Sound Processing Unit of the Playstation 2
 * Developed and maintained by the Pcsx2 Development Team.
 * 
 * Original portions from SPU2ghz are (c) 2008 by David Quintana [gigaherz]
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free 
 * Software Foundation; either version 2.1 of the the License, or (at your
 * option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but WITHOUT 
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License along
 * with this library; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 */

#include "Spu2.h"

extern u8 callirq;

FILE *DMA4LogFile=0;
FILE *DMA7LogFile=0;
FILE *ADMA4LogFile=0;
FILE *ADMA7LogFile=0;
FILE *ADMAOutLogFile=0;

FILE *REGWRTLogFile[2]={0,0};

int packcount=0;

void DMALogOpen()
{
	if(!DMALog()) return;
	DMA4LogFile    = fopen( Unicode::Convert( DMA4LogFileName ).c_str(), "wb");
	DMA7LogFile    = fopen( Unicode::Convert( DMA7LogFileName ).c_str(), "wb");
	ADMA4LogFile   = fopen( "logs/adma4.raw", "wb" );
	ADMA7LogFile   = fopen( "logs/adma7.raw", "wb" );
	ADMAOutLogFile = fopen( "logs/admaOut.raw", "wb" );
	//REGWRTLogFile[0]=fopen("logs/RegWrite0.raw","wb");
	//REGWRTLogFile[1]=fopen("logs/RegWrite1.raw","wb");
}
void DMA4LogWrite(void *lpData, u32 ulSize) {
	if(!DMALog()) return;
	if (!DMA4LogFile) return;
	fwrite(lpData,ulSize,1,DMA4LogFile);
}

void DMA7LogWrite(void *lpData, u32 ulSize) {
	if(!DMALog()) return;
	if (!DMA7LogFile) return;
	fwrite(lpData,ulSize,1,DMA7LogFile);
}

void ADMA4LogWrite(void *lpData, u32 ulSize) {
	if(!DMALog()) return;
	if (!ADMA4LogFile) return;
	fwrite(lpData,ulSize,1,ADMA4LogFile);
}
void ADMA7LogWrite(void *lpData, u32 ulSize) {
	if(!DMALog()) return;
	if (!ADMA7LogFile) return;
	fwrite(lpData,ulSize,1,ADMA7LogFile);
}
void ADMAOutLogWrite(void *lpData, u32 ulSize) {
	if(!DMALog()) return;
	if (!ADMAOutLogFile) return;
	fwrite(lpData,ulSize,1,ADMAOutLogFile);
}

void RegWriteLog(u32 core,u16 value)
{
	if(!DMALog()) return;
	if (!REGWRTLogFile[core]) return;
	fwrite(&value,2,1,REGWRTLogFile[core]);
}

void AdmaLogWritePacket(int core, s16* data)
{
	if(core==0)
	{
		for(int i=0;i<256;i++)
		{
			ADMA4LogWrite(data+i,2);
			ADMA4LogWrite(data+i+256,2);
		}
	}
	else
	{
		for(int i=0;i<256;i++)
		{
			ADMA7LogWrite(data+i,2);
			ADMA7LogWrite(data+i+256,2);
		}
	}

}

void DMALogClose() {
	if(!DMALog()) return;
	if (DMA4LogFile) fclose(DMA4LogFile);
	if (DMA7LogFile) fclose(DMA7LogFile);
	if (REGWRTLogFile[0]) fclose(REGWRTLogFile[0]);
	if (REGWRTLogFile[1]) fclose(REGWRTLogFile[1]);
	if (ADMA4LogFile) fclose(ADMA4LogFile);
	if (ADMA7LogFile) fclose(ADMA7LogFile);
	if (ADMAOutLogFile) fclose(ADMAOutLogFile);
}


__forceinline u16 DmaRead(u32 core)
{
	const u16 ret = (u16)spu2M_Read(Cores[core].TDA);
	Cores[core].TDA++;
	Cores[core].TDA&=0xfffff;
	return ret;
}

__forceinline void DmaWrite(u32 core, u16 value)
{
	spu2M_Write( Cores[core].TSA, value );
	Cores[core].TSA++;
	Cores[core].TSA&=0xfffff;
}

#define MAX_SINGLE_TRANSFER_SIZE 2048

#define GET_DMA_DATA_PTR(offset) (((s16*)Cores[core].AdmaTempBuffer)+offset)
//#define GET_DMA_DATA_PTR(offset) (GetMemPtr(0x2000 + (core<<10) + offset))

s32 CALLBACK SPU2dmaWrite(s32 channel, s16* data, u32 bytesLeft, u32* bytesProcessed)
{
	if(hasPtr) TimeUpdate(*cPtr);

	FileLog("[%10d] SPU2 dmaWrite channel %d size %x\n", Cycles, channel, bytesLeft);

	int core = channel/7;

	Cores[core].Regs.STATX &= ~0x80;

	if(bytesLeft<16) 
		return 0;

	Cores[core].TSA&=~7;

	bool isAdma = ((Cores[core].AutoDMACtrl&(core+1))==(core+1));

	if(isAdma)
	{
		//Cores[core].TSA&=0x1fff;

		if(Cores[core].AdmaDataLeft==0)
		{
			Cores[core].AdmaDataLeft = bytesLeft;
			Cores[core].AdmaFree = 2;
			Cores[core].AdmaReadPos = 0;
			Cores[core].AdmaWritePos = 0;
		}

		if(Cores[core].AdmaFree<=0) // no space, try later
		{
			*bytesProcessed = 0;
			return 768*2;
		}

		int transferSize = 0;
		while (Cores[core].AdmaFree>0)
		{
			AdmaLogWritePacket(core,data);
			// partL
			{
				memcpy(GET_DMA_DATA_PTR(Cores[core].AdmaWritePos),data,512);
				data += 256;
				
				int TSA = 0x2000 + (core<<10);
				int TDA = TSA + 0x100;
				if((Cores[core].IRQA>=TSA)&&(Cores[core].IRQA<TDA))
				{
					Spdif.Info=4<<core;
					SetIrqCall();
				}
			}

			// partR
			{
				memcpy(GET_DMA_DATA_PTR(Cores[core].AdmaWritePos + 0x200),data,512);
				data += 256;
				
				int TSA = 0x2200 + (core<<10);
				int TDA = TSA + 0x100;
				if((Cores[core].IRQA>=TSA)&&(Cores[core].IRQA<TDA))
				{
					Spdif.Info=4<<core;
					SetIrqCall();
				}
			}

			Cores[core].AdmaWritePos = (Cores[core].AdmaWritePos + 0x100) & 0x1ff;
			Cores[core].AdmaFree--; // there's two buffers, we have one less available

			transferSize+=1024;
		}

		if( IsDevBuild )
			DebugCores[core].lastsize = transferSize;

		*bytesProcessed = transferSize;

		return 0;
	}
	else
	{
		int transferSize = MAX_SINGLE_TRANSFER_SIZE;

		if(bytesLeft < transferSize)
			transferSize = bytesLeft;		

		transferSize >>=1; // we work in half-words

		int part1 = 0xFFFFFF - Cores[core].TSA;
		if(part1 > transferSize)
			part1 = transferSize;

		if(part1>0)
		{
			memcpy(GetMemPtr(Cores[core].TSA),data,part1<<1);
			data += part1;
			
			Cores[core].TDA = Cores[core].TSA + part1;
			if((Cores[core].IRQA>=Cores[core].TSA)&&(Cores[core].IRQA<Cores[core].TDA))
			{
				Spdif.Info=4<<core;
				SetIrqCall();
			}

			// invalidate caches between TSA and TDA
			int first = Cores[core].TSA / pcm_WordsPerBlock;
			int last = Cores[core].TDA / pcm_WordsPerBlock;
			PcmCacheEntry* pfirst = pcm_cache_data + first;
			PcmCacheEntry* plast  = pcm_cache_data + last;
			for(;pfirst<plast;pfirst++)
				pfirst->Validated=0;

			Cores[core].TSA = Cores[core].TDA;
		}

		int part2 = transferSize - part1;
		if(part2>0)
		{
			memcpy(GetMemPtr(0),data,part2<<1);
			data += part2;
			
			Cores[core].TDA = Cores[core].TSA + part2;
			if((Cores[core].IRQA>=Cores[core].TSA)&&(Cores[core].IRQA<Cores[core].TDA))
			{
				Spdif.Info=4<<core;
				SetIrqCall();
			}

			// invalidate caches between TSA and TDA
			int first = Cores[core].TSA / pcm_WordsPerBlock;
			int last = Cores[core].TDA / pcm_WordsPerBlock;
			PcmCacheEntry* pfirst = pcm_cache_data + first;
			PcmCacheEntry* plast  = pcm_cache_data + last;
			for(;pfirst<plast;pfirst++)
				pfirst->Validated=0;

			Cores[core].TSA = Cores[core].TDA;
		}

		if((bytesLeft>>1) == (transferSize))
		{
			if((Cores[core].IRQA>=Cores[core].TSA)&&(Cores[core].IRQA<(Cores[core].TSA+0x20)))
			{
				Spdif.Info=4<<core;
				SetIrqCall();
			}
			transferSize = bytesLeft;
		}
		else transferSize<<=1;


		if( IsDevBuild )
			DebugCores[core].lastsize = transferSize;

		*bytesProcessed = transferSize;
		return 0;
	}
}

s32 CALLBACK SPU2dmaRead(s32 channel, u16* data, u32 bytesLeft, u32* bytesProcessed)
{
	if(hasPtr) TimeUpdate(*cPtr);

	FileLog("[%10d] SPU2 dmaRead channel %d size %x\n", Cycles, channel, bytesLeft);

	int core = channel/7;

	Cores[core].Regs.STATX &= ~0x80;

	if(bytesLeft<16) 
		return 0;

	Cores[core].TSA&=~7;

	// If there's autodma reading, and somehow some game needs it, then you can implement it yourselves :P
	{
		int transferSize = MAX_SINGLE_TRANSFER_SIZE;
		if(bytesLeft < transferSize)
			transferSize = bytesLeft;

		transferSize >>=1; // we work in half-words

		int part1 = 0xFFFFFF - Cores[core].TSA;
		if(part1 > transferSize)
			part1 = transferSize;

		if(part1>0)
		{
			memcpy(data,GetMemPtr(Cores[core].TSA),part1<<1);
			data += part1;
			
			Cores[core].TDA = Cores[core].TSA + part1;
			if((Cores[core].IRQA>=Cores[core].TSA)&&(Cores[core].IRQA<Cores[core].TDA))
			{
				Spdif.Info=4<<core;
				SetIrqCall();
			}

			Cores[core].TSA = Cores[core].TDA;
		}

		int part2 = transferSize - part1;
		if(part2>0)
		{
			memcpy(data,GetMemPtr(0),part2<<1);
			data += part2;
			
			Cores[core].TDA = Cores[core].TSA + part2;
			if((Cores[core].IRQA>=Cores[core].TSA)&&(Cores[core].IRQA<Cores[core].TDA))
			{
				Spdif.Info=4<<core;
				SetIrqCall();
			}

			Cores[core].TSA = Cores[core].TDA;
		}

		if(bytesLeft == transferSize)
		{
			if((Cores[core].IRQA==Cores[core].TSA))
			{
				Spdif.Info=4<<core;
				SetIrqCall();
			}
		}

		if( IsDevBuild )
			DebugCores[core].lastsize = transferSize;

		*bytesProcessed = transferSize<<1;
		return 0;
	}
}

void CALLBACK SPU2dmaInterrupt(s32 channel) 
{
	int core = channel/7;
	FileLog("[%10d] SPU2 dma Interrupt channel %d\n",Cycles,channel);
	Cores[core].Regs.STATX |= 0x80;
}

