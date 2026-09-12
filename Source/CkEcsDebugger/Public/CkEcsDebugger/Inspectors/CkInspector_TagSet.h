#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

#include "Widgets/SCompoundWidget.h"

class FCkUiCollection;
class FCkUiView;
class SBox;

class SCkInspector_TagSetAuthored : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInspector_TagSetAuthored) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
        SLATE_ARGUMENT(FString, Filter)
        SLATE_ARGUMENT(TSharedPtr<SWidget>, AddTagWidget)
        SLATE_ARGUMENT(bool, CountDiffMarked)
        SLATE_ARGUMENT(bool, TagsDiffMarked)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInspector_TagSetAuthored() override;
    virtual auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    auto Release() -> void;
    auto Get_View() const -> TSharedPtr<FCkUiView> { return _View; }
    auto Get_TagsCollection() const -> TSharedPtr<FCkUiCollection> { return _TagsCollection; }
    auto Get_LoadError() const -> const FString& { return _LoadError; }
    auto Is_Mounted() const -> bool { return _Mounted; }
    auto Is_Inert() const -> bool { return NOT _Active; }
    auto Is_CountDiffMarked() const -> bool { return _CountDiffMarked; }
    auto Is_TagsDiffMarked() const -> bool { return _TagsDiffMarked; }
    auto Get_CountText() const -> FString;
    auto Get_IsAvailable() const -> bool;
    auto Get_CanRequest() const -> bool;

private:
    auto Build_AuthoredView() -> bool;
    auto Build_TagRecords() -> bool;
    auto Populate_NativePort() -> bool;
    auto Detach_NativePort() -> void;
    auto Request_RemoveTag(const FString& InStableKey) -> void;

private:
    FCk_Handle _Entity;
    FString _Filter;
    TSharedPtr<SWidget> _AddTagWidget;
    TSharedPtr<SBox> _AddTagPort;
    TSharedPtr<FCkUiCollection> _TagsCollection;
    TSharedPtr<FCkUiView> _View;
    FString _LoadError;
    bool _CountDiffMarked = false;
    bool _TagsDiffMarked = false;
    bool _CountRowVisible = false;
    bool _TagsRowVisible = false;
    bool _Active = true;
    bool _Mounted = false;
};

class FCkInspector_TagSet : public ICkDebuggerComponentInspector_Base
{
public:
    ~FCkInspector_TagSet() override;

    auto Get_ComponentName() const -> FText override;
    auto Get_Icon() const -> ECk_Icon override { return ECk_Icon::Label; }
    auto Get_FeatureColor() const -> TOptional<FLinearColor> override { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("8FA0B8"))); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Build_Inspector(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 25; }
    auto IsFilterable() const -> bool override { return true; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;
    auto OnDeactivated() -> void override;
    auto Get_LastAuthoredLoadError() const -> const FString& { return _LastAuthoredLoadError; }

private:
    auto Build_NativeBody(const FCk_Handle& InEntity, const FString& InFilter) const -> TSharedRef<SWidget>;
    auto Build_NativeAddTagRow(const FCk_Handle& InEntity, const FString& InFilter) const -> TSharedRef<SWidget>;

private:
    TArray<TWeakPtr<SCkInspector_TagSetAuthored>> _AuthoredInstances;
    FString _LastAuthoredLoadError;
};
