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
 
#pragma once
namespace R3000A {

using namespace x86Emitter;

// ------------------------------------------------------------------------
// Provisions an array type which is accessible either by dynamic/runtime values using the
// [] indexer, or via constants using .eax/.ebx/etc syntax.  (convenience!)
//
template< typename ContentType >
class xRegisterArray32
{
public:
	ContentType eax, ecx, edx, ebx, ebp, esp, esi, edi;

	int Length() const { return iREGCNT_GPR; }

	__forceinline ContentType& operator[]( const xRegister32& src )
	{
		jASSUME( !src.IsEmpty() );
		switch( src.Id )
		{
			case 0: return eax;
			case 1: return ecx;
			case 2: return edx;
			case 3: return ebx;
			case 4: return ebp;
			case 5: return esp;
			case 6: return esi;
			case 7: return edi;
			
			jNO_DEFAULT
		}
	}

	__forceinline const ContentType& operator[]( const xRegister32& src ) const 
	{
		jASSUME( !src.IsEmpty() );
		switch( src.Id )
		{
			case 0: return eax;
			case 1: return ecx;
			case 2: return edx;
			case 3: return ebx;
			case 4: return ebp;
			case 5: return esp;
			case 6: return esi;
			case 7: return edi;
			
			jNO_DEFAULT
		}
	}
};

// ------------------------------------------------------------------------
// An array of valid temp registers - eax, ebx, ecx, and edx.  The other four x86
// registers have special defined purposes.
//
template< typename ContentType >
class xTempRegsArray32
{
public:
	ContentType eax, ecx, edx, ebx;

	int Length() const { return 4; }

	__forceinline ContentType& operator[]( const xRegister32& src )
	{
		jASSUME( !src.IsEmpty() );
		switch( src.Id )
		{
			case 0: return eax;
			case 1: return ecx;
			case 2: return edx;
			case 3: return ebx;
			
			jNO_DEFAULT		// assertion here means someone tried to use a register that's not ok for temp'ing.
		}
	}

	__forceinline const ContentType& operator[]( const xRegister32& src ) const 
	{
		jASSUME( !src.IsEmpty() );
		switch( src.Id )
		{
			case 0: return eax;
			case 1: return ecx;
			case 2: return edx;
			case 3: return ebx;
			
			jNO_DEFAULT		// assertion here means someone tried to use a register that's not ok for temp'ing.
		}
	}
};

//////////////////////////////////////////////////////////////////////////////////////////
//
template< typename T >
struct RegFieldArray
{
	T Rd, Rt, Rs, Hi, Lo;

	__forceinline T& operator[]( RegField_t idx )
	{
		jASSUME( ((uint)idx) < RF_Count );
		switch( idx )
		{
			case RF_Rd: return Rd;
			case RF_Rt: return Rt;
			case RF_Rs: return Rs;
			case RF_Hi: return Hi;
			case RF_Lo: return Lo;
			jNO_DEFAULT
		}
	}

	__forceinline const T& operator[]( RegField_t idx ) const
	{
		jASSUME( ((uint)idx) < RF_Count );
		switch( idx )
		{
			case RF_Rd: return Rd;
			case RF_Rt: return Rt;
			case RF_Rs: return Rs;
			case RF_Hi: return Hi;
			case RF_Lo: return Lo;
			jNO_DEFAULT
		}
	}

	__forceinline T& operator[]( uint idx )
	{
		jASSUME( idx < RF_Count );
		return operator[]( (RegField_t)idx );
	}

	__forceinline const T& operator[]( int idx ) const
	{
		jASSUME( idx < RF_Count );
		return operator[]( (RegField_t)idx );
	}
};

}

#include "RegMap_Dynamic.h"
#include "RegMap_Strict.h"

namespace R3000A {

//////////////////////////////////////////////////////////////////////////////////////////
// Register mapping options for each instruction emitter implementation
//
class RegisterMappingOptions
{
public:
	bool IsStrictMode;

	RegMapInfo_Dynamic DynMap;
	RegMapInfo_Strict StatMap;

	// Set to true to inform the recompiler that it can swap Rs and Rt freely.  This option
	// takes effect even if Rs or Rt have been forced to registers, or mapped to specific
	// registers.
	bool CommutativeSources;

	RegisterMappingOptions() :
	IsStrictMode( false ),
		CommutativeSources( false )
	{
	}

	RegMapInfo_Dynamic& UseDynMode()		{ IsStrictMode = false; return DynMap; }
	RegMapInfo_Strict& UseStrictMode()		{ IsStrictMode = true; return StatMap; }

	/*bool IsRequiredRegOnEntry( const xRegister32& reg ) const
	{
	for( int i=0; i<RF_Count; ++i )
	{
	if( GprFields[i].EntryMap == reg ) return true;
	}

	return false;
	}*/
};

}
