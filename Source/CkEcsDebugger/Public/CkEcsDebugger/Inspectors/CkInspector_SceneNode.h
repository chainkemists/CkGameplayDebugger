#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SWrapBox.h"

class FCkUiView;
class FCkUiCollection;
class FCkInspectorEditScope;
class FCkDebuggerModel_EntitySelection;

class SCkInspector_SceneNodeAuthored final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_SceneNodeAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(TSharedPtr<FCkInspectorEditGuard>, EditGuard)
        SLATE_ARGUMENT(TWeakPtr<FCkDebuggerModel_EntitySelection>, SelectionModel)
        SLATE_ARGUMENT(TSet<FString>, DiffLabels)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_SceneNodeAuthored() override;
    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;
    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_SiblingsCollection() const -> TSharedPtr<FCkUiCollection> { return _Siblings; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }
    auto Get_IsAvailable() const -> bool;
    auto Get_Parent() const -> FCk_Handle;
    auto Get_LayerText() const -> FString;
    auto Get_DirtyText() const -> FString;
    auto Get_RelativeTransform() const -> FTransform;
    auto Get_ResolvedTransform() const -> FTransform;
    auto Get_CanRequest() const -> bool;
    auto Get_RequestDisabledReason() const -> FString;
    auto Commit_Location(const FVector& InLocation) -> void;
    auto Commit_Rotation(const FRotator& InRotation) -> void;
    auto Commit_Scale(const FVector& InScale) -> void;
    auto Request_Detach() -> void;
    auto Navigate_Sibling(const FString& InStableKey) -> void;
    auto Is_DiffMarked(const FString& InLabel) const -> bool { return _DiffLabels.Contains(InLabel); }

private:
    auto Build_AuthoredView() -> bool;
    auto Build_VectorPort(FName InTag, TFunction<FVector()> InGetter, TFunction<void(const FVector&)> InCommit) -> TSharedRef<SWidget>;
    auto Build_RotatorPort() -> TSharedRef<SWidget>;
    auto Refresh_Siblings() -> bool;

private:
    FCk_Handle _Entity;
    TSharedPtr<FCkUiView> _View;
    TSharedPtr<FCkUiCollection> _Siblings;
    TArray<TSharedPtr<FCkInspectorEditScope>> _EditScopes;
    TSet<FString> _DiffLabels;
    TSharedPtr<FCkInspectorEditGuard> _EditGuard;
    TWeakPtr<FCkDebuggerModel_EntitySelection> _SelectionModel;
    TMap<FString, FCk_Handle> _SiblingsByKey;
    FString _LoadError;
    bool _Active = true;
    bool _Mounted = false;
};

class FCkInspector_SceneNode : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_SceneNode() override;
    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::SceneNode; }
    auto Get_FeatureFlagId() const -> FName override { return TEXT("SceneNode"); }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("199E70"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 22; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& InEntity) -> TSharedRef<SWidget>;
    TSharedPtr<SWrapBox> _SiblingsBox;
    int32 _LastSiblingCount = -1;
    TArray<TWeakPtr<SCkInspector_SceneNodeAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;

};
