#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "Widgets/SCompoundWidget.h"

class FCkUiCollection;
class FCkUiView;
class SBox;

class SCkInspector_EntityTagAuthored : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_EntityTagAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(FString, Filter)
        SLATE_ARGUMENT(TSharedPtr<SWidget>, AddNameWidget)
        SLATE_ARGUMENT(TSharedPtr<SWidget>, AddGameplayTagWidget)
        SLATE_ARGUMENT(bool, FNameCountDiffMarked)
        SLATE_ARGUMENT(bool, FNameTagsDiffMarked)
        SLATE_ARGUMENT(bool, GameplayTagRootCountDiffMarked)
        SLATE_ARGUMENT(bool, GameplayTagRootsDiffMarked)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_EntityTagAuthored() override;
    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_FNameTagsCollection() const -> TSharedPtr<FCkUiCollection> { return _FNameTagsCollection; }
    auto Get_GameplayTagRootsCollection() const -> TSharedPtr<FCkUiCollection> { return _GameplayTagRootsCollection; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }
    auto Get_IsAvailable() const -> bool;
    auto Get_FNameCountText() const -> FString;
    auto Get_GameplayTagRootCountText() const -> FString;
    auto Is_FNameCountDiffMarked() const -> bool { return _FNameCountDiffMarked; }
    auto Is_FNameTagsDiffMarked() const -> bool { return _FNameTagsDiffMarked; }
    auto Is_GameplayTagRootCountDiffMarked() const -> bool { return _GameplayTagRootCountDiffMarked; }
    auto Is_GameplayTagRootsDiffMarked() const -> bool { return _GameplayTagRootsDiffMarked; }

private:
    auto Build_Records() -> bool;
    auto Refresh_Records() -> bool;
    auto Build_AuthoredView() -> bool;
    auto Populate_NativePorts() -> bool;
    auto Detach_NativePorts() -> void;
    auto Request_RemoveName(const FString& InStableKey) -> void;
    auto Request_RemoveGameplayTagRoot(const FString& InStableKey) -> void;

private:
    FCk_Handle _Entity;
    FString _Filter;
    TSharedPtr<SWidget> _AddNameWidget;
    TSharedPtr<SWidget> _AddGameplayTagWidget;
    TSharedPtr<SBox> _AddNamePort;
    TSharedPtr<SBox> _AddGameplayTagPort;
    TSharedPtr<FCkUiCollection> _FNameTagsCollection;
    TSharedPtr<FCkUiCollection> _GameplayTagRootsCollection;
    TSharedPtr<FCkUiView> _View;
    FString _LoadError;
    bool _FNameCountDiffMarked = false;
    bool _FNameTagsDiffMarked = false;
    bool _GameplayTagRootCountDiffMarked = false;
    bool _GameplayTagRootsDiffMarked = false;
    bool _FNameCountRowVisible = false;
    bool _FNameTagsRowVisible = false;
    bool _GameplayTagRootCountRowVisible = false;
    bool _GameplayTagRootsRowVisible = false;
    TSet<FString> _FNameDiffMarkedKeys;
    TSet<FString> _GameplayTagRootDiffMarkedKeys;
    bool _Active = true;
    bool _Mounted = false;
};

class FCkInspector_EntityTag : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_EntityTag() override;
    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Label; }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("8FA0B8"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Build_Inspector(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 26; }
    auto IsFilterable() const -> bool override { return true; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& InEntity, const FString& InFilter) const -> TSharedRef<SWidget>;
    auto Build_NativeAddNameRow(const FCk_Handle& InEntity, const FString& InFilter) const -> TSharedRef<SWidget>;
    auto Build_NativeAddGameplayTagRow(const FCk_Handle& InEntity, const FString& InFilter) const -> TSharedRef<SWidget>;

private:
    TArray<TWeakPtr<SCkInspector_EntityTagAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;
};
