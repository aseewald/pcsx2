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
typedef InstructionRecMess InstAPI;


//////////////////////////////////////////////////////////////////////////////////////////
// Branch Recompilers
//
// At it's core, branching is simply the process of (conditionally) setting a new PC.  The
// recompiler has a few added responsibilities however, due to the fact that we can't
// efficiently handle updates of some things on a per-instruction basis.
//
// Branching Duties:
//  * Flush cached and const registers out to the GPRs struct
//  * Test for events
//  * Execute block dispatcher
//
// Delay slots:
//  The branch delay slot is handled by the IL, so we don't have to worry with it here.

IMPL_RecPlacebo( JAL );
IMPL_RecPlacebo( J );
IMPL_RecPlacebo( JALR );
IMPL_RecPlacebo( JR );

IMPL_RecPlacebo( BNE );
IMPL_RecPlacebo( BEQ );

IMPL_RecPlacebo( BLTZ );
IMPL_RecPlacebo( BLEZ );
IMPL_RecPlacebo( BGEZ );
IMPL_RecPlacebo( BGTZ );
IMPL_RecPlacebo( BLTZAL );
IMPL_RecPlacebo( BGEZAL );

}

/*void R3000A::recJ( const IntermediateInstruction& info )
{

}

void R3000A::recJR( const IntermediateInstruction& info )
{
}*/
