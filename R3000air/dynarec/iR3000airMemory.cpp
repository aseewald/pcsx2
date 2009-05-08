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

namespace IopMemory
{
	namespace Internal
	{
		// ------------------------------------------------------------------------
		// allocate one page for our templated naked indirect dispatcher function.
		// this *must* be a full page, since we'll give it execution permission later.
		// If it were smaller than a page we'd end up allowing execution rights on some
		// other vars additionally (bad!).
		//
		PCSX2_ALIGNED( 0x1000, static u8 m_IndirectDispatchers[0x1000] );

		template< typename T >
		static uint GetOperandIndex()
		{
			switch( sizeof(T) )
			{
				case 1: return 0;
				case 2: return 1;
				case 4: return 2;
				jNO_DEFAULT;
			}
		}

		// ------------------------------------------------------------------------
		// mode        - 0 for read, 1 for write!
		// operandsize - 0 thru 2 represents 8, 16, and 32 bits.
		//
		static u8* GetIndirectDispatcherPtr( int mode, int operandsize )
		{
			// Each dispatcher is aligned to 64 bytes.  The actual dispatchers are only like
			// 20-some bytes each, but 64 byte alignment on functions that are called
			// more frequently than a hot sex hotline at 1:15am is probably a good thing.

			// 3*64?  Because 3 operand types per each mode :D
			
			return &m_IndirectDispatchers[(mode*(3*64)) + (operandsize*64)];
		}

		template< typename T >
		static void CallIndirectDispatcher( int mode )
		{
			xCALL( GetIndirectDispatcherPtr(mode, GetOperandIndex<T>()) );
		}

		// ------------------------------------------------------------------------
		// Generates a JB instruction that targets the appropriate templated instance of
		// the vtlb Indirect Dispatcher.
		//
		template< typename T >
		static void DynGen_IndirectDispatch( int mode )
		{
			xJB( GetIndirectDispatcherPtr( mode, GetOperandIndex<T>() ) );
		}
	}
	
	using namespace Internal;

	//////////////////////////////////////////////////////////////////////////////////////////
	// Initializes the 'templated' handlers for indirect memory operations.
	//
	void recInitialize()
	{
		// In case init gets called multiple times:
		HostSys::MemProtect( m_IndirectDispatchers, 0x1000, Protect_ReadWrite, false );

		// clear the buffer to 0xcc (easier debugging).
		memset_8<0xcc,0x1000>( m_IndirectDispatchers );

		u8* baseptr = m_IndirectDispatchers;

		for( int mode=0; mode<2; ++mode )
		{
			for( int bits=0; bits<3; ++bits )
			{
				xSetPtr( GetIndirectDispatcherPtr( mode, bits ) );

				// redirect to the indirect handler, which is a __fastcall C++ function.
				// [ecx is address, edx is data (writes only, ignored for reads)]
				xCALL( ptr32[(eax*4) + tbl_IndirectHandlers] );
				xJMP( ebx );
			}
		}

		HostSys::MemProtect( m_IndirectDispatchers, 0x1000, Protect_ReadOnly, true );
	}

	struct ConstLoadOpInfo
	{
		const u32 addr;
		const u32 masked;
		const sptr xlated;
		
		ConstLoadOpInfo( const IntermediateInstruction& info ) :
			addr( info.SrcRs.Imm + info.GetImm() ),
			masked( addr & AddressMask ),
			xlated( tbl_Translation.Contents[masked / PageSize] )
		{
		}
	};

