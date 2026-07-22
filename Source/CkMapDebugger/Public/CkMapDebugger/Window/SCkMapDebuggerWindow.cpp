#include "CkMapDebugger/Window/SCkMapDebuggerWindow.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Search/SCkDebug_DualSearchBar.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkDebuggerCommon/Window/SCkDebugger_RefreshControls.h"

#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"

#include "CkEcsExt/Transform/CkTransform_Utils.h"

#include "CkEntityTag/CkEntityTag_Utils.h"
#include "CkLabel/CkLabel_Utils.h"
#include "CkVisibleRange/CkVisibleRange_Utils.h"

#include "CkCompass/CkCompass_Fragment.h"
#include "CkCompass/CkCompass_Utils.h"
#include "CkMinimap/CkFogOfWar_Fragment.h"
#include "CkMinimap/CkFogOfWar_Utils.h"
#include "CkMinimap/CkMinimap_Fragment.h"
#include "CkMinimap/CkMinimap_Utils.h"
#include "CkPoi/CkPoi_Fragment.h"
#include "CkPoi/CkPoi_Utils.h"

#include "Engine/Engine.h"
#include "Engine/World.h"

#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

// --------------------------------------------------------------------------------------------------------------------
// Local style + helpers (module-unique namespace name — unity builds concatenate TUs).
// --------------------------------------------------------------------------------------------------------------------

namespace ck_map_debugger
{
    static constexpr auto Pad_S = 4.0f;
    static constexpr auto Pad_M = 8.0f;

    static const auto Bg_Dark    = FLinearColor(0.01f, 0.01f, 0.01f);
    static const auto Bg_Medium  = FLinearColor(0.025f, 0.025f, 0.025f);
    static const auto Bg_Canvas  = FLinearColor(0.008f, 0.008f, 0.010f);

    static const auto Text_Primary   = FLinearColor(0.85f, 0.85f, 0.85f);
    static const auto Text_Secondary = FLinearColor(0.6f, 0.6f, 0.6f);
    static const auto Text_Highlight = FLinearColor(0.95f, 0.95f, 0.95f);
    static const auto Text_Muted     = FLinearColor(0.35f, 0.35f, 0.35f);

    static const auto Color_Disabled  = FLinearColor(0.75f, 0.33f, 0.33f);
    static const auto Color_Fog       = FLinearColor(0.61f, 0.50f, 0.83f, 0.16f);
    static const auto Color_Bounds    = FLinearColor(1.0f, 1.0f, 1.0f, 0.25f);
    static const auto Color_Minimap   = FLinearColor(0.35f, 0.56f, 0.83f);
    static const auto Color_Compass   = FLinearColor(0.31f, 0.70f, 0.66f);
    static const auto Color_Observer  = FLinearColor(0.91f, 0.91f, 0.93f);

    static auto Normal(int32 InSize = 9) -> FSlateFontInfo { return FCoreStyle::GetDefaultFontStyle("Regular", InSize); }
    static auto Bold(int32 InSize = 9)   -> FSlateFontInfo { return FCoreStyle::GetDefaultFontStyle("Bold", InSize); }
    static auto Mono(int32 InSize = 9)   -> FSlateFontInfo { return FCoreStyle::GetDefaultFontStyle("Mono", InSize); }

    static auto CategoryColor(const FGameplayTag& InCategory) -> FLinearColor
    {
        const auto Hash = GetTypeHash(InCategory.GetTagName());
        return FLinearColor::MakeFromHSV8(static_cast<uint8>(Hash), 170, 235);
    }

    static auto FindGameWorld() -> UWorld*
    {
        if (NOT GEngine)
        { return nullptr; }

        for (const auto& Context : GEngine->GetWorldContexts())
        {
            auto* World = Context.World();

            if (ck::Is_NOT_Valid(World))
            { continue; }

            if (World->WorldType != EWorldType::Game && World->WorldType != EWorldType::PIE)
            { continue; }

            if (NOT World->HasBegunPlay())
            { continue; }

            return World;
        }

        return nullptr;
    }

    static auto NetModeLabel(const UWorld* InWorld) -> FString
    {
        switch (InWorld->GetNetMode())
        {
            case NM_DedicatedServer: return TEXT("Dedicated Server");
            case NM_ListenServer:    return TEXT("Listen Server");
            case NM_Client:          return TEXT("Client");
            case NM_Standalone:      return TEXT("Standalone");
            default:                 return TEXT("Unknown");
        }
    }

    // World yaw degrees (0 = North = +X, 90 = East = +Y) -> world-XY unit direction
    static auto YawToWorldDir(float InYawDegrees) -> FVector2D
    {
        const auto Radians = FMath::DegreesToRadians(InYawDegrees);
        return FVector2D{FMath::Cos(Radians), FMath::Sin(Radians)};
    }

    // Skip painting individual cells past this budget — a pathological grid should not tank the editor
    static constexpr auto MaxPaintedFogCells = 65536;
}

// ====================================================================================================================
// SCkMapDebug_Canvas
// ====================================================================================================================

auto
    SCkMapDebug_Canvas::
    Construct(
        const FArguments& InArgs)
    -> void
{
}

auto
    SCkMapDebug_Canvas::
    ComputeDesiredSize(
        float InLayoutScaleMultiplier) const
    -> FVector2D
{
    return FVector2D{480.0, 420.0};
}

