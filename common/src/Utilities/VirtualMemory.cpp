/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"

#include "PageFaultSource.h"
#include "EventSource.inl"
#include "MemsetFast.inl"

#ifndef __WXMSW__
#include <wx/thread.h>
#endif

template class EventSource< IEventListener_PageFault >;

SrcType_PageFault* Source_PageFault = NULL;

EventListener_PageFault::EventListener_PageFault()
{
	pxAssume(Source_PageFault);
	Source_PageFault->Add( *this );
}

EventListener_PageFault::~EventListener_PageFault() throw()
{
	if (Source_PageFault)
		Source_PageFault->Remove( *this );
}

void SrcType_PageFault::Dispatch( const PageFaultInfo& params )
{
	m_handled = false;
	_parent::Dispatch( params );
}

void SrcType_PageFault::_DispatchRaw( ListenerIterator iter, const ListenerIterator& iend, const PageFaultInfo& evt )
{
	do {
		(*iter)->DispatchEvent( evt, m_handled );
	} while( (++iter != iend) && !m_handled );
}

// --------------------------------------------------------------------------------------
//  BaseVirtualMemoryReserve  (implementations)
// --------------------------------------------------------------------------------------

BaseVirtualMemoryReserve::BaseVirtualMemoryReserve( const wxString& name )
	: Name( name )
{
	m_commited		= 0;
	m_reserved		= 0;
	m_baseptr		= NULL;
	m_blocksize	= __pagesize;
	m_prot_mode		= PageAccess_None();
}

// Parameters:
//   upper_bounds - criteria that must be met for the allocation to be valid.
//     If the OS refuses to allocate the memory below the specified address, the
//     object will fail to initialize and an exception will be thrown.
void* BaseVirtualMemoryReserve::Reserve( uint size, uptr base, uptr upper_bounds )
{
	if (!pxAssertDev( m_baseptr == NULL, "(VirtualMemoryReserve) Invalid object state; object has already been reserved." ))
		return m_baseptr;

	m_reserved = (size + __pagesize-4) / __pagesize;
	uptr reserved_bytes = m_reserved * __pagesize;

	m_baseptr = (void*)HostSys::MmapReserve(base, reserved_bytes);

	if (!m_baseptr && (upper_bounds != 0 && (((uptr)m_baseptr + reserved_bytes) > upper_bounds)))
	{
		if (base)
		{
			DevCon.Warning( L"%s: address 0x%08x is unavailable; trying OS-selected address instead.", Name.c_str(), base );

			// Let's try again at an OS-picked memory area, and then hope it meets needed
			// boundschecking criteria below.
			SafeSysMunmap( m_baseptr, reserved_bytes );
			m_baseptr = HostSys::MmapReserve( 0, reserved_bytes );
		}

		if ((upper_bounds != 0) && (((uptr)m_baseptr + reserved_bytes) > upper_bounds))
		{
			SafeSysMunmap( m_baseptr, reserved_bytes );
			// returns null, caller should throw an exception or handle appropriately.
		}
	}

	if (!m_baseptr) return NULL;
	
	DevCon.WriteLn( Color_Blue, L"%-32s @ 0x%08X -> 0x%08X [%umb]", Name.c_str(),
		m_baseptr, (uptr)m_baseptr+reserved_bytes, reserved_bytes / _1mb);

	return m_baseptr;
}

// Clears all committed blocks, restoring the allocation to a reserve only.
void BaseVirtualMemoryReserve::Reset()
{
	if (!m_commited) return;
	
	HostSys::MemProtect(m_baseptr, m_commited*__pagesize, PageAccess_None());
	HostSys::MmapResetPtr(m_baseptr, m_commited*__pagesize);
	m_commited = 0;
}

void BaseVirtualMemoryReserve::Free()
{
	HostSys::Munmap((uptr)m_baseptr, m_reserved*__pagesize);
}

