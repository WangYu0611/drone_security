#pragma once
#include "Layout/Geometry.h"
#include "Types/PaintArgs.h"

// OnPaint receives window-space geometry; cached UMG geometry and pointer
// coordinates are in desktop space. Keep hit checks and drawing in their own
// spaces so moving/resizing a client window cannot hide its map overlays.
namespace MapPaint {
inline FGeometry DesktopGeometry(const FGeometry& Paint,const FPaintArgs& Args) {
    FGeometry Desktop=Paint;
    Desktop.AppendTransform(FSlateLayoutTransform(FVector2D(Args.GetWindowToDesktopTransform())));
    return Desktop;
}
inline FGeometry ClipGeometry(const FGeometry& Paint,const FGeometry& Desktop,const FGeometry& Frame) {
    const FVector2D Min=Desktop.AbsoluteToLocal(Frame.GetAbsolutePosition());
    const FVector2D Max=Desktop.AbsoluteToLocal(Frame.LocalToAbsolute(Frame.GetLocalSize()));
    return Paint.MakeChild(Max-Min,FSlateLayoutTransform(Min));
}
}