auto
    SCkMapDebug_Canvas::
    DoCompute_WorldWindow(
        FVector2D& OutCenter,
        double& OutHalfExtent) const
    -> bool
{
    if (NOT _Snapshot.IsValid() || NOT _Snapshot->HasWorld)
    { return false; }

    auto Min = FVector2D{TNumericLimits<double>::Max(), TNumericLimits<double>::Max()};
    auto Max = FVector2D{TNumericLimits<double>::Lowest(), TNumericLimits<double>::Lowest()};
    auto Any = false;

    const auto Absorb = [&](const FVector2D& InPoint) -> void
    {
        Min.X = FMath::Min(Min.X, InPoint.X); Min.Y = FMath::Min(Min.Y, InPoint.Y);
        Max.X = FMath::Max(Max.X, InPoint.X); Max.Y = FMath::Max(Max.Y, InPoint.Y);
        Any = true;
    };

    for (const auto& Fog : _Snapshot->Fogs)
    {
        Absorb(Fog.Bounds.Get_Center() - Fog.Bounds.Get_HalfExtents());
        Absorb(Fog.Bounds.Get_Center() + Fog.Bounds.Get_HalfExtents());
    }

    for (const auto& Minimap : _Snapshot->Minimaps)
    {
        if (Minimap.ProjectionMode == ECk_Minimap_ProjectionMode::FixedBounds && ck::IsValid(Minimap.FixedBounds))
        {
            Absorb(Minimap.FixedBounds.Get_Center() - Minimap.FixedBounds.Get_HalfExtents());
            Absorb(Minimap.FixedBounds.Get_Center() + Minimap.FixedBounds.Get_HalfExtents());
        }

        Absorb(FVector2D{Minimap.ViewOrigin.X, Minimap.ViewOrigin.Y});
    }

    for (const auto& Compass : _Snapshot->Compasses)
    {
        if (Compass.HasObserverLocation)
        { Absorb(FVector2D{Compass.ObserverLocation.X, Compass.ObserverLocation.Y}); }
    }

    for (const auto& Poi : _Snapshot->Pois)
    { Absorb(FVector2D{Poi.WorldPos.X, Poi.WorldPos.Y}); }

    if (NOT Any)
    { return false; }

    OutCenter = (Min + Max) * 0.5;
    OutHalfExtent = FMath::Max3(Max.X - Min.X, Max.Y - Min.Y, 2000.0) * 0.5;
    return true;
}

auto
    SCkMapDebug_Canvas::
    DoWorldToPanel(
        const FVector2D& InWorldXY,
        const FVector2D& InPanelSize,
        const FVector2D& InWorldCenter,
        double InScale) const
    -> FVector2D
{
    // North-up: panel right = world +Y, panel up = world +X
    return FVector2D
    {
        InPanelSize.X * 0.5 + (InWorldXY.Y - InWorldCenter.Y) * InScale + _PanOffset.X,
        InPanelSize.Y * 0.5 - (InWorldXY.X - InWorldCenter.X) * InScale + _PanOffset.Y
    };
}

