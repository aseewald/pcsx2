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

namespace R3000Air
{
typedef InstructionDiagnostic InstDiag;		// makes life easier on the verboseness front.
typedef InstDiagInfo RetType;				// makes life even more easier on the verboseness front.

static const char *tbl_regname_gpr[32] =
{
	"zero",	"at", "v0", "v1", "a0", "a1", "a2", "a3",
	"t0",	"t1", "t2", "t3", "t4", "t5", "t6", "t7",
	"s0",	"s1", "s2", "s3", "s4", "s5", "s6", "s7",
	"t8",	"t9", "k0", "k1", "gp", "sp", "fp", "ra"
};

static const char *tbl_regname_cop0[32] =
{
	"Index",	"Random",	"EntryLo0",	"EntryLo1",	"Context",	"PageMask", "Wired",	"C0r7",
	"BadVaddr",	"Count",	"EntryHi",	"Compare",	"Status",	"Cause",	"EPC",		"PRId",
	"Config",	"C0r17",	"C0r18",	"C0r19",	"C0r20",	"C0r21",	"C0r22",	"C0r23",
	"Debug",	"Perf",		"C0r26",	"C0r27",	"TagLo",	"TagHi",	"ErrorPC",	"C0r31"
};


InstDiagInfo InstDiag::MakeInfoS( const char *name, ParamType one, ParamType two, ParamType three ) const
{
	InstDiagInfo info = { name, one, two, three, false };
	return info;
}

InstDiagInfo InstDiag::MakeInfoU( const char *name, ParamType one, ParamType two, ParamType three ) const
{
	InstDiagInfo info = { name, one, two, three, true };
	return info;
}

string Instruction::GetParamName( const uint pidx  ) const
{
	const char* pname = NULL;

	switch( m_Syntax->Param[pidx] )
	{
		case Param_None: break;

		case Param_Rt:
			pname  = tbl_regname_gpr[_Rt_];
		break;

		case Param_Rs:
			pname  = tbl_regname_gpr[_Rs_];
		break;
		
		case Param_Rd:
			pname  = tbl_regname_gpr[_Rd_];
		break;

		case Param_Sa:
			pname  = "Sa";
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
			pname = "HiLo";
		break;

		case Param_Imm:
		case Param_Imm16:		// Immediate, shifted up by 16. [used by LUI]
		break;

		case Param_AddrImm:		// Address Immediate (Rs + Imm()), used by load/store
			// TODO : Calculate label.
			//AddrImm()
			return fmt_string( "0x%4.4x(%s)", (s32)Imm(), tbl_regname_gpr[_Rs_] );
		break;

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
	
	return string( (pname != NULL) ? pname : "" );
}

string Instruction::GetParamValue( const uint pidx ) const
{
	// First parameter is always "target" form, so it never has a value.
	if( pidx == 0 || m_Syntax->Param[pidx] == Param_None ) return string();

	s32 pvalue = 0;

	switch( m_Syntax->Param[pidx] )
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
			pvalue = m_Syntax->IsUnsigned ? ImmU() : Imm();
		break;

		case Param_Imm16:		// Immediate, shifted up by 16. [used by LUI]
			pvalue = ImmU()<<16;
		break;

		case Param_AddrImm:		// Address Immediate (Rs + Imm()), used by load/store
			pvalue = AddrImm();
		break;

		case Param_BranchOffset:
			pvalue = BranchTarget();
		break;
		
		case Param_JumpTarget:	// 26 bit immediate used as a jump target
			pvalue = JumpTarget();
		break;
		
		jNO_DEFAULT;
	}
	
	return fmt_string( "0x%8.8x", pvalue );
}

// Creates a string representation of the given instruction status.
string Instruction::GetDisasm() const
{	
	string dest;
	ssprintf( dest, "%-8s", m_Syntax->Name );

	bool needComma = false;

	for( uint i=0; i<3; ++i )
	{
		string paramName( GetParamName( i ) );
		
		
		if( paramName.empty() )
			paramName = GetParamValue( i );
			
		if( !paramName.empty() )
		{
			if( needComma )
				dest += ", ";

			dest += paramName;
			needComma = true;
		}
	}
	return dest;
}

// Returns the values of the given string as an assembly comment
string Instruction::GetValuesComment() const
{
	string values;
	bool needComma = false;

	for( uint i=1; i<3; ++i )
	{
		string paramName( GetParamName( i ) );
		if( paramName == "zero" ) continue;

		string paramValue( GetParamValue( i ) );

		if( !paramName.empty() && !paramValue.empty() )
		{
			if( needComma )
				values += ", ";

			values += paramName + "=" + paramValue;
			needComma = true;
		}
	}

	return values;
}


#define Form_RtRsImm		Param_Rt, Param_Rs, Param_Imm
#define Form_RdRsRt			Param_Rd, Param_Rs, Param_Rt
#define Form_RsRt			Param_Rs, Param_Rt, Param_None
#define Form_RdRtSa			Param_Rd, Param_Rt, Param_Sa
#define Form_RdRtRs			Param_Rd, Param_Rt, Param_Rs
#define Form_Rd				Param_Rd, Param_None, Param_None
#define Form_Rs				Param_Rs, Param_None, Param_None
#define Form_RsRd			Param_Rs, Param_Rd, Param_None

#define Form_RsOffset		Param_None, Param_Rs, Param_BranchOffset
#define Form_RsRtOffset		Param_Rs, Param_Rt, Param_BranchOffset
#define Form_JumpTarget		Param_None, Param_JumpTarget, Param_None

#define Form_RtImm16		Param_Rt, Param_Imm16, Param_None
#define Form_RtAddrImm		Param_Rt, Param_AddrImm, Param_None
#define Form_RtStore		Param_None, Param_Rt, Param_AddrImm

#define Form_Cop0Load_RtRd		Param_Rt, Param_Fs, Param_None
#define Form_Cop0Store_RtRd		Param_None, Param_Rt, Param_Fs

#define Form_HiRd			Param_Hi, Param_Rd, Param_None
#define Form_LoRd			Param_Lo, Param_Rd, Param_None
#define Form_RdHi			Param_Rd, Param_Hi, Param_None
#define Form_RdLo			Param_Rd, Param_Lo, Param_None

#define Form_None			Param_None, Param_None, Param_None


#define MakeDiagS( name, form ) void InstDiag::name() \
{	static const InstDiagInfo secret_signed_stuff = { #name, form, false }; m_Syntax = &secret_signed_stuff; }

#define MakeDiagU( name, form ) void InstDiag::name() \
{	static const InstDiagInfo secret_unsigned_stuff = { #name, form, true }; m_Syntax = &secret_unsigned_stuff; }

/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, offset                                 *
*********************************************************/
MakeDiagS( BGEZ,	Form_RsOffset )
MakeDiagS( BGEZAL,	Form_RsOffset )
MakeDiagS( BGTZ,	Form_RsOffset )
MakeDiagS( BLEZ,	Form_RsOffset )
MakeDiagS( BLTZ,	Form_RsOffset )
MakeDiagS( BLTZAL,	Form_RsOffset )

/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, rt, offset                             *
*********************************************************/
MakeDiagS( BEQ,		Form_RsRtOffset )
MakeDiagS( BNE,		Form_RsRtOffset )

/*********************************************************
* Jump to target                                         *
* Format:  OP target                                     *
*********************************************************/
MakeDiagS( J,	Form_JumpTarget )
MakeDiagS( JAL,	Form_JumpTarget )

/*********************************************************
* Register jump                                          *
* Format:  OP rs, rd                                     *
*********************************************************/
MakeDiagU( JR,		Form_Rs )
MakeDiagU( JALR,	Form_RsRd )

/*********************************************************
* Arithmetic with immediate operand                      *
* Format:  OP rt, rs, immediate                          *
*********************************************************/
MakeDiagS( ADDI,	Form_RtRsImm )
MakeDiagS( ADDIU,	Form_RtRsImm )	// Note: Yes, ADDIU is *signed*
MakeDiagU( ANDI,	Form_RtRsImm )
MakeDiagU( ORI,		Form_RtRsImm )
MakeDiagU( XORI,	Form_RtRsImm )
MakeDiagS( SLTI,	Form_RtRsImm )
MakeDiagU( SLTIU,	Form_RtRsImm )


/*********************************************************
* Register arithmetic                                    *
* Format:  OP rd, rs, rt                                 *
*********************************************************/
MakeDiagS( ADD,		Form_RdRsRt )
MakeDiagS( SUB,		Form_RdRsRt )
MakeDiagS( ADDU,	Form_RdRsRt )	// Note: Yes, ADDU is *signed*
MakeDiagS( SUBU,	Form_RdRsRt )	// Note: Yes, SUBU is *signed*

MakeDiagU( AND,		Form_RdRsRt )
MakeDiagU( NOR,		Form_RdRsRt )
MakeDiagU( OR,		Form_RdRsRt )
MakeDiagU( XOR,		Form_RdRsRt )

MakeDiagS( SLT,		Form_RdRsRt )
MakeDiagU( SLTU,	Form_RdRsRt )	// tricky!  sign-extended immediate, unsigned comparison


/*********************************************************
* Register arithmetic & Register trap logic              *
* Format:  OP rs, rt                                     *
*********************************************************/
MakeDiagS( DIV,		Form_RsRt )
MakeDiagU( DIVU,	Form_RsRt )
MakeDiagS( MULT,	Form_RsRt )
MakeDiagU( MULTU,	Form_RsRt )

/*********************************************************
* Shift arithmetic with constant shift                   *
* Format:  OP rd, rt, sa                                 *
*********************************************************/
MakeDiagU( SLL,		Form_RdRtSa )
MakeDiagS( SRA,		Form_RdRtSa )
MakeDiagU( SRL,		Form_RdRtSa )

/*********************************************************
* Shift arithmetic with variant register shift           *
* Format:  OP rd, rt, rs                                 *
*********************************************************/
MakeDiagU( SLLV,	Form_RdRtRs )
MakeDiagS( SRAV,	Form_RdRtRs )
MakeDiagU( SRLV,	Form_RdRtRs )

/*********************************************************
* Load higher 16 bits of the first word in GPR with imm  *
* Format:  OP rt, immediate                              *
*********************************************************/
MakeDiagS( LUI,		Form_RtImm16 )

/*********************************************************
* Move from HI/LO to GPR                                 *
* Format:  OP rd                                         *
*********************************************************/
MakeDiagS( MFHI,	Form_RdHi )
MakeDiagS( MFLO,	Form_RdLo )

/*********************************************************
* Move from GPR to HI/LO                                 *
* Format:  OP rd                                         *
*********************************************************/
MakeDiagS( MTHI,	Form_HiRd )
MakeDiagS( MTLO,	Form_LoRd )

/*********************************************************
* Special purpose instructions                           *
* Format:  OP                                            *
*********************************************************/
MakeDiagS( BREAK,	Form_None )
MakeDiagS( RFE,		Form_None )
MakeDiagS( SYSCALL,	Form_None )

/*********************************************************
* Load and store for GPR                                 *
* Format:  OP rt, offset(base)                           *
*********************************************************/
MakeDiagS( LB,		Form_RtAddrImm )
MakeDiagS( LBU,		Form_RtAddrImm )
MakeDiagS( LH,		Form_RtAddrImm )
MakeDiagS( LHU,		Form_RtAddrImm )
MakeDiagS( LW,		Form_RtAddrImm )
MakeDiagS( LWL,		Form_RtAddrImm )
MakeDiagS( LWR,		Form_RtAddrImm )
MakeDiagS( SB,		Form_RtStore )
MakeDiagS( SH,		Form_RtStore )
MakeDiagS( SW,		Form_RtStore )
MakeDiagS( SWL,		Form_RtStore )
MakeDiagS( SWR,		Form_RtStore )

/*********************************************************
* Moves between GPR and COPx                             *
* Format:  OP rt, fs                                     *
*********************************************************/
MakeDiagS( MFC0,	Form_Cop0Load_RtRd )
MakeDiagS( MTC0,	Form_Cop0Store_RtRd )
MakeDiagS( CFC0,	Form_Cop0Load_RtRd )
MakeDiagS( CTC0,	Form_Cop0Store_RtRd )

/*********************************************************
* Unknown instruction (would generate an exception)      *
* Format:  ?                                             *
*********************************************************/
void InstDiag::Unknown() { static const InstDiagInfo omg = { "*** Bad OP ***", Param_None, Param_None, Param_None, false }; m_Syntax = &omg; }


/*********************************************************
* COP2 Instructions (PS1 GPU)                            *
* Format:  ?                                             *
*********************************************************/
/*
MakeDiagS( LWC2,	Form_RtAddrImm )
MakeDiagS( SWC2,	Form_RtAddrImm )

MakeDisF(disRTPS,		dName("RTPS"))
MakeDisF(disOP  ,		dName("OP"))
MakeDisF(disNCLIP,		dName("NCLIP"))
MakeDisF(disDPCS,		dName("DPCS"))
MakeDisF(disINTPL,		dName("INTPL"))
MakeDisF(disMVMVA,		dName("MVMVA"))
MakeDisF(disNCDS ,		dName("NCDS"))
MakeDisF(disCDP ,		dName("CDP"))
MakeDisF(disNCDT ,		dName("NCDT"))
MakeDisF(disNCCS ,		dName("NCCS"))
MakeDisF(disCC  ,		dName("CC"))
MakeDisF(disNCS ,		dName("NCS"))
MakeDisF(disNCT  ,		dName("NCT"))
MakeDisF(disSQR  ,		dName("SQR"))
MakeDisF(disDCPL ,		dName("DCPL"))
MakeDisF(disDPCT ,		dName("DPCT"))
MakeDisF(disAVSZ3,		dName("AVSZ3"))
MakeDisF(disAVSZ4,		dName("AVSZ4"))
MakeDisF(disRTPT ,		dName("RTPT"))
MakeDisF(disGPF  ,		dName("GPF"))
MakeDisF(disGPL  ,		dName("GPL"))
MakeDisF(disNCCT ,		dName("NCCT"))

MakeDisF(disMFC2,		dName("MFC2"); dGPR(_Rt_);)
MakeDisF(disCFC2,		dName("CFC2"); dGPR(_Rt_);)
MakeDisF(disMTC2,		dName("MTC2"))
MakeDisF(disCTC2,		dName("CTC2"))
*/

}