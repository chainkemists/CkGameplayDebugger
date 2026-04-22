#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FCkDebuggerModel_EntitySelection;
class FCkDebuggerModel_WorldContext;
class FCkDebuggerModel_InspectorFilter;
class SBox;
class SCkDebug_DualSearchBar;
class SCkDebuggerWidget_EntityTree;

class SCkDebuggerPanel_EntityList : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebuggerPanel_EntityList) {}
    SLATE_END_ARGS()

    auto Construct(
        const FArguments& InArgs,
        TSharedPtr<FCkDebuggerModel_EntitySelection> InSelectionModel,
        TSharedPtr<FCkDebuggerModel_WorldContext> InWorldModel,
        TSharedPtr<FCkDebuggerModel_InspectorFilter> InFilterModel) -> void;

    auto Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime) -> void override;

private:
    auto Build_WorldSelector() -> TSharedRef<SWidget>;
    auto Build_Toolbar() -> TSharedRef<SWidget>;
    auto Build_StatusBar() -> TSharedRef<SWidget>;

    auto OnFilterTextChanged(const FString& InText) -> void;
    auto OnHighlightTextChanged(const FString& InText) -> void;
    auto OnRefreshClicked() -> FReply;
    auto OnExpandAllClicked() -> FReply;
    auto OnCollapseAllClicked() -> FReply;
    auto OnWorldButtonClicked(TWeakObjectPtr<UWorld> InWorldWeak) -> FReply;
    auto OnCopyableTextRightClicked(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, FString InText) -> FReply;

    auto Get_EntityCountText() const -> FText;
    auto Get_SelectionCountText() const -> FText;

    TSharedPtr<SCkDebug_DualSearchBar> SearchBar;
    TSharedPtr<SCkDebuggerWidget_EntityTree> EntityTree;
    TSharedPtr<SBox> WorldSelectorContainer;

    TSharedPtr<FCkDebuggerModel_EntitySelection> SelectionModel;
    TSharedPtr<FCkDebuggerModel_WorldContext> WorldModel;

    TArray<TWeakObjectPtr<UWorld>> LastKnownWorlds;
    float TimeSinceWorldCheck = 0.0f;
    static constexpr float WorldCheckInterval = 1.0f;
};
