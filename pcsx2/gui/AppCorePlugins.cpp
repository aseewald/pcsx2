/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
 * 
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"
#include "App.h"
#include "AppSaveStates.h"
#include "GSFrame.h"

#include <wx/dir.h>
#include <wx/file.h>

#include "Plugins.h"
#include "GS.h"
#include "HostGui.h"
#include "AppConfig.h"

using namespace Threading;

// The GS plugin needs to be opened to save/load the state during plugin configuration, but
// the window shouldn't. This blocks it. :)
static bool s_DisableGsWindow = false;

__aligned16 AppPluginManager CorePlugins;

PluginManager& GetCorePlugins()
{
	return CorePlugins;
}


// --------------------------------------------------------------------------------------
//  CorePluginsEvent
// --------------------------------------------------------------------------------------
class CorePluginsEvent : public pxInvokeActionEvent
{
	typedef pxInvokeActionEvent _parent;

protected:
	PluginEventType		m_evt;

public:
	virtual ~CorePluginsEvent() throw() {}
	CorePluginsEvent* Clone() const { return new CorePluginsEvent( *this ); }

	explicit CorePluginsEvent( PluginEventType evt, SynchronousActionState* sema=NULL )
		: pxInvokeActionEvent( sema )
	{
		m_evt = evt;
	}

	explicit CorePluginsEvent( PluginEventType evt, SynchronousActionState& sema )
		: pxInvokeActionEvent( sema )
	{
		m_evt = evt;
	}

	CorePluginsEvent( const CorePluginsEvent& src )
		: pxInvokeActionEvent( src )
	{
		m_evt = src.m_evt;
	}
	
	void SetEventType( PluginEventType evt ) { m_evt = evt; }
	PluginEventType GetEventType() { return m_evt; }

protected:
	void InvokeEvent()
	{
		sApp.DispatchEvent( m_evt );
	}
};

static void PostPluginStatus( PluginEventType pevt )
{
	sApp.PostAction( CorePluginsEvent( pevt ) );
}

static void ConvertPluginFilenames( wxString (&passins)[PluginId_Count] )
{
	const PluginInfo* pi = tbl_PluginInfo; do
	{
		passins[pi->id] = OverrideOptions.Filenames[pi->id].GetFullPath();

		if( passins[pi->id].IsEmpty() || !wxFileExists( passins[pi->id] ) )
			passins[pi->id] = g_Conf->FullpathTo( pi->id );
	} while( ++pi, pi->shortname != NULL );
}

typedef void (AppPluginManager::*FnPtr_AppPluginManager)();
typedef void (AppPluginManager::*FnPtr_AppPluginPid)( PluginsEnum_t pid );

class SysExecEvent_AppPluginManager : public SysExecEvent
{
protected:
	FnPtr_AppPluginManager	m_method;
	
public:
	virtual ~SysExecEvent_AppPluginManager() throw() {}
	SysExecEvent_AppPluginManager* Clone() const { return new SysExecEvent_AppPluginManager( *this ); }

	SysExecEvent_AppPluginManager( FnPtr_AppPluginManager method )
	{
		m_method = method;
	}
	
protected:
	void InvokeEvent()
	{
		if( m_method ) (CorePlugins.*m_method)();
	}
};

class LoadSinglePluginEvent : public pxInvokeActionEvent
{
	typedef pxInvokeActionEvent _parent;
	DECLARE_DYNAMIC_CLASS_NO_ASSIGN(LoadSinglePluginEvent)

protected:
	wxString		m_filename;
	PluginsEnum_t	m_pid;

public:
	virtual ~LoadSinglePluginEvent() throw() { }
	virtual LoadSinglePluginEvent *Clone() const { return new LoadSinglePluginEvent(*this); }
	
	LoadSinglePluginEvent( PluginsEnum_t pid = PluginId_GS, const wxString& filename=wxEmptyString )
		: m_filename( filename )
	{
		m_pid = pid;
	}

protected:
	void InvokeEvent()
	{
		GetCorePlugins().Load( m_pid, m_filename );
	}
};

