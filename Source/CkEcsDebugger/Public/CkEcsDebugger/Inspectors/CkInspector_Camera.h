#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

// --------------------------------------------------------------------------------------------------------------------
// Inspector for the ECS Camera feature (CkCamera/Camera). Lights up for the director entity
// (FFragment_Camera_Current) and for any selected layer child entity (FFragment_CameraLayer_*).
//
// Sections: Director (intention / dominant / composed-profile summary / final ViewInfo), Layer Stack
// (one live row per record entry with a blend-weight bar; the persistent base layer is marked), POV pipeline
// (the FPov_State intermediates), and Layer detail.
//
// Tick draws the live camera rig in-world (boom, gizmo+forward, look-at, collision push-in).
// --------------------------------------------------------------------------------------------------------------------

class CKECSDEBUGGER_API FCkInspector_Camera : public ICkDebuggerComponentInspector_Base
{
public:
    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Camera; }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("B8A1E3"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    ~FCkInspector_Camera() override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 62; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& Entity) const -> TSharedRef<SWidget>;
    // Rebuild the stack section when the modifier count changes (modifiers are added / pruned over time).
    int32 _LastModifierCount = -1;
    TArray<TWeakPtr<class SCkInspector_CameraAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;
};

class FCkUiCollection;
class FCkUiView;

class CKECSDEBUGGER_API SCkInspector_CameraAuthored final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_CameraAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(TSet<FString>, DiffLabels)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_CameraAuthored() override;
    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_LayersCollection() const -> TSharedPtr<FCkUiCollection> { return _Layers; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }
    auto Get_IsAvailable() const -> bool;
    auto Get_IsLayerAvailable() const -> bool;
    auto Get_CanEdit() const -> bool;
    auto Get_EditDisabledReason() const -> FString;
    auto Get_Text(const FString& InKey) const -> FString;
    auto Get_Bool(const FString& InKey) const -> bool;
    auto Get_Number(const FString& InKey) const -> float;
    auto Is_DiffMarked(const FString& InLabel) const -> bool { return _DiffLabels.Contains(InLabel); }

private:
    auto Refresh_Layers() -> bool;
    auto Build_AuthoredView() -> bool;
    auto Set_Flag(const FString& InKey, bool bInEnabled) -> void;
    auto Commit_BoomRotation(const FRotator& InRotation) -> void;
    auto Commit_BoomComponent(const FString& InKey, float InValue) -> void;
    auto Commit_YawMin(float InValue) -> void;
    auto Commit_YawMax(float InValue) -> void;
    auto Commit_OrientationIntention(const FVector& InValue) -> void;
    auto Commit_OrientationComponent(const FString& InKey, float InValue) -> void;
    auto Get_DiffColor(const FString& InLabel) const -> FLinearColor;

private:
    FCk_Handle _Entity;
    TSet<FString> _DiffLabels;
    TSharedPtr<FCkUiCollection> _Layers;
    TSharedPtr<FCkUiView> _View;
    FString _LoadError;
    bool _Active = true;
    bool _Mounted = false;
};
