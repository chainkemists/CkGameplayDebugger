#include "CkGoapDebugger/Window/SCkGoapDebugger_WorldStateRail.h"

#include "CkGoapDebugger/CkGoapDebuggerStyle.h"
#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkGoapDebugger/CkGoapDebugger_Axes.h"

#include "CkDebuggerCommon/Search/SCkDebug_DualSearchBar.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Chip.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NameLabel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Switch.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_ValuePill.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "CkGoap/Algorithm/CkGoap_WorldState.h"
#include "CkGoap/WorldState/CkGoap_WorldState_Utils.h"

#include "Styling/CoreStyle.h"
#include "Styling/StyleDefaults.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================
// Internal helpers
// ====================================================================================================================

namespace ck_goap_debugger_wsrail_internal
{
    // Cap displayed key text so very long tags don't blow out the 300px rail.
    constexpr int32 WsRail_MaxKeyChars = 28;

    // Layer name used by the WS rail when pushing single-key debug overrides.
    // AI-deliberation layers use their own ad-hoc names; "DebugUI" is reserved
    // for hands-on toggles from the rail.
    static const FName WsRail_DebugUiLayerName = FName{TEXT("DebugUI")};

    auto TruncateKey(const FString& InKey) -> FString
    {
        if (InKey.Len() <= WsRail_MaxKeyChars) { return InKey; }
        // Show the leaf bit by preferring the right side of the tag, since
        // GameplayTag tail-segments tend to be the discriminating identifier.
        const auto Tail = InKey.Right(WsRail_MaxKeyChars - 1);
        return FString(TEXT("…")) + Tail;
    }

}

// ====================================================================================================================
// CONSTRUCT / DESTRUCT
// ====================================================================================================================

auto
    SCkGoapDebugger_WorldStateRail::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _ViewModel = InArgs._ViewModel;

    ChildSlot
    [
        SNew(SBorder)
            .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Panel")))
            .Padding(FMargin(0.0f))
            [
                SNew(SVerticalBox)

                    // ---- Header (pane-head) ----------------------------------
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SAssignNew(_HeaderHost, SBox)
                        ]

                    + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(SBox)
                                .HeightOverride_Lambda([]() -> FOptionalSize
                                { return FOptionalSize{ck_goap_debugger_axes::Get_SeparatorThickness()}; })
                                .Visibility_Lambda([]()
                                {
                                    return ck_goap_debugger_axes::Get_SeparatorThickness() > 0.0f
                                        ? EVisibility::Visible : EVisibility::Collapsed;
                                })
                                [
                                    SNew(SImage)
                                        .Image(CkStyle::GetFilledBrush())
                                        .ColorAndOpacity(FSlateColor(CkStyle::Border()))
                                ]
                        ]

                    // ---- Search + sort (fixed chrome — keeps input focus) ----
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Small,
                                         FCkGoapDebuggerStyle::Padding_Small))
                        [
                            BuildSearchAndSortBar()
                        ]

                    // ---- Body (scrollable key list) --------------------------
                    + SVerticalBox::Slot()
                        .FillHeight(1.0f)
                        [
                            SNew(SScrollBox)
                                .Orientation(Orient_Vertical)
                                + SScrollBox::Slot()
                                .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium,
                                                 FCkGoapDebuggerStyle::Padding_Small))
                                [
                                    SAssignNew(_Body, SVerticalBox)
                                ]
                        ]

                    // ---- Footer ----------------------------------------------
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SAssignNew(_FooterHost, SBox)
                        ]
            ]
    ];

    RefreshFromViewModel();
}

SCkGoapDebugger_WorldStateRail::~SCkGoapDebugger_WorldStateRail() = default;

// ====================================================================================================================
// REFRESH
// ====================================================================================================================

auto
    SCkGoapDebugger_WorldStateRail::
    Resolve_DisplayedWorldState(
        const TArray<FCkGoapDebugger_WorldStateEntry>*& OutEntries,
        FString& OutLabel) const
    -> bool
{
    OutEntries = nullptr;
    OutLabel.Reset();

    if (NOT _ViewModel.IsValid()) { return false; }

    const auto* AsInfo = _ViewModel->GetSelectedActionSetInfo();
    if (AsInfo == nullptr) { return false; }

    // Prefer the selected Action's WS source label when set; the per-Planner
    // WS array remains the source of resolved values (we don't yet collect a
    // per-Action override on FCkGoapDebugger_ActionInfo).
    const auto* SelAction = _ViewModel->GetSelectedActionInfo();
    if (SelAction != nullptr && NOT SelAction->WorldStateSourceLabel.IsEmpty())
    {
        OutLabel = SelAction->WorldStateSourceLabel;
    }
    else
    {
        OutLabel = AsInfo->WorldStateSourceLabel;
    }

    OutEntries = &AsInfo->WorldState;
    return true;
}

