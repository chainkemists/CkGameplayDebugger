#include "CkDebuggerWidget_EntityTree.h"

#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SNullWidget.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"

#include "CkDebuggerCommon/Navigation/CkDebug_SelectionSync.h"
#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"
#include "CkDebuggerCommon/Utils/CkDebug_NameClean_Utils.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"

#include "CkCore/Algorithms/CkAlgorithms.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Archetype/CkArchetype_Registry.h"
#include "CkEcs/DebugFeatureFlags/CkDebugFeatureFlags.h"
#include "CkEcs/Net/CkNet_Utils.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"
#include "CkEcsDebugger/Classification/CkEcsDebugger_Classification.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_WorldContext.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_InspectorFilter.h"
#include "CkEcsDebugger/Presentation/CkEcsDebugger_FeatureVisuals.h"
#include "CkEcsDebugger/Query/CkEcsDebugger_Query.h"
#include "CkEcsDebugger/Settings/CkEcsDebuggerSettings.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"

#include "Widgets/Input/SCheckBox.h"

namespace ck_debugger_entity_tree
{
    // Shorthand into the shared presentation metadata (also used by the rail + Overview).
    using ck::ecs_debugger_feature_visuals::Get_FeatureVisuals;
    using ck::ecs_debugger_feature_visuals::Get_BadgeFeatures;
    constexpr auto MaxBadges = ck::ecs_debugger_feature_visuals::MaxBadges;

    // Feature-token table for the query grammar: flag ids + wired inspectors' display
    // names, both prefix-matchable (spec §3.5). Built once — registration is startup-only.
    static auto Get_QueryTokenTable() -> const ck::ecs_debugger_query::FFeatureTokenTable&
    {
        static const auto Table = []() -> ck::ecs_debugger_query::FFeatureTokenTable
        {
            auto Result = ck::ecs_debugger_query::FFeatureTokenTable{};

            for (const auto& [FeatureId, Bit] : Get_BadgeFeatures())
            {
                Result.Add(FeatureId.ToString(), uint64{1} << Bit);
            }

            for (const auto& Metadata : FCkDebuggerInspectorRegistry::Get().Get_AllMetadata())
            {
                if (Metadata.FeatureFlagId.IsNone())
                { continue; }

                const auto Bit = ck::debug_feature_flags::Get_BitIndex(Metadata.FeatureFlagId);
                if (Bit != INDEX_NONE)
                { Result.Add(Metadata.DisplayName.ToString(), uint64{1} << Bit); }
            }

            return Result;
        }();

        return Table;
    }

    static auto Make_QueryContext(const FCkEntityTreeNode& InNode) -> ck::ecs_debugger_query::FEntityQueryContext
    {
        using namespace ck::ecs_debugger_query;

        auto Context = FEntityQueryContext{};
        Context.OwnBits = InNode.OwnBits;
        Context.RollupBits = InNode.RollupBits;
        Context.IsInternal = InNode.IsInternal;
        Context.NetMode = InNode.NetRole == ECk_Net_EntityNetRole::Authority ? EQueryNetMode::Authority
                        : InNode.NetRole == ECk_Net_EntityNetRole::Proxy     ? EQueryNetMode::Proxy
                        : EQueryNetMode::None;
        Context.EntityId = static_cast<uint32>(InNode.Entity.Get_Entity().Get_ID());
        Context.ArchetypeName = InNode.ArchetypeKey.ToLower();
        Context.DisplayName = InNode.CachedDebugName;
        return Context;
    }
}

class SCkDebuggerEntityTreeRow : public STableRow<TSharedPtr<FCkEntityTreeNode>>
{
public:
    SLATE_BEGIN_ARGS(SCkDebuggerEntityTreeRow) {}
        SLATE_ARGUMENT(TSharedPtr<FCkEntityTreeNode>, Node)
        SLATE_ARGUMENT(TSharedPtr<FCkDebuggerModel_EntitySelection>, SelectionModel)
        SLATE_ARGUMENT(TWeakPtr<SCkDebuggerWidget_EntityTree>, TreeWidget)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable) -> void
    {
        Node = InArgs._Node;
        SelectionModel = InArgs._SelectionModel;
        TreeWidget = InArgs._TreeWidget;

        const auto IsGroup = Node.IsValid() && Node->IsGroupNode;

        auto RowContent = SNew(SHorizontalBox);

        // Status dot (entity rows only — groups have no live/dead state).
        if (NOT IsGroup)
        {
            RowContent->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f)
            [
                SNew(SImage)
                .Image(FAppStyle::GetBrush("Icons.FilledCircle"))
                .ColorAndOpacity(this, &SCkDebuggerEntityTreeRow::Get_EntityStatusColor)
                .DesiredSizeOverride(FVector2D(6.0f, 6.0f))
            ];
        }

        // Identity glyph — one icon language across the debugger (spec §2.4): internals
        // wear their single feature's glyph, primaries the generic cube, groups the
        // representative member's glyph.
        if (const auto* IdentityBrush = Get_IdentityBrush(); IdentityBrush != nullptr)
        {
            RowContent->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f)
            [
                SNew(SImage)
                .Image(IdentityBrush)
                .ColorAndOpacity(Get_IdentityColor())
                .DesiredSizeOverride(FVector2D(14.0f, 14.0f))
            ];
        }

        RowContent->AddSlot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Text(this, &SCkDebuggerEntityTreeRow::Get_NodeDisplayText)
            .ColorAndOpacity(this, &SCkDebuggerEntityTreeRow::Get_NodeTextColor)
            .HighlightText(this, &SCkDebuggerEntityTreeRow::Get_HighlightText)
        ];

        // ⊞ chip — folded-internals count; bounded click target per the STableRow
        // contract (only the chip traps the click, the row still selects elsewhere).
        RowContent->AddSlot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(FCkDebuggerStyle::Padding_Small, 0.0f, 0.0f, 0.0f)
        [
            SNew(SButton)
            .ButtonStyle(FAppStyle::Get(), "SimpleButton")
            .Visibility(this, &SCkDebuggerEntityTreeRow::Get_FoldChipVisibility)
            .ToolTipText(FText::FromString(TEXT("Folded internal entities - click to show/hide them under this row")))
            .ContentPadding(FMargin(2.0f, 0.0f))
            .OnClicked(this, &SCkDebuggerEntityTreeRow::OnFoldChipClicked)
            [
                SNew(STextBlock)
                .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Normal"))
                .Text(this, &SCkDebuggerEntityTreeRow::Get_FoldChipText)
                .ColorAndOpacity(CkStyle::TextDim())
            ]
        ];

        // Badge strip — solid = own features, hollow+count = rolled-up internals
        // (spec §3.2 rendering). Built once per row generation; rows regenerate on
        // every tree refresh, which follows every signature recompute.
        RowContent->AddSlot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(FCkDebuggerStyle::Padding_Small, 0.0f, 0.0f, 0.0f)
        [
            BuildBadgeStrip()
        ];

        RowContent->AddSlot()
        .FillWidth(1.0f)
        [
            SNew(SBox)
        ];

        // Canonical entity identity — the common pill instead of a raw [ID] STextBlock,
        // so the tree matches every other debugger surface (right-click → Copy
        // included). The pill only traps clicks within its own bounds; the rest of the
        // row still selects normally. Group rows carry no single entity → no pill.
        if (NOT IsGroup)
        {
            RowContent->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FCkDebuggerStyle::Padding_Small, 0.0f, 0.0f, 0.0f)
            [
                SNew(SCkDebug_EntityRef)
                .Entity_Lambda([WeakNode = TWeakPtr<FCkEntityTreeNode>(InArgs._Node)]() -> FCk_Handle
                {
                    const auto Pinned = WeakNode.Pin();
                    return Pinned.IsValid() ? Pinned->Entity : FCk_Handle{};
                })
            ];
        }

        STableRow<TSharedPtr<FCkEntityTreeNode>>::Construct(
            STableRow<TSharedPtr<FCkEntityTreeNode>>::FArguments()
            .Style(&FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row"))
            .Padding(FMargin(FCkDebuggerStyle::Padding_Small))
            .ShowSelection(true)
            .Content()
            [
                SNew(SBox)
                .Padding(FMargin(FCkDebuggerStyle::Padding_Small, 2.0f))
                [
                    RowContent
                ]
            ],
            InOwnerTable
        );
    }

