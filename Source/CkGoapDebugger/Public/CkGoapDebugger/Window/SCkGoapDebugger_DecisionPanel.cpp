#include "CkGoapDebugger/Window/SCkGoapDebugger_DecisionPanel.h"

#include "CkGoapDebugger/CkGoapDebuggerStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NameLabel.h"
#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"

#include "CkCore/Algorithms/CkAlgorithms.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_Chip.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_MeterBar.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Stepper.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "CkGoap/EntityScripts/CkGoapAction_EntityScript.h"
#include "CkGoap/Planner/CkGoap_Planner_Utils.h"

#include "Styling/CoreStyle.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================
// Internal helpers — PlannerInfo → DecisionModel adapters
// ====================================================================================================================

namespace ck_goap_debugger_decision_panel
{
    using namespace ck_goap_debugger_decision_model;

    auto LeafOfTag(const FGameplayTag& InTag) -> FString
    {
        auto Full = InTag.ToString();
        if (auto Idx = int32{INDEX_NONE}; Full.FindLastChar(TEXT('.'), Idx))
        { return Full.RightChop(Idx + 1); }
        return Full;
    }

    auto MakeDefs(const TArray<FCkGoapDebugger_ActionInfo>& InActions) -> TArray<FCkGoapDebugger_ActionDefLite>
    {
        auto Defs = TArray<FCkGoapDebugger_ActionDefLite>{};
        Defs.Reserve(InActions.Num());
        for (const auto& Action : InActions)
        {
            auto Def = FCkGoapDebugger_ActionDefLite{};
            Def.ClassName = Action.ClassName;
            Def.Cost = Action.Cost;
            Def.IsComposite = Action.IsPlannerRole;
            for (const auto& Pre : Action.Preconditions) { Def.Preconditions.Add(Pre.Key.GetTagName(), Pre.Value); }
            for (const auto& Eff : Action.Effects)       { Def.Effects.Add(Eff.Key.GetTagName(), Eff.Value); }
            Defs.Add(MoveTemp(Def));
        }
        return Defs;
    }

    auto MakeWsMap(const TArray<FCkGoapDebugger_WorldStateEntry>& InEntries) -> TMap<FName, bool>
    {
        auto Map = TMap<FName, bool>{};
        Map.Reserve(InEntries.Num());
        for (const auto& Entry : InEntries) { Map.Add(Entry.Key.GetTagName(), Entry.Value); }
        return Map;
    }

    auto MakeGoalMap(const TArray<FCkGoapDebugger_Condition>& InGoal) -> TMap<FName, bool>
    {
        auto Map = TMap<FName, bool>{};
        Map.Reserve(InGoal.Num());
        for (const auto& Cond : InGoal) { Map.Add(Cond.Key.GetTagName(), Cond.Value); }
        return Map;
    }

    // Mirror the ENGINE's chosen plan into a PlanLite (never re-plan for the
    // chosen path — the engine's result is the ground truth being explained).
    auto MakeChosenPlan(const FCkGoapDebugger_PlannerInfo& InPlanner, const TArray<FCkGoapDebugger_ActionDefLite>& InDefs) -> FCkGoapDebugger_PlanLite
    {
        auto Plan = FCkGoapDebugger_PlanLite{};
        Plan.Found = InPlanner.PlanStatus == ECk_GoapPlanStatus::PlanFound;
        Plan.TotalCost = InPlanner.PlanCost;
        for (const auto& StepName : InPlanner.PlanClassNames)
        {
            const auto DefIndex = InDefs.IndexOfByPredicate(
                [&StepName](const FCkGoapDebugger_ActionDefLite& In) { return In.ClassName == StepName; });
            if (DefIndex != INDEX_NONE) { Plan.StepDefIndices.Add(DefIndex); }
        }
        return Plan;
    }

    auto StripeColorFor(ECkGoapDebugger_CandidateState InState) -> FLinearColor
    {
        switch (InState)
        {
            case ECkGoapDebugger_CandidateState::InPlan:   return CkStyle::Accent();
            case ECkGoapDebugger_CandidateState::Blocked:  return CkStyle::Err();
            case ECkGoapDebugger_CandidateState::Fallback: return CkStyle::Warn();
            default:                                       return FLinearColor::Transparent;
        }
    }

