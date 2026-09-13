#pragma once

#include "CkCore/Enums/CkEnums.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"
#include "Widgets/SCompoundWidget.h"

class FCkUiView;
class FCkInspectorEditScope;
class SBox;

/**
 * The authored shell deliberately owns only per-view state.  In particular, it never borrows the
 * inspector's edit scratch: a second details panel must not change the first panel's pending offset.
 */
class SCkInspector_TransformAuthored final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_TransformAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(TSharedPtr<FCkInspectorEditGuard>, EditGuard)
        SLATE_ARGUMENT(TSet<FString>, DiffLabels)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_TransformAuthored() override;
    virtual auto Tick(const FGeometry&, double, float) -> void override;
    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }
    auto Get_IsAvailable() const -> bool;
    auto Get_HasInterpolation() const -> bool;
    auto Get_CanRequest() const -> bool;
    auto Get_RequestDisabledReason() const -> FString;
    auto Get_Location() const -> FVector;
    auto Get_Rotation() const -> FRotator;
    auto Get_Scale() const -> FVector;
    auto Get_AxisText(const FString& InKind, int32 InAxis) const -> FString;
    auto Get_EditSpaceText() const -> FString;
    auto Is_DiffMarked(const FString& InLabel) const -> bool { return _DiffLabels.Contains(InLabel); }
    auto Commit_EditSpace(int32 InIndex) -> void;
    auto Commit_SetLocation(const FVector& InValue) -> void;
    auto Commit_SetRotation(const FRotator& InValue) -> void;
    auto Commit_SetScale(const FVector& InValue) -> void;
    auto Commit_LocationOffset(const FVector& InValue) -> void;
    auto Commit_RotationOffset(const FRotator& InValue) -> void;
    auto Commit_InterpolationLocation(const FVector& InValue) -> void;
    auto Commit_InterpolationRotation(const FRotator& InValue) -> void;
    auto Request_AddLocation() -> void;
    auto Request_AddRotation() -> void;
    auto Request_ForceRefresh() -> void;

private:
    auto Build_AuthoredView() -> bool;
    auto Build_OrientationValue() -> TSharedRef<SWidget>;
    auto Build_SpaceValue() -> TSharedRef<SWidget>;
    auto Build_VectorValue(FName InTagPrefix, TFunction<FVector()> InGet, TFunction<void(const FVector&)> InSet,
        TFunction<bool()> InIsEnabled) -> TSharedRef<SWidget>;
    auto Build_RotatorValue(FName InTagPrefix, TFunction<FRotator()> InGet, TFunction<void(const FRotator&)> InSet,
        TFunction<bool()> InIsEnabled) -> TSharedRef<SWidget>;
    auto Present_EditControl(TSharedRef<SWidget> InInput, TAttribute<FText> InReadOnlyText) -> TSharedRef<SWidget>;
    auto Make_NativePort(TSharedRef<SWidget> InLeaf) -> TSharedRef<SBox>;
    auto Detach_NativePorts() -> void;

    FCk_Handle _Entity;
    TSharedPtr<FCkUiView> _View;
    TSharedPtr<FCkInspectorEditGuard> _EditGuard;
    TArray<TSharedPtr<SBox>> _NativePorts;
    TArray<TSharedPtr<FCkInspectorEditScope>> _EditScopes;
    TArray<TSharedPtr<FString>> _SpaceOptions;
    TSet<FString> _DiffLabels;
    FString _LoadError;
    ECk_LocalWorld _EditSpace = ECk_LocalWorld::World;
    FVector _LocationOffset = FVector::ZeroVector;
    FRotator _RotationOffset = FRotator::ZeroRotator;
    FVector _InterpGoalLoc = FVector::ZeroVector;
    FRotator _InterpGoalRot = FRotator::ZeroRotator;
    bool _Active = true, _Mounted = false;
};

class FCkInspector_Transform : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_Transform() override;
    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Transform; }
    auto Get_FeatureFlagId() const -> FName override { return TEXT("Transform"); }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("8B93A1"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 10; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& Entity) -> TSharedRef<SWidget>;
    // Interpolation is an optional sibling feature, so its rows are a STRUCTURAL section: Tick asks for
    // a rebuild only when its presence flips, never per frame.
    bool _HadInterpolation = false;

    // Pending arguments for the edit rows. Shared boxes, not plain members: the row lambdas capture
    // them BY VALUE so a widget that briefly outlives this inspector during panel teardown still reads
    // a live object (the idiom FCkInspector_Probes established for its debug-draw box).
    //
    // _EditSpace is shared by the three setters and the two offset verbs, which is what makes one
    // "Space:" dropdown honest for all of them.
    TSharedRef<ECk_LocalWorld> _EditSpace       = MakeShared<ECk_LocalWorld>(ECk_LocalWorld::World);
    TSharedRef<FVector>        _LocationOffset  = MakeShared<FVector>(FVector::ZeroVector);
    TSharedRef<FRotator>       _RotationOffset  = MakeShared<FRotator>(FRotator::ZeroRotator);
    TSharedRef<FVector>        _InterpGoalLoc   = MakeShared<FVector>(FVector::ZeroVector);
    TSharedRef<FRotator>       _InterpGoalRot   = MakeShared<FRotator>(FRotator::ZeroRotator);
    TArray<TWeakPtr<SCkInspector_TransformAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;
};