auto
    SCkGoapDebugger_WorldStateRail::
    RefreshFromViewModel()
    -> void
{
    if (NOT _ViewModel.IsValid()) { return; }

    const TArray<FCkGoapDebugger_WorldStateEntry>* Entries = nullptr;
    auto Label = FString{};
    const auto HasSelection = Resolve_DisplayedWorldState(Entries, Label);

    // Cache the resolved WS handle for click handlers + TAttribute lambdas.
    // The handle is read straight from the ActionSet info populated by the
    // data collector (no ECS lookups in this hot path).
    _CurrentWorldState = FCk_Handle_Goap_WorldState{};
    if (HasSelection && _ViewModel.IsValid())
    {
        if (const auto* AsInfo = _ViewModel->GetSelectedActionSetInfo())
        { _CurrentWorldState = AsInfo->WorldStateHandle; }
    }

    // ----------------------------------------------------------------------
    // Content-hash gate — skip the destructive SetContent / ClearChildren
    // cascade when this snapshot would render identical to the last one.
    // Covers the empty-state path too: the first time the user lands on a
    // panel-less state we materialise it; subsequent broadcasts with the
    // same null selection early-return.
    // ----------------------------------------------------------------------
    auto NewHash = uint32{0};
    NewHash = HashCombine(NewHash, ::GetTypeHash(HasSelection ? 1 : 0));
    NewHash = HashCombine(NewHash, GetTypeHash(Label));
    if (HasSelection && Entries != nullptr)
    {
        NewHash = HashCombine(NewHash, ::GetTypeHash(Entries->Num()));
        for (const auto& E : *Entries)
        {
            auto Pair = GetTypeHash(E.Key);
            Pair = HashCombine(Pair, ::GetTypeHash(E.Value ? 1 : 0));
            Pair = HashCombine(Pair, ::GetTypeHash(E.RecentlyChanged ? 1 : 0));
            NewHash ^= Pair;  // commutative — entry-order doesn't fake a change
        }
    }
    // Fold in the per-key usage census. Rows capture their nP·mE text by VALUE
    // at build time, and the census back-fills a frame or two AFTER the key
    // list exists (child-action CDO extraction runs a tick after AddAction) —
    // without this fold the rail keeps the goal-only census forever.
    if (const auto* CensusPlannerInfo = _ViewModel->GetSelectedPlannerInfo())
    {
        for (const auto& [UsageKey, Usage] : CensusPlannerInfo->KeyUsage)
        {
            auto Pair = GetTypeHash(UsageKey);
            Pair = HashCombine(Pair, ::GetTypeHash(Usage.X));
            Pair = HashCombine(Pair, ::GetTypeHash(Usage.Y));
            NewHash ^= Pair;  // commutative — map order doesn't fake a change
        }
    }

    // Fold in the shared name-depth so toolbar +/- toggles re-render the rail
    // with new key truncation. WS keys honour the same depth as planner-tier
    // display names — SCkDebug_NameLabel::Get_ShortName is the one shortener.
    if (_ViewModel.IsValid())
    { NewHash = HashCombine(NewHash, ::GetTypeHash(_ViewModel->Get_NameDepth())); }

    // Fold in search + sort state — typing into the dual search bar or cycling
    // the sort mode rebuilds the body (the inputs themselves live in the fixed
    // chrome, so focus survives).
    NewHash = HashCombine(NewHash, GetTypeHash(_FilterString));
    NewHash = HashCombine(NewHash, GetTypeHash(_HighlightString));
    NewHash = HashCombine(NewHash, ::GetTypeHash(static_cast<uint8>(_SortMode)));

    // Fold in the override-stack identity so push/pop/expand triggers a rebuild
    // of the layers section (different stack contents = different rendered rows).
    if (ck::IsValid(_CurrentWorldState))
    {
        const auto LayerNames = UCk_Utils_Goap_WorldState_UE::Get_OverrideLayerNames(_CurrentWorldState);
        NewHash = HashCombine(NewHash, ::GetTypeHash(LayerNames.Num()));
        for (const auto& Name : LayerNames)
        {
            auto LayerHash = GetTypeHash(Name);
            LayerHash = HashCombine(LayerHash, ::GetTypeHash(
                UCk_Utils_Goap_WorldState_UE::Get_LayerKeyCount(_CurrentWorldState, Name)));
            // Per-layer expand state — collapse/expand must rebuild the row.
            LayerHash = HashCombine(LayerHash, ::GetTypeHash(_ExpandedLayers.Contains(Name) ? 1 : 0));
            NewHash ^= LayerHash;  // commutative — stack-order shifts below caught by Num + per-row content
        }
    }

    if (_HasMaterialized && NewHash == _LastContentHash)
    { return; }
    _LastContentHash = NewHash;
    _HasMaterialized = true;

    // ---- Empty state ----------------------------------------------------------
    if (NOT HasSelection || Entries == nullptr || Entries->Num() == 0)
    {
        if (_HeaderHost.IsValid())
        { _HeaderHost->SetContent(BuildHeader(FString{})); }

        if (_Body.IsValid())
        {
            _Body->ClearChildren();
            _Body->AddSlot()
                .AutoHeight()
                .Padding(FMargin(0.0f, FCkGoapDebuggerStyle::Padding_Medium))
                [
                    BuildEmptyState()
                ];
        }

        if (_FooterHost.IsValid())
        { _FooterHost->SetContent(SNew(SSpacer)); }

        return;
    }

    // ---- Sorted + filtered view over the snapshot entries ---------------------
    // Filter narrows; sort orders (Name default, or TRUE-first). Highlight is
    // applied per-row below (dims non-matches without hiding them).
    auto ViewEntries = TArray<const FCkGoapDebugger_WorldStateEntry*>{};
    ViewEntries.Reserve(Entries->Num());
    for (const auto& Entry : *Entries)
    {
        if (NOT _FilterString.IsEmpty() &&
            NOT Entry.Key.ToString().Contains(_FilterString))
        { continue; }
        ViewEntries.Add(&Entry);
    }

    const auto SortMode = _SortMode;
    ViewEntries.Sort([SortMode](const FCkGoapDebugger_WorldStateEntry& A, const FCkGoapDebugger_WorldStateEntry& B)
    {
        if (SortMode == ECkGoapDebugger_WsSortMode::ByTrueFirst && A.Value != B.Value)
        { return A.Value; }
        return A.Key.ToString() < B.Key.ToString();
    });

    // ---- Header (SectionHeader + Sandbox switch) -----------------------------
    const auto KeyCount = Entries->Num();

    if (_HeaderHost.IsValid())
    { _HeaderHost->SetContent(BuildHeader(Label)); }

    // ---- Body (override layers section + rows) -------------------------------
    if (_Body.IsValid())
    {
        _Body->ClearChildren();

        // Override-stack inspector (mockup "layerbox") — always present so the
        // base store row anchors the mental model; pushed layers stack above it.
        if (ck::IsValid(_CurrentWorldState))
        {
            _Body->AddSlot()
                .AutoHeight()
                .Padding(FMargin(0.0f, 0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium))
                [
                    BuildOverrideLayersSection(_CurrentWorldState, KeyCount)
                ];
        }

        // Per-key usage census from the selected Planner's subtree fold-up
        // (X = precondition/goal reads, Y = effect writes).
        const auto* PlannerInfo = _ViewModel->GetSelectedPlannerInfo();

        for (const auto* Entry : ViewEntries)
        {
            const auto HighlightDimmed =
                NOT _HighlightString.IsEmpty() &&
                NOT Entry->Key.ToString().Contains(_HighlightString);

            auto Usage = FIntPoint::ZeroValue;
            if (PlannerInfo != nullptr)
            {
                if (const auto* Found = PlannerInfo->KeyUsage.Find(Entry->Key))
                { Usage = *Found; }
            }

            _Body->AddSlot()
                .AutoHeight()
                [
                    BuildKeyRow(*Entry, HighlightDimmed, Usage)
                ];
        }

        if (ViewEntries.Num() == 0)
        {
            _Body->AddSlot()
                .AutoHeight()
                .Padding(FMargin(0.0f, FCkGoapDebuggerStyle::Padding_Medium))
                [
                    SNew(SCkDebug_SelectableLabel)
                        .Text(FText::FromString(TEXT("(no keys match filter)")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
                        .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
                ];
        }
    }

    // ---- Footer ---------------------------------------------------------------
    if (_FooterHost.IsValid())
    { _FooterHost->SetContent(BuildFooter(KeyCount)); }
}

// ====================================================================================================================
// BUILD — EMPTY STATE
// ====================================================================================================================

auto
    SCkGoapDebugger_WorldStateRail::
    BuildEmptyState()
    -> TSharedRef<SWidget>
{
    auto Message = FString(TEXT("No entity selected"));
    if (_ViewModel.IsValid())
    {
        if (ck::IsValid(_ViewModel->GetSelectedEntity()))
        {
            if (_ViewModel->GetSelectedActionSetInfo() == nullptr)
            { Message = TEXT("No Planner selected"); }
            else
            { Message = TEXT("(WorldState empty)"); }
        }
    }

    return SNew(SBox)
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Large))
        [
            SNew(SCkDebug_SelectableLabel)
                .Text(FText::FromString(Message))
                .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
                .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
        ];
}

