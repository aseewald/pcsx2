/*
 *	Copyright (C) 2007-2009 Gabest
 *	http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#pragma once

#include "GS.h"
#include "GSVertexSW.h"
#include "GSFunctionMap.h"
#include "GSThread.h"
#include "GSAlignedClass.h"

__aligned(class, 32) GSRasterizerData : public GSAlignedClass<32>
{
public:
	GSVector4i scissor;
	GSVector4i bbox;
	GS_PRIM_CLASS primclass;
	GSVertexSW* vertices;
	int count;
	bool solidrect;
	uint64 frame;
	void* param;

	GSRasterizerData() 
		: vertices(NULL)
		, count(0)
		, solidrect(false)
		, param(NULL)
	{
	}

	virtual ~GSRasterizerData() 
	{
		if(vertices != NULL) _aligned_free(vertices);

		// derived class should free param and its members
	} 
};

class IDrawScanline : public GSAlignedClass<32>
{
public:
	typedef void (__fastcall *SetupPrimPtr)(const GSVertexSW* vertices, const GSVertexSW& dscan);
	typedef void (__fastcall *DrawScanlinePtr)(int pixels, int left, int top, const GSVertexSW& scan);
	typedef void (IDrawScanline::*DrawRectPtr)(const GSVector4i& r, const GSVertexSW& v); // TODO: jit

protected:
	SetupPrimPtr m_sp;
	DrawScanlinePtr m_ds;
	DrawScanlinePtr m_de;
	DrawRectPtr m_dr;

public:
	IDrawScanline() : m_sp(NULL), m_ds(NULL), m_de(NULL), m_dr(NULL) {}
	virtual ~IDrawScanline() {}

	virtual void BeginDraw(const void* param) = 0;
	virtual void EndDraw(uint64 frame, uint64 ticks, int pixels) = 0;

#ifdef ENABLE_JIT_RASTERIZER

	__forceinline void SetupPrim(const GSVertexSW* vertices, const GSVertexSW& dscan) {m_sp(vertices, dscan);}
	__forceinline void DrawScanline(int pixels, int left, int top, const GSVertexSW& scan) {m_ds(pixels, left, top, scan);}
	__forceinline void DrawEdge(int pixels, int left, int top, const GSVertexSW& scan) {m_de(pixels, left, top, scan);}
	__forceinline void DrawRect(const GSVector4i& r, const GSVertexSW& v) {(this->*m_dr)(r, v);}

#else

	virtual void SetupPrim(const GSVertexSW* vertices, const GSVertexSW& dscan) = 0;
	virtual void DrawScanline(int pixels, int left, int top, const GSVertexSW& scan) = 0;
	virtual void DrawEdge(int pixels, int left, int top, const GSVertexSW& scan) = 0;
	virtual void DrawRect(const GSVector4i& r, const GSVertexSW& v) = 0;
	
#endif

	__forceinline bool HasEdge() const {return m_de != NULL;}
};

class IRasterizer : public GSAlignedClass<32>
{
public:
	virtual ~IRasterizer() {}

	virtual void Queue(shared_ptr<GSRasterizerData> data) = 0;
	virtual void Sync() = 0;
};

__aligned(class, 32) GSRasterizer : public IRasterizer
{
protected:
	IDrawScanline* m_ds;
	int m_id;
	int m_threads;
	uint8* m_myscanline;
	GSVector4i m_scissor;
	GSVector4 m_fscissor;
	struct {GSVertexSW* buff; int count;} m_edge;
	int m_pixels;

	typedef void (GSRasterizer::*DrawPrimPtr)(const GSVertexSW* v, int count);

	template<bool scissor_test> 
	void DrawPoint(const GSVertexSW* v, int count);
	void DrawLine(const GSVertexSW* v);
	void DrawTriangle(const GSVertexSW* v);
	void DrawSprite(const GSVertexSW* v, bool solidrect);

	__forceinline void DrawTriangleSection(int top, int bottom, GSVertexSW& edge, const GSVertexSW& dedge, const GSVertexSW& dscan, const GSVector4& p0);

	void DrawEdge(const GSVertexSW& v0, const GSVertexSW& v1, const GSVertexSW& dv, int orientation, int side);

	__forceinline void AddScanline(GSVertexSW* e, int pixels, int left, int top, const GSVertexSW& scan);
	__forceinline void Flush(const GSVertexSW* vertices, const GSVertexSW& dscan, bool edge = false);

public:
	GSRasterizer(IDrawScanline* ds, int id, int threads);
	virtual ~GSRasterizer();

	__forceinline bool IsOneOfMyScanlines(int scanline) const;
	__forceinline bool IsOneOfMyScanlines(int top, int bottom) const;

	void Draw(shared_ptr<GSRasterizerData> data);

	// IRasterizer

	void Queue(shared_ptr<GSRasterizerData> data);
};

class GSRasterizerMT : public GSRasterizer, private GSThread
{
protected:
	volatile bool m_exit;
	volatile bool m_break;
	volatile bool m_ready;
	GSAutoResetEvent m_draw;
	queue<shared_ptr<GSRasterizerData> > m_queue;
	GSCritSec m_lock;

	void ThreadProc();

public:
	GSRasterizerMT(IDrawScanline* ds, int id, int threads);
	virtual ~GSRasterizerMT();

	// IRasterizer

	void Queue(shared_ptr<GSRasterizerData> data);
	void Sync();
};

class GSRasterizerList : public IRasterizer, protected vector<GSRasterizer*>
{
protected:
	int m_count;
	int m_dispatched;

	GSRasterizerList();

public:
	virtual ~GSRasterizerList();

	template<class DS> static GSRasterizerList* Create(int threads)
	{
		GSRasterizerList* rl = new GSRasterizerList();

		threads = std::max<int>(threads, 1); // TODO: min(threads, number of cpu cores)

		for(int i = 0; i < threads; i++)
		{
			rl->push_back(new GSRasterizerMT(new DS(), i, threads));
		}

		return rl;
	}

	size_t IsMultiThreaded() const {return size();}

	void Draw(shared_ptr<GSRasterizerData> data);

	// IRasterizer

	void Queue(shared_ptr<GSRasterizerData> data);
	void Sync();

	int m_sync_count;
};
