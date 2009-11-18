/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2009  PCSX2 Dev Team
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

// Register counts for x86/32 mode:
static const int iREGCNT_XMM = 8;
static const int iREGCNT_GPR = 8;
static const int iREGCNT_MMX = 8;

enum XMMSSEType
{
	XMMT_INT = 0, // integer (sse2 only)
	XMMT_FPS = 1, // floating point
	//XMMT_FPD = 3, // double
};

extern __threadlocal u8  *x86Ptr;
extern __threadlocal XMMSSEType g_xmmtypes[iREGCNT_XMM];

namespace x86Emitter
{

extern void xWrite8( u8 val );
extern void xWrite16( u16 val );
extern void xWrite32( u32 val );
extern void xWrite64( u64 val );

extern const char *const x86_regnames_gpr8[8];
extern const char *const x86_regnames_gpr16[8];
extern const char *const x86_regnames_gpr32[8];

extern const char *const x86_regnames_sse[8];
extern const char *const x86_regnames_mmx[8];

extern const char* xGetRegName( int regid, int operandSize );

//------------------------------------------------------------------
// templated version of is_s8 is required, so that u16's get correct sign extension treatment.
template< typename T >
static __forceinline bool is_s8( T imm ) { return (s8)imm == (s32)imm; }

template< typename T > void xWrite( T val );

// --------------------------------------------------------------------------------------
//  ALWAYS_USE_MOVAPS [define] / AlwaysUseMovaps [const]
// --------------------------------------------------------------------------------------
// This tells the recompiler's emitter to always use movaps instead of movdqa.  Both instructions
// do the exact same thing, but movaps is 1 byte shorter, and thus results in a cleaner L1 cache
// and some marginal speed gains as a result.  (it's possible someday in the future the per-
// formance of the two instructions could change, so this constant is provided to restore MOVDQA
// use easily at a later time, if needed).
//
#define ALWAYS_USE_MOVAPS

#ifdef ALWAYS_USE_MOVAPS
	static const bool AlwaysUseMovaps = true;
#else
	static const bool AlwaysUseMovaps = false;
#endif

// --------------------------------------------------------------------------------------
//  __emitline - preprocessors definition
// --------------------------------------------------------------------------------------
// This is configured to inline emitter functions appropriately for release builds, and
// disables some of the more aggressive inlines for dev builds (which can be helpful when
// debugging).  Additionally,  I've set up the inlining to be as practical and intelligent
// as possible with regard to constant propagation.  Namely this involves forcing inlining
// for (void*) forms of ModRM, which (thanks to constprop) reduce to virtually no code, and
// force-disabling inlining on complicated SibSB forms [since MSVC would sometimes inline
// despite being a generally bad idea].
//
// In the case of (Reg, Imm) forms, the inlining is up to the discreation of the compiler.
//
// Note: I *intentionally* use __forceinline directly for most single-line class members,
// when needed.  There's no point in using __emitline in these cases since the debugger
// can't trace into single-line functions anyway.
//
#ifdef PCSX2_DEVBUILD
#	define __emitinline
#else
#	define __emitinline __forceinline
#endif

	// ModRM 'mod' field enumeration.   Provided mostly for reference:
	enum ModRm_ModField
	{
		Mod_NoDisp = 0,		// effective address operation with no displacement, in the form of [reg] (or uses special Disp32-only encoding in the case of [ebp] form)
		Mod_Disp8,			// effective address operation with 8 bit displacement, in the form of [reg+disp8]
		Mod_Disp32,			// effective address operation with 32 bit displacement, in the form of [reg+disp32],
		Mod_Direct,			// direct reg/reg operation
	};