private:
    auto Get_NodeDisplayText() const -> FText
    {
        if (NOT Node.IsValid())
        { return FText::GetEmpty(); }

        if (Node->IsGroupNode)
        {
            return FText::FromString(FString::Printf(TEXT("%s x%d"),
                *Node->GroupBaseName, Node->Children.Num()));
        }

        // Cached at node creation — bound attributes poll every frame, so deriving
        // Get_DebugName + name-clean here was a per-row per-frame hot path.
        return Node->CachedDisplayText;
    }

    auto Get_RepresentativeNode() const -> TSharedPtr<FCkEntityTreeNode>
    {
        if (NOT Node.IsValid())
        { return nullptr; }

        if (Node->IsGroupNode)
        { return Node->Children.IsEmpty() ? nullptr : Node->Children[0]; }

        return Node;
    }

    auto Get_IdentityBrush() const -> const FSlateBrush*
    {
        const auto Representative = Get_RepresentativeNode();
        if (NOT Representative.IsValid())
        { return nullptr; }

        if (Representative->IsInternal)
        {
            if (const auto* Visual = ck_debugger_entity_tree::Get_FeatureVisuals().Find(Representative->InternalFeatureId))
            {
                if (const auto* Brush = FCkDebuggerStyle::Get_IconBrush(Visual->IconName))
                { return Brush; }
            }
        }

        return FCkDebuggerStyle::Get_IconBrush(TEXT("Cube"));
    }

    auto Get_IdentityColor() const -> FSlateColor
    {
        const auto Representative = Get_RepresentativeNode();
        if (Representative.IsValid() && Representative->IsInternal)
        {
            if (const auto* Visual = ck_debugger_entity_tree::Get_FeatureVisuals().Find(Representative->InternalFeatureId))
            { return Visual->Color; }
        }

        return CkStyle::TextDim();
    }

    auto Get_FoldChipVisibility() const -> EVisibility
    {
        return Node.IsValid() && NOT Node->IsGroupNode && Node->FoldedInternalCount > 0
            ? EVisibility::Visible
            : EVisibility::Collapsed;
    }

    auto Get_FoldChipText() const -> FText
    {
        if (NOT Node.IsValid())
        { return FText::GetEmpty(); }

        // ASCII on purpose — decorated text glyphs render as tofu boxes in the editor
        // font (the long-standing chevron complaint). "+3" = 3 internals hidden.
        const auto Symbol = Node->UnfoldInternalsOverride ? TEXT("-") : TEXT("+");
        return FText::FromString(FString::Printf(TEXT("%s%d"), Symbol, Node->FoldedInternalCount));
    }

    auto OnFoldChipClicked() const -> FReply
    {
        if (const auto Tree = TreeWidget.Pin(); Tree.IsValid() && Node.IsValid())
        { Tree->ToggleUnfoldOverride(Node); }

        return FReply::Handled();
    }

    auto BuildBadgeStrip() const -> TSharedRef<SWidget>
    {
        const auto Representative = Get_RepresentativeNode();
        if (NOT Representative.IsValid())
        { return SNullWidget::NullWidget; }

        auto Strip = SNew(SHorizontalBox);
        auto BadgeCount = 0;
        auto OverflowCount = 0;

        const auto AddBadge = [&](const FName InFeatureId, const int32 InRollupCount) -> void
        {
            if (BadgeCount >= ck_debugger_entity_tree::MaxBadges)
            {
                ++OverflowCount;
                return;
            }

            const auto* Visual = ck_debugger_entity_tree::Get_FeatureVisuals().Find(InFeatureId);
            const auto* Brush = Visual != nullptr ? FCkDebuggerStyle::Get_IconBrush(Visual->IconName) : nullptr;
            if (Brush == nullptr)
            { return; }

            const auto IsRollup = InRollupCount > 0;
            const auto Tint = Visual->Color.CopyWithNewOpacity(IsRollup ? 0.45f : 1.0f);
            const auto Tooltip = IsRollup
                ? FString::Printf(TEXT("%s ×%d (rolled up from internals)"), *InFeatureId.ToString(), InRollupCount)
                : InFeatureId.ToString();

            auto BadgeBox = SNew(SHorizontalBox)
                .ToolTipText(FText::FromString(Tooltip));

            BadgeBox->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SImage)
                .Image(Brush)
                .ColorAndOpacity(Tint)
                .DesiredSizeOverride(FVector2D(12.0f, 12.0f))
            ];

            if (IsRollup)
            {
                BadgeBox->AddSlot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(1.0f, 0.0f, 0.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Normal"))
                    .Text(FText::FromString(FString::Printf(TEXT("%d"), InRollupCount)))
                    .ColorAndOpacity(CkStyle::TextMute())
                ];
            }

            Strip->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, 3.0f, 0.0f)
            [
                BadgeBox
            ];

            ++BadgeCount;
        };

        // Solid badges: own features (structural carriers excluded by the table).
        for (const auto& [FeatureId, Bit] : ck_debugger_entity_tree::Get_BadgeFeatures())
        {
            if ((Representative->OwnBits & (uint64{1} << Bit)) != 0)
            { AddBadge(FeatureId, 0); }
        }

        // Hollow badges: rollup types not already solid.
        for (const auto& [FeatureId, Count] : Representative->RollupCounts)
        {
            const auto Bit = ck::debug_feature_flags::Get_BitIndex(FeatureId);
            const auto AlreadySolid = Bit != INDEX_NONE && (Representative->OwnBits & (uint64{1} << Bit)) != 0;
            if (NOT AlreadySolid)
            { AddBadge(FeatureId, Count); }
        }

        if (OverflowCount > 0)
        {
            Strip->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Normal"))
                .Text(FText::FromString(FString::Printf(TEXT("+%d"), OverflowCount)))
                .ColorAndOpacity(CkStyle::TextMute())
            ];
        }

        return Strip;
    }

    auto Get_NodeTextColor() const -> FSlateColor
    {
        if (NOT Node.IsValid())
        { return CkStyle::Text(); }

        // Inspector-filter dim takes precedence over selection — dimmed entities still
        // read as dimmed even when clicked, so the user knows the row didn't pass the filter.
        // The same dim applies to non-matches in Highlight search mode.
        if (NOT Node->IsFilterMatch || NOT Node->IsSearchMatch)
        { return CkStyle::TextMute(); }

        if (SelectionModel.IsValid() && SelectionModel->IsSelected(Node->Entity))
        { return CkStyle::TextStrong(); }

        return CkStyle::Text();
    }

    auto Get_EntityStatusColor() const -> FSlateColor
    {
        if (NOT Node.IsValid() || ck::Is_NOT_Valid(Node->Entity))
        { return CkStyle::Err(); }

        // Fresh spawn → bright flash for a beat (spec §3.6 churn flashes).
        if (FPlatformTime::Seconds() < Node->FlashUntilSeconds)
        { return FSlateColor{FLinearColor{0.25f, 1.0f, 0.35f}}; }

        if (NOT Node->IsFilterMatch || NOT Node->IsSearchMatch)
        { return CkStyle::TextMute(); }

        if (SelectionModel.IsValid() && SelectionModel->IsSelected(Node->Entity))
        { return CkStyle::Selection(); }

        return CkStyle::Ok();
    }

    auto Get_HighlightText() const -> FText
    {
        const auto TreeWidgetPinned = TreeWidget.Pin();
        if (NOT TreeWidgetPinned.IsValid())
        { return FText::GetEmpty(); }

        // Prefer the Highlight bar — it's the user's "find this" query. Fall
        // back to the Filter so substrings still draw when only Filter is set.
        const auto HighlightText = TreeWidgetPinned->Get_CurrentHighlight();
        return HighlightText.IsEmpty() ? TreeWidgetPinned->Get_CurrentFilter() : HighlightText;
    }

    TSharedPtr<FCkEntityTreeNode> Node;
    TSharedPtr<FCkDebuggerModel_EntitySelection> SelectionModel;
    TWeakPtr<SCkDebuggerWidget_EntityTree> TreeWidget;
};