auto
    SCkMapDebug_Canvas::
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
    const auto* WhiteBrush = FAppStyle::GetBrush("WhiteBrush");
    const auto PanelSize = InAllottedGeometry.GetLocalSize();

    // Background
    FSlateDrawElement::MakeBox(OutDrawElements, InLayerId,
        InAllottedGeometry.ToPaintGeometry(FVector2f(PanelSize), FSlateLayoutTransform(FVector2f::ZeroVector)),
        WhiteBrush, ESlateDrawEffect::None, ck_map_debugger::Bg_Canvas);

    auto WorldCenter = FVector2D{};
    auto WorldHalfExtent = 0.0;

    if (NOT DoCompute_WorldWindow(WorldCenter, WorldHalfExtent))
    { return InLayerId + 1; }

    const auto Scale = 0.45 * FMath::Min(PanelSize.X, PanelSize.Y) / WorldHalfExtent * static_cast<double>(_Zoom);
    const auto ToPanel = [&](const FVector2D& InWorldXY) { return DoWorldToPanel(InWorldXY, PanelSize, WorldCenter, Scale); };

    const auto DrawBoxAt = [&](const FVector2D& InPanelPos, const FVector2D& InSize, const FLinearColor& InColor, int32 InLayer) -> void
    {
        FSlateDrawElement::MakeBox(OutDrawElements, InLayer,
            InAllottedGeometry.ToPaintGeometry(FVector2f(InSize), FSlateLayoutTransform(FVector2f(InPanelPos - InSize * 0.5))),
            WhiteBrush, ESlateDrawEffect::None, InColor);
    };

    const auto DrawRectOutline = [&](const TArray<FVector2D>& InCorners, const FLinearColor& InColor, int32 InLayer, float InThickness) -> void
    {
        auto Points = InCorners;

        // Close the loop via a COPY — Points.Add(Points[0]) hands Add a reference into the array itself,
        // which dangles when the Add reallocates (TArray::CheckAddress assert)
        if (Points.Num() > 0)
        {
            const auto FirstCorner = Points[0];
            Points.Add(FirstCorner);
        }
        FSlateDrawElement::MakeLines(OutDrawElements, InLayer, InAllottedGeometry.ToPaintGeometry(), Points,
            ESlateDrawEffect::None, InColor, true, InThickness);
    };

    const auto FogLayer = InLayerId + 1;
    const auto BoundsLayer = InLayerId + 2;
    const auto FrameLayer = InLayerId + 3;
    const auto PoiLayer = InLayerId + 4;
    const auto ObserverLayer = InLayerId + 5;

    // ---- Fog grids: shade UNEXPLORED cells; outline the bounds ----

    for (const auto& Fog : _Snapshot->Fogs)
    {
        const auto BoundsMin = Fog.Bounds.Get_Center() - Fog.Bounds.Get_HalfExtents();
        const auto BoundsMax = Fog.Bounds.Get_Center() + Fog.Bounds.Get_HalfExtents();

        const auto TotalCells = Fog.CellCounts.X * Fog.CellCounts.Y;

        if (TotalCells > 0 && TotalCells <= ck_map_debugger::MaxPaintedFogCells && Fog.Explored.Num() == TotalCells)
        {
            for (auto CellY = 0; CellY < Fog.CellCounts.Y; ++CellY)
            {
                for (auto CellX = 0; CellX < Fog.CellCounts.X; ++CellX)
                {
                    const auto CellIndex = CellY * Fog.CellCounts.X + CellX;

                    if (Fog.Explored[CellIndex])
                    { continue; }

                    const auto CellMin = FVector2D{BoundsMin.X + CellX * Fog.CellSize, BoundsMin.Y + CellY * Fog.CellSize};
                    const auto CellMax = FVector2D{
                        FMath::Min(CellMin.X + Fog.CellSize, BoundsMax.X),
                        FMath::Min(CellMin.Y + Fog.CellSize, BoundsMax.Y)};

                    // Cell corners in world -> panel (axis-aligned in both spaces bar the north-up swap)
                    const auto A = ToPanel(CellMin);
                    const auto B = ToPanel(CellMax);
                    const auto TopLeft = FVector2D{FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y)};
                    const auto Size = FVector2D{FMath::Abs(B.X - A.X), FMath::Abs(B.Y - A.Y)};

                    if (TopLeft.X > PanelSize.X || TopLeft.Y > PanelSize.Y || TopLeft.X + Size.X < 0.0 || TopLeft.Y + Size.Y < 0.0)
                    { continue; }

                    FSlateDrawElement::MakeBox(OutDrawElements, FogLayer,
                        InAllottedGeometry.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(TopLeft))),
                        WhiteBrush, ESlateDrawEffect::None, ck_map_debugger::Color_Fog);
                }
            }
        }

        DrawRectOutline(
            {ToPanel({BoundsMin.X, BoundsMin.Y}), ToPanel({BoundsMin.X, BoundsMax.Y}),
             ToPanel({BoundsMax.X, BoundsMax.Y}), ToPanel({BoundsMax.X, BoundsMin.Y})},
            ck_map_debugger::Color_Bounds, BoundsLayer, 1.0f);
    }

    // ---- Minimap view frames ----

    for (const auto& Minimap : _Snapshot->Minimaps)
    {
        if (Minimap.ProjectionMode == ECk_Minimap_ProjectionMode::FixedBounds)
        {
            if (ck::Is_NOT_Valid(Minimap.FixedBounds))
            { continue; }

            const auto BoundsMin = Minimap.FixedBounds.Get_Center() - Minimap.FixedBounds.Get_HalfExtents();
            const auto BoundsMax = Minimap.FixedBounds.Get_Center() + Minimap.FixedBounds.Get_HalfExtents();

            DrawRectOutline(
                {ToPanel({BoundsMin.X, BoundsMin.Y}), ToPanel({BoundsMin.X, BoundsMax.Y}),
                 ToPanel({BoundsMax.X, BoundsMax.Y}), ToPanel({BoundsMax.X, BoundsMin.Y})},
                ck_map_debugger::Color_Minimap, FrameLayer, 1.2f);

            continue;
        }

        // Observer-centric: a ViewExtent-sized square around the view origin, rotated by the view yaw when
        // the minimap rotates with its observer (world-space corners; the canvas transform handles north-up)
        const auto Origin = FVector2D{Minimap.ViewOrigin.X, Minimap.ViewOrigin.Y};
        const auto Yaw = Minimap.RotationMode == ECk_Minimap_RotationMode::RotateWithObserver ? Minimap.ViewYawDegrees : 0.0f;
        const auto Forward = ck_map_debugger::YawToWorldDir(Yaw) * Minimap.ViewExtent;
        const auto Right = ck_map_debugger::YawToWorldDir(Yaw + 90.0f) * Minimap.ViewExtent;

        DrawRectOutline(
            {ToPanel(Origin + Forward - Right), ToPanel(Origin + Forward + Right),
             ToPanel(Origin - Forward + Right), ToPanel(Origin - Forward - Right)},
            ck_map_debugger::Color_Minimap, FrameLayer, 1.2f);

        DrawBoxAt(ToPanel(Origin), FVector2D{6.0, 6.0}, ck_map_debugger::Color_Observer, ObserverLayer);
    }

    // ---- Compass observers + heading rays ----

    for (const auto& Compass : _Snapshot->Compasses)
    {
        if (NOT Compass.HasObserverLocation)
        { continue; }

        const auto Origin = FVector2D{Compass.ObserverLocation.X, Compass.ObserverLocation.Y};
        const auto PanelOrigin = ToPanel(Origin);
        const auto WorldDir = ck_map_debugger::YawToWorldDir(Compass.HeadingDegrees);
        const auto PanelDir = FVector2D{WorldDir.Y, -WorldDir.X};

        FSlateDrawElement::MakeLines(OutDrawElements, ObserverLayer, InAllottedGeometry.ToPaintGeometry(),
            {PanelOrigin, PanelOrigin + PanelDir * 46.0}, ESlateDrawEffect::None, ck_map_debugger::Color_Compass, true, 1.5f);

        DrawBoxAt(PanelOrigin, FVector2D{6.0, 6.0}, ck_map_debugger::Color_Observer, ObserverLayer);
    }

    // ---- POIs ----

    for (const auto& Poi : _Snapshot->Pois)
    {
        const auto PanelPos = ToPanel(FVector2D{Poi.WorldPos.X, Poi.WorldPos.Y});

        if (PanelPos.X < -20.0 || PanelPos.Y < -20.0 || PanelPos.X > PanelSize.X + 20.0 || PanelPos.Y > PanelSize.Y + 20.0)
        { continue; }

        const auto IsSelected = ck::IsValid(_SelectedPoi) && Poi.Handle == _SelectedPoi;

        if (IsSelected)
        { DrawBoxAt(PanelPos, FVector2D{13.0, 13.0}, ck_map_debugger::Color_Observer, PoiLayer); }

        auto Color = ck_map_debugger::CategoryColor(Poi.Category);
        Color.A = Poi.Enabled ? 1.0f : 0.35f;

        DrawBoxAt(PanelPos, FVector2D{8.0, 8.0}, Color, PoiLayer);
    }

    return ObserverLayer + 1;
}

