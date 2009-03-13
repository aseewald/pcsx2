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

#include "Exceptions.h"

namespace R3000Exception
{
	using Exception::Ps2Generic;

	//////////////////////////////////////////////////////////////////////////////////
	// Abstract base class for R3000 exceptions; contains the psxRegs instance at the
	// time the exception is raised.
	//
	class BaseExcept : public Ps2Generic
	{
	public:
		const R3000Air::Registers cpuState;
		const R3000Air::Instruction Inst;

	protected:
		const bool m_IsDelaySlot;

	public:
		virtual ~BaseExcept() throw()=0;

		explicit BaseExcept( const R3000Air::Instruction& inst, const std::string& msg ) :
			Exception::Ps2Generic( "(IOP) " + msg ),
			cpuState( R3000Air::iopRegs ),
			Inst( inst ),
			m_IsDelaySlot( iopRegs.IsDelaySlot )
		{
		}
		
		u32 GetPc() const { return cpuState.pc; }
		bool IsDelaySlot() const { return m_IsDelaySlot; }
	};

	//////////////////////////////////////////////////////////////////////////////////
	//
	class AddressError : public BaseExcept
	{
	public:
		const bool OnWrite;
		const u32 Address;

	public:
		virtual ~AddressError() throw() {}

		explicit AddressError( const R3000Air::Instruction& inst, u32 ps2addr, bool onWrite ) :
			BaseExcept( inst, fmt_string( "Address error, addr=0x%x [%s]", ps2addr, onWrite ? "store" : "load" ) ),
			OnWrite( onWrite ),
			Address( ps2addr )
		{}
	};
	
	//////////////////////////////////////////////////////////////////////////////////
	//
	class TLBMiss : public BaseExcept
	{
	public:
		const bool OnWrite;
		const u32 Address;

	public:
		virtual ~TLBMiss() throw() {}

		explicit TLBMiss( const R3000Air::Instruction& inst, u32 ps2addr, bool onWrite ) :
			BaseExcept( inst, fmt_string( "Tlb Miss, addr=0x%x [%s]", ps2addr, onWrite ? "store" : "load" ) ),
			OnWrite( onWrite ),
			Address( ps2addr )
		{}
	};

	//////////////////////////////////////////////////////////////////////////////////
	//
	class BusError : public BaseExcept
	{
	public:
		const bool OnWrite;
		const u32 Address;

	public:
		virtual ~BusError() throw() {}

		//
		explicit BusError( const R3000Air::Instruction& inst, u32 ps2addr, bool onWrite ) :
			BaseExcept( inst, fmt_string( "Bus Error, addr=0x%x [%s]", ps2addr, onWrite ? "store" : "load" ) ),
			OnWrite( onWrite ),
			Address( ps2addr )
		{}
	};
	
	//////////////////////////////////////////////////////////////////////////////////
	//
	class SystemCall : public BaseExcept
	{
	public:
		virtual ~SystemCall() throw() {}

		explicit SystemCall( const R3000Air::Instruction& inst ) :
			BaseExcept( inst, "SystemCall [SYSCALL]" )
		{}
	};

	//////////////////////////////////////////////////////////////////////////////////
	//
	class Trap : public BaseExcept
	{
	public:
		const u16 TrapCode;

	public:
		virtual ~Trap() throw() {}

		// Generates a trap for immediate-style Trap opcodes
		explicit Trap( const R3000Air::Instruction& inst ) :
			BaseExcept( Inst, "Trap" ),
			TrapCode( 0 )
		{}

		// Generates a trap for register-style Trap instructions, which contain an
		// error code in the opcode
		explicit Trap( const R3000Air::Instruction& inst, u16 trapcode ) :
			BaseExcept( inst, "Trap" ),
			TrapCode( trapcode )
		{}
	};

	//////////////////////////////////////////////////////////////////////////////////
	//
	class Break : public BaseExcept
	{
	public:
		virtual ~Break() throw() {}

		explicit Break( const R3000Air::Instruction& inst ) :
			BaseExcept( inst, "Break Instruction" )
		{}
	};
	
	//////////////////////////////////////////////////////////////////////////////////
	//
	class Overflow : public BaseExcept
	{
	public:
		virtual ~Overflow() throw() {}

		explicit Overflow( const R3000Air::Instruction& inst ) :
			BaseExcept( inst, "Overflow" )
		{}
	};

	//////////////////////////////////////////////////////////////////////////////////
	//
	class DebugBreakpoint : public BaseExcept
	{
	public:
		virtual ~DebugBreakpoint() throw() {}

		explicit DebugBreakpoint( const R3000Air::Instruction& inst ) :
			BaseExcept( inst, "Debug Breakpoint" )
		{}
	};
}
