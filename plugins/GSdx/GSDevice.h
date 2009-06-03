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

#include "GSWnd.h"
#include "GSTexture.h"
#include "GSVertex.h"
#include "GSAlignedClass.h"

#pragma pack(push, 1)

struct MergeConstantBuffer
{
	GSVector4 BGColor;

	struct MergeConstantBuffer() {memset(this, 0, sizeof(*this));}
};

struct InterlaceConstantBuffer
{
	GSVector2 ZrH;
	float hH;
	float _pad[1];

	struct InterlaceConstantBuffer() {memset(this, 0, sizeof(*this));}
};

#pragma pack(pop)

class GSDevice : public GSAlignedClass<16>
{
	list<GSTexture*> m_pool;

	GSTexture* Fetch(int type, int w, int h, int format);

protected:
	GSWnd* m_wnd;
	bool m_vsync;
	GSTexture* m_backbuffer;
	GSTexture* m_merge;
	GSTexture* m_weavebob;
	GSTexture* m_blend;
	GSTexture* m_1x1;
	GSTexture* m_current;

	virtual GSTexture* Create(int type, int w, int h, int format) = 0;

	virtual void DoMerge(GSTexture* st[2], GSVector4* sr, GSVector4* dr, GSTexture* dt, bool slbg, bool mmod, const GSVector4& c) = 0;
	virtual void DoInterlace(GSTexture* st, GSTexture* dt, int shader, bool linear, float yoffset) = 0;

public:
	GSDevice();
	virtual ~GSDevice();

	void Recycle(GSTexture* t);

	virtual bool Create(GSWnd* wnd, bool vsync);
	virtual bool Reset(int w, int h, bool fs);

	virtual bool IsLost() {return false;}
	virtual void Present(const GSVector4i& r, int shader);
	virtual void Flip() {};

	virtual void BeginScene() {};
	virtual void EndScene() {};

	virtual void ClearRenderTarget(GSTexture* t, const GSVector4& c) {};
	virtual void ClearRenderTarget(GSTexture* t, uint32 c) {};
	virtual void ClearDepth(GSTexture* t, float c) {};
	virtual void ClearStencil(GSTexture* t, uint8 c) {};

	virtual GSTexture* CreateRenderTarget(int w, int h, int format = 0);
	virtual GSTexture* CreateDepthStencil(int w, int h, int format = 0);
	virtual GSTexture* CreateTexture(int w, int h, int format = 0);
	virtual GSTexture* CreateOffscreen(int w, int h, int format = 0);

	virtual GSTexture* CopyOffscreen(GSTexture* src, const GSVector4& sr, int w, int h, int format = 0) {return NULL;}

	virtual void StretchRect(GSTexture* st, GSTexture* dt, const GSVector4& dr, int shader = 0, bool linear = true);
	virtual void StretchRect(GSTexture* st, const GSVector4& sr, GSTexture* dt, const GSVector4& dr, int shader = 0, bool linear = true) {}

	GSTexture* GetCurrent();
	virtual bool IsCurrentRGBA() {return true;}

	void Merge(GSTexture* st[2], GSVector4* sr, GSVector4* dr, const GSVector2i& fs, bool slbg, bool mmod, const GSVector4& c);
	void Interlace(const GSVector2i& ds, int field, int mode, float yoffset);

	virtual void PSSetShaderResources(GSTexture* sr0, GSTexture* sr1) {}
	virtual void OMSetRenderTargets(GSTexture* rt, GSTexture* ds) {};
};
