#include "Convert.h"

// This page provides a good overview of YUV to RGB conversion.
// http://www.fourcc.org/fccyvrgb.php
// In this case, we want 0-255 RGB values into a 0-1 float, which is what HLSL texture shaders expect.
// There may be color matrix distortion (interpreting Rec709 data in Rec601), but converting back with
// the same formula will cancel most of the distortion. There might be color distortion on the effect of the shader.

ConvertToShader::ConvertToShader(PClip _child, IScriptEnvironment* env) :
GenericVideoFilter(_child) {
	// Convert from YV12 to float-precision RGB
	viRGB = vi;
	viRGB.pixel_type = VideoInfo::CS_BGR32;
	viRGB.width <<= 2; // RGBA is 4-byte per pixel, float-precision RGB is 16-byte per pixel (4 float). We need to increase width to store all the data.
}

ConvertToShader::~ConvertToShader() {
}

PVideoFrame __stdcall ConvertToShader::GetFrame(int n, IScriptEnvironment* env) {
	PVideoFrame src = child->GetFrame(n, env);

	// Convert from YV12 to float-precision RGB
	PVideoFrame dst = env->NewVideoFrame(viRGB);
	conv420toFloatRGB(src->GetReadPtr(PLANAR_Y), src->GetReadPtr(PLANAR_U), src->GetReadPtr(PLANAR_V), dst->GetWritePtr(), src->GetPitch(PLANAR_Y), src->GetPitch(PLANAR_U), dst->GetPitch(), vi.width, vi.height);
	return dst;
}

void ConvertToShader::conv420toFloatRGB(const byte *py, const byte *pu, const byte *pv,
	unsigned char *dst, int pitch1Y, int pitch1UV, int pitch2, int width, int height)
{
	width >>= 1;
	height >>= 1;
	int Y1, Y2, Y3, Y4, U, V;
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			Y1 = py[x << 1];
			U = pu[x];
			Y2 = py[(x << 1) + 1];
			V = pv[x];
			// pixels on 2nd row
			Y3 = py[(x << 1) + pitch1Y];
			Y4 = py[(x << 1) + pitch1Y + 1];

			convFloat(Y1, U, V, &dst[(x << 3)]);
			convFloat(Y2, U, V, &dst[(x << 3) + bytesPerPixel]);
			convFloat(Y3, U, V, &dst[(x << 3)] + pitch2);
			convFloat(Y4, U, V, &dst[(x << 3)] + pitch2 + bytesPerPixel);
		}
		py += pitch1Y;
		pu += pitch1UV;
		pv += pitch1UV;
		dst += pitch2;
	}
}

// Using Rec601 color space. Can be optimized with MMX assembly or by converting on the GPU with a shader.
void ConvertToShader::convFloat(int y, int u, int v, unsigned char* out) {
	float r, g, b;
	//r = y + (1.370705f * (v - 128));
	//g = y - (0.698001f * (v - 128)) - (0.337633f * (u - 128));
	//b = y + (1.732446f * (u - 128));
	r = y + 1.403 * u;
	g = y - 0.344 * u - 0.714 * v;
	b = y + 1.770 * u;

	if (r > 255) r = 255;
	if (g > 255) g = 255;
	if (b > 255) b = 255;
	if (r < 0) r = 0;
	if (g < 0) g = 0;
	if (b < 0) b = 0;

	// Texture shaders expect data between 0 and 1
	r = r / 255 * 1;
	g = g / 255 * 1;
	b = b / 255 * 1;

	// Empty alpha channel
	out[0] = out[1] = out[2] = out[3] = 0;
	// Store RGB
	memcpy(out + 4, &b, sizeof(float));
	memcpy(out + 8, &g, sizeof(float));
	memcpy(out + 12, &r, sizeof(float));
}


ConvertFromShader::ConvertFromShader(PClip _child, IScriptEnvironment* env) :
GenericVideoFilter(_child) {
	// Convert from float-precision RGB to YV12
	viYV = vi;
	viYV.pixel_type = VideoInfo::CS_YV12;
	viYV.width >>= 3; // Float-precision RGB is 16-byte per pixel (4 float), YV12 is 2-byte per pixel
}

ConvertFromShader::~ConvertFromShader() {
}


PVideoFrame __stdcall ConvertFromShader::GetFrame(int n, IScriptEnvironment* env) {
	PVideoFrame src = child->GetFrame(n, env);

	// Convert from float-precision RGB to YV12
	PVideoFrame dst = env->NewVideoFrame(viYV);
	convFloatRGBto420(src->GetReadPtr(PLANAR_Y), src->GetWritePtr(PLANAR_U), src->GetWritePtr(PLANAR_V), dst->GetWritePtr(), src->GetPitch(PLANAR_Y), src->GetPitch(PLANAR_U), dst->GetPitch(), viYV.width, viYV.height);
	return dst;
}

void ConvertFromShader::convFloatRGBto420(const byte *src, unsigned char *py, unsigned char *pu, unsigned char *pv,
	int pitch1, int pitch2Y, int pitch2UV, int width, int height)
{
	width >>= 1;
	height >>= 1;
	int Y1, Y2, Y3, Y4, U, V;
	float R, G, B;
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			convFloat(&src[(x << 3)], &py[x << 1], &pu[x], &pv[x]);
			convFloat(&src[(x << 3) + bytesPerPixel], &py[(x << 1) + 1], &pu[x], &pv[x]);
			convFloat(&src[(x << 3) + pitch1], &py[(x << 1) + 1], &pu[x], &pv[x]);
			convFloat(&src[(x << 3) + pitch1 + bytesPerPixel], &py[(x << 1) + pitch2Y + 1], &pu[x], &pv[x]);
		}
		src += pitch1;
		py += pitch2Y;
		pu += pitch2UV;
		pv += pitch2UV;
	}
}

// Using Rec601 color space. Can be optimized with MMX assembly or by converting on the GPU with a shader.
void ConvertFromShader::convFloat(const byte* src, byte* outY, unsigned char* outU, unsigned char* outV) {
	float r, g, b;
	memcpy(&b, &src[4], sizeof(float));
	memcpy(&g, &src[8], sizeof(float));
	memcpy(&r, &src[12], sizeof(float));

	// rgb are in the 0 to 1 range
	r = r / 1 * 255;
	b = b / 1 * 255;
	g = g / 1 * 255;

	float y, u, v;
	y = 0.299f * r + 0.587f * g + 0.114f * b;
	u = (b - y) * 0.565f;
	v = (r - y) * 0.713;

	if (y > 255) y = 255;
	if (u > 255) u = 255;
	if (v > 255) v = 255;
	if (y < 0) y = 0;
	if (u < 0) u = 0;
	if (v < 0) v = 0;

	// Store YUV
	outY[0] = unsigned char(y);
	outU[0] = unsigned char(u);
	outV[0] = unsigned char(v);
}