class SinglePluginMethodEvent : public pxInvokeActionEvent
{
	typedef pxInvokeActionEvent _parent;
	DECLARE_DYNAMIC_CLASS_NO_ASSIGN(SinglePluginMethodEvent)

protected:
	PluginsEnum_t			m_pid;
	FnPtr_AppPluginPid		m_method;

public:
	virtual ~SinglePluginMethodEvent() throw() { }
	virtual SinglePluginMethodEvent *Clone() const { return new SinglePluginMethodEvent(*this); }
	
	SinglePluginMethodEvent( FnPtr_AppPluginPid method=NULL,  PluginsEnum_t pid = PluginId_GS )
	{
		m_pid		= pid;
		m_method	= method;
	}

protected:
	void InvokeEvent()
	{
		//GetCorePlugins().Unload( m_pid );
		if( m_method ) (CorePlugins.*m_method)( m_pid );
	}
};

IMPLEMENT_DYNAMIC_CLASS( LoadSinglePluginEvent,	 pxInvokeActionEvent );
IMPLEMENT_DYNAMIC_CLASS( SinglePluginMethodEvent, pxInvokeActionEvent );

// --------------------------------------------------------------------------------------
//  AppPluginManager
// --------------------------------------------------------------------------------------
//
// Thread Affinity Notes:
//  It's important to ensure that Load/Unload/Init/Shutdown are all called from the
//  MAIN/UI Thread only.  Those APIs are allowed to issue modal popups, and as such
//  are only safe when invoked form the UI thread.  Under windows the popups themselves
//  will typically work from any thread, but some common control activities will fail
//  (such as opening the browser windows).  On Linux it's probably just highly unsafe, period.
//
//  My implementation is to execute the main Load/Init/Shutdown/Unload procedure on the
//  SysExecutor, and then dispatch each individual plugin to the main thread.  This keeps
//  the main thread from being completely busy while plugins are loaded and initialized.
//  (responsiveness is bliss!!) -- air
//
AppPluginManager::AppPluginManager()
{
}

AppPluginManager::~AppPluginManager() throw()
{
}

void AppPluginManager::Load( PluginsEnum_t pid, const wxString& srcfile )
{
	if( !wxThread::IsMain() )
	{
		wxGetApp().ProcessAction( LoadSinglePluginEvent( pid, srcfile ) );
		Sleep( 5 );
		return;
	}
	
	_parent::Load( pid, srcfile );
}

void AppPluginManager::Unload( PluginsEnum_t pid )
{
	if( !wxThread::IsMain() )
	{
		wxGetApp().ProcessAction( SinglePluginMethodEvent( &AppPluginManager::Unload, pid ) );
		Sleep( 5 );
		return;
	}

	_parent::Unload( pid );
}

void AppPluginManager::Load( const wxString (&folders)[PluginId_Count] )
{
	if( !pxAssert(!AreLoaded()) ) return;

	SetSettingsFolder( GetSettingsFolder().ToString() );
	_parent::Load( folders );
	PostPluginStatus( CorePlugins_Loaded );
}

void AppPluginManager::Unload()
{
	_parent::Unload();
	PostPluginStatus( CorePlugins_Unloaded );
}

void AppPluginManager::Init( PluginsEnum_t pid )
{
	if( !wxThread::IsMain() )
	{
		wxGetApp().ProcessAction( SinglePluginMethodEvent( &AppPluginManager::Init, pid ) );
		Sleep( 5 );
		return;
	}

	_parent::Init( pid );
}

void AppPluginManager::Shutdown( PluginsEnum_t pid )
{
	if( !wxThread::IsMain() )
	{
		wxGetApp().ProcessAction( SinglePluginMethodEvent( &AppPluginManager::Shutdown, pid ) );
		Sleep( 5 );
		return;
	}

	_parent::Shutdown( pid );
}

void AppPluginManager::Init()
{
	SetSettingsFolder( GetSettingsFolder().ToString() );
	_parent::Init();
	PostPluginStatus( CorePlugins_Init );
}

void AppPluginManager::Shutdown()
{
	_parent::Shutdown();
	PostPluginStatus( CorePlugins_Shutdown );
}

void AppPluginManager::Close()
{
	AffinityAssert_AllowFrom_CoreThread();

	/*if( !GetSysExecutorThread().IsSelf() )
	{
		GetSysExecutorThread().ProcessEvent( new SysExecEvent_AppPluginManager( &AppPluginManager::Close ) );
		return;
	}*/

	if( !NeedsClose() ) return;

	PostPluginStatus( CorePlugins_Closing );
	_parent::Close();
	PostPluginStatus( CorePlugins_Closed );
}