SCkDebuggerWidget_EntityTree::~SCkDebuggerWidget_EntityTree()
{
    if (FilterChangedHandle.IsValid() && FilterModel.IsValid())
    {
        FilterModel->OnFilterChanged.Remove(FilterChangedHandle);
        FilterChangedHandle.Reset();
    }
}

auto SCkDebuggerWidget_EntityTree::Construct(
    const FArguments& InArgs,
    TSharedPtr<FCkDebuggerModel_EntitySelection> InSelectionModel,
    TSharedPtr<FCkDebuggerModel_WorldContext> InWorldModel,
    TSharedPtr<FCkDebuggerModel_InspectorFilter> InFilterModel) -> void
{
    SelectionModel = InSelectionModel;
    WorldModel = InWorldModel;
    FilterModel = InFilterModel;

    if (const auto* Settings = UCkEcsDebuggerSettings::Get())
    {
        FoldInternals = Settings->FoldInternalEntities;
        GroupSiblings = Settings->GroupSiblingsByArchetype;
        GroupThreshold = FMath::Max(2, Settings->SiblingGroupThreshold);
    }

    if (SelectionModel.IsValid())
    {
        SelectionModel->OnSelectionChanged.AddSP(this, &SCkDebuggerWidget_EntityTree::OnExternalSelectionChanged);
    }

    if (FilterModel.IsValid())
    {
        // TWeakPtr capture so a destroyed tree widget cannot be called back into.
        auto WeakSelf = TWeakPtr<SCkDebuggerWidget_EntityTree>(SharedThis(this));
        FilterChangedHandle = FilterModel->OnFilterChanged.AddLambda(
        [WeakSelf]()
        {
            const auto Pinned = WeakSelf.Pin();
            if (NOT Pinned.IsValid())
            { return; }

            Pinned->ApplyInspectorFilter();
            // Exclusions are applied inside ApplyFilterToNodes (re-runs the visibility pass)
            // so toggling an exclusion immediately hides/reveals matching rows.
            Pinned->ApplyFilterToNodes();
            Pinned->UpdateFilteredRootNodes();
            if (Pinned->TreeView.IsValid())
            {
                Pinned->TreeView->RequestTreeRefresh();
            }
        });
    }

    ChildSlot
    [
        SAssignNew(TreeView, STreeView<TSharedPtr<FCkEntityTreeNode>>)
        .TreeItemsSource(&FilteredRootNodes)
        .OnGenerateRow(this, &SCkDebuggerWidget_EntityTree::OnGenerateRow)
        .OnGetChildren(this, &SCkDebuggerWidget_EntityTree::OnGetChildren)
        .OnSelectionChanged(this, &SCkDebuggerWidget_EntityTree::OnSelectionChanged)
        .OnExpansionChanged(this, &SCkDebuggerWidget_EntityTree::OnExpansionChanged)
        .OnContextMenuOpening(this, &SCkDebuggerWidget_EntityTree::OnContextMenuOpening)
        .SelectionMode(ESelectionMode::Multi)
        .ClearSelectionOnClick(false)
        .HighlightParentNodesForSelection(true)
    ];

    RefreshTree();
}

auto SCkDebuggerWidget_EntityTree::Tick(
    const FGeometry& InAllottedGeometry,
    const double InCurrentTime,
    const float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    TimeSinceLastRefresh += InDeltaTime;

    if (TimeSinceLastRefresh >= RefreshInterval)
    {
        TimeSinceLastRefresh = 0.0f;

        if (NeedsRefresh || (WorldModel.IsValid() && WorldModel->IsCacheDirty()))
        {
            RefreshTree();   // syncs the exclusion list as part of its body
            NeedsRefresh = false;
        }
        else if (FilterModel.IsValid() && FilterModel->Sync_ExclusionsFromSettings())
        {
            // Steady world (no entity churn), but the exclusion list changed in
            // Project Settings — re-apply the filter without a full rebuild so
            // edits land within one interval instead of waiting for a manual
            // refresh or entity change.
            ApplyFilterToNodes();
            ApplyInspectorFilter();
            UpdateFilteredRootNodes();
            if (TreeView.IsValid())
            { TreeView->RequestTreeRefresh(); }
        }
    }
}

auto SCkDebuggerWidget_EntityTree::RefreshTree() -> void
{
    if (NOT WorldModel.IsValid())
    { return; }

    // Store current selection before refresh
    auto PreviouslySelectedEntities = TArray<FCk_Handle>{};
    if (SelectionModel.IsValid())
    {
        PreviouslySelectedEntities = SelectionModel->Get_SelectedEntities();
    }

    auto DidFullRebuild = false;

    if (WorldModel->IsCacheDirty())
    {
        WorldModel->Refresh_EntityCache();

        // Incremental by default (stable TSharedPtr node identity, spec §4): apply the
        // membership diff and re-derive signatures. Full build only from empty — a world
        // switch still flows through the diff (all-removed + all-added).
        if (NodeMap.IsEmpty())
        {
            BuildEntityTree();
            DidFullRebuild = true;
        }
        else
        {
            ApplyCacheDiff(WorldModel->Get_LastAdded(), WorldModel->Get_LastRemoved());
        }

        // Bits may have changed without membership churn (feature added to a live
        // entity also bumps the revision) — recompute classification either way.
        RecomputeNodeSignatures();
    }

    // Pick up Project-Settings edits to the exclusion list live (the model only
    // loads them once at construction, and name-pattern tokens can only be added
    // there — not via the popover). Cheap; the filter is re-applied right below.
    if (FilterModel.IsValid())
    { FilterModel->Sync_ExclusionsFromSettings(); }

    ApplyFilterToNodes();
    ApplyInspectorFilter();
    UpdateFilteredRootNodes();

    if (TreeView.IsValid())
    {
        TreeView->RequestTreeRefresh();
    }

    // Restore selection only after a FULL rebuild — the incremental path keeps node
    // TSharedPtrs stable, so the tree view's selection survives untouched. Restoring on
    // every churn refresh would clear-and-reselect (and scroll-jump) at up to 10 Hz.
    if (DidFullRebuild)
    {
        RestoreSelection(PreviouslySelectedEntities);
    }
}