// ====================================================================================================================
// BUILD — HEADER (SectionHeader + Sandbox switch)
// ====================================================================================================================

auto
    SCkGoapDebugger_WorldStateRail::
    BuildHeader(const FString& InLabel)
    -> TSharedRef<SWidget>
{
    // The WS label doubles as the clarifier when resolved (rendered through the
    // shared name-label widget so it honours the depth tuner + expand button);
    // the mockup's static subtext covers the unresolved / empty states.
    const auto SubWidget = InLabel.IsEmpty()
        ? SNullWidget::NullWidget
        : StaticCastSharedRef<SWidget>(
            SNew(SCkDebug_NameLabel)
                .FullName(InLabel)
                .NameDepth_Lambda([this]() -> int32
                {
                    return _ViewModel.IsValid() ? _ViewModel->Get_NameDepth() : 1;
                })
                .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
                .ColorAndOpacity(FSlateColor(CkStyle::TextMute())));

    return SNew(SCkDebug_SectionHeader)
        .Label(FText::FromString(TEXT("World State")))
        .SubText(FText::FromString(TEXT("what the agent believes")))
        .SubContent()
        [
            SubWidget
        ]
        .Underline(true)
        .RightContent()
        [
            // One click target for label + switch (mockup <label> semantics):
            // the switch consumes its own clicks; clicks on the "Sandbox" text
            // land on this wrapper button and toggle the same state.
            SNew(SButton)
                .ButtonStyle(FCoreStyle::Get(), "NoBorder")
                .ContentPadding(FMargin(0.0f))
                .Cursor(EMouseCursor::Hand)
                .ToolTipText(FText::FromString(TEXT(
                    "Sandbox pushes a 'DebugUI' override layer onto the World State.\n"
                    "Reads are shadowed; the base store is untouched.\n"
                    "Switching off pops the layer — back to live truth.")))
                .OnClicked_Lambda([this]() -> FReply
                {
                    HandleSandboxToggled(NOT _SandboxMode);
                    return FReply::Handled();
                })
                [
                    SNew(SHorizontalBox)

                        + SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Center)
                            .Padding(FMargin(0.0f, 0.0f, CkStyle::SpaceS, 0.0f))
                            [
                                SNew(STextBlock)
                                    .Text(FText::FromString(TEXT("Sandbox")))
                                    .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
                                    .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                            ]

                        + SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Center)
                            [
                                SNew(SCkDebug_Switch)
                                    .IsOn_Lambda([this]() { return _SandboxMode; })
                                    .OnStateChanged(FOnCkDebug_SwitchChanged::CreateSP(
                                        this, &SCkGoapDebugger_WorldStateRail::HandleSandboxToggled))
                            ]
                ]
        ];
}

// ====================================================================================================================
// BUILD — FOOTER (key budget + subscribers + trace hint)
// ====================================================================================================================