void AppPluginManager::Open()
{
	AffinityAssert_AllowFrom_CoreThread();

	/*if( !GetSysExecutorThread().IsSelf() )
	{
		GetSysExecutorThread().ProcessEvent( new SysExecEvent_AppPluginManager( &AppPluginManager::Open ) );
		return;
	}*/

	SetSettingsFolder( GetSettingsFolder().ToString() );

	if( !NeedsOpen() ) return;

	PostPluginStatus( CorePlugins_Opening );
	_parent::Open();
	PostPluginStatus( CorePlugins_Opened );
}

// Yay, this plugin is guaranteed to always be opened first and closed last.
bool AppPluginManager::OpenPlugin_GS()
{
	if( GSopen2 && !s_DisableGsWindow )
	{
		sApp.OpenGsPanel();
	}

	bool retval = _parent::OpenPlugin_GS();

	if( g_LimiterMode == Limit_Turbo )
		GSsetVsync( false );
		
	return retval;
}

static int _guard = 0;

// Yay, this plugin is guaranteed to always be opened first and closed last.
void AppPluginManager::ClosePlugin_GS()
{
	/*if( GSopen2 == NULL || CloseViewportWithPlugins )
	{
		// All other plugins must be closed before the GS, because they all rely on
		// the GS window handle being valid.  The recursion guard will protect this
		// function from being called a million times. ;)

		RecursionGuard mess( _guard );
		if( !mess.IsReentrant() ) Close();
	}*/

	_parent::ClosePlugin_GS();
	if( GetMTGS().IsSelf() && GSopen2 && CloseViewportWithPlugins ) sApp.CloseGsPanel();
}


// --------------------------------------------------------------------------------------
//  LoadCorePluginsEvent
// --------------------------------------------------------------------------------------
class LoadCorePluginsEvent : public SysExecEvent
{
protected:
	wxString			m_folders[PluginId_Count];

public:
	LoadCorePluginsEvent()
	{
		ConvertPluginFilenames( m_folders );
	}
	
	wxString GetEventName() const
	{
		return L"LoadCorePlugins";
	}

	wxString GetEventMessage() const
	{
		return _("Loading PS2 system plugins...");
	}

protected:
	void InvokeEvent()
	{
		CorePlugins.Load( m_folders );
	}
	~LoadCorePluginsEvent() throw() {}
};

// --------------------------------------------------------------------------------------
//  Public API / Interface
// --------------------------------------------------------------------------------------

int EnumeratePluginsInFolder( const wxDirName& searchpath, wxArrayString* dest )
{
	ScopedPtr<wxArrayString> placebo;
	wxArrayString* realdest = dest;
	if( realdest == NULL )
		placebo = realdest = new wxArrayString();		// placebo is our /dev/null -- gets deleted when done

#ifdef __WXMSW__
	// Windows pretty well has a strict "must end in .dll" rule.
	wxString pattern( L"*%s" );
#else
	// Other platforms seem to like to version their libs after the .so extension:
	//    blah.so.3.1.fail?
	wxString pattern( L"*%s*" );
#endif

	return searchpath.Exists() ?
		wxDir::GetAllFiles( searchpath.ToString(), realdest, wxsFormat( pattern, wxDynamicLibrary::GetDllExt()), wxDIR_FILES ) : 0;
}

// Posts a message to the App to reload plugins.  Plugins are loaded via a background thread
// which is started on a pending event, so don't expect them to be ready  "right now."
// If plugins are already loaded, onComplete is invoked, and the function returns with no
// other actions performed.
void LoadPluginsPassive()
{
	AffinityAssert_AllowFrom_MainUI();

	// Plugins already loaded?
	if( !CorePlugins.AreLoaded() )
	{
		wxGetApp().SysExecutorThread.PostEvent( new LoadCorePluginsEvent() );
	}
}

static void _LoadPluginsImmediate()
{
	if( CorePlugins.AreLoaded() ) return;

	wxString passins[PluginId_Count];
	ConvertPluginFilenames( passins );
	CorePlugins.Load( passins );
}