auto SCkDebuggerWidget_EntityTree::ForceFullRefresh() -> void
{
    RootNodes.Empty();
    AllNodes.Empty();
    NodeMap.Empty();
    FilteredRootNodes.Empty();

    if (WorldModel.IsValid())
    { WorldModel->MarkCacheDirty(); }

    RefreshTree();
}

auto SCkDebuggerWidget_EntityTree::ApplyFilter(const FString& InFilterText) -> void
{
    CurrentFilter = InFilterText;
    ApplyFilterToNodes();
    UpdateFilteredRootNodes();

    if (TreeView.IsValid())
    {
        TreeView->RequestTreeRefresh();
    }
}

auto SCkDebuggerWidget_EntityTree::ApplyHighlight(const FString& InHighlightText) -> void
{
    if (CurrentHighlight == InHighlightText)
    { return; }

    CurrentHighlight = InHighlightText;
    ApplyFilterToNodes();
    UpdateFilteredRootNodes();

    if (TreeView.IsValid())
    {
        TreeView->RequestTreeRefresh();
    }
}

auto SCkDebuggerWidget_EntityTree::ExpandAll() -> void
{
    if (NOT TreeView.IsValid())
    { return; }

    for (const auto& Node : AllNodes)
    {
        if (Node.IsValid() && Node->Children.Num() > 0)
        {
            TreeView->SetItemExpansion(Node, true);
        }
    }
}

auto SCkDebuggerWidget_EntityTree::CollapseAll() -> void
{
    if (NOT TreeView.IsValid())
    { return; }

    for (const auto& Node : AllNodes)
    {
        if (Node.IsValid())
        {
            TreeView->SetItemExpansion(Node, false);
        }
    }
}

auto SCkDebuggerWidget_EntityTree::BuildEntityTree() -> void
{
    RootNodes.Empty();
    AllNodes.Empty();
    NodeMap.Empty();
    GroupNodeCache.Empty();
    MemberToGroup.Empty();

    if (NOT WorldModel.IsValid())
    { return; }

    const auto& CachedEntities = WorldModel->Get_CachedEntities();
    BuildHierarchy(CachedEntities);
}

auto SCkDebuggerWidget_EntityTree::BuildHierarchy(const TArray<FCk_Handle>& InEntities) -> void
{
    for (const auto& Entity : InEntities)
    {
        if (ck::Is_NOT_Valid(Entity))
        { continue; }

        const auto Node = DoCreateNode(Entity);
        NodeMap.Add(Entity, Node);
        AllNodes.Add(Node);
    }

    for (const auto& [Entity, Node] : NodeMap)
    {
        DoLinkNode(Node);
    }
}

auto SCkDebuggerWidget_EntityTree::DoCreateNode(const FCk_Handle& InEntity) -> TSharedPtr<FCkEntityTreeNode>
{
    auto Node = MakeShared<FCkEntityTreeNode>();
    Node->Entity = InEntity;
    Node->IsVisible = true;

    // Names derive once here (spec §5.2 cached names) — routed through the shared
    // name-clean so the settings-driven strip patterns apply.
    Node->CachedDebugName = UCk_Utils_Handle_UE::Get_DebugName(InEntity).ToString();
    Node->CachedCleanName = ck::DebugNameClean::Get_CleanName(Node->CachedDebugName);
    Node->CachedDisplayText = FText::FromString(Node->CachedCleanName);

    return Node;
}

auto SCkDebuggerWidget_EntityTree::DoLinkNode(const TSharedPtr<FCkEntityTreeNode>& InNode) -> void
{
    const auto& Entity = InNode->Entity;

    if (NOT Entity.Has<ck::FFragment_LifetimeOwner>())
    {
        RootNodes.Add(InNode);
        return;
    }

    const auto& LifetimeOwner = Entity.Get<ck::FFragment_LifetimeOwner>().Get_Entity();

    if (UCk_Utils_EntityLifetime_UE::Get_IsTransientEntity(LifetimeOwner))
    {
        RootNodes.Add(InNode);
        return;
    }

    if (const auto ParentNode = NodeMap.Find(LifetimeOwner))
    {
        (*ParentNode)->Children.Add(InNode);
        InNode->Parent = *ParentNode;
    }
    else
    {
        RootNodes.Add(InNode);
    }
}

auto SCkDebuggerWidget_EntityTree::ApplyCacheDiff(
    const TArray<FCk_Handle>& InAdded,
    const TArray<FCk_Handle>& InRemoved) -> void
{
    // ---- Removals ----
    // Lifetime-owned children die with their owner, so a destroyed subtree arrives as a
    // batch of removals in the same diff — collect the set first so parent/child unlink
    // order doesn't matter.
    auto RemovedNodes = TSet<TSharedPtr<FCkEntityTreeNode>>{};
    for (const auto& Entity : InRemoved)
    {
        if (auto Node = TSharedPtr<FCkEntityTreeNode>{}; NodeMap.RemoveAndCopyValue(Entity, Node))
        { RemovedNodes.Add(Node); }
    }

    if (NOT RemovedNodes.IsEmpty())
    {
        AllNodes.RemoveAll([&RemovedNodes](const TSharedPtr<FCkEntityTreeNode>& InNode)
        {
            return RemovedNodes.Contains(InNode);
        });
        RootNodes.RemoveAll([&RemovedNodes](const TSharedPtr<FCkEntityTreeNode>& InNode)
        {
            return RemovedNodes.Contains(InNode);
        });

        for (const auto& Removed : RemovedNodes)
        {
            const auto Parent = Removed->Parent.Pin();
            if (Parent.IsValid() && NOT RemovedNodes.Contains(Parent))
            { Parent->Children.Remove(Removed); }
        }

        // Prune group-cache entries keyed by dead owner pointers — a recycled
        // allocation must never resurrect another owner's group rows.
        auto RemovedPtrs = TSet<const FCkEntityTreeNode*>{};
        RemovedPtrs.Reserve(RemovedNodes.Num());
        for (const auto& Removed : RemovedNodes)
        { RemovedPtrs.Add(Removed.Get()); }

        for (auto It = GroupNodeCache.CreateIterator(); It; ++It)
        {
            if (RemovedPtrs.Contains(It->Key.Key))
            { It.RemoveCurrent(); }
        }
    }

    // ---- Additions ----
    // Create every new node before linking: a new node's owner may itself be new.
    auto NewNodes = TArray<TSharedPtr<FCkEntityTreeNode>>{};
    for (const auto& Entity : InAdded)
    {
        if (ck::Is_NOT_Valid(Entity) || NodeMap.Contains(Entity))
        { continue; }

        const auto Node = DoCreateNode(Entity);
        // Churn flash (spec §3.6): the status dot reads spawn-green for a beat.
        Node->FlashUntilSeconds = FPlatformTime::Seconds() + 0.6;
        NodeMap.Add(Entity, Node);
        AllNodes.Add(Node);
        NewNodes.Add(Node);
    }

    for (const auto& Node : NewNodes)
    {
        DoLinkNode(Node);
    }
}