auto
    SCkGoapDebugger_WorldStateRail::
    BuildFooter(int32 InKeyCount)
    -> TSharedRef<SWidget>
{
    const auto WeakRail = TWeakPtr<SCkGoapDebugger_WorldStateRail>(SharedThis(this));

    // Live subscriber count — entities re-planned when a key flips. Feeds the
    // sandbox blast-radius read ("this WS is shared").
    auto SubscriberTextAttr = TAttribute<FText>::Create(
        TAttribute<FText>::FGetter::CreateLambda(
            [WeakRail]() -> FText
            {
                const auto Pinned = WeakRail.Pin();
                if (NOT Pinned.IsValid()) { return FText::FromString(TEXT("0")); }

                auto WsHandle = Pinned->_CurrentWorldState;
                if (NOT ck::IsValid(WsHandle)) { return FText::FromString(TEXT("0")); }

                return FText::AsNumber(UCk_Utils_Goap_WorldState_UE::Get_SubscriberCount(WsHandle));
            }));

    // Trace hint — mirrors the mockup: idle prompt, or the traced key + clear.
    const auto TracedKey = [WeakRail]() -> FGameplayTag
    {
        const auto Pinned = WeakRail.Pin();
        if (NOT Pinned.IsValid() || NOT Pinned->_ViewModel.IsValid()) { return {}; }
        return Pinned->_ViewModel->Get_TracedWsKey();
    };

    auto TraceHintTextAttr = TAttribute<FText>::Create(
        TAttribute<FText>::FGetter::CreateLambda(
            [TracedKey]() -> FText
            {
                const auto Key = TracedKey();
                if (NOT Key.IsValid())
                { return FText::FromString(TEXT("click a key to trace it")); }

                auto Leaf = Key.ToString();
                if (auto Idx = int32{INDEX_NONE}; Leaf.FindLastChar(TEXT('.'), Idx))
                { Leaf = Leaf.RightChop(Idx + 1); }

                return FText::FromString(FString::Printf(TEXT("tracing %s"), *Leaf));
            }));

    auto TraceClearVisibilityAttr = TAttribute<EVisibility>::Create(
        TAttribute<EVisibility>::FGetter::CreateLambda(
            [TracedKey]() -> EVisibility
            {
                return TracedKey().IsValid() ? EVisibility::Visible : EVisibility::Collapsed;
            }));

    return SNew(SBorder)
        .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Border.Subtle")))
        .Padding(FMargin(0.0f, 1.0f, 0.0f, 0.0f))   // top-only divider line
        [
            SNew(SBorder)
                .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Panel")))
                .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium,
                                 FCkGoapDebuggerStyle::Padding_Small))
                [
                    SNew(SHorizontalBox)

                        + SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Center)
                            .Padding(FMargin(0.0f, 0.0f, CkStyle::SpaceM, 0.0f))
                            [
                                SNew(SHorizontalBox)
                                    .ToolTipText(FText::FromString(TEXT("FKeyRegistry capacity — 64 boolean keys per World State")))

                                    + SHorizontalBox::Slot()
                                        .AutoWidth()
                                        .Padding(FMargin(0.0f, 0.0f, CkStyle::SpaceXS, 0.0f))
                                        [
                                            SNew(STextBlock)
                                                .Text(FText::FromString(TEXT("keys")))
                                                .Font(CkStyle::RegularFont(CkStyle::FontSizeMicro()))
                                                .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
                                        ]

                                    + SHorizontalBox::Slot()
                                        .AutoWidth()
                                        [
                                            SNew(SCkDebug_SelectableLabel)
                                                .Text(FText::FromString(FString::Printf(
                                                    TEXT("%d / %d"), InKeyCount, ck::goap::WorldState_MaxKeys)))
                                                .Font(CkStyle::BoldFont(CkStyle::FontSizeMicro()))
                                                .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                                        ]
                            ]

                        + SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Center)
                            [
                                SNew(SHorizontalBox)
                                    .ToolTipText(FText::FromString(TEXT("Request_AddSubscriber — entities re-planned when a key flips")))

                                    + SHorizontalBox::Slot()
                                        .AutoWidth()
                                        .Padding(FMargin(0.0f, 0.0f, CkStyle::SpaceXS, 0.0f))
                                        [
                                            SNew(STextBlock)
                                                .Text(FText::FromString(TEXT("subscribers")))
                                                .Font(CkStyle::RegularFont(CkStyle::FontSizeMicro()))
                                                .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
                                        ]

                                    + SHorizontalBox::Slot()
                                        .AutoWidth()
                                        [
                                            SNew(STextBlock)
                                                .Text(SubscriberTextAttr)
                                                .Font(CkStyle::BoldFont(CkStyle::FontSizeMicro()))
                                                .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                                        ]
                            ]

                        + SHorizontalBox::Slot()
                            .FillWidth(1.0f)
                            .HAlign(HAlign_Right)
                            .VAlign(VAlign_Center)
                            [
                                SNew(SHorizontalBox)

                                    + SHorizontalBox::Slot()
                                        .AutoWidth()
                                        .VAlign(VAlign_Center)
                                        [
                                            SNew(STextBlock)
                                                .Text(TraceHintTextAttr)
                                                .Font(CkStyle::RegularFont(CkStyle::FontSizeMicro()))
                                                .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
                                        ]

                                    + SHorizontalBox::Slot()
                                        .AutoWidth()
                                        .VAlign(VAlign_Center)
                                        .Padding(FMargin(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f))
                                        [
                                            SNew(SButton)
                                                .ButtonStyle(FCoreStyle::Get(), "NoBorder")
                                                .Visibility(TraceClearVisibilityAttr)
                                                .ContentPadding(FMargin(CkStyle::SpaceXS, 0.0f))
                                                .ToolTipText(FText::FromString(TEXT("Clear the key trace.")))
                                                .OnClicked_Lambda([WeakRail]() -> FReply
                                                {
                                                    const auto Pinned = WeakRail.Pin();
                                                    if (Pinned.IsValid() && Pinned->_ViewModel.IsValid())
                                                    { Pinned->_ViewModel->Set_TracedWsKey(FGameplayTag{}); }
                                                    return FReply::Handled();
                                                })
                                                [
                                                    SNew(STextBlock)
                                                        .Text(FText::FromString(TEXT("clear")))
                                                        .Font(CkStyle::BoldFont(CkStyle::FontSizeMicro()))
                                                        .ColorAndOpacity(FSlateColor(CkStyle::Accent()))
                                                ]
                                        ]
                            ]
                ]
        ];
}

// ====================================================================================================================
// BUILD — SEARCH + SORT BAR (fixed chrome)
// ====================================================================================================================

auto
    SCkGoapDebugger_WorldStateRail::
    BuildSearchAndSortBar()
    -> TSharedRef<SWidget>
{
    return SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(SCkDebug_DualSearchBar)
                    .FilterHintText(FText::FromString(TEXT("Filter keys…")))
                    .HighlightHintText(FText::FromString(TEXT("Highlight…")))
                    .OnFilterTextChanged_Lambda([this](const FString& InText)
                    {
                        if (_FilterString == InText) { return; }
                        _FilterString = InText;
                        RefreshFromViewModel();   // hash folds _FilterString → body rebuild
                    })
                    .OnHighlightTextChanged_Lambda([this](const FString& InText)
                    {
                        if (_HighlightString == InText) { return; }
                        _HighlightString = InText;
                        RefreshFromViewModel();   // hash folds _HighlightString → body rebuild
                    })
            ]

        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Small, 0.0f, 0.0f, 0.0f))
            [
                SNew(SButton)
                    .ToolTipText(FText::FromString(TEXT("Toggle key-row sort order:\nName — alphabetical (default)\nTRUE — true values first, then alphabetical")))
                    .OnClicked(this, &SCkGoapDebugger_WorldStateRail::HandleClick_CycleSortMode)
                    .ContentPadding(FMargin{FCkGoapDebuggerStyle::Padding_Small, 1.0f})
                    [
                        SNew(STextBlock)
                            // Plain text — the ↕ glyph renders poorly in Slate's default font.
                            .Text_Lambda([this]() -> FText
                            {
                                return FText::FromString(_SortMode == ECkGoapDebugger_WsSortMode::ByName
                                    ? TEXT("Sort: Name")
                                    : TEXT("Sort: TRUE"));
                            })
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
                            .ColorAndOpacity(FSlateColor(CkStyle::Accent()))
                    ]
            ];
}

auto
    SCkGoapDebugger_WorldStateRail::
    HandleClick_CycleSortMode()
    -> FReply
{
    _SortMode = _SortMode == ECkGoapDebugger_WsSortMode::ByName
        ? ECkGoapDebugger_WsSortMode::ByTrueFirst
        : ECkGoapDebugger_WsSortMode::ByName;

    RefreshFromViewModel();   // hash folds _SortMode → body rebuild
    return FReply::Handled();
}

// ====================================================================================================================
// BUILD — KEY ROW
// ====================================================================================================================

