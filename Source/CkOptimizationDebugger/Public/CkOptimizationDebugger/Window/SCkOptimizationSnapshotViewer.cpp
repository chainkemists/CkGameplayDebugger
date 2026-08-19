#include "CkOptimizationDebugger/Window/SCkOptimizationSnapshotViewer.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "Brushes/SlateDynamicImageBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "Rendering/DrawElements.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

// Named rather than anonymous: this module compiles unity, and a same-named helper in another .cpp would collide in
// the merged translation unit.
namespace ck_optimization_snapshot_viewer
{
    auto
        Build_BrushName(
            const FGuid& InSnapshotId)
        -> FName
    {
        return FName{*ck::Format_UE(TEXT("CkOptSnapColor_{}"), InSnapshotId.ToString())};
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationSnapshotViewer::
    Construct(
        const FArguments& InArgs)
    -> void
{
    ChildSlot
    [
        SNullWidget::NullWidget
    ];
}

// --------------------------------------------------------------------------------------------------------------------

SCkOptimizationSnapshotViewer::
    ~SCkOptimizationSnapshotViewer()
{
    DoRelease_Brush();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationSnapshotViewer::
    Set_Snapshot(
        const FCkOptimizationDebugger_Snapshot* InSnapshot)
    -> void
{
    _Snapshot = InSnapshot;

    const auto NewId = _Snapshot != nullptr ? _Snapshot->Id : FGuid{};

    if (NewId == _BrushSnapshotId)
    { return; }

    // Released HERE rather than lazily at the next paint: the brush is a GPU allocation, and a viewer the reader
    // clicked away from should not still be holding one.
    DoRelease_Brush();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationSnapshotViewer::
    DoRelease_Brush()
    -> void
{
    if (_ColorBrush.IsValid())
    { _ColorBrush->ReleaseResource(); }

    _ColorBrush.Reset();
    _BrushSnapshotId = FGuid{};
    _BrushDecodeFailed = false;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationSnapshotViewer::
    DoEnsure_Brush() const
    -> void
{
    using namespace ck_optimization_snapshot_viewer;

    if (_Snapshot == nullptr || _ColorBrush.IsValid())
    { return; }

    // One attempt per snapshot. Without this a picture that cannot be decoded would be re-decoded on every paint,
    // which is a stutter the reader cannot explain and cannot escape without switching snapshots.
    if (_BrushDecodeFailed && _BrushSnapshotId == _Snapshot->Id)
    { return; }

    _BrushSnapshotId = _Snapshot->Id;
    _BrushDecodeFailed = true;

    if (_Snapshot->ColorPng.IsEmpty() || _Snapshot->Width <= 0 || _Snapshot->Height <= 0)
    { return; }

    auto Decoded = FImage{};

    if (NOT FImageUtils::DecompressImage(_Snapshot->ColorPng.GetData(), _Snapshot->ColorPng.Num(), Decoded))
    { return; }

    // `CreateWithImageData` documents its payload as BGRA, which is what the capture read back and what the PNG
    // decodes to once asked for that format.
    Decoded.ChangeFormat(ERawImageFormat::BGRA8, EGammaSpace::sRGB);

    const auto ExpectedBytes = static_cast<int64>(Decoded.SizeX) * Decoded.SizeY * 4;

    if (Decoded.RawData.Num() < ExpectedBytes)
    { return; }

    auto Bytes = TArray<uint8>{};
    Bytes.Append(Decoded.RawData.GetData(), ExpectedBytes);

    _ColorBrush = FSlateDynamicImageBrush::CreateWithImageData(
        Build_BrushName(_Snapshot->Id),
        FVector2D{static_cast<double>(Decoded.SizeX), static_cast<double>(Decoded.SizeY)},
        Bytes);

    _BrushDecodeFailed = NOT _ColorBrush.IsValid();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationSnapshotViewer::
    OnPaint(
        const FPaintArgs& InArgs,
        const FGeometry& InAllottedGeometry,
        const FSlateRect& InCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 InLayerId,
        const FWidgetStyle& InWidgetStyle,
        bool InParentEnabled) const
    -> int32
{
    using namespace ck_optimization_debugger_snapshot;

    const auto LayerId = SCompoundWidget::OnPaint(
        InArgs, InAllottedGeometry, InCullingRect, OutDrawElements, InLayerId, InWidgetStyle, InParentEnabled);

    DoEnsure_Brush();

    if (_Snapshot == nullptr || NOT _ColorBrush.IsValid())
    { return LayerId; }

    const auto Geometry = Get_LetterboxGeometry(
        InAllottedGeometry.GetLocalSize(), FIntPoint{_Snapshot->Width, _Snapshot->Height});

    if (NOT Geometry.IsSet())
    { return LayerId; }

    // The SAME geometry a click resolves through — see `Get_LetterboxGeometry`.
    FSlateDrawElement::MakeBox(
        OutDrawElements,
        LayerId + 1,
        InAllottedGeometry.ToPaintGeometry(Geometry->DrawnSize, FSlateLayoutTransform{Geometry->Offset}),
        _ColorBrush.Get(),
        ESlateDrawEffect::None,
        FLinearColor::White);

    return LayerId + 1;
}

// --------------------------------------------------------------------------------------------------------------------
