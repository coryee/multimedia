#ifndef CTVIDEOUTIL_H
#define CTVIDEOUTIL_H      

#include <windows.h>
#include <d3d9.h>

// 100%
extern const D3DCOLOR RGB_WHITE;
extern const D3DCOLOR RGB_RED;
extern const D3DCOLOR RGB_YELLOW;
extern const D3DCOLOR RGB_GREEN;
extern const D3DCOLOR RGB_CYAN;
extern const D3DCOLOR RGB_BLUE;
extern const D3DCOLOR RGB_MAGENTA;
extern const D3DCOLOR RGB_BLACK;
extern const D3DCOLOR RGB_ORANGE;

// 75%
extern const D3DCOLOR RGB_WHITE_75pc;
extern const D3DCOLOR RGB_YELLOW_75pc;
extern const D3DCOLOR RGB_CYAN_75pc;
extern const D3DCOLOR RGB_GREEN_75pc;
extern const D3DCOLOR RGB_MAGENTA_75pc;
extern const D3DCOLOR RGB_RED_75pc;
extern const D3DCOLOR RGB_BLUE_75pc;

// -4% / +4%
extern const D3DCOLOR RGB_BLACK_n4pc;
extern const D3DCOLOR RGB_BLACK_p4pc;

// -Inphase / +Quadrature
extern const D3DCOLOR RGB_I;
extern const D3DCOLOR RGB_Q;

extern DWORD RGBtoYUV(const D3DCOLOR rgb);
extern VOID RGBtoYUY(const D3DCOLOR rgb, BYTE *Y, BYTE *U, BYTE *V);
extern BOOL FillNV12SurfaceWithColor(IDirect3DSurface9 *surface, int width, int height, D3DCOLOR color);
extern BOOL SaveNV12Surface2File(IDirect3DSurface9 *surface, int width, int height, char *filepath);
extern BOOL SurfaceCopy_NV12(IDirect3DSurface9 *dst_surface, IDirect3DSurface9 *src_surface, int width, int height);

#endif