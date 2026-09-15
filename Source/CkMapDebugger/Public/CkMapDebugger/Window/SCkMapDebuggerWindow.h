#pragma once

#include "CoreMinimal.h"
#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"

#include "CkEcs/Handle/CkHandle.h"

#include "CkMinimap/CkMinimap_Fragment_Data.h"
#include "CkMinimap/CkMinimap_WorldBounds.h"

#include "Containers/BitArray.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Views/SListView.h"

#include <GameplayTagContainer.h>

class ITableRow;
class STableViewBase;
class SBox;
class SCkDebug_DualSearchBar;
class SCkDebug_EntityRef;
class SCkDebug_IconToggle;
class SCkDebug_WindowChrome;
class FCkUiView;
struct FCkMapDebuggerAuthoredTestAccess;

// --------------------------------------------------------------------------------------------------------------------
// Snapshot of the map stack, rebuilt on the gated Tick and read by TAttribute lambdas + the canvas OnPaint.
// Handles inside are cleared on EndPIE (SmDebugger contract — cached handles crash on destruct once the PIE
// registry dies).
// --------------------------------------------------------------------------------------------------------------------

// Priority is per-consumer (CkPoiDisplayDefinition) since the CkPoi v2 refactor — a single number per Poi is
// meaningless, so this snapshot no longer carries one; the PoiDisplayDefinition inspector owns per-consumer priority.
struct FCkMapDebug_PoiInfo
{
    FCk_Handle Handle;
    FGameplayTag Category;
    FString DisplayName;
    bool    Enabled = true;
    FVector WorldPos = FVector::ZeroVector;
    float   MaxRange = 0.0f;
    FGameplayTagContainer StateTags;
    int32   VisibleOnCount = 0;
};

struct FCkMapDebug_MinimapInfo
{
    FCk_Handle Handle;
    FVector ViewOrigin = FVector::ZeroVector;
    float   ViewYawDegrees = 0.0f;
    float   ViewExtent = 1.0f;
    ECk_Minimap_ProjectionMode ProjectionMode = ECk_Minimap_ProjectionMode::ObserverCentric;
    ECk_Minimap_RotationMode   RotationMode   = ECk_Minimap_RotationMode::NorthLocked;
    FCk_Minimap_WorldBounds FixedBounds;
    int32 EntryCount = 0;
};

struct FCkMapDebug_CompassInfo
{
    FCk_Handle Handle;
    float   HeadingDegrees = 0.0f;
    FVector ObserverLocation = FVector::ZeroVector;
    bool    HasObserverLocation = false;
    int32   EntryCount = 0;
};

struct FCkMapDebug_FogInfo
{
    FCk_Handle Handle;
    FCk_Minimap_WorldBounds Bounds;
    FIntPoint CellCounts = FIntPoint::ZeroValue;
    float     CellSize = 1.0f;
    TBitArray<> Explored;
    float     ExploredFraction = 0.0f;
};

struct FCkMapDebug_Snapshot
{
    bool    HasWorld = false;
    FString WorldLabel;

    TArray<FCkMapDebug_PoiInfo>     Pois;
    TArray<FCkMapDebug_MinimapInfo> Minimaps;
    TArray<FCkMapDebug_CompassInfo> Compasses;
    TArray<FCkMapDebug_FogInfo>     Fogs;

    int32 NumEnabledPois = 0;
};

// One list row per POI. Pointer identity is stable across refreshes (SListView selection contract);
// fields are updated in place and row widgets read them via TAttribute lambdas.
struct FCkMapDebug_PoiRow
{
    FCk_Handle Handle;
    FLinearColor Color = FLinearColor::White;
    FString Name;
    FString Distance;
    bool Enabled = true;
    bool HighlightMatch = true;
    TSharedPtr<FCkUiView> Presentation;
};

// --------------------------------------------------------------------------------------------------------------------
// Top-down 2D canvas: world bounds, fog grids, POI diamonds, compass heading rays, minimap view frames.
// North-up mapping matches the CkMinimap frame convention: panel right = world +Y (East), panel up = world +X
// (North). Wheel zooms, right-drag pans, left click selects the nearest POI.
// --------------------------------------------------------------------------------------------------------------------

class SCkMapDebug_Canvas : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCkMapDebug_Canvas) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    auto Set_Snapshot(TSharedPtr<const FCkMapDebug_Snapshot> InSnapshot) -> void { _Snapshot = MoveTemp(InSnapshot); }
    auto Set_SelectedPoi(const FCk_Handle& InPoi) -> void { _SelectedPoi = InPoi; }
    auto Set_OnPoiPicked(TFunction<void(const FCk_Handle&)> InOnPoiPicked) -> void { _OnPoiPicked = MoveTemp(InOnPoiPicked); }

    auto ClearHandles() -> void { _SelectedPoi = {}; _Snapshot.Reset(); }
    auto Release_Interaction() -> void;

protected:
    auto OnPaint(const FPaintArgs& InArgs, const FGeometry& InAllottedGeometry, const FSlateRect& InCullingRect, FSlateWindowElementList& OutDrawElements, int32 InLayerId, const FWidgetStyle& InWidgetStyle, bool InParentEnabled) const -> int32 override;
    auto ComputeDesiredSize(float InLayoutScaleMultiplier) const -> FVector2D override;

    auto OnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) -> FReply override;
    auto OnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) -> FReply override;
    auto OnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) -> FReply override;
    auto OnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) -> FReply override;

