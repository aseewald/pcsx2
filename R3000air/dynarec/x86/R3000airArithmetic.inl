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

namespace R3000A
{

IMPL_RecPlacebo( Mult );

// ------------------------------------------------------------------------
class recAddImm : public x86IntRep
{
public:
	recAddImm( const IntermediateRepresentation& src ) : x86IntRep( src )
	{
		RegMapInfo_Dynamic& dyn( RegOpts.UseDynMode() );

		dyn.ExitMap.Rt = DynEM_Rs;
		if( GetImm() == 0 )
			dyn.ExitMap.Rs = DynEM_Untouched;
	}

	void Emit() const
	{
		if( GetImm() != 0 )
			xADD( RegRs, GetImm() );

		MoveToRt( RegRs );
	}
};

// ------------------------------------------------------------------------
class recAdd : public x86IntRep
{
public:
	recAdd( const IntermediateRepresentation& src ) : x86IntRep( src )
	{
		RegMapInfo_Dynamic& dyn( RegOpts.UseDynMode() );

		// Rs and Rt can be swapped freely:
		RegOpts.CommutativeSources	= true;
	}

	void Emit() const
	{
		xADD( RegRs, RegRt );
		MoveToRd( RegRs );
	}
};

// ------------------------------------------------------------------------
class recSub : public x86IntRep
{
public:
	recSub( const IntermediateRepresentation& src ) : x86IntRep( src )
	{
		RegMapInfo_Dynamic& dyn( RegOpts.UseDynMode() );
	}

	void recSub_Emit() const
	{
		xSUB( RegRt, RegRs );
		MoveToRd( RegRs );
	}
};

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

class recDiv : public x86IntRep
{
protected:
	// ------------------------------------------------------------------------
	void Init_ConstNone()
	{
		RegMapInfo_Strict& strict( RegOpts.UseStrictMode() );
		
		strict.EntryMap.Rs = ecx;
		strict.EntryMap.Rt = eax;

		strict.ExitMap.eax = StrictEM_Lo;
		strict.ExitMap.ecx = StrictEM_Untouched;
		strict.ExitMap.ebx = StrictEM_Untouched;
	}

	// ------------------------------------------------------------------------
	void Emit_ConstNone() const
	{
		// Make sure recompiler did it's job:
		if( Inst.MipsInst._Rs_ != Inst.MipsInst._Rt_ )
		{
			DynarecAssert( (RegRs == ecx) && (RegRt == eax), "Recompiler failed to prepare strict eax/ecx mappings." );
		}

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
		MoveToHiLo( eax );	// eax == 0x80000000
		xForwardJump8 skipAll;

		// All the evil MIPS behavior checks fell through..?
		// Then Perform full-on div!
		skipOverflowRtNeg.SetTarget();
		skipOverflowRtNeg2.SetTarget();
		xDIV( ecx );

		// writeback for Rt-overflow case.
		writeBack.SetTarget();
		MoveToHiLo( edx, eax );

		skipAll.SetTarget();
	}
	
	void Init_ConstRt()
	{
		RegMapInfo_Strict& strict( RegOpts.UseStrictMode() );

		if( GetConstRt() == 0 )
		{
			strict.EntryMap.Rs = edx;
			strict.ExitMap.edx = StrictEM_Untouched;
			strict.ExitMap.ebx = StrictEM_Untouched;
			strict.ExitMapHiLo( edx, eax );
		}
		else if( GetConstRt() == -1 )
		{
			strict.ExitMap.ecx = StrictEM_Untouched;
			strict.ExitMap.ebx = StrictEM_Untouched;
			strict.ExitMapHiLo( edx, eax );
		}
		else
		{
			Init_ConstNone();
		}
	}

