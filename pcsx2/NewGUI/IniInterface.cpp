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
#include "System.h"
#include "IniInterface.h"

//////////////////////////////////////////////////////////////////////////////////////////
//
IniInterface::IniInterface( wxConfigBase& config ) :
	m_Config( config )
{
}

IniInterface::IniInterface() :
	m_Config( *wxConfigBase::Get() )
{
}

IniInterface::~IniInterface()
{
	Flush();
}

void IniInterface::SetPath( const wxString& path )
{
	m_Config.SetPath( path );
}

void IniInterface::Flush()
{
	m_Config.Flush();
}

//////////////////////////////////////////////////////////////////////////////////////////
//

IniLoader::IniLoader( wxConfigBase& config ) : IniInterface( config )
{
}

IniLoader::IniLoader() : IniInterface() {}
IniLoader::~IniLoader() {}


void IniLoader::Entry( const wxString& var, wxString& value, const wxString& defvalue )
{
	m_Config.Read( var, &value, defvalue );
}

void IniLoader::Entry( const wxString& var, int& value, const int defvalue )
{
	m_Config.Read( var, &value, defvalue );
}

void IniLoader::Entry( const wxString& var, uint& value, const uint defvalue )
{
	m_Config.Read( var, (int*)&value, (int)defvalue );
}

void IniLoader::Entry( const wxString& var, bool& value, const bool defvalue )
{
	wxString dest;
	m_Config.Read( var, &dest, defvalue ? L"enabled" : L"disabled" );
	value = (dest == L"enabled") || (dest == L"1");
}

void IniLoader::Entry( const wxString& var, wxPoint& value, const wxPoint& defvalue )
{
	TryParse( value, m_Config.Read( var, ToString( defvalue ) ), defvalue );
}

void IniLoader::Entry( const wxString& var, wxSize& value, const wxSize& defvalue )
{
	TryParse( value, m_Config.Read( var, ToString( defvalue ) ), defvalue );
}

void IniLoader::Entry( const wxString& var, wxRect& value, const wxRect& defvalue )
{
	TryParse( value, m_Config.Read( var, ToString( defvalue ) ), defvalue );
}

void IniLoader::EnumEntry( const wxString& var, int& value, const wxChar* const* enumArray, const int defvalue )
{
	wxString retval;
	m_Config.Read( var, &retval, enumArray[defvalue] );

	int i=0;
	while( enumArray[i] != NULL && ( retval != enumArray[i] ) ) i++;

	if( enumArray[i] == NULL )
	{
		Console::Notice( wxsFormat( L"Loadini Warning: Unrecognized value '%s' on key '%s'\n\tUsing the default setting of '%s'.",
			params retval.c_str(), var.c_str(), enumArray[defvalue]
		) );
		value = defvalue;
	}
	else
		value = i;
}

//////////////////////////////////////////////////////////////////////////////////////////
//

IniSaver::IniSaver( wxConfigBase& config ) : IniInterface( config )
{
}

IniSaver::IniSaver() : IniInterface() {}
IniSaver::~IniSaver() {}

void IniSaver::Entry( const wxString& var, wxString& value, const wxString& defvalue )
{
	m_Config.Write( var, value );
}

void IniSaver::Entry( const wxString& var, int& value, const int defvalue )
{
	m_Config.Write( var, value );
}

void IniSaver::Entry( const wxString& var, uint& value, const uint defvalue )
{
	m_Config.Write( var, (int)value );
}

void IniSaver::Entry( const wxString& var, bool& value, const bool defvalue )
{
	m_Config.Write( var, value ? L"enabled" : L"disabled" );
}

void IniSaver::Entry( const wxString& var, wxPoint& value, const wxPoint& defvalue )
{
	m_Config.Write( var, ToString( value ) );
}

void IniSaver::Entry( const wxString& var, wxSize& value, const wxSize& defvalue )
{
	m_Config.Write( var, ToString( value ) );
}

void IniSaver::Entry( const wxString& var, wxRect& value, const wxRect& defvalue )
{
	m_Config.Write( var, ToString( value ) );
}

void IniSaver::EnumEntry( const wxString& var, int& value, const wxChar* const* enumArray, const int defvalue )
{
	m_Config.Write( var, enumArray[value] );
}