auto
    SCkMapDebug_Canvas::
    OnMouseButtonDown(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent)
    -> FReply
{
    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        _IsPanning = true;
        _LastMousePos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
        return FReply::Handled().CaptureMouse(SharedThis(this));
    }

    if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
    { return FReply::Unhandled(); }

    if (NOT _Snapshot.IsValid() || NOT _OnPoiPicked)
    { return FReply::Unhandled(); }

    auto WorldCenter = FVector2D{};
    auto WorldHalfExtent = 0.0;

    if (NOT DoCompute_WorldWindow(WorldCenter, WorldHalfExtent))
    { return FReply::Unhandled(); }

    const auto PanelSize = InGeometry.GetLocalSize();
    const auto Scale = 0.45 * FMath::Min(PanelSize.X, PanelSize.Y) / WorldHalfExtent * static_cast<double>(_Zoom);
    const auto MousePos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

    auto BestPoi = FCk_Handle{};
    auto BestDistSq = 14.0 * 14.0;

    for (const auto& Poi : _Snapshot->Pois)
    {
        const auto PanelPos = DoWorldToPanel(FVector2D{Poi.WorldPos.X, Poi.WorldPos.Y}, PanelSize, WorldCenter, Scale);
        const auto DistSq = FVector2D::DistSquared(PanelPos, MousePos);

        if (DistSq < BestDistSq)
        {
            BestDistSq = DistSq;
            BestPoi = Poi.Handle;
        }
    }

    if (ck::IsValid(BestPoi))
    {
        _OnPoiPicked(BestPoi);
        return FReply::Handled();
    }

    return FReply::Unhandled();
}

auto
    SCkMapDebug_Canvas::
    OnMouseButtonUp(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent)
    -> FReply
{
    if (_IsPanning && InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        _IsPanning = false;
        return FReply::Handled().ReleaseMouseCapture();
    }

    return FReply::Unhandled();
}

auto
    SCkMapDebug_Canvas::
    OnMouseMove(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent)
    -> FReply
{
    if (NOT _IsPanning)
    { return FReply::Unhandled(); }

    const auto MousePos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
    _PanOffset += MousePos - _LastMousePos;
    _LastMousePos = MousePos;
    return FReply::Handled();
}

auto
    SCkMapDebug_Canvas::
    OnMouseWheel(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent)
    -> FReply
{
    const auto ZoomFactor = InMouseEvent.GetWheelDelta() > 0.0f ? 1.15f : 1.0f / 1.15f;
    _Zoom = FMath::Clamp(_Zoom * ZoomFactor, 0.2f, 20.0f);
    return FReply::Handled();
}

// ====================================================================================================================
// SCkMapDebuggerWindow
// ====================================================================================================================

const FName SCkMapDebuggerWindow::WindowId = FName(TEXT("MapDebugger"));

SCkMapDebuggerWindow::~SCkMapDebuggerWindow()
{
#if WITH_EDITOR
    FEditorDelegates::EndPIE.Remove(_EndPieHandle);
#endif
}

