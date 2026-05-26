#include "CkGoapDebugger/Window/SCkGoapDebugger_WorldStateRail.h"

#include "CkGoapDebugger/CkGoapDebuggerStyle.h"
#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"

#include "CkGoap/WorldState/CkGoap_WorldState_Utils.h"

#include "Styling/CoreStyle.h"
#include "Styling/StyleDefaults.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
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

    // Tag-depth truncation: keep the last InDepth dot-separated segments of a
    // gameplay tag. Depth 0 = full tag (no truncation). Depth 1 = leaf segment.
    // Mirrors FCkGoapDebugger_NameParams::ComputeDisplayName but operates on
    // tag-style strings (period-separated) instead of class names.
    auto TruncateTagByDepth(const FString& InTag, int32 InDepth) -> FString
    {
        if (InDepth <= 0) { return InTag; }
        TArray<FString> Segments;
        InTag.ParseIntoArray(Segments, TEXT("."), true);
        if (Segments.Num() <= InDepth) { return InTag; }
        auto Tail = TArray<FString>{};
        Tail.Reserve(InDepth);
        for (auto i = Segments.Num() - InDepth; i < Segments.Num(); ++i)
        { Tail.Add(Segments[i]); }
        return FString::Join(Tail, TEXT("."));
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
                            SNew(SSeparator)
                                .Thickness(1.0f)
                                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Border_Subtle))
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
    // Fold in the shared name-depth so toolbar +/- toggles re-render the rail
    // with new key truncation. WS keys honour the same depth as planner-tier
    // display names — see TruncateTagByDepth below.
    if (_ViewModel.IsValid())
    { NewHash = HashCombine(NewHash, ::GetTypeHash(_ViewModel->Get_NameDepth())); }

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
        {
            _HeaderHost->SetContent(
                SNew(SBorder)
                    .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Panel")))
                    .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium,
                                     FCkGoapDebuggerStyle::Padding_Medium))
                    [
                        SNew(SCkDebug_SelectableLabel)
                            .Text(FText::FromString(TEXT("WORLDSTATE")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted))
                    ]);
        }

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

    // ---- Header (label + key count) ------------------------------------------
    const auto KeyCount = Entries->Num();
    auto RecentCount    = 0;
    for (const auto& E : *Entries)
    { if (E.RecentlyChanged) { ++RecentCount; } }

    if (_HeaderHost.IsValid())
    {
        const auto CountText = FString::Printf(TEXT("%d keys"), KeyCount);

        // The "[+N layer]" suffix is bound via TAttribute so push/pop of the
        // DebugUI layer updates the header text live, without forcing the
        // rail to rebuild. Per CkDebuggerCommon SGraphNode live-bind invariant.
        auto HeaderTextAttr = TAttribute<FText>::Create(
            TAttribute<FText>::FGetter::CreateLambda(
                [WeakThis = TWeakPtr<SCkGoapDebugger_WorldStateRail>(SharedThis(this)),
                 Label]() -> FText
                {
                    const auto Pinned = WeakThis.Pin();
                    if (NOT Pinned.IsValid())
                    { return FText::FromString(TEXT("WORLDSTATE")); }

                    auto Base = FString::Printf(TEXT("WORLDSTATE · %s"),
                        Label.IsEmpty() ? TEXT("(unresolved)") : *Label);

                    auto WsHandle = Pinned->_CurrentWorldState;
                    if (ck::IsValid(WsHandle))
                    {
                        const auto Depth = UCk_Utils_Goap_WorldState_UE::Get_OverrideDepth(WsHandle);
                        if (Depth > 0)
                        {
                            Base += FString::Printf(TEXT("  [+%d layer%s]"),
                                Depth, Depth == 1 ? TEXT("") : TEXT("s"));
                        }
                    }
                    return FText::FromString(Base);
                }));

        // Reset button visibility: only when DebugUI layer is active.
        auto ResetVisibilityAttr = TAttribute<EVisibility>::Create(
            TAttribute<EVisibility>::FGetter::CreateLambda(
                [WeakThis = TWeakPtr<SCkGoapDebugger_WorldStateRail>(SharedThis(this))]() -> EVisibility
                {
                    const auto Pinned = WeakThis.Pin();
                    if (NOT Pinned.IsValid()) { return EVisibility::Collapsed; }

                    auto WsHandle = Pinned->_CurrentWorldState;
                    if (NOT ck::IsValid(WsHandle)) { return EVisibility::Collapsed; }

                    return UCk_Utils_Goap_WorldState_UE::Get_OverrideDepth(WsHandle) > 0
                        ? EVisibility::Visible
                        : EVisibility::Collapsed;
                }));

        // Reset tooltip: list currently-pushed layer names (live).
        auto ResetTooltipAttr = TAttribute<FText>::Create(
            TAttribute<FText>::FGetter::CreateLambda(
                [WeakThis = TWeakPtr<SCkGoapDebugger_WorldStateRail>(SharedThis(this))]() -> FText
                {
                    const auto Pinned = WeakThis.Pin();
                    if (NOT Pinned.IsValid())
                    { return FText::FromString(TEXT("Reset DebugUI override layer.")); }

                    auto WsHandle = Pinned->_CurrentWorldState;
                    if (NOT ck::IsValid(WsHandle))
                    { return FText::FromString(TEXT("Reset DebugUI override layer.")); }

                    const auto Layers = UCk_Utils_Goap_WorldState_UE::Get_OverrideLayerNames(WsHandle);
                    if (Layers.Num() == 0)
                    { return FText::FromString(TEXT("Reset DebugUI override layer.")); }

                    auto Joined = FString{};
                    for (auto Idx = 0; Idx < Layers.Num(); ++Idx)
                    {
                        if (Idx > 0) { Joined += TEXT(", "); }
                        Joined += Layers[Idx].ToString();
                    }
                    return FText::FromString(FString::Printf(
                        TEXT("Pop the \"DebugUI\" override layer.\nActive layers: %s"),
                        *Joined));
                }));

        _HeaderHost->SetContent(
            SNew(SBorder)
                .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Panel")))
                .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium,
                                 FCkGoapDebuggerStyle::Padding_Medium))
                [
                    SNew(SHorizontalBox)

                        + SHorizontalBox::Slot()
                            .FillWidth(1.0f)
                            .VAlign(VAlign_Center)
                            [
                                SNew(SCkDebug_SelectableLabel)
                                    .Text(HeaderTextAttr)
                                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                                    .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted))
                            ]

                        + SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Center)
                            .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Small, 0.0f, 0.0f, 0.0f))
                            [
                                SNew(SButton)
                                    .Visibility(ResetVisibilityAttr)
                                    .ToolTipText(ResetTooltipAttr)
                                    .OnClicked(this, &SCkGoapDebugger_WorldStateRail::HandleClick_ResetDebugUiLayer)
                                    .ContentPadding(FMargin(FCkGoapDebuggerStyle::Padding_Small, 1.0f))
                                    [
                                        SNew(SCkDebug_SelectableLabel)
                                            .Text(FText::FromString(TEXT("Reset")))
                                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
                                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Status_Selected))
                                    ]
                            ]

                        + SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Center)
                            .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Small, 0.0f, 0.0f, 0.0f))
                            [
                                SNew(SCkDebug_SelectableLabel)
                                    .Text(FText::FromString(CountText))
                                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                                    .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Status_Planning))
                            ]
                ]);
    }

    // ---- Body (override layers section + rows) -------------------------------
    if (_Body.IsValid())
    {
        _Body->ClearChildren();

        // Override-stack inspector — only visible when one or more layers are
        // pushed. Sits above the key rows so layer state is the first thing the
        // user sees when overrides are active.
        if (ck::IsValid(_CurrentWorldState) &&
            UCk_Utils_Goap_WorldState_UE::Get_OverrideDepth(_CurrentWorldState) > 0)
        {
            _Body->AddSlot()
                .AutoHeight()
                .Padding(FMargin(0.0f, 0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium))
                [
                    BuildOverrideLayersSection(_CurrentWorldState)
                ];
        }

        for (const auto& Entry : *Entries)
        {
            _Body->AddSlot()
                .AutoHeight()
                [
                    BuildKeyRow(Entry)
                ];
        }
    }

    // ---- Footer ---------------------------------------------------------------
    if (_FooterHost.IsValid())
    {
        const auto FooterText = FString::Printf(
            TEXT("★ = recently changed · %d of %d keys"), RecentCount, KeyCount);

        _FooterHost->SetContent(
            SNew(SBorder)
                .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Border.Subtle")))
                .Padding(FMargin(0.0f, 1.0f, 0.0f, 0.0f))   // top-only divider line
                [
                    SNew(SBorder)
                        .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Panel")))
                        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium,
                                         FCkGoapDebuggerStyle::Padding_Small))
                        [
                            SNew(SCkDebug_SelectableLabel)
                                .Text(FText::FromString(FooterText))
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim))
                        ]
                ]);
    }
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
                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim))
        ];
}

// ====================================================================================================================
// BUILD — KEY ROW
// ====================================================================================================================

auto
    SCkGoapDebugger_WorldStateRail::
    BuildKeyRow(
        const FCkGoapDebugger_WorldStateEntry& InEntry)
    -> TSharedRef<SWidget>
{
    using namespace ck_goap_debugger_wsrail_internal;

    // First apply tag-depth truncation (so toolbar +/- toggles the displayed
    // segment count), then char-length truncation as a final fallback for
    // pathologically long single segments.
    const auto FullKey = InEntry.Key.IsValid() ? InEntry.Key.ToString() : FString(TEXT("(invalid)"));
    const auto Depth = _ViewModel.IsValid() ? _ViewModel->Get_NameDepth() : 0;
    const auto KeyText = TruncateKey(TruncateTagByDepth(FullKey, Depth));
    const auto KeyCol   = FCkGoapDebuggerStyle::Color_Text_Secondary;
    const auto EntryKey = InEntry.Key;
    const bool EffectiveValue = InEntry.Value;

    // The displayed value follows the resolved (post-override) WS view that
    // the data collector already flattened into _Entries.Value. It stays in
    // sync with the snapshot, so on click we flip from THAT value.
    const auto ValueStr = EffectiveValue ? FString(TEXT("TRUE")) : FString(TEXT("false"));
    const auto ValueCol = EffectiveValue
        ? FCkGoapDebuggerStyle::Color_Status_PlanFound
        : FCkGoapDebuggerStyle::Color_Text_Dim;

    // ---- TAttribute lambdas — live updates without rail rebuild -------------
    // OVERRIDE pill visibility: appears when Has_KeyOverride is true for this
    // key. Driven by the live override stack so push/pop reflects immediately.
    auto OverrideVisibilityAttr = TAttribute<EVisibility>::Create(
        TAttribute<EVisibility>::FGetter::CreateLambda(
            [WeakThis = TWeakPtr<SCkGoapDebugger_WorldStateRail>(SharedThis(this)),
             EntryKey]() -> EVisibility
            {
                const auto Pinned = WeakThis.Pin();
                if (NOT Pinned.IsValid()) { return EVisibility::Collapsed; }

                auto WsHandle = Pinned->_CurrentWorldState;
                if (NOT ck::IsValid(WsHandle)) { return EVisibility::Collapsed; }

                return UCk_Utils_Goap_WorldState_UE::Has_KeyOverride(WsHandle, EntryKey)
                    ? EVisibility::Visible
                    : EVisibility::Collapsed;
            }));

    auto RowContent = SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Small, 0.0f)
            [
                SNew(STextBlock)
                    .Visibility(InEntry.RecentlyChanged ? EVisibility::Visible : EVisibility::Collapsed)
                    .Text(FText::FromString(TEXT("★")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                    .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Status_Selected))
            ]
        + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(KeyText))
                    .ToolTipText(FText::FromString(InEntry.Key.IsValid() ? InEntry.Key.ToString() : FString{}))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                    .ColorAndOpacity(FSlateColor(KeyCol))
            ]
        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SCkDebug_SelectableLabel)
                    .Text(FText::FromString(ValueStr))
                    .Font(EffectiveValue
                        ? FCoreStyle::GetDefaultFontStyle("Bold", 9)
                        : FCoreStyle::GetDefaultFontStyle("Regular", 9))
                    .ColorAndOpacity(FSlateColor(ValueCol))
            ]
        // OVERRIDE pill — TAttribute-driven visibility. Amber (#f59e0b ~ Color_Status_Selected).
        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Small, 0.0f, 0.0f, 0.0f))
            [
                SNew(SBorder)
                    .Visibility(OverrideVisibilityAttr)
                    .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.WS.RowRecent")))
                    .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Small, 0.0f))
                    // Live-bound tooltip naming the specific shadowing layer
                    // instead of the previous generic "a debug override"
                    // wording — when the gameplay-side picks up override
                    // layers ("FlierAttract", "KioskBrainwash", etc.) the
                    // tooltip names them so behaviour is explicable.
                    .ToolTipText(TAttribute<FText>::Create(
                        TAttribute<FText>::FGetter::CreateLambda(
                            [WeakThis = TWeakPtr<SCkGoapDebugger_WorldStateRail>(SharedThis(this)),
                             EntryKey]() -> FText
                            {
                                const auto Pinned = WeakThis.Pin();
                                if (NOT Pinned.IsValid()) { return FText::FromString(TEXT("Shadowed by an override layer.")); }
                                auto WsHandle = Pinned->_CurrentWorldState;
                                if (NOT ck::IsValid(WsHandle))
                                { return FText::FromString(TEXT("Shadowed by an override layer.")); }
                                const auto LayerName = UCk_Utils_Goap_WorldState_UE::Get_TopOverrideLayerForKey(WsHandle, EntryKey);
                                if (LayerName.IsNone())
                                { return FText::FromString(TEXT("Shadowed by an override layer.")); }
                                return FText::FromString(FString::Printf(
                                    TEXT("Shadowed by layer: %s\n\nTop-of-stack wins. Pop this layer in the Override Layers section above to reveal the base or next-lower-layer value."),
                                    *LayerName.ToString()));
                            })))
                    [
                        SNew(SCkDebug_SelectableLabel)
                            .Text(FText::FromString(TEXT("OVERRIDE")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 7))
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Status_Selected))
                    ]
            ];

    // The row background. Pick the "recently changed" amber brush when either
    // RecentlyChanged OR an override is active on this key. The background
    // tint matches the spec — a single brush for both "debug-shadowed" and
    // "recently changed" reads naturally as "this row has something to look at".
    auto BorderBrushAttr = TAttribute<const FSlateBrush*>::Create(
        TAttribute<const FSlateBrush*>::FGetter::CreateLambda(
            [WeakThis = TWeakPtr<SCkGoapDebugger_WorldStateRail>(SharedThis(this)),
             EntryKey,
             RecentlyChanged = InEntry.RecentlyChanged]() -> const FSlateBrush*
            {
                if (RecentlyChanged)
                {
                    return FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.WS.RowRecent"));
                }
                const auto Pinned = WeakThis.Pin();
                if (Pinned.IsValid())
                {
                    auto WsHandle = Pinned->_CurrentWorldState;
                    if (ck::IsValid(WsHandle) &&
                        UCk_Utils_Goap_WorldState_UE::Has_KeyOverride(WsHandle, EntryKey))
                    {
                        return FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.WS.RowRecent"));
                    }
                }
                return FStyleDefaults::GetNoBrush();
            }));

    // The clickable row — SButton wraps the content so left-click invokes the
    // toggle. Style-wise we use a transparent button to preserve the rail's
    // visual cadence; the row's amber tint is what signals override state.
    return SNew(SBorder)
        .BorderImage(BorderBrushAttr)
        .Padding(FMargin(0.0f))
        [
            SNew(SButton)
                .ButtonStyle(FCoreStyle::Get(), "NoBorder")
                .ContentPadding(FMargin(FCkGoapDebuggerStyle::Padding_Small, 1.0f))
                .ToolTipText(FText::FromString(TEXT("Click to push a DebugUI override that flips this key.\nClick again to flip back.")))
                .OnClicked(FOnClicked::CreateSP(
                    this,
                    &SCkGoapDebugger_WorldStateRail::HandleClick_ToggleKeyOverride,
                    EntryKey,
                    EffectiveValue))
                [
                    RowContent
                ]
        ];
}

// ====================================================================================================================
// CLICK HANDLERS — DebugUI override layer
// ====================================================================================================================

auto
    SCkGoapDebugger_WorldStateRail::
    HandleClick_ToggleKeyOverride(
        FGameplayTag InKey,
        bool InCurrentEffectiveValue)
    -> FReply
{
    using namespace ck_goap_debugger_wsrail_internal;

    if (NOT ck::IsValid(_CurrentWorldState)) { return FReply::Handled(); }
    if (NOT InKey.IsValid())                 { return FReply::Handled(); }

    // Push or update the DebugUI layer with the flipped value. The API is
    // idempotent and creates the layer on first call; subsequent clicks on
    // the same key update the override in place. Click again to flip back.
    auto MutableWs = _CurrentWorldState;
    UCk_Utils_Goap_WorldState_UE::Push_Override_SingleKey(
        MutableWs,
        WsRail_DebugUiLayerName,
        InKey,
        NOT InCurrentEffectiveValue);

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
    BuildOverrideLayersSection(const FCk_Handle_Goap_WorldState& InWs)
    -> TSharedRef<SWidget>
{
    const auto LayerNames = UCk_Utils_Goap_WorldState_UE::Get_OverrideLayerNames(InWs);

    auto Section = SNew(SVerticalBox);

    // Section header
    Section->AddSlot()
        .AutoHeight()
        .Padding(FMargin(0.0f, 0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Small))
        [
            SNew(SCkDebug_SelectableLabel)
                .Text(FText::FromString(FString::Printf(
                    TEXT("OVERRIDE LAYERS · %d"), LayerNames.Num())))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted))
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

    return SNew(SBorder)
        .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.WS.RowRecent")))
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
                    .ContentPadding(FMargin(FCkGoapDebuggerStyle::Padding_Small, 1.0f))
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
                                        .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted))
                                ]
                            + SHorizontalBox::Slot()
                                .AutoWidth()
                                .VAlign(VAlign_Center)
                                [
                                    SNew(SCkDebug_SelectableLabel)
                                        .Text(FText::FromString(InLayerName.ToString()))
                                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                                        .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Status_Selected))
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
                                        .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted))
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
                    .ContentPadding(FMargin(FCkGoapDebuggerStyle::Padding_Small, 1.0f))
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(TEXT("Pop")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Status_Failed))
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
                ? FCkGoapDebuggerStyle::Color_Status_PlanFound
                : FCkGoapDebuggerStyle::Color_Text_Dim;

            KeysBox->AddSlot()
                .AutoHeight()
                .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium, 1.0f, 0.0f, 1.0f))
                [
                    SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                            .FillWidth(1.0f)
                            .VAlign(VAlign_Center)
                            [
                                SNew(SCkDebug_SelectableLabel)
                                    .Text(FText::FromString(Kv.Key.ToString()))
                                    .Font(FCoreStyle::GetDefaultFontStyle("Mono", 8))
                                    .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Secondary))
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
