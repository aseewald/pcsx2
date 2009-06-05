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

using namespace IopMemory;

namespace Internal
{
	using namespace IopMemory;

	// ------------------------------------------------------------------------
	// allocate one page for our templated naked indirect dispatcher function.
	// this *must* be a full page, since we'll give it execution permission later.
	// If it were smaller than a page we'd end up allowing execution rights on some
	// other vars additionally (bad!).
	//
	PCSX2_ALIGNED( 0x1000, static u8 m_IndirectDispatchers[0x1000] );

	// Needs to be enough room for each function .. 32 bytes should be plenty!
	static const int DispatchAlign = 32;

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
	static u8* GetIndirectDispatcherPtr( int mode, int operandsize, bool signExtend )
	{
		// Each dispatcher is aligned to 64 bytes.  The actual dispatchers are only like
		// 20-some bytes each, but 64 byte alignment on functions that are called
		// more frequently than a hot sex hotline at 1:15am is probably a good thing.

		if( operandsize == 2 ) signExtend = false;	// no sign extension on 32 bits.
		return &m_IndirectDispatchers[(mode*(3*DispatchAlign)) + (operandsize*DispatchAlign*2) + ((signExtend?1:0)*DispatchAlign)];
	}

	template< typename T >
	static void CallIndirectDispatcher( int mode, bool signExtend=false )
	{
		xCALL( GetIndirectDispatcherPtr( mode, GetOperandIndex<T>(), signExtend ) );
	}

	// ------------------------------------------------------------------------
	// Generates a JB instruction that targets the appropriate templated instance of
	// the vtlb Indirect Dispatcher.
	//
	template< typename T >
	static void DynGen_IndirectDispatchJump( int mode, bool signExtend=false )
	{
		xCMP( eax, HandlerId_Maximum );
		xJB( GetIndirectDispatcherPtr( mode, GetOperandIndex<T>(), signExtend ) );
	}
	
	// bits - range values 0 to 2, representing 8, 16, and 32 respectively.
	static void DynGen_IndirectDispatcherFunction( int mode, int bits, bool signExtend )
	{
		xSetPtr( GetIndirectDispatcherPtr( mode, bits, signExtend ) );

		// redirect to the indirect handler, which is a __fastcall C++ function.
		// [ecx is address, edx is data (writes only, ignored for reads)]
		xCALL( ptr32[(eax*4) + tbl_IndirectHandlers] );
		
		switch( bits )
		{
			case 0:
				if( signExtend )
					xMOVSX( eax, al );
				else
					xMOVZX( eax, al );
			break;

			case 1:
				if( signExtend )
					xMOVSX( eax, ax );
				else
					xMOVZX( eax, ax );
			break;
		}
		xJMP( ebx );
	}
}

using namespace Internal;

struct ConstLoadOpInfo
{
	const u32 addr;
	const u32 masked;
	const sptr xlated;
	
	ConstLoadOpInfo( const IntermediateRepresentation& info ) :
		addr( info.GetConstRs() + info.GetImm() ),
		masked( addr & AddressMask ),
		xlated( tbl_Translation.Contents[masked / PageSize] )
	{
	}
};

//////////////////////////////////////////////////////////////////////////////////////////
//
template< typename T>
static void DynGen_DirectWrite( const IntermediateRepresentation& info, const ModSibStrict<T>& dest )
{
	xTEST( ptr32[&iopRegs.CP0.n.Status], 0x10000 );
	xForwardJNZ8 skipWrite;

	xMOV( dest, edx );
	skipWrite.SetTarget();
}

//////////////////////////////////////////////////////////////////////////////////////////
//
namespace recLoad_ConstNone
{
	static void RegMapInfo( IntermediateRepresentation& info )
	{
		RegMapInfo_Strict& rs( info.RegOpts.UseStrictMode() );
		rs.EntryMap.Rs = ecx;
		rs.ExitMap.eax = StrictEM_Rt;
	}

	template< typename T >
	static void Emit( const IntermediateRepresentation& info )
	{
		DynarecAssume( RegRs == ecx, info.Inst,
			"Recompiler failed to prepare strict eax/ecx mappings [ConstRs form]." );

		xMOV( eax, ecx );
		xAND( eax, AddressMask );
		xSHR( eax, PageBitShift );

		xMOV( ebx, 0xcdcdcdcd );
		uptr* writeback = ((uptr*)xGetPtr()) - 1;

		xMOV( eax, ptr[(eax*4)+tbl_Translation.Contents] );
		DynGen_IndirectDispatchJump<T>( 0, info.SignExtendsResult() );

		// Direct Access on Load Operation:
		xAND( ecx, PageMask );
		info.SignExtendedMove( eax, ptrT[ecx+eax] );

		*writeback = (uptr)xGetPtr();		// return target for indirect's call/ret
		info.MoveToRt( eax );				// recompiler handles it for us
	}
	
	IMPL_GetInterfaceTee( typename )
};

