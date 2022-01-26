/*
**  Softpoly backend
**  Copyright (c) 2016-2020 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/

#include "poly_buffers.h"
#include "poly_framebuffer.h"
#include "poly_renderstate.h"
#include "poly_thread.h"
#include "engineerrors.h"

PolyBuffer *PolyBuffer::First = nullptr;

PolyBuffer::PolyBuffer()
{
	Next = First;
	First = this;
	if (Next) Next->Prev = this;
}

PolyBuffer::~PolyBuffer()
{
	if (Next) Next->Prev = Prev;
	if (Prev) Prev->Next = Next;
	else First = Next;

	auto fb = GetPolyFrameBuffer();
	if (fb && !mData.empty())
		fb->FrameDeleteList.Buffers.push_back(std::move(mData));
}

void PolyBuffer::ResetAll()
{
	for (PolyBuffer *cur = PolyBuffer::First; cur; cur = cur->Next)
		cur->Reset();
}

void PolyBuffer::Reset()
{
}

void PolyBuffer::SetData(size_t size, const void *data, BufferUsageType usage)
{
	mData.resize(size);
	map = mData.data();
	if (data)
		memcpy(map, data, size);
	buffersize = size;
}

void PolyBuffer::SetSubData(size_t offset, size_t size, const void *data)
{
	memcpy(static_cast<uint8_t*>(map) + offset, data, size);
}

void PolyBuffer::Resize(size_t newsize)
{
	mData.resize(newsize);
	buffersize = newsize;
	map = mData.data();
}

void PolyBuffer::Map()
{
}

void PolyBuffer::Unmap()
{
}

void *PolyBuffer::Lock(unsigned int size)
{
	if (mData.size() < (size_t)size)
		Resize(size);
	return map;
}

void PolyBuffer::Unlock()
{
}

/////////////////////////////////////////////////////////////////////////////

void PolyVertexBuffer::SetFormat(int numBindingPoints, int numAttributes, size_t stride, const FVertexBufferAttribute *attrs)
{
	VertexFormat = GetPolyFrameBuffer()->GetRenderState()->GetVertexFormat(numBindingPoints, numAttributes, stride, attrs);
}

/////////////////////////////////////////////////////////////////////////////

void PolyVertexInputAssembly::Load(PolyTriangleThreadData *thread, const void *vertices, int frame0, int frame1, int index)
{
	uint8_t* buff = (uint8_t*)vertices;

	// [GEC] finds the right frame.
	uint32_t offsets[2] = { static_cast<uint32_t>(frame0 * Stride), static_cast<uint32_t>(frame1 * Stride) };
	uint8_t* vertexBuffers[2] = { buff + offsets[0], buff + offsets[1] };

	const uint8_t* vertex = static_cast<const uint8_t*>(vertexBuffers[0]) + mStride * index;
	const float* attrVertex = reinterpret_cast<const float*>(vertex + mOffsets[VATTR_VERTEX]);

	const uint8_t* vertex2 = static_cast<const uint8_t*>(vertexBuffers[1]) + mStride * index;
	const float* attrVertex2 = reinterpret_cast<const float*>(vertex2 + mOffsets[VATTR_VERTEX]);

	const float *attrTexcoord = reinterpret_cast<const float*>(vertex + mOffsets[VATTR_TEXCOORD]);
	const uint8_t *attrColor = reinterpret_cast<const uint8_t*>(vertex + mOffsets[VATTR_COLOR]);
	const uint32_t* attrNormal = reinterpret_cast<const uint32_t*>(vertex + mOffsets[VATTR_NORMAL]);
	const uint32_t* attrNormal2 = reinterpret_cast<const uint32_t*>(vertex + mOffsets[VATTR_NORMAL2]);

	// [GEC] Apply the formula for model interpolation

	float newVertex[3];

	float t = thread->mainVertexShader.Data.uInterpolationFactor;
	float invt = 1.0f - t;

	newVertex[0] = (invt * attrVertex[0]) + (t * attrVertex2[0]);
	newVertex[1] = (invt * attrVertex[1]) + (t * attrVertex2[1]);
	newVertex[2] = (invt * attrVertex[2]) + (t * attrVertex2[2]);

	thread->mainVertexShader.aPosition = { newVertex[0], newVertex[1], newVertex[2], 1.0f };
	thread->mainVertexShader.aTexCoord = { attrTexcoord[0], attrTexcoord[1] };

	if ((UseVertexData & 1) == 0)
	{
		const auto &c = thread->mainVertexShader.Data.uVertexColor;
		thread->mainVertexShader.aColor.X = c.X;
		thread->mainVertexShader.aColor.Y = c.Y;
		thread->mainVertexShader.aColor.Z = c.Z;
		thread->mainVertexShader.aColor.W = c.W;
	}
	else
	{
		thread->mainVertexShader.aColor.X = attrColor[0] * (1.0f / 255.0f);
		thread->mainVertexShader.aColor.Y = attrColor[1] * (1.0f / 255.0f);
		thread->mainVertexShader.aColor.Z = attrColor[2] * (1.0f / 255.0f);
		thread->mainVertexShader.aColor.W = attrColor[3] * (1.0f / 255.0f);
	}

	if ((UseVertexData & 2) == 0)
	{
		const auto &n = thread->mainVertexShader.Data.uVertexNormal;
		thread->mainVertexShader.aNormal = FVector4(n.X, n.Y, n.Z, 1.0);
		thread->mainVertexShader.aNormal2 = thread->mainVertexShader.aNormal;
	}
	else
	{
		int n = *attrNormal;
		int n2 = *attrNormal2;
		float x = ((n << 22) >> 22) / 512.0f;
		float y = ((n << 12) >> 22) / 512.0f;
		float z = ((n << 2) >> 22) / 512.0f;
		float x2 = ((n2 << 22) >> 22) / 512.0f;
		float y2 = ((n2 << 12) >> 22) / 512.0f;
		float z2 = ((n2 << 2) >> 22) / 512.0f;
		thread->mainVertexShader.aNormal = FVector4(x, y, z, 0.0f);
		thread->mainVertexShader.aNormal2 = FVector4(x2, y2, z2, 0.0f);
	}
}

/////////////////////////////////////////////////////////////////////////////

void PolyDataBuffer::BindRange(FRenderState *state, size_t start, size_t length)
{
	static_cast<PolyRenderState*>(state)->Bind(this, (uint32_t)start, (uint32_t)length);
}
