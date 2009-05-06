
#pragma once

#include "ix86/ix86.h"

using namespace x86Emitter;

//////////////////////////////////////////////////////////////////////////////////////////
//
class xImmOrReg
{
	xRegister32 m_reg;
	u32 m_imm;
	
public:
	xImmOrReg( u32 imm, const xRegister32& reg=xRegister32::Empty ) :
		m_reg( reg ), m_imm( imm ) { }
		
	const xRegister32& GetReg() const { return m_reg; }
	const u32 GetImm() const { return m_imm; }
	bool IsReg() const { return !m_reg.IsEmpty(); }
};

//////////////////////////////////////////////////////////////////////////////////////////
//
class xDirectOrIndirect
{
	xRegister32 m_RegDirect;
	ModSibBase m_MemIndirect;
	
public:
	xDirectOrIndirect( const xRegister32& srcreg ) :
		m_RegDirect( srcreg ), m_MemIndirect( 0 ) {}

	xDirectOrIndirect( const ModSibBase& srcmem ) :
		m_RegDirect( xRegister32::Empty ), m_MemIndirect( srcmem ) {}
		
	const xRegister32& GetReg() const { return m_RegDirect; }
	const ModSibBase& GetMem() const { return m_MemIndirect; }
	bool IsReg() const { return !m_RegDirect.IsEmpty(); }
};

//////////////////////////////////////////////////////////////////////////////////////////
// xAnyOperand -
//
// Note: Styled as a struct so that we can use convenient C-style array initializers.
//
struct xAnyOperand
{
	xRegister32 RegDirect;
	ModSibBase MemIndirect;
	u32 Imm;
	bool IsConst;
};

//////////////////////////////////////////////////////////////////////////////////////////
// Contains information about each instruction of the original MIPS code in a sort of
// "halfway house" status -- with partial x86 register mapping.
//
class IntermediateInstruction
{
	// Callback to emit the instruction in question.
	void (*Emitter)();

public:
	const xDirectOrIndirect DestRs;
	const xDirectOrIndirect DestRt;
	const xDirectOrIndirect DestRd;

	// Source operands can either be const/non-const and can have either a register
	// or memory operand allocated to them (at least one but never both).  Even if
	// a register is const, it may be allocated an x86 register for performance
	// reasons [in the case of a const that is reused many times, for example]

	const xAnyOperand SrcRs;
	const xAnyOperand SrcRt;
	const xAnyOperand SrcRd;
	
	const xImmOrReg ixImm;
	const GprStatus IsConst;
	
public:
	s32 GetImm() const { return ixImm.GetImm(); }

	void AddImmTo( const xRegister32& dest ) const 
	{
		if( !ixImm.IsReg() )
			xADD( dest, ixImm.GetReg() );
		else
			xADD( dest, ixImm.GetImm() );
	}

	// ------------------------------------------------------------------------
	void MoveRsTo( const xRegister32& dest ) const 
	{
		if( SrcRs.IsConst )
			xMOV( dest, SrcRs.Imm );

		else if( !SrcRs.RegDirect.IsEmpty() )
			xMOV( dest, SrcRs.RegDirect );

		else
			xMOV( dest, SrcRs.MemIndirect );
	}

	void MoveRtTo( const xRegister32& dest ) const 
	{
		if( SrcRt.IsConst )
			xMOV( dest, SrcRt.Imm );

		else if( !SrcRt.RegDirect.IsEmpty() )
			xMOV( dest, SrcRt.RegDirect );

		else
			xMOV( dest, SrcRt.MemIndirect );
	}

	// ------------------------------------------------------------------------
	void MoveRsTo( const ModSibStrict<u32>& dest, const xRegister32 tempreg=eax ) const 
	{
		if( SrcRs.IsConst )
			xMOV( dest, SrcRs.Imm );

		else if( !SrcRs.RegDirect.IsEmpty() )
			xMOV( dest, SrcRs.RegDirect );

		else
		{
			// pooh.. gotta move the 'hard' way :(
			xMOV( tempreg, SrcRs.MemIndirect );
			xMOV( dest, tempreg );
		}
	}

	void MoveRtTo( const ModSibStrict<u32>& dest, const xRegister32 tempreg=eax ) const 
	{
		if( SrcRt.IsConst )
			xMOV( dest, SrcRt.Imm );

		else if( !SrcRt.RegDirect.IsEmpty() )
			xMOV( dest, SrcRt.RegDirect );

		else
		{
			// pooh.. gotta move the 'hard' way :(
			xMOV( tempreg, SrcRs.MemIndirect );
			xMOV( dest, tempreg );
		}
	}

	// ------------------------------------------------------------------------
	void MoveToRt( const xRegister32& src ) const 
	{
		if( !DestRt.IsReg() )
			xMOV( DestRt.GetReg(), src );
		else
			xMOV( DestRt.GetMem(), src );
	}
};

//////////////////////////////////////////////////////////////////////////////////////////
//
class IntermediateBlock
{
};