auto
    SCkGoapDebugger_WorldStateRail::
    BuildKeyRow(
        const FCkGoapDebugger_WorldStateEntry& InEntry,
        bool InHighlightDimmed,
        FIntPoint InUsage)
    -> TSharedRef<SWidget>
{
    using namespace ck_goap_debugger_wsrail_internal;

    // First apply tag-depth truncation (so toolbar +/- toggles the displayed
    // segment count), then char-length truncation as a final fallback for
    // pathologically long single segments.
    const auto FullKey = InEntry.Key.IsValid() ? InEntry.Key.ToString() : FString(TEXT("(invalid)"));
    const auto Depth = _ViewModel.IsValid() ? _ViewModel->Get_NameDepth() : 0;
    const auto KeyText = TruncateKey(SCkDebug_NameLabel::Get_ShortName(FullKey, Depth));
    const auto KeyCol = InHighlightDimmed ? CkStyle::TextMute() : CkStyle::TextDim();
    const auto EntryKey = InEntry.Key;
    const bool SnapshotValue = InEntry.Value;
    const bool RecentlyChanged = InEntry.RecentlyChanged;
    const auto WeakRail = TWeakPtr<SCkGoapDebugger_WorldStateRail>(SharedThis(this));

    // ---- TAttribute lambdas — live updates without rail rebuild -------------
    const auto HasOverride = [WeakRail, EntryKey]() -> bool
    {
        const auto Pinned = WeakRail.Pin();
        if (NOT Pinned.IsValid()) { return false; }
        auto WsHandle = Pinned->_CurrentWorldState;
        if (NOT ck::IsValid(WsHandle)) { return false; }
        return UCk_Utils_Goap_WorldState_UE::Has_KeyOverride(WsHandle, EntryKey);
    };

    // Layer shadow badge — names the top-most shadowing layer (mockup shows
    // "DebugUI"; gameplay layers like "FlierAttract" name themselves too).
    auto ShadowVisibilityAttr = TAttribute<EVisibility>::Create(
        TAttribute<EVisibility>::FGetter::CreateLambda(
            [HasOverride]() -> EVisibility
            { return HasOverride() ? EVisibility::Visible : EVisibility::Collapsed; }));

    auto ShadowTextAttr = TAttribute<FText>::Create(
        TAttribute<FText>::FGetter::CreateLambda(
            [WeakRail, EntryKey]() -> FText
            {
                const auto Pinned = WeakRail.Pin();
                if (NOT Pinned.IsValid()) { return FText::GetEmpty(); }
                auto WsHandle = Pinned->_CurrentWorldState;
                if (NOT ck::IsValid(WsHandle)) { return FText::GetEmpty(); }
                const auto LayerName = UCk_Utils_Goap_WorldState_UE::Get_TopOverrideLayerForKey(WsHandle, EntryKey);
                return LayerName.IsNone() ? FText::GetEmpty() : FText::FromName(LayerName);
            }));

    auto ShadowTooltipAttr = TAttribute<FText>::Create(
        TAttribute<FText>::FGetter::CreateLambda(
            [WeakRail, EntryKey]() -> FText
            {
                const auto Fallback = FText::FromString(TEXT("Shadowed by an override layer."));
                const auto Pinned = WeakRail.Pin();
                if (NOT Pinned.IsValid()) { return Fallback; }
                auto WsHandle = Pinned->_CurrentWorldState;
                if (NOT ck::IsValid(WsHandle)) { return Fallback; }
                const auto LayerName = UCk_Utils_Goap_WorldState_UE::Get_TopOverrideLayerForKey(WsHandle, EntryKey);
                if (LayerName.IsNone()) { return Fallback; }
                return FText::FromString(FString::Printf(
                    TEXT("Shadowed by layer: %s\n\nTop-of-stack wins. Pop this layer in the Override Layers section above to reveal the base or next-lower-layer value."),
                    *LayerName.ToString()));
            }));

    // "just changed" chip — snapshot's RecentlyChanged, hidden while an
    // override shadows the key (the shadow badge carries the row then).
    auto JustChangedVisibilityAttr = TAttribute<EVisibility>::Create(
        TAttribute<EVisibility>::FGetter::CreateLambda(
            [HasOverride, RecentlyChanged]() -> EVisibility
            {
                if (NOT RecentlyChanged) { return EVisibility::Collapsed; }
                return HasOverride() ? EVisibility::Collapsed : EVisibility::Visible;
            }));

    // Pill value — live effective read (override-aware) so sandbox flips show
    // instantly; falls back to the snapshot value if the handle died.
    auto PillValueAttr = TAttribute<bool>::Create(
        TAttribute<bool>::FGetter::CreateLambda(
            [WeakRail, EntryKey, SnapshotValue]() -> bool
            {
                const auto Pinned = WeakRail.Pin();
                if (NOT Pinned.IsValid()) { return SnapshotValue; }
                auto WsHandle = Pinned->_CurrentWorldState;
                if (NOT ck::IsValid(WsHandle)) { return SnapshotValue; }
                return UCk_Utils_Goap_WorldState_UE::Get_Value(WsHandle, EntryKey);
            }));

    auto PillEditableAttr = TAttribute<bool>::Create(
        TAttribute<bool>::FGetter::CreateLambda(
            [WeakRail]() -> bool
            {
                const auto Pinned = WeakRail.Pin();
                if (NOT Pinned.IsValid()) { return false; }
                return Pinned->_SandboxMode && ck::IsValid(Pinned->_CurrentWorldState);
            }));

    // Row background — traced = accent wash (cross-pane trace), otherwise the
    // amber wash for shadowed / just-changed rows, else transparent.
    auto RowBgColorAttr = TAttribute<FSlateColor>::Create(
        TAttribute<FSlateColor>::FGetter::CreateLambda(
            [WeakRail, EntryKey, HasOverride, RecentlyChanged]() -> FSlateColor
            {
                const auto Pinned = WeakRail.Pin();
                if (Pinned.IsValid() && Pinned->_ViewModel.IsValid() &&
                    Pinned->_ViewModel->Get_TracedWsKey() == EntryKey)
                { return FSlateColor(CkStyle::AccentDim()); }

                if (RecentlyChanged || HasOverride())
                { return FSlateColor(CkStyle::WarnDim()); }

                return FSlateColor(FLinearColor::Transparent);
            }));

    auto RowContent = SNew(SHorizontalBox)

        // Key name
        + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(KeyText))
                    .ToolTipText(FText::FromString(InEntry.Key.IsValid() ? InEntry.Key.ToString() : FString{}))
                    .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
                    .ColorAndOpacity(FSlateColor(KeyCol))
            ]

        // Usage census — "nP·mE" (precondition/goal reads · effect writes)
        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(CkStyle::SpaceS, 0.0f))
            [
                SNew(STextBlock)
                    .Text(FText::FromString(FString::Printf(TEXT("%dP·%dE"), InUsage.X, InUsage.Y)))
                    .ToolTipText(FText::FromString(FString::Printf(
                        TEXT("%d precondition/goal reads · %d effect writes"), InUsage.X, InUsage.Y)))
                    .Font(CkStyle::MonoFont(CkStyle::FontSizeMicro()))
                    .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
            ]

        // Layer shadow badge — live text names the shadowing layer.
        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(0.0f, 0.0f, CkStyle::SpaceS, 0.0f))
            [
                SNew(SBorder)
                    .Visibility(ShadowVisibilityAttr)
                    .BorderImage(CkStyle::GetFilledBrush())
                    .BorderBackgroundColor(FSlateColor(CkStyle::WarnDim()))
                    .Padding(FMargin(CkStyle::SpaceS, 0.0f))
                    .ToolTipText(ShadowTooltipAttr)
                    [
                        SNew(STextBlock)
                            .Text(ShadowTextAttr)
                            .Font(CkStyle::BoldFont(CkStyle::FontSizeMicro()))
                            .ColorAndOpacity(FSlateColor(CkStyle::Warn()))
                    ]
            ]

        // "just changed" chip
        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(0.0f, 0.0f, CkStyle::SpaceS, 0.0f))
            [
                SNew(SBorder)
                    .Visibility(JustChangedVisibilityAttr)
                    .BorderImage(CkStyle::GetFilledBrush())
                    .BorderBackgroundColor(FSlateColor(CkStyle::WarnDim()))
                    .Padding(FMargin(CkStyle::SpaceS, 0.0f))
                    .ToolTipText(FText::FromString(TEXT("Value changed since the previous tick.")))
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(TEXT("just changed")))
                            .Font(CkStyle::RegularFont(CkStyle::FontSizeMicro()))
                            .ColorAndOpacity(FSlateColor(CkStyle::Warn()))
                    ]
            ]

        // Base-store toggle — the truth table. Set_Value writes the BASE store, which is what every
        // planner falls through to; the pill beside it writes the DebugUI override layer instead. Two
        // controls because they are two different writes, not two ways to do one.
        //
        // Disabled while a layer shadows this key: the write would land on the base and the row would
        // not move (the override still wins the read), which reads as a broken control. The tooltip
        // names the layer to pop.
        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(0.0f, 0.0f, CkStyle::SpaceS, 0.0f))
            [
                SNew(SBox)
                    .HAlign(HAlign_Left)
                    .IsEnabled(TAttribute<bool>::Create(
                        TAttribute<bool>::FGetter::CreateLambda(
                            [WeakRail, HasOverride]() -> bool
                            {
                                const auto Pinned = WeakRail.Pin();
                                if (NOT Pinned.IsValid() || NOT ck::IsValid(Pinned->_CurrentWorldState)) { return false; }
                                return NOT HasOverride();
                            })))
                    .ToolTipText(TAttribute<FText>::Create(
                        TAttribute<FText>::FGetter::CreateLambda(
                            [WeakRail, EntryKey, HasOverride]() -> FText
                            {
                                if (NOT HasOverride())
                                { return FText::FromString(TEXT("Base store (Set_Value). Flips what every planner believes — no layer, nothing to pop.")); }

                                const auto Pinned = WeakRail.Pin();
                                auto LayerName = FName{};
                                if (Pinned.IsValid() && ck::IsValid(Pinned->_CurrentWorldState))
                                { LayerName = UCk_Utils_Goap_WorldState_UE::Get_TopOverrideLayerForKey(Pinned->_CurrentWorldState, EntryKey); }

                                return FText::FromString(FString::Printf(
                                    TEXT("Base edit disabled: layer '%s' shadows this key, so a base write would not change what anyone reads. Pop the layer first."),
                                    LayerName.IsNone() ? TEXT("?") : *LayerName.ToString()));
                            })))
                    [
                        SNew(SCkDebug_Switch)
                            .IsOn(PillValueAttr)
                            .OnStateChanged(FOnCkDebug_SwitchChanged::CreateSP(
                                this,
                                &SCkGoapDebugger_WorldStateRail::HandleBaseValueToggled,
                                EntryKey))
                    ]
            ]

        // Value pill — editable only in Sandbox mode.
        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SCkDebug_ValuePill)
                    .Value(PillValueAttr)
                    .Editable(PillEditableAttr)
                    .ToolTipText_Lambda([WeakRail]() -> FText
                    {
                        const auto Pinned = WeakRail.Pin();
                        const auto Sandbox = Pinned.IsValid() && Pinned->_SandboxMode;
                        return FText::FromString(Sandbox
                            ? TEXT("Click to flip in the DebugUI layer")
                            : TEXT("Live value — enable Sandbox to experiment"));
                    })
                    .OnToggled(FOnCkDebug_ValuePillToggled::CreateSP(
                        this,
                        &SCkGoapDebugger_WorldStateRail::HandlePillToggled,
                        EntryKey))
            ];

    // The clickable row — click traces the key across panes (the pill handles
    // its own clicks when editable, so sandbox flips don't retarget the trace).
    return SNew(SBorder)
        .BorderImage(CkStyle::GetFilledBrush())
        .BorderBackgroundColor(RowBgColorAttr)
        .Padding(FMargin(0.0f))
        [
            SNew(SButton)
                .ButtonStyle(FCoreStyle::Get(), "NoBorder")
                .ContentPadding(ck_goap_debugger_axes::Apply_RowDensity(
                    FMargin{FCkGoapDebuggerStyle::Padding_Small, 1.0f}))
                .ToolTipText(FText::FromString(TEXT("Click to trace this key across panes (plan chips, decision cards, graph).")))
                .OnClicked(FOnClicked::CreateSP(
                    this,
                    &SCkGoapDebugger_WorldStateRail::HandleRowClicked_Trace,
                    EntryKey))
                [
                    RowContent
                ]
        ];
}