auto SCkDebuggerWidget_EntityTree::RecomputeNodeSignatures() -> void
{
    const auto* Settings = UCkEcsDebuggerSettings::Get();
    const auto Table = ck::ecs_debugger_classification::BuildTable(
        Settings != nullptr ? Settings->InternalFeatureIds : TSet<FName>{});

    const auto Num = AllNodes.Num();

    auto IndexOf = TMap<const FCkEntityTreeNode*, int32>{};
    IndexOf.Reserve(Num);
    for (auto Index = 0; Index < Num; ++Index)
    { IndexOf.Add(AllNodes[Index].Get(), Index); }

    auto Bits = TArray<uint64>{};
    Bits.Reserve(Num);
    auto OwnerIndex = TArray<int32>{};
    OwnerIndex.Reserve(Num);

    for (const auto& Node : AllNodes)
    {
        const auto NodeBits = ck::IsValid(Node->Entity)
            ? ck::debug_feature_flags::Get_Flags(Node->Entity.Get_RegistryView(), Node->Entity.Get_Entity())
            : uint64{0};
        Bits.Add(NodeBits);

        const auto Parent = Node->Parent.Pin();
        const auto* ParentIndex = Parent.IsValid() ? IndexOf.Find(Parent.Get()) : nullptr;
        OwnerIndex.Add(ParentIndex != nullptr ? *ParentIndex : int32{INDEX_NONE});
    }

    auto Rollups = ck::ecs_debugger_classification::ComputeRollups(Bits, OwnerIndex, Table);

    for (auto Index = 0; Index < Num; ++Index)
    {
        const auto& Node = AllNodes[Index];

        Node->OwnBits = Bits[Index];

        const auto Classification = ck::ecs_debugger_classification::Classify(Bits[Index], Table);
        Node->IsInternal = Classification.IsInternal;
        Node->InternalFeatureId = Classification.InternalFeatureId;

        Node->RollupCounts = MoveTemp(Rollups[Index]);
        Node->RollupBits = 0;
        for (const auto& [FeatureId, Count] : Node->RollupCounts)
        {
            const auto Bit = ck::debug_feature_flags::Get_BitIndex(FeatureId);
            if (Bit != INDEX_NONE)
            { Node->RollupBits |= uint64{1} << Bit; }
        }

        if (ck::IsValid(Node->Entity))
        {
            // Registry-first archetype keying (spec §3.3): registered archetype beats
            // the inferred base-name + signature key.
            const auto RegisteredArchetype = ck::archetype_registry::TryGet_BestMatchName(Node->Entity);
            Node->ArchetypeKey = RegisteredArchetype.IsNone()
                ? ck::ecs_debugger_query::Get_InferredArchetypeKey(Node->CachedCleanName, Bits[Index])
                : RegisteredArchetype.ToString();

            Node->NetRole = UCk_Utils_Net_UE::Get_EntityNetRole(Node->Entity);
        }
        else
        {
            Node->ArchetypeKey.Reset();
            Node->NetRole = ECk_Net_EntityNetRole::None;
        }
    }
}

auto SCkDebuggerWidget_EntityTree::ApplyFilterToNodes() -> void
{
    // Two-pass pipeline driven by the dual search bar:
    //   1. Filter pass — CurrentFilter hides non-matches via IsVisible.
    //   2. Highlight pass — CurrentHighlight (independent input) flags
    //      IsSearchMatch on the visible set so the row's text colour can dim
    //      anything that doesn't match.
    // Either string can be empty; an empty Filter shows everything, an empty
    // Highlight makes every visible row read as a match.

    // ---- Pass 1: visibility from CurrentFilter (query grammar, spec §3.5) + rail ----
    const auto RailMask = [this]() -> uint64
    {
        auto Mask = uint64{0};
        for (const auto& FeatureId : RailIncluded)
        {
            const auto Bit = ck::debug_feature_flags::Get_BitIndex(FeatureId);
            if (Bit != INDEX_NONE)
            { Mask |= uint64{1} << Bit; }
        }
        return Mask;
    }();

    if (CurrentFilter.IsEmpty() && RailMask == 0)
    {
        for (const auto& Node : AllNodes)
        {
            if (Node.IsValid()) { Node->IsVisible = true; }
        }
    }
    else
    {
        const auto Query = ck::ecs_debugger_query::Parse(CurrentFilter);
        const auto& TokenTable = ck_debugger_entity_tree::Get_QueryTokenTable();

        for (const auto& Node : AllNodes)
        {
            if (Node.IsValid()) { Node->IsVisible = false; }
        }

        for (const auto& Node : AllNodes)
        {
            if (NOT Node.IsValid())
            { continue; }

            if (RailMask != 0 && ((Node->OwnBits | Node->RollupBits) & RailMask) == 0)
            { continue; }

            if (ck::ecs_debugger_query::Matches(
                    ck_debugger_entity_tree::Make_QueryContext(*Node), Query, TokenTable))
            {
                MarkNodeVisibilityRecursive(Node, true);
            }
        }
    }

    // ---- Pass 1b: force-hide entities matched by the persistent exclusion list ----
    // Exclusions override the search filter — an entity the user has added to the
    // "always hide" list should never appear even when they type a matching filter.
    if (FilterModel.IsValid() && FilterModel->Get_HasActiveExclusion())
    {
        for (const auto& Node : AllNodes)
        {
            if (NOT Node.IsValid() || NOT Node->IsVisible)
            { continue; }

            if (FilterModel->Test_Entity_IsExcluded(Node->Entity))
            {
                Node->IsVisible = false;
            }
        }
    }

    // ---- Pass 2: IsSearchMatch from CurrentHighlight ----
    if (CurrentHighlight.IsEmpty())
    {
        for (const auto& Node : AllNodes)
        {
            if (Node.IsValid()) { Node->IsSearchMatch = true; }
        }
    }
    else
    {
        const auto HighlightQuery = ck::ecs_debugger_query::Parse(CurrentHighlight);
        const auto& TokenTable = ck_debugger_entity_tree::Get_QueryTokenTable();

        for (const auto& Node : AllNodes)
        {
            if (NOT Node.IsValid())
            { continue; }

            Node->IsSearchMatch = ck::ecs_debugger_query::Matches(
                ck_debugger_entity_tree::Make_QueryContext(*Node), HighlightQuery, TokenTable);
        }
    }

    // Third pass: Auto-expand parents of visible nodes — ONLY while a filter narrows the
    // set. Unconditional expansion would override the user's collapse state on every
    // refresh now that live churn refreshes at up to 10 Hz.
    if (TreeView.IsValid() && NOT CurrentFilter.IsEmpty())
    {
        for (const auto& Node : AllNodes)
        {
            if (NOT Node.IsValid() || NOT Node->IsVisible)
            { continue; }

            // Check if this node has visible children
            auto HasVisibleChildren = false;
            for (const auto& Child : Node->Children)
            {
                if (Child.IsValid() && Child->IsVisible)
                {
                    HasVisibleChildren = true;
                    break;
                }
            }

            // If node has visible children, expand it
            if (HasVisibleChildren)
            {
                TreeView->SetItemExpansion(Node, true);
            }
        }
    }
}