	// ----------------------------------------------------------------------------
	// JccComparisonType - enumerated possibilities for inspired code branching!
	//
	enum JccComparisonType
	{
		Jcc_Unknown			= -2,
		Jcc_Unconditional	= -1,
		Jcc_Overflow		= 0x0,
		Jcc_NotOverflow		= 0x1,
		Jcc_Below			= 0x2,
		Jcc_Carry			= 0x2,
		Jcc_AboveOrEqual	= 0x3,
		Jcc_NotCarry		= 0x3,
		Jcc_Zero			= 0x4,
		Jcc_Equal			= 0x4,
		Jcc_NotZero			= 0x5,
		Jcc_NotEqual		= 0x5,
		Jcc_BelowOrEqual	= 0x6,
		Jcc_Above			= 0x7,
		Jcc_Signed			= 0x8,
		Jcc_Unsigned		= 0x9,
		Jcc_ParityEven		= 0xa,
		Jcc_ParityOdd		= 0xb,
		Jcc_Less			= 0xc,
		Jcc_GreaterOrEqual	= 0xd,
		Jcc_LessOrEqual		= 0xe,
		Jcc_Greater			= 0xf,
	};

	// Not supported yet:
	//E3 cb 	JECXZ rel8 	Jump short if ECX register is 0.

	// ----------------------------------------------------------------------------
	// SSE2_ComparisonType - enumerated possibilities for SIMD data comparison!
	//
	enum SSE2_ComparisonType
	{
		SSE2_Equal = 0,
		SSE2_Less,
		SSE2_LessOrEqual,
		SSE2_Unordered,
		SSE2_NotEqual,
		SSE2_NotLess,
		SSE2_NotLessOrEqual,
		SSE2_Ordered
	};

	static const int ModRm_UseSib = 4;		// same index value as ESP (used in RM field)
	static const int ModRm_UseDisp32 = 5;	// same index value as EBP (used in Mod field)

	class xAddressInfo;
	class ModSibBase;

	extern void xSetPtr( void* ptr );
	extern u8* xGetPtr();
	extern void xAlignPtr( uint bytes );
	extern void xAdvancePtr( uint bytes );

	extern JccComparisonType xInvertCond( JccComparisonType src );

	// --------------------------------------------------------------------------------------
	//  OperandSizedObject
	// --------------------------------------------------------------------------------------
	class OperandSizedObject
	{
	public:
		virtual uint GetOperandSize() const=0;

		bool Is8BitOp() const		{ return GetOperandSize() == 1; }
		void prefix16() const		{ if( GetOperandSize() == 2 ) xWrite8( 0x66 ); }

		void xWriteImm( int imm ) const
		{
			switch( GetOperandSize() )
			{
				case 1: xWrite8( imm ); break;
				case 2: xWrite16( imm ); break;
				case 4: xWrite32( imm ); break;
				case 8: xWrite64( imm ); break;

				jNO_DEFAULT
			}
		}
	};

	// Represents an unused or "empty" register assignment.  If encountered by the emitter, this
	// will be ignored (in some cases it is disallowed and generates an assertion)
	static const int xRegId_Empty	= -1;

	// Represents an invalid or uninitialized register.  If this is encountered by the emitter it
	// will generate an assertion.
	static const int xRegId_Invalid	= -2;

	// --------------------------------------------------------------------------------------
	//  xRegisterBase  -  type-unsafe x86 register representation.
	// --------------------------------------------------------------------------------------
	// Unless doing some fundamental stuff, use the friendly xRegister32/16/8 and xRegisterSSE/MMX
	// instead, which are built using this class and provide strict register type safety when
	// passed into emitter instructions.
	//
	class xRegisterBase : public OperandSizedObject
	{
	public:
		int Id;

		xRegisterBase()
		{
			Id = xRegId_Invalid;
		}

		explicit xRegisterBase( int regId )
		{
			Id = regId;
			pxAssert( (Id >= xRegId_Empty) && (Id < 8) );
		}

		bool IsEmpty() const		{ return Id < 0 ; }
		bool IsInvalid() const		{ return Id == xRegId_Invalid; }

		// Returns true if the register is a valid accumulator: Eax, Ax, Al, XMM0.
		bool IsAccumulator() const	{ return Id == 0; }

		// returns true if the register is a valid MMX or XMM register.
		bool IsSIMD() const { return GetOperandSize() == 8 || GetOperandSize() == 16; }

		bool operator==( const xRegisterBase& src ) const	{ return (Id == src.Id); }
		bool operator!=( const xRegisterBase& src ) const	{ return (Id != src.Id); }