// ====================================================================================================================
// HANDLERS — Sandbox / DebugUI override layer / key trace
// ====================================================================================================================

auto
    SCkGoapDebugger_WorldStateRail::
    HandlePillToggled(
        bool InNewValue,
        FGameplayTag InKey)
    -> void
{
    using namespace ck_goap_debugger_wsrail_internal;

    if (NOT _SandboxMode)                    { return; }
    if (NOT ck::IsValid(_CurrentWorldState)) { return; }
    if (NOT InKey.IsValid())                 { return; }

    // Push or update the DebugUI layer with the pill's new value. The API is
    // idempotent and creates the layer on first call; subsequent flips on the
    // same key update the override in place.
    auto MutableWs = _CurrentWorldState;
    UCk_Utils_Goap_WorldState_UE::Push_Override_SingleKey(
        MutableWs,
        WsRail_DebugUiLayerName,
        InKey,
        InNewValue);
}

auto
    SCkGoapDebugger_WorldStateRail::
    HandleBaseValueToggled(
        bool InNewValue,
        FGameplayTag InKey)
    -> void
{
    if (NOT ck::IsValid(_CurrentWorldState)) { return; }
    if (NOT InKey.IsValid())                 { return; }

    // Deliberately NOT gated on Sandbox: the sandbox exists so an override layer can be popped to undo
    // an experiment, and this write has no layer to pop. It is the honest truth-table edit — the same
    // call gameplay makes — and the row's switch is disabled whenever a layer would hide its effect.
    auto MutableWs = _CurrentWorldState;
    UCk_Utils_Goap_WorldState_UE::Set_Value(MutableWs, InKey, InNewValue, {});
}

