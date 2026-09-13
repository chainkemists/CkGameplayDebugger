#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "Widgets/SCompoundWidget.h"

class FCkUiView;
class FCkInspectorEditScope;
class SBox;

class SCkInspector_ShapesAuthored final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_ShapesAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(TSharedPtr<FCkInspectorEditGuard>, EditGuard)
        SLATE_ARGUMENT(TSet<FString>, DiffLabels)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_ShapesAuthored() override;
    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }
    auto Get_IsAvailable() const -> bool;
    auto Get_HasSphere() const -> bool;
    auto Get_HasBox() const -> bool;
    auto Get_HasCapsule() const -> bool;
    auto Get_HasCylinder() const -> bool;
    auto Get_CanRequest() const -> bool;
    auto Get_RequestDisabledReason() const -> FString;
    auto Get_SphereRadius() const -> float;
    auto Get_BoxHalfExtents() const -> FVector;
    auto Get_BoxConvexRadius() const -> float;
    auto Get_CapsuleHalfHeight() const -> float;
    auto Get_CapsuleRadius() const -> float;
    auto Get_CylinderHalfHeight() const -> float;
    auto Get_CylinderRadius() const -> float;
    auto Get_CylinderConvexRadius() const -> float;
    auto Commit_SphereRadius(float InValue) -> void;
    auto Commit_BoxHalfExtents(const FVector& InValue) -> void;
    auto Commit_BoxConvexRadius(float InValue) -> void;
    auto Commit_CapsuleHalfHeight(float InValue) -> void;
    auto Commit_CapsuleRadius(float InValue) -> void;
    auto Commit_CylinderHalfHeight(float InValue) -> void;
    auto Commit_CylinderRadius(float InValue) -> void;
    auto Commit_CylinderConvexRadius(float InValue) -> void;
    auto Is_DiffMarked(const FString& InLabel) const -> bool { return _DiffLabels.Contains(InLabel); }

private:
    auto Build_AuthoredView() -> bool;
    auto Build_Number(
        FName InTag,
        TFunction<float()> InGetter,
        TFunction<void(float)> InCommit,
        TFunction<bool()> InIsEnabled,
        bool bClampNonNegative = true) -> TSharedRef<SWidget>;
    auto Build_BoxHalfExtentsValue() -> TSharedRef<SWidget>;
    auto Make_NativePort(TSharedRef<SWidget> InLeaf) -> TSharedRef<SBox>;
    auto Detach_NativePorts() -> void;

private:
    FCk_Handle _Entity;
    TSharedPtr<FCkUiView> _View;
    TSharedPtr<FCkInspectorEditGuard> _EditGuard;
    TArray<TSharedPtr<SBox>> _NativePorts;
    TArray<TSharedPtr<FCkInspectorEditScope>> _EditScopes;
    TSet<FString> _DiffLabels;
    FString _LoadError;
    bool _Active = true;
    bool _Mounted = false;
};

class FCkInspector_Shapes : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_Shapes() override;

    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Shapes; }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 130; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& InEntity) const -> TSharedRef<SWidget>;

private:
    TArray<TWeakPtr<SCkInspector_ShapesAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;
};
