
; PCSX2 NSIS installer script
; Copyright 2009-2010  PCSX2 Dev Team

; Application version, changed for each release to match the version

; Uncomment this to create a package that includes binaries and binary dependencies only.

!ifndef INC_PLUGINS
  !define INC_PLUGINS   1
!endif

!ifndef INC_CRT_2008
  !define INC_CRT_2008  0
!endif

!ifndef INC_CRT_2010
  !define INC_CRT_2010  1
!endif

!ifndef INC_LANGS
  !define INC_LANGS     0
!endif

!include "SharedSettings.nsh"

; The name of the installer
Name "${APP_NAME}"

OutFile "${APP_FILENAME}-setup.exe"

; The default installation directory
InstallDir "$PROGRAMFILES\PCSX2 ${APP_VERSION}"

; Registry key to check for directory (so if you install again, it will 
; overwrite the old one automatically)
InstallDirRegKey ${INSTDIR_REG_ROOT} "Software\PCSX2" "Install_Dir"

; These defines are dependent on NSIS vars assigned above.

!define APP_EXE          "$INSTDIR\${APP_FILENAME}.exe"
!define INSTDIR_REG_KEY  "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_FILENAME}"

Var DirectXSetupError


!include "MUI2.nsh"
!include "AdvUninstLog.nsh"

; UNINSTALL.LOG_OPEN_INSTALL_SECTION {section_name}
; UNINSTALL.LOG_CLOSE_INSTALL_SECTION {section_name}
;
; Advanced Uninstaller Extension: This allows us to safely log to arbitrary "sections" of
; installation of our choosing, without having to rely on $OUTDIR (which is how the default
; provided LOG_OPEN_INSTALL works).  In other words, different files in the same folder can
; be added to different install lists. :)
;
!macro UNINSTALL.LOG_OPEN_INSTALL_SECTION SectionName
  !verbose push
     !verbose ${UNINST_LOG_VERBOSE}

        StrCmp $unlog_error "error" +2
        ${uninstall.log_install} "${EXCLU_LIST}" "${UNINST_DAT}" "${SectionName}"

  !verbose pop
!macroend

!macro UNINSTALL.LOG_CLOSE_INSTALL_SECTION SectionName
  !verbose push
     !verbose ${UNINST_LOG_VERBOSE}

   !define ID ${__LINE__}

        ${uninstall.log_install} "${UNLOG_PART}${ID}" "${EXCLU_LIST}" "${SectionName}"
        ${uninstall.log_mergeID} "${UNLOG_PART}${ID}"

   !undef ID ${__LINE__}

  !verbose pop
!macroend

; =======================================================================
;                          Vista/Win7 UAC Stuff
; =======================================================================

!include "IsUserAdmin.nsi"

; Allow admin-rights PCSX2 users to be hardcore!
AllowRootDirInstall true

; FIXME !!
; Request application privileges for Windows Vista/7; I'd love for this to be sensible about which
; execution level it requests, but UAC is breaking my mind.  I included some code for User type
; detection in function IsUserAdmin, but not really using it constructively yet.  (see also our
; uses of SetShellVarContext in the installer sections) 
RequestExecutionLevel admin

; This defines the Advanced Uninstaller mode of operation...
!insertmacro UNATTENDED_UNINSTALL

!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "banner.bmp"
;!define MUI_COMPONENTSPAGE_NODESC
!define MUI_COMPONENTSPAGE_SMALLDESC

!insertmacro MUI_PAGE_COMPONENTS 
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
  
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_COMPONENTS
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

!include "ApplyExeProps.nsh"
!include "SharedRedtape.nsh"

; =======================================================================
;                            Installer Sections
; =======================================================================

; -----------------------------------------------------------------------
; Basic section (emulation proper)
Section "!${APP_NAME} (required)" SEC_CORE

  SectionIn RO

!include "SectionCoreReqs.nsh"

  ; ------------------------------------------
  ;          -- Plugins Section --
  ; ------------------------------------------