	//////////////////////////////////////////////////////////////////////////////////////////
	// Loads address in Rs+Imm into Rt.
	//
	template< typename T >
	static void DynGen_LoadOp( const IntermediateInstruction& info )
	{
		if( info.IsConstRs() )
		{
			// ---------------------------------------------------
			//    Constant Propagation on the Src Address (Rs)
			// ---------------------------------------------------

			const ConstLoadOpInfo constinfo( info );

			if( constinfo.xlated > HandlerId_Maximum )
				info.MoveToRt( ptrT[constinfo.xlated] );
			else
			{
				xMOV( ecx, constinfo.addr );
				CallIndirectDispatcher<T>( 0 );
				switch( sizeof(T) )
				{
					case 1:	info.MoveToRt( al ); break;
					case 2:	info.MoveToRt( ax ); break;				
					case 4:	info.MoveToRt( eax ); break;
				}
			}
		}
		else
		{
			// --------------------------------------------------
			//    Full Implementation (no const optimizations)
			// --------------------------------------------------

			info.MoveRsTo( ecx );
			info.AddImmTo( ecx );

			xMOV( eax, ecx );
			xAND( eax, AddressMask );
			xSHR( eax, PageBitShift );

			xMOV( ebx, 0xcdcdcdcd );
			uptr* writeback = ((uptr*)xGetPtr()) - 1;

			xMOV( eax, ptr[(eax*4)+tbl_Translation.Contents] );
			xCMP( eax, HandlerId_Maximum );
			DynGen_IndirectDispatch<T>( 0 );

			// Direct Access on Load Operation:

			xAND( ecx, PageMask );
			info.MoveToRt( ptrT[ecx+eax] );

			*writeback = (uptr)xGetPtr();		// return target for indirect's call/ret
		}
	}

	//////////////////////////////////////////////////////////////////////////////////////////
	//
	template< typename T>
	static void DynGen_DirectWrite( const IntermediateInstruction& info, const ModSibStrict<T>& dest )
	{
		xTEST( ptr32[&iopRegs.CP0.n.Status], 0x10000 );
		xForwardJNZ8 skipWrite;

		// The data being written is either a register or an immediate, held in Rt.

		info.MoveRtTo( dest );
		skipWrite.SetTarget();
	}

	//////////////////////////////////////////////////////////////////////////////////////////
	//
	template< typename T >
	static void DynGen_StoreOp( const IntermediateInstruction& info )
	{
		if( info.IsConstRs() )
		{
			// ---------------------------------------------------
			//    Constant Propagation on the Dest Address (Rs)
			// ---------------------------------------------------

			const ConstLoadOpInfo constinfo( info );
			
			if( constinfo.xlated > HandlerId_Maximum )
				DynGen_DirectWrite( info, ptrT[constinfo.xlated] );
			else
			{
				// Indirect writes don't need a check against the CP0 write bit.
				xMOV( ecx, constinfo.addr );
				info.MoveRtTo( edx );
				CallIndirectDispatcher<T>( 1 );
			}
		}
		else
		{
			// --------------------------------------------------
			//    Full Implementation (no const optimizations)
			// --------------------------------------------------

			info.MoveRsTo( ecx );
			info.AddImmTo( ecx );

			xMOV( eax, ecx );
			xAND( eax, AddressMask );
			xSHR( eax, PageBitShift );

			xMOV( ebx, 0xcdcdcdcd );
			uptr* writeback = ((uptr*)xGetPtr()) - 1;

			xMOV( eax, ptr[(eax*4) + tbl_Translation.Contents] );
			xCMP( eax, HandlerId_Maximum );
			DynGen_IndirectDispatch<T>( 1 );

			// Direct Access on Load Operation:

			xAND( ecx, PageMask );
			DynGen_DirectWrite( info, ptrT[ecx+eax] );

			*writeback = (uptr)xGetPtr();		// return target for indirect's call/ret
		}
	}
	
