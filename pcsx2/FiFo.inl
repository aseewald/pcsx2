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

#pragma once

template< uint size >
void FifoRingBuffer<size>::SendToPeripheral(Fnptr_ToPeripheral toFunc)
{
	uint transferred = toFunc( buffer, size, readpos, qwc );
	qwc -= transferred;

	readpos += transferred;
	readpos &= size-1;
}

template< uint size >
void FifoRingBuffer<size>::HwWrite(Fnptr_ToPeripheral toFunc, const u128* src, SysTraceLog_EE_Peripherals& logger)
{
	if (logger.IsActive())
		logger.Write("WriteFIFO/%s <- %ls (FQC=%u)", logger.GetShortName(), src->ToString().c_str(), qwc);

	if (qwc >= size) return;
	WriteSingle(src);

	if (qwc >= size)
	{
		ProcessFifoEvent();
		CPU_ClearEvent(FIFO_EVENT);
	}
	else
	{
		CPU_ScheduleEvent(FIFO_EVENT, 128);
	}
}

template< uint size >
void FifoRingBuffer<size>::ReadSingle(u128* dest)
{
	pxAssume(qwc);
	CopyQWC(dest, &buffer[readpos]);
	readpos = (readpos+1) & (size-1);
	--qwc;
}

template< uint size >
uint FifoRingBuffer<size>::Read(u128* dest, uint qwc)
{
	pxAssume(qwc);

	uint minLen = std::min(qwc, size);
	MemCopy_WrappedSrc(buffer, readpos, size, dest, minLen);
	readpos += minLen;
	readpos &= size-1;
	qwc -= minLen;
	return minLen;
}


template< uint size >
uint FifoRingBuffer<size>::Write(const u128* src, uint qwc)
{
	pxAssume(qwc);

	uint minLen = std::min(qwc, size);
	MemCopy_WrappedDest(src, buffer, writepos, size, minLen);
	writepos += minLen;
	writepos &= size-1;
	qwc += minLen;
	return minLen;
}


template< uint size >
void FifoRingBuffer<size>::WriteSingle(const u128* src)
{
	pxAssume(qwc < size);
	CopyQWC(&buffer[writepos], src);
	writepos = (writepos+1) & (size-1);
	++qwc;
}

