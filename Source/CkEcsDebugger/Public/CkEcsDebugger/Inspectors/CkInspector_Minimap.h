#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"
#include "Widgets/SCompoundWidget.h"

class FCkUiView;
class FCkInspectorEditScope;

class SCkInspector_MinimapAuthored final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_MinimapAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(TSharedPtr<FCkInspectorEditGuard>, EditGuard)
        SLATE_ARGUMENT(TSet<FString>, DiffLabels)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_MinimapAuthored() override;
    virtual auto Tick(const FGeometry&, double, float) -> void override;
    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }
    auto Get_IsAvailable() const -> bool;
    auto Get_ProjectionText() const -> FString;
    auto Get_RotationText() const -> FString;
    auto Get_FrameText() const -> FString;
    auto Get_ViewExtent() const -> float;
    auto Get_ViewOriginText() const -> FString;
    auto Get_ViewYawText() const -> FString;
    auto Get_Observer() const -> FCk_Handle;
    auto Get_EntriesText() const -> FString;
    auto Get_EntriesFraction() const -> float;
    auto Get_FixedBoundsText() const -> FString;
    auto Is_DiffMarked(const FString& InLabel) const -> bool { return _DiffLabels.Contains(InLabel); }
    auto Commit_Rotation(int32 InIndex) -> void;
    auto Commit_ViewExtent(float InValue) -> void;

private:
    auto Build_AuthoredView() -> bool;
    auto Build_RotationValue() -> TSharedRef<SWidget>;
    auto Build_ViewExtentValue() -> TSharedRef<SWidget>;

    FCk_Handle _Entity;
    TSharedPtr<FCkUiView> _View;
    TSharedPtr<FCkInspectorEditScope> _ViewExtentEditScope;
    TSet<FString> _DiffLabels;
    TArray<TSharedPtr<FString>> _RotationOptions;
    FString _LoadError;
    bool _Active = true;
    bool _Mounted = false;
};

class FCkInspector_Minimap final : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_Minimap() override;
    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Minimap; }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 65; }
    auto Tick(const FCk_Handle&, float) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& InEntity) const -> TSharedRef<SWidget>;
    TArray<TWeakPtr<SCkInspector_MinimapAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;
};
