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
#include "iR3000air.h"

namespace R3000A
{

class EmitConstError_t : public InstructionEmitter
{
public:
	EmitConstError_t() {}

	virtual void MapRegisters( const IntermediateInstruction& info, RegisterMappings& dest, RegisterMappings& outmaps ) const
	{
		assert( false );
		throw Exception::LogicError( "R3000A Recompiler Logic Error: Const Rs/Rt form is not valid for this instruction." );
	}

	virtual void Emit( const IntermediateInstruction& info ) const
	{
		assert( false );
		throw Exception::LogicError( "R3000A Recompiler Logic Error: Const Rs/Rt form is not valid for this instruction." );
	}
};

static const EmitConstError_t EmitConstError;
const InstructionEmitter& InstructionRecompiler::ConstRsRt() const { return EmitConstError; }

//////////////////////////////////////////////////////////////////////////////////////////
//
// Div on MIPS:
// * If Rt is zero, then the result is undefined by MIPS standard.  EE/IOP results are pretty
//   consistent, however:  Hi == Rs, Lo == (Rs >= 0) ? -1 : 1
//
// * MIPS has special defined behavior on signed DIVs, to cope with it's lack of overflow
//   exception handling.  If Rs == -0x80000000 (which is the same as unsigned 0x80000000 on
//   our beloved twos-compliment math), and Rt == -1 (0xffffffff as unsigned), the result
//   is 0x80000000 and remainder of zero.


//////////////////////////////////////////////////////////////////////////////////////////
//
class recDIV_ConstNone : public InstructionEmitter
{
public:
	recDIV_ConstNone() {}

	void MapRegisters( const IntermediateInstruction& info, RegisterMappings& inmaps, RegisterMappings& outmaps ) const
	{
		inmaps.Rt = eax;
		inmaps.Rs = ecx;

		outmaps.Rs = ecx;	// untouched!

		outmaps.Hi = edx;
		outmaps.Lo = eax;
	}

	void Emit( const IntermediateInstruction& info ) const
	{
		xCMP( eax, 0 );
		xForwardJE8 skipOverflowRt;

		xMOV( eax, 1 );
		xMOV( edx, -1 );
		xCMP( ecx, 0 );
		xCMOVGE( eax, edx );
		xForwardJump8 writeBack;	// write eax:edx and be done!

		skipOverflowRt.SetTarget();
		xCMP( eax, -1 );
		xForwardJNE8 skipOverflowRtNeg;
		xMOV( eax, 0x80000000 );
		xCMP( ecx, eax );
		xForwardJNE8 skipOverflowRtNeg2;
		info.MoveToHiLo( eax );	// eax == 0x80000000
		xForwardJump8 skipAll;

		// All the evil MIPS behavior checks fell through..?
		// Then Perform full-on div!
		skipOverflowRtNeg.SetTarget();
		skipOverflowRtNeg2.SetTarget();
		xDIV( edx );

		// writeback for Rt-overflow case.
		writeBack.SetTarget();
		info.MoveToHiLo( edx, eax );	// eax == 0x80000000

		skipAll.SetTarget();
	}
};

//////////////////////////////////////////////////////////////////////////////////////////
//
class recDIV_ConstRt : public InstructionEmitter
{
public:
	void MapRegisters( const IntermediateInstruction& info, RegisterMappings& inmaps, RegisterMappings& outmaps ) const
	{
		if( info.Src[RF_Rt].Imm == 0 )
		{
			inmaps.Rs = edx;
			outmaps.Rs = edx;		// unmodified.
		}
		else if( info.Src[RF_Rt].Imm == -1 )
		{
			inmaps.Rs = ecx;
			outmaps.Rs = ecx;		// unmodified.
		}
		else
		{
			inmaps.Rt = eax;
			inmaps.Rs = edx;
		}

		outmaps.Hi = edx;
		outmaps.Lo = eax;
	}

