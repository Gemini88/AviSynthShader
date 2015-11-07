#include "ConvertFromFloat.h"

ConvertFromFloat::ConvertFromFloat(PClip _child, const char* _format, bool _convertYuv, int _precision, IScriptEnvironment* env) :
	GenericVideoFilter(_child), precision(_precision), format(_format), convertYUV(_convertYuv) {
	if (!vi.IsRGB32())
		env->ThrowError("ConvertFromFloat: Source must be float-precision RGB");
	if (strcmp(format, "YV24") != 0 && strcmp(format, "YV12") != 0 && strcmp(format, "RGB32") != 0)
		env->ThrowError("ConvertFromFloat: Destination format must be YV24, YV12 or RGB32");
	if (precision < 1 || precision > 3)
		env->ThrowError("ConvertFromFloat: Precision must be 1, 2 or 3");

	// Convert from float-precision RGB to YV24
	viYV = vi;
	if (strcmp(format, "RGB32") == 0)
		viYV.pixel_type = VideoInfo::CS_BGR32;
	else
		viYV.pixel_type = VideoInfo::CS_YV24;

	if (precision > 1) // Half-float frame has its width twice larger than normal
		viYV.width >>= 1;

	if (precision == 1)
		precisionShift = 2;
	else if (precision == 2)
		precisionShift = 3;
	else
		precisionShift = 4;

	if (precision == 3) {
		floatBufferPitch = vi.width << 3;
		floatBuffer = (unsigned char*)malloc(floatBufferPitch);
		halfFloatBufferPitch = vi.width << 2;
		halfFloatBuffer = (unsigned char*)malloc(halfFloatBufferPitch);
	}
}

ConvertFromFloat::~ConvertFromFloat() {
	if (precision == 3) {
		free(floatBuffer);
		free(halfFloatBuffer);
	}
}


PVideoFrame __stdcall ConvertFromFloat::GetFrame(int n, IScriptEnvironment* env) {
	PVideoFrame src = child->GetFrame(n, env);

	// Convert from float-precision RGB to YV24
	PVideoFrame dst = env->NewVideoFrame(viYV);
	if (viYV.pixel_type == VideoInfo::CS_BGR32)
		convFloatToRGB32(src->GetReadPtr(), dst->GetWritePtr(), src->GetPitch(), dst->GetPitch(), viYV.width, viYV.height, env);
	else
		convFloatToYV24(src->GetReadPtr(),
			dst->GetWritePtr(PLANAR_Y), dst->GetWritePtr(PLANAR_U), dst->GetWritePtr(PLANAR_V),
			src->GetPitch(), dst->GetPitch(PLANAR_Y), dst->GetPitch(PLANAR_U), viYV.width, viYV.height, env);
	return dst;
}

void ConvertFromFloat::convFloatToYV24(const byte *src, unsigned char *py, unsigned char *pu, unsigned char *pv,
	int pitch1, int pitch2Y, int pitch2UV, int width, int height, IScriptEnvironment* env)
{
	const byte* srcLoop = precision == 3 ? floatBuffer : src;
	int srcLoopPitch = precision == 3 ? floatBufferPitch : pitch1;

	unsigned char U, V;
	for (int y = 0; y < height; ++y) {
		if (precision == 3) {
			// Copy half-float data back into frame
			env->BitBlt(halfFloatBuffer, halfFloatBufferPitch, src, pitch1, width << 3, 1);

			// Convert float buffer to half-float
			DirectX::PackedVector::XMConvertHalfToFloatStream((float*)floatBuffer, 4, (DirectX::PackedVector::HALF*)halfFloatBuffer, 2, width << 2);
			src += pitch1;
		}

		for (int x = 0; x < width; ++x) {
			if (!convertYUV && precision < 3)
				convInt(srcLoop + (x << precisionShift), &py[x], &pu[x], &pv[x]);
			else
				convFloat(srcLoop + (x << precisionShift), &py[x], &pu[x], &pv[x]);
		}
		if (precision < 3)
			srcLoop += srcLoopPitch;
		py += pitch2Y;
		pu += pitch2UV;
		pv += pitch2UV;
	}
}

