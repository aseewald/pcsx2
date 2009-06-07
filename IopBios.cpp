/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2002-2009  Pcsx2 Team
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

#include "PrecompiledHeader.h"

#include <ctype.h>
#include "IopCommon.h"
#include "Common.h"

namespace R3000A
{

struct irxlib
{
	const char *name;
	const char *names[64];
	const int maxn;
};

//////////////////////////////////////////////////////////////////////////////////////////
//
#define IRXLIBS 14
const irxlib irxlibs[] =
{
/*00*/	
	{ 
	"sysmem",
	{ "start", "init_memory", "retonly", "return_addr_of_memsize",
	"AllocSysMemory", "FreeSysMemory", "QueryMemSize", "QueryMaxFreeMemSize",
	"QueryTotalFreeMemSize", "QueryBlockTopAddress", "QueryBlockSize", "retonly",
	"retonly", "retonly", "Kprintf", "set_Kprintf" } ,
	16
	},
/*01*/	
      { 
	"loadcore",
	{ "start", "retonly", "retonly_", "return_LibraryEntryTable",
	"FlushIcache", "FlushDcache", "RegisterLibraryEntries", "ReleaseLibraryEntries",
	"findFixImports", "restoreImports", "RegisterNonAutoLinkEntries", "QueryLibraryEntryTable",
	"QueryBootMode", "RegisterBootMode", "setFlag", "resetFlag",
	"linkModule", "unlinkModule", "retonly_", "retonly_",
	"registerFunc", "jumpA0001B34", "read_header", "load_module",
	"findImageInfo" },
	25
      },
/*02*/	
	{ 
	"excepman",
	{ "start", "reinit", "deinit", "getcommon",
	"RegisterExceptionHandler", "RegisterPriorityExceptionHandler",
	"RegisterDefaultExceptionHandler", "ReleaseExceptionHandler",
	"ReleaseDefaultExceptionHandler" } ,
	9
	},
/*03_4*/
	{ 
	"intrman",
	{ "start", "return_0", "deinit", "call3",
	"RegisterIntrHandler", "ReleaseIntrHandler", "EnableIntr", "DisableIntr",
	"CpuDisableIntr", "CpuEnableIntr", "syscall04", "syscall08",
	"resetICTRL", "setICTRL", "syscall0C", "call15",
	"call16", "CpuSuspendIntr", "CpuResumeIntr", "CpuSuspendIntr",
	"CpuResumeIntr",  "syscall10", "syscall14", "QueryIntrContext",
	"QueryIntrStack", "iCatchMultiIntr", "retonly", "call27",
	"set_h1", "reset_h1", "set_h2", "reset_h2" } ,
	0x20
	},
/*05*/	
	{ 
	"ssbusc",
	{ "start", "retonly", "return_0", "retonly",
	"setTable1", "getTable1", "setTable2", "getTable2",
	"setCOM_DELAY_1st", "getCOM_DELAY_1st", "setCOM_DELAY_2nd", "getCOM_DELAY_2nd",
	"setCOM_DELAY_3rd", "getCOM_DELAY_3rd", "setCOM_DELAY_4th", "getCOM_DELAY_4th",
	"setCOM_DELAY", "getCOM_DELAY" } ,
	18
	},
/*06*/
	{
	"dmacman",
	{ "start", "retonly", "deinit", "retonly",
	"SetD_MADR", "GetD_MADR", "SetD_BCR", "GetD_BCR",
	"SetD_CHCR", "GetD_CHCR", "SetD_TADR", "GetD_TADR",
	"Set_4_9_A", "Get_4_9_A", "SetDPCR", "GetDPCR",
	"SetDPCR2", "GetDPCR2", "SetDPCR3", "GetDPCR3",
	"SetDICR", "GetDICR", "SetDICR2", "GetDICR2",
	"SetBF80157C", "GetBF80157C", "SetBF801578", "GetBF801578",
	"SetDMA", "SetDMA_chainedSPU_SIF0", "SetDMA_SIF0", "SetDMA_SIF1",
	"StartTransfer", "SetVal", "EnableDMAch", "DisableDMAch" } ,
	36
	},
/*07_8*/
	{
	"timrman",
  	{ "start", "retonly", "retonly", "call3",
	"AllocHardTimer", "ReferHardTimer", "FreeHardTimer", "SetTimerMode",
	"GetTimerStatus", "SetTimerCounter", "GetTimerCounter", "SetTimerCompare",
	"GetTimerCompare", "SetHoldMode", "GetHoldMode", "GetHoldReg",
	"GetHardTimerIntrCode" } ,
	17
	},
/*09*/	
	{ 
	"sysclib",
	{ "start", "reinit", "retonly", "retonly",
	"setjmp", "longjmp", "toupper", "tolower",
	"look_ctype_table", "get_ctype_table", "memchr", "memcmp",
	"memcpy", "memmove", "memset", "bcmp",
	"bcopy", "bzero", "prnt", "sprintf",
	"strcat", "strchr", "strcmp", "strcpy",
	"strcspn", "index", "rindex", "strlen",
	"strncat", "strncmp", "strncpy", "strpbrk",
	"strrchr", "strspn", "strstr", "strtok",
	"strtol", "atob", "strtoul", "wmemcopy",
	"wmemset", "vsprintf" } ,
	0x2b
	},
/*0A*/	
	{
	"heaplib",
	{ "start", "retonly", "retonly", "retonly",
	"CreateHeap", "DestroyHeap", "HeapMalloc", "HeapFree",
	"HeapSize", "retonly", "retonly", "call11",
	"call12", "call13", "call14", "call15",
	"retonly", "retonly" } ,
	18
	},
/*13*/	
	{
	"stdio",
	{ "start", "unknown", "unknown", "unknown",
	"printf" } ,
	5
	},
/*14*/	
	{ 
	"sifman",
	{ "start", "retonly", "deinit", "retonly",
	"sceSif2Init", "sceSifInit", "sceSifSetDChain", "sceSifSetDma",
	"sceSifDmaStat", "sceSifSend", "sceSifSendSync", "sceSifIsSending",
	"sceSifSetSIF0DMA", "sceSifSendSync0", "sceSifIsSending0", "sceSifSetSIF1DMA",
	"sceSifSendSync1", "sceSifIsSending1", "sceSifSetSIF2DMA", "sceSifSendSync2",
	"sceSifIsSending2", "getEEIOPflags", "setEEIOPflags", "getIOPEEflags",
	"setIOPEEflags", "getEErcvaddr", "getIOPrcvaddr", "setIOPrcvaddr",
	"call28", "sceSifCheckInit", "setSif0CB", "resetSif0CB",
	"retonly", "retonly", "retonly", "retonly" } ,
	36
	},
/*16*/	
	{ 
	"sifcmd",
	{ "start", "retonly", "deinit", "retonly",
	"sceSifInitCmd", "sceSifExitCmd", "sceSifGetSreg", "sceSifSetSreg",
	"sceSifSetCmdBuffer", "sceSifSetSysCmdBuffer",
	"sceSifAddCmdHandler", "sceSifRemoveCmdHandler",
	"sceSifSendCmd", "isceSifSendCmd", "sceSifInitRpc", "sceSifBindRpc",
	"sceSifCallRpc", "sceSifRegisterRpc",
	"sceSifCheckStatRpc", "sceSifSetRpcQueue",
	"sceSifGetNextRequest", "sceSifExecRequest",
	"sceSifRpcLoop", "sceSifGetOtherData",
	"sceSifRemoveRpc", "sceSifRemoveRpcQueue",
	"setSif1CB", "resetSif1CB",
	"retonly", "retonly", "retonly", "retonly" } ,
	32
	},
/*19*/	
	{ 
	"cdvdman",
	{ "start", "retonly", "retonly", "retonly",
	"sceCdInit", "sceCdStandby", "sceCdRead", "sceCdSeek",
	"sceCdGetError", "sceCdGetToc", "sceCdSearchFile", "sceCdSync",
	"sceCdGetDiskType", "sceCdDiskReady", "sceCdTrayReq", "sceCdStop",
	"sceCdPosToInt", "sceCdIntToPos", "retonly", "call19",
	"sceDvdRead", "sceCdCheckCmd", "_sceCdRI", "sceCdWriteILinkID",
	"sceCdReadClock", "sceCdWriteRTC", "sceCdReadNVM", "sceCdWriteNVM",
	"sceCdStatus", "sceCdApplySCmd", "setHDmode", "sceCdOpenConfig",
	"sceCdCloseConfig", "sceCdReadConfig", "sceCdWriteConfig", "sceCdReadKey",
	"sceCdDecSet", "sceCdCallback", "sceCdPause", "sceCdBreak",
	"call40", "sceCdReadConsoleID", "sceCdWriteConsoleID", "sceCdGetMecaconVersion",
	"sceCdGetReadPos", "AudioDigitalOut", "sceCdNop", "_sceGetFsvRbuf",
	"_sceCdstm0Cb", "_sceCdstm1Cb", "_sceCdSC", "_sceCdRC",
	"sceCdForbidDVDP", "sceCdReadSubQ", "sceCdApplyNCmd", "AutoAdjustCtrl",
	"sceCdStInit", "sceCdStRead", "sceCdStSeek", "sceCdStStart",
	"sceCdStStat", "sceCdStStop" } ,
	62
	},
/*??*/	
	{ 
	"sio2man",
	{ "start", "retonly", "deinit", "retonly",
	"set8268_ctrl", "get8268_ctrl", "get826C_recv1", "call7_send1",
	"call8_send1", "call9_send2", "call10_send2", "get8270_recv2",
	"call12_set_params", "call13_get_params", "get8274_recv3", "set8278",
	"get8278", "set827C", "get827C", "set8260_datain",
	"get8264_dataout", "set8280_intr", "get8280_intr", "signalExchange1",
	"signalExchange2", "packetExchange" } ,
	26
	}
};

//////////////////////////////////////////////////////////////////////////////////////////
//
const char *biosA0n[256] = {
// 0x00
	"open",		"lseek",	"read",		"write",
	"close",	"ioctl",	"exit",		"sys_a0_07",
	"getc",		"putc",		"todigit",	"atof",
	"strtoul",	"strtol",	"abs",		"labs",
// 0x10
	"atoi",		"atol",		"atob",		"setjmp",
	"longjmp",	"strcat",	"strncat",	"strcmp",
	"strncmp",	"strcpy",	"strncpy",	"strlen",
	"index",	"rindex",	"strchr",	"strrchr",
// 0x20
	"strpbrk",	"strspn",	"strcspn",	"strtok",
	"strstr",	"toupper",	"tolower",	"bcopy",
	"bzero",	"bcmp",		"memcpy",	"memset",
	"memmove",	"memcmp",	"memchr",	"rand",
// 0x30
	"srand",	"qsort",	"strtod",	"malloc",
	"free",		"lsearch",	"bsearch",	"calloc",
	"realloc",	"InitHeap",	"_exit",	"getchar",
	"putchar",	"gets",		"puts",		"printf",
// 0x40
	"sys_a0_40",		"LoadTest",					"Load",		"Exec",
	"FlushCache",		"InstallInterruptHandler",	"GPU_dw",	"mem2vram",
	"SendGPUStatus",	"GPU_cw",					"GPU_cwb",	"SendPackets",
	"sys_a0_4c",		"GetGPUStatus",				"GPU_sync",	"sys_a0_4f",
// 0x50
	"sys_a0_50",		"LoadExec",				"GetSysSp",		"sys_a0_53",
	"_96_init()",		"_bu_init()",			"_96_remove()",	"sys_a0_57",
	"sys_a0_58",		"sys_a0_59",			"sys_a0_5a",	"dev_tty_init",
	"dev_tty_open",		"sys_a0_5d",			"dev_tty_ioctl","dev_cd_open",
// 0x60
	"dev_cd_read",		"dev_cd_close",			"dev_cd_firstfile",	"dev_cd_nextfile",
	"dev_cd_chdir",		"dev_card_open",		"dev_card_read",	"dev_card_write",
	"dev_card_close",	"dev_card_firstfile",	"dev_card_nextfile","dev_card_erase",
	"dev_card_undelete","dev_card_format",		"dev_card_rename",	"dev_card_6f",
// 0x70
	"_bu_init",			"_96_init",		"_96_remove",		"sys_a0_73",
	"sys_a0_74",		"sys_a0_75",	"sys_a0_76",		"sys_a0_77",
	"_96_CdSeekL",		"sys_a0_79",	"sys_a0_7a",		"sys_a0_7b",
	"_96_CdGetStatus",	"sys_a0_7d",	"_96_CdRead",		"sys_a0_7f",
// 0x80
	"sys_a0_80",		"sys_a0_81",	"sys_a0_82",		"sys_a0_83",
	"sys_a0_84",		"_96_CdStop",	"sys_a0_86",		"sys_a0_87",
	"sys_a0_88",		"sys_a0_89",	"sys_a0_8a",		"sys_a0_8b",
	"sys_a0_8c",		"sys_a0_8d",	"sys_a0_8e",		"sys_a0_8f",
// 0x90
	"sys_a0_90",		"sys_a0_91",	"sys_a0_92",		"sys_a0_93",
	"sys_a0_94",		"sys_a0_95",	"AddCDROMDevice",	"AddMemCardDevide",
	"DisableKernelIORedirection",		"EnableKernelIORedirection", "sys_a0_9a", "sys_a0_9b",
	"SetConf",			"GetConf",		"sys_a0_9e",		"SetMem",
// 0xa0
	"_boot",			"SystemError",	"EnqueueCdIntr",	"DequeueCdIntr",
	"sys_a0_a4",		"ReadSector",	"get_cd_status",	"bufs_cb_0",
	"bufs_cb_1",		"bufs_cb_2",	"bufs_cb_3",		"_card_info",
	"_card_load",		"_card_auto",	"bufs_cd_4",		"sys_a0_af",
// 0xb0
	"sys_a0_b0",		"sys_a0_b1",	"do_a_long_jmp",	"sys_a0_b3",
	"?? sub_function",
};

//////////////////////////////////////////////////////////////////////////////////////////
//
const char *biosB0n[256] = {
// 0x00
	"SysMalloc",		"sys_b0_01",	"sys_b0_02",	"sys_b0_03",
	"sys_b0_04",		"sys_b0_05",	"sys_b0_06",	"DeliverEvent",
	"OpenEvent",		"CloseEvent",	"WaitEvent",	"TestEvent",
	"EnableEvent",		"DisableEvent",	"OpenTh",		"CloseTh",
// 0x10
	"ChangeTh",			"sys_b0_11",	"InitPAD",		"StartPAD",
	"StopPAD",			"PAD_init",		"PAD_dr",		"ReturnFromExecption",
	"ResetEntryInt",	"HookEntryInt",	"sys_b0_1a",	"sys_b0_1b",
	"sys_b0_1c",		"sys_b0_1d",	"sys_b0_1e",	"sys_b0_1f",
// 0x20
	"UnDeliverEvent",	"sys_b0_21",	"sys_b0_22",	"sys_b0_23",
	"sys_b0_24",		"sys_b0_25",	"sys_b0_26",	"sys_b0_27",
	"sys_b0_28",		"sys_b0_29",	"sys_b0_2a",	"sys_b0_2b",
	"sys_b0_2c",		"sys_b0_2d",	"sys_b0_2e",	"sys_b0_2f",
// 0x30
	"sys_b0_30",		"sys_b0_31",	"open",			"lseek",
	"read",				"write",		"close",		"ioctl",
	"exit",				"sys_b0_39",	"getc",			"putc",
	"getchar",			"putchar",		"gets",			"puts",
// 0x40
	"cd",				"format",		"firstfile",	"nextfile",
	"rename",			"delete",		"undelete",		"AddDevice",
	"RemoteDevice",		"PrintInstalledDevices", "InitCARD", "StartCARD",
	"StopCARD",			"sys_b0_4d",	"_card_write",	"_card_read",
// 0x50
	"_new_card",		"Krom2RawAdd",	"sys_b0_52",	"sys_b0_53",
	"_get_errno",		"_get_error",	"GetC0Table",	"GetB0Table",
	"_card_chan",		"sys_b0_59",	"sys_b0_5a",	"ChangeClearPAD",
	"_card_status",		"_card_wait",
};

//////////////////////////////////////////////////////////////////////////////////////////
//
const char *biosC0n[256] = {
// 0x00
	"InitRCnt",			  "InitException",		"SysEnqIntRP",		"SysDeqIntRP",
	"get_free_EvCB_slot", "get_free_TCB_slot",	"ExceptionHandler",	"InstallExeptionHandler",
	"SysInitMemory",	  "SysInitKMem",		"ChangeClearRCnt",	"SystemError",
	"InitDefInt",		  "sys_c0_0d",			"sys_c0_0e",		"sys_c0_0f",
// 0x10
	"sys_c0_10",		  "sys_c0_11",			"InstallDevices",	"FlushStfInOutPut",
	"sys_c0_14",		  "_cdevinput",			"_cdevscan",		"_circgetc",
	"_circputc",		  "ioabort",			"sys_c0_1a",		"KernelRedirect",
	"PatchAOTable",
};

//////////////////////////////////////////////////////////////////////////////////////////
//
const char* intrname[] =
{
	"INT_VBLANK",   "INT_GM",       "INT_CDROM",   "INT_DMA",	//00
	"INT_RTC0",     "INT_RTC1",     "INT_RTC2",    "INT_SIO0",	//04
	"INT_SIO1",     "INT_SPU",      "INT_PIO",     "INT_EVBLANK",	//08
	"INT_DVD",      "INT_PCMCIA",   "INT_RTC3",    "INT_RTC4",	//0C
	"INT_RTC5",     "INT_SIO2",     "INT_HTR0",    "INT_HTR1",	//10
	"INT_HTR2",     "INT_HTR3",     "INT_USB",     "INT_EXTR",	//14
	"INT_FWRE",     "INT_FDMA",     "INT_1A",      "INT_1B",	//18
	"INT_1C",       "INT_1D",       "INT_1E",      "INT_1F",	//1C
	"INT_dmaMDECi", "INT_dmaMDECo", "INT_dmaGPU",  "INT_dmaCD",	//20
	"INT_dmaSPU",   "INT_dmaPIO",   "INT_dmaOTC",  "INT_dmaBERR",	//24
	"INT_dmaSPU2",  "INT_dma8",     "INT_dmaSIF0", "INT_dmaSIF1",	//28
	"INT_dmaSIO2i", "INT_dmaSIO2o", "INT_2E",      "INT_2F",	//2C
	"INT_30",       "INT_31",       "INT_32",      "INT_33",	//30
	"INT_34",       "INT_35",       "INT_36",      "INT_37",	//34
	"INT_38",       "INT_39",       "INT_3A",      "INT_3B",	//38
	"INT_3C",       "INT_3D",       "INT_3E",      "INT_3F",	//3C
	"INT_MAX"							//40
};

//////////////////////////////////////////////////////////////////////////////////////////
//
//#define r0 (iopRegs.GPR_r0)
#define at (iopRegs[GPR_at].UL)
#define v0 (iopRegs[GPR_v0].UL)
#define v1 (iopRegs[GPR_v1].UL)
#define a0 (iopRegs[GPR_a0].UL)
#define a1 (iopRegs[GPR_a1].UL)
#define a2 (iopRegs[GPR_a2].UL)
#define a3 (iopRegs[GPR_a3].UL)
#define t0 (iopRegs[GPR_t0].UL)
#define t1 (iopRegs[GPR_t1].UL)
#define t2 (iopRegs[GPR_t2].UL)
#define t3 (iopRegs[GPR_t3].UL)
#define t4 (iopRegs[GPR_t4].UL)
#define t5 (iopRegs[GPR_t5].UL)
#define t6 (iopRegs[GPR_t6].UL)
#define t7 (iopRegs[GPR_t7].UL)
#define s0 (iopRegs[GPR_s0].UL)
#define s1 (iopRegs[GPR_s1].UL)
#define s2 (iopRegs[GPR_s2].UL)
#define s3 (iopRegs[GPR_s3].UL)
#define s4 (iopRegs[GPR_s4].UL)
#define s5 (iopRegs[GPR_s5].UL)
#define s6 (iopRegs[GPR_s6].UL)
#define s7 (iopRegs[GPR_s7].UL)
#define t8 (iopRegs[GPR_t6].UL)
#define t9 (iopRegs[GPR_t7].UL)
#define k0 (iopRegs[GPR_k0].UL)
#define k1 (iopRegs[GPR_k1].UL)
#define gp (iopRegs[GPR_gp].UL)
#define sp (iopRegs[GPR_sp].UL)
#define fp (iopRegs[GPR_s8].UL)
#define ra (iopRegs[GPR_ra].UL)
#define pc0 (iopRegs.pc)

#define Ra0 (iopVirtMemR<char>(a0))
#define Ra1 (iopVirtMemR<char>(a1))
#define Ra2 (iopVirtMemR<char>(a2))
#define Ra3 (iopVirtMemR<char>(a3))
#define Rv0 (iopVirtMemR<char>(v0))
#define Rsp (iopVirtMemR<char>(sp))

//////////////////////////////////////////////////////////////////////////////////////////
//
void bios_write()  // 0x35/0x03
{
	if (a0 == 1)  // stdout
	{
		const char *ptr = Ra1;

		while (a2 > 0) 
		{
			Console::Write("%c", params *ptr++);
			a2--;
		}
	}
	else
	{
		PSXBIOS_LOG("bios_%s: %x,%x,%x", biosB0n[0x35], a0, a1, a2);

		v0 = -1;
	}
	pc0 = ra;
}

void bios_printf() // 3f
{
	char tmp[1024], tmp2[1024];
	u32 save[4];
	char *ptmp = tmp;
	int n=1, i=0, j = 0;

	memcpy(save, iopVirtMemR<void>(sp), 4*4);

	iopMemWrite32(sp, a0);
	iopMemWrite32(sp + 4, a1);
	iopMemWrite32(sp + 8, a2);
	iopMemWrite32(sp + 12, a3);

	// old code used phys... is tlb more correct?
	//psxMu32(sp) = a0;
	//psxMu32(sp + 4) = a1;
	//psxMu32(sp + 8) = a2;
	//psxMu32(sp + 12) = a3;+

	while (Ra0[i]) 
	{
		switch (Ra0[i]) 
		{
			case '%':
				j = 0;
				tmp2[j++] = '%';
			
_start:
				switch (Ra0[++i]) 
				{
					case '.':
					case 'l':
						tmp2[j++] = Ra0[i]; 
						goto _start;
					default:
						if (Ra0[i] >= '0' && Ra0[i] <= '9') 
						{
							tmp2[j++] = Ra0[i];
							goto _start;
						}
						break;
				}
				
				tmp2[j++] = Ra0[i];
				tmp2[j] = 0;

				switch (Ra0[i]) 
				{
					case 'f': case 'F':
						ptmp+= sprintf(ptmp, tmp2, (float)iopMemRead32(sp + n * 4));
						n++; 
						break;
					
					case 'a': case 'A':
					case 'e': case 'E':
					case 'g': case 'G':
						ptmp+= sprintf(ptmp, tmp2, (double)iopMemRead32(sp + n * 4)); 
						n++;
						break;
					
					case 'p':
					case 'i':
					case 'd': case 'D':
					case 'o': case 'O':
					case 'x': case 'X':
						ptmp+= sprintf(ptmp, tmp2, (u32)iopMemRead32(sp + n * 4)); 
						n++; 
						break;
					
					case 'c':
						ptmp+= sprintf(ptmp, tmp2, (u8)iopMemRead32(sp + n * 4)); 
						n++; 
						break;
					
					case 's':
						ptmp+= sprintf(ptmp, tmp2, iopVirtMemR<char>(iopMemRead32(sp + n * 4)));
						n++; 
						break;
					
					case '%':
						*ptmp++ = Ra0[i];
						break;
					
					default:
						break;
				}
				i++;
				break;
				
			default:
				*ptmp++ = Ra0[i++];
				break;
		}
	}
	*ptmp = 0;

	// Note: Use Read to obtain a write pointer here, since we're just writing back the 
	// temp buffer we saved earlier.
	memcpy( (void*)iopVirtMemR<void>(sp), save, 4*4);
	Console::Write( Color_Cyan, "%s", params tmp);
	pc0 = ra;
}

void bios_putchar ()  // 3d
{
    Console::Write( Color_Cyan, "%c", params a0 );
    pc0 = ra;
}

void bios_puts ()  // 3e/3f
{
    Console::Write( Color_Cyan, Ra0 );
    pc0 = ra;
}

void (*biosA0[256])();
void (*biosB0[256])();
void (*biosC0[256])();

void psxBiosInit() 
{
	int i;

	for(i = 0; i < 256; i++) 
	{
		biosA0[i] = NULL;
		biosB0[i] = NULL;
		biosC0[i] = NULL;
	}
	biosA0[0x3e] = bios_puts;
	biosA0[0x3f] = bios_printf;

	biosB0[0x3d] = bios_putchar;
	biosB0[0x3f] = bios_puts;

}

void psxBiosShutdown() 
{
}

void zeroEx()
{
#ifdef PCSX2_DEVBUILD
	u32 pc;
	u32 code;
	const char *lib;
	const char *fname = NULL;
	int i;

	if (!Config.PsxOut) return;

	pc = iopRegs.pc;
	while (iopMemDirectRead32(pc) != 0x41e00000) pc-=4;

	lib  = iopVirtMemR<char>(pc+12);
	code = iopMemDirectRead32(iopRegs.pc) & 0xffff;

	for (i=0; i<IRXLIBS; i++) {
		if (!strncmp(lib, irxlibs[i].name, 8)) {
			if (code >= (u32)irxlibs[i].maxn) break;

			fname = irxlibs[i].names[code];
            //if( strcmp(fname, "setIOPrcvaddr") == 0 ) {
//                SysPrintf("yo\n");
//                varLog |= 0x100000;
//                Log = 1;
//            }
            break;
		}
	}

	{
		char libz[9]; memcpy(libz, lib, 8); libz[8]=0;
		PSXBIOS_LOG(
			"%s: %s (%x) (%x, %x, %x, %x)",
			libz, fname == NULL ? "unknown" : fname, code,
			a0, a1, a2, a3
		);
	}

//	Log=0;
//	if (!strcmp(lib, "intrman") && code == 0x11) Log=1;
//	if (!strcmp(lib, "sifman") && code == 0x5) Log=1;
//	if (!strcmp(lib, "sifcmd") && code == 0x4) Log=1;
//	if (!strcmp(lib, "thbase") && code == 0x6) Log=1;
/*
	if (!strcmp(lib, "sifcmd") && code == 0xe) {
		branchPC = iopRegs.GPR.n.ra;
		iopRegs.GPR.n.v0 = 0;
		return;
	}
*/
	if (!strncmp(lib, "ioman", 5) && code == 7) {
		if (a0 == 1) {
			pc = iopRegs.pc;
			bios_write();
			iopRegs.pc = pc;
		}
	}

	if (!strncmp(lib, "sysmem", 6) && code == 0xe) {
		bios_printf();
		iopRegs.pc = ra;
	}

	if (!strncmp(lib, "loadcore", 8) && code == 6) {
		DevCon::WriteLn("loadcore RegisterLibraryEntries (%x): %8.8s", params iopRegs.pc, iopVirtMemR<char>(a0+12));
	}

	if (!strncmp(lib, "intrman", 7) && code == 4) {
		DevCon::WriteLn("intrman RegisterIntrHandler (%x): intr %s, handler %x", params iopRegs.pc, intrname[a0], a2);
	}

	if (!strncmp(lib, "sifcmd", 6) && code == 17) {
		DevCon::WriteLn("sifcmd sceSifRegisterRpc (%x): rpc_id %x", params iopRegs.pc, a1);
	}

	if (!strncmp(lib, "sysclib", 8))
	{
		switch (code)
		{
			case 0x16: // strcmp
				PSXBIOS_LOG(" \"%s\": \"%s\"", Ra0, Ra1);
				break;

			case 0x1e: // strncpy
				PSXBIOS_LOG(" \"%s\"", Ra1);
				break;
		}
	}

/*	iopRegs.pc = branchPC;
	pc = iopRegs.GPR.n.ra;
	while (iopRegs.pc != pc) psxCpu->ExecuteBlock();

	PSXBIOS_LOG("%s: %s (%x) END", lib, fname == NULL ? "unknown" : fname, code);*/
#endif

}
/*/==========================================CALL LOG
char* getName(char *file, u32 addr){
	FILE *f; u32 a;
	static char name[100];

	f=fopen(file, "r");
	if (!f)
		name[0]=0;
	else{
		while (!feof(f)){
			fscanf(f, "%08X %s\n", &a, name);
			if (a==addr)break;
		}
		fclose(f);
	}
	return name;
}

void spyFunctions(){
	register irxImageInfo *iii;
	if (iopRegs.pc >= 0x200000)	return;
	for (iii=(irxImageInfo*)PSXM(0x800); iii && iii->text_size;
		iii=iii->next ? (irxImageInfo*)PSXM(iii->next) : NULL)
			if (iii->vaddr<=iopRegs.pc && iopRegs.pc<iii->vaddr+iii->text_size+iii->data_size+iii->bss_size){
				if (strcmp("secrman_for_cex", PSXM(iii->name))==0){
					char *name=getName("secrman.fun", iopRegs.pc-iii->vaddr);
					if (strncmp("__push_params", name, 13)==0){
						PAD_LOG(PSXM(iopRegs.GPR.n.a0), iopRegs.GPR.n.a1, iopRegs.GPR.n.a2, iopRegs.GPR.n.a3);
					}else{
						PAD_LOG("secrman: %s (ra=%06X cycle=%d)", name, iopRegs.GPR.n.ra-iii->vaddr, iopRegs.cycle);}}else
				if (strcmp("mcman", PSXM(iii->name))==0){
					PAD_LOG("mcman: %s (ra=%06X cycle=%d)",  getName("mcman.fun", iopRegs.pc-iii->vaddr), iopRegs.GPR.n.ra-iii->vaddr, iopRegs.cycle);}else
				if (strcmp("padman", PSXM(iii->name))==0){
					PAD_LOG("padman: %s (ra=%06X cycle=%d)",  getName("padman.fun", iopRegs.pc-iii->vaddr), iopRegs.GPR.n.ra-iii->vaddr, iopRegs.cycle);}else
				if (strcmp("sio2man", PSXM(iii->name))==0){
					PAD_LOG("sio2man: %s (ra=%06X cycle=%d)", getName("sio2man.fun", iopRegs.pc-iii->vaddr), iopRegs.GPR.n.ra-iii->vaddr, iopRegs.cycle);}
				break;
			}
}
*/

}	// end namespace R3000A