// If growing the array, or if shrinking the array to some point that's still *greater* than the
// committed memory range, then attempt a passive "on-the-fly" resize that maps/unmaps some portion
// of the reserve.
//
// If the above conditions are not met, or if the map/unmap fails, this method returns false.
// The caller will be responsible for manually resetting the reserve.
//
// Parameters:
//  newsize - new size of the reserved buffer, in bytes.
bool BaseVirtualMemoryReserve::TryResize( uint newsize )
{
	uint newPages = (newsize + __pagesize - 1) / __pagesize;

	if (newPages > m_reserved)
	{
		uint toReservePages = newPages - m_reserved;
		uint toReserveBytes = toReservePages * __pagesize;

		DevCon.WriteLn( L"%-32s is being expanded by %u pages.", Name.c_str(), toReservePages);

		m_baseptr = (void*)HostSys::MmapReserve((uptr)GetPtrEnd(), toReserveBytes);

		if (!m_baseptr)
		{
			Console.Warning("%-32s could not be passively resized due to virtual memory conflict!");
			Console.Indent().Warning("(attempted to map memory @ 0x%08X -> 0x%08X", m_baseptr, (uptr)m_baseptr+toReserveBytes);
		}

		DevCon.WriteLn( Color_Blue, L"%-32s @ 0x%08X -> 0x%08X [%umb]", Name.c_str(),
			m_baseptr, (uptr)m_baseptr+toReserveBytes, toReserveBytes / _1mb);
	}
	else if (newPages < m_reserved)
	{
		if (m_commited > newsize) return false;
	
		uint toRemovePages = m_reserved - newPages;
		uint toRemoveBytes = toRemovePages * __pagesize;

		DevCon.WriteLn( L"%-32s is being shrunk by %u pages.", Name.c_str(), toRemovePages);

		HostSys::MmapResetPtr(GetPtrEnd(), toRemoveBytes);

		DevCon.WriteLn( Color_Blue, L"%-32s @ 0x%08X -> 0x%08X [%umb]", Name.c_str(),
			m_baseptr, (uptr)m_baseptr+toRemoveBytes, toRemoveBytes / _1mb);
	}
	
	return true;
}


void BaseVirtualMemoryReserve::CommitBlocks( uptr page, uint blocks )
{
	const uptr blocksbytes = blocks * m_blocksize * __pagesize;
	void* blockptr = (u8*)m_baseptr + (page * __pagesize);

	// Depending on the operating system, this call could fail if the system is low on either
	// physical ram or virtual memory.
	if (!HostSys::MmapCommitPtr(blockptr, blocksbytes, m_prot_mode))
	{
		throw Exception::OutOfMemory(Name)
			.SetDiagMsg(pxsFmt("An additional %u blocks @ 0x%08x were requested, but could not be committed!", blocks, blockptr));
	}

	u8* init = (u8*)blockptr;
	u8* endpos = init + blocksbytes;
	for( ; init<endpos; init += m_blocksize*__pagesize )
		OnCommittedBlock(init);

	m_commited += m_blocksize * blocks;
}

void BaseVirtualMemoryReserve::OnPageFaultEvent(const PageFaultInfo& info, bool& handled)
{
	sptr offset = (info.addr - (uptr)m_baseptr) / __pagesize;
	if ((offset < 0) || ((uptr)offset >= m_reserved)) return;

	// Linux Note!  the SIGNAL handler is very limited in what it can do, and not only can't 
	// we let the C++ exception try to unwind the stack, we may not be able to log it either.
	// (but we might as well try -- kernel/posix rules says not to do it, but Linux kernel
	//  implementations seem to support it).
	// Note also that logging the exception and/or issuing an assertion dialog are always
	// possible if the thread handling the signal is not the main thread.

	// In windows we can let exceptions bubble out of the page fault handler.  SEH will more
	// or less handle them in a semi-expected way, and might even avoid a GPF long enough
	// for the system to log the error or something.

	#ifndef __WXMSW__
	try	{
	#endif
		DoCommitAndProtect( offset );
		handled = true;

	#ifndef __WXMSW__
	}
	catch (Exception::BaseException& ex)
	{
		handled = false;
		if (!wxThread::IsMain())
		{
			pxFailRel( ex.FormatDiagnosticMessage() );
		}
		else
		{
			wxTrap();
		}
	}
	#endif
}


// --------------------------------------------------------------------------------------
//  SpatialArrayReserve  (implementations)
// --------------------------------------------------------------------------------------

