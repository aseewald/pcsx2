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
#include "IopCommon.h"
#include "../iR3000air.h"

#include "R3000airArithmetic.inl"
#include "R3000airLogic.inl"
#include "R3000airMemory.inl"
#include "R3000airMisc.inl"

#define PLACE_REC_OBJ( name ) \
	new (dest) rec##name( src ); \
	dest += sizeof( rec##name )

#define CASE_PLACE_REC_OBJ( name ) \
	case IrInst_##name: PLACE_REC_OBJ( name ); break

typedef recLoad<u8>  recLoadByte;
typedef recLoad<u16> recLoadHalfword;
typedef recLoad<u32> recLoadWord;

typedef recStore<u8>  recStoreByte;
typedef recStore<u16> recStoreHalfword;
typedef recStore<u32> recStoreWord;

void R3000A::x86IntRep::PlacementNew( u8** dest, const IntermediateRepresentation& src )
{
	switch( src.InstId )
	{
		/*CASE_PLACE_REC_OBJ( Branch )
		CASE_PLACE_REC_OBJ( BranchReg )*/

		CASE_PLACE_REC_OBJ( Add );
		CASE_PLACE_REC_OBJ( Sub );
		CASE_PLACE_REC_OBJ( AddImm );
		CASE_PLACE_REC_OBJ( Mult );
		CASE_PLACE_REC_OBJ( Div );

		CASE_PLACE_REC_OBJ( ShiftLeft );
		CASE_PLACE_REC_OBJ( ShiftRight );
		CASE_PLACE_REC_OBJ( ShiftLeftImm );
		CASE_PLACE_REC_OBJ( ShiftRightImm );

		CASE_PLACE_REC_OBJ( And );
		CASE_PLACE_REC_OBJ( Or );
		CASE_PLACE_REC_OBJ( Xor );
		CASE_PLACE_REC_OBJ( Nor );
		CASE_PLACE_REC_OBJ( AndImm );
		CASE_PLACE_REC_OBJ( OrImm );
		CASE_PLACE_REC_OBJ( XorImm );

		CASE_PLACE_REC_OBJ( Set );
		CASE_PLACE_REC_OBJ( SetImm );

		CASE_PLACE_REC_OBJ( TranslateAddr );
		CASE_PLACE_REC_OBJ( LoadByte );
		CASE_PLACE_REC_OBJ( LoadHalfword );
		CASE_PLACE_REC_OBJ( LoadWord );
		CASE_PLACE_REC_OBJ( StoreByte );
		CASE_PLACE_REC_OBJ( StoreHalfword );
		CASE_PLACE_REC_OBJ( StoreWord );

		case IrInst_LoadWordLeft:
		case IrInst_LoadWordRight:
			new (dest) recLoadWord_LorR( src, src.InstId == IrInst_LoadWordRight );
			dest += sizeof( recLoadWord_LorR );
		break;

		case IrInst_StoreWordLeft:
		case IrInst_StoreWordRight:
			new (dest) recStoreWord_LorR( src, src.InstId == IrInst_StoreWordRight );
			dest += sizeof( recStoreWord_LorR );
		break;

		/*CASE_PLACE_REC_OBJ( MFC0 )
		CASE_PLACE_REC_OBJ( MTC0 )

		CASE_PLACE_REC_OBJ( Exception )
		CASE_PLACE_REC_OBJ( RFE )
		CASE_PLACE_REC_OBJ( DivUnitStall )
		CASE_PLACE_REC_OBJ( ZeroEx )
		CASE_PLACE_REC_OBJ( NOP )*/
	}
}