		// Diagnostics -- returns a string representation of this register.  Return string
		// is a valid non-null string for any Id, valid or invalid.  No assertions are generated.
		const char* GetName();
	};

	class xRegisterInt : public xRegisterBase
	{
		typedef xRegisterBase _parent;

	public:
		xRegisterInt() {}
		explicit xRegisterInt( const xRegisterBase& src ) : _parent( src ) {}
		explicit xRegisterInt( int regId ) : _parent( regId ) { }

		bool IsSIMD() const { return false; }

		bool operator==( const xRegisterInt& src ) const	{ return Id == src.Id && (GetOperandSize() == src.GetOperandSize()); }
		bool operator!=( const xRegisterInt& src ) const	{ return !operator==(src); }
	};

	// --------------------------------------------------------------------------------------
	//  xRegister8/16/32  -  Represents a basic 8/16/32 bit GPR on the x86
	// --------------------------------------------------------------------------------------
	class xRegister8 : public xRegisterInt
	{
		typedef xRegisterInt _parent;

	public:
		xRegister8(): _parent() {}
		explicit xRegister8( int regId ) : _parent( regId ) {}

		virtual uint GetOperandSize() const { return 1; }

		bool operator==( const xRegister8& src ) const	{ return Id == src.Id; }
		bool operator!=( const xRegister8& src ) const	{ return Id != src.Id; }
	};

	class xRegister16 : public xRegisterInt
	{
		typedef xRegisterInt _parent;

	public:
		xRegister16(): _parent() {}
		explicit xRegister16( int regId ) : _parent( regId ) {}

		virtual uint GetOperandSize() const { return 2; }

		bool operator==( const xRegister16& src ) const	{ return this->Id == src.Id; }
		bool operator!=( const xRegister16& src ) const	{ return this->Id != src.Id; }
	};

	class xRegister32 : public xRegisterInt
	{
		typedef xRegisterInt _parent;

	public:
		xRegister32(): _parent() {}
		explicit xRegister32( int regId ) : _parent( regId ) {}

		virtual uint GetOperandSize() const { return 4; }

		bool operator==( const xRegister32& src ) const	{ return this->Id == src.Id; }
		bool operator!=( const xRegister32& src ) const	{ return this->Id != src.Id; }
	};

	// --------------------------------------------------------------------------------------
	//  xRegisterMMX/SSE  -  Represents either a 64 bit or 128 bit SIMD register
	// --------------------------------------------------------------------------------------
	// This register type is provided to allow legal syntax for instructions that accept either
	// an XMM or MMX register as a parameter, but do not allow for a GPR.

	class xRegisterMMX : public xRegisterBase
	{
		typedef xRegisterBase _parent;

	public:
		xRegisterMMX(): _parent() {}
		//xRegisterMMX( const xRegisterBase& src ) : _parent( src ) {}
		explicit xRegisterMMX( int regId ) : _parent( regId ) {}

		virtual uint GetOperandSize() const { return 8; }

		bool operator==( const xRegisterMMX& src ) const	{ return this->Id == src.Id; }
		bool operator!=( const xRegisterMMX& src ) const	{ return this->Id != src.Id; }
	};

	class xRegisterSSE : public xRegisterBase
	{
		typedef xRegisterBase _parent;

	public:
		xRegisterSSE(): _parent() {}
		//xRegisterSSE( const xRegisterBase& src ) : _parent( src ) {}
		explicit xRegisterSSE( int regId ) : _parent( regId ) {}

		virtual uint GetOperandSize() const { return 16; }

		bool operator==( const xRegisterSSE& src ) const	{ return this->Id == src.Id; }
		bool operator!=( const xRegisterSSE& src ) const	{ return this->Id != src.Id; }
	};

	class xRegisterCL : public xRegister8
	{
	public:
		xRegisterCL(): xRegister8( 1 ) {}
	};

