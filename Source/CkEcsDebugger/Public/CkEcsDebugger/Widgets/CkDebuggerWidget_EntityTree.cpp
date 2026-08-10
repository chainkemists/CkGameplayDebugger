#include "CkDebuggerWidget_EntityTree.h"

#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SNullWidget.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"

#include "CkDebuggerCommon/Classification/CkDebug_DepthTransparency.h"
#include "CkDebuggerCommon/Navigation/CkDebug_Focus.h"
#include "CkDebuggerCommon/Navigation/CkDebug_SelectionSync.h"
#include "CkDebuggerCommon/Utils/CkDebug_CopyMenu_Utils.h"
#include "CkDebuggerCommon/Utils/CkDebug_NameClean_Utils.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Icon.h"

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
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"

namespace ck_debugger_entity_tree
{
    // Shorthand into the shared presentation metadata (also used by the rail + Overview).
    using ck::ecs_debugger_feature_visuals::Get_FeatureVisuals;
    using ck::ecs_debugger_feature_visuals::Get_BadgeFeatures;
    constexpr auto MaxBadges = ck::ecs_debugger_feature_visuals::MaxBadges;

    // RowDensity applies as a DELTA on this surface's own base padding, not as an absolute.
    // The surfaces deliberately disagree on absolute spacing (the on-screen overlay card is
    // far denser than this tree); only the offset between density options is the axis'
    // business. Comfortable is the axis default, so the delta is zero and the tree renders
    // exactly as it shipped. Clamped at zero so Compact can't produce negative margins.
    static auto Apply_RowDensity(const FMargin& InBase) -> FMargin
    {
        const auto Baseline = ck::debug_axes::Get_RowPadding(FCkDebuggerStyleSelection{});
        const auto Current  = ck::debug_axes::Get_RowPadding(UCkDebuggerStyleSettings::Get_Selection());

        const auto DeltaX = Current.Left - Baseline.Left;
        const auto DeltaY = Current.Top  - Baseline.Top;

        return FMargin
        {
            FMath::Max(0.0f, InBase.Left   + DeltaX),
            FMath::Max(0.0f, InBase.Top    + DeltaY),
            FMath::Max(0.0f, InBase.Right  + DeltaX),
            FMath::Max(0.0f, InBase.Bottom + DeltaY)
        };
    }

    // IconSize on the two raw glyph boxes in a row. Both are far below the axis' own 12/16/20
    // scale (a 6px status dot, a 12px feature badge), so the axis applies as its offset rather
    // than as an absolute — Medium is the default, so the delta is zero and Classic is unchanged.
    // Bound through DesiredSizeOverride (an SImage attribute) so the flip reaches rows that Slate's
    // widget generator recycles across a refresh.
    static auto Get_StatusDotSize() -> TOptional<FVector2D>
    {
        const auto Size = ck::debug_axes::Apply_IconSize(6.0f);
        return FVector2D{Size, Size};
    }

    static auto Get_FeatureBadgeSize() -> TOptional<FVector2D>
    {
        const auto Size = ck::debug_axes::Apply_IconSize(12.0f);
        return FVector2D{Size, Size};
    }

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