!if ${INC_PLUGINS} > 0

  SetOutPath "$INSTDIR\Plugins"
  !insertmacro UNINSTALL.LOG_OPEN_INSTALL

    File /nonfatal /oname=gsdx-sse2-r${SVNREV_GSDX}.dll    ..\bin\Plugins\gsdx-sse2.dll
    File /nonfatal /oname=gsdx-ssse3-r${SVNREV_GSDX}.dll   ..\bin\Plugins\gsdx-ssse3.dll 
    File /nonfatal /oname=gsdx-sse4-r${SVNREV_GSDX}.dll    ..\bin\Plugins\gsdx-sse4.dll
    File /nonfatal /oname=zerogs-r${SVNREV_ZEROGS}.dll     ..\bin\Plugins\zerogs.dll
  
    File /nonfatal /oname=spu2-x-r${SVNREV_SPU2X}.dll      ..\bin\Plugins\spu2-x.dll
    File /nonfatal /oname=zerospu2-r${SVNREV_ZEROSPU2}.dll ..\bin\Plugins\zerospu2.dll
  
    File /nonfatal /oname=cdvdiso-r${SVNREV_CDVDISO}.dll   ..\bin\Plugins\cdvdiso.dll
    File                                                   ..\bin\Plugins\cdvdGigaherz.dll
  
    File /nonfatal /oname=lilypad-r${SVNREV_LILYPAD}.dll   ..\bin\Plugins\lilypad.dll
    File                                                   ..\bin\Plugins\PadSSSPSX.dll
  
  ;File                                                   ..\bin\Plugins\FWlinuz.dll

  !insertmacro UNINSTALL.LOG_CLOSE_INSTALL

!endif

SectionEnd

; -----------------------------------------------------------------------
; Start Menu - Optional section (can be disabled by the user)
Section "Start Menu Shortcuts" SEC_STARTMENU

  ; CreateShortCut gets the working directory from OutPath
  SetOutPath "$INSTDIR"
  
  CreateDirectory "$SMPROGRAMS\PCSX2"
  CreateShortCut "$SMPROGRAMS\PCSX2\Uninstall ${APP_NAME}.lnk"  "${UNINST_EXE}"      ""    "${UNINST_EXE}"    0
  CreateShortCut "$SMPROGRAMS\PCSX2\${APP_NAME}.lnk"            "${APP_EXE}"         ""    "${APP_EXE}"       0

  ;IfFileExists ..\bin\pcsx2-dev.exe 0 +2
  ;  CreateShortCut "PCSX2\pcsx2-dev-r${SVNREV}.lnk"  "$INSTDIR\pcsx2-dev-r${SVNREV}.exe"  "" "$INSTDIR\pcsx2-dev-r${SVNREV}.exe" 0 "" "" \
  ;    "PCSX2 Devel (has additional logging support)"

SectionEnd

; -----------------------------------------------------------------------
; Desktop Icon - Optional section (can be disabled by the user)
Section "Desktop Shortcut" SEC_DESKTOP

  ; CreateShortCut gets the working directory from OutPath
  SetOutPath "$INSTDIR"
  
  CreateShortCut "$DESKTOP\${APP_NAME}.lnk"            "${APP_EXE}"      "" "${APP_EXE}"     0 "" "" "A Playstation 2 Emulator"
    
SectionEnd

; -----------------------------------------------------------------------
; MSVC Redistributable - required if the user does not already have it
; Note: if your NSIS generates an error here it means you need to download the latest
; visual studio redist package from microsoft.  Any redist 2008/SP1 or newer will do.
;
; IMPORTANT: Online references for how to detect the presence of the VS2008 redists LIE.
; None of the methods are reliable, because the registry keys placed by the MSI installer
; vary depending on operating system *and* MSI installer version (youch).
;
!if ${INC_CRT_2008} > 0
Section "Microsoft Visual C++ 2008 SP1 Redist (required)"  SEC_CRT2008

  SectionIn RO

  ; Downloaded from:
  ;  http://download.microsoft.com/download/d/d/9/dd9a82d0-52ef-40db-8dab-795376989c03/vcredist_x86.exe
 
  SetOutPath "$TEMP"
  File "vcredist_2008_sp1_x86.exe"
  DetailPrint "Running Visual C++ 2008 SP1 Redistributable Setup..."
  ExecWait '"$TEMP\vcredist_2008_sp1_x86.exe" /qb'
  DetailPrint "Finished Visual C++ 2008 SP1 Redistributable Setup"
  
  Delete "$TEMP\vcredist_2008_sp1_x86.exe"

SectionEnd
!endif