auto SCkDebuggerWidget_EntityTree::ApplyInspectorFilter() -> void
{
    // Walk the FULL node list (independent of IsVisible) so the inspector filter and the
    // search filter compose correctly: search hides via IsVisible, inspector filter dims
    // via IsFilterMatch.
    const auto HasFilter = FilterModel.IsValid();
    for (const auto& Node : AllNodes)
    {
        if (NOT Node.IsValid())
        { continue; }

        Node->IsFilterMatch = HasFilter
            ? FilterModel->Test_Entity(Node->Entity)
            : true;
    }
}

auto SCkDebuggerWidget_EntityTree::UpdateFilteredRootNodes() -> void
{
    RebuildPresentation();
}

auto SCkDebuggerWidget_EntityTree::RebuildPresentation() -> void
{
    MemberToGroup.Reset();

    FilteredRootNodes.Reset();
    DoPresentContainer(RootNodes, nullptr, FilteredRootNodes);

    for (const auto& Node : AllNodes)
    {
        Node->PresentedChildren.Reset();
        DoPresentContainer(Node->Children, Node, Node->PresentedChildren);
    }
}

auto SCkDebuggerWidget_EntityTree::DoPresentContainer(
    const TArray<TSharedPtr<FCkEntityTreeNode>>& InStructuralChildren,
    const TSharedPtr<FCkEntityTreeNode>& InOwner,
    TArray<TSharedPtr<FCkEntityTreeNode>>& OutPresented) -> void
{
    // Folding never applies while a text filter or rail selection narrows the set —
    // a filter hit on an internal entity must stay reachable.
    const auto NoActiveNarrowing = CurrentFilter.IsEmpty() && RailIncluded.IsEmpty();
    const auto FoldActive = FoldInternals
        && NoActiveNarrowing
        && InOwner.IsValid()
        && NOT InOwner->UnfoldInternalsOverride;

    if (InOwner.IsValid())
    { InOwner->FoldedInternalCount = 0; }

    auto Candidates = TArray<TSharedPtr<FCkEntityTreeNode>>{};
    Candidates.Reserve(InStructuralChildren.Num());

    for (const auto& Child : InStructuralChildren)
    {
        if (NOT Child.IsValid() || NOT Child->IsVisible)
        { continue; }

        if (FoldInternals && NoActiveNarrowing && InOwner.IsValid() && Child->IsInternal)
        {
            // Counted even when the override shows them, so the chip can flip back.
            ++InOwner->FoldedInternalCount;

            if (FoldActive)
            { continue; }
        }

        Candidates.Add(Child);
    }

    if (NOT GroupSiblings || Candidates.Num() < GroupThreshold)
    {
        OutPresented.Append(Candidates);
        return;
    }

    // Coalesce runs of same-archetype siblings (spec idea #6): keys at/over the
    // threshold collapse into one stable synthetic group row, emitted at the first
    // member's position.
    auto CountByKey = TMap<FString, int32>{};
    for (const auto& Candidate : Candidates)
    {
        if (NOT Candidate->ArchetypeKey.IsEmpty())
        { ++CountByKey.FindOrAdd(Candidate->ArchetypeKey); }
    }

    auto EmittedGroups = TSet<TSharedPtr<FCkEntityTreeNode>>{};

    for (const auto& Candidate : Candidates)
    {
        const auto* KeyCount = CountByKey.Find(Candidate->ArchetypeKey);
        if (KeyCount == nullptr || *KeyCount < GroupThreshold)
        {
            OutPresented.Add(Candidate);
            continue;
        }

        const auto CacheKey = TPair<const FCkEntityTreeNode*, FString>{ InOwner.Get(), Candidate->ArchetypeKey };
        auto& Group = GroupNodeCache.FindOrAdd(CacheKey);
        if (NOT Group.IsValid())
        {
            Group = MakeShared<FCkEntityTreeNode>();
            Group->IsGroupNode = true;

            auto BaseName = FString{};
            if (NOT Candidate->ArchetypeKey.Split(TEXT("#"), &BaseName, nullptr))
            { BaseName = Candidate->ArchetypeKey; }
            Group->GroupBaseName = BaseName;
        }

        if (NOT EmittedGroups.Contains(Group))
        {
            Group->Children.Reset();
            EmittedGroups.Add(Group);
            OutPresented.Add(Group);
        }

        Group->Children.Add(Candidate);
        MemberToGroup.Add(Candidate.Get(), Group);
    }

    for (const auto& Group : EmittedGroups)
    {
        Group->PresentedChildren = Group->Children;
        Group->IsVisible = true;
    }
}

auto SCkDebuggerWidget_EntityTree::ToggleUnfoldOverride(const TSharedPtr<FCkEntityTreeNode>& InNode) -> void
{
    if (NOT InNode.IsValid())
    { return; }

    InNode->UnfoldInternalsOverride = NOT InNode->UnfoldInternalsOverride;

    RebuildPresentation();

    if (TreeView.IsValid())
    {
        // Revealing internals only helps when the owner row is open.
        if (InNode->UnfoldInternalsOverride)
        { TreeView->SetItemExpansion(InNode, true); }

        TreeView->RequestTreeRefresh();
    }
}

auto SCkDebuggerWidget_EntityTree::Toggle_RailFeature(FName InFeatureId) -> void
{
    if (RailIncluded.Contains(InFeatureId))
    { RailIncluded.Remove(InFeatureId); }
    else
    { RailIncluded.Add(InFeatureId); }

    ApplyFilterToNodes();
    UpdateFilteredRootNodes();

    if (TreeView.IsValid())
    { TreeView->RequestTreeRefresh(); }
}

auto SCkDebuggerWidget_EntityTree::Get_IsPinned(const FCk_Handle& InEntity) const -> bool
{
    return PinnedEntities.Contains(InEntity);
}

auto SCkDebuggerWidget_EntityTree::TogglePin(const FCk_Handle& InEntity) -> void
{
    if (ck::Is_NOT_Valid(InEntity))
    { return; }

    if (PinnedEntities.Contains(InEntity))
    { PinnedEntities.Remove(InEntity); }
    else
    { PinnedEntities.Add(InEntity); }

    OnPinsChanged.Broadcast();
}

auto SCkDebuggerWidget_EntityTree::Set_FoldInternals(bool InFold) -> void
{
    if (FoldInternals == InFold)
    { return; }

    FoldInternals = InFold;

    if (auto* Settings = GetMutableDefault<UCkEcsDebuggerSettings>())
    {
        Settings->FoldInternalEntities = InFold;
        Settings->SaveConfig();
    }

    RebuildPresentation();
    if (TreeView.IsValid())
    { TreeView->RequestTreeRefresh(); }
}