private:
    friend struct FCkMapDebuggerAuthoredTestAccess;

    // World window (center + half-extent, world cm) fitted around everything the snapshot contains
    auto DoCompute_WorldWindow(FVector2D& OutCenter, double& OutHalfExtent) const -> bool;
    auto DoWorldToPanel(const FVector2D& InWorldXY, const FVector2D& InPanelSize, const FVector2D& InWorldCenter, double InScale) const -> FVector2D;

    TSharedPtr<const FCkMapDebug_Snapshot> _Snapshot;
    FCk_Handle _SelectedPoi;
    TFunction<void(const FCk_Handle&)> _OnPoiPicked;

    float     _Zoom = 1.0f;
    FVector2D _PanOffset = FVector2D::ZeroVector;
    bool      _IsPanning = false;
    bool      _InteractionReleased = false;
    FVector2D _LastMousePos = FVector2D::ZeroVector;
};

// --------------------------------------------------------------------------------------------------------------------
// CK Map Debugger window — POI list (left), top-down canvas (center), selected-POI detail (right).
// Scrunch-free: the tree is built once in Construct; values flow via TAttribute lambdas; the list source
// mutates only via the stable-pointer refresh; the canvas is pure OnPaint.
// --------------------------------------------------------------------------------------------------------------------

class SCkMapDebuggerWindow : public SCkDebugger_WindowBase
{
public:
    static const FName WindowId;

    SLATE_BEGIN_ARGS(SCkMapDebuggerWindow) {}
        SLATE_ARGUMENT(FString, ResourceDirectoryOverride)
    SLATE_END_ARGS()

    virtual ~SCkMapDebuggerWindow() override;

    auto Construct(const FArguments& InArgs) -> void;
    auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    /** Idempotently drops every handle-bearing and authored presentation reference before tab teardown. */
    auto Release_Presentation() -> void;
    auto Get_AuthoredShellView() const -> TSharedPtr<FCkUiView> { return _AuthoredShellView; }
    auto Get_AuthoredShellLoadFailure() const -> const FString& { return _AuthoredShellLoadFailure; }
    auto IsUsingNativeShellFallback() const -> bool { return _UsingNativeShellFallback; }

    virtual auto Get_WindowId() const -> FName override { return WindowId; }
    virtual auto Get_WindowDisplayName() const -> FText override { return FText::FromString(TEXT("CK Map Debugger")); }

private:
    friend struct FCkMapDebuggerAuthoredTestAccess;

    auto Build_AuthoredShell() -> void;
    auto Poll_AuthoredShell(double InCurrentTime) -> void;
    auto Mount_AuthoredShell() -> bool;
    auto Build_PoiRowPresentation(const TSharedPtr<FCkMapDebug_PoiRow>& InRow) -> TSharedPtr<SWidget>;
    auto Build_NativeShellFallback(TSharedRef<SWidget> InLeftRail, TSharedRef<SWidget> InRightRail,
        TSharedRef<SWidget> InStatusBar) -> TSharedRef<SWidget>;
    auto DoRefreshSnapshot() -> void;
    auto DoRefreshRowItems() -> void;
    auto DoClearOnWorldGone() -> void;
    /** Future authored admission moves only these retained hosts, never a live fallback child. */
    auto Detach_FallbackPorts() -> void;
    auto Restore_FallbackPorts() -> void;

    auto DoFind_SelectedPoiInfo() const -> const FCkMapDebug_PoiInfo*;

    auto OnGenerateRow(TSharedPtr<FCkMapDebug_PoiRow> InRow, const TSharedRef<STableViewBase>& InOwnerTable) -> TSharedRef<ITableRow>;
    auto OnRowSelectionChanged(TSharedPtr<FCkMapDebug_PoiRow> InRow, ESelectInfo::Type InSelectInfo) -> void;

    auto MakeRailSeparator() const -> TSharedRef<SWidget>;
    auto MakeSectionHeader(const FString& InText) const -> TSharedRef<SWidget>;
    auto MakeStatRow(const FString& InLabel, TAttribute<FText> InValue) const -> TSharedRef<SWidget>;

    TSharedPtr<FCkMapDebug_Snapshot> _Snapshot;
    FCk_Handle _SelectedPoi;

    TArray<TSharedPtr<FCkMapDebug_PoiRow>> _AllRows;
    TArray<TSharedPtr<FCkMapDebug_PoiRow>> _VisibleRows;
    TSharedPtr<SListView<TSharedPtr<FCkMapDebug_PoiRow>>> _PoiList;
    TSharedPtr<SCkMapDebug_Canvas> _Canvas;
    TSharedPtr<SCkDebug_DualSearchBar> _SearchBar;
    TSharedPtr<SCkDebug_EntityRef> _SelectedEntity;
    TSharedPtr<SCkDebug_IconToggle> _EnabledPoisToggle;
    TSharedPtr<SBox> _FallbackSearchHost;
    TSharedPtr<SBox> _FallbackListHost;
    TSharedPtr<SBox> _FallbackCanvasHost;
    TSharedPtr<SBox> _FallbackSelectedEntityHost;
    TSharedPtr<SBox> _FallbackEnabledPoisHost;
    TSharedPtr<SBox> _ShellHost;
    TSharedPtr<SBox> _ActionsHost;
    TSharedPtr<SCkDebug_WindowChrome> _Chrome;
    TSharedPtr<FCkUiView> _AuthoredShellView;
    FString _AuthoredShellLoadFailure;
    FString _AuthoredShellMarkupPath;
    FString _AuthoredShellStylesheetPath;
    FString _PoiRowMarkupPath;
    FString _PoiRowStylesheetPath;
    FString _ResourceDirectoryOverride;
    double _NextAuthoredShellPollSeconds = 0.0;
    bool _UsingNativeShellFallback = true;
    bool _PresentationReleased = false;

    FString _FilterString;
    FString _HighlightString;
    bool _ShowEnabledPoisOnly = false;

    FDelegateHandle _EndPieHandle;
};

// --------------------------------------------------------------------------------------------------------------------