//////////////////////////////////////////////////////////////////////////////////////////
//
namespace recLoad_ConstRs
{
	static void RegMapInfo( IntermediateRepresentation& info )
	{
		RegMapInfo_Strict& rs( info.RegOpts.UseStrictMode() );
		const ConstLoadOpInfo constinfo( info );

		// ecx/edx get clobbered by the indirect handler, but are preserved on direct memOps:
		if( constinfo.xlated > HandlerId_Maximum )
		{
			rs.ExitMap.ecx = StrictEM_Untouched;
			rs.ExitMap.edx = StrictEM_Untouched;
		}

		rs.ExitMap.eax = StrictEM_Rt;
	}

	template< typename T >
	static void Emit( const IntermediateRepresentation& info )
	{
		const ConstLoadOpInfo constinfo( info );

		if( constinfo.xlated > HandlerId_Maximum )
		{
			info.SignExtendedMove( eax, ptrT[constinfo.xlated] );
		}
		else
		{
			xMOV( ecx, constinfo.addr );
			CallIndirectDispatcher<T>( 0, info.SignExtendsResult() );
			info.SignExtendEax<T>();
		}
		info.MoveToRt( eax );
	}

	IMPL_GetInterfaceTee( typename )
};

//////////////////////////////////////////////////////////////////////////////////////////
//
namespace recLoadWord_LorR_ConstNone
{
	static void RegMapInfo( IntermediateRepresentation& info )
	{
		RegMapInfo_Strict& rs( info.RegOpts.UseStrictMode() );

		// Rt is unmapped/flushed here because Rt is used only after we've already clobbered
		// all the regular regs (due to LWL / LWR needing to enact a memory operation), so
		// might as well force the rec to flush it, and then reload it when ready.

		rs.EntryMap.Rs = ecx;
		rs.ExitMap.eax = StrictEM_Rt;
	}

	template< bool IsLoadRight >
	static void Emit( const IntermediateRepresentation& info )
	{
		xADD( ecx, info.ixImm );
		xMOV( eax, ecx );

		xAND( eax, AddressMask & ~3 );	// mask off bottom bits as well as top!
		xSHR( eax, PageBitShift );
		xMOV( ebx, 0xcdcdcdcd );
		uptr* writeback = ((uptr*)xGetPtr()) - 1;

		xMOV( eax, ptr[(eax*4)+tbl_Translation.Contents] );
		xPUSH( ecx );		// save the original address
		xAND( ecx, ~3 );	// mask off bottom bits.
		DynGen_IndirectDispatchJump<u32>( 0 );

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

	IMPL_GetInterfaceTee( bool )
};


//////////////////////////////////////////////////////////////////////////////////////////
//
namespace recLoadWord_LorR_ConstRs
{
	static void RegMapInfo( IntermediateRepresentation& info )
	{
		RegMapInfo_Strict& rs( info.RegOpts.UseStrictMode() );

		rs.EntryMap.Rt = ebx;		// use ebx since it's preserved across indirect handlers.
		rs.ExitMap.eax = StrictEM_Rt;

		// Note: ecx/edx are preserved for direct ops:
		const ConstLoadOpInfo constinfo( info );
		if( constinfo.xlated > HandlerId_Maximum )
		{
			rs.ExitMap.edx = StrictEM_Untouched;
			rs.ExitMap.ecx = StrictEM_Untouched;
		}
	}
	
	template< bool IsLoadRight >
	static void Emit( const IntermediateRepresentation& info )
	{
		jASSUME( RegRt == ebx );

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

		if( IsLoadRight )	xSHR( eax, memshift );
		else				xSHL( eax, memshift );

		xAND( ebx, rtmask );
		xOR( eax, ebx );
		info.MoveToRt( eax );
	}

	IMPL_GetInterfaceTee( bool )
};
	
//////////////////////////////////////////////////////////////////////////////////////////
//
namespace recStore_ConstNone
{
	static void RegMapInfo( IntermediateRepresentation& info )
	{
		RegMapInfo_Strict& rs( info.RegOpts.UseStrictMode() );
		
		rs.EntryMap.Rs = ecx;

		//if( info.Inst._Rs_ != info.Inst._Rt_ )
			rs.EntryMap.Rt = edx;
	}

	template< typename T >
	static void Emit( const IntermediateRepresentation& info )
	{
		if( info.Inst._Rs_ != info.Inst._Rt_ )
		{
			DynarecAssume( (RegRs == ecx) && (RegRt == edx), info.Inst,
				"Recompiler failed to prepare strict ecx/edx mappings." );
		}

		xADD( ecx, info.ixImm );

		xMOV( eax, ecx );
		xAND( eax, AddressMask );
		xSHR( eax, PageBitShift );

		xMOV( ebx, 0xcdcdcdcd );
		uptr* writeback = ((uptr*)xGetPtr()) - 1;

		xMOV( eax, ptr[(eax*4) + tbl_Translation.Contents] );
		DynGen_IndirectDispatchJump<T>( 1, false );

		// Direct Access on Load Operation:

		xAND( ecx, PageMask );
		DynGen_DirectWrite( info, ptrT[ecx+eax] );

		*writeback = (uptr)xGetPtr();		// return target for indirect's call/ret
	}

