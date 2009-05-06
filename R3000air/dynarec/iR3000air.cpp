
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

		// ------------------------------------------------------------------------
		// Generates a JB instruction that targets the appropriate templated instance of
		// the vtlb Indirect Dispatcher.
		//
		static void DynGen_IndirectDispatch( int mode, int bits )
		{
			int szidx;
			switch( bits )
			{
				case 8:		szidx=0;	break;
				case 16:	szidx=1;	break;
				case 32:	szidx=2;	break;
				jNO_DEFAULT;
			}
			xJB( GetIndirectDispatcherPtr( mode, szidx ) );
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
}

//////////////////////////////////////////////////////////////////////////////////////////
//
static void DynGen_DirectRead32( const IntermediateInstruction& info, const ModSibBase& src )
{
	if( info.DestRt.IsReg() )
		xMOV( info.DestRt.GetReg(), src );
	else
	{
		// edx is open when we're being called. :)
		xMOV( edx, src );
		info.MoveToRt( edx );
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
//
static void DynGen_DirectWrite32( const IntermediateInstruction& info, const ModSibStrict<u32>& dest )
{
	xTEST( ptr32[&iopRegs.CP0.n.Status], 0x10000 );
	xForwardJNZ8 skipWrite;

	// The data being written is either a register or an immediate, held in Rt.

	info.MoveRtTo( dest, edx );
	skipWrite.SetTarget();
}

//////////////////////////////////////////////////////////////////////////////////////////
// Loads address in Rs+Imm into Rt.
//
void recLW( const IntermediateInstruction& info )
{
	using namespace IopMemory;
	using namespace IopMemory::Internal;

	if( info.IsConst.Rs )
	{
		// ---------------------------------------------------
		//    Constant Propagation on the Src Address (Rs)
		// ---------------------------------------------------
		
		const u32 srcaddr = info.IsConst.Rs + info.GetImm();
		const u32 masked = srcaddr & AddressMask;
		
		const sptr xlated = tbl_Translation.Contents[masked / PageSize];
		if( xlated > HandlerId_Maximum )
			DynGen_DirectRead32( info, ptr[xlated] );
		else
		{
			xMOV( ecx, srcaddr );
			xCALL( GetIndirectDispatcherPtr( 0,32 ) );
			info.MoveToRt( eax );
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
		DynGen_IndirectDispatch( 0, 32 );

		// Direct Access on Load Operation:

		xAND( ecx, PageMask );
		DynGen_DirectRead32( info, ptr[ecx+eax] );

		*writeback = (uptr)xGetPtr();		// return target for indirect's call/ret
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// Loads address in Rs+Imm into Rt.
//
void recSW( const IntermediateInstruction& info )
{
	using namespace IopMemory;
	using namespace IopMemory::Internal;

	if( info.IsConst.Rs )
	{
		// ---------------------------------------------------
		//    Constant Propagation on the Dest Address (Rs)
		// ---------------------------------------------------
		
		const u32 dstaddr = info.IsConst.Rs + info.GetImm();
		const u32 masked = dstaddr & AddressMask;
		
		const sptr xlated = tbl_Translation.Contents[masked / PageSize];
		if( xlated > HandlerId_Maximum )
			DynGen_DirectWrite32( info, ptr32[xlated] );
		else
		{
			xMOV( ecx, dstaddr );
			info.MoveRtTo( edx );
			xCALL( GetIndirectDispatcherPtr( 1, 32 ) );
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
		DynGen_IndirectDispatch( 1, 32 );

		// Direct Access on Load Operation:

		xAND( ecx, PageMask );
		DynGen_DirectWrite32( info, ptr32[ecx+eax] );

		*writeback = (uptr)xGetPtr();		// return target for indirect's call/ret
	}
}