auto
    SCkGoapDebugger_WorldStateRail::
    HandleSandboxToggled(
        bool InNewState)
    -> void
{
    _SandboxMode = InNewState;

    // Leaving sandbox pops the DebugUI layer — live truth restored (mockup
    // semantics). Gameplay-pushed layers are untouched.
    if (NOT InNewState)
    { HandleClick_ResetDebugUiLayer(); }
}

auto
    SCkGoapDebugger_WorldStateRail::
    HandleRowClicked_Trace(
        FGameplayTag InKey)
    -> FReply
{
    if (NOT _ViewModel.IsValid()) { return FReply::Handled(); }
    if (NOT InKey.IsValid())      { return FReply::Handled(); }

    // Click again to clear the trace.
    const auto NewTrace = _ViewModel->Get_TracedWsKey() == InKey ? FGameplayTag{} : InKey;
    _ViewModel->Set_TracedWsKey(NewTrace);

    return FReply::Handled();
}

auto
    SCkGoapDebugger_WorldStateRail::
    HandleClick_ResetDebugUiLayer()
    -> FReply
{
    using namespace ck_goap_debugger_wsrail_internal;

    if (NOT ck::IsValid(_CurrentWorldState)) { return FReply::Handled(); }

    // Pop ONLY the DebugUI layer — preserves any AI-deliberation layers that
    // might have been pushed concurrently (e.g., hypothesis scopes).
    auto MutableWs = _CurrentWorldState;
    UCk_Utils_Goap_WorldState_UE::Pop_Override_ByName(
        MutableWs,
        WsRail_DebugUiLayerName);

    return FReply::Handled();
}

auto
    SCkGoapDebugger_WorldStateRail::
    HandleClick_ClearAllOverrides()
    -> FReply
{
    if (NOT ck::IsValid(_CurrentWorldState)) { return FReply::Handled(); }

    // Clear_Overrides drops the WHOLE stack, gameplay-pushed layers included — that is the point of
    // having it beside the per-layer Pop, and why it lives on the stack header rather than a row.
    auto MutableWs = _CurrentWorldState;
    UCk_Utils_Goap_WorldState_UE::Clear_Overrides(MutableWs);

    _ExpandedLayers.Reset();

    return FReply::Handled();
}

auto
    SCkGoapDebugger_WorldStateRail::
    HandleClick_PopLayer(FName InLayerName)
    -> FReply
{
    if (NOT ck::IsValid(_CurrentWorldState)) { return FReply::Handled(); }
    if (InLayerName.IsNone())                { return FReply::Handled(); }

    auto MutableWs = _CurrentWorldState;
    UCk_Utils_Goap_WorldState_UE::Pop_Override_ByName(MutableWs, InLayerName);

    // Also clean up our expand-state set so we don't carry stale entries.
    _ExpandedLayers.Remove(InLayerName);

    return FReply::Handled();
}

auto
    SCkGoapDebugger_WorldStateRail::
    HandleClick_ToggleLayerExpand(FName InLayerName)
    -> FReply
{
    if (InLayerName.IsNone()) { return FReply::Handled(); }

    if (_ExpandedLayers.Contains(InLayerName))
    { _ExpandedLayers.Remove(InLayerName); }
    else
    { _ExpandedLayers.Add(InLayerName); }

    // Force a rebuild — the layer-expand state lives in our hash so a manual
    // refresh wouldn't catch the toggle on its own (no underlying WS change).
    _HasMaterialized = false;
    RefreshFromViewModel();
    return FReply::Handled();
}

// ====================================================================================================================
// BUILD — OVERRIDE LAYERS SECTION (top of body when override stack non-empty)
// ====================================================================================================================

auto
    SCkGoapDebugger_WorldStateRail::
    BuildOverrideLayersSection(const FCk_Handle_Goap_WorldState& InWs, int32 InBaseKeyCount)
    -> TSharedRef<SWidget>
{
    const auto LayerNames = UCk_Utils_Goap_WorldState_UE::Get_OverrideLayerNames(InWs);
    const auto WeakRail   = TWeakPtr<SCkGoapDebugger_WorldStateRail>(SharedThis(this));

    auto Section = SNew(SVerticalBox);

    // Section header — mockup: "Override layers · depth N".
    Section->AddSlot()
        .AutoHeight()
        .Padding(FMargin(0.0f, 0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Small))
        [
            SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(SCkDebug_SelectableLabel)
                            .Text(FText::FromString(TEXT("OVERRIDE LAYERS")))
                            .Font(CkStyle::BoldFont(CkStyle::FontSizeMicro()))
                            .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
                    ]

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(FMargin(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f))
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(FString::Printf(
                                TEXT("· depth %d"), LayerNames.Num())))
                            .Font(CkStyle::MonoFont(CkStyle::FontSizeMicro()))
                            .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
                    ]

                // Clear-all. Visibility is an ATTRIBUTE, not the build-time LayerNames count: pushing a
                // layer (a sandbox pill flip) deliberately does not rebuild this section, so a static
                // gate would leave the button hidden exactly when it becomes useful.
                + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .HAlign(HAlign_Right)
                    .VAlign(VAlign_Center)
                    [
                        SNew(SButton)
                            .Visibility(TAttribute<EVisibility>::Create(
                                TAttribute<EVisibility>::FGetter::CreateLambda(
                                    [WeakRail]() -> EVisibility
                                    {
                                        const auto Pinned = WeakRail.Pin();
                                        if (NOT Pinned.IsValid() || NOT ck::IsValid(Pinned->_CurrentWorldState))
                                        { return EVisibility::Collapsed; }

                                        return UCk_Utils_Goap_WorldState_UE::Get_OverrideDepth(Pinned->_CurrentWorldState) > 0
                                            ? EVisibility::Visible : EVisibility::Collapsed;
                                    })))
                            .ToolTipText(FText::FromString(TEXT(
                                "Clear_Overrides — pops EVERY layer at once, including gameplay-pushed ones "
                                "(a per-layer Pop and the Sandbox switch both leave those alone). Reads fall back to the base store.")))
                            .OnClicked(FOnClicked::CreateSP(
                                this,
                                &SCkGoapDebugger_WorldStateRail::HandleClick_ClearAllOverrides))
                            .ContentPadding(FMargin{FCkGoapDebuggerStyle::Padding_Small, 1.0f})
                            [
                                SNew(STextBlock)
                                    .Text(FText::FromString(TEXT("Clear all")))
                                    .Font(CkStyle::BoldFont(CkStyle::FontSizeMicro()))
                                    .ColorAndOpacity(FSlateColor(CkStyle::Err()))
                            ]
                    ]
        ];

    // One row per layer, rendered top-of-stack first (most recent / wins)
    for (auto i = LayerNames.Num() - 1; i >= 0; --i)
    {
        Section->AddSlot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 0.0f, 0.0f, 1.0f))
            [
                BuildOverrideLayerRow(InWs, LayerNames[i])
            ];
    }

    // Base store — permanent bottom row; every read falls through to it when
    // no layer shadows the key.
    Section->AddSlot()
        .AutoHeight()
        .Padding(ck_goap_debugger_axes::Apply_RowDensity(
            FMargin{FCkGoapDebuggerStyle::Padding_Small, 1.0f}))
        [
            SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    [
                        SNew(SCkDebug_SelectableLabel)
                            .Text(FText::FromString(TEXT("base store")))
                            .Font(CkStyle::RegularFont(CkStyle::FontSizeMicro()))
                            .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                    ]

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(FString::Printf(
                                TEXT("%d key%s"), InBaseKeyCount, InBaseKeyCount == 1 ? TEXT("") : TEXT("s"))))
                            .Font(CkStyle::MonoFont(CkStyle::FontSizeMicro()))
                            .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
                    ]
        ];

    return SNew(SBorder)
        .BorderImage(CkStyle::GetFilledBrush())
        .BorderBackgroundColor(FSlateColor(CkStyle::Bg2()))
        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium,
                         FCkGoapDebuggerStyle::Padding_Small))
        [
            Section
        ];
}

