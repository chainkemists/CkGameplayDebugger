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

    /**
     * Depth transparency (design 2026-08-09). True when this node's LITERAL lifetime owner
     * is a depth-transparent ActorRelay the user has not revealed — the node links to the
     * roots instead of nesting, and RelayOwner keeps the literal owner for the row
     * affordance and the reveal action. Linking only: the node is never duplicated or
     * recreated, so selection/expansion survive a reveal toggle (UI contract §4).
     */
    bool HoistedFromRelay = false;
    FCk_Handle RelayOwner;

    /** On a relay node: how many of its children are currently hoisted away (0 = none). */
    int32 HoistedChildCount = 0;

    /** Per-owner ⊞-chip override: true = show this owner's internals despite folding. */
    bool UnfoldInternalsOverride = false;

    /** Folded-internal count for the ⊞ chip (0 = no chip). */
    int32 FoldedInternalCount = 0;

    /** Spawn-flash deadline (FPlatformTime::Seconds) — status dot reads green until then. */
    double FlashUntilSeconds = 0.0;

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

    /**
     * Whether this node itself matched the active narrowing (Filter query + rail).
     * Ancestors of a match stay visible to keep the path readable, but with this flag
     * false they render dimmed — the eye lands on the actual matches.
     */
    bool IsNarrowingMatch = true;
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

    // F — frame the primary selection in the ejected editor viewport.
    auto OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) -> FReply override;

    auto RefreshTree() -> void;

    /** Drops every node and rebuilds from scratch (re-derives cached names). */
    auto ForceFullRefresh() -> void;

    /** Drop all handles owned by the tree without querying the old registry. */
    auto Reset_ForWorldChange() -> void;

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

    // ---- Relay depth transparency (P2) --------------------------------------
    // Per-relay escape hatch out of the hoisting default. Revealing is presentation only:
    // it re-nests the relay's children without touching classification or rollups.

    auto Get_IsRelayRevealed(const FCk_Handle& InRelay) const -> bool;
    auto Set_RelayRevealed(const TArray<FCk_Handle>& InRelays, bool InRevealed) -> void;

    // ---- Feature rail (Phase 3) --------------------------------------------
    // Included features compose with the query: entity passes when it carries ANY
    // included feature (own ∪ rollup). Empty set = rail off.

    auto Get_RailIncluded() const -> const TSet<FName>& { return RailIncluded; }
    auto Toggle_RailFeature(FName InFeatureId) -> void;

    // ---- Pinned (Phase 3) ---------------------------------------------------

    auto Get_PinnedEntities() const -> const TArray<FCk_Handle>& { return PinnedEntities; }
    auto Get_IsPinned(const FCk_Handle& InEntity) const -> bool;
    auto TogglePin(const FCk_Handle& InEntity) -> void;

    DECLARE_MULTICAST_DELEGATE(FOnPinsChanged);
    FOnPinsChanged OnPinsChanged;

private:
    auto BuildEntityTree() -> void;
    auto BuildHierarchy(const TArray<FCk_Handle>& InEntities) -> void;
    auto DoCreateNode(const FCk_Handle& InEntity) -> TSharedPtr<FCkEntityTreeNode>;
    /** Re-link current nodes after owner changes without replacing their Slate identities. */
    auto RebuildHierarchyLinks() -> void;
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

    TSet<FName> RailIncluded;
    TArray<FCk_Handle> PinnedEntities;

    /**
     * Relays the user asked to see nested. Holds registry handles, so it MUST NOT outlive
     * the PIE session — cleared in Reset_ForWorldChange, which the main window drives from
     * both OnWorldChanged and the common session-invalidation signal (EndPIE).
     */
    TSet<FCk_Handle> RevealedRelays;
};
