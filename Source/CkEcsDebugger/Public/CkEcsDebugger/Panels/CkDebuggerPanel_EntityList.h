#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FCkDebuggerModel_EntitySelection;
class FCkDebuggerModel_WorldContext;
class SBox;
class SCkDebuggerWidget_SearchBar;
class SCkDebuggerWidget_EntityTree;

class SCkDebuggerPanel_EntityList : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebuggerPanel_EntityList) {}
    SLATE_END_ARGS()

    auto Construct(
        const FArguments& InArgs,
        TSharedPtr<FCkDebuggerModel_EntitySelection> InSelectionModel,
        TSharedPtr<FCkDebuggerModel_WorldContext> InWorldModel) -> void;

    auto Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime) -> void override;

private:
    auto Build_WorldSelector() -> TSharedRef<SWidget>;
    auto Build_Toolbar() -> TSharedRef<SWidget>;
    auto Build_StatusBar() -> TSharedRef<SWidget>;

    auto OnSearchTextChanged(const FString& InSearchText) -> void;
    auto OnRefreshClicked() -> FReply;
    auto OnExpandAllClicked() -> FReply;
    auto OnCollapseAllClicked() -> FReply;
    auto OnWorldButtonClicked(TWeakObjectPtr<UWorld> InWorldWeak) -> FReply;

    auto Get_EntityCountText() const -> FText;
    auto Get_SelectionCountText() const -> FText;

    TSharedPtr<SCkDebuggerWidget_SearchBar> SearchBar;
    TSharedPtr<SCkDebuggerWidget_EntityTree> EntityTree;
    TSharedPtr<SBox> WorldSelectorContainer;

    TSharedPtr<FCkDebuggerModel_EntitySelection> SelectionModel;
    TSharedPtr<FCkDebuggerModel_WorldContext> WorldModel;

    int32 LastKnownWorldCount = -1;
    float TimeSinceWorldCheck = 0.0f;
    static constexpr float WorldCheckInterval = 1.0f;
};
