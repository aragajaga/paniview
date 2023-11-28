#include "d2dwrapper.h"

extern "C"
{

D2D1_SIZE_F dxID2D1Bitmap_GetSize(ID2D1Bitmap *_this)
{
    return _this->GetSize();
}

HRESULT dxID2D1Factory_CreateHwndRenderTarget(
    ID2D1Factory* _this,
    const D2D1_RENDER_TARGET_PROPERTIES* renderTargetProperties,
    const D2D1_HWND_RENDER_TARGET_PROPERTIES* hwndRenderTargetProperties,
    ID2D1HwndRenderTarget** hwndRenderTarget
)
{
    return _this->CreateHwndRenderTarget(renderTargetProperties, hwndRenderTargetProperties, hwndRenderTarget);
}

D2D1_SIZE_F dxID2D1HwndRenderTarget_GetSize(ID2D1HwndRenderTarget* _this)
{
    return _this->GetSize();
}

HRESULT dxID2D1HwndRenderTarget_Resize(ID2D1HwndRenderTarget* _this, D2D1_SIZE_U* pixelSize)
{
    return _this->Resize(pixelSize);
}

void dxID2D1RenderTarget_BeginDraw(ID2D1RenderTarget *_this)
{
    _this->BeginDraw();
}

void dxID2D1RenderTarget_Clear(ID2D1RenderTarget* _this, D2D1_COLOR_F* clearColor)
{
    _this->Clear(clearColor);
}

HRESULT dxID2D1RenderTarget_CreateBitmapFromWicBitmap(
    ID2D1RenderTarget* _this,
    IWICBitmapSource* wicBitmapSource,
    const D2D1_BITMAP_PROPERTIES* bitmapProperties,
    ID2D1Bitmap** bitmap
)
{
    return _this->CreateBitmapFromWicBitmap(wicBitmapSource, bitmapProperties, bitmap);
}

void dxID2D1RenderTarget_DrawBitmap(
    ID2D1RenderTarget* _this,
    ID2D1Bitmap* bitmap,
    const D2D1_RECT_F* destinationRectangle,
    FLOAT opacity,
    D2D1_BITMAP_INTERPOLATION_MODE interpolationMode,
    const D2D1_RECT_F* sourceRectangle
)
{
    _this->DrawBitmap(bitmap, destinationRectangle, opacity, interpolationMode, sourceRectangle);
}

HRESULT dxID2D1RenderTarget_EndDraw(ID2D1RenderTarget* _this, D2D1_TAG* tag1, D2D1_TAG* tag2)
{
    return _this->EndDraw(tag1, tag2);
}

void dxID2D1RenderTarget_SetTransform(ID2D1RenderTarget *_this, D2D_MATRIX_3X2_F *transform)
{
    _this->SetTransform(transform);
}

}