	//////////////////////////////////////////////////////////////////////////////////////////
	//
	static void DynGen_LoadWord_LorR( const IntermediateInstruction& info, bool IsLoadRight )
	{
		if( info.IsConstRs() )
		{
			// ---------------------------------------------------
			//    Constant Propagation on the Src Address (Rs)
			// ---------------------------------------------------

			const ConstLoadOpInfo constinfo( info );
			const uint shift		= (constinfo.addr & 3) << 3;
			const uint aligned_addr	= constinfo.addr & ~3;
			const uint rtmask		= IsLoadRight ? (0xffffff00 << (24-shift)) : (0x00ffffff >> shift);
			const uint memshift		= IsLoadRight ? shift : (24 - shift);
			
			// Load from memory, merge, and write back.
			
			if( constinfo.xlated > HandlerId_Maximum )
				xMOV( eax, ptr32[constinfo.xlated] );
			else
			{
				xMOV( ecx, aligned_addr );
				CallIndirectDispatcher<u32>( 0 );
			}
			
			// Merge!  and WriteBack!
			// code: SetRt_UL( (GetRt().UL & rtmask) | (mem << memshift) );
			
			info.MoveRtTo( ebx );
			if( IsLoadRight )	xSHR( eax, memshift );
			else				xSHL( eax, memshift );

			xAND( ebx, rtmask );
			xOR( eax, ebx );
			info.MoveToRt( eax );
		}
		else
		{
			// --------------------------------------------------
			//    Full Implementation (no const optimizations)
			// --------------------------------------------------

			info.MoveRsTo( ecx );
			info.AddImmTo( ecx );

			xMOV( eax, ecx );

			xAND( eax, AddressMask & ~3 );	// mask off bottom bits as well as top!
			xSHR( eax, PageBitShift );
			xMOV( ebx, 0xcdcdcdcd );
			uptr* writeback = ((uptr*)xGetPtr()) - 1;

			xMOV( eax, ptr[(eax*4)+tbl_Translation.Contents] );
			xPUSH( ecx );		// save the original address
			xAND( ecx, ~3 );	// mask off bottom bits.
			xCMP( eax, HandlerId_Maximum );

			DynGen_IndirectDispatch<u32>( 0 );

			// Direct Access on Load Operation (loads eax with data):

			xAND( ecx, PageMask );
			xMOV( eax, ptr32[ecx+eax] );

			// Post LoadOp: Calculate shift, aligned address, rtmask, and memshift:
			
			*writeback = (uptr)xGetPtr();		// return target for indirect's call/ret
			xPOP( ecx );
			xAND( ecx, 3 );
			xSHL( ecx, 3 );		// ecx == shift

			info.MoveRtTo( ebx );
			
			if( IsLoadRight )
			{
				xSHR( eax, cl );	// ecx == shift/memshift

				xMOV( edx, 0xffffff00 );
				xNEG( ecx );
				xADD( ecx, 24 );	// ecx == rtshift
				xSHL( edx, cl );	// edx == rtmask

				// Operation: SetRt_UL( (GetRt().UL & rtmask) | (mem << memshift) );
				xAND( ebx, edx );
				xSHL( eax, cl );
				xOR( eax, ebx );
			}
			else	// LoadWordLeft!
			{
				xMOV( edx, 0x00ffffff );
				xSHR( edx, cl );	// edx == rtmask

				xNEG( ecx );
				xADD( ecx, 24 );	// ecx == memshift

				// Operation: SetRt_UL( (GetRt().UL & rtmask) | (mem << memshift) );
				xAND( ebx, edx );
				xSHL( eax, cl );
				xOR( eax, ebx );
			}

			info.MoveToRt( eax );
		}
	}