	// --------------------------------------------------------------------------------------
	//  xAddressReg
	// --------------------------------------------------------------------------------------
	// Use 32 bit registers as our index registers (for ModSib-style memory address calculations).
	// This type is implicitly exchangeable with xRegister32.
	//
	// Only xAddressReg provides operators for constructing xAddressInfo types.  These operators
	// could have been added to xRegister32 directly instead, however I think this design makes
	// more sense and allows the programmer a little more type protection if needed.
	//
	class xAddressReg : public xRegister32
	{
	public:
		xAddressReg(): xRegister32() {}
		xAddressReg( const xAddressReg& src ) : xRegister32( src.Id ) {}
		xAddressReg( const xRegister32& src ) : xRegister32( src ) {}
		explicit xAddressReg( int regId ) : xRegister32( regId ) {}

		// Returns true if the register is the stack pointer: ESP.
		bool IsStackPointer() const { return Id == 4; }

		inline xAddressInfo operator+( const xAddressReg& right ) const;
		inline xAddressInfo operator+( const xAddressInfo& right ) const;
		inline xAddressInfo operator+( s32 right ) const;
		inline xAddressInfo operator+( const void* right ) const;

		inline xAddressInfo operator-( s32 right ) const;
		inline xAddressInfo operator-( const void* right ) const;

		inline xAddressInfo operator*( u32 factor ) const;
		inline xAddressInfo operator<<( u32 shift ) const;

		/*xAddressReg& operator=( const xRegister32& src )
		{
			Id = src.Id;
			return *this;
		}*/
	};

	// --------------------------------------------------------------------------------------
	//  xRegisterEmpty
	// --------------------------------------------------------------------------------------
	struct xRegisterEmpty
	{
		operator xRegister8() const
		{
			return xRegister8( xRegId_Empty );
		}

		operator xRegister16() const
		{
			return xRegister16( xRegId_Empty );
		}

		operator xRegisterMMX() const
		{
			return xRegisterMMX( xRegId_Empty );
		}

		operator xRegisterSSE() const
		{
			return xRegisterSSE( xRegId_Empty );
		}

		operator xAddressReg() const
		{
			return xAddressReg( xRegId_Empty );
		}
	};

	class xRegister16or32
	{
	protected:
		const xRegisterInt&		m_convtype;

	public:
		xRegister16or32( const xRegister32& src ) : m_convtype( src ) {}
		xRegister16or32( const xRegister16& src ) : m_convtype( src ) {}

		operator const xRegisterBase&() const { return m_convtype; }

		const xRegisterInt* operator->() const
		{
			return &m_convtype;
		}
	};

	extern const xRegisterEmpty xEmptyReg;

	// --------------------------------------------------------------------------------------
	//  xAddressInfo
	// --------------------------------------------------------------------------------------
	class xAddressInfo
	{
	public:
		xAddressReg	Base;			// base register (no scale)
		xAddressReg	Index;			// index reg gets multiplied by the scale
		int			Factor;			// scale applied to the index register, in factor form (not a shift!)
		s32			Displacement;	// address displacement

	public:
		__forceinline xAddressInfo( const xAddressReg& base, const xAddressReg& index, int factor=1, s32 displacement=0 )
		{
			Base		= base;
			Index		= index;
			Factor		= factor;
			Displacement= displacement;

			pxAssertMsg( base.Id != xRegId_Invalid, "Uninitialized x86 register." );
			pxAssertMsg( index.Id != xRegId_Invalid, "Uninitialized x86 register." );
		}

		__forceinline explicit xAddressInfo( const xAddressReg& index, int displacement=0 )
		{
			Base		= xEmptyReg;
			Index		= index;
			Factor		= 0;
			Displacement= displacement;

			pxAssertMsg( index.Id != xRegId_Invalid, "Uninitialized x86 register." );
		}

		__forceinline explicit xAddressInfo( s32 displacement=0 )
		{
			Base		= xEmptyReg;
			Index		= xEmptyReg;
			Factor		= 0;
			Displacement= displacement;
		}

		static xAddressInfo FromIndexReg( const xAddressReg& index, int scale=0, s32 displacement=0 );

	public:
		bool IsByteSizeDisp() const { return is_s8( Displacement ); }

		__forceinline xAddressInfo& Add( s32 imm )
		{
			Displacement += imm;
			return *this;
		}