auto
    SCkMapDebuggerWindow::
    Construct(
        const FArguments& InArgs)
    -> void
{
    Register_WithGate();

    _Snapshot = MakeShared<FCkMapDebug_Snapshot>();

#if WITH_EDITOR
    _EndPieHandle = FEditorDelegates::EndPIE.AddLambda([WeakThis = TWeakPtr<SCkMapDebuggerWindow>(SharedThis(this))](const bool)
    {
        if (const auto This = WeakThis.Pin())
        { This->DoClearOnWorldGone(); }
    });
#endif

    // ---- Toolbar ----

    auto Toolbar =
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
        .BorderBackgroundColor(ck_map_debugger::Bg_Dark)
        .Padding(FMargin(ck_map_debugger::Pad_M, ck_map_debugger::Pad_S))
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Font(ck_map_debugger::Bold(11))
                    .ColorAndOpacity(ck_map_debugger::Text_Highlight)
                    .Text(FText::FromString(TEXT("Map Stack")))
                ]

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(ck_map_debugger::Pad_M, 0.0f)
                [
                    SNew(STextBlock)
                    .Font(ck_map_debugger::Normal(10))
                    .ColorAndOpacity(ck_map_debugger::Text_Secondary)
                    .Text_Lambda([this]() -> FText
                    {
                        return _Snapshot->HasWorld
                            ? FText::FromString(ck::Format_UE(TEXT("World: {}"), _Snapshot->WorldLabel))
                            : FText::FromString(TEXT("No active world. Start PIE to see the map stack."));
                    })
                ]

            + SHorizontalBox::Slot().FillWidth(1.0f)

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(SCkDebugger_RefreshControls)
                        .WindowId(SCkMapDebuggerWindow::WindowId)
                ]
        ];

    // ---- Left rail: search + POI list ----

    auto LeftRail =
        SNew(SVerticalBox)

        + SVerticalBox::Slot().AutoHeight().Padding(ck_map_debugger::Pad_S)
            [
                SNew(SCkDebug_DualSearchBar)
                .FilterHintText(FText::FromString(TEXT("Filter POIs…")))
                .HighlightHintText(FText::FromString(TEXT("Highlight…")))
                .OnFilterTextChanged_Lambda([this](const FString& InText)
                {
                    if (_FilterString == InText) { return; }
                    _FilterString = InText;
                    DoRefreshRowItems();
                })
                .OnHighlightTextChanged_Lambda([this](const FString& InText)
                {
                    if (_HighlightString == InText) { return; }
                    _HighlightString = InText;
                    DoRefreshRowItems();
                })
            ]

        + SVerticalBox::Slot().FillHeight(1.0f)
            [
                SAssignNew(_PoiList, SListView<TSharedPtr<FCkMapDebug_PoiRow>>)
                .ListItemsSource(&_VisibleRows)
                .OnGenerateRow(this, &SCkMapDebuggerWindow::OnGenerateRow)
                .OnSelectionChanged(this, &SCkMapDebuggerWindow::OnRowSelectionChanged)
                .SelectionMode(ESelectionMode::Single)
            ]

        + SVerticalBox::Slot().AutoHeight().Padding(ck_map_debugger::Pad_S)
            [
                SNew(STextBlock)
                .Font(ck_map_debugger::Normal(8))
                .ColorAndOpacity(ck_map_debugger::Text_Muted)
                .Text_Lambda([this]() -> FText
                {
                    return FText::FromString(ck::Format_UE(TEXT("{} / {} POIs shown"),
                        _VisibleRows.Num(), _AllRows.Num()));
                })
            ];

    // ---- Center: canvas ----

    SAssignNew(_Canvas, SCkMapDebug_Canvas);
    _Canvas->Set_OnPoiPicked([this](const FCk_Handle& InPoi)
    {
        _SelectedPoi = InPoi;
        _Canvas->Set_SelectedPoi(InPoi);

        const auto FoundRow = _VisibleRows.FindByPredicate([&](const TSharedPtr<FCkMapDebug_PoiRow>& InRow)
        { return InRow.IsValid() && InRow->Handle == InPoi; });

        if (FoundRow != nullptr && _PoiList.IsValid())
        {
            const auto Current = _PoiList->GetSelectedItems();
            const auto AlreadySelected = Current.Num() == 1 && Current[0] == *FoundRow;

            if (NOT AlreadySelected)
            { _PoiList->SetItemSelection(*FoundRow, true, ESelectInfo::Direct); }
        }
    });

    // ---- Right rail: selected POI detail ----

    auto RightRail =
        SNew(SScrollBox)

        + SScrollBox::Slot().Padding(ck_map_debugger::Pad_M, ck_map_debugger::Pad_S)
            [ MakeSectionHeader(TEXT("Selected POI")) ]
        + SScrollBox::Slot().Padding(ck_map_debugger::Pad_M, 0.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(0.4f).VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Font(ck_map_debugger::Normal())
                        .ColorAndOpacity(ck_map_debugger::Text_Secondary)
                        .Text(FText::FromString(TEXT("Entity:")))
                    ]
                + SHorizontalBox::Slot().FillWidth(0.6f).VAlign(VAlign_Center)
                    [
                        SNew(SCkDebug_EntityRef)
                        .Entity_Lambda([this]() -> FCk_Handle { return _SelectedPoi; })
                        .ShowName(true)
                    ]
            ]
        + SScrollBox::Slot().Padding(ck_map_debugger::Pad_M, 0.0f)
            [ MakeStatRow(TEXT("Category:"), TAttribute<FText>::CreateLambda([this]()
                {
                    const auto* Info = DoFind_SelectedPoiInfo();
                    return FText::FromString(Info != nullptr ? Info->Category.ToString() : TEXT("--"));
                })) ]
        + SScrollBox::Slot().Padding(ck_map_debugger::Pad_M, 0.0f)
            [ MakeStatRow(TEXT("Name:"), TAttribute<FText>::CreateLambda([this]()
                {
                    const auto* Info = DoFind_SelectedPoiInfo();
                    return FText::FromString(Info != nullptr ? Info->DisplayName : TEXT("--"));
                })) ]
        + SScrollBox::Slot().Padding(ck_map_debugger::Pad_M, 0.0f)
            [ MakeStatRow(TEXT("State:"), TAttribute<FText>::CreateLambda([this]()
                {
                    const auto* Info = DoFind_SelectedPoiInfo();
                    if (Info == nullptr) { return FText::FromString(TEXT("--")); }
                    return FText::FromString(Info->Enabled ? TEXT("Enabled") : TEXT("Disabled"));
                })) ]
        + SScrollBox::Slot().Padding(ck_map_debugger::Pad_M, 0.0f)
            [ MakeStatRow(TEXT("Priority:"), TAttribute<FText>::CreateLambda([this]()
                {
                    const auto* Info = DoFind_SelectedPoiInfo();
                    return Info != nullptr ? FText::AsNumber(Info->Priority) : FText::FromString(TEXT("--"));
                })) ]
        + SScrollBox::Slot().Padding(ck_map_debugger::Pad_M, 0.0f)
            [ MakeStatRow(TEXT("World Pos:"), TAttribute<FText>::CreateLambda([this]()
                {
                    const auto* Info = DoFind_SelectedPoiInfo();
                    if (Info == nullptr) { return FText::FromString(TEXT("--")); }
                    return FText::FromString(ck::Format_UE(TEXT("X {:.0f}  Y {:.0f}  Z {:.0f}"),
                        Info->WorldPos.X, Info->WorldPos.Y, Info->WorldPos.Z));
                })) ]
        + SScrollBox::Slot().Padding(ck_map_debugger::Pad_M, 0.0f)
            [ MakeStatRow(TEXT("State Tags:"), TAttribute<FText>::CreateLambda([this]()
                {
                    const auto* Info = DoFind_SelectedPoiInfo();
                    if (Info == nullptr) { return FText::FromString(TEXT("--")); }
                    return FText::FromString(Info->StateTags.IsEmpty() ? TEXT("(none)") : Info->StateTags.ToStringSimple());
                })) ]
        + SScrollBox::Slot().Padding(ck_map_debugger::Pad_M, 0.0f)
            [ MakeStatRow(TEXT("Visible On:"), TAttribute<FText>::CreateLambda([this]()
                {
                    const auto* Info = DoFind_SelectedPoiInfo();
                    if (Info == nullptr) { return FText::FromString(TEXT("--")); }
                    return FText::FromString(ck::Format_UE(TEXT("{} projector(s)"), Info->VisibleOnCount));
                })) ]

        + SScrollBox::Slot().Padding(ck_map_debugger::Pad_M, ck_map_debugger::Pad_S)
            [ MakeSectionHeader(TEXT("Projectors")) ]
        + SScrollBox::Slot().Padding(ck_map_debugger::Pad_M, 0.0f)
            [ MakeStatRow(TEXT("Compasses:"), TAttribute<FText>::CreateLambda([this]()
                { return FText::AsNumber(_Snapshot->Compasses.Num()); })) ]
        + SScrollBox::Slot().Padding(ck_map_debugger::Pad_M, 0.0f)
            [ MakeStatRow(TEXT("Minimaps:"), TAttribute<FText>::CreateLambda([this]()
                { return FText::AsNumber(_Snapshot->Minimaps.Num()); })) ]
        + SScrollBox::Slot().Padding(ck_map_debugger::Pad_M, 0.0f)
            [ MakeStatRow(TEXT("Fog Grids:"), TAttribute<FText>::CreateLambda([this]()
                { return FText::AsNumber(_Snapshot->Fogs.Num()); })) ];

    // ---- Status bar ----

    auto StatusBar =
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
        .BorderBackgroundColor(ck_map_debugger::Bg_Dark)
        .Padding(FMargin(ck_map_debugger::Pad_M, ck_map_debugger::Pad_S))
        [
            SNew(STextBlock)
            .Font(ck_map_debugger::Mono(9))
            .ColorAndOpacity(ck_map_debugger::Text_Secondary)
            .Text_Lambda([this]() -> FText
            {
                if (NOT _Snapshot->HasWorld)
                { return FText::FromString(TEXT("--")); }

                auto FogPart = FString{TEXT("no fog")};

                if (_Snapshot->Fogs.Num() > 0)
                {
                    FogPart = ck::Format_UE(TEXT("fog {:.1f}% explored"),
                        _Snapshot->Fogs[0].ExploredFraction * 100.0f);
                }

                return FText::FromString(ck::Format_UE(TEXT("POIs: {} ({} enabled)   Compasses: {}   Minimaps: {}   {}"),
                    _Snapshot->Pois.Num(), _Snapshot->NumEnabledPois,
                    _Snapshot->Compasses.Num(), _Snapshot->Minimaps.Num(), FogPart));
            })
        ];

    // ---- Body ----

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
        .BorderBackgroundColor(ck_map_debugger::Bg_Medium)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot().AutoHeight()
                [ Toolbar ]

            + SVerticalBox::Slot().FillHeight(1.0f)
                [
                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot().AutoWidth()
                        [ SNew(SBox).WidthOverride(250.0f) [ LeftRail ] ]

                    + SHorizontalBox::Slot().AutoWidth()
                        [ SNew(SSeparator).Orientation(Orient_Vertical).Thickness(1.0f) ]

                    + SHorizontalBox::Slot().FillWidth(1.0f)
                        [ _Canvas.ToSharedRef() ]

                    + SHorizontalBox::Slot().AutoWidth()
                        [ SNew(SSeparator).Orientation(Orient_Vertical).Thickness(1.0f) ]

                    + SHorizontalBox::Slot().AutoWidth()
                        [ SNew(SBox).WidthOverride(280.0f) [ RightRail ] ]
                ]

            + SVerticalBox::Slot().AutoHeight()
                [ StatusBar ]
        ]
    ];
}