	//////////////////////////////////////////////////////////////////////////////////////////
	//
	static void DynGen_StoreWord_LorR( const IntermediateInstruction& info, bool IsStoreRight )
	{
		if( info.IsConstRs() )
		{
			// ---------------------------------------------------
			//    Constant Propagation on the Src Address (Rs)
			// ---------------------------------------------------

			const ConstLoadOpInfo constinfo( info );
			const uint shift		= (constinfo.addr & 3) << 3;
			const uint aligned_addr	= constinfo.addr & ~3;

			const uint memmask		= IsStoreRight ? (0x00ffffff >> (24-shift)) : (0xffffff00 << shift);
			const uint rtshift		= IsStoreRight ? shift : (24 - shift);
			
			// Load from memory, merge, and write back.
			
			if( constinfo.xlated > HandlerId_Maximum )
				xMOV( eax, ptr32[constinfo.xlated] );
			else
			{
				xMOV( ecx, aligned_addr );
				CallIndirectDispatcher<u32>( 0 );
			}
			
			// Merge!  and WriteBack!  [Right inverts the Rt shift]
			//	MemoryWrite32( aligned_addr,
			//		(( GetRt().UL >> rtshift )) | (mem & memmask)
			//	);
			
			info.MoveRtTo( ebx );
			if( IsStoreRight )	xSHL( ebx, rtshift );	// inverted Rt shift!
			else				xSHR( ebx, rtshift );
			xAND( eax, memmask );
			xOR( eax, ebx );

			// Store the value to memory -->

			if( constinfo.xlated > HandlerId_Maximum )
				xMOV( ptr32[constinfo.xlated], eax );
			else
			{
				xMOV( ecx, aligned_addr );
				xMOV( edx, eax );
				CallIndirectDispatcher<u32>( 1 );
			}
		}
		else
		{
			// --------------------------------------------------
			//    Full Implementation (no const optimizations)
			// --------------------------------------------------

			info.MoveRsTo( ecx );
			info.AddImmTo( ecx );

			xMOV( eax, ecx );

			xAND( eax, AddressMask & ~3 );	// mask off bottom bits as well as top!
			xSHR( eax, PageBitShift );
			xMOV( ebx, 0xcdcdcdcd );
			uptr* writeback = ((uptr*)xGetPtr()) - 1;

			xMOV( eax, ptr[(eax*4)+tbl_Translation.Contents] );
			xPUSH( ecx );		// save the original address
			xPUSH( ecx );		// twice!!
			xPUSH( eax );		// and save the translated address
			xAND( ecx, ~3 );	// mask off bottom bits.
			xCMP( eax, HandlerId_Maximum );

			DynGen_IndirectDispatch<u32>( 0 );

			// Direct Access on Load Operation (loads eax with data):

			xAND( ecx, PageMask );
			xMOV( eax, ptr32[ecx+eax] );

			// Post LoadOp: Calculate shift, aligned address, rtmask, and memshift:
			
			*writeback = (uptr)xGetPtr();		// return target for indirect's call/ret
			xPOP( ecx );
			xAND( ecx, 3 );
			xSHL( ecx, 3 );		// ecx == shift

			info.MoveRtTo( edx );

			if( IsStoreRight )
			{
				xSHL( edx, cl );

				xMOV( ebx, 0x00ffffff );
				xNEG( ecx );
				xADD( ecx, 24 );
				xSHR( ebx, cl );	// edx == memmask  (0x00ffffff >> (24 - shift))

				xAND( eax, ebx );
				xOR( edx, eax );
			}
			else	// LoadWordLeft!
			{
				xMOV( ebx, 0xffffff00 );
				xSHL( ebx, cl );	// edx == memmask
				xAND( eax, ebx );	// eax == mem & (0xffffff00 << shift)

				xNEG( ecx );
				xADD( ecx, 24 );	// ecx == rtshift
				xSHR( edx, cl );

				xOR( edx, eax );	// edx == value to be written back to memory
			}

			// Store to Memory -->
			
			xPOP( ecx );		// original physical address
			xPOP( eax );		// translated address
			xMOV( ebx, 0xcdcdcdcd );
			writeback = ((uptr*)xGetPtr()) - 1;
			xCMP( eax, HandlerId_Maximum );

			DynGen_IndirectDispatch<u32>( 1 );

			// Direct Access on Load Operation:
			xAND( ecx, PageMask );
			DynGen_DirectWrite( info, ptr32[ecx+eax] );

			*writeback = (uptr)xGetPtr();		// return target for indirect's call/ret
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void recLB( const IntermediateInstruction& info )
{
	IopMemory::DynGen_LoadOp<u8>( info );
}

void recLH( const IntermediateInstruction& info )
{
	IopMemory::DynGen_LoadOp<u16>( info );
}

void recLW( const IntermediateInstruction& info )
{
	IopMemory::DynGen_LoadOp<u32>( info );
}

void recLWL( const IntermediateInstruction& info )
{
	IopMemory::DynGen_LoadWord_LorR( info, false );
}

void recLWR( const IntermediateInstruction& info )
{
	IopMemory::DynGen_LoadWord_LorR( info, true );
}
//////////////////////////////////////////////////////////////////////////////////////////
// Loads address in Rs+Imm into Rt.
//
void recSB( const IntermediateInstruction& info )
{
	IopMemory::DynGen_StoreOp<u32>( info );
}

void recSH( const IntermediateInstruction& info )
{
	IopMemory::DynGen_StoreOp<u32>( info );
}

void recSW( const IntermediateInstruction& info )
{
	IopMemory::DynGen_StoreOp<u32>( info );
}

void recSWL( const IntermediateInstruction& info )
{
	IopMemory::DynGen_StoreWord_LorR( info, false );
}

void recSWR( const IntermediateInstruction& info )
{
	IopMemory::DynGen_StoreWord_LorR( info, true );
}