!if ${INC_CRT_2010} > 0
Section "Microsoft Visual C++ 2010 Redist (recommended)" SEC_CRT2010

  ;SectionIn RO

  ; Detection made easy: Unlike previous redists, VC2010 now generates a platform
  ; independent key for checking availability.
  
  ; Downloaded from:
  ;   http://download.microsoft.com/download/5/B/C/5BC5DBB3-652D-4DCE-B14A-475AB85EEF6E/vcredist_x86.exe

  ReadRegDword $R0 HKLM "SOFTWARE\Microsoft\VisualStudio\10.0\VC\VCRedist\x86" "Installed"
  IfErrors done
  StrCmp $R0 "1" done

  SetOutPath "$TEMP"
  File "vcredist_2010_x86.exe"
  DetailPrint "Running Visual C++ 2010 SP1 Redistributable Setup..."
  ExecWait '"$TEMP\vcredist_2010_x86.exe" /qb'
  DetailPrint "Finished Visual C++ 2010 SP1 Redistributable Setup"
  
  Delete "$TEMP\vcredist_2010_x86.exe"

done:
SectionEnd
!endif

; -----------------------------------------------------------------------
; This section needs to be last, so that in case it fails, the rest of the program will
; be installed cleanly.
; 
; This section could be optional, but why not?  It's pretty painless to double-check that
; all the libraries are up-to-date.
;
Section "DirectX Web Setup (recommended)" SEC_DIRECTX
                                                                              
 ;SectionIn RO

 SetOutPath "$TEMP"
 File "dxwebsetup.exe"
 DetailPrint "Running DirectX Web Setup..."
 ExecWait '"$TEMP\dxwebsetup.exe" /Q' $DirectXSetupError
 DetailPrint "Finished DirectX Web Setup"                                     
                                                                              
 Delete "$TEMP\dxwebsetup.exe"

SectionEnd

; =======================================================================
;                           Un.Installer Sections
; =======================================================================

; -----------------------------------------------------------------------
Section "Un.Core Executables ${APP_NAME}"

  SetShellVarContext all

  !insertmacro UNINSTALL.LOG_UNINSTALL "$INSTDIR"

  ; Remove registry keys (but only the ones related to the installer -- user options remain)
  DeleteRegKey HKLM "${INSTDIR_REG_KEY}"

  Call un.removeShorties

SectionEnd

; -----------------------------------------------------------------------
Section "Un.Shared Components (DLLs, Languages, etc)"

  MessageBox MB_YESNO "WARNING!  If you have multiple versions of PCSX2 installed, removing all shared files will probably break them.  Are you sure you want to proceed?" \
    IDYES true IDNO false

  true:
    !insertmacro UNINSTALL.LOG_UNINSTALL "$INSTDIR\Langs"
    !insertmacro UNINSTALL.LOG_UNINSTALL "$INSTDIR\Plugins"

    ; Kill the entire PCSX2 registry key.
    DeleteRegKey ${INSTDIR_REG_ROOT} Software\PCSX2

  false:
    ; User cancelled -- do nothing!!

SectionEnd

LangString DESC_CORE       ${LANG_ENGLISH} "Core components (binaries, plugins, languages, etc)."

LangString DESC_STARTMENU  ${LANG_ENGLISH} "Adds shortcuts for PCSX2 to the start menu (all users)."
LangString DESC_DESKTOP    ${LANG_ENGLISH} "Adds a shortcut for PCSX2 to the desktop (all users)."

LangString DESC_CRT2008    ${LANG_ENGLISH} "The 2008 Redist is required by the PCSX2 binaries packaged in this installer."
LangString DESC_CRT2010    ${LANG_ENGLISH} "The 2010 Redist will be used by future PCSX2 plugins and updates, but is not (yet) necessary."
LangString DESC_DIRECTX    ${LANG_ENGLISH} "Only uncheck this if you are quite certain your Direct3D runtimes are up to date."

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_CORE}        $(DESC_CORE)
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_STARTMENU}   $(DESC_STARTMENU)
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_DESKTOP}     $(DESC_DESKTOP)

!if ${INC_CRT_2008} > 0
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_CRT2008}     $(DESC_CRT2008)
!endif

!if ${INC_CRT_2010} > 0
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_CRT2010}     $(DESC_CRT2010)
!endif

  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_DIRECTX}     $(DESC_DIRECTX)
!insertmacro MUI_FUNCTION_DESCRIPTION_END