auto
    SCkGoapDebugger_WorldStateRail::
    BuildOverrideLayerRow(const FCk_Handle_Goap_WorldState& InWs, FName InLayerName)
    -> TSharedRef<SWidget>
{
    const auto IsExpanded = _ExpandedLayers.Contains(InLayerName);
    const auto KeyCount   = UCk_Utils_Goap_WorldState_UE::Get_LayerKeyCount(InWs, InLayerName);

    // Header — caret + layer name + key count + Pop button. The whole left side
    // (caret + name + count) is one button so the click target is generous.
    auto Header = SNew(SHorizontalBox)

        // Expand-toggle row (caret + name + count) — wide click target.
        + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(SButton)
                    .ButtonStyle(FCoreStyle::Get(), "NoBorder")
                    .ContentPadding(ck_goap_debugger_axes::Apply_RowDensity(
                        FMargin{FCkGoapDebuggerStyle::Padding_Small, 1.0f}))
                    .ToolTipText(FText::FromString(FString::Printf(
                        TEXT("Click to %s this layer's keys."),
                        IsExpanded ? TEXT("collapse") : TEXT("expand"))))
                    .OnClicked(FOnClicked::CreateSP(
                        this,
                        &SCkGoapDebugger_WorldStateRail::HandleClick_ToggleLayerExpand,
                        InLayerName))
                    [
                        SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                                .AutoWidth()
                                .VAlign(VAlign_Center)
                                .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Small, 0.0f)
                                [
                                    SNew(STextBlock)
                                        .Text(FText::FromString(IsExpanded ? TEXT("v") : TEXT(">")))
                                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                                        .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                                ]
                            + SHorizontalBox::Slot()
                                .AutoWidth()
                                .VAlign(VAlign_Center)
                                [
                                    SNew(SCkDebug_SelectableLabel)
                                        .Text(FText::FromString(InLayerName.ToString()))
                                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                                        .ColorAndOpacity(FSlateColor(CkStyle::Warn()))
                                ]
                            + SHorizontalBox::Slot()
                                .AutoWidth()
                                .VAlign(VAlign_Center)
                                .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Small, 0.0f, 0.0f, 0.0f))
                                [
                                    SNew(SCkDebug_SelectableLabel)
                                        .Text(FText::FromString(FString::Printf(
                                            TEXT("· %d key%s"),
                                            KeyCount, KeyCount == 1 ? TEXT("") : TEXT("s"))))
                                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                                        .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                                ]
                    ]
            ]

        // Pop button — narrow, right-aligned.
        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Small, 0.0f, 0.0f, 0.0f))
            [
                SNew(SButton)
                    .ToolTipText(FText::FromString(FString::Printf(
                        TEXT("Pop the '%s' layer. Removes it from the stack; the next-lower-layer or base value is revealed for every key it carried."),
                        *InLayerName.ToString())))
                    .OnClicked(FOnClicked::CreateSP(
                        this,
                        &SCkGoapDebugger_WorldStateRail::HandleClick_PopLayer,
                        InLayerName))
                    .ContentPadding(FMargin{FCkGoapDebuggerStyle::Padding_Small, 1.0f})
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(TEXT("Pop")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
                            .ColorAndOpacity(FSlateColor(CkStyle::Err()))
                    ]
            ];

    auto Row = SNew(SVerticalBox)
        + SVerticalBox::Slot()
            .AutoHeight()
            [
                Header
            ];

    // Per-key drilldown (visible only when expanded).
    if (IsExpanded)
    {
        const auto Values = UCk_Utils_Goap_WorldState_UE::Get_LayerValues(InWs, InLayerName);

        auto KeysBox = SNew(SVerticalBox);
        for (const auto& Kv : Values)
        {
            const auto ValueStr = Kv.Value ? FString(TEXT("TRUE")) : FString(TEXT("false"));
            const auto ValueCol = Kv.Value
                ? CkStyle::Ok()
                : CkStyle::TextMute();

            KeysBox->AddSlot()
                .AutoHeight()
                .Padding(ck_goap_debugger_axes::Apply_RowDensity(
                FMargin{FCkGoapDebuggerStyle::Padding_Medium, 1.0f, 0.0f, 1.0f}))
                [
                    SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                            .FillWidth(1.0f)
                            .VAlign(VAlign_Center)
                            [
                                SNew(SCkDebug_SelectableLabel)
                                    .Text(FText::FromString(Kv.Key.ToString()))
                                    .Font(FCoreStyle::GetDefaultFontStyle("Mono", 8))
                                    .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                            ]
                        + SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Center)
                            [
                                SNew(SCkDebug_SelectableLabel)
                                    .Text(FText::FromString(ValueStr))
                                    .Font(Kv.Value
                                        ? FCoreStyle::GetDefaultFontStyle("Bold", 8)
                                        : FCoreStyle::GetDefaultFontStyle("Regular", 8))
                                    .ColorAndOpacity(FSlateColor(ValueCol))
                            ]
                ];
        }

        Row->AddSlot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 2.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Small))
            [
                KeysBox
            ];
    }

    return Row;
}

// ====================================================================================================================
