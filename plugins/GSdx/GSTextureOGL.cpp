/*
 *	Copyright (C) 2011-2011 Gregory hainaut
 *	Copyright (C) 2007-2009 Gabest
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

#include "GSTextureOGL.h"

GSTextureOGL::GSTextureOGL(int type, int w, int h, bool msaa, int format)
	: m_texture_unit(0),
	  m_extra_buffer_id(0),
	  m_extra_buffer_allocated(false)
{
	// *************************************************************
	// Opengl world

	// http://www.opengl.org/wiki/Renderbuffer_Object
	// render:	constructor -> glGenRenderbuffers, glDeleteRenderbuffers
	//			info		-> glGetRenderbufferParameteriv, glIsRenderbuffer
	//		    binding		-> glBindRenderbuffer (only 1 target), glFramebufferRenderbuffer (attach actually)
	//			setup param -> glRenderbufferStorageMultisample (after this call the buffer is unitialized)
	//
	// the only way to use a renderbuffer object is to attach it to a Framebuffer Object. After you bind that
	// FBO to the context, and set up the draw or read buffers appropriately, you can use pixel transfer operations
	// to read and write to it. Of course, you can also render to it. The standard glClear function will also clear the appropriate buffer.
	//
	// Render work in parallal with framebuffer object (FBO) http://www.opengl.org/wiki/Framebuffer_Objects
	// render are attached to FBO through : glFramebufferRenderbuffer. You can query the number of colorattachement with GL_MAX_COLOR_ATTACHMENTS
	// FBO   :  constructor -> glGenFramebuffers, glDeleteFramebuffers
	//			binding     -> glBindFramebuffer (target can be read/write/both)
	//			blit		-> glBlitFramebuffer (read FB to draw FB)
	//			info		-> glIsFramebuffer, glGetFramebufferAttachmentParameter, glCheckFramebufferStatus
	//
	// There are two types of framebuffer-attachable images; texture images and renderbuffer images.
	// If an image of a texture object is attached to a framebuffer, OpenGL performs "render to texture".
	// And if an image of a renderbuffer object is attached to a framebuffer, then OpenGL performs "offscreen rendering".
	// Blitting:
	// glDrawBuffers
	// glReadBuffer
	// glBlitFramebuffer

	// *************************************************************
	// Doc
	// It might need a texture structure to replace ID3D11Texture2D.
	//
	// == The function need to set (from parameter)
	//		: m_size.x
	//		: m_size.y
	//		: m_type
	//		: m_format
	//		: m_msaa
	m_size.x = w;
	m_size.y = h;
	m_format = format;
	m_type   = type;
	m_msaa   = msaa;

	// == Might be useful to save
	//		: m_texture_target (like GL_TEXTURE_2D, GL_TEXTURE_RECTANGLE etc...)
	//		: m_texture_id (return by glGen*)
	//
	// == Then generate the texture or buffer.
	// D3D11_BIND_RENDER_TARGET: Bind a texture as a render target for the output-merger stage.
	//		=> glGenRenderbuffers with GL_COLOR_ATTACHMENTi (Hum glGenTextures might work too)
	// D3D11_BIND_DEPTH_STENCIL: Bind a texture as a depth-stencil target for the output-merger stage.
	//		=> glGenRenderbuffers with GL_DEPTH_STENCIL_ATTACHMENT
	// D3D11_BIND_SHADER_RESOURCE: Bind a buffer or texture to a shader stage
	//		=> glGenTextures
	// D3D11_USAGE_STAGING: A resource that supports data transfer (copy) from the GPU to the CPU.
	//		glGenTextures
	//		glReadPixels seems to use a framebuffer. In this case you can setup the source
	//		with glReadBuffer(GL_COLOR_ATTACHMENTi)
	//		glGetTexImage: read pixels of a bound texture
	//		=> To allow map/unmap. I think we can use a pixel buffer (target GL_PIXEL_UNPACK_BUFFER)
	//		http://www.opengl.org/wiki/Pixel_Buffer_Objects

	// Generate the buffer
	switch (m_type) {
		case GSTexture::RenderTarget:
			// FIXME what is the real use case of this texture
			// Maybe a texture will be better
			// glGenRenderbuffers(1, &m_texture_id);
			// m_texture_target = GL_RENDERBUFFER;
			// Buffer can not be read by shader and must be blit before. I see no point to use a render buffer
			// when you can directly render on the texture. It said it could be faster...
			glGenTextures(1, &m_texture_id);
			m_texture_target = GL_TEXTURE_2D;
			break;
		case GSTexture::DepthStencil:
			glGenRenderbuffers(1, &m_texture_id);
			m_texture_target = GL_RENDERBUFFER;
			break;
		case GSTexture::Texture:
			glGenTextures(1, &m_texture_id);
			// FIXME, do we need rectangle (or better to use 2D texture)
			m_texture_target = GL_TEXTURE_2D;
			//m_texture_target = GL_TEXTURE_RECTANGLE;
			// == For texture, the Unit must be selected
			break;
		case GSTexture::Offscreen:
			//FIXME I not sure we need a pixel buffer object. It seems more a texture
			// glGenBuffers(1, &m_texture_id);
			// m_texture_target = GL_PIXEL_UNPACK_BUFFER;
			assert(0);
			// Note there is also a buffer texture!!!
			// http://www.opengl.org/wiki/Buffer_Texture
			// Note: in this case it must use in GLSL
			// gvec texelFetch(gsampler sampler, ivec texCoord, int lod[, int sample]);
			// corollary we can maybe use it for multisample stuff
			break;
		default:
			break;
	}
	// Extra buffer to handle various pixel transfer
	glGenBuffers(1, &m_extra_buffer_id);

	uint msaa_level;
	if (m_msaa) {
		// FIXME  which level of MSAA
		msaa_level = 1;
	} else {
		msaa_level = 0;
	}

	// Allocate the buffer
	switch (m_type) {
		case GSTexture::DepthStencil:
			glBindRenderbuffer(m_texture_target, m_texture_id);
			glRenderbufferStorageMultisample(m_texture_target, msaa_level, m_format, m_size.y, m_size.x);
			break;
		case GSTexture::RenderTarget:
		case GSTexture::Texture:
			// FIXME
			// Howto allocate the texture unit !!!
			// In worst case the HW renderer seems to use 3 texture unit
			// For the moment SW renderer only use 1 so don't bother
			EnableUnit(0);
			if (m_format == GL_RGBA8) {
				glTexImage2D(m_texture_target, 0, m_format, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			}
			else
				assert(0);
			break;
		case GSTexture::Offscreen:
			assert(0);
			break;
		default: break;
	}
}

GSTextureOGL::~GSTextureOGL()
{
	// glDeleteTextures or glDeleteRenderbuffers
}

void GSTextureOGL::Attach(GLenum attachment)
{
	if (m_type == GSTexture::DepthStencil)
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment, m_texture_target, m_texture_id);
	else
		glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, m_texture_target, m_texture_id, 0);
}

bool GSTextureOGL::Update(const GSVector4i& r, const void* data, int pitch)
{
	// static int update_count = 0;
	// update_count++;
	// To update only a part of the texture you can use:
	// glTexSubImage2D — specify a two-dimensional texture subimage
	if (m_type == GSTexture::DepthStencil || m_type == GSTexture::Offscreen) assert(0);

	// glTexSubImage2D specifies a two-dimensional subtexture for the current texture unit, specified with glActiveTexture.
	// If a non-zero named buffer object is bound to the GL_PIXEL_UNPACK_BUFFER target
	//(see glBindBuffer) while a texture image is
	// specified, data is treated as a byte offset into the buffer object's data store
	// FIXME warning order of the y axis
	// FIXME I'm not confident with GL_UNSIGNED_BYTE type
	// FIXME add a state check

	EnableUnit(0);

	if (m_format != GL_RGBA8) assert(0);

	// FIXME. That suck but I don't know how to do better for now. I think we need to create a texture buffer and directly upload the copy
	// The case appears on SW mode. Src pitch is 2x dst pitch.
	int rowbytes = r.width() << 2;
	if (pitch != rowbytes) {
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_extra_buffer_id);

		if (!m_extra_buffer_allocated) {
			glBufferData(GL_PIXEL_UNPACK_BUFFER, m_size.x * m_size.y * 4, NULL, GL_STREAM_DRAW);
			m_extra_buffer_allocated = true;
		}

		uint8* src = (uint8*) data;
		uint8* dst = (uint8*) glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
		for(int h = r.height(); h > 0; h--, src += pitch, dst += rowbytes)
		{
			memcpy(dst, src, rowbytes);
		}
		glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

		glTexSubImage2D(m_texture_target, 0, r.x, r.y, r.width(), r.height(), GL_RGBA, GL_UNSIGNED_BYTE, 0 /* offset in UNPACK BUFFER */);

		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

	} else {
		glTexSubImage2D(m_texture_target, 0, r.x, r.y, r.width(), r.height(), GL_RGBA, GL_UNSIGNED_BYTE, data);
	}