	void Emit( const IntermediateInstruction& info ) const
	{
		// If both Rt and Rs are const, then this instruction shouldn't even be recompiled
		// since the result is known at IL time.
		jASSUME( !info.IsConstRs() );

		if( info.Src[RF_Rt].Imm == 0 )
		{
			// Rs is loaded into edx.
			
			xMOV( eax, 1 );
			xMOV( ecx, -1 );
			xCMP( edx, 0 );
			xCMOVGE( eax, ecx );
			// edx == Hi, eax == Lo
			info.MoveToHiLo( edx, eax );
		}
		else if( info.Src[RF_Rt].Imm == -1 )
		{
			// Rs is loaded into ecx.

			xMOV( eax, 0x80000000 );
			if( info.Src[RF_Rs].RegDirect.IsEmpty() )
				xCMP( info.Src[RF_Rs].GetMem(), eax );
			else
				xCMP( info.Src[RF_Rs].RegDirect, eax );

			xForwardJNE8 doFullDiv;
			info.MoveToHiLo( eax );
			xForwardJump8 skipAll;

			doFullDiv.SetTarget();
			xDIV( ecx );
			info.MoveToHiLo( edx, eax );

			skipAll.SetTarget();
		}
		else
		{
			recDIV_ConstNone().Emit( info );
		}
	}
};


//////////////////////////////////////////////////////////////////////////////////////////
//
class recDIV_ConstRs : public InstructionEmitter
{
public:
	void MapRegisters( const IntermediateInstruction& info, RegisterMappings& inmaps, RegisterMappings& outmaps ) const
	{
		inmaps.Rt = eax;
		
		// DIV doesn't have an imm form, so we need to have the immediate value
		// loaded into ecx anyway:
		inmaps.Rs = ecx;
		outmaps.Rs = ecx;		// and it's unmodified!

		outmaps.Hi = edx;
		outmaps.Lo = eax;
	}
	
	void Emit( const IntermediateInstruction& info )
	{
		// If both Rt and Rs are const, then this instruction shouldn't even be recompiled
		// since the result is known at IL time.
		jASSUME( !info.IsConstRt() );

		xCMP( eax, 0 );
		xForwardJZ8 skipOverflowRt;

		xMOV( eax, (info.Src[RF_Rs].Imm >= 0) ? -1 : 1 );
		info.MoveToHiLo( ecx, eax );
		xForwardJump8 skipAll;

		skipOverflowRt.SetTarget();
		if( info.Src[RF_Rs].Imm == 0x80000000 )
		{
			// If Rs is 0x80000000 then we need to check Rt for -1.

			xCMP( eax, -1 );
			xForwardJNE8 skipOverflowRtNeg;

			info.MoveToHiLo( ecx );	// ecx is Rs, which is const 0x80000000
			xForwardJump8 skipDiv;

			// eh, need to duplicate div code because of jump label scoping rules.
			skipOverflowRtNeg.SetTarget();
			xDIV( ecx );
			info.MoveToHiLo( edx, eax );

			skipDiv.SetTarget();
		}
		else
		{
			xDIV( ecx );
			info.MoveToHiLo( edx, eax );
		}
		skipAll.SetTarget();
	}
};

class recInst_DIV : public InstructionRecompiler
{
protected:
	static const recDIV_ConstNone m_ConstNone;
	static const recDIV_ConstRt m_ConstRt;
	static const recDIV_ConstRs m_ConstRs;

public:
	recInst_DIV() {}
	
	const InstructionEmitter& ConstNone() const	{ return m_ConstNone; }
	const InstructionEmitter& ConstRt() const	{ return m_ConstRt; }
	const InstructionEmitter& ConstRs() const	{ return m_ConstRs; }
};

const recInst_DIV __recDIV;

const recDIV_ConstNone recInst_DIV::m_ConstNone;
const recDIV_ConstRt recInst_DIV::m_ConstRt;
const recDIV_ConstRs recInst_DIV::m_ConstRs;

const InstructionRecompiler& recDIV = __recDIV;

}