		xAddressInfo& Add( const xAddressReg& src );
		xAddressInfo& Add( const xAddressInfo& src );

		__forceinline xAddressInfo operator+( const xAddressReg& right ) const	{ return xAddressInfo( *this ).Add( right ); }
		__forceinline xAddressInfo operator+( const xAddressInfo& right ) const	{ return xAddressInfo( *this ).Add( right ); }
		__forceinline xAddressInfo operator+( s32 imm ) const					{ return xAddressInfo( *this ).Add( imm ); }
		__forceinline xAddressInfo operator-( s32 imm ) const					{ return xAddressInfo( *this ).Add( -imm ); }
		__forceinline xAddressInfo operator+( const void* addr ) const			{ return xAddressInfo( *this ).Add( (uptr)addr ); }
	};

	extern const xRegisterSSE
		xmm0, xmm1, xmm2, xmm3,
		xmm4, xmm5, xmm6, xmm7;

	extern const xRegisterMMX
		mm0, mm1, mm2, mm3,
		mm4, mm5, mm6, mm7;

	extern const xAddressReg
		eax, ebx, ecx, edx,
		esi, edi, ebp, esp;

	extern const xRegister16
		ax, bx, cx, dx,
		si, di, bp, sp;

	extern const xRegister8
		al, dl, bl,
		ah, ch, dh, bh;

	extern const xRegisterCL cl;		// I'm special!

	// --------------------------------------------------------------------------------------
	//  xImmReg< typename xRegType >
	// --------------------------------------------------------------------------------------
	// Used to represent an immediate value which can also be optimized to a register. Note
	// that the immediate value represented by this structure is *always* legal.  The register
	// assignment is an optional optimization which can be implemented in cases where an
	// immediate is used enough times to merit allocating it to a register.
	//
	// Note: not all instructions support this operand type (yet).  You can always implement it
	// manually by checking the status of IsReg() and generating the xOP conditionally.
	//
	template< typename xRegType >
	class xImmReg
	{
		xRegType	m_reg;
		int			m_imm;

	public:
		xImmReg() : m_reg()
		{
			m_imm = 0;
		}

		xImmReg( int imm, const xRegType& reg = xEmptyReg )
		{
			m_reg = reg;
			m_imm = imm;
		}

		const xRegType& GetReg() const { return m_reg; }
		int GetImm() const { return m_imm; }
		bool IsReg() const { return !m_reg.IsEmpty(); }
	};

	// --------------------------------------------------------------------------------------
	//  ModSib - Internal low-level representation of the ModRM/SIB information.
	// --------------------------------------------------------------------------------------
	// This class serves two purposes:  It houses 'reduced' ModRM/SIB info only, which means
	// that the Base, Index, Scale, and Displacement values are all in the correct arrange-
	// ments, and it serves as a type-safe layer between the xRegister's operators (which
	// generate xAddressInfo types) and the emitter's ModSib instruction forms.  Without this,
	// the xRegister would pass as a ModSib type implicitly, and that would cause ambiguity
	// on a number of instructions.
	//
	// End users should always use xAddressInfo instead.
	//
	class ModSibBase : public OperandSizedObject
	{
	public:
		xAddressReg		Base;			// base register (no scale)
		xAddressReg		Index;			// index reg gets multiplied by the scale
		uint			Scale;			// scale applied to the index register, in scale/shift form
		s32				Displacement;	// offset applied to the Base/Index registers.

	public:
		explicit ModSibBase( const xAddressInfo& src )
		{
			Base		= src.Base;
			Index		= src.Index;
			Scale		= src.Factor;
			Displacement= src.Displacement;

			Reduce();
		}

		ModSibBase( xAddressReg base, xAddressReg index, int scale=0, s32 displacement=0 )
		{
			Base		= base;
			Index		= index;
			Scale		= scale;
			Displacement= displacement;

			Reduce();
		}

		explicit ModSibBase( s32 disp )
		{
			Base		= xEmptyReg;
			Index		= xEmptyReg;
			Scale		= 0;
			Displacement= disp;

			// no reduction necessary :D
		}

