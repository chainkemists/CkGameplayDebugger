#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "CkEcs/Handle/CkHandle.h"

class SCkDebuggerViewBase;
class SCkDebuggerSidebar;
class SCkDebuggerToolbar;
class UWorld;

class SCkSlateDebuggerWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkSlateDebuggerWindow) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

    auto GetSelectedWorld() const -> UWorld*;
    auto SetSelectedWorld(UWorld* InWorld) -> void;

    auto GetSelectedEntities() const -> const TArray<FCk_Handle>&;
    auto SetSelectedEntities(const TArray<FCk_Handle>& InEntities) -> void;
    auto AddSelectedEntity(const FCk_Handle& InEntity) -> void;
    auto RemoveSelectedEntity(const FCk_Handle& InEntity) -> void;
    auto ClearSelectedEntities() -> void;

    auto SetActiveView(const FName& ViewName) -> void;
    auto GetActiveView() const -> TSharedPtr<SCkDebuggerViewBase>;

    auto RefreshCurrentView() -> void;
    auto RefreshWorldOptions() -> void;

private:
    auto CreateViews() -> void;
    auto RegisterView(const FName& ViewName, TSharedRef<SCkDebuggerViewBase> View) -> void;

    auto OnWorldChanged() -> void;
    auto OnSelectionChanged() -> void;

    auto BuildWorldSelector() -> TSharedRef<SWidget>;
    auto GetWorldSelectorText() const -> FText;

private:
    TSharedPtr<SCkDebuggerSidebar> Sidebar;
    TSharedPtr<SCkDebuggerToolbar> Toolbar;
    TSharedPtr<SBorder> ViewContainer;
    TSharedPtr<SComboBox<TWeakObjectPtr<UWorld>>> WorldComboBox;

    TMap<FName, TSharedRef<SCkDebuggerViewBase>> Views;
    TSharedPtr<SCkDebuggerViewBase> ActiveView;
    FName ActiveViewName;

    TWeakObjectPtr<UWorld> SelectedWorld;
    TArray<TWeakObjectPtr<UWorld>> WorldOptions;
    TArray<FCk_Handle> SelectedEntities;
    TArray<FCk_Handle> PreviousSelectedEntities;

    double LastUpdateTime = 0.0;
    static constexpr float UpdateInterval = 0.1f; // 100ms update rate
};