#if 0
	if(m_dev && m_texture)
	{
		D3D11_BOX box = {r.left, r.top, 0, r.right, r.bottom, 1};

		m_ctx->UpdateSubresource(m_texture, 0, &box, data, pitch, 0);

		return true;
	}

	return false;
#endif
}

void GSTextureOGL::EnableUnit(uint unit)
{
	switch (m_type) {
		case GSTexture::DepthStencil:
		case GSTexture::Offscreen:
			assert(0);
			break;
		case GSTexture::RenderTarget:
		case GSTexture::Texture:
			// FIXME
			// Howto allocate the texture unit !!!
			// In worst case the HW renderer seems to use 3 texture unit
			// For the moment SW renderer only use 1 so don't bother
			glActiveTexture(GL_TEXTURE0 + unit);
			glBindTexture(m_texture_target, m_texture_id);
			break;
	}
}

bool GSTextureOGL::Map(GSMap& m, const GSVector4i* r)
{
	// The function allow to modify the texture from the CPU
	// Set m.bits <- pointer to the data
	// Set m.pitch <- size of a row
	// I think in opengl we need to copy back the data to the RAM: glReadPixels — read a block of pixels from the frame buffer
	//
	// glMapBuffer — map a buffer object's data store
	// Can be used on GL_PIXEL_UNPACK_BUFFER or GL_TEXTURE_BUFFER
	return false;
#if 0
	if(r != NULL)
	{
		// ASSERT(0); // not implemented

		return false;
	}

	if(m_texture && m_desc.Usage == D3D11_USAGE_STAGING)
	{
		D3D11_MAPPED_SUBRESOURCE map;

		if(SUCCEEDED(m_ctx->Map(m_texture, 0, D3D11_MAP_READ_WRITE, 0, &map)))
		{
			m.bits = (uint8*)map.pData;
			m.pitch = (int)map.RowPitch;

			return true;
		}
	}

	return false;
#endif
}