		ModSibBase( const void* target )
		{
			Base		= xEmptyReg;
			Index		= xEmptyReg;
			Scale		= 0;
			Displacement= (s32)target;

			// no reduction necessary :D
		}

		virtual uint GetOperandSize() const { pxFail( "Invalid operation on ModSibBase" ); return 0; }
		bool IsByteSizeDisp() const { return is_s8( Displacement ); }

		ModSibBase& Add( s32 imm )
		{
			Displacement += imm;
			return *this;
		}

		__forceinline ModSibBase operator+( const s32 imm ) const { return ModSibBase( *this ).Add( imm ); }
		__forceinline ModSibBase operator-( const s32 imm ) const { return ModSibBase( *this ).Add( -imm ); }

	protected:
		void Reduce();
	};

	// --------------------------------------------------------------------------------------
	//  ModSib32rrLass  -  base class 32, 16, and 8 bit operand types
	// --------------------------------------------------------------------------------------
	class ModSib32orLess : public ModSibBase
	{
		typedef ModSibBase _parent;

	protected:
		explicit ModSib32orLess( const xAddressInfo& src ) : _parent( src ) {}
		explicit ModSib32orLess( s32 disp ) : _parent( disp ) {}
		ModSib32orLess( const void* target ) : _parent( target ) {}
		ModSib32orLess( xAddressReg base, xAddressReg index, int scale=0, s32 displacement=0 ) :
			_parent( base, index, scale, displacement ) {}
	};