auto
    SCkMapDebuggerWindow::
    Tick(
        const FGeometry& InAllottedGeometry,
        double InCurrentTime,
        float InDeltaTime)
    -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(WindowId))
    { return; }

    DoRefreshSnapshot();
    DoRefreshRowItems();
}

auto
    SCkMapDebuggerWindow::
    DoClearOnWorldGone()
    -> void
{
    _SelectedPoi = {};
    _Snapshot = MakeShared<FCkMapDebug_Snapshot>();
    _AllRows.Reset();
    _VisibleRows.Reset();

    if (_Canvas.IsValid())
    {
        _Canvas->ClearHandles();
        _Canvas->Set_Snapshot(_Snapshot);
    }

    if (_PoiList.IsValid())
    { _PoiList->RequestListRefresh(); }
}

auto
    SCkMapDebuggerWindow::
    DoFind_SelectedPoiInfo() const
    -> const FCkMapDebug_PoiInfo*
{
    if (ck::Is_NOT_Valid(_SelectedPoi))
    { return nullptr; }

    return _Snapshot->Pois.FindByPredicate([this](const FCkMapDebug_PoiInfo& InInfo)
    { return InInfo.Handle == _SelectedPoi; });
}

auto
    SCkMapDebuggerWindow::
    DoRefreshSnapshot()
    -> void
{
    auto Snapshot = MakeShared<FCkMapDebug_Snapshot>();

    auto* World = ck_map_debugger::FindGameWorld();

    if (World == nullptr)
    {
        if (_Snapshot->HasWorld)
        { DoClearOnWorldGone(); }

        return;
    }

    Snapshot->HasWorld = true;
    Snapshot->WorldLabel = ck::Format_UE(TEXT("{} ({})"), World->GetName(), ck_map_debugger::NetModeLabel(World));

    auto TransientEntity = UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(World);

    if (ck::IsValid(TransientEntity))
    {
        auto PoiIndexByHandle = TMap<FCk_Handle, int32>{};

        TransientEntity.View<ck::FTag_Poi>().ForEach(
            [&](FCk_Entity InEntity)
            {
                auto Handle = ck::MakeHandle(InEntity, TransientEntity);
                const auto PoiHandle = UCk_Utils_Poi_UE::Cast(Handle);

                if (ck::Is_NOT_Valid(PoiHandle))
                { return; }

                auto Info = FCkMapDebug_PoiInfo{};
                Info.Handle = Handle;
                Info.Category = UCk_Utils_Poi_UE::Get_CategoryTags(PoiHandle).First();
                Info.DisplayName = UCk_Utils_GameplayLabel_UE::Has(Handle)
                    ? UCk_Utils_GameplayLabel_UE::Get_Label(Handle).GetTagName().ToString()
                    : Info.Category.GetTagName().ToString();
                Info.Enabled = NOT UCk_Utils_EntityTag_UE::Has_UsingGameplayTag(Handle, Tag_Poi_DisabledName);
                Info.Priority = 0; // Priority moved to CkPoiDisplayDefinition in CkPoi v2 refactor; column fate decided in Gate 4.
                Info.WorldPos = UCk_Utils_Poi_UE::Get_WorldLocation(PoiHandle);
                Info.MaxRange = UCk_Utils_VisibleRange_UE::Has(Handle)
                    ? UCk_Utils_VisibleRange_UE::Get_MaxRange(UCk_Utils_VisibleRange_UE::Cast(Handle))
                    : 0.0f;
                Info.StateTags = UCk_Utils_EntityTag_UE::Get_AllTagsAsContainer(Handle);

                if (Info.Enabled)
                { ++Snapshot->NumEnabledPois; }

                PoiIndexByHandle.Add(Handle, Snapshot->Pois.Num());
                Snapshot->Pois.Add(MoveTemp(Info));
            });

        const auto BumpVisibleOn = [&](const FCk_Handle& InPoiHandle) -> void
        {
            if (const auto* Index = PoiIndexByHandle.Find(InPoiHandle))
            { ++Snapshot->Pois[*Index].VisibleOnCount; }
        };

        TransientEntity.View<ck::FFragment_Minimap_Params, ck::FFragment_Minimap_Current>().ForEach(
            [&](FCk_Entity InEntity, const ck::FFragment_Minimap_Params&, const ck::FFragment_Minimap_Current&)
            {
                auto Handle = ck::MakeHandle(InEntity, TransientEntity);
                auto MinimapHandle = UCk_Utils_Minimap_UE::Cast(Handle);

                if (ck::Is_NOT_Valid(MinimapHandle))
                { return; }

                auto Info = FCkMapDebug_MinimapInfo{};
                Info.Handle = Handle;
                Info.ViewOrigin = UCk_Utils_Minimap_UE::Get_ViewOrigin(MinimapHandle);
                Info.ViewYawDegrees = UCk_Utils_Minimap_UE::Get_ViewYawDegrees(MinimapHandle);
                Info.ViewExtent = UCk_Utils_Minimap_UE::Get_ViewExtent(MinimapHandle);
                Info.ProjectionMode = UCk_Utils_Minimap_UE::Get_ProjectionMode(MinimapHandle);
                Info.RotationMode = UCk_Utils_Minimap_UE::Get_RotationMode(MinimapHandle);
                Info.FixedBounds = UCk_Utils_Minimap_UE::Get_FixedBounds(MinimapHandle);

                const auto Entries = UCk_Utils_Minimap_UE::Get_Entries(MinimapHandle);
                Info.EntryCount = Entries.Num();

                for (const auto& Entry : Entries)
                { BumpVisibleOn(Entry.Get_Poi()); }

                Snapshot->Minimaps.Add(MoveTemp(Info));
            });

        TransientEntity.View<ck::FFragment_Compass_Params, ck::FFragment_Compass_Current>().ForEach(
            [&](FCk_Entity InEntity, const ck::FFragment_Compass_Params&, const ck::FFragment_Compass_Current&)
            {
                auto Handle = ck::MakeHandle(InEntity, TransientEntity);
                auto CompassHandle = UCk_Utils_Compass_UE::Cast(Handle);

                if (ck::Is_NOT_Valid(CompassHandle))
                { return; }

                auto Info = FCkMapDebug_CompassInfo{};
                Info.Handle = Handle;
                Info.HeadingDegrees = UCk_Utils_Compass_UE::Get_Heading(CompassHandle);

                auto Observer = UCk_Utils_Compass_UE::Get_Observer(CompassHandle);

                if (ck::IsValid(Observer))
                {
                    const auto ObserverTransform = UCk_Utils_Transform_UE::Cast(Observer);

                    if (ck::IsValid(ObserverTransform))
                    {
                        Info.ObserverLocation = UCk_Utils_Transform_UE::Get_EntityCurrentLocation(ObserverTransform);
                        Info.HasObserverLocation = true;
                    }
                }

                const auto Entries = UCk_Utils_Compass_UE::Get_Entries(CompassHandle);
                Info.EntryCount = Entries.Num();

                for (const auto& Entry : Entries)
                { BumpVisibleOn(Entry.Get_Poi()); }

                Snapshot->Compasses.Add(MoveTemp(Info));
            });

        TransientEntity.View<ck::FFragment_FogOfWar_Params, ck::FFragment_FogOfWar_Current>().ForEach(
            [&](FCk_Entity InEntity, const ck::FFragment_FogOfWar_Params& InParams, const ck::FFragment_FogOfWar_Current& InCurrent)
            {
                auto Handle = ck::MakeHandle(InEntity, TransientEntity);
                const auto FogHandle = UCk_Utils_FogOfWar_UE::Cast(Handle);

                if (ck::Is_NOT_Valid(FogHandle))
                { return; }

                auto Info = FCkMapDebug_FogInfo{};
                Info.Handle = Handle;
                Info.Bounds = InParams.Get_Bounds();
                Info.CellSize = InParams.Get_CellSize();
                Info.CellCounts = InCurrent.Get_CellCounts();
                Info.Explored = InCurrent.Get_Explored();
                Info.ExploredFraction = UCk_Utils_FogOfWar_UE::Get_ExploredFraction(FogHandle);

                Snapshot->Fogs.Add(MoveTemp(Info));
            });
    }

    // Selected POI died -> drop the selection (its snapshot row is gone anyway)
    if (ck::IsValid(_SelectedPoi))
    {
        const auto StillExists = Snapshot->Pois.ContainsByPredicate([this](const FCkMapDebug_PoiInfo& InInfo)
        { return InInfo.Handle == _SelectedPoi; });

        if (NOT StillExists)
        {
            _SelectedPoi = {};

            if (_Canvas.IsValid())
            { _Canvas->Set_SelectedPoi({}); }
        }
    }

    _Snapshot = MoveTemp(Snapshot);

    if (_Canvas.IsValid())
    {
        _Canvas->Set_Snapshot(_Snapshot);
        _Canvas->Set_SelectedPoi(_SelectedPoi);
    }
}

