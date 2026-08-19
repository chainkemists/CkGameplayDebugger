#include "CkOptimizationDebugger/Window/SCkOptimizationSnapshotViewer.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkEditorTools/Style/CkStyle.h"

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
        Build_ViewBrushName(
            const FGuid& InSnapshotId,
            int32 InCounter)
        -> FName
    {
        return FName{*ck::Format_UE(TEXT("CkOptSnapView_{}_{}"), InSnapshotId.ToString(), InCounter)};
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_OverlayName(
            const FGuid& InSnapshotId,
            int32 InCounter)
        -> FName
    {
        return FName{*ck::Format_UE(TEXT("CkOptSnapOverlay_{}_{}"), InSnapshotId.ToString(), InCounter)};
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** BGRA, premultiplied by nothing — Slate blends it over the image. The alphas are deliberately far apart:
     *  hover and selection have to be distinguishable at a glance on top of arbitrary scene colour. */
    constexpr auto k_SelectionAlpha = 110;
    constexpr auto k_HoverAlpha = 60;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationSnapshotViewer::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _OnHoveredPrimChanged = InArgs._OnHoveredPrimChanged;
    _OnPrimClicked = InArgs._OnPrimClicked;

    ChildSlot
    [
        SNullWidget::NullWidget
    ];
}

// --------------------------------------------------------------------------------------------------------------------

SCkOptimizationSnapshotViewer::
    ~SCkOptimizationSnapshotViewer()
{
    DoRelease_Brushes();
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

    // Released HERE rather than lazily at the next paint: the brushes are GPU allocations, and a viewer the reader
    // clicked away from should not still be holding them.
    DoRelease_Brushes();

    _HoveredPrim.Reset();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationSnapshotViewer::
    Set_InteractionEnabled(
        bool InEnabled)
    -> void
{
    if (_InteractionEnabled == InEnabled)
    { return; }

    _InteractionEnabled = InEnabled;

    // Leaving selection mode drops the hover, or the last-hovered mesh would stay lit with no cursor explaining it.
    if (NOT _InteractionEnabled && _HoveredPrim.IsSet())
    {
        _HoveredPrim.Reset();
        _OnHoveredPrimChanged.ExecuteIfBound(_HoveredPrim);
    }

    _OverlayDirty = true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationSnapshotViewer::
    Set_View(
        const FCkOptimizationDebugger_SnapshotView& InView,
        const FCkOptimizationDebugger_Thresholds& InThresholds)
    -> void
{
    _Thresholds = InThresholds;

    if (_View == InView)
    { return; }

    _View = InView;

    // Released HERE rather than at the next paint: the outgoing brush is a full-size GPU allocation, and the reader
    // has already stopped looking at it.
    if (_ViewBrush.IsValid())
    {
        _ViewBrush->ReleaseResource();
        _ViewBrush.Reset();
    }

    _BrushDecodeFailed = false;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationSnapshotViewer::
    Get_ScreenCoverage() const
    -> const TArray<int32>&
{
    DoEnsure_DecodedIds();

    return _Coverage;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationSnapshotViewer::
    Invalidate_Overlay()
    -> void
{
    _OverlayDirty = true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationSnapshotViewer::
    DoRelease_Brushes()
    -> void
{
    if (_ViewBrush.IsValid())
    { _ViewBrush->ReleaseResource(); }

    if (_OverlayBrush.IsValid())
    { _OverlayBrush->ReleaseResource(); }

    _ViewBrush.Reset();
    _OverlayBrush.Reset();

    _BrushSnapshotId = FGuid{};
    _BrushView = FCkOptimizationDebugger_SnapshotView{};
    _BrushDecodeFailed = false;

    _DecodedIds.Reset();
    _Coverage.Reset();
    _DecodedIdsSnapshotId = FGuid{};

    _OverlayDirty = true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationSnapshotViewer::
    DoEnsure_ViewBrush() const
    -> void
{
    using namespace ck_optimization_debugger_snapshot_lens;
    using namespace ck_optimization_snapshot_viewer;

    if (_Snapshot == nullptr)
    { return; }

    const auto BrushMatchesWhatIsShown = _BrushSnapshotId == _Snapshot->Id && _BrushView == _View;

    if (BrushMatchesWhatIsShown && (_ViewBrush.IsValid() || _BrushDecodeFailed))
    {
        // One attempt per (snapshot, view). Without the failed half a picture that cannot be decoded would be
        // re-decoded on every paint, which is a stutter the reader cannot explain and cannot escape.
        return;
    }

    if (_ViewBrush.IsValid())
    {
        _ViewBrush->ReleaseResource();
        _ViewBrush.Reset();
    }

    _BrushSnapshotId = _Snapshot->Id;
    _BrushView = _View;
    _BrushDecodeFailed = true;

    ++_BrushCounter;

    const auto BrushName = Build_ViewBrushName(_Snapshot->Id, _BrushCounter);

    // ---- A stored PNG: the capture itself, or one of the auxiliary views ----
    const auto TryBuild_FromPng = [this, &BrushName](const TArray64<uint8>& InPng) -> void
    {
        if (InPng.IsEmpty())
        { return; }

        auto Decoded = FImage{};

        if (NOT FImageUtils::DecompressImage(InPng.GetData(), InPng.Num(), Decoded))
        { return; }

        // `CreateWithImageData` documents its payload as BGRA, which is what the capture read back and what the PNG
        // decodes to once asked for that format.
        Decoded.ChangeFormat(ERawImageFormat::BGRA8, EGammaSpace::sRGB);

        const auto ExpectedBytes = static_cast<int64>(Decoded.SizeX) * Decoded.SizeY * 4;

        if (Decoded.RawData.Num() < ExpectedBytes)
        { return; }

        auto Bytes = TArray<uint8>{};
        Bytes.Append(Decoded.RawData.GetData(), ExpectedBytes);

        _ViewBrush = FSlateDynamicImageBrush::CreateWithImageData(
            BrushName,
            FVector2D{static_cast<double>(Decoded.SizeX), static_cast<double>(Decoded.SizeY)},
            Bytes);
    };

    switch (_View.Kind)
    {
        case ECkOptimizationDebugger_SnapshotViewKind::Aux:
        {
            if (_Snapshot->AuxImages.IsValidIndex(_View.AuxIndex))
            { TryBuild_FromPng(_Snapshot->AuxImages[_View.AuxIndex].Png); }

            break;
        }
        case ECkOptimizationDebugger_SnapshotViewKind::Lens:
        {
            if (_Snapshot->Width <= 0 || _Snapshot->Height <= 0 || NOT _Snapshot->HasIdMap)
            { break; }

            DoEnsure_DecodedIds();

            // Computed, never captured: this is what lets a lens work on a snapshot loaded from a file, on a machine
            // that never saw the level it pictures.
            const auto Pixels = Build_LensPixels(*_Snapshot, _DecodedIds, _View.Lens, _Thresholds);

            if (Pixels.Num() != _Snapshot->Width * _Snapshot->Height)
            { break; }

            auto Bytes = TArray<uint8>{};
            Bytes.Append(reinterpret_cast<const uint8*>(Pixels.GetData()), Pixels.Num() * sizeof(FColor));

            _ViewBrush = FSlateDynamicImageBrush::CreateWithImageData(
                BrushName,
                FVector2D{static_cast<double>(_Snapshot->Width), static_cast<double>(_Snapshot->Height)},
                Bytes);

            break;
        }
        default:
        {
            if (_Snapshot->Width > 0 && _Snapshot->Height > 0)
            { TryBuild_FromPng(_Snapshot->ColorPng); }

            break;
        }
    }

    _BrushDecodeFailed = NOT _ViewBrush.IsValid();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationSnapshotViewer::
    DoEnsure_DecodedIds() const
    -> void
{
    using namespace ck_optimization_debugger_snapshot;

    if (_Snapshot == nullptr || NOT _Snapshot->HasIdMap)
    {
        // A colour-only snapshot has no coverage, and keeping the last one's would answer questions about a picture
        // that is no longer on screen.
        _DecodedIds.Reset();
        _Coverage.Reset();
        _DecodedIdsSnapshotId = FGuid{};

        return;
    }

    if (_DecodedIdsSnapshotId == _Snapshot->Id)
    { return; }

    // Decoded ONCE per snapshot, on the first interactive use — an ID map is one uint32 per pixel, so decoding it
    // for a snapshot the reader only glances at would be the largest allocation on this page for no reason.
    _DecodedIdsSnapshotId = _Snapshot->Id;
    _DecodedIds = Decode_IdMapRle(_Snapshot->IdMapRle);

    // Counted in the same pass the decode already pays for. Picking, every lens and the mesh list all want it, and
    // three separate walks over a million pixels would be three times the same answer.
    _Coverage = ck_optimization_debugger_snapshot_lens::Get_ScreenCoverage(_DecodedIds, _Snapshot->Prims.Num());
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationSnapshotViewer::
    DoEnsure_Overlay() const
    -> void
{
    using namespace ck_optimization_debugger_snapshot;
    using namespace ck_optimization_snapshot_viewer;

    if (NOT _OverlayDirty)
    { return; }

    _OverlayDirty = false;

    if (_OverlayBrush.IsValid())
    {
        _OverlayBrush->ReleaseResource();
        _OverlayBrush.Reset();
    }

    if (_Snapshot == nullptr || NOT _Snapshot->HasIdMap)
    { return; }

    const auto HasAnythingToPaint = _HoveredPrim.IsSet() || NOT _Snapshot->SelectedPrims.IsEmpty();

    if (NOT HasAnythingToPaint)
    { return; }

    DoEnsure_DecodedIds();

    const auto PixelCount = _Snapshot->Width * _Snapshot->Height;

    if (_DecodedIds.Num() != PixelCount)
    { return; }

    const auto SelectionTint = CkStyle::Accent();

    auto Bytes = TArray<uint8>{};
    Bytes.SetNumZeroed(PixelCount * 4);

    for (auto PixelIndex = 0; PixelIndex < PixelCount; ++PixelIndex)
    {
        const auto Id = _DecodedIds[PixelIndex];

        if (Id == k_NoPrim)
        { continue; }

        const auto PrimIndex = static_cast<int32>(Id);
        const auto IsSelected = _Snapshot->SelectedPrims.Contains(PrimIndex);
        const auto IsHovered = _HoveredPrim.IsSet() && _HoveredPrim.GetValue() == PrimIndex;

        if (NOT IsSelected && NOT IsHovered)
        { continue; }

        // Selection wins where both apply: hovering something already selected must not make it look less selected.
        const auto Alpha = IsSelected ? k_SelectionAlpha : k_HoverAlpha;
        const auto ByteIndex = PixelIndex * 4;

        Bytes[ByteIndex + 0] = static_cast<uint8>(FMath::Clamp(SelectionTint.B * 255.0f, 0.0f, 255.0f));
        Bytes[ByteIndex + 1] = static_cast<uint8>(FMath::Clamp(SelectionTint.G * 255.0f, 0.0f, 255.0f));
        Bytes[ByteIndex + 2] = static_cast<uint8>(FMath::Clamp(SelectionTint.R * 255.0f, 0.0f, 255.0f));
        Bytes[ByteIndex + 3] = static_cast<uint8>(Alpha);
    }

    ++_OverlayCounter;

    _OverlayBrush = FSlateDynamicImageBrush::CreateWithImageData(
        Build_OverlayName(_Snapshot->Id, _OverlayCounter),
        FVector2D{static_cast<double>(_Snapshot->Width), static_cast<double>(_Snapshot->Height)},
        Bytes);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationSnapshotViewer::
    TryGet_PrimAt(
        const FGeometry& InGeometry,
        const FVector2D& InScreenPosition) const
    -> TOptional<int32>
{
    using namespace ck_optimization_debugger_snapshot;

    if (_Snapshot == nullptr || NOT _Snapshot->HasIdMap)
    { return {}; }

    const auto LocalPoint = InGeometry.AbsoluteToLocal(InScreenPosition);

    const auto Pixel = Map_ViewerPointToPixel(
        InGeometry.GetLocalSize(), FIntPoint{_Snapshot->Width, _Snapshot->Height}, LocalPoint);

    if (NOT Pixel.IsSet())
    { return {}; }

    DoEnsure_DecodedIds();

    return Get_IdAt(_DecodedIds, _Snapshot->Width, _Snapshot->Height, Pixel.GetValue());
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationSnapshotViewer::
    OnMouseMove(
        const FGeometry& InGeometry,
        const FPointerEvent& InEvent)
    -> FReply
{
    if (NOT _InteractionEnabled)
    { return FReply::Unhandled(); }

    const auto Hovered = TryGet_PrimAt(InGeometry, InEvent.GetScreenSpacePosition());

    // Delta-gated: the overlay costs a pass over every pixel, and a mouse move within one mesh must not pay it.
    const auto Changed = Hovered.IsSet() != _HoveredPrim.IsSet()
        || (Hovered.IsSet() && _HoveredPrim.IsSet() && Hovered.GetValue() != _HoveredPrim.GetValue());

    if (NOT Changed)
    { return FReply::Handled(); }

    _HoveredPrim = Hovered;
    _OverlayDirty = true;

    _OnHoveredPrimChanged.ExecuteIfBound(_HoveredPrim);

    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationSnapshotViewer::
    OnMouseButtonDown(
        const FGeometry& InGeometry,
        const FPointerEvent& InEvent)
    -> FReply
{
    if (NOT _InteractionEnabled || InEvent.GetEffectingButton() != EKeys::LeftMouseButton)
    { return FReply::Unhandled(); }

    const auto Modifier = InEvent.IsShiftDown()
        ? ECkOptimizationDebugger_SnapshotClickModifier::Shift
        : InEvent.IsControlDown()
            ? ECkOptimizationDebugger_SnapshotClickModifier::Ctrl
            : ECkOptimizationDebugger_SnapshotClickModifier::None;

    // Reported, never applied here: the window owns the model, so there is one mutation path and one refresh path.
    _OnPrimClicked.ExecuteIfBound(TryGet_PrimAt(InGeometry, InEvent.GetScreenSpacePosition()), Modifier);

    return FReply::Handled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkOptimizationSnapshotViewer::
    OnMouseLeave(
        const FPointerEvent& InEvent)
    -> void
{
    SCompoundWidget::OnMouseLeave(InEvent);

    if (NOT _HoveredPrim.IsSet())
    { return; }

    _HoveredPrim.Reset();
    _OverlayDirty = true;

    _OnHoveredPrimChanged.ExecuteIfBound(_HoveredPrim);
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

    auto LayerId = SCompoundWidget::OnPaint(
        InArgs, InAllottedGeometry, InCullingRect, OutDrawElements, InLayerId, InWidgetStyle, InParentEnabled);

    DoEnsure_ViewBrush();

    if (_Snapshot == nullptr || NOT _ViewBrush.IsValid())
    { return LayerId; }

    const auto Geometry = Get_LetterboxGeometry(
        InAllottedGeometry.GetLocalSize(), FIntPoint{_Snapshot->Width, _Snapshot->Height});

    if (NOT Geometry.IsSet())
    { return LayerId; }

    // The SAME geometry a click resolves through — see `Get_LetterboxGeometry`.
    const auto PaintGeometry = InAllottedGeometry.ToPaintGeometry(
        Geometry->DrawnSize, FSlateLayoutTransform{Geometry->Offset});

    ++LayerId;

    FSlateDrawElement::MakeBox(
        OutDrawElements, LayerId, PaintGeometry, _ViewBrush.Get(), ESlateDrawEffect::None, FLinearColor::White);

    DoEnsure_Overlay();

    if (NOT _OverlayBrush.IsValid())
    { return LayerId; }

    ++LayerId;

    FSlateDrawElement::MakeBox(
        OutDrawElements, LayerId, PaintGeometry, _OverlayBrush.Get(), ESlateDrawEffect::None, FLinearColor::White);

    return LayerId;
}

// --------------------------------------------------------------------------------------------------------------------
