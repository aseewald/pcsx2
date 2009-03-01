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
 
#include "Win32.h"
#include "cdvd.h"

static LARGE_INTEGER lfreq;

void InitCPUTicks()
{
	QueryPerformanceFrequency( &lfreq );
}

u64 GetTickFrequency()
{
	return lfreq.QuadPart;
}

u64 GetCPUTicks()
{
	LARGE_INTEGER count;
	QueryPerformanceCounter( &count );
	return count.QuadPart;
}

void cdvdSetSystemTime( cdvdStruct& cdvd )
{
	SYSTEMTIME st;
	GetSystemTime(&st);

	cdvd.RTC.second = (u8)(st.wSecond);
	cdvd.RTC.minute = (u8)(st.wMinute);
	cdvd.RTC.hour = (u8)(st.wHour+1)%24;
	cdvd.RTC.day = (u8)(st.wDay);
	cdvd.RTC.month = (u8)(st.wMonth);
	cdvd.RTC.year = (u8)(st.wYear - 2000);
}
