#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "CkEcs/Handle/CkHandle.h"

class FCkDebuggerModel_EntitySelection;
class FCkDebuggerModel_WorldContext;
class FCkDebuggerModel_InspectorFilter;
class SCkDebug_DualSearchBar;
class SCkDebuggerWidget_EntityTree;
class SVerticalBox;

class SCkDebuggerPanel_EntityList : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebuggerPanel_EntityList) {}
    SLATE_END_ARGS()

    ~SCkDebuggerPanel_EntityList();

    auto Construct(
        const FArguments& InArgs,
        TSharedPtr<FCkDebuggerModel_EntitySelection> InSelectionModel,
        TSharedPtr<FCkDebuggerModel_WorldContext> InWorldModel,
        TSharedPtr<FCkDebuggerModel_InspectorFilter> InFilterModel) -> void;

    /** Push a query into the Filter bar (Archetypes-page card click-through). */
    auto Set_FilterText(const FString& InText) -> void;

    /** Current Filter bar text — archetype cards derive toggled state from it. */
    auto Get_FilterText() const -> FString;

    /** Drop all quick-access and tree handles owned by the previous world. */
    auto Reset_ForWorldChange() -> void;

    /**
     * The Layer-B style selection changed structurally. Routed from the window; regenerates the
     * quick-access rows, whose per-entity composition (row banding index, EntityRef pill) is a
     * build-time decision. The tree owns its own revision poll and the feature rails are entirely
     * attribute-bound, so neither is touched here.
     */
    auto Notify_StyleRevisionChanged() -> void;

private:
    auto Build_Toolbar() -> TSharedRef<SWidget>;
    auto Build_StatusBar() -> TSharedRef<SWidget>;
    // The feature-filter chips flank the tree on BOTH sides (whole groups split
    // deterministically by weight) so a tall window shows them all without
    // scrolling; each flank still scrolls independently when the window is short.
    auto Build_FeatureRail(bool InRightFlank) -> TSharedRef<SWidget>;
    auto Build_QueryHelpButton() -> TSharedRef<SWidget>;

    /** Pinned quick-access section (Phase 3) — rebuilt on change, not bound. */
    auto RefreshQuickAccessSections() -> void;
    auto DoBuildQuickAccessRows(
        const TArray<FCk_Handle>& InEntities,
        const TSharedPtr<SVerticalBox>& InContainer) -> void;

    auto OnFilterTextChanged(const FString& InText) -> void;
    auto OnHighlightTextChanged(const FString& InText) -> void;
    auto OnRefreshClicked() -> FReply;
    auto OnExpandAllClicked() -> FReply;
    auto OnCollapseAllClicked() -> FReply;
    auto Get_EntityCountText() const -> FText;
    auto Get_SelectionCountText() const -> FText;

    TSharedPtr<SCkDebug_DualSearchBar> SearchBar;
    TSharedPtr<SCkDebuggerWidget_EntityTree> EntityTree;
    TSharedPtr<SVerticalBox> PinnedSectionBox;

    TSharedPtr<FCkDebuggerModel_EntitySelection> SelectionModel;
    TSharedPtr<FCkDebuggerModel_WorldContext> WorldModel;

    FDelegateHandle PinsChangedHandle;
};
