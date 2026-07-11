#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"
#include "CkEcs/Handle/CkHandle.h"
#include "CkEcs/Net/CkNet_Fragment_Data.h"

class FCkDebuggerModel_EntitySelection;
class FCkDebuggerModel_WorldContext;
class FCkDebuggerModel_InspectorFilter;

struct FCkEntityTreeNode
{
    FCk_Handle Entity;
    TArray<TSharedPtr<FCkEntityTreeNode>> Children;
    TWeakPtr<FCkEntityTreeNode> Parent;
    bool IsVisible = true;
    bool IsExpanded = false;

    /**
     * Cached identity — derived ONCE at node creation (rows previously re-derived
     * Get_DebugName + name-clean per row per frame through bound text attributes).
     * ForceFullRefresh re-derives after external renames.
     */
    FString CachedDebugName;
    FString CachedCleanName;
    FText CachedDisplayText;

    /**
     * Classification + signature (redesign spec §3.1/§3.2) — recomputed on structural
     * churn only. Phase 2 renders fold/badges from these; query terms evaluate them.
     */
    uint64 OwnBits = 0;
    uint64 RollupBits = 0;
    bool IsInternal = false;
    FName InternalFeatureId;
    TMap<FName, int32> RollupCounts;
    FString ArchetypeKey;   // registered archetype name, else inferred "Base#signature"
    ECk_Net_EntityNetRole NetRole = ECk_Net_EntityNetRole::None;

    /**
     * Presentation layer (Phase 2). The STreeView renders PresentedChildren — the
     * structural Children after folding internals and coalescing same-archetype sibling
     * runs into synthetic GROUP nodes (IsGroupNode; Entity invalid; Children = members).
     */
    bool IsGroupNode = false;
    FString GroupBaseName;
    TArray<TSharedPtr<FCkEntityTreeNode>> PresentedChildren;

    /** Per-owner ⊞-chip override: true = show this owner's internals despite folding. */
    bool UnfoldInternalsOverride = false;

    /** Folded-internal count for the ⊞ chip (0 = no chip). */
    int32 FoldedInternalCount = 0;

    /**
     * Inspector-filter dim state. Independent of IsVisible — search-text filtering hides
     * non-matches via IsVisible, while inspector filtering dims non-matches via IsFilterMatch.
     * The two flags compose: a row is rendered iff IsVisible, and rendered dimmed iff !IsFilterMatch.
     */
    bool IsFilterMatch = true;

    /**
     * Highlight-text match state. Driven by the Highlight search input — when false,
     * the row is shown but dimmed so matches stand out within the filtered set.
     * Independent of IsVisible (which is driven by the Filter input).
     */
    bool IsSearchMatch = true;
};

class SCkDebuggerWidget_EntityTree : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebuggerWidget_EntityTree) {}
    SLATE_END_ARGS()

    ~SCkDebuggerWidget_EntityTree();

    auto Construct(
        const FArguments& InArgs,
        TSharedPtr<FCkDebuggerModel_EntitySelection> InSelectionModel,
        TSharedPtr<FCkDebuggerModel_WorldContext> InWorldModel,
        TSharedPtr<FCkDebuggerModel_InspectorFilter> InFilterModel) -> void;

    auto Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime) -> void override;

    auto RefreshTree() -> void;

    /** Drops every node and rebuilds from scratch (re-derives cached names). */
    auto ForceFullRefresh() -> void;

    auto ApplyFilter(const FString& InFilterText) -> void;
    auto ApplyHighlight(const FString& InHighlightText) -> void;
    auto ExpandAll() -> void;
    auto CollapseAll() -> void;
    auto Get_CurrentFilter() const -> FText { return FText::FromString(CurrentFilter); }
    auto Get_CurrentFilterString() const -> const FString& { return CurrentFilter; }
    auto Get_CurrentHighlight() const -> FText { return FText::FromString(CurrentHighlight); }

    // ---- Presentation (Phase 2) -------------------------------------------

    struct FTreeCounts
    {
        int32 Total = 0;
        int32 Primaries = 0;
        int32 Internals = 0;
        int32 Shown = 0;
    };

    auto Get_Counts() const -> FTreeCounts;

    auto Get_FoldInternals() const -> bool { return FoldInternals; }
    auto Set_FoldInternals(bool InFold) -> void;
    auto Get_GroupSiblings() const -> bool { return GroupSiblings; }
    auto Set_GroupSiblings(bool InGroup) -> void;

    /** ⊞-chip click: reveal/re-fold one owner's internals. */
    auto ToggleUnfoldOverride(const TSharedPtr<FCkEntityTreeNode>& InNode) -> void;

