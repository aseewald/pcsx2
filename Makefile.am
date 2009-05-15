AUTOMAKE_OPTIONS = foreign
INCLUDES = -I@srcdir@/x86/  -I@srcdir@/../common/include -I@srcdir@/../3rdparty
noinst_LIBRARIES = libpcsx2.a

libpcsx2_a_SOURCES = \
AlignedMalloc.cpp  Decode_XA.cpp  HwRead.cpp       MTGS.cpp                  Sio.cpp             VUflags.cpp \
CDVD.cpp           Dump.cpp       HwWrite.cpp                     SourceLog.cpp       VUmicroMem.cpp \
CDVDiso.cpp        Elfheader.cpp  Interpreter.cpp  Memory.cpp             R5900.cpp               Stats.cpp           VUops.cpp \
CDVDisodrv.cpp     FPU.cpp        IopBios.cpp      MemoryCard.cpp         R5900OpcodeImpl.cpp     System.cpp          Vif.cpp \
COP0.cpp           FPU2.cpp       IopCounters.cpp  Misc.cpp               R5900OpcodeTables.cpp   ThreadTools.cpp     VifDma.cpp \
COP2.cpp           FiFo.cpp       IopDma.cpp       Patch.cpp              RecoverySystem.cpp      VU0.cpp             vssprintf.cpp \
Cache.cpp          GS.cpp         IopHw.cpp        PathUtils.cpp          SPR.cpp                 VU0micro.cpp        vtlb.cpp \
CdRom.cpp          Gif.cpp        IopMem.cpp       Plugins.cpp            SaveState.cpp           VU0microInterp.cpp  xmlpatchloader.cpp \
Console.cpp         IopSio2.cpp      PrecompiledHeader.cpp  Saveslots.cpp           VU1micro.cpp \
Counters.cpp       Hw.cpp         MMI.cpp                       Sif.cpp                 VU1microInterp.cpp
#R3000A.cpp R3000AInterpreter.cpp R3000AOpcodeTables.cpp  Mdec.cpp HashTools.cpp

libpcsx2_a_SOURCES += \	
ps2/Iop/IopHwRead.cpp ps2/Iop/IopHwWrite.cpp ps2/Iop/IopHw_Internal.h

libpcsx2_a_SOURCES += \
CDVD.h        Common.h       IopDma.h      MemoryCard.h   PrecompiledHeader.h  SPR.h        Stats.h        VUops.h \
CDVDiso.h     Counters.h    HostGui.h      IopHw.h       MemoryTypes.h               SafeArray.h  StringUtils.h  Vif.h \
CDVDisodrv.h  Decode_XA.h   Hw.h           IopMem.h      Misc.h            SamplProf.h  System.h       VifDma.h \
CDVDlib.h     Dump.h        IopBios.h      IopSio2.h     NakedAsm.h     R5900.h              SaveState.h  Threading.h    cheatscpp.h \
COP0.h        Elfheader.h   IopBios2.h             Patch.h        R5900Exceptions.h    Sif.h        VU.h           vtlb.h \
Cache.h       Exceptions.h  IopCommon.h    MemcpyFast.h  Paths.h        R5900OpcodeTables.h  Sifcmd.h     VUflags.h \
CdRom.h       GS.h          IopCounters.h  Memory.h      Plugins.h      RedtapeWindows.h     Sio.h        VUmicro.h
#R3000A.h  R3000Exceptions.h Mdec.h  HashMap.h 

libpcsx2_a_SOURCES += \
R3000air/R3000air.cpp  R3000air/R3000airDiagnostic.cpp   R3000air/R3000airInterpreter.cpp  R3000air/R3000airOpcodeTables.inl \
R3000air/R3000air.h    R3000air/R3000airInstruction.inl  R3000air/R3000airOpcodeImpl.inl

libpcsx2_a_SOURCES += \
R3000air/dynarec/iR3000air.cpp  R3000air/dynarec/iR3000air.h  R3000air/dynarec/iR3000airArithmetic.cpp  \
R3000air/dynarec/iR3000airBranch.cpp  R3000air/dynarec/iR3000airIntermediate.cpp  R3000air/dynarec/iR3000airMemory.cpp

SUBDIRS = x86 . DebugTools IPU RDebug tinyxml Linux