auto
    SCkMapDebuggerWindow::
    DoRefreshRowItems()
    -> void
{
    // Stable-pointer refresh (SListView selection contract): reuse the TSharedPtr when the POI handle
    // matches, update fields in place, allocate only for new POIs.
    auto Existing = TMap<FCk_Handle, TSharedPtr<FCkMapDebug_PoiRow>>{};

    for (const auto& Row : _AllRows)
    {
        if (Row.IsValid())
        { Existing.Add(Row->Handle, Row); }
    }

    auto ReferenceLocation = FVector{};
    auto HasReferenceLocation = false;

    if (_Snapshot->Minimaps.Num() > 0)
    {
        ReferenceLocation = _Snapshot->Minimaps[0].ViewOrigin;
        HasReferenceLocation = true;
    }
    else if (_Snapshot->Compasses.Num() > 0 && _Snapshot->Compasses[0].HasObserverLocation)
    {
        ReferenceLocation = _Snapshot->Compasses[0].ObserverLocation;
        HasReferenceLocation = true;
    }

    auto NewRows = TArray<TSharedPtr<FCkMapDebug_PoiRow>>{};
    auto SetChanged = false;

    for (const auto& Info : _Snapshot->Pois)
    {
        auto Row = TSharedPtr<FCkMapDebug_PoiRow>{};

        if (const auto* Found = Existing.Find(Info.Handle))
        {
            Row = *Found;
            Existing.Remove(Info.Handle);
        }
        else
        {
            Row = MakeShared<FCkMapDebug_PoiRow>();
            Row->Handle = Info.Handle;
            SetChanged = true;
        }

        Row->Color = ck_map_debugger::CategoryColor(Info.Category);
        Row->Name = Info.DisplayName;
        Row->Enabled = Info.Enabled;
        Row->Distance = HasReferenceLocation
            ? ck::Format_UE(TEXT("{:.0f}m"), FVector::Dist(ReferenceLocation, Info.WorldPos) / 100.0f)
            : FString{TEXT("--")};
        Row->HighlightMatch = _HighlightString.IsEmpty()
            || Row->Name.Contains(_HighlightString)
            || Info.Category.ToString().Contains(_HighlightString);

        NewRows.Add(MoveTemp(Row));
    }

    if (Existing.Num() > 0)
    { SetChanged = true; }

    _AllRows = MoveTemp(NewRows);

    auto NewVisible = TArray<TSharedPtr<FCkMapDebug_PoiRow>>{};

    for (const auto& Row : _AllRows)
    {
        const auto PassesFilter = _FilterString.IsEmpty() || Row->Name.Contains(_FilterString);

        if (PassesFilter)
        { NewVisible.Add(Row); }
    }

    const auto VisibleChanged = NewVisible.Num() != _VisibleRows.Num() || SetChanged;
    _VisibleRows = MoveTemp(NewVisible);

    if (VisibleChanged && _PoiList.IsValid())
    {
        _PoiList->RequestListRefresh();

        // Restore selection onto the (possibly reallocated) row for the selected handle
        if (ck::IsValid(_SelectedPoi))
        {
            const auto FoundRow = _VisibleRows.FindByPredicate([this](const TSharedPtr<FCkMapDebug_PoiRow>& InRow)
            { return InRow.IsValid() && InRow->Handle == _SelectedPoi; });

            if (FoundRow != nullptr)
            {
                const auto Current = _PoiList->GetSelectedItems();
                const auto AlreadySelected = Current.Num() == 1 && Current[0] == *FoundRow;

                if (NOT AlreadySelected)
                { _PoiList->SetItemSelection(*FoundRow, true, ESelectInfo::Direct); }
            }
        }
    }
}