	// --------------------------------------------------------------------------------------
	//  ModSib8/16/32/64/128
	// --------------------------------------------------------------------------------------
	// Strictly-typed version of ModSibBase, which is used to apply operand size information
	// to operations that do not involve an implicit operand size via register (such as
	// imm,mem or mem,imm)
	//
#define DECLARE_CLASS_ModSibBits( bits, baseClass ) \
	class ModSib##bits : public baseClass \
	{ \
		typedef baseClass _parent; \
	public: \
		explicit ModSib##bits( const xAddressInfo& src ) : _parent( src ) {} \
		explicit ModSib##bits( s32 disp ) : _parent( disp ) {} \
		ModSib##bits( const u##bits* target ) : _parent( target ) {} \
		ModSib##bits( xAddressReg base, xAddressReg index, int scale=0, s32 displacement=0 ) : \
			_parent( base, index, scale, displacement ) {} \
 \
		virtual uint GetOperandSize() const { return bits / 8; } \
 \
		__forceinline ModSib##bits& Add( s32 imm ) \
		{ \
			Displacement += imm; \
			return *this; \
		} \
 \
		__forceinline ModSib##bits operator+( const s32 imm ) const { return ModSib##bits( *this ).Add( imm ); } \
		__forceinline ModSib##bits operator-( const s32 imm ) const { return ModSib##bits( *this ).Add( -imm ); } \
 \
		bool operator==( const ModSib##bits& src ) const \
		{ \
			return \
				( Base == src.Base ) && ( Index == src.Index ) && \
				( Scale == src.Scale ) && ( Displacement == src.Displacement ); \
		} \
 \
		bool operator!=( const ModSib##bits& src ) const \
		{ \
			return !operator==( src ); \
		} \
	}

	DECLARE_CLASS_ModSibBits( 8, ModSib32orLess );
	DECLARE_CLASS_ModSibBits( 16, ModSib32orLess );
	DECLARE_CLASS_ModSibBits( 32, ModSib32orLess );
	DECLARE_CLASS_ModSibBits( 64, ModSibBase );
	DECLARE_CLASS_ModSibBits( 128, ModSibBase );

	// --------------------------------------------------------------------------------------
	//  xAddressIndexer
	// --------------------------------------------------------------------------------------
	// This is a type-translation "interface class" which provisions our ptr[] syntax.
	// xAddressReg types go in, and ModSibBase derived types come out.
	//
	template< typename xModSibType >
	struct xAddressIndexer
	{
		// passthrough instruction, allows ModSib to pass silently through ptr translation
		// without doing anything and without compiler error.
		const xModSibType& operator[]( const xModSibType& src ) const { return src; }

		xModSibType operator[]( xAddressReg src ) const
		{
			return xModSibType( src, xEmptyReg );
		}

		xModSibType operator[]( const xAddressInfo& src ) const
		{
			return xModSibType( src );
		}

		xModSibType operator[]( uptr src ) const
		{
			return xModSibType( src );
		}

		xModSibType operator[]( const void* src ) const
		{
			return xModSibType( (uptr)src );
		}
	};

	// ptr[] - use this form for instructions which can resolve the address operand size from
	// the other register operand sizes.
	extern const xAddressIndexer<ModSibBase>	ptr;
	extern const xAddressIndexer<ModSib128>		ptr128;
	extern const xAddressIndexer<ModSib64>		ptr64;
	extern const xAddressIndexer<ModSib32>		ptr32;
	extern const xAddressIndexer<ModSib16>		ptr16;
	extern const xAddressIndexer<ModSib8>		ptr8;

	// --------------------------------------------------------------------------------------
	//  xDirectOrIndirect
	// --------------------------------------------------------------------------------------
	// This utility class can represent either a direct (register) or indirect (memory address)
	// source or destination operand.  When supplied to an emitted instruction, the direct form
	// is favored *if* it is not Empty (xEmptyReg).  Otherwise the indirect form is used.
	//
#if 0
	template< typename xRegType, typename xSibType >
	class xDirectOrIndirect
	{
		xRegType	m_RegDirect;
		xSibType	m_MemIndirect;

	public:
		xDirectOrIndirect() :
			m_RegDirect(), m_MemIndirect( 0 ) {}

		xDirectOrIndirect( const xRegType& srcreg ) :
			m_RegDirect( srcreg ), m_MemIndirect( 0 ) {}

		explicit xDirectOrIndirect( const xSibType& srcmem ) :
			m_RegDirect(), m_MemIndirect( srcmem ) {}

		const xRegType& GetReg() const { return m_RegDirect; }
		const xSibType& GetMem() const { return m_MemIndirect; }
		bool IsDirect() const { return !m_RegDirect.IsEmpty(); }
		bool IsIndirect() const { return m_RegDirect.IsEmpty(); }

		bool operator==( const xDirectOrIndirect& src ) const
		{
			return IsDirect() ?
				(m_RegDirect == src.m_RegDirect) :
				(m_MemIndirect == src.m_MemIndirect);
		}

		bool operator!=( const xDirectOrIndirect& src ) const
		{
			return !operator==( src );
		}

		bool operator==( const xRegType& src ) const	{ return (m_RegDirect == src); }
		bool operator!=( const xRegType& src ) const	{ return (m_RegDirect != src); }
	};

	typedef xDirectOrIndirect<xRegister8,ModSib8>		xDirectOrIndirect8;
	typedef xDirectOrIndirect<xRegister16,ModSib16>		xDirectOrIndirect16;
	typedef xDirectOrIndirect<xRegister32,ModSib32>		xDirectOrIndirect32;
	typedef xDirectOrIndirect<xRegisterMMX,ModSib64>	xDirectOrIndirect64;
	typedef xDirectOrIndirect<xRegisterSSE,ModSib128>	xDirectOrIndirect128;
#endif

	// --------------------------------------------------------------------------------------
	//  xSmartJump
	// --------------------------------------------------------------------------------------
	// This class provides an interface for generating forward-based j8's or j32's "smartly"
	// as per the measured displacement distance.  If the displacement is a valid s8, then
	// a j8 is inserted, else a j32.
	//
	// Note: This class is inherently unsafe, and so it's recommended to use xForwardJump8/32
	// whenever it is known that the jump destination is (or is not) short.  Only use
	// xSmartJump in cases where it's unknown what jump encoding will be ideal.
	//
	// Important: Use this tool with caution!  xSmartJump cannot be used in cases where jump
	// targets overlap, since the writeback of the second target will alter the position of
	// the first target (which breaks the relative addressing).  To assist in avoiding such
	// errors, xSmartJump works based on C++ block scope, where the destruction of the
	// xSmartJump object (invoked by a '}') signals the target of the jump.  Example:
	//
	// {
	//     iCMP( EAX, ECX );
	//     xSmartJump jumpTo( Jcc_Above );
	//     [... conditional code ...]
	// }  // smartjump targets this spot.
	//
	// No code inside the scope can attempt to jump outside the scoped block (unless the jump
	// uses an immediate addressing method, such as Register or Mod/RM forms of JMP/CALL).
	// Multiple SmartJumps can be safely nested inside scopes, as long as they are properly
	// scoped themselves.
	//
	// Performance Analysis:  j8's use 4 less byes per opcode, and thus can provide minor
	// speed benefits in the form of L1/L2 cache clutter, on any CPU.  They're also notably
	// faster on P4's, and mildly faster on AMDs.  (Core2's and i7's don't care)
	//
	class xSmartJump
	{
		DeclareNoncopyableObject(xSmartJump);

	protected:
		u8* m_baseptr;				// base address of the instruction (passed to the instruction emitter)
		JccComparisonType m_cc;		// comparison type of the instruction

	public:
		int GetMaxInstructionSize() const
		{
			pxAssert( m_cc != Jcc_Unknown );
			return ( m_cc == Jcc_Unconditional ) ? 5 : 6;
		}

		JccComparisonType GetCondition() const	{ return m_cc; }
		virtual ~xSmartJump();

		// ------------------------------------------------------------------------
		// ccType - Comparison type to be written back to the jump instruction position.
		//
		xSmartJump( JccComparisonType ccType )
		{
			pxAssert( ccType != Jcc_Unknown );
			m_baseptr = xGetPtr();
			m_cc = ccType;
			xAdvancePtr( GetMaxInstructionSize() );
		}

	protected:
		void SetTarget();
	};

	// --------------------------------------------------------------------------------------
	//  xForwardJump
	// --------------------------------------------------------------------------------------
	// Primary use of this class is through the various xForwardJA8/xForwardJLE32/etc. helpers
	// defined later in this header. :)
	//

	class xForwardJumpBase
	{
	public:
		// pointer to base of the instruction *Following* the jump.  The jump address will be
		// relative to this address.
		s8* BasePtr;

	public:
		xForwardJumpBase( uint opsize, JccComparisonType cctype );

	protected:
		void _setTarget( uint opsize ) const;
	};

	template< typename OperandType >
	class xForwardJump : public xForwardJumpBase
	{
	public:
		static const uint OperandSize = sizeof( OperandType );

		// The jump instruction is emitted at the point of object construction.  The conditional
		// type must be valid (Jcc_Unknown generates an assertion).
		xForwardJump( JccComparisonType cctype = Jcc_Unconditional ) : xForwardJumpBase( OperandSize, cctype ) { }

		// Sets the jump target by writing back the current x86Ptr to the jump instruction.
		// This method can be called multiple times, re-writing the jump instruction's target
		// in each case. (the the last call is the one that takes effect).
		void SetTarget() const
		{
			_setTarget( OperandSize );
		}
	};

	static __forceinline xAddressInfo operator+( const void* addr, const xAddressReg& reg )
	{
		return xAddressInfo( reg, (sptr)addr );
	}

	static __forceinline xAddressInfo operator+( const void* addr, const xAddressInfo& reg )
	{
		return xAddressInfo( (sptr)addr ).Add( reg );
	}

	static __forceinline xAddressInfo operator+( s32 addr, const xAddressReg& reg )
	{
		return xAddressInfo( reg, (sptr)addr );
	}

	static __forceinline xAddressInfo operator+( s32 addr, const xAddressInfo& reg )
	{
		return xAddressInfo( (sptr)addr ).Add( reg );
	}
}

#include "implement/helpers.h"

#include "implement/simd_helpers.h"
#include "implement/simd_moremovs.h"
#include "implement/simd_arithmetic.h"
#include "implement/simd_comparisons.h"
#include "implement/simd_shufflepack.h"

#include "implement/group1.h"
#include "implement/group2.h"
#include "implement/group3.h"
#include "implement/movs.h"		// cmov and movsx/zx
#include "implement/dwshift.h"	// doubleword shifts!
#include "implement/incdec.h"
#include "implement/test.h"
#include "implement/jmpcall.h"

#include "inlines.inl"
