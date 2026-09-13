#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "Widgets/SCompoundWidget.h"

class FCkUiCollection;
class FCkUiView;
class SWrapBox;
struct FCkInspector_ProbeDrawState;

class CKECSDEBUGGER_API SCkInspector_ProbesAuthored : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_ProbesAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(TSet<FString>, DiffLabels)
        SLATE_ARGUMENT(TSharedPtr<FCkInspector_ProbeDrawState>, DrawState)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_ProbesAuthored() override;
    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_Overlaps() const -> TSharedPtr<FCkUiCollection> { return _Overlaps; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Get_IsAvailable() const -> bool;
    auto Get_NameText() const -> FString;
    auto Get_StateText() const -> FString;
    auto Get_StateForeground() const -> FLinearColor;
    auto Get_StateBackground() const -> FLinearColor;
    auto Get_DebugDrawEnabled() const -> bool;
    auto Get_CanToggleDebugDraw() const -> bool;
    auto Get_DebugDrawDisabledReason() const -> FString;
    auto Get_ResponseText() const -> FString;
    auto Get_MotionText() const -> FString;
    auto Get_QualityText() const -> FString;
    auto Get_FilterText() const -> FString;
    auto Is_DiffMarked(const FString& InLabel) const -> bool { return _DiffLabels.Contains(InLabel); }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }

private:
    auto Build_AuthoredView() -> bool;
    auto Refresh_Overlaps() -> bool;
    auto Set_DebugDrawEnabled(bool bInEnabled) -> void;
    auto Navigate_Overlap(const FString& InStableKey) -> void;

private:
    FCk_Handle _Entity;
    TSet<FString> _DiffLabels;
    TSharedPtr<FCkInspector_ProbeDrawState> _DrawState;
    TSharedPtr<FCkUiCollection> _Overlaps;
    TMap<FString, FCk_Handle> _OverlapsByKey;
    TArray<FString> _OverlapSnapshot;
    TSharedPtr<FCkUiView> _View;
    FString _LoadError;
    bool _Active = true;
    bool _Mounted = false;
};

class CKECSDEBUGGER_API FCkInspector_Probes : public ICkDebuggerComponentInspector_Base
{
public:
    FCkInspector_Probes();
    ~FCkInspector_Probes() override;

    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Probe; }
    auto Get_FeatureFlagId() const -> FName override { return TEXT("Probe"); }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("D55181"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 70; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& Entity) -> TSharedRef<SWidget>;
    auto Ensure_DrawState() -> void;
    auto Disable_AllOwnedDebugDraw() -> void;

private:
    TArray<TWeakPtr<SCkInspector_ProbesAuthored>> _AuthoredInstances;
    TSharedPtr<FCkInspector_ProbeDrawState> _DrawState;
    FCk_Handle _LastInspectedEntity;
    TSharedPtr<SWrapBox> _NativeOverlapsBox;
    int32 _NativeLastOverlapCount = -1;
    FString _LastAuthoredLoadError;
    bool _DebugDrawPreference = true;
};