private:
    auto BuildEntityTree() -> void;
    auto BuildHierarchy(const TArray<FCk_Handle>& InEntities) -> void;
    auto DoCreateNode(const FCk_Handle& InEntity) -> TSharedPtr<FCkEntityTreeNode>;
    auto DoLinkNode(const TSharedPtr<FCkEntityTreeNode>& InNode) -> void;
    auto ApplyCacheDiff(const TArray<FCk_Handle>& InAdded, const TArray<FCk_Handle>& InRemoved) -> void;
    auto RecomputeNodeSignatures() -> void;
    auto ApplyFilterToNodes() -> void;
    auto ApplyInspectorFilter() -> void;
    auto UpdateFilteredRootNodes() -> void;

    /**
     * Fold + coalesce pass: fills every node's PresentedChildren (and FilteredRootNodes)
     * from the structural tree, honoring visibility, folding, and sibling grouping.
     * Group nodes come from GroupNodeCache so their TSharedPtr identity is stable
     * across refreshes (UI contract, spec §4).
     */
    auto RebuildPresentation() -> void;
    auto DoPresentContainer(
        const TArray<TSharedPtr<FCkEntityTreeNode>>& InStructuralChildren,
        const TSharedPtr<FCkEntityTreeNode>& InOwner,
        TArray<TSharedPtr<FCkEntityTreeNode>>& OutPresented) -> void;
    auto ExpandToReveal(const TSharedPtr<FCkEntityTreeNode>& InNode) -> void;
    auto MarkNodeVisibilityRecursive(TSharedPtr<FCkEntityTreeNode> InNode, bool InVisible) -> void;
    auto RestoreSelection(const TArray<FCk_Handle>& InPreviousSelection) -> void;
    auto TrySelectLocallyControlledCharacter() -> void;

    auto OnGetChildren(TSharedPtr<FCkEntityTreeNode> InNode, TArray<TSharedPtr<FCkEntityTreeNode>>& OutChildren) -> void;
    auto OnGenerateRow(TSharedPtr<FCkEntityTreeNode> InNode, const TSharedRef<STableViewBase>& InOwnerTable) -> TSharedRef<ITableRow>;
    auto OnSelectionChanged(TSharedPtr<FCkEntityTreeNode> InNode, ESelectInfo::Type InSelectInfo) -> void;
    auto OnExpansionChanged(TSharedPtr<FCkEntityTreeNode> InNode, bool InIsExpanded) -> void;
    auto OnContextMenuOpening() -> TSharedPtr<SWidget>;

    auto OnExternalSelectionChanged(const TArray<FCk_Handle>& InNewSelection) -> void;

    bool IsUpdatingSelection = false;

    TSharedPtr<STreeView<TSharedPtr<FCkEntityTreeNode>>> TreeView;
    TArray<TSharedPtr<FCkEntityTreeNode>> RootNodes;
    TArray<TSharedPtr<FCkEntityTreeNode>> FilteredRootNodes;
    TArray<TSharedPtr<FCkEntityTreeNode>> AllNodes;
    TMap<FCk_Handle, TSharedPtr<FCkEntityTreeNode>> NodeMap;

    TSharedPtr<FCkDebuggerModel_EntitySelection> SelectionModel;
    TSharedPtr<FCkDebuggerModel_WorldContext> WorldModel;
    TSharedPtr<FCkDebuggerModel_InspectorFilter> FilterModel;
    FDelegateHandle FilterChangedHandle;

    FString CurrentFilter;
    FString CurrentHighlight;
    bool NeedsRefresh = true;
    float TimeSinceLastRefresh = 0.0f;
    static constexpr float RefreshInterval = 0.1f;

    // ---- Presentation state (Phase 2) --------------------------------------
    bool FoldInternals = true;
    bool GroupSiblings = true;
    int32 GroupThreshold = 5;

    /** Stable synthetic group nodes, keyed by (owner ptr, archetype key). */
    TMap<TPair<const FCkEntityTreeNode*, FString>, TSharedPtr<FCkEntityTreeNode>> GroupNodeCache;

    /** Entity node → the group row currently presenting it (for expand-to-reveal). */
    TMap<const FCkEntityTreeNode*, TSharedPtr<FCkEntityTreeNode>> MemberToGroup;
};