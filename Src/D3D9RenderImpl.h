#pragma once

#define D3D_DEBUG_INFO
#include "d3d9.h"
#include "atlbase.h"
#include "D3D9Macros.h"
#include "avisynth.h"
#include <windows.h>

struct InputTexture {
	CComPtr<IDirect3DSurface9> Memory;
	CComPtr<IDirect3DTexture9> Texture;
	CComPtr<IDirect3DSurface9> Surface;
};

class D3D9RenderImpl // : public IRenderable
{
public:
	D3D9RenderImpl();
	~D3D9RenderImpl();

	HRESULT Initialize(HWND hDisplayWindow, int width, int height, int precision);
	HRESULT CreateInputTexture(int index, int width, int height);
	HRESULT CopyToBuffer(const byte* src, int srcPitch, int index, int width, int height, IScriptEnvironment* env);
	HRESULT ProcessFrame(byte* dst, int dstPitch, int width, int height, IScriptEnvironment* env);

	HRESULT SetPixelShader(LPCSTR pPixelShaderName, LPCSTR entryPoint, LPCSTR shaderModel, LPSTR* ppError);
	HRESULT SetPixelShader(DWORD* buffer);
	HRESULT SetPixelShaderIntConstant(LPCSTR name, int value);
	HRESULT SetPixelShaderFloatConstant(LPCSTR name, float value);
	HRESULT SetPixelShaderBoolConstant(LPCSTR name, bool value);
	HRESULT SetPixelShaderConstant(LPCSTR name, LPVOID value, UINT size);
	HRESULT SetPixelShaderMatrix(D3DXMATRIX* matrix, LPCSTR name);
	HRESULT SetPixelShaderVector(LPCSTR name, D3DXVECTOR4* vector);

private:
	HRESULT SetupMatrices(int width, int height);
	HRESULT ResetDevice();
	HRESULT CreateScene();
	HRESULT CopyFromRenderTarget(byte* dst, int dstPitch, int width, int height, IScriptEnvironment* env);
	HRESULT CreateRenderTarget(int width, int height);
	HRESULT Present();
	HRESULT GetPresentParams(D3DPRESENT_PARAMETERS* params);
	HRESULT CheckFormatConversion(D3DFORMAT format);

	CComPtr<IDirect3D9Ex>           m_pD3D9;
	CComPtr<IDirect3DDevice9Ex>     m_pDevice;
	CComPtr<IDirect3DTexture9>      m_pRenderTarget;
	CComPtr<IDirect3DSurface9>      m_pRenderTargetSurface;
	CComPtr<IDirect3DSurface9>		m_pReadSurfaceGpu;
	CComPtr<IDirect3DSurface9>		m_pReadSurfaceCpu;
	CComPtr<IDirect3DVertexBuffer9> m_pVertexBuffer;
	CComPtr<ID3DXConstantTable>     m_pPixelConstantTable;
	CComPtr<IDirect3DPixelShader9>  m_pPixelShader;
	static const int maxTextures = 5;
	InputTexture					m_InputTextures[maxTextures];

	int m_precision;
	D3DFORMAT m_format;
	D3DDISPLAYMODE m_displayMode;
	HWND m_hDisplayWindow;
	D3DPRESENT_PARAMETERS m_presentParams;
	IScriptEnvironment* m_env;
};