uint SpatialArrayReserve::_calcBlockBitArrayLength() const
{
	return (m_numblocks + 127) / 128;
}

void* SpatialArrayReserve::Reserve( uint size, uptr base, uptr upper_bounds )
{
	return __parent::Reserve( size, base, upper_bounds );
}

// Resets/clears the spatial array, reducing the memory commit pool overhead to zero (0).
void SpatialArrayReserve::Reset()
{
	__parent::Reset();
	memzero_sse_a(m_blockbits.GetPtr(), _calcBlockBitArrayLength());
}

// Important!  The number of blocks of the array will be altered when using this method.
// 
bool SpatialArrayReserve::TryResize( uint newsize )
{
	uint newpages = (newsize + __pagesize - 1) / __pagesize;

	// find the last allocated block -- we cannot be allowed to resize any smaller than that:

	uint i;
	for (i=m_numblocks-1; i; --i)
	{
		uint bit = i & 7;
		if (m_blockbits[i / 8] & bit) break;
	}

	uint pages_in_use = i * m_blocksize;
	if (newpages < pages_in_use) return false;

	if (!__parent::TryResize( newsize )) return false;
	
	// On success, we must re-calibrate the internal blockbits array.
	
	m_blockbits.Resize( (m_numblocks + 7) / 8 );
	
	return true;
}

// This method allows the programmer to specify the block size of the array as a function
// of its reserved size.  This function *must* be called *after* the reserve has been made,
// and *before* the array contents have been accessed.
//
// Calls to this function prior to initializing the reserve or after the reserve has been
// accessed (resulting in committed blocks) will be ignored -- and will generate an assertion
// in debug builds.
SpatialArrayReserve& SpatialArrayReserve::SetBlockCount( uint blocks )
{
	pxAssumeDev( !m_commited, "Invalid object state: SetBlockCount must be called prior to reserved memory accesses." );

	// Calculate such that the last block extends past the end of the array, if necessary.

	m_numblocks = blocks;
	m_blocksize = (m_reserved + m_numblocks-1) / m_numblocks;
	
	return *this;
}

// Sets the block size via pages (pages are defined by the __pagesize global, which is
// typically 4096).
//
// This method must be called prior to accessing or modifying the array contents.  Calls to
// a modified buffer will be ignored (and generate an assertion in dev/debug modes).
SpatialArrayReserve& SpatialArrayReserve::SetBlockSizeInPages( uint pages )
{
	if (pxAssertDev(m_commited, "Invalid object state: Block size can only be changed prior to accessing or modifying the reserved buffer contents."))
	{
		m_blocksize = pages;
		m_numblocks = (m_reserved + m_blocksize - 1) / m_blocksize;
		m_blockbits.Alloc( _calcBlockBitArrayLength() );
	}
	return *this;
}

// SetBlockSize assigns the block size of the spatial array, in bytes.  The actual size of
// each block will be rounded up to the nearest page size.  The resulting size is returned.
//
// This method must be called prior to accessing or modifying the array contents.  Calls to
// a modified buffer will be ignored (and generate an assertion in dev/debug modes).
uint SpatialArrayReserve::SetBlockSize( uint bytes )
{
	SetBlockSizeInPages((bytes + __pagesize - 1) / __pagesize);
	return m_blocksize * __pagesize;
}

void SpatialArrayReserve::OnCommittedBlock( void* block )
{
	// Determine the block position in the blockbits array, flag it, and be done!
	
	uptr relative = (uptr)m_baseptr - (uptr)block;
	pxAssume( (relative % (m_blocksize * __pagesize)) == 0);
	relative /= m_blocksize * __pagesize;

	m_blockbits[relative/32] |= 1 << (relative & 31);
	m_commited += m_blocksize;
}


// --------------------------------------------------------------------------------------
//  PageProtectionMode  (implementations)
// --------------------------------------------------------------------------------------
wxString PageProtectionMode::ToString() const
{
	wxString modeStr;

	if (m_read)		modeStr += L"Read";
	if (m_write)	modeStr += L"Write";
	if (m_exec)		modeStr += L"Exec";

	if (modeStr.IsEmpty()) return L"NoAccess";
	if (modeStr.Length() <= 5) modeStr += L"Only";

	return modeStr;
}