    // Cost meter scale — mockup clamps at 8 "for scale"; fallbacks peg full.
    constexpr auto CostMeterCeiling = 8.0f;
}

// ====================================================================================================================
// CONSTRUCT / LIFECYCLE
// ====================================================================================================================

auto
    SCkGoapDebugger_DecisionPanel::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _ViewModel = InArgs._ViewModel;

    ChildSlot
    [
        SNew(SBorder)
            .BorderImage(CkStyle::GetFilledBrush())
            .BorderBackgroundColor(FSlateColor(CkStyle::Bg1()))
            .Padding(FMargin(CkStyle::SpaceL))
            [
                SNew(SScrollBox)
                    .Orientation(Orient_Vertical)

                    + SScrollBox::Slot()
                    [
                        SAssignNew(_Body, SVerticalBox)
                    ]
            ]
    ];

    RefreshFromViewModel();
}

auto
    SCkGoapDebugger_DecisionPanel::
    Reset_ForWorldChange()
    -> void
{
    // _EditTargets holds Planner handles by value — release them while the
    // registry is alive. Edited-cost history dies with the world too.
    _EditTargets.Reset();
    _OriginalCosts.Reset();
    _LastHash = 0;

    if (_Body.IsValid())
    { _Body->ClearChildren(); }
}

// ====================================================================================================================
// REFRESH
// ====================================================================================================================

auto
    SCkGoapDebugger_DecisionPanel::
    RefreshFromViewModel()
    -> void
{
    if (NOT _ViewModel.IsValid() || NOT _Body.IsValid()) { return; }

    const auto* Planner = _ViewModel->GetSelectedPlannerInfo();

    // ---- Content hash — skip identical rebuilds --------------------------------
    auto NewHash = uint32{0};
    if (Planner != nullptr)
    {
        NewHash = HashCombine(NewHash, GetTypeHash(Planner->PlannerHandle));
        NewHash = HashCombine(NewHash, ::GetTypeHash(static_cast<uint8>(Planner->PlanStatus)));
        NewHash = HashCombine(NewHash, ::GetTypeHash(Planner->PlanCost));
        NewHash = HashCombine(NewHash, ::GetTypeHash(Planner->PlanAttemptCount));

        const auto FoldTier = [&NewHash](const FCkGoapDebugger_PlannerInfo& InTier)
        {
            for (const auto& StepName : InTier.PlanClassNames)
            { NewHash = HashCombine(NewHash, GetTypeHash(StepName)); }
            for (const auto& Action : InTier.ChildActions)
            {
                NewHash = HashCombine(NewHash, GetTypeHash(Action.ClassName));
                NewHash = HashCombine(NewHash, ::GetTypeHash(Action.Cost));
            }
            for (const auto& Entry : InTier.WorldState)
            {
                auto Pair = GetTypeHash(Entry.Key);
                Pair = HashCombine(Pair, ::GetTypeHash(Entry.Value ? 1 : 0));
                NewHash ^= Pair;
            }
        };

        FoldTier(*Planner);
        for (const auto& Child : Planner->ChildPlanners)
        {
            NewHash = HashCombine(NewHash, GetTypeHash(Child.PlannerHandle));
            FoldTier(Child);
        }
    }
    for (const auto& Kvp : _OriginalCosts)
    {
        NewHash = HashCombine(NewHash, GetTypeHash(Kvp.Key));
        NewHash = HashCombine(NewHash, ::GetTypeHash(Kvp.Value));
    }

    // Card titles run through the shared name-depth tuner — cycling it must
    // re-render the cards.
    NewHash = HashCombine(NewHash, ::GetTypeHash(_ViewModel->Get_NameDepth()));

    if (NewHash == _LastHash) { return; }
    _LastHash = NewHash;

    _Body->ClearChildren();

    if (Planner == nullptr)
    {
        _Body->AddSlot()
            .AutoHeight()
            .Padding(FMargin(0.0f, CkStyle::SpaceXL))
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Select a Planner to see its decision breakdown.")))
                    .Font(CkStyle::RegularFont(CkStyle::FontSizeBody()))
                    .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                    .Justification(ETextJustify::Center)
            ];
        return;
    }

    DoRebuild(*Planner);
}

