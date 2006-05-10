/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2002-2003  Pcsx2 Team
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __STATS_H__
#define __STATS_H__

#include <time.h>

typedef struct {
	time_t vsyncTime;
	u32 vsyncCount;
	u64 eeCycles;
	u32 eeSCycle;
	u64 iopCycles;
	u32 iopSCycle;

	u64 ticko;
	u64 framecount;
	u64 vu1count;
	u64 vif1count;
} Stats;
Stats stats;

void statsOpen();
void statsClose();
void statsVSync();

#endif /* __STATS_H__ */
