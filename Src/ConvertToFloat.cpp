#include "ConvertToFloat.h"

ConvertToFloat::ConvertToFloat(PClip _child, bool _convertYuv, int _precision, IScriptEnvironment* env) :
	GenericVideoFilter(_child), precision(_precision), convertYUV(_convertYuv) {
	if (!vi.IsYV24() && !vi.IsRGB32())
		env->ThrowError("Source must be YV12, YV24 or RGB32");
	if (precision != 1 && precision != 2)
		env->ThrowError("Precision must be 1 or 2");

	// Convert from YV24 to half-float RGB
	viRGB = vi;
	viRGB.pixel_type = VideoInfo::CS_BGR32;
	if (precision == 2) // Half-float frame has its width twice larger than normal
		viRGB.width <<= 1;
	
	if (precision == 1)
		precisionShift = 2;
	else
		precisionShift = 4;

	if (precision == 2) {
		floatBufferPitch = vi.width * 4 * 4;
		floatBuffer = (unsigned char*)malloc(floatBufferPitch * vi.height);
		halfFloatBufferPitch = vi.width * 4 * 2;
		halfFloatBuffer = (unsigned char*)malloc(halfFloatBufferPitch * vi.height);
	}
}

ConvertToFloat::~ConvertToFloat() {
	if (precision == 2) {
		free(floatBuffer);
		free(halfFloatBuffer);
	}
}

PVideoFrame __stdcall ConvertToFloat::GetFrame(int n, IScriptEnvironment* env) {
	PVideoFrame src = child->GetFrame(n, env);

	// Convert from YV24 to half-float RGB
	PVideoFrame dst = env->NewVideoFrame(viRGB);
	if (vi.IsRGB32())
		convRgbToFloat(src->GetReadPtr(), dst->GetWritePtr(), src->GetPitch(), dst->GetPitch(), vi.width, vi.height, env);
	else
		convYV24ToFloat(src->GetReadPtr(PLANAR_Y), src->GetReadPtr(PLANAR_U), src->GetReadPtr(PLANAR_V), dst->GetWritePtr(), src->GetPitch(PLANAR_Y), src->GetPitch(PLANAR_U), dst->GetPitch(), vi.width, vi.height, env);

	return dst;
}

void ConvertToFloat::convYV24ToFloat(const byte *py, const byte *pu, const byte *pv,
	unsigned char *dst, int pitch1Y, int pitch1UV, int pitch2, int width, int height, IScriptEnvironment* env)
{
	unsigned char* dstLoop = precision == 1 ? dst : floatBuffer;
	int dstLoopPitch = precision == 1 ? pitch2 : floatBufferPitch;

	// Convert all data to float
	int Y, U, V;
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			Y = py[x];
			U = pu[x];
			V = pv[x];

			convFloat(Y, U, V, dstLoop + (x << precisionShift));
		}
		py += pitch1Y;
		pu += pitch1UV;
		pv += pitch1UV;
		dstLoop += dstLoopPitch;
	}

	if (precision == 2) {
		// Convert float buffer to half-float
		D3DXFloat32To16Array((D3DXFLOAT16*)halfFloatBuffer, (float*)floatBuffer, width * 4 * height);

		// Copy half-float data back into frame
		env->BitBlt(dst, pitch2, halfFloatBuffer, halfFloatBufferPitch, halfFloatBufferPitch, height);
	}
}

void ConvertToFloat::convRgbToFloat(const byte *src, unsigned char *dst, int srcPitch, int dstPitch, int width, int height, IScriptEnvironment* env) {
	unsigned char* dstLoop = precision == 1 ? dst : floatBuffer;
	int dstLoopPitch = precision == 1 ? dstPitch : floatBufferPitch;

	int B, G, R;
	src += height * srcPitch;
	for (int y = 0; y < height; ++y) {
		src -= srcPitch;
		for (int x = 0; x < width; ++x) {
			B = src[x * 4];
			G = src[x * 4 + 1];
			R = src[x * 4 + 2];

			convFloat(R, G, B, dstLoop + (x << precisionShift));
		}
		dstLoop += dstLoopPitch;
	}

	if (precision == 2) {
		// Convert float buffer to half-float
		D3DXFloat32To16Array((D3DXFLOAT16*)halfFloatBuffer, (float*)floatBuffer, width * 4 * height);

		// Copy half-float data back into frame
		env->BitBlt(dst, dstPitch, halfFloatBuffer, halfFloatBufferPitch, halfFloatBufferPitch, height);
	}
}

// Using Rec601 color space. Can be optimized with MMX assembly or by converting on the GPU with a shader.
void ConvertToFloat::convFloat(int y, int u, int v, unsigned char* out) {
	float r, g, b;
	if (convertYUV) {
		b = 1.164f * (y - 16) + 2.018f * (u - 128);
		g = 1.164f * (y - 16) - 0.813f * (v - 128) - 0.391f * (u - 128);
		r = 1.164f * (y - 16) + 1.596f * (v - 128);
	}
	else {
		// Pass YUV values to be converted by a shader
		r = float(y);
		g = float(u);
		b = float(v);
	}


	if (precision == 1) {
		if (convertYUV) {
			if (r > 255) r = 255;
			if (g > 255) g = 255;
			if (b > 255) b = 255;
			if (r < 0) r = 0;
			if (g < 0) g = 0;
			if (b < 0) b = 0;
		}

		unsigned char r2 = unsigned char(r);
		unsigned char g2 = unsigned char(g);
		unsigned char b2 = unsigned char(b);
		memcpy(out + 0, &b2, precision);
		memcpy(out + precision, &g2, precision);
		memcpy(out + precision * 2, &r2, precision);
		out[precision * 3] = 0;
	}
	else {
		// Texture shaders expect data between 0 and 1
		r = r / 255;
		g = g / 255;
		b = b / 255;

		// Convert the data at the position of RGB with 16-bit float values.
		//D3DXFLOAT16 r2 = D3DXFLOAT16(r);
		//D3DXFLOAT16 g2 = D3DXFLOAT16(g);
		//D3DXFLOAT16 b2 = D3DXFLOAT16(b);
		//memcpy(out + precision * 0, &r, 4);
		//memcpy(out + precision * 1, &g, 4);
		//memcpy(out + precision * 2, &b, 4);
		//memcpy(out + precision * 3, &AlphaValue, 4);
		memcpy(out, &r, 4);
		memcpy(out + 4, &g, 4);
		memcpy(out + 8, &b, 4);
		memcpy(out + 12, &AlphaValue, 4);
	}
}