auto
    SCkGoapDebugger_DecisionPanel::
    DoRebuild(
        const FCkGoapDebugger_PlannerInfo& InPlanner)
    -> void
{
    // Top tier — the selected Planner's own catalog.
    _Body->AddSlot()
        .AutoHeight()
        [
            DoBuildGroup(InPlanner, FString::Printf(
                TEXT("%s — every candidate, scored"), *InPlanner.DisplayName))
        ];

    // Sub tiers — one group per in-plan composite, walking the chain; a
    // dormant note for composites that exist but aren't in the current plan.
    const auto* Cursor = &InPlanner;
    while (Cursor != nullptr)
    {
        const FCkGoapDebugger_PlannerInfo* NextInPlan = nullptr;

        for (const auto& Child : Cursor->ChildPlanners)
        {
            const auto IsInPlan = Cursor->PlanHandles.ContainsByPredicate(
                [&Child](const FCk_Handle_Goap_Action& InStep)
                { return static_cast<FCk_Handle>(InStep) == static_cast<FCk_Handle>(Child.PlannerHandle); });

            if (IsInPlan)
            {
                _Body->AddSlot()
                    .AutoHeight()
                    [
                        DoBuildGroup(Child, FString::Printf(
                            TEXT("» inside %s — which concrete step?"), *Child.DisplayName))
                    ];
                NextInPlan = &Child;
            }
            else
            {
                _Body->AddSlot()
                    .AutoHeight()
                    [
                        DoBuildDormantCard(Child.DisplayName)
                    ];
            }
        }

        Cursor = NextInPlan;
    }

    // Reset-edited-costs — right-aligned, only when edits exist.
    if (_OriginalCosts.Num() > 0)
    {
        _Body->AddSlot()
            .AutoHeight()
            .HAlign(HAlign_Right)
            .Padding(FMargin(0.0f, CkStyle::SpaceS, 0.0f, 0.0f))
            [
                SNew(SButton)
                    .ToolTipText(FText::FromString(TEXT("Re-issue Request_SetChildActionCost with each action's pre-edit cost.")))
                    .OnClicked(this, &SCkGoapDebugger_DecisionPanel::HandleResetEditedCosts)
                    .ContentPadding(FMargin(CkStyle::SpaceM, 2.0f))
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(TEXT("reset edited costs")))
                            .Font(CkStyle::BoldFont(CkStyle::FontSizeMicro()))
                            .ColorAndOpacity(FSlateColor(CkStyle::Accent()))
                    ]
            ];
    }
}

// ====================================================================================================================
// BUILD — GROUP
// ====================================================================================================================