auto SCkDebuggerWidget_EntityTree::Set_GroupSiblings(bool InGroup) -> void
{
    if (GroupSiblings == InGroup)
    { return; }

    GroupSiblings = InGroup;

    if (auto* Settings = GetMutableDefault<UCkEcsDebuggerSettings>())
    {
        Settings->GroupSiblingsByArchetype = InGroup;
        Settings->SaveConfig();
    }

    RebuildPresentation();
    if (TreeView.IsValid())
    { TreeView->RequestTreeRefresh(); }
}

auto SCkDebuggerWidget_EntityTree::Get_Counts() const -> FTreeCounts
{
    auto Counts = FTreeCounts{};
    Counts.Total = AllNodes.Num();

    for (const auto& Node : AllNodes)
    {
        if (NOT Node.IsValid())
        { continue; }

        if (Node->IsInternal) { ++Counts.Internals; }
        else                  { ++Counts.Primaries; }

        if (Node->IsVisible)  { ++Counts.Shown; }
    }

    return Counts;
}

auto SCkDebuggerWidget_EntityTree::ExpandToReveal(const TSharedPtr<FCkEntityTreeNode>& InNode) -> void
{
    if (NOT InNode.IsValid() || NOT TreeView.IsValid())
    { return; }

    // A folded-away internal has no row at all — flip its owner's override first.
    auto NeedsPresentationRebuild = false;
    if (InNode->IsInternal && FoldInternals)
    {
        if (const auto Owner = InNode->Parent.Pin(); Owner.IsValid() && NOT Owner->UnfoldInternalsOverride)
        {
            Owner->UnfoldInternalsOverride = true;
            NeedsPresentationRebuild = true;
        }
    }

    if (NeedsPresentationRebuild)
    {
        RebuildPresentation();
        TreeView->RequestTreeRefresh();
    }

    // Expand real ancestors AND any group row presenting them (or the node itself).
    if (const auto* OwnGroup = MemberToGroup.Find(InNode.Get()))
    { TreeView->SetItemExpansion(*OwnGroup, true); }

    auto Current = InNode->Parent.Pin();
    while (Current.IsValid())
    {
        TreeView->SetItemExpansion(Current, true);

        if (const auto* Group = MemberToGroup.Find(Current.Get()))
        { TreeView->SetItemExpansion(*Group, true); }

        Current = Current->Parent.Pin();
    }
}

auto SCkDebuggerWidget_EntityTree::MarkNodeVisibilityRecursive(
    TSharedPtr<FCkEntityTreeNode> InNode,
    bool InVisible) -> void
{
    if (NOT InNode.IsValid())
    { return; }

    InNode->IsVisible = InVisible;

    if (InVisible)
    {
        auto CurrentNode = InNode->Parent.Pin();
        while (CurrentNode.IsValid())
        {
            CurrentNode->IsVisible = true;
            CurrentNode = CurrentNode->Parent.Pin();
        }
    }
}

auto SCkDebuggerWidget_EntityTree::RestoreSelection(const TArray<FCk_Handle>& InPreviousSelection) -> void
{
    if (NOT SelectionModel.IsValid() || NOT TreeView.IsValid())
    { return; }

    // Find which previously selected entities still exist using O(1) NodeMap lookup
    auto EntitiesToReselect = TArray<FCk_Handle>{};
    auto NodesToSelect = TArray<TSharedPtr<FCkEntityTreeNode>>{};

    for (const auto& PreviousEntity : InPreviousSelection)
    {
        if (const auto FoundNode = NodeMap.Find(PreviousEntity))
        {
            EntitiesToReselect.Add(PreviousEntity);
            NodesToSelect.Add(*FoundNode);
        }
    }

    // If no previous selection was restored, try to auto-select the locally controlled character
    if (EntitiesToReselect.IsEmpty())
    {
        TrySelectLocallyControlledCharacter();
        return;
    }

    // Expand all parent nodes (and any presenting group rows) so they're visible
    for (const auto& Node : NodesToSelect)
    {
        ExpandToReveal(Node);
    }

    // Update the selection model
    SelectionModel->Set_SelectedEntities(EntitiesToReselect);

    // Update the tree view selection
    TreeView->ClearSelection();
    for (const auto& Node : NodesToSelect)
    {
        TreeView->SetItemSelection(Node, true);
    }

    // Scroll to the first selected item to make it visible
    if (NodesToSelect.Num() > 0 && NodesToSelect[0].IsValid())
    {
        TreeView->RequestScrollIntoView(NodesToSelect[0]);
    }
}

auto SCkDebuggerWidget_EntityTree::TrySelectLocallyControlledCharacter() -> void
{
    if (NOT SelectionModel.IsValid() || NOT TreeView.IsValid())
    { return; }

    // Search for the locally controlled character
    TSharedPtr<FCkEntityTreeNode> LocallyControlledNode = nullptr;

    for (const auto& Node : AllNodes)
    {
        if (NOT Node.IsValid())
        { continue; }

        const auto ControlledResult = UCk_Utils_Net_UE::Get_IsEntityLocallyControlled_ByPlayer(Node->Entity);

        if (ControlledResult == ECk_Utils_Net_IsLocallyControlled_Result::IsLocallyControlled)
        {
            LocallyControlledNode = Node;
            break;
        }
    }

    // If no locally controlled character found, don't select anything
    if (NOT LocallyControlledNode.IsValid())
    { return; }

    // Expand all parent nodes (and presenting groups) so the character is visible
    ExpandToReveal(LocallyControlledNode);

    // Select the locally controlled character
    const auto EntitiesToSelect = TArray<FCk_Handle>{ LocallyControlledNode->Entity };
    SelectionModel->Set_SelectedEntities(EntitiesToSelect);

    TreeView->ClearSelection();
    TreeView->SetItemSelection(LocallyControlledNode, true);
    TreeView->RequestScrollIntoView(LocallyControlledNode);
}

auto SCkDebuggerWidget_EntityTree::OnGetChildren(
    TSharedPtr<FCkEntityTreeNode> InNode,
    TArray<TSharedPtr<FCkEntityTreeNode>>& OutChildren) -> void
{
    if (NOT InNode.IsValid())
    { return; }

    // Presentation layer: folded internals removed, sibling runs coalesced into
    // group rows (RebuildPresentation). Visibility is already applied.
    OutChildren = InNode->PresentedChildren;
}

auto SCkDebuggerWidget_EntityTree::OnGenerateRow(
    TSharedPtr<FCkEntityTreeNode> InNode,
    const TSharedRef<STableViewBase>& InOwnerTable) -> TSharedRef<ITableRow>
{
    return SNew(SCkDebuggerEntityTreeRow, InOwnerTable)
        .Node(InNode)
        .SelectionModel(SelectionModel)
        .TreeWidget(SharedThis(this));
}