	IMPL_GetInterfaceTee( typename )
};


//////////////////////////////////////////////////////////////////////////////////////////
//
namespace recStore_ConstRs
{
	static void RegMapInfo( IntermediateRepresentation& info )
	{
		RegMapInfo_Strict& rs( info.RegOpts.UseStrictMode() );
		rs.EntryMap.Rt = edx;

		const ConstLoadOpInfo constinfo( info );
		if( constinfo.xlated > HandlerId_Maximum )
		{
			rs.ClobbersNothing();
		}
		else
			rs.ExitMap.ebx = StrictEM_Untouched;
	}

	template< typename T >
	static void Emit( const IntermediateRepresentation& info )
	{
		const ConstLoadOpInfo constinfo( info );

		if( constinfo.xlated > HandlerId_Maximum )
			DynGen_DirectWrite( info, ptrT[constinfo.xlated] );
		else
		{
			// Indirect writes don't need a check against the CP0 write bit.
			xMOV( ecx, constinfo.addr );
			CallIndirectDispatcher<T>( 1 );
		}
	}

	IMPL_GetInterfaceTee( typename )
};

//////////////////////////////////////////////////////////////////////////////////////////
//
namespace recStoreWord_LorR_ConstNone
{
	static void RegMapInfo( IntermediateRepresentation& info )
	{
		RegMapInfo_Strict& rs( info.RegOpts.UseStrictMode() );

		// Yay most inefficient function ever -- no regs preserved and we need to
		// flush Rt as well on entry (so map only Rs)

		rs.EntryMap.Rs = ecx;
	}

	template< bool IsStoreRight >
	static void Emit( const IntermediateRepresentation& info )
	{
		jASSUME( RegRs == ecx );
		xADD( ecx, info.ixImm );

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

		DynGen_IndirectDispatchJump<u32>( 0 );

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
		DynGen_IndirectDispatchJump<u32>( 1 );

		// Direct Access on Load Operation:
		xAND( ecx, PageMask );
		DynGen_DirectWrite( info, ptr32[ecx+eax] );

		*writeback = (uptr)xGetPtr();		// return target for indirect's call/ret
	}

	IMPL_GetInterfaceTee( bool )
};

//////////////////////////////////////////////////////////////////////////////////////////
//
namespace recStoreWord_LorR_ConstRs
{
	static void RegMapInfo( IntermediateRepresentation& info )
	{
		RegMapInfo_Strict& rs( info.RegOpts.UseStrictMode() );

		rs.EntryMap.Rt = ebx;
		
		// Direct memory preserves ecx/edx
		const ConstLoadOpInfo constinfo( info );
		if( constinfo.xlated > HandlerId_Maximum )
		{
			rs.ExitMap.ecx = StrictEM_Untouched;
			rs.ExitMap.ebx = StrictEM_Untouched;
		}
	}

	template< bool IsStoreRight >
	static void Emit( const IntermediateRepresentation& info )
	{
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

	IMPL_GetInterfaceTee( bool )
};

//////////////////////////////////////////////////////////////////////////////////////////
//

typedef InstructionRecMess InstAPI;

#define DefineAPI( mess, name, opsize ) \
	void InstAPI::name() \
	{ \
		API.ConstNone	= rec##mess##_ConstNone::GetInterface<opsize>; \
		API.ConstRt		= rec##mess##_ConstNone::GetInterface<opsize>; \
		API.ConstRs		= rec##mess##_ConstRs::GetInterface<opsize>; \
		API.ConstRsRt	= rec##mess##_ConstRs::GetInterface<opsize>; \
	}

DefineAPI( Load, LB, u8 )
DefineAPI( Load, LH, u16 )
DefineAPI( Load, LBU, u8 )
DefineAPI( Load, LHU, u16 )
DefineAPI( Load, LW, u32 )

DefineAPI( Store, SB, u8 )
DefineAPI( Store, SH, u16 )
DefineAPI( Store, SW, u32 )

DefineAPI( LoadWord_LorR, LWL, false )
DefineAPI( LoadWord_LorR, LWR, true )
DefineAPI( StoreWord_LorR, SWL, false )
DefineAPI( StoreWord_LorR, SWR, true )

}

using namespace R3000A;

//////////////////////////////////////////////////////////////////////////////////////////
// Initializes the 'templated' handlers for indirect memory operations.
//
void iopInitRecMem()
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
			DynGen_IndirectDispatcherFunction( mode, bits, false );
			if( bits != 2 )
				DynGen_IndirectDispatcherFunction( mode, bits, true );
		}
	}

	HostSys::MemProtect( m_IndirectDispatchers, 0x1000, Protect_ReadOnly, true );
}