auto
    SCkGoapDebugger_DecisionPanel::
    DoBuildGroup(
        const FCkGoapDebugger_PlannerInfo& InTier,
        const FString& InGroupLabel)
    -> TSharedRef<SWidget>
{
    using namespace ck_goap_debugger_decision_panel;

    const auto Defs    = MakeDefs(InTier.ChildActions);
    const auto WsMap   = MakeWsMap(InTier.WorldState);
    const auto GoalMap = MakeGoalMap(InTier.GoalResolved);
    const auto Chosen  = MakeChosenPlan(InTier, Defs);
    const auto Scores  = ScoreCandidates(WsMap, Defs, GoalMap, Chosen);

    // Cross-tier coaching notes — top-tier candidates whose effects would
    // change an in-plan composite's sub-plan cost.
    auto SubDefsByComposite = TMap<int32, TArray<FCkGoapDebugger_ActionDefLite>>{};
    for (const auto& Child : InTier.ChildPlanners)
    {
        const auto CompositeDefIndex = Defs.IndexOfByPredicate(
            [&Child](const FCkGoapDebugger_ActionDefLite& In)
            { return In.IsComposite && In.ClassName == Child.DisplayName; });

        // Composite defs come from ChildActions (class names); ChildPlanners
        // carry planner display names — fall back to matching by handle via
        // the ChildActions list when the name shapes differ.
        auto ResolvedIndex = CompositeDefIndex;
        if (ResolvedIndex == INDEX_NONE)
        {
            const auto* AsAction = InTier.ChildActions.FindByPredicate(
                [&Child](const FCkGoapDebugger_ActionInfo& In)
                { return static_cast<FCk_Handle>(In.Handle) == static_cast<FCk_Handle>(Child.PlannerHandle); });
            if (AsAction != nullptr)
            {
                ResolvedIndex = Defs.IndexOfByPredicate(
                    [AsAction](const FCkGoapDebugger_ActionDefLite& In)
                    { return In.ClassName == AsAction->ClassName; });
            }
        }

        if (ResolvedIndex != INDEX_NONE)
        { SubDefsByComposite.Add(ResolvedIndex, MakeDefs(Child.ChildActions)); }
    }
    const auto Notes = ComputeCrossTierNotes(WsMap, Defs, SubDefsByComposite, GoalMap);

    // ---- Order: in-plan (by step) → viable → blocked → fallback ---------------
    auto InPlan = TArray<const FCkGoapDebugger_CandidateScore*>{};
    auto Viable = TArray<const FCkGoapDebugger_CandidateScore*>{};
    auto Blocked = TArray<const FCkGoapDebugger_CandidateScore*>{};
    auto Fallback = TArray<const FCkGoapDebugger_CandidateScore*>{};

    for (const auto& Score : Scores)
    {
        switch (Score.State)
        {
            case ECkGoapDebugger_CandidateState::InPlan:          InPlan.Add(&Score); break;
            case ECkGoapDebugger_CandidateState::ViableNotChosen: Viable.Add(&Score); break;
            case ECkGoapDebugger_CandidateState::Fallback:        Fallback.Add(&Score); break;
            default:                                              Blocked.Add(&Score); break;
        }
    }
    InPlan.Sort([](const FCkGoapDebugger_CandidateScore& A, const FCkGoapDebugger_CandidateScore& B)
    { return A.PlanStepIndex < B.PlanStepIndex; });

    auto Group = SNew(SVerticalBox);

    Group->AddSlot()
        .AutoHeight()
        .Padding(FMargin(0.0f, 0.0f, 0.0f, CkStyle::SpaceM))
        [
            SNew(SCkDebug_SelectableLabel)
                .Text(FText::FromString(InGroupLabel.ToUpper()))
                .Font(CkStyle::BoldFont(CkStyle::FontSizeMicro()))
                .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
        ];

    const auto AddCards = [&](const TArray<const FCkGoapDebugger_CandidateScore*>& InScores)
    {
        for (const auto* Score : InScores)
        {
            if (NOT InTier.ChildActions.IsValidIndex(Score->DefIndex)) { continue; }

            // Cross-tier note for this candidate, rendered as coaching text.
            auto NoteText = FString{};
            if (const auto* Note = Notes.FindByPredicate(
                [Score](const FCkGoapDebugger_CrossTierNote& In) { return In.CandidateDefIndex == Score->DefIndex; }))
            {
                const auto CompositeName = InTier.ChildActions.IsValidIndex(Note->CompositeDefIndex)
                    ? SCkDebug_NameLabel::Get_ShortName(
                          InTier.ChildActions[Note->CompositeDefIndex].ClassName,
                          _ViewModel.IsValid() ? _ViewModel->Get_NameDepth() : 1)
                    : FString(TEXT("the composite"));
                NoteText = FString::Printf(
                    TEXT("But: applying its effect would flip %s's expansion from %.1f to %.1f — invisible to this tier (the composite's authored cost is all this tier compares)."),
                    *CompositeName, Note->SubCostNow, Note->SubCostIfApplied);
            }
            else if (Score->State == ECkGoapDebugger_CandidateState::ViableNotChosen && Score->MakesNoProgress)
            {
                const auto& Action = InTier.ChildActions[Score->DefIndex];
                const auto AllEffectsAlreadyTrue = ck::algo::AllOf(Action.Effects,
                    [&WsMap](const FCkGoapDebugger_Condition& InEff)
                    {
                        const auto* Found = WsMap.Find(InEff.Key.GetTagName());
                        return Found != nullptr && *Found == InEff.Value;
                    });
                NoteText = AllEffectsAlreadyTrue
                    ? TEXT("Its effect is already true — it makes no progress toward the goal from here.")
                    : TEXT("Its effect doesn't shorten the path to the goal at this tier.");
            }

            Group->AddSlot()
                .AutoHeight()
                .Padding(FMargin(0.0f, 0.0f, 0.0f, CkStyle::SpaceS))
                [
                    DoBuildCandidateCard(InTier, InTier.ChildActions[Score->DefIndex], *Score, NoteText)
                ];
        }
    };

    AddCards(InPlan);
    AddCards(Viable);
    AddCards(Blocked);
    AddCards(Fallback);

    if (Scores.Num() == 0)
    {
        Group->AddSlot()
            .AutoHeight()
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("(no registered candidate actions)")))
                    .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
                    .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
            ];
    }

    return SNew(SBox)
        .Padding(FMargin(0.0f, 0.0f, 0.0f, CkStyle::SpaceL))
        [
            Group
        ];
}