auto SCkDebuggerWidget_EntityTree::OnSelectionChanged(
    TSharedPtr<FCkEntityTreeNode> InNode,
    ESelectInfo::Type InSelectInfo) -> void
{
    if (NOT SelectionModel.IsValid() || NOT TreeView.IsValid())
    { return; }

    if (IsUpdatingSelection)
    { return; }

    IsUpdatingSelection = true;

    const auto SelectedItems = TreeView->GetSelectedItems();

    auto SelectedEntities = TArray<FCk_Handle>{};
    SelectedEntities.Reserve(SelectedItems.Num());

    auto AnyGroupSelected = false;
    for (const auto& Item : SelectedItems)
    {
        if (NOT Item.IsValid())
        { continue; }

        if (Item->IsGroupNode)
        {
            AnyGroupSelected = true;
            continue;
        }

        SelectedEntities.Add(Item->Entity);
    }

    // A group row carries no entity — clicking one expands/inspects the group without
    // clobbering the current entity selection.
    if (NOT (SelectedEntities.IsEmpty() && AnyGroupSelected))
    {
        SelectionModel->Set_SelectedEntities(SelectedEntities);
    }

    IsUpdatingSelection = false;

    // User-driven selection → sync the other debuggers (Crowd / GOAP resolve
    // their own entity by lifetime-owner lineage). Direct = programmatic
    // (navigator jump, selection restore, sync apply) — never re-broadcast.
    if (InSelectInfo != ESelectInfo::Direct && SelectedEntities.Num() > 0)
    {
        ck::DebugSelectionSync::Broadcast(SelectedEntities[0], TEXT("EcsDebugger"));
    }
}

auto SCkDebuggerWidget_EntityTree::OnExpansionChanged(
    TSharedPtr<FCkEntityTreeNode> InNode,
    bool InIsExpanded) -> void
{
    if (InNode.IsValid())
    {
        InNode->IsExpanded = InIsExpanded;
    }
}

auto SCkDebuggerWidget_EntityTree::OnContextMenuOpening() -> TSharedPtr<SWidget>
{
    if (NOT SelectionModel.IsValid())
    { return nullptr; }

    auto MenuBuilder = FMenuBuilder(true, nullptr);

    // Copy entries: prefer the right-clicked row when STableRow auto-selected it,
    // otherwise fall back to all currently-selected rows. Joined with newlines so
    // multi-select copy is useful for pasting into a list.
    auto SelectedNodes = TArray<TSharedPtr<FCkEntityTreeNode>>{};
    if (TreeView.IsValid())
    {
        SelectedNodes = TreeView->GetSelectedItems();
    }

    if (SelectedNodes.Num() > 0)
    {
        auto NameLines = TArray<FString>{};
        auto IdLines = TArray<FString>{};
        auto NameAndIdLines = TArray<FString>{};
        for (const auto& Node : SelectedNodes)
        {
            if (NOT Node.IsValid())
            { continue; }

            // A group row copies every member — "Copy all N names/IDs" (UI contract §4).
            auto EntityNodes = TArray<TSharedPtr<FCkEntityTreeNode>>{};
            if (Node->IsGroupNode)
            { EntityNodes = Node->Children; }
            else
            { EntityNodes.Add(Node); }

            for (const auto& EntityNode : EntityNodes)
            {
                if (NOT EntityNode.IsValid())
                { continue; }

                const auto& NameStr = EntityNode->CachedDebugName;
                const auto IdStr = ck::Format_UE(TEXT("{}"), EntityNode->Entity.Get_Entity().Get_ID());
                NameLines.Add(NameStr);
                IdLines.Add(IdStr);
                NameAndIdLines.Add(ck::Format_UE(TEXT("{} [{}]"), NameStr, IdStr));
            }
        }

        ck::DebugCopyMenu::AddCopyEntry(MenuBuilder,
            FText::FromString(TEXT("Copy Name")),
            FText::FromString(TEXT("Copy the entity name(s) to the clipboard")),
            FString::Join(NameLines, TEXT("\n")));

        ck::DebugCopyMenu::AddCopyEntry(MenuBuilder,
            FText::FromString(TEXT("Copy ID")),
            FText::FromString(TEXT("Copy the entity ID(s) to the clipboard")),
            FString::Join(IdLines, TEXT("\n")));

        ck::DebugCopyMenu::AddCopyEntry(MenuBuilder,
            FText::FromString(TEXT("Copy Name + ID")),
            FText::FromString(TEXT("Copy \"Name [ID]\" line(s) to the clipboard")),
            FString::Join(NameAndIdLines, TEXT("\n")));

        MenuBuilder.AddMenuSeparator();
    }

    // Pin / Unpin (Phase 3): pinned entities surface in the panel's Pinned section.
    {
        auto EntityNodes = TArray<TSharedPtr<FCkEntityTreeNode>>{};
        for (const auto& Node : SelectedNodes)
        {
            if (Node.IsValid() && NOT Node->IsGroupNode && ck::IsValid(Node->Entity))
            { EntityNodes.Add(Node); }
        }

        if (EntityNodes.Num() > 0)
        {
            const auto AllPinned = ck::algo::AllOf(EntityNodes,
                [this](const TSharedPtr<FCkEntityTreeNode>& InNode)
                { return Get_IsPinned(InNode->Entity); });

            MenuBuilder.AddMenuEntry(
                FText::FromString(AllPinned ? TEXT("Unpin") : TEXT("Pin")),
                FText::FromString(TEXT("Pinned entities stay one click away in the Pinned section above the tree")),
                FSlateIcon(),
                FUIAction(FExecuteAction::CreateLambda([this, EntityNodes, AllPinned]()
                {
                    for (const auto& Node : EntityNodes)
                    {
                        if (Get_IsPinned(Node->Entity) == AllPinned)
                        { TogglePin(Node->Entity); }
                    }
                }))
            );

            MenuBuilder.AddMenuSeparator();
        }
    }

    MenuBuilder.AddMenuEntry(
        FText::FromString(TEXT("Clear Selection")),
        FText::FromString(TEXT("Clears all selected entities")),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateLambda([this]()
        {
            if (SelectionModel.IsValid())
            {
                SelectionModel->Clear_Selection();
                if (TreeView.IsValid())
                {
                    TreeView->ClearSelection();
                }
            }
        }))
    );

    return MenuBuilder.MakeWidget();
}


auto SCkDebuggerWidget_EntityTree::OnExternalSelectionChanged(const TArray<FCk_Handle>& InNewSelection) -> void
{
    if (IsUpdatingSelection || NOT TreeView.IsValid())
    { return; }

    IsUpdatingSelection = true;

    // The entity cache refreshes only on world change / manual Refresh, so an externally
    // selected entity (viewport picker, cross-debugger navigation) may have spawned after
    // the last rebuild and have no node yet — which silently skipped the highlight/expand/
    // scroll below. Rebuild once on miss. Re-entrancy from RefreshTree's RestoreSelection
    // broadcast is blocked by IsUpdatingSelection.
    {
        auto AnyMissing = false;
        for (const auto& Entity : InNewSelection)
        {
            if (ck::IsValid(Entity) && NOT NodeMap.Contains(Entity))
            {
                AnyMissing = true;
                break;
            }
        }

        if (AnyMissing)
        {
            if (WorldModel.IsValid())
            {
                WorldModel->MarkCacheDirty();
            }
            RefreshTree();
        }
    }

    TreeView->ClearSelection();

    for (const auto& Entity : InNewSelection)
    {
        if (const auto FoundNode = NodeMap.Find(Entity))
        {
            TreeView->SetItemSelection(*FoundNode, true);

            // Expand parents (and presenting groups) so the selected node is visible
            ExpandToReveal(*FoundNode);
        }
    }

    // Scroll to the first selected item
    if (InNewSelection.Num() > 0)
    {
        if (const auto FoundNode = NodeMap.Find(InNewSelection[0]))
        {
            TreeView->RequestScrollIntoView(*FoundNode);
        }
    }

    IsUpdatingSelection = false;
}