        // A derived-name row has no real name to search, and the raw handle dump the fallback
        // used to produce is not what the user is looking at. Match what the row RENDERS, so
        // typing "Timer" finds the nameless timer-carrying rows too.
        Context.DisplayName = InNode.HasRealName ? InNode.CachedDebugName : InNode.DerivedName;
        return Context;
    }

    // ---- Derived names (P4) -------------------------------------------------
    // A nameless entity's row used to read as the handle's own ToString dump. Synthesize a label
    // out of what the entity IS instead, using the classification/rollup data the tree already
    // computes. Presentation only — classification is never consulted for anything but reading.

    // ASCII on purpose (decorated glyphs render as tofu boxes in the editor font). The marker is
    // half the signal that this is NOT a name; the muted text colour is the other half.
    constexpr auto DerivedNameMarker = TEXT("~");

    // How many features a derived label spells out before it summarises the remainder as "+N".
    constexpr auto MaxDerivedFeatures = 3;

    static auto Derive_NameBody(const FCkEntityTreeNode& InNode, bool InHasRegisteredArchetype) -> FString
    {
        // A registered archetype is the strongest available statement of what the entity is.
        if (InHasRegisteredArchetype && NOT InNode.ArchetypeKey.IsEmpty())
        { return InNode.ArchetypeKey; }

        // Internals are single-feature by definition (classification §3.1) — that feature IS
        // the identity of the row.
        if (InNode.IsInternal && NOT InNode.InternalFeatureId.IsNone())
        { return InNode.InternalFeatureId.ToString(); }

        auto Tokens = TArray<FString>{};
        auto Omitted = 0;

        // Badge order, so the label and the badge strip tell the same story left to right.
        for (const auto& [FeatureId, Bit] : Get_BadgeFeatures())
        {
            if ((InNode.OwnBits & (uint64{1} << Bit)) == 0)
            { continue; }

            if (Tokens.Num() < MaxDerivedFeatures)
            { Tokens.Add(FeatureId.ToString()); }
            else
            { ++Omitted; }
        }

        if (NOT Tokens.IsEmpty())
        {
            auto Body = FString::Join(Tokens, TEXT("+"));

            if (Omitted > 0)
            { Body += ck::Format_UE(TEXT("+{}"), Omitted); }

            return Body;
        }

        // No own features worth naming: what it CONTAINS is the only thing left that says
        // anything. Ties break lexically so the label can't flicker between equal rollups.
        auto Dominant = FName{};
        auto DominantCount = 0;

        for (const auto& [FeatureId, Count] : InNode.RollupCounts)
        {
            if (Count > DominantCount || (Count == DominantCount && FeatureId.Compare(Dominant) < 0))
            {
                Dominant = FeatureId;
                DominantCount = Count;
            }
        }

        if (NOT Dominant.IsNone())
        { return ck::Format_UE(TEXT("{} host"), Dominant.ToString()); }

        return FString{TEXT("Entity")};
    }

    static auto Derive_Name(const FCkEntityTreeNode& InNode, bool InHasRegisteredArchetype) -> FString
    {
        // The same strip patterns real names go through (Project Settings -> Ck -> Debugger), so a
        // registered archetype name reads identically whether it arrived as a name or a fallback.
        return FString{DerivedNameMarker} + ck::DebugNameClean::Get_CleanName(
            Derive_NameBody(InNode, InHasRegisteredArchetype));
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
                .DesiredSizeOverride_Static(&ck_debugger_entity_tree::Get_StatusDotSize)
            ];
        }

        // Identity glyph — one icon language across the debugger (spec §2.4): internals
        // wear their single feature's glyph, primaries the generic cube, groups the
        // representative member's glyph. SCkDebug_Icon is click-passive, so the
        // row still selects (STableRow contract).
        if (const auto* IdentityBrush = Get_IdentityBrush(); IdentityBrush != nullptr)
        {
            const auto Representative = Get_RepresentativeNode();
            const auto IdentityMeaning = Representative.IsValid() && Representative->IsInternal
                ? Representative->InternalFeatureId.ToString()
                : FString(TEXT("Entity"));

            RowContent->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f)
            [
                SNew(SCkDebug_Icon)
                .Brush(IdentityBrush)
                .Meaning(FText::FromString(IdentityMeaning))
                .ColorAndOpacity(Get_IdentityColor())
                .Size(FVector2D(14.0f, 14.0f))
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

        // "via relay" chip — this row's literal lifetime owner is a depth-transparent
        // ActorRelay we looked through. Purely visual (SBorder/SBox/STextBlock), so the
        // row still selects; the reveal action lives in the row context menu instead of an
        // inline click target (STableRow click-trap contract).
        if (Node.IsValid() && NOT IsGroup)
        {
            RowContent->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FCkDebuggerStyle::Padding_Small, 0.0f, 0.0f, 0.0f)
            [
                SNew(SBox)
                .Visibility(this, &SCkDebuggerEntityTreeRow::Get_ViaRelayChipVisibility)
                .ToolTipText(FText::FromString(TEXT(
                    "Hoisted out of its ActorRelay owner. Right-click > Reveal relay nesting to see it nested.")))
                [
                    ck::debug_axes::Make_Chip(
                        UCkDebuggerStyleSettings::Get_Selection(),
                        FText::FromString(TEXT("via relay")),
                        ECk_Tone::Neutral)
                ]
            ];

            // Relay's own row: compact annotation of what it is currently hiding, plus the
            // muted treatment applied in Get_NodeTextColor.
            RowContent->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FCkDebuggerStyle::Padding_Small, 0.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .TextStyle(&FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Normal"))
                .Visibility(this, &SCkDebuggerEntityTreeRow::Get_RelayAnnotationVisibility)
                .Text(this, &SCkDebuggerEntityTreeRow::Get_RelayAnnotationText)
                .ColorAndOpacity(CkStyle::TextMute())
                .ToolTipText(FText::FromString(TEXT(
                    "This ActorRelay's children are shown at top level. Right-click > Reveal relay nesting to nest them back.")))
            ];
        }

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

        // STableRow::Padding is a TAttribute, so RowDensity lands live on every existing row
        // — no revision poll, no tree rebuild, no loss of node pointer identity.
        STableRow<TSharedPtr<FCkEntityTreeNode>>::Construct(
            STableRow<TSharedPtr<FCkEntityTreeNode>>::FArguments()
            .Style(&FCkDebuggerStyle::Get().GetWidgetStyle<FTableRowStyle>("CkDebugger.TableView.Row"))
            .Padding(TAttribute<FMargin>::CreateLambda([]()
            {
                return ck_debugger_entity_tree::Apply_RowDensity(FMargin{FCkDebuggerStyle::Padding_Small});
            }))
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

    auto Get_ViaRelayChipVisibility() const -> EVisibility
    {
        return Node.IsValid() && Node->HoistedFromRelay
            ? EVisibility::Visible
            : EVisibility::Collapsed;
    }

    auto Get_RelayAnnotationVisibility() const -> EVisibility
    {
        return Node.IsValid() && Node->HoistedChildCount > 0
            ? EVisibility::Visible
            : EVisibility::Collapsed;
    }

    auto Get_RelayAnnotationText() const -> FText
    {
        if (NOT Node.IsValid())
        { return FText::GetEmpty(); }

        // ASCII on purpose — decorated glyphs render as tofu boxes in the editor font.
        return FText::FromString(ck::Format_UE(TEXT("via ActorRelay - {} hoisted"), Node->HoistedChildCount));
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

        // TreeComplexity modulates the strip's OWN budget: the surface still owns what "6 badges"
        // means, the axis only owns the offset between levels. Overflow still renders as "+N", so
        // a tighter cap summarises rather than silently drops.
        const auto BadgeCap = ck::debug_axes::Tree_BadgeCap(
            UCkDebuggerStyleSettings::Get_Selection(), ck_debugger_entity_tree::MaxBadges);

        const auto AddBadge = [&](const FName InFeatureId, const int32 InRollupCount) -> void
        {
            if (BadgeCount >= BadgeCap)
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
                .DesiredSizeOverride_Static(&ck_debugger_entity_tree::Get_FeatureBadgeSize)
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
        // The same dim applies to non-matches in Highlight search mode and to ancestors
        // that are only visible to keep a narrowing match's path readable.
        if (NOT Node->IsFilterMatch || NOT Node->IsSearchMatch || NOT Node->IsNarrowingMatch)
        { return CkStyle::TextMute(); }

        if (SelectionModel.IsValid() && SelectionModel->IsSelected(Node->Entity))
        { return CkStyle::TextStrong(); }

        // A relay whose children were hoisted away is plumbing on this surface — mute it so
        // the eye lands on the first-class rows it used to hide.
        if (Node->HoistedChildCount > 0)
        { return CkStyle::TextMute(); }

        // A DERIVED name is not a name. Muting it (plus the '~' marker the label carries) is what
        // keeps "this entity is called X" and "this entity looks like an X" visually separable.
        if (NOT Node->HasRealName)
        { return CkStyle::TextMute(); }

        return CkStyle::Text();
    }

    auto Get_EntityStatusColor() const -> FSlateColor
    {
        if (NOT Node.IsValid() || ck::Is_NOT_Valid(Node->Entity))
        { return CkStyle::Err(); }

        // Fresh spawn → bright flash for a beat (spec §3.6 churn flashes).
        if (FPlatformTime::Seconds() < Node->FlashUntilSeconds)
        { return FSlateColor{FLinearColor{0.25f, 1.0f, 0.35f}}; }

        if (NOT Node->IsFilterMatch || NOT Node->IsSearchMatch || NOT Node->IsNarrowingMatch)
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

    // Seed the style-axis revision so the first gated Tick doesn't read an unrelated bump from
    // earlier in the session as "TreeComplexity just changed".
    if (const auto* StyleSettings = UCkDebuggerStyleSettings::Get())
    { LastSeenStyleRevision = StyleSettings->Get_Revision(); }

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

        // TreeComplexity is a STRUCTURAL axis — it modulates the fold/group values the
        // presentation pass reads — so it needs the settings revision poll rather than a bound
        // attribute. Consumed unconditionally: a refresh happening this interval already
        // re-presents, and leaving the revision unread would re-fire the rebuild forever.
        auto StyleChanged = false;
        if (const auto* StyleSettings = UCkDebuggerStyleSettings::Get())
        {
            const auto Revision = StyleSettings->Get_Revision();
            StyleChanged = Revision != LastSeenStyleRevision;
            LastSeenStyleRevision = Revision;
        }

        if (NeedsRefresh || (WorldModel.IsValid() && WorldModel->IsCacheDirty()))
        {
            RefreshTree();   // syncs the exclusion list as part of its body
            NeedsRefresh = false;
        }
        else if (StyleChanged)
        {
            // Presentation only: nodes keep their TSharedPtr identity, so selection and
            // expansion survive the level change untouched — only PresentedChildren move.
            RebuildPresentation();

            if (TreeView.IsValid())
            {
                // RebuildList, not RequestTreeRefresh: the refresh reuses the generated row widget
                // for any item whose TSharedPtr survived, and stable node identity is exactly what
                // this tree guarantees — so a row's build-time reads (the Tree_BadgeCap budget in
                // BuildBadgeStrip) would never re-run. Clearing the widget generator forces every
                // visible row through Construct again. Selection lives on the list and expansion in
                // the tree's sparse item info, so neither is disturbed.
                TreeView->RebuildList();
            }
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

        // Lifetime-owner transfer advances the revision while retaining entity
        // membership. Re-link all survivors so hierarchy and sibling grouping
        // track it without replacing nodes, selection, or expansion state.
        RebuildHierarchyLinks();

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

auto SCkDebuggerWidget_EntityTree::Reset_ForWorldChange() -> void
{
    IsUpdatingSelection = true;
    if (TreeView.IsValid())
    {
        TreeView->ClearSelection();
        CollapseAll();
    }
    IsUpdatingSelection = false;

    const auto HadPins = NOT PinnedEntities.IsEmpty();
    PinnedEntities.Reset();
    RootNodes.Reset();
    FilteredRootNodes.Reset();
    AllNodes.Reset();
    NodeMap.Reset();
    GroupNodeCache.Reset();
    MemberToGroup.Reset();
    // Registry handles — must not outlive the session that produced them.
    RevealedRelays.Reset();
    NeedsRefresh = true;

    if (TreeView.IsValid())
    { TreeView->RequestTreeRefresh(); }

    if (HadPins)
    { OnPinsChanged.Broadcast(); }
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

auto SCkDebuggerWidget_EntityTree::RebuildHierarchyLinks() -> void
{
    RootNodes.Reset();

    for (const auto& Node : AllNodes)
    {
        if (NOT Node.IsValid())
        { continue; }

        Node->Children.Reset();
        Node->Parent.Reset();
        Node->HoistedChildCount = 0;
    }

    for (const auto& Node : AllNodes)
    {
        if (Node.IsValid())
        { DoLinkNode(Node); }
    }
}

auto SCkDebuggerWidget_EntityTree::DoCreateNode(const FCk_Handle& InEntity) -> TSharedPtr<FCkEntityTreeNode>
{
    auto Node = MakeShared<FCkEntityTreeNode>();
    Node->Entity = InEntity;
    Node->IsVisible = true;

    // Names derive once here (spec §5.2 cached names) — routed through the shared
    // name-clean so the settings-driven strip patterns apply.
    const auto DebugName = UCk_Utils_Handle_UE::Get_DebugName(InEntity);
    Node->CachedDebugName = DebugName.ToString();
    Node->CachedCleanName = ck::DebugNameClean::Get_CleanName(Node->CachedDebugName);
    Node->CachedDisplayText = FText::FromString(Node->CachedCleanName);

    // "No debug name" is not one condition but three, and only one of them is an empty string:
    //   - no debug-name fragment      -> Get_DebugName falls back to the handle's OWN ToString,
    //                                    which is how a nameless row ended up rendering a raw
    //                                    handle dump. Detected by identity with that source.
    //   - fragment present, name None -> renders the literal "None".
    //   - handle debugging compiled out -> NAME_None.
    // Public Utils only, so no CK_DISABLE_ECS_HANDLE_DEBUGGING guard leaks into debugger code.
    // Node creation is incremental (new entities only), so the second format is not a hot path.
    Node->HasRealName = NOT DebugName.IsNone()
        && NOT Node->CachedDebugName.IsEmpty()
        && Node->CachedDebugName != UCk_Utils_Handle_UE::Conv_HandleToString(InEntity);

    if (NOT Node->HasRealName)
    {
        // Signature-free derivation until the first RecomputeNodeSignatures fills in the real one
        // — the row must never render the handle dump, not even for one refresh interval.
        constexpr auto NoRegisteredArchetype = false;
        Node->DerivedName = ck_debugger_entity_tree::Derive_Name(*Node, NoRegisteredArchetype);
        Node->CachedDisplayText = FText::FromString(Node->DerivedName);
    }

    return Node;
}

auto SCkDebuggerWidget_EntityTree::DoLinkNode(const TSharedPtr<FCkEntityTreeNode>& InNode) -> void
{
    const auto& Entity = InNode->Entity;

    InNode->HoistedFromRelay = false;
    InNode->RelayOwner = FCk_Handle{};

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

    // Depth transparency: an ActorRelay is plumbing, not a conceptual parent, so its
    // children read as depth 0 — they link to the roots unless the user revealed that
    // specific relay. LINKING only; the node keeps its TSharedPtr identity either way.
    if (ck::DebugDepthTransparency::Get_IsTransparentOwner(LifetimeOwner)
        && NOT RevealedRelays.Contains(LifetimeOwner))
    {
        InNode->HoistedFromRelay = true;
        InNode->RelayOwner = LifetimeOwner;

        // The relay's own row reads "N hoisted" off this count. Reset happens in the
        // unlink pass of RebuildHierarchyLinks, which every refresh runs before relinking.
        if (const auto RelayNode = NodeMap.Find(LifetimeOwner))
        { ++(*RelayNode)->HoistedChildCount; }

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

    // The LITERAL lifetime owner, read from the entity rather than from Node->Parent:
    // hoisted nodes have no parent link, but they still have a literal owner, and rollups
    // must not depend on how the tree chose to draw them.
    const auto Get_LiteralOwnerNode = [this](const FCk_Handle& InEntity) -> const FCkEntityTreeNode*
    {
        if (ck::Is_NOT_Valid(InEntity) || NOT InEntity.Has<ck::FFragment_LifetimeOwner>())
        { return nullptr; }

        const auto& LifetimeOwner = InEntity.Get<ck::FFragment_LifetimeOwner>().Get_Entity();
        if (UCk_Utils_EntityLifetime_UE::Get_IsTransientEntity(LifetimeOwner))
        { return nullptr; }

        const auto* OwnerNode = NodeMap.Find(LifetimeOwner);
        return OwnerNode != nullptr ? OwnerNode->Get() : nullptr;
    };

    auto Bits = TArray<uint64>{};
    Bits.Reserve(Num);
    auto OwnerIndex = TArray<int32>{};
    OwnerIndex.Reserve(Num);
    auto IsTransparentOwner = TArray<bool>{};
    IsTransparentOwner.Reserve(Num);

    for (const auto& Node : AllNodes)
    {
        const auto NodeBits = ck::IsValid(Node->Entity)
            ? ck::debug_feature_flags::Get_Flags(Node->Entity.Get_RegistryView(), Node->Entity.Get_Entity())
            : uint64{0};
        Bits.Add(NodeBits);

        const auto* OwnerNode = Get_LiteralOwnerNode(Node->Entity);
        const auto* OwnerNodeIndex = OwnerNode != nullptr ? IndexOf.Find(OwnerNode) : nullptr;
        OwnerIndex.Add(OwnerNodeIndex != nullptr ? *OwnerNodeIndex : int32{INDEX_NONE});

        IsTransparentOwner.Add(ck::DebugDepthTransparency::Get_IsTransparentOwner(Node->Entity));
    }

    // Depth transparency: relays are looked THROUGH before the rollup walk, so a relay
    // child's effective owner is the relay's own owner and internals under that child roll
    // up to the child. Keyed off the shared predicate, NOT off RevealedRelays — revealing a
    // relay is a drawing choice and must not move counts.
    const auto EffectiveOwnerIndex = ck::ecs_debugger_classification::Apply_TransparentOwnerPassThrough(
        OwnerIndex, IsTransparentOwner);

    auto Rollups = ck::ecs_debugger_classification::ComputeRollups(Bits, EffectiveOwnerIndex, Table);

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

        auto HasRegisteredArchetype = false;

        if (ck::IsValid(Node->Entity))
        {
            // Registry-first archetype keying (spec §3.3): registered archetype beats
            // the inferred base-name + signature key.
            const auto RegisteredArchetype = ck::archetype_registry::TryGet_BestMatchName(Node->Entity);
            HasRegisteredArchetype = NOT RegisteredArchetype.IsNone();

            Node->ArchetypeKey = HasRegisteredArchetype
                ? RegisteredArchetype.ToString()
                : ck::ecs_debugger_query::Get_InferredArchetypeKey(Node->CachedCleanName, Bits[Index]);

            Node->NetRole = UCk_Utils_Net_UE::Get_EntityNetRole(Node->Entity);
        }
        else
        {
            Node->ArchetypeKey.Reset();
            Node->NetRole = ECk_Net_EntityNetRole::None;
        }

        // Derived name (P4) — recomputed here rather than at node creation because it reads the
        // classification/rollup/archetype data this pass just produced. Real names always win, so
        // a named node never enters this branch and its display text is untouched.
        if (NOT Node->HasRealName)
        {
            Node->DerivedFromArchetype = HasRegisteredArchetype;
            Node->DerivedName = ck_debugger_entity_tree::Derive_Name(*Node, HasRegisteredArchetype);
            Node->CachedDisplayText = FText::FromString(Node->DerivedName);
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
            if (Node.IsValid())
            {
                Node->IsVisible = true;
                Node->IsNarrowingMatch = true;
            }
        }
    }
    else
    {
        const auto Query = ck::ecs_debugger_query::Parse(CurrentFilter);
        const auto& TokenTable = ck_debugger_entity_tree::Get_QueryTokenTable();

        for (const auto& Node : AllNodes)
        {
            if (Node.IsValid())
            {
                Node->IsVisible = false;
                Node->IsNarrowingMatch = false;
            }
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
                // The node itself matched; ancestors become visible for the path but
                // keep IsNarrowingMatch=false, so they render greyed.
                Node->IsNarrowingMatch = true;
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

    // Third pass: Auto-expand parents of visible nodes — ONLY while a narrowing (filter
    // query OR rail chips) is active. Unconditional expansion would override the user's
    // collapse state on every refresh now that live churn refreshes at up to 10 Hz.
    if (TreeView.IsValid() && (NOT CurrentFilter.IsEmpty() || RailMask != 0))
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
    // TreeComplexity resolves ONCE per pass, not per node: the settings CDO lookup is not free at
    // thousands of nodes × 10 Hz. The axis MODULATES the project settings — those stay the base,
    // and the Normal default resolves every value back to exactly what the project asked for.
    const auto& StyleSelection = UCkDebuggerStyleSettings::Get_Selection();
    const auto ThresholdMultiplier = ck::debug_axes::Tree_FoldThresholdMultiplier(StyleSelection);

    EffectiveGroupThreshold = FMath::Max(2,
        FMath::RoundToInt(static_cast<float>(GroupThreshold) * ThresholdMultiplier));
    PresentsInternalRows = ck::debug_axes::Tree_ShowsInternalRows(StyleSelection);
    GroupsSiblingRuns    = GroupSiblings && ck::debug_axes::Tree_GroupsSiblings(StyleSelection);
    GroupsUnnamedRows    = ck::debug_axes::Tree_GroupsUnnamedRows(StyleSelection);

    MemberToGroup.Reset();

    FilteredRootNodes.Reset();
    DoPresentContainer(RootNodes, nullptr, FilteredRootNodes);

    for (const auto& Node : AllNodes)
    {
        Node->PresentedChildren.Reset();
        DoPresentContainer(Node->Children, Node, Node->PresentedChildren);
    }
}

auto SCkDebuggerWidget_EntityTree::Get_GroupingKey(const FCkEntityTreeNode& InNode) const -> FString
{
    if (GroupsUnnamedRows)
    {
        // Technical noise coalesces by WHAT IT IS rather than by its exact archetype signature —
        // twelve sibling timers under one owner read as one "Timer x12" row instead of twelve
        // near-identical ones. Everything stays reachable inside the group row.
        if (InNode.IsInternal && NOT InNode.InternalFeatureId.IsNone())
        { return ck::Format_UE(TEXT("{}#internal"), InNode.InternalFeatureId.ToString()); }

        // Derived-name rows too — but NOT the ones whose label came from a registered archetype,
        // which already group correctly (and identically to their named siblings) by that key.
        if (NOT InNode.HasRealName && NOT InNode.DerivedFromArchetype && NOT InNode.DerivedName.IsEmpty())
        { return ck::Format_UE(TEXT("{}#derived"), InNode.DerivedName); }
    }

    return InNode.ArchetypeKey;
}

auto SCkDebuggerWidget_EntityTree::DoPresentContainer(
    const TArray<TSharedPtr<FCkEntityTreeNode>>& InStructuralChildren,
    const TSharedPtr<FCkEntityTreeNode>& InOwner,
    TArray<TSharedPtr<FCkEntityTreeNode>>& OutPresented) -> void
{
    // Folding never applies while a text filter or rail selection narrows the set —
    // a filter hit on an internal entity must stay reachable.
    //
    // The Full complexity level takes the fold pass out entirely (PresentsInternalRows): internals
    // are composed as plain rows rather than folded and then revealed, so no ⊞ chip appears and
    // nothing has to be clicked open. Every other level leaves the decision to the project's own
    // FoldInternals setting.
    const auto NoActiveNarrowing = CurrentFilter.IsEmpty() && RailIncluded.IsEmpty();
    const auto FoldsInternals = FoldInternals && NOT PresentsInternalRows;
    const auto FoldActive = FoldsInternals
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

        if (FoldsInternals && NoActiveNarrowing && InOwner.IsValid() && Child->IsInternal)
        {
            // Counted even when the override shows them, so the chip can flip back.
            ++InOwner->FoldedInternalCount;

            if (FoldActive)
            { continue; }
        }

        Candidates.Add(Child);
    }

    if (NOT GroupsSiblingRuns || Candidates.Num() < EffectiveGroupThreshold)
    {
        OutPresented.Append(Candidates);
        return;
    }

    // Coalesce runs of same-key siblings (spec idea #6): keys at/over the threshold collapse into
    // one stable synthetic group row, emitted at the first member's position. WHAT the key is
    // depends on the complexity level (Get_GroupingKey — archetype everywhere but Minimal), so it
    // is resolved ONCE per candidate here: counting and emission can never disagree about a run.
    auto KeyOf = TMap<const FCkEntityTreeNode*, FString>{};
    KeyOf.Reserve(Candidates.Num());

    auto CountByKey = TMap<FString, int32>{};
    for (const auto& Candidate : Candidates)
    {
        auto Key = Get_GroupingKey(*Candidate);

        if (NOT Key.IsEmpty())
        { ++CountByKey.FindOrAdd(Key); }

        KeyOf.Add(Candidate.Get(), MoveTemp(Key));
    }

    auto EmittedGroups = TSet<TSharedPtr<FCkEntityTreeNode>>{};

    for (const auto& Candidate : Candidates)
    {
        const auto& CandidateKey = KeyOf[Candidate.Get()];

        const auto* KeyCount = CountByKey.Find(CandidateKey);
        if (KeyCount == nullptr || *KeyCount < EffectiveGroupThreshold)
        {
            OutPresented.Add(Candidate);
            continue;
        }

        const auto CacheKey = TPair<const FCkEntityTreeNode*, FString>{ InOwner.Get(), CandidateKey };
        auto& Group = GroupNodeCache.FindOrAdd(CacheKey);
        if (NOT Group.IsValid())
        {
            Group = MakeShared<FCkEntityTreeNode>();
            Group->IsGroupNode = true;

            auto BaseName = FString{};
            if (NOT CandidateKey.Split(TEXT("#"), &BaseName, nullptr))
            { BaseName = CandidateKey; }
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

auto SCkDebuggerWidget_EntityTree::Get_IsRelayRevealed(const FCk_Handle& InRelay) const -> bool
{
    return RevealedRelays.Contains(InRelay);
}

auto SCkDebuggerWidget_EntityTree::Set_RelayRevealed(
    const TArray<FCk_Handle>& InRelays,
    bool InRevealed) -> void
{
    auto AnyChanged = false;

    for (const auto& Relay : InRelays)
    {
        if (ck::Is_NOT_Valid(Relay))
        { continue; }

        if (InRevealed)
        {
            auto AlreadyRevealed = false;
            RevealedRelays.Add(Relay, &AlreadyRevealed);
            AnyChanged |= NOT AlreadyRevealed;
        }
        else
        {
            AnyChanged |= RevealedRelays.Remove(Relay) > 0;
        }
    }

    if (NOT AnyChanged)
    { return; }

    // Re-link only: nodes, selection, and expansion state all survive. Classification is
    // deliberately NOT recomputed — the rollup pass-through ignores reveal state.
    RebuildHierarchyLinks();
    ApplyFilterToNodes();
    UpdateFilteredRootNodes();

    if (NOT TreeView.IsValid())
    { return; }

    // Re-nesting only helps when the relay row is open.
    if (InRevealed)
    {
        for (const auto& Relay : InRelays)
        {
            if (const auto RelayNode = NodeMap.Find(Relay))
            { TreeView->SetItemExpansion(*RelayNode, true); }
        }
    }

    TreeView->RequestTreeRefresh();
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
    // ENTITIES, never presented rows. AllNodes holds one node per live entity and no synthetic
    // group rows, so folding, grouping, and the TreeComplexity level cannot move these numbers —
    // the status bar stays truthful however few rows the tree happens to be drawing.
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

    // A folded-away internal has no row at all — flip its owner's override first. Under the Full
    // complexity level nothing is folded, so there is no override to flip.
    auto NeedsPresentationRebuild = false;
    if (InNode->IsInternal && FoldInternals && NOT PresentsInternalRows)
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

                // Copy what the row SHOWS. A derived-name row has no real name, and the handle
                // dump Get_DebugName falls back to is not what the user is pointing at — the id
                // is copied on its own line (and in "Name [ID]") either way.
                const auto& NameStr = EntityNode->HasRealName
                    ? EntityNode->CachedDebugName
                    : EntityNode->DerivedName;

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

    // Relay depth transparency: per-relay escape hatch out of the hoisting default. A
    // menu entry rather than an inline row widget — an extra click target inside the row
    // would trap the selection click (STableRow contract).
    {
        auto RelaysToReveal = TArray<FCk_Handle>{};
        auto RelaysToHide = TArray<FCk_Handle>{};

        for (const auto& Node : SelectedNodes)
        {
            if (NOT Node.IsValid() || Node->IsGroupNode)
            { continue; }

            if (Node->HoistedFromRelay && ck::IsValid(Node->RelayOwner))
            { RelaysToReveal.AddUnique(Node->RelayOwner); }
            else if (Node->HoistedChildCount > 0)
            { RelaysToReveal.AddUnique(Node->Entity); }
            else if (Get_IsRelayRevealed(Node->Entity))
            { RelaysToHide.AddUnique(Node->Entity); }
        }

        if (NOT RelaysToReveal.IsEmpty())
        {
            MenuBuilder.AddMenuEntry(
                FText::FromString(TEXT("Reveal relay nesting")),
                FText::FromString(TEXT("Nest this ActorRelay's children back under it instead of hoisting them to the top level")),
                FSlateIcon(),
                FUIAction(FExecuteAction::CreateLambda([this, RelaysToReveal]()
                {
                    Set_RelayRevealed(RelaysToReveal, true);
                }))
            );
        }

        if (NOT RelaysToHide.IsEmpty())
        {
            MenuBuilder.AddMenuEntry(
                FText::FromString(TEXT("Hide relay nesting")),
                FText::FromString(TEXT("Hoist this ActorRelay's children back to the top level")),
                FSlateIcon(),
                FUIAction(FExecuteAction::CreateLambda([this, RelaysToHide]()
                {
                    Set_RelayRevealed(RelaysToHide, false);
                }))
            );
        }

        if (NOT RelaysToReveal.IsEmpty() || NOT RelaysToHide.IsEmpty())
        { MenuBuilder.AddMenuSeparator(); }
    }

    // Focus in viewport — ejected/simulate only; hidden while possessed (the
    // gameplay camera is not fought over).
    if (ck::DebugFocus::Get_CanFocus())
    {
        auto FocusTarget = FCk_Handle{};
        for (const auto& Node : SelectedNodes)
        {
            if (Node.IsValid() && NOT Node->IsGroupNode && ck::IsValid(Node->Entity))
            { FocusTarget = Node->Entity; break; }
        }

        if (ck::IsValid(FocusTarget))
        {
            MenuBuilder.AddMenuEntry(
                FText::FromString(TEXT("Focus in Viewport (F)")),
                FText::FromString(TEXT("Glide the editor camera to frame this entity (auto-ejects while possessed)")),
                FSlateIcon(),
                FUIAction(FExecuteAction::CreateLambda([FocusTarget]()
                {
                    ck::DebugFocus::Focus_Entity(FocusTarget);
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


auto SCkDebuggerWidget_EntityTree::OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) -> FReply
{
    // F — frame the primary selection in the ejected editor viewport (no-op possessed).
    if (InKeyEvent.GetKey() == EKeys::F && SelectionModel.IsValid())
    {
        const auto Primary = SelectionModel->Get_PrimarySelection();
        if (ck::IsValid(Primary) && ck::DebugFocus::Focus_Entity(Primary))
        { return FReply::Handled(); }
    }

    return SCompoundWidget::OnKeyDown(InGeometry, InKeyEvent);
}


auto SCkDebuggerWidget_EntityTree::OnExternalSelectionChanged(const TArray<FCk_Handle>& InNewSelection) -> void
{
    if (IsUpdatingSelection || NOT TreeView.IsValid())
    { return; }

    IsUpdatingSelection = true;

    // The entity cache polls structural changes at a bounded cadence, so an externally
    // selected entity (viewport picker, cross-debugger navigation) may have spawned after
    // the last poll and have no node yet — which would skip the highlight/expand/scroll
    // below. Refresh immediately on a miss. Re-entrancy from RefreshTree's
    // RestoreSelection broadcast is blocked by IsUpdatingSelection.
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