// ====================================================================================================================
// BUILD — CANDIDATE CARD
// ====================================================================================================================

auto
    SCkGoapDebugger_DecisionPanel::
    DoBuildCandidateCard(
        const FCkGoapDebugger_PlannerInfo& InTier,
        const FCkGoapDebugger_ActionInfo& InAction,
        const FCkGoapDebugger_CandidateScore& InScore,
        const FString& InNoteText)
    -> TSharedRef<SWidget>
{
    using namespace ck_goap_debugger_decision_panel;

    const auto WeakPanel = TWeakPtr<SCkGoapDebugger_DecisionPanel>(SharedThis(this));

    // ---- Why-line --------------------------------------------------------------
    auto WhyText = FString{};
    switch (InScore.State)
    {
        case ECkGoapDebugger_CandidateState::InPlan:
            WhyText = FString::Printf(TEXT("in the plan — step %d"), InScore.PlanStepIndex + 1);
            break;
        case ECkGoapDebugger_CandidateState::ViableNotChosen:
            WhyText = FString::Printf(TEXT("viable, not chosen — forcing it first would cost %.1f (+%.1f)"),
                InScore.ForcedFirstTotal, InScore.DeltaVsPlan);
            break;
        case ECkGoapDebugger_CandidateState::Fallback:
            WhyText = FString::Printf(TEXT("fallback — %.1f only wins when nothing else can"), InAction.Cost);
            break;
        default:
        {
            auto Needs = FString{};
            for (auto Idx = 0; Idx < InScore.UnmetPreconditions.Num(); ++Idx)
            {
                if (Idx > 0) { Needs += TEXT(", "); }
                Needs += InScore.UnmetPreconditions[Idx].Key.ToString();
                if (NOT InScore.UnmetPreconditions[Idx].Value) { Needs += TEXT(" = false"); }
            }
            WhyText = FString::Printf(TEXT("blocked — needs %s"), *Needs);
            break;
        }
    }

    const auto StripeColor = StripeColorFor(InScore.State);
    const auto IsEdited = _OriginalCosts.Contains(InAction.ClassName);

    // ---- Header row ------------------------------------------------------------
    auto HeaderRow = SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(0.0f, 0.0f, CkStyle::SpaceS, 0.0f))
            [
                SNew(SCkDebug_NameLabel)
                    .FullName(InAction.ClassName)
                    .Prefix(FString::Printf(TEXT("%s "),
                        InAction.IsPlannerRole ? TEXT("◆●") : TEXT("●")))
                    .NameDepth_Lambda([WeakPanel]() -> int32
                    {
                        const auto Pinned = WeakPanel.Pin();
                        return Pinned.IsValid() && Pinned->_ViewModel.IsValid()
                            ? Pinned->_ViewModel->Get_NameDepth()
                            : 1;
                    })
                    .Font(CkStyle::BoldFont(CkStyle::FontSizeSmall()))
                    .ColorAndOpacity(FSlateColor(CkStyle::Text()))
            ]

        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(FString::Printf(TEXT("%.1f"), InAction.Cost)))
                    .Font(CkStyle::MonoFont(CkStyle::FontSizeSmall()))
                    .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
            ]

        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(CkStyle::SpaceXS, 0.0f, 0.0f, 0.0f))
            [
                SNew(SCkDebug_Stepper)
                    .ToolTipText(FText::FromString(TEXT("Request_SetChildActionCost — live; replans per the tier's policy")))
                    .OnDelta(FOnCkDebug_StepperDelta::CreateSP(
                        this,
                        &SCkGoapDebugger_DecisionPanel::HandleCostDelta,
                        InTier.PlannerHandle,
                        InAction.ActionClass,
                        InAction.ClassName,
                        InAction.Cost))
            ];

    if (IsEdited)
    {
        HeaderRow->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f))
            [
                SNew(SBorder)
                    .BorderImage(CkStyle::GetFilledBrush())
                    .BorderBackgroundColor(FSlateColor(CkStyle::AccentDim()))
                    .Padding(FMargin(CkStyle::SpaceS, 0.0f))
                    .ToolTipText(FText::FromString(FString::Printf(
                        TEXT("Cost edited from the debugger (was %.1f)."), _OriginalCosts[InAction.ClassName])))
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(TEXT("edited")))
                            .Font(CkStyle::BoldFont(CkStyle::FontSizeMicro()))
                            .ColorAndOpacity(FSlateColor(CkStyle::Accent()))
                    ]
            ];
    }

    if (InAction.IsPlannerRole)
    {
        HeaderRow->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f))
            [
                SNew(SBorder)
                    .BorderImage(CkStyle::GetFilledBrush())
                    .BorderBackgroundColor(FSlateColor(CkStyle::Bg3()))
                    .Padding(FMargin(CkStyle::SpaceS, 0.0f))
                    .ToolTipText(FText::FromString(TEXT("Dual-role: this Action is also a Planner with its own sub-catalog.")))
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(TEXT("sub-planner")))
                            .Font(CkStyle::BoldFont(CkStyle::FontSizeMicro()))
                            .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                    ]
            ];
    }

    if (InScore.State == ECkGoapDebugger_CandidateState::Fallback)
    {
        HeaderRow->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f))
            [
                SNew(SBorder)
                    .BorderImage(CkStyle::GetFilledBrush())
                    .BorderBackgroundColor(FSlateColor(CkStyle::WarnDim()))
                    .Padding(FMargin(CkStyle::SpaceS, 0.0f))
                    .ToolTipText(FText::FromString(TEXT("Always-valid-plan tenet: no preconditions + very high cost — only wins when nothing else can.")))
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(TEXT("fallback")))
                            .Font(CkStyle::BoldFont(CkStyle::FontSizeMicro()))
                            .ColorAndOpacity(FSlateColor(CkStyle::Warn()))
                    ]
            ];
    }

    HeaderRow->AddSlot()
        .FillWidth(1.0f)
        .VAlign(VAlign_Center)
        .Padding(FMargin(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f))
        [
            SNew(STextBlock)
                .Text(FText::FromString(WhyText))
                .Font(CkStyle::RegularFont(CkStyle::FontSizeMicro()))
                .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                .Justification(ETextJustify::Right)
        ];

    // ---- Condition chips (live trace highlight) --------------------------------
    auto Chips = SNew(SWrapBox)
        .UseAllottedSize(true)
        .InnerSlotPadding(FVector2D(CkStyle::SpaceXS, CkStyle::SpaceXS));

    const auto MakeTraceAttr = [WeakPanel](FGameplayTag InKey) -> TAttribute<bool>
    {
        return TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda(
            [WeakPanel, InKey]() -> bool
            {
                const auto Pinned = WeakPanel.Pin();
                if (NOT Pinned.IsValid() || NOT Pinned->_ViewModel.IsValid()) { return false; }
                return Pinned->_ViewModel->Get_TracedWsKey() == InKey;
            }));
    };

    // Click a chip → trace its key everywhere (same toggle as a WS-rail row
    // click). Chips live in plain panels here, so consuming the click is safe.
    const auto MakeTraceClick = [WeakPanel](FGameplayTag InKey) -> FOnCkDebug_ChipClicked
    {
        return FOnCkDebug_ChipClicked::CreateLambda([WeakPanel, InKey]()
        {
            const auto Pinned = WeakPanel.Pin();
            if (NOT Pinned.IsValid() || NOT Pinned->_ViewModel.IsValid()) { return; }
            auto& Vm = *Pinned->_ViewModel;
            Vm.Set_TracedWsKey(Vm.Get_TracedWsKey() == InKey ? FGameplayTag{} : InKey);
        });
    };

    const auto WsMap = MakeWsMap(InTier.WorldState);

    if (InAction.Preconditions.Num() == 0)
    {
        Chips->AddSlot()
        [
            SNew(SCkDebug_Chip)
                .Text(FText::FromString(TEXT("no preconditions")))
                .Kind(ECkDebug_ChipKind::Neutral)
        ];
    }

    for (const auto& Pre : InAction.Preconditions)
    {
        const auto* Found = WsMap.Find(Pre.Key.GetTagName());
        const auto Satisfied = Found != nullptr && *Found == Pre.Value;

        auto Label = LeafOfTag(Pre.Key);
        if (NOT Pre.Value) { Label += TEXT(" = false"); }

        Chips->AddSlot()
        [
            SNew(SCkDebug_Chip)
                .Text(FText::FromString(Label))
                .ToolTipText(FText::FromString(FString::Printf(TEXT("%s — %s. Click to trace this key."),
                    *Pre.Key.ToString(), Satisfied ? TEXT("met") : TEXT("unmet"))))
                .Kind(Satisfied ? ECkDebug_ChipKind::Satisfied : ECkDebug_ChipKind::Unsatisfied)
                .Highlighted(MakeTraceAttr(Pre.Key))
                .OnClicked(MakeTraceClick(Pre.Key))
        ];
    }

    for (const auto& Eff : InAction.Effects)
    {
        // Chip Kind=Effect draws its own "→" glyph — pass the bare key.
        auto Label = LeafOfTag(Eff.Key);
        if (NOT Eff.Value) { Label += TEXT(" = false"); }

        Chips->AddSlot()
        [
            SNew(SCkDebug_Chip)
                .Text(FText::FromString(Label))
                .ToolTipText(FText::FromString(FString::Printf(TEXT("Effect: sets %s. Click to trace this key."), *Eff.Key.ToString())))
                .Kind(ECkDebug_ChipKind::Effect)
                .Highlighted(MakeTraceAttr(Eff.Key))
                .OnClicked(MakeTraceClick(Eff.Key))
        ];
    }

    // ---- Card body -------------------------------------------------------------
    auto CardBody = SNew(SVerticalBox)

        + SVerticalBox::Slot()
            .AutoHeight()
            [
                HeaderRow
            ]

        + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FMargin(0.0f, CkStyle::SpaceS, 0.0f, 0.0f))
            [
                Chips
            ];

    if (NOT InNoteText.IsEmpty())
    {
        CardBody->AddSlot()
            .AutoHeight()
            .Padding(FMargin(0.0f, CkStyle::SpaceS, 0.0f, 0.0f))
            [
                SNew(SCkDebug_SelectableLabel)
                    .Text(FText::FromString(InNoteText))
                    .Font(CkStyle::RegularFont(CkStyle::FontSizeMicro()))
                    .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
            ];
    }

    const auto MeterFraction = InScore.State == ECkGoapDebugger_CandidateState::Fallback
        ? 1.0f
        : FMath::Min(InAction.Cost, CostMeterCeiling) / CostMeterCeiling;
    const auto MeterColor = [&InScore]() -> FLinearColor
    {
        switch (InScore.State)
        {
            case ECkGoapDebugger_CandidateState::InPlan:   return CkStyle::Accent();
            case ECkGoapDebugger_CandidateState::Fallback: return CkStyle::Warn();
            default:                                       return CkStyle::TextMute();
        }
    }();

    CardBody->AddSlot()
        .AutoHeight()
        .Padding(FMargin(0.0f, CkStyle::SpaceS, 0.0f, 0.0f))
        [
            SNew(SCkDebug_MeterBar)
                .ToolTipText(FText::FromString(TEXT("cost, clamped at 8 for scale")))
                .Fraction(MeterFraction)
                .FillColor(MeterColor)
        ];

    // ---- Stripe + rounded card -------------------------------------------------
    return SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SBox)
                    .WidthOverride(3.0f)
                    [
                        SNew(SBorder)
                            .BorderImage(CkStyle::GetFilledBrush())
                            .BorderBackgroundColor(FSlateColor(StripeColor))
                    ]
            ]

        + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                SNew(SBorder)
                    .BorderImage(CkStyle::GetRoundedBrush())
                    .BorderBackgroundColor(FSlateColor(CkStyle::Bg2()))
                    .Padding(FMargin(CkStyle::SpaceM, CkStyle::SpaceS))
                    [
                        CardBody
                    ]
            ];
}

// ====================================================================================================================
// BUILD — DORMANT SUB-PLANNER CARD
// ====================================================================================================================

auto
    SCkGoapDebugger_DecisionPanel::
    DoBuildDormantCard(
        const FString& InCompositeName)
    -> TSharedRef<SWidget>
{
    return SNew(SBox)
        .Padding(FMargin(0.0f, 0.0f, 0.0f, CkStyle::SpaceL))
        [
            SNew(SVerticalBox)

                + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(FMargin(0.0f, 0.0f, 0.0f, CkStyle::SpaceM))
                    [
                        SNew(SCkDebug_SelectableLabel)
                            .Text(FText::FromString(FString::Printf(TEXT("» INSIDE %s"), *InCompositeName.ToUpper())))
                            .Font(CkStyle::BoldFont(CkStyle::FontSizeMicro()))
                            .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
                    ]

                + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(SBorder)
                            .BorderImage(CkStyle::GetRoundedBrush())
                            .BorderBackgroundColor(FSlateColor(CkStyle::Bg2()))
                            .Padding(FMargin(CkStyle::SpaceM, CkStyle::SpaceS))
                            [
                                SNew(STextBlock)
                                    .Text(FText::FromString(FString::Printf(
                                        TEXT("Sub-planner dormant — %s isn't in the current plan, so its goal hasn't been injected."),
                                        *InCompositeName)))
                                    .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
                                    .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                            ]
                    ]
        ];
}

// ====================================================================================================================
// HANDLERS — cost edits
// ====================================================================================================================

auto
    SCkGoapDebugger_DecisionPanel::
    HandleCostDelta(
        int32 InDelta,
        FCk_Handle_Goap_Planner InPlanner,
        TSubclassOf<UCk_GoapAction_EntityScript> InClass,
        FString InClassName,
        float InCurrentCost)
    -> void
{
    if (NOT ck::IsValid(InPlanner)) { return; }
    if (InClass == nullptr)         { return; }

    // ±0.5 per click, floored at 0.1 (a zero-cost action would always win).
    const auto NewCost = FMath::Max(0.1f, InCurrentCost + static_cast<float>(InDelta) * 0.5f);

    if (NOT _OriginalCosts.Contains(InClassName))
    {
        _OriginalCosts.Add(InClassName, InCurrentCost);
        _EditTargets.Add(InClassName, {InPlanner, InClass});
    }
    else if (FMath::IsNearlyEqual(_OriginalCosts[InClassName], NewCost, 0.01f))
    {
        // Back where it started — no longer "edited".
        _OriginalCosts.Remove(InClassName);
        _EditTargets.Remove(InClassName);
    }

    auto MutablePlanner = InPlanner;
    UCk_Utils_Goap_Planner_UE::Request_SetChildActionCost(MutablePlanner, InClass, NewCost, {});
}

auto
    SCkGoapDebugger_DecisionPanel::
    HandleResetEditedCosts()
    -> FReply
{
    for (const auto& Kvp : _OriginalCosts)
    {
        const auto* Target = _EditTargets.Find(Kvp.Key);
        if (Target == nullptr)                  { continue; }
        if (NOT ck::IsValid(Target->Key))       { continue; }
        if (Target->Value == nullptr)           { continue; }

        auto MutablePlanner = Target->Key;
        UCk_Utils_Goap_Planner_UE::Request_SetChildActionCost(MutablePlanner, Target->Value, Kvp.Value, {});
    }

    _OriginalCosts.Reset();
    _EditTargets.Reset();

    // Edited state is folded into the content hash — force the rebuild so the
    // badges and this button disappear immediately.
    _LastHash = 0;
    RefreshFromViewModel();

    return FReply::Handled();
}

// ====================================================================================================================