void ConvertFromFloat::convFloatToRGB32(const byte *src, unsigned char *dst,
	int pitchSrc, int pitchDst, int width, int height, IScriptEnvironment* env) {

	const byte* srcLoop = precision == 3 ? floatBuffer : src;
	int srcLoopPitch = precision == 3 ? floatBufferPitch : pitchSrc;

	dst += height * pitchDst;
	for (int y = 0; y < height; ++y) {
		if (precision == 3) {
			// Copy frame half-float data into buffer
			env->BitBlt(halfFloatBuffer, halfFloatBufferPitch, src, pitchSrc, width << 3, 1);

			// Convert half-float buffer to float
			DirectX::PackedVector::XMConvertHalfToFloatStream((float*)floatBuffer, 4, (DirectX::PackedVector::HALF*)halfFloatBuffer, 2, width << 2);
			src += pitchSrc;
		}

		dst -= pitchDst;
		for (int x = 0; x < width; ++x) {
			if (!convertYUV && precision < 3)
				convInt(srcLoop + (x << precisionShift), &dst[(x << 2) + 2], &dst[(x << 2) + 1], &dst[x << 2]);
			else
				convFloat(srcLoop + (x << precisionShift), &dst[(x << 2) + 2], &dst[(x << 2) + 1], &dst[x << 2]);
		}
		if (precision < 3)
			srcLoop += srcLoopPitch;
	}
}

// Using Rec601 color space. Can be optimized with MMX assembly or by converting on the GPU with a shader.
// For ConvertToFloat, it's faster to process in INT, but for ConvertFromFloat, processing with FLOAT is faster
void ConvertFromFloat::convFloat(const byte* src, unsigned char* outY, unsigned char* outU, unsigned char* outV) {
	float r, g, b;
	float y2, u2, v2;
	short y, u, v;
	if (precision == 1) {
		unsigned char r1, g1, b1;
		memcpy(&b1, src, 1);
		memcpy(&g1, src + 1, 1);
		memcpy(&r1, src + 2, 1);
		b = float(b1);
		g = float(g1);
		r = float(r1);
	}
	else if (precision == 2) {
		unsigned short rs, gs, bs;
		memcpy(&rs, src, 2);
		memcpy(&gs, src + 2, 2);
		memcpy(&bs, src + 4, 2);
		// rgb are in the 0 to UINT16_MAX range
		r = float(rs) / 255;
		g = float(gs) / 255;
		b = float(bs) / 255;
	}
	else {
		memcpy(&r, src, 4);
		memcpy(&g, src + 4, 4);
		memcpy(&b, src + 8, 4);
		// rgb are in the 0 to 1 range
		r = r * 255;
		g = g * 255;
		b = b * 255;
	}

	if (convertYUV) {
		y2 = (0.257f * r) + (0.504f * g) + (0.098f * b) + 16;
		v2 = (0.439f * r) - (0.368f * g) - (0.071f * b) + 128;
		u2 = -(0.148f * r) - (0.291f * g) + (0.439f * b) + 128;
	}
	else {
		y2 = r;
		u2 = g;
		v2 = b;
	}

	y = short(y2 + 0.5f);
	u = short(u2 + 0.5f);
	v = short(v2 + 0.5f);

	if (convertYUV || precision == 3) {
		if (y > 255) y = 255;
		if (u > 255) u = 255;
		if (v > 255) v = 255;
		if (y < 0) y = 0;
		if (u < 0) u = 0;
		if (v < 0) v = 0;
	}

	// Store YUV
	outY[0] = unsigned char(y);
	outU[0] = unsigned char(u);
	outV[0] = unsigned char(v);
}

// Shortcut to process BYTE or UINT16 values faster when not converting colors
void ConvertFromFloat::convInt(const byte* src, unsigned char* outY, unsigned char* outU, unsigned char* outV) {
	if (precision == 1) {
		outY[0] = src[0];
		outU[0] = src[1];
		outV[0] = src[2];
	}
	else { // precision == 2
		outY[0] = ((unsigned short*)src)[0] >> 8;
		outU[0] = ((unsigned short*)src)[1] >> 8;
		outV[0] = ((unsigned short*)src)[2] >> 8;
	}
}