void GSTextureOGL::Unmap()
{
	// copy the texture to the GPU
	// GLboolean glUnmapBuffer(GLenum target);
#if 0
	if(m_texture)
	{
		m_ctx->Unmap(m_texture, 0);
	}
#endif
}

#ifndef _WINDOWS

#pragma pack(push, 1)

struct BITMAPFILEHEADER
{
	uint16 bfType;
	uint32 bfSize;
	uint16 bfReserved1;
	uint16 bfReserved2;
	uint32 bfOffBits;
};

struct BITMAPINFOHEADER
{
	uint32 biSize;
	int32 biWidth;
	int32 biHeight;
	uint16 biPlanes;
	uint16 biBitCount;
	uint32 biCompression;
	uint32 biSizeImage;
	int32 biXPelsPerMeter;
	int32 biYPelsPerMeter;
	uint32 biClrUsed;
	uint32 biClrImportant;
};

#define BI_RGB 0

#pragma pack(pop)

#endif

bool GSTextureOGL::Save(const string& fn, bool dds)
{
	switch (m_type) {
		case GSTexture::DepthStencil:
		case GSTexture::Offscreen:
			ASSERT(0);
			break;
		default: break;
	}

	// Collect the texture data
	char* image = (char*)malloc(4 * m_size.x * m_size.y);
	if (m_type) {
		EnableUnit(0);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
	} else {
		// TODO backbuffer
		glReadBuffer(GL_BACK);
		glReadPixels(0, 0, m_size.x, m_size.y, GL_RGBA, GL_UNSIGNED_BYTE, image);
	}

	// Build a BMP file
	if(FILE* fp = fopen(fn.c_str(), "wb"))
	{
		BITMAPINFOHEADER bih;

		memset(&bih, 0, sizeof(bih));

		bih.biSize = sizeof(bih);
		bih.biWidth = m_size.x;
		bih.biHeight = m_size.y;
		bih.biPlanes = 1;
		bih.biBitCount = 32;
		bih.biCompression = BI_RGB;
		bih.biSizeImage = m_size.x * m_size.y << 2;

		BITMAPFILEHEADER bfh;

		memset(&bfh, 0, sizeof(bfh));

		uint8* bfType = (uint8*)&bfh.bfType;

		// bfh.bfType = 'MB';
		bfType[0] = 0x42;
		bfType[1] = 0x4d;
		bfh.bfOffBits = sizeof(bfh) + sizeof(bih);
		bfh.bfSize = bfh.bfOffBits + bih.biSizeImage;
		bfh.bfReserved1 = bfh.bfReserved2 = 0;

		fwrite(&bfh, 1, sizeof(bfh), fp);
		fwrite(&bih, 1, sizeof(bih), fp);

		uint32 pitch = 4 * m_size.x;
		uint8* data = (uint8*)image + (m_size.y - 1) * pitch;

		for(int h = m_size.y; h > 0; h--, data -= pitch)
		{
			fwrite(data, 1, m_size.x << 2, fp); // TODO: swap red-blue?
		}

		fclose(fp);

		free(image);
		return true;
	}

	return false;
#if 0
	CComPtr<ID3D11Resource> res;

	if(m_desc.BindFlags & D3D11_BIND_DEPTH_STENCIL)
	{
		HRESULT hr;

		D3D11_TEXTURE2D_DESC desc;

		memset(&desc, 0, sizeof(desc));

		m_texture->GetDesc(&desc);

		desc.Usage = D3D11_USAGE_STAGING;
		desc.BindFlags = 0;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

		CComPtr<ID3D11Texture2D> src, dst;

		hr = m_dev->CreateTexture2D(&desc, NULL, &src);

		m_ctx->CopyResource(src, m_texture);

		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		hr = m_dev->CreateTexture2D(&desc, NULL, &dst);

		D3D11_MAPPED_SUBRESOURCE sm, dm;

		hr = m_ctx->Map(src, 0, D3D11_MAP_READ, 0, &sm);
		hr = m_ctx->Map(dst, 0, D3D11_MAP_WRITE, 0, &dm);

		uint8* s = (uint8*)sm.pData;
		uint8* d = (uint8*)dm.pData;

		for(uint32 y = 0; y < desc.Height; y++, s += sm.RowPitch, d += dm.RowPitch)
		{
			for(uint32 x = 0; x < desc.Width; x++)
			{
				((uint32*)d)[x] = (uint32)(ldexpf(((float*)s)[x*2], 32));
			}
		}

		m_ctx->Unmap(src, 0);
		m_ctx->Unmap(dst, 0);

		res = dst;
	}
	else
	{
		res = m_texture;
	}

	return SUCCEEDED(D3DX11SaveTextureToFile(m_ctx, res, dds ? D3DX11_IFF_DDS : D3DX11_IFF_BMP, fn.c_str()));
#endif
}
