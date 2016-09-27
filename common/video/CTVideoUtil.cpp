#include "CTVideoUtil.h"
#include <stdio.h>
//
// Studio RGB [16...235] colors.
//

// 100%
const D3DCOLOR RGB_WHITE = D3DCOLOR_XRGB(0xEB, 0xEB, 0xEB);
const D3DCOLOR RGB_RED = D3DCOLOR_XRGB(0xEB, 0x10, 0x10);
const D3DCOLOR RGB_YELLOW = D3DCOLOR_XRGB(0xEB, 0xEB, 0x10);
const D3DCOLOR RGB_GREEN = D3DCOLOR_XRGB(0x10, 0xEB, 0x10);
const D3DCOLOR RGB_CYAN = D3DCOLOR_XRGB(0x10, 0xEB, 0xEB);
const D3DCOLOR RGB_BLUE = D3DCOLOR_XRGB(0x10, 0x10, 0xEB);
const D3DCOLOR RGB_MAGENTA = D3DCOLOR_XRGB(0xEB, 0x10, 0xEB);
const D3DCOLOR RGB_BLACK = D3DCOLOR_XRGB(0x10, 0x10, 0x10);
const D3DCOLOR RGB_ORANGE = D3DCOLOR_XRGB(0xEB, 0x7E, 0x10);

// 75%
const D3DCOLOR RGB_WHITE_75pc = D3DCOLOR_XRGB(0xB4, 0xB4, 0xB4);
const D3DCOLOR RGB_YELLOW_75pc = D3DCOLOR_XRGB(0xB4, 0xB4, 0x10);
const D3DCOLOR RGB_CYAN_75pc = D3DCOLOR_XRGB(0x10, 0xB4, 0xB4);
const D3DCOLOR RGB_GREEN_75pc = D3DCOLOR_XRGB(0x10, 0xB4, 0x10);
const D3DCOLOR RGB_MAGENTA_75pc = D3DCOLOR_XRGB(0xB4, 0x10, 0xB4);
const D3DCOLOR RGB_RED_75pc = D3DCOLOR_XRGB(0xB4, 0x10, 0x10);
const D3DCOLOR RGB_BLUE_75pc = D3DCOLOR_XRGB(0x10, 0x10, 0xB4);

// -4% / +4%
const D3DCOLOR RGB_BLACK_n4pc = D3DCOLOR_XRGB(0x07, 0x07, 0x07);
const D3DCOLOR RGB_BLACK_p4pc = D3DCOLOR_XRGB(0x18, 0x18, 0x18);

// -Inphase / +Quadrature
const D3DCOLOR RGB_I = D3DCOLOR_XRGB(0x00, 0x1D, 0x42);
const D3DCOLOR RGB_Q = D3DCOLOR_XRGB(0x2C, 0x00, 0x5C);


DWORD RGBtoYUV(const D3DCOLOR rgb)
{
	const INT A = HIBYTE(HIWORD(rgb));
	const INT R = LOBYTE(HIWORD(rgb)) - 16;
	const INT G = HIBYTE(LOWORD(rgb)) - 16;
	const INT B = LOBYTE(LOWORD(rgb)) - 16;

	//
	// studio RGB [16...235] to SDTV ITU-R BT.601 YCbCr
	//
	INT Y = (77 * R + 150 * G + 29 * B + 128) / 256 + 16;
	INT U = (-44 * R - 87 * G + 131 * B + 128) / 256 + 128;
	INT V = (131 * R - 110 * G - 21 * B + 128) / 256 + 128;

	return D3DCOLOR_AYUV(A, Y, U, V);
}

VOID RGBtoYUY(const D3DCOLOR rgb, BYTE *Y, BYTE *U, BYTE *V)
{
	const D3DCOLOR yuv = RGBtoYUV(rgb);

	*Y = LOBYTE(HIWORD(yuv));
	*U = HIBYTE(LOWORD(yuv));
	*V = LOBYTE(LOWORD(yuv));
}

BOOL FillNV12SurfaceWithColor(IDirect3DSurface9 *surface, int width, int height, D3DCOLOR color)
{
	HRESULT hr;

	D3DLOCKED_RECT lr;
	hr = surface->LockRect(&lr, NULL, D3DLOCK_NOSYSLOCK);
	if (FAILED(hr))
	{
		return FALSE;
	}

	BYTE Y, U, V;
	RGBtoYUY(color, &Y, &U, &V);

	BYTE* p = (BYTE*)lr.pBits;
	// Y
	for (UINT i = 0; i < height; i++)
	{
		memset(p + (i * lr.Pitch), Y, width);
	}

	UINT uvHeight = height / 2;
	UINT uvWidth = width / 2;
	for (UINT i = height; i < height + uvHeight; i++)
	{
		BYTE *uv = p + (i * lr.Pitch);
		for (UINT j = 0; j < uvWidth; j++)
		{
			uv[0] = U;
			uv[1] = V;
			uv += 2;
		}
	}

	hr = surface->UnlockRect();
	if (FAILED(hr))
		return FALSE;

	return TRUE;
}

BOOL SaveNV12Surface2File(IDirect3DSurface9 *surface, int width, int height, char *filepath)
{
	HRESULT hr;

	D3DLOCKED_RECT lr;

	if (surface == NULL || filepath == NULL)
		return FALSE;

	hr = surface->LockRect(&lr, NULL, D3DLOCK_NOSYSLOCK);
	if (FAILED(hr))
	{
		return FALSE;
	}

	FILE *fp = fopen(filepath, "wb+");
	if (fp == NULL)
		return FALSE;

	BYTE* p = (BYTE*)lr.pBits;
	// Y
	for (UINT i = 0; i < height; i++)
	{
		if (width != fwrite(p + (i * lr.Pitch), 1, width, fp))
			return FALSE;
	}

	UINT uvHeight = height / 2;
	UINT uvWidth = width / 2;
	for (UINT i = height; i < height + uvHeight; i++)
	{
		if (width != fwrite(p + ((i - 1) * lr.Pitch), 1, width, fp))
			return FALSE;
	}

	fclose(fp);
	hr = surface->UnlockRect();
	if (FAILED(hr))
		return FALSE;

	return TRUE;
}

BOOL SurfaceCopy_NV12(IDirect3DSurface9 *dst_surface, IDirect3DSurface9 *src_surface, int width, int height)
{
	HRESULT hr;
	D3DLOCKED_RECT src_lr;
	D3DLOCKED_RECT dst_lr;

	hr = src_surface->LockRect(&src_lr, NULL, D3DLOCK_NOSYSLOCK);
	if (FAILED(hr))
	{
		return FALSE;
	}

	hr = dst_surface->LockRect(&dst_lr, NULL, D3DLOCK_NOSYSLOCK);
	if (FAILED(hr))
	{
		return FALSE;
	}

	BYTE* src_data = (BYTE*)src_lr.pBits;
	BYTE* dst_data = (BYTE*)dst_lr.pBits;
	// Y
	for (UINT i = 0; i < height; i++)
	{
		memcpy(dst_data + (i * dst_lr.Pitch), src_data + (i * src_lr.Pitch), width);
	}

	UINT uvHeight = height / 2;
	UINT uvWidth = width / 2;
	for (UINT i = height; i < height + uvHeight; i++)
	{
		memcpy(dst_data + (i * dst_lr.Pitch), src_data + (i * src_lr.Pitch), width);
	}

	hr = src_surface->UnlockRect();
	if (FAILED(hr))
	{
		return FALSE;
	}

	hr = dst_surface->UnlockRect();
	if (FAILED(hr))
	{
		return FALSE;
	}

	return TRUE;
}