void LoadPluginsImmediate()
{
	AffinityAssert_AllowFrom_SysExecutor();
	_LoadPluginsImmediate();
}

// Performs a blocking load of plugins.  If the emulation thread is active, it is shut down
// automatically to prevent race conditions (it depends on plugins).
//
// Exceptions regarding plugin failures will propagate out of this function, so be prepared
// to handle them.
//
// Note that this is not recommended for most situations, but coding improper passive loads
// is probably worse, so if in doubt use this and air will fix it up for you later. :)
//
void ScopedCoreThreadClose::LoadPlugins()
{
	DbgCon.WriteLn("(ScopedCoreThreadClose) Loading plugins!");
	_LoadPluginsImmediate();
}


class SysExecEvent_UnloadPlugins : public SysExecEvent
{
public:
	wxString GetEventName() const { return L"UnloadPlugins"; }

	virtual ~SysExecEvent_UnloadPlugins() throw() {}
	SysExecEvent_UnloadPlugins* Clone() const { return new SysExecEvent_UnloadPlugins(*this); }

	virtual bool AllowCancelOnExit() const { return false; }
	virtual bool IsCriticalEvent() const { return true; }

	void InvokeEvent()
	{
		CoreThread.Cancel();
		CorePlugins.Unload();
	}
};

class SysExecEvent_ShutdownPlugins : public SysExecEvent
{
public:
	wxString GetEventName() const { return L"ShutdownPlugins"; }

	virtual ~SysExecEvent_ShutdownPlugins() throw() {}
	SysExecEvent_ShutdownPlugins* Clone() const { return new SysExecEvent_ShutdownPlugins(*this); }

	virtual bool AllowCancelOnExit() const { return false; }
	virtual bool IsCriticalEvent() const { return true; }

	void InvokeEvent()
	{
		CoreThread.Cancel();
		CorePlugins.Shutdown();
	}
};

void UnloadPlugins()
{
	GetSysExecutorThread().PostEvent( new SysExecEvent_UnloadPlugins() );
}

void ShutdownPlugins()
{
	GetSysExecutorThread().PostEvent( new SysExecEvent_ShutdownPlugins() );
}

// --------------------------------------------------------------------------------------
//  SaveSinglePluginHelper  (Implementations)
// --------------------------------------------------------------------------------------

SaveSinglePluginHelper::SaveSinglePluginHelper( PluginsEnum_t pid )
	: m_plugstore( L"PluginConf Savestate" )
{
	s_DisableGsWindow = true;

	m_pid			= pid;
	m_validstate	= SysHasValidState();

	_LoadPluginsImmediate();
	if( !CorePlugins.AreLoaded() ) return;

	if( !m_validstate ) return;
	Console.WriteLn( Color_Green, L"Suspending single plugin: " + tbl_PluginInfo[m_pid].GetShortname() );

	memSavingState save( m_plugstore );
	GetCorePlugins().Freeze( m_pid, save );
	GetCorePlugins().Close( pid );
}

SaveSinglePluginHelper::~SaveSinglePluginHelper() throw()
{
	bool allowResume = true;

	try {
		if( m_validstate )
		{
			Console.WriteLn( Color_Green, L"Recovering single plugin: " + tbl_PluginInfo[m_pid].GetShortname() );
			memLoadingState load( m_plugstore );
			//if( m_plugstore.IsDisposed() ) load.SeekToSection( m_pid );
			GetCorePlugins().Open( m_pid );
			GetCorePlugins().Freeze( m_pid, load );
		}

		s_DisableGsWindow = false;
	}
	catch( BaseException& ex )
	{
		allowResume = false;
		wxGetApp().PostEvent( pxExceptionEvent( ex ) );

		//Console.Error( "Unhandled BaseException in %s (ignored!):", __pxFUNCTION__ );
		//Console.Error( ex.FormatDiagnosticMessage() );
	}
	catch( std::exception& ex )
	{
		allowResume = false;
		wxGetApp().PostEvent( pxExceptionEvent(new Exception::RuntimeError( ex, L"SaveSinglePlugin" )) );

		//Console.Error( "Unhandled std::exception in %s (ignored!):", __pxFUNCTION__ );
		//Console.Error( ex.what() );
	}

	s_DisableGsWindow = false;
	if( allowResume ) m_scoped_pause.AllowResume();
}