	// ------------------------------------------------------------------------
	void Emit_ConstRt() const 
	{
		// If both Rt and Rs are const, then this instruction shouldn't even be recompiled
		// since the result is known at IR time.
		jASSUME( !IsConstRs() );

		if( GetConstRt() == 0 )
		{
			// Rs is loaded into edx.
			
			xMOV( eax, 1 );
			xMOV( ecx, -1 );
			xCMP( edx, 0 );
			xCMOVGE( eax, ecx );
			// edx == Hi, eax == Lo
			MoveToHiLo( edx, eax );
		}
		else if( GetConstRt() == -1 )
		{
			xMOV( eax, 0x80000000 );
			xCMP( RegRs, eax );

			xForwardJNE8 doFullDiv;
			MoveToHiLo( eax );
			xForwardJump8 skipAll;

			doFullDiv.SetTarget();
			xMOV( eax, GetConstRt() );
			xDIV( RegRs );
			MoveToHiLo( edx, eax );

			skipAll.SetTarget();
		}
		else
		{
			Emit_ConstNone();
		}
	}

	void Init_ConstRs()
	{
		RegMapInfo_Strict& strict( RegOpts.UseStrictMode() );

		strict.EntryMap.Rt = eax;
		strict.EntryMap.Rs = ecx;		// DIV lacks Imm forms, so force-load const Rs into ecx

		// When Const Rs == 0x80000000, the mappings of Hi/Lo are indeterminate.
		if( GetConstRs() != 0x80000000 )
		{
			strict.ExitMapHiLo( edx, eax );
		}

		strict.ExitMap.ecx = StrictEM_Untouched;
		strict.ExitMap.ebx = StrictEM_Untouched;
	}

	void Emit_ConstRs() const
	{
		// If both Rt and Rs are const, then this instruction shouldn't even be recompiled
		// since the result is known at IR time.
		jASSUME( !IsConstRt() );

		// Make sure recompiler did it's job:
		if( Inst.MipsInst._Rs_ != Inst.MipsInst._Rt_ )
		{
			DynarecAssert( (RegRs == ecx) && (RegRt == eax),
				"Recompiler failed to prepare strict eax/ecx mappings [ConstRs form]." );
		}

		xCMP( eax, 0 );
		xForwardJZ8 skipOverflowRt;

		xMOV( eax, (GetConstRs() >= 0) ? -1 : 1 );
		MoveToHiLo( ecx, eax );
		xForwardJump8 skipAll;

		skipOverflowRt.SetTarget();
		if( GetConstRs() == 0x80000000 )
		{
			// If Rs is 0x80000000 then we need to check Rt for -1.

			xCMP( eax, -1 );
			xForwardJNE8 skipOverflowRtNeg;

			MoveToHiLo( ecx );	// ecx is Rs, which is const 0x80000000
			xForwardJump8 skipDiv;

			// eh, need to duplicate div code because of jump label scoping rules.
			skipOverflowRtNeg.SetTarget();
			xDIV( ecx );
			MoveToHiLo( edx, eax );

			skipDiv.SetTarget();
		}
		else
		{
			xDIV( ecx );
			MoveToHiLo( edx, eax );
		}
		skipAll.SetTarget();
	}

public:
	// ------------------------------------------------------------------------
	recDiv( const IntermediateRepresentation& src ) : x86IntRep( src )
	{
		InstructionConstOptimizer mips( Inst.MipsInst );
		if( mips.IsConstRt() && mips.IsConstRs() )
		{
			DynarecAssert( false, "Invalid const status on DIV instruction." );
		}
		else if( mips.IsConstRt() )
		{
			Init_ConstRt();
		}
		else if( mips.IsConstRs() )
		{
			Init_ConstRs();
		}
		else
		{
			Init_ConstNone();
		}
	}

	// ------------------------------------------------------------------------
	void Emit() const
	{
		if( Inst.MipsInst.IsConstRt() )
		{
			Emit_ConstRt();
		}
		else if( Inst.MipsInst.IsConstRs() )
		{
			Emit_ConstRs();
		}
		else
		{
			Emit_ConstNone();
		}
	}
};

}
