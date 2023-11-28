#pragma once

#include <d2d1.h>
#include <d2d1_1.h>

#ifdef __cplusplus
extern "C"
{
#endif

D2D1_SIZE_F dxID2D1Bitmap_GetSize(ID2D1Bitmap *pID2DBitmap);

HRESULT dxID2D1Factory_CreateHwndRenderTarget(
    ID2D1Factory* _this,
    const D2D1_RENDER_TARGET_PROPERTIES* renderTargetProperties,
    const D2D1_HWND_RENDER_TARGET_PROPERTIES* hwndRenderTargetProperties,
    ID2D1HwndRenderTarget** hwndRenderTarget
);

D2D1_SIZE_F dxID2D1HwndRenderTarget_GetSize(ID2D1HwndRenderTarget* _this);

HRESULT dxID2D1HwndRenderTarget_Resize(ID2D1HwndRenderTarget* _this, D2D1_SIZE_U* pixelSize);

void dxID2D1RenderTarget_BeginDraw(ID2D1RenderTarget *_this);

void dxID2D1RenderTarget_Clear(ID2D1RenderTarget* _this, D2D1_COLOR_F* clearColor);

HRESULT dxID2D1RenderTarget_CreateBitmapFromWicBitmap(
    ID2D1RenderTarget* _this,
    IWICBitmapSource* wicBitmapSource,
    const D2D1_BITMAP_PROPERTIES* bitmapProperties,
    ID2D1Bitmap** bitmap
);

void dxID2D1RenderTarget_DrawBitmap(
    ID2D1RenderTarget* _this,
    ID2D1Bitmap* bitmap,
    const D2D1_RECT_F* destinationRectangle,
    FLOAT opacity,
    D2D1_BITMAP_INTERPOLATION_MODE interpolationMode,
    const D2D1_RECT_F* sourceRectangle
);

HRESULT dxID2D1RenderTarget_EndDraw(ID2D1RenderTarget* _this, D2D1_TAG* tag1, D2D1_TAG* tag2);

void dxID2D1RenderTarget_SetTransform(ID2D1RenderTarget *_this, D2D_MATRIX_3X2_F *transform);

#ifdef __cplusplus
}
#endif
