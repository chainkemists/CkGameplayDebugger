#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "Widgets/SCompoundWidget.h"

class FCkUiCollection;
class FCkUiView;
class SWrapBox;

class CKECSDEBUGGER_API SCkInspector_ProbeTracesAuthored : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_ProbeTracesAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(TSet<FString>, DiffLabels)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_ProbeTracesAuthored() override;
    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_Overlaps() const -> TSharedPtr<FCkUiCollection> { return _Overlaps; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Get_IsAvailable() const -> bool;
    auto Get_IsEnabled() const -> bool;
    auto Get_CanToggleEnabled() const -> bool;
    auto Get_EnabledTooltip() const -> FString;
    auto Get_TypeText() const -> FString;
    auto Get_DirectionXText() const -> FString;
    auto Get_DirectionYText() const -> FString;
    auto Get_DirectionZText() const -> FString;
    auto Get_PolicyText() const -> FString;
    auto Get_IsShapeVisible() const -> bool;
    auto Get_ShapeText() const -> FString;
    auto Get_FilterText() const -> FString;
    auto Get_HasOverlaps() const -> bool { return _Active && NOT _OverlapSnapshot.IsEmpty(); }
    auto Is_DiffMarked(const FString& InLabel) const -> bool { return _DiffLabels.Contains(InLabel); }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }

private:
    auto Build_AuthoredView() -> bool;
    auto Refresh_Overlaps() -> bool;
    auto Set_Enabled(bool bInEnabled) -> void;
    auto Navigate_Overlap(const FString& InStableKey) -> void;

private:
    FCk_Handle _Entity;
    TSet<FString> _DiffLabels;
    TSharedPtr<FCkUiCollection> _Overlaps;
    TMap<FString, FCk_Handle> _OverlapsByKey;
    TArray<FString> _OverlapSnapshot;
    TSharedPtr<FCkUiView> _View;
    FString _LoadError;
    bool _Active = true;
    bool _Mounted = false;
};

class CKECSDEBUGGER_API FCkInspector_ProbeTraces : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_ProbeTraces() override;

    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Probe; }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("D55181"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 75; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& Entity) -> TSharedRef<SWidget>;
    auto Disable_OwnedDebugDraw(const FCk_Handle& Entity) -> void;
    auto Disable_AllOwnedDebugDraw() -> void;

private:
    TArray<TWeakPtr<SCkInspector_ProbeTracesAuthored>> _AuthoredInstances;
    TArray<FCk_Handle> _OwnedDebugDrawTraces;
    FCk_Handle _LastInspectedEntity;
    TSharedPtr<SWrapBox> _NativeOverlapsBox;
    TArray<FString> _NativeOverlapSnapshot;
    FString _LastAuthoredLoadError;
    uint8 _StructureMask = 0;
};
