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

#include "R3000A.h"
#include "R3000Exceptions.h"
#include "IopMem.h"

#include "R3000airInstruction.inl"


namespace R3000A
{
typedef InstructionDiagnostic InstDiag;		// makes life easier on the verboseness front.

static const char *tbl_regname_gpr[34] =
{
	"r0",	"at", "v0", "v1", "a0", "a1", "a2", "a3",
	"t0",	"t1", "t2", "t3", "t4", "t5", "t6", "t7",
	"s0",	"s1", "s2", "s3", "s4", "s5", "s6", "s7",
	"t8",	"t9", "k0", "k1", "gp", "sp", "fp", "ra",
	
	"hi",	"lo"
};

const char* Diag_GetGprName( MipsGPRs_t gpr )
{
	jASSUME( ((uint)gpr) < 34 );
	return tbl_regname_gpr[gpr];
}

static const char *tbl_regname_cop0[32] =
{
	"Index",	"Random",	"EntryLo0",	"EntryLo1",	"Context",	"PageMask", "Wired",	"C0r7",
	"BadVaddr",	"Count",	"EntryHi",	"Compare",	"Status",	"Cause",	"EPC",		"PRId",
	"Config",	"C0r17",	"C0r18",	"C0r19",	"C0r20",	"C0r21",	"C0r22",	"C0r23",
	"Debug",	"Perf",		"C0r26",	"C0r27",	"TagLo",	"TagHi",	"ErrorPC",	"C0r31"
};

const char* Diag_GetCP0Name( uint cp0reg )
{
	jASSUME( ((uint)cp0reg) < 32 );
	return tbl_regname_cop0[cp0reg];
}

// Same as Diag_GetGprName, but returns r0 as "zero" for visual convenience.
static const char* _get_GprName( MipsGPRs_t gpr )
{
	return ( gpr == GPR_r0 ) ? "zero" : tbl_regname_gpr[gpr];
}

bool InstDiag::ParamIsRead( const InstParamType ptype ) const
{
	switch( ptype )
	{
		case Param_None: return false;

		case Param_Rt: return m_ReadsGPR.Rt;
		case Param_Rs: return m_ReadsGPR.Rs;
		case Param_Rd: return m_ReadsGPR.Rd;
		case Param_Sa: return true;
		case Param_Fs: return m_ReadsGPR.Fs;		// Used by Cop0 load/store
		case Param_Hi: return m_ReadsGPR.Hi;
		case Param_Lo: return m_ReadsGPR.Lo;
			
		case Param_HiLo: return false;		// 64 bit operand (allowed as destination only)

		case Param_Imm:
			return false;

		case Param_AddrImm:		// Address Immediate (Rs + Imm()), used by load/store
			return true;		// always true since we want to see the result 

		case Param_BranchOffset:
			return false;
		
		case Param_JumpTarget:	// 26 bit immediate used as a jump target
			return false;
		
		jNO_DEFAULT;
	}
}

void InstDiag::GetParamName( const InstParamType ptype, string& dest ) const
{
	const char* pname = NULL;

	switch( ptype )
	{
		case Param_None: break;

		case Param_Rt:
			pname  = _get_GprName(_Rt_);
		break;

		case Param_Rs:
			pname  = _get_GprName(_Rs_);
		break;
		
		case Param_Rd:
			pname  = _get_GprName(_Rd_);
		break;

		case Param_Sa:
			pname  = "sa";
		break;

		case Param_Fs:		// Used by Cop0 load/store
			pname  = tbl_regname_cop0[_Rd_];
		break;

		case Param_Hi:
			pname  = "hi";
		break;
		
		case Param_Lo:
			pname  = "lo";
		break;
			
		case Param_HiLo:		// 64 bit operand (allowed as destination only)
			pname = "hilo";
		break;

		case Param_Imm:
		break;

		case Param_AddrImm:		// Address Immediate (Rs + Imm()), used by load/store
			// TODO : Calculate label.
			//AddrImm()
			ssprintf( dest, "0x%4.4x(%s)", (s32)_Opcode_.Imm(), _get_GprName(_Rs_) );
		return;

		case Param_BranchOffset:
			// TODO : Calculate label.
			//BranchTarget();
		break;
		
		case Param_JumpTarget:	// 26 bit immediate used as a jump target
			// TODO : Calculate label.
			//JumpTarget()
		break;
		
		jNO_DEFAULT;
	}
	
	dest = (pname != NULL) ? pname : "";
}

void InstDiag::GetParamValue( const InstParamType ptype, string& dest ) const
{
	// First parameter is always "target" form, so it never has a value.
	if( ptype == Param_None ) { dest.clear(); return; }

	s32 pvalue = 0;

	switch( ptype )
	{
		case Param_HiLo:		// 64 bit operand (allowed as destination [param0] only)
			jASSUME( 0 );
		break;

		case Param_Rt:
			pvalue = RtValue().SL;
		break;

		case Param_Rs:
			pvalue = RsValue().SL;
		break;
		
		case Param_Rd:
			pvalue = RdValue().SL;
		break;

		case Param_Sa:
			pvalue = Sa();
		break;

		case Param_Fs:		// Used by Cop0 load/store
			pvalue = FsValue().SL;
		break;

		case Param_Hi:
			pvalue = HiValue().SL;
		break;
		
		case Param_Lo:
			pvalue = LoValue().SL;
		break;
			
		case Param_Imm:
			ssprintf( dest, "0x%04x", m_SignExtImm ? _Opcode_.Imm() : _Opcode_.ImmU() );
		return;

		case Param_AddrImm:		// Address Immediate (Rs + Imm()), used by load/store
			pvalue = RsValue().UL + _Opcode_.Imm();
		break;

		case Param_BranchOffset:
			pvalue = BranchTarget();
		break;
		
		case Param_JumpTarget:	// 26 bit immediate used as a jump target
			pvalue = JumpTarget();
		break;
		
		jNO_DEFAULT;
	}
	
	ssprintf( dest, "0x%8.8x", pvalue );
}

// ------------------------------------------------------------------------
//
template< RegField_t field >
InstParamType InstructionDiagnostic::AssignFieldParam() const
{
	if( (ReadsField( field ) == GPR_Invalid) && (WritesField( field ) == GPR_Invalid) ) return Param_None;

	if( field == RF_Rs )
	{
		if( ReadsMemory() || WritesMemory() )
		{
			// Rs is rendered in a special format.
			return Param_AddrImm;
		}
		else if( ReadsFs() || WritesFs() )
		{
			return Param_Fs;
		}
	}
	
	switch( field )
	{
		case RF_Rd: return Param_Rd;
		case RF_Rt: return Param_Rt;
		case RF_Rs: return Param_Rs;

		case RF_Hi: return Param_Hi;
		case RF_Lo: return Param_Lo;
	}
	
	return Param_None;		// should never get here anyway
}

// ------------------------------------------------------------------------
//
void InstDiag::GetParamLayout( InstParamType iparam[3] ) const
{
	// MIPS lays all instructions out in one of a few the basic patterns:
	//   InstName  Rd,[Rs],[Rt]
	//   InstName  Rt,[Rs]
	//   InstName  Rs
	//
	// Fields in brackets are optional, and any instruction with less than than 3
	// parameters could have either/or Imm or Sa parameter.
	//
	// Complication: Handling of address instructions, since the address parameters must
	// be handled in a special way.

	if( HasDelaySlot() && !ReadsRs() )
	{
		// Branch instructions which do not read Rs are J and JAL only:

		jASSUME( strcmp( m_Name, "J" ) == 0 || strcmp( m_Name, "JAL" ) == 0 );

		iparam[0] = Param_JumpTarget;
	}
	else
	{
		int cur = 0;	// current 'active' parameter

		iparam[cur] = AssignFieldParam<RF_Rd>();
		if( iparam[cur] != Param_None )
		{
			// Format could be:
			//   InstName  Rd,[Rs],[Rt]
			//   InstName  Rd,[Rt]

			cur++;
			iparam[cur] = AssignFieldParam<RF_Rs>(); if( iparam[cur] != Param_None ) cur++;
			iparam[cur] = AssignFieldParam<RF_Rt>(); if( iparam[cur] != Param_None ) cur++;
		}
		else
		{
			// Format should be either:
			//   InstName  Rt,[Rs]
			//   InstName  Rs

			iparam[cur] = AssignFieldParam<RF_Rt>(); if( iparam[cur] != Param_None ) cur++;
			iparam[cur] = AssignFieldParam<RF_Rs>(); if( iparam[cur] != Param_None ) cur++;
		}

		if( WritesHi() && WritesLo() )
		{
			iparam[cur++] = Param_HiLo;		// Used by DIV / MULT instructions [target only]
		}
		else
		{
			iparam[cur] = AssignFieldParam<RF_Hi>(); if( iparam[cur] != Param_None ) cur++;
			iparam[cur] = AssignFieldParam<RF_Lo>(); if( iparam[cur] != Param_None ) cur++;
		}

		if( m_ReadsSa )
		{
			// reading of Sa should only occur on instructions with 2 valid params above.
			jASSUME( cur < 3 );
			iparam[cur++] = Param_Sa;
		}
		else if( m_ReadsImm )
		{
			// reading of Imm8 should only occur on instructions with 2 valid params above.
			jASSUME( cur < 3 );
			iparam[cur++] = HasDelaySlot() ? Param_BranchOffset : Param_Imm;
		}
	}
}

// ------------------------------------------------------------------------
// Creates a string representation of the given instruction status.
//
void InstDiag::GetDisasm( string& dest ) const
{
	// Note: Allocate one extra here on purpose.  Code in GetParamLayout will write
	// to it (but only param_none values).
	InstParamType iparam[4] = { Param_None, Param_None, Param_None };
	GetParamLayout( iparam );

	ssprintf( dest, "%-8s", m_Name );

	bool needComma = false;
	string paramName;

	for( uint i=0; i<3; ++i )
	{
		GetParamName( iparam[i], paramName );

		if( paramName.empty() )
			GetParamValue( iparam[i], paramName );
			
		if( !paramName.empty() )
		{
			if( needComma )
				dest += ", ";

			dest += paramName;
			needComma = true;
		}
	}
}

// ------------------------------------------------------------------------
// Returns the values of the given string as comma delimited.
//
void InstDiag::GetValuesComment( string& dest ) const
{
	bool needComma = false;
	dest.clear();

	// Note: Allocate one extra here on purpose.  Code in GetParamLayout will write
	// to it (but only param_none values).
	InstParamType iparam[4] = { Param_None, Param_None, Param_None };
	GetParamLayout( iparam );

	string paramName;
	for( uint i=0; i<3; ++i )
	{
		if( !ParamIsRead( iparam[i] ) ) continue;

		GetParamName( iparam[i], paramName );
		if( paramName == "zero" ) continue;

		string paramValue;
		GetParamValue( iparam[i], paramValue );

		if( !paramName.empty() && !paramValue.empty() )
		{
			ssappendf( dest, "%s%hs=%hs", needComma ? ", " : "", &paramName, &paramValue );
			needComma = true;
		}
	}
}

}