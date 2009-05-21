AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4 --install

pcsx2_dir = $(top_srcdir)/..
common_dir = $(pcsx2_dir)/common/include
third_dir = $(pcsx2_dir)/3rdparty
cdvd_dir = $(top_srcdir)/CDVD
x86_dir = $(top_srcdir)/x86
rdebug_dir = $(top_srcdir)/RDebug
debugtools_dir = $(top_srcdir)/DebugTools
ipu_dir = $(top_srcdir)/IPU
linux_dir = $(top_srcdir)/Linux

INCLUDES = -I$(x86_dir) -I$(cdvd_dir) -I$(common_dir) -I$(third_dir)
noinst_LIBRARIES = libpcsx2.a

libpcsx2_a_SOURCES = \
AlignedMalloc.cpp  Decode_XA.cpp  HwRead.cpp       MTGS.cpp                  Sio.cpp             VUflags.cpp \
Dump.cpp       HwWrite.cpp                     SourceLog.cpp       VUmicroMem.cpp \
Elfheader.cpp  Interpreter.cpp  Memory.cpp             R5900.cpp               Stats.cpp           VUops.cpp \
FPU.cpp        IopBios.cpp      MemoryCard.cpp         R5900OpcodeImpl.cpp     System.cpp          Vif.cpp \
COP0.cpp           FPU2.cpp       IopCounters.cpp  Misc.cpp               R5900OpcodeTables.cpp   ThreadTools.cpp     VifDma.cpp \
COP2.cpp           FiFo.cpp       IopDma.cpp       Patch.cpp              RecoverySystem.cpp      VU0.cpp             vssprintf.cpp \
Cache.cpp          GS.cpp         IopHw.cpp        PathUtils.cpp          SPR.cpp                 VU0micro.cpp        vtlb.cpp \
Gif.cpp        IopMem.cpp       Plugins.cpp            SaveState.cpp           VU0microInterp.cpp  xmlpatchloader.cpp \
Console.cpp         IopSio2.cpp      PrecompiledHeader.cpp  Saveslots.cpp           VU1micro.cpp \
Counters.cpp       Hw.cpp         MMI.cpp                       Sif.cpp                 VU1microInterp.cpp
#R3000A.cpp R3000AInterpreter.cpp R3000AOpcodeTables.cpp  Mdec.cpp HashTools.cpp


libpcsx2_a_SOURCES += \
ps2/Iop/IopHwRead.cpp ps2/Iop/IopHwWrite.cpp ps2/Iop/IopHw_Internal.h

libpcsx2_a_SOURCES += \
Common.h       IopDma.h      MemoryCard.h   PrecompiledHeader.h  SPR.h        Stats.h        VUops.h \
Counters.h    HostGui.h      IopHw.h       MemoryTypes.h               SafeArray.h  StringUtils.h  Vif.h \
Decode_XA.h   Hw.h           IopMem.h      Misc.h            SamplProf.h  System.h       VifDma.h \
Dump.h        IopBios.h      IopSio2.h     NakedAsm.h     R5900.h              SaveState.h  Threading.h    cheatscpp.h \
COP0.h        Elfheader.h   IopBios2.h             Patch.h        R5900Exceptions.h    Sif.h        VU.h           vtlb.h \
Cache.h       Exceptions.h  IopCommon.h    MemcpyFast.h  Paths.h        R5900OpcodeTables.h  Sifcmd.h     VUflags.h \
GS.h          IopCounters.h  Memory.h      Plugins.h      RedtapeWindows.h     Sio.h        VUmicro.h
#R3000A.h  R3000Exceptions.h Mdec.h  HashMap.h 

libpcsx2_a_SOURCES += \
R3000air/R3000air.cpp  R3000air/R3000airDiagnostic.cpp   R3000air/R3000airInterpreter.cpp  R3000air/R3000airOpcodeTables.inl \
R3000air/R3000air.h    R3000air/R3000airInstruction.inl  R3000air/R3000airOpcodeImpl.inl

libpcsx2_a_SOURCES += \
R3000air/dynarec/iR3000air.cpp  R3000air/dynarec/iR3000air.h  R3000air/dynarec/iR3000airArithmetic.cpp  \
R3000air/dynarec/iR3000airBranch.cpp  R3000air/dynarec/iR3000airIntermediate.cpp  R3000air/dynarec/iR3000airMemory.cpp

SUBDIRS = x86 . DebugTools IPU RDebug tinyxml Linux
