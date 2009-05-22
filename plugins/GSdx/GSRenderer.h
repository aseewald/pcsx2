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

#include "GSdx.h"
#include "GSWnd.h"
#include "GSState.h"
#include "GSVertexList.h"
#include "GSSettingsDlg.h"
#include "GSCapture.h"

class GSRenderer : public GSState
{
	GSCapture m_capture;
	string m_snapshot;
	int m_shader;

	bool Merge(int field);

protected:
	int m_interlace;
	int m_aspectratio;
	int m_filter;
	bool m_vsync;
	bool m_nativeres;
	bool m_aa1;
	bool m_blur;

	virtual void ResetDevice() {}
	virtual GSTexture* GetOutput(int i) = 0;

public:
	GSWnd m_wnd;
	GSDevice* m_dev;
	bool m_psrr;

	int s_n;
	bool s_dump;
	bool s_save;
	bool s_savez;

public:
	GSRenderer(uint8* base, bool mt, void (*irq)(), GSDevice* dev, bool psrr = true);
	virtual ~GSRenderer();

	virtual bool Create(const string& title);

	virtual void VSync(int field);
	virtual void KeyEvent(GSKeyEventData* e);
	virtual bool MakeSnapshot(const string& path);

	virtual void MinMaxUV(int w, int h, GSVector4i& r)
	{
		r = GSVector4i(0, 0, w, h);
	}

	virtual bool CanUpscale() 
	{
		return !m_nativeres;
	}
};

template<class Vertex> class GSRendererT : public GSRenderer
{
protected:
	Vertex* m_vertices;
	int m_count;
	int m_maxcount;
	GSVertexList<Vertex> m_vl;

	void Reset()
	{
		m_count = 0;
		m_vl.RemoveAll();

		__super::Reset();
	}

	void ResetPrim()
	{
		m_vl.RemoveAll();
	}

	void FlushPrim() 
	{
		if(m_count > 0)
		{
			/*
			TRACE(_T("[%d] Draw f %05x (%d) z %05x (%d %d %d %d) t %05x %05x (%d)\n"), 
				  (int)m_perfmon.GetFrame(), 
				  (int)m_context->FRAME.Block(), 
				  (int)m_context->FRAME.PSM, 
				  (int)m_context->ZBUF.Block(), 
				  (int)m_context->ZBUF.PSM, 
				  m_context->TEST.ZTE, 
				  m_context->TEST.ZTST, 
				  m_context->ZBUF.ZMSK, 
				  PRIM->TME ? (int)m_context->TEX0.TBP0 : 0xfffff, 
				  PRIM->TME && m_context->TEX0.PSM > PSM_PSMCT16S ? (int)m_context->TEX0.CBP : 0xfffff, 
				  PRIM->TME ? (int)m_context->TEX0.PSM : 0xff);
			*/

			if(GSUtil::EncodePSM(m_context->FRAME.PSM) != 3 && GSUtil::EncodePSM(m_context->ZBUF.PSM) != 3)
			{
				// FIXME: berserk fpsm = 27 (8H)

				Draw();
			}

			m_count = 0;
		}
	}

	void GrowVertexBuffer()
	{
		m_maxcount = max(10000, m_maxcount * 3/2);
		m_vertices = (Vertex*)_aligned_realloc(m_vertices, sizeof(Vertex) * m_maxcount, 16);
		m_maxcount -= 100;
	}

	template<uint32 prim> __forceinline Vertex* DrawingKick(bool skip, int& count)
	{
		switch(prim)
		{
		case GS_POINTLIST: count = 1; break;
		case GS_LINELIST: count = 2; break;
		case GS_LINESTRIP: count = 2; break;
		case GS_TRIANGLELIST: count = 3; break;
		case GS_TRIANGLESTRIP: count = 3; break;
		case GS_TRIANGLEFAN: count = 3; break;
		case GS_SPRITE: count = 2; break;
		case GS_INVALID: count = 1; break;
		default: __assume(0);
		}

		if(m_vl.GetCount() < count)
		{
			return NULL;
		}

		if(m_count >= m_maxcount)
		{
			GrowVertexBuffer();
		}

		Vertex* v = &m_vertices[m_count];

		switch(prim)
		{
		case GS_POINTLIST:
			m_vl.GetAt(0, v[0]);
			m_vl.RemoveAll();
			break;
		case GS_LINELIST:
			m_vl.GetAt(0, v[0]);
			m_vl.GetAt(1, v[1]);
			m_vl.RemoveAll();
			break;
		case GS_LINESTRIP:
			m_vl.GetAt(0, v[0]);
			m_vl.GetAt(1, v[1]);
			m_vl.RemoveAt(0, 1);
			break;
		case GS_TRIANGLELIST:
			m_vl.GetAt(0, v[0]);
			m_vl.GetAt(1, v[1]);
			m_vl.GetAt(2, v[2]);
			m_vl.RemoveAll();
			break;
		case GS_TRIANGLESTRIP:
			m_vl.GetAt(0, v[0]);
			m_vl.GetAt(1, v[1]);
			m_vl.GetAt(2, v[2]);
			m_vl.RemoveAt(0, 2);
			break;
		case GS_TRIANGLEFAN:
			m_vl.GetAt(0, v[0]);
			m_vl.GetAt(1, v[1]);
			m_vl.GetAt(2, v[2]);
			m_vl.RemoveAt(1, 1);
			break;
		case GS_SPRITE:
			m_vl.GetAt(0, v[0]);
			m_vl.GetAt(1, v[1]);
			m_vl.RemoveAll();
			break;
		case GS_INVALID:
			ASSERT(0);
			m_vl.RemoveAll();
			return NULL;
		default:
			__assume(0);
		}

		return !skip ? v : NULL;
	}

	virtual void Draw() = 0;

public:
	GSRendererT(uint8* base, bool mt, void (*irq)(), GSDevice* dev, bool psrr = true)
		: GSRenderer(base, mt, irq, dev, psrr)
		, m_count(0)
		, m_maxcount(0)
		, m_vertices(NULL)
	{
	}

	~GSRendererT()
	{
		if(m_vertices) _aligned_free(m_vertices);
	}
};