auto
    SCkMapDebuggerWindow::
    OnGenerateRow(
        TSharedPtr<FCkMapDebug_PoiRow> InRow,
        const TSharedRef<STableViewBase>& InOwnerTable)
    -> TSharedRef<ITableRow>
{
    const auto WeakRow = TWeakPtr<FCkMapDebug_PoiRow>{InRow};

    // Plain visual widgets only — no click-consuming children, so STableRow selection bubbles cleanly
    return SNew(STableRow<TSharedPtr<FCkMapDebug_PoiRow>>, InOwnerTable)
        .Padding(FMargin{2.0f, 1.0f})
        .ShowSelection(true)
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2.0f, 0.0f)
                [
                    SNew(SBox).WidthOverride(8.0f).HeightOverride(8.0f)
                    [
                        SNew(SImage)
                        .Image(FAppStyle::GetBrush("WhiteBrush"))
                        .ColorAndOpacity_Lambda([WeakRow]() -> FSlateColor
                        {
                            const auto Row = WeakRow.Pin();
                            return Row.IsValid() ? Row->Color : FLinearColor::White;
                        })
                    ]
                ]

            + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(4.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .Font(ck_map_debugger::Normal())
                    .Text_Lambda([WeakRow]() -> FText
                    {
                        const auto Row = WeakRow.Pin();
                        return FText::FromString(Row.IsValid() ? Row->Name : FString{});
                    })
                    .ColorAndOpacity_Lambda([WeakRow]() -> FSlateColor
                    {
                        const auto Row = WeakRow.Pin();

                        if (NOT Row.IsValid())
                        { return ck_map_debugger::Text_Primary; }

                        if (NOT Row->Enabled)
                        { return ck_map_debugger::Color_Disabled; }

                        return Row->HighlightMatch ? ck_map_debugger::Text_Primary : ck_map_debugger::Text_Muted;
                    })
                ]

            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .Font(ck_map_debugger::Mono(8))
                    .ColorAndOpacity(ck_map_debugger::Text_Secondary)
                    .Text_Lambda([WeakRow]() -> FText
                    {
                        const auto Row = WeakRow.Pin();
                        return FText::FromString(Row.IsValid() ? Row->Distance : FString{});
                    })
                ]
        ];
}

auto
    SCkMapDebuggerWindow::
    OnRowSelectionChanged(
        TSharedPtr<FCkMapDebug_PoiRow> InRow,
        ESelectInfo::Type InSelectInfo)
    -> void
{
    // Programmatic restores echo back with Direct — ignore them so the restore doesn't loop
    if (InSelectInfo == ESelectInfo::Direct)
    { return; }

    _SelectedPoi = InRow.IsValid() ? InRow->Handle : FCk_Handle{};

    if (_Canvas.IsValid())
    { _Canvas->Set_SelectedPoi(_SelectedPoi); }
}

auto
    SCkMapDebuggerWindow::
    MakeSectionHeader(
        const FString& InText) const
    -> TSharedRef<SWidget>
{
    return SNew(STextBlock)
        .Font(ck_map_debugger::Bold(10))
        .ColorAndOpacity(ck_map_debugger::Text_Highlight)
        .Text(FText::FromString(InText));
}

auto
    SCkMapDebuggerWindow::
    MakeStatRow(
        const FString&     InLabel,
        TAttribute<FText>  InValue) const
    -> TSharedRef<SWidget>
{
    return SNew(SHorizontalBox)

        + SHorizontalBox::Slot().FillWidth(0.4f).VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Font(ck_map_debugger::Normal())
                .ColorAndOpacity(ck_map_debugger::Text_Secondary)
                .Text(FText::FromString(InLabel))
            ]

        + SHorizontalBox::Slot().FillWidth(0.6f).VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Font(ck_map_debugger::Mono())
                .ColorAndOpacity(ck_map_debugger::Text_Primary)
                .Text(MoveTemp(InValue))
            ]
        ;
}

// --------------------------------------------------------------------------------------------------------------------
