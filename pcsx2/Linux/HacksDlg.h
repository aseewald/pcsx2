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

 char vu_stealing_labels[5][256] = 
{
	"0: No speedup.",
	"1: Slight speedup, should work with most games.",
	"2: Moderate speedup, should work with most games with minor problems.",
	"3: Large speedup, may break many games and make others skip frames.",
	"4: Very large speedup, will break games in interesting ways."
};

char ee_cycle_labels[3][256] = 
{
	"Default Cycle Rate: Most compatible option - recommended for everyone with high-end machines.",
	"x1.5 Cycle Rate: Moderate speedup, and works well with most games.",
	"x2 Cycle Rate: Big speedup! Works well with many games."
};