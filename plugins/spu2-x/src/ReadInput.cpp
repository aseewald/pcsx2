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

#define GET_DMA_DATA_PTR(offset) (((s16*)thisCore.AdmaTempBuffer)+offset)
//#define GET_DMA_DATA_PTR(offset) (GetMemPtr(0x2000 + (core<<10) + offset))

void __fastcall ReadInput( uint core, StereoOut32& PData ) 
{
	V_Core& thisCore( Cores[core] );

	if((thisCore.AutoDMACtrl&(core+1))==(core+1))
	{
		s32 tl,tr;

		if((core==1)&&((PlayMode&8)==8))
		{
			thisCore.AdmaReadPos&=~1;

			// CDDA mode
			// Source audio data is 32 bits.
			// We don't yet have the capability to handle this high res input data
			// so we just downgrade it to 16 bits for now.

			s32 *pl=(s32*)GET_DMA_DATA_PTR(thisCore.AdmaReadPos);
			s32 *pr=(s32*)GET_DMA_DATA_PTR(thisCore.AdmaReadPos+0x200);
			PData.Left = *pl;
			PData.Right = *pr;

			PData.Left >>= 2; //give 30 bit data (SndOut downsamples the rest of the way)
			PData.Right >>= 2;

			thisCore.AdmaReadPos+=2;
			if((thisCore.AdmaReadPos==0x100)||(thisCore.AdmaReadPos>=0x200)) {
				thisCore.AdmaInProgress=0;
				if(thisCore.AdmaDataLeft>=0x200)
				{
					thisCore.AdmaFree+=2;
					if(thisCore.AdmaFree>2)
					{
						thisCore.AdmaFree=2;
						thisCore.AdmaReadPos=thisCore.AdmaWritePos;
					}

					thisCore.AdmaInProgress=1;

					thisCore.TSA=(core<<10)+thisCore.AdmaReadPos;

					if (thisCore.AdmaDataLeft<0x200) 
					{
						FileLog("[%10d] AutoDMA%c block end.\n",Cycles, (core==0)?'4':'7');

						if( IsDevBuild )
						{
							if(thisCore.AdmaDataLeft>0)
							{
								if(MsgAutoDMA()) ConLog("WARNING: adma buffer didn't finish with a whole block!!\n");
							}
						}
						thisCore.AdmaDataLeft=0;
					}
				}
				thisCore.AdmaReadPos&=0x1ff;
			}

		}
		else if((core==0)&&((PlayMode&4)==4))
		{
			thisCore.AdmaReadPos&=~1;

			s32 *pl=(s32*)GET_DMA_DATA_PTR(thisCore.AdmaReadPos);
			s32 *pr=(s32*)GET_DMA_DATA_PTR(thisCore.AdmaReadPos+0x200);
			PData.Left  = *pl;
			PData.Right = *pr;

			thisCore.AdmaReadPos+=2;
			if(thisCore.AdmaReadPos>=0x200) {
				thisCore.AdmaInProgress=0;
				if(thisCore.AdmaDataLeft>=0x200)
				{
					thisCore.AdmaFree+=2;
					if(thisCore.AdmaFree>2)
					{
						thisCore.AdmaFree=2;
						thisCore.AdmaReadPos=thisCore.AdmaWritePos;
					}

					thisCore.AdmaInProgress=1;

					thisCore.TSA=(core<<10)+thisCore.AdmaReadPos;

					if (thisCore.AdmaDataLeft<0x200) 
					{
						FileLog("[%10d] Spdif AutoDMA%c block end.\n",Cycles, (core==0)?'4':'7');

						if( IsDevBuild )
						{
							if(thisCore.AdmaDataLeft>0)
							{
								if(MsgAutoDMA()) ConLog("WARNING: adma buffer didn't finish with a whole block!!\n");
							}
						}
						thisCore.AdmaDataLeft=0;
					}
				}
				thisCore.AdmaReadPos&=0x1ff;
			}

		}
		else
		{
			if((core==1)&&((PlayMode&2)!=0))
			{
				tl=0;
				tr=0;
			}
			else
			{
				tl = (s32)*GET_DMA_DATA_PTR(thisCore.AdmaReadPos);
				tr = (s32)*GET_DMA_DATA_PTR(thisCore.AdmaReadPos+0x200);
			}

			PData.Left  = tl;
			PData.Right = tr;

			thisCore.AdmaReadPos++;
			if((thisCore.AdmaReadPos==0x100)||(thisCore.AdmaReadPos>=0x200)) {
				thisCore.AdmaInProgress=0;
				if(thisCore.AdmaDataLeft>=0x200)
				{
					thisCore.AdmaFree++;
					if(thisCore.AdmaFree>2)
					{
						thisCore.AdmaFree=2;
						thisCore.AdmaReadPos=thisCore.AdmaWritePos;
					}

					thisCore.AdmaInProgress=1;

					thisCore.TSA=(core<<10)+thisCore.AdmaReadPos;

					if (thisCore.AdmaDataLeft<0x200) 
					{
						thisCore.AutoDMACtrl |= ~3;

						if( IsDevBuild )
						{
							FileLog("[%10d] AutoDMA%c block end.\n",Cycles, (core==0)?'4':'7');
							if(thisCore.AdmaDataLeft>0)
							{
								if(MsgAutoDMA()) ConLog("WARNING: adma buffer didn't finish with a whole block!!\n");
							}
						}

						thisCore.AdmaDataLeft = 0;
					}
				}
				thisCore.AdmaReadPos&=0x1ff;
			}
		}
	}
	else
	{
		PData.Left  = 0;
		PData.Right = 0;
	}
}
