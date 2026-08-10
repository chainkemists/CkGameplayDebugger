#include "CkGoapDebugger/Window/SCkGoapDebugger_CatalogPanel.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_NameLabel.h"
#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkGoapDebugger/CkGoapDebugger_Axes.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_Chip.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_MeterBar.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Stepper.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "CkGoap/Algorithm/CkGoap_WorldState.h"
#include "CkGoap/EntityScripts/CkGoapAction_EntityScript.h"
#include "CkGoap/Planner/CkGoap_Planner_Utils.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#include "Styling/CoreStyle.h"

// ====================================================================================================================

namespace ck_goap_debugger_catalog_panel
{
    using namespace ck_goap_debugger_decision_model;

    auto LeafOfTag(const FGameplayTag& InTag) -> FString
    {
        auto Full = InTag.ToString();
        if (auto Idx = int32{INDEX_NONE}; Full.FindLastChar(TEXT('.'), Idx))
        { return Full.RightChop(Idx + 1); }
        return Full;
    }

    auto MakeDefsForLint(const TArray<FCkGoapDebugger_ActionInfo>& InActions) -> TArray<FCkGoapDebugger_ActionDefLite>
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
}

// ====================================================================================================================
// CONSTRUCT / LIFECYCLE
// ====================================================================================================================

auto
    SCkGoapDebugger_CatalogPanel::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _ViewModel = InArgs._ViewModel;

    // Persistent splitter chrome — built once so the user's drag positions
    // survive the hash-gated content rebuilds. Every major pane (catalog,
    // health checks, matrix) is resizable; the catalog defaults to half the
    // window per the mockup.
    ChildSlot
    [
        SNew(SBorder)
            .BorderImage(CkStyle::GetFilledBrush())
            .BorderBackgroundColor(FSlateColor(CkStyle::Bg1()))
            .Padding(FMargin(CkStyle::SpaceL))
            [
                SAssignNew(_ViewSwitcher, SWidgetSwitcher)

                    // 0 — empty state
                    + SWidgetSwitcher::Slot()
                    [
                        SNew(SBox)
                            .Padding(FMargin(0.0f, CkStyle::SpaceXL))
                            .VAlign(VAlign_Top)
                            [
                                SNew(STextBlock)
                                    .Text(FText::FromString(TEXT("Select a Planner to audit its catalog.")))
                                    .Font(CkStyle::RegularFont(CkStyle::FontSizeBody()))
                                    .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                                    .Justification(ETextJustify::Center)
                            ]
                    ]

                    // 1 — resizable audit layout
                    + SWidgetSwitcher::Slot()
                    [
                        SNew(SSplitter)
                            .Orientation(Orient_Horizontal)
                            .PhysicalSplitterHandleSize(4.0f)

                            + SSplitter::Slot()
                                .Value(0.5f)
                                .MinSize(320.0f)
                                [
                                    SNew(SScrollBox)
                                        .Orientation(Orient_Vertical)
                                        + SScrollBox::Slot()
                                        [
                                            SAssignNew(_CatalogHost, SBox)
                                                .Padding(FMargin(0.0f, 0.0f, CkStyle::SpaceL, 0.0f))
                                        ]
                                ]

                            + SSplitter::Slot()
                                .Value(0.5f)
                                .MinSize(280.0f)
                                [
                                    SNew(SSplitter)
                                        .Orientation(Orient_Vertical)
                                        .PhysicalSplitterHandleSize(4.0f)

                                        + SSplitter::Slot()
                                            .Value(0.45f)
                                            .MinSize(120.0f)
                                            [
                                                SNew(SScrollBox)
                                                    .Orientation(Orient_Vertical)
                                                    + SScrollBox::Slot()
                                                    [
                                                        SAssignNew(_HealthHost, SBox)
                                                            .Padding(FMargin(CkStyle::SpaceL, 0.0f, 0.0f, 0.0f))
                                                    ]
                                            ]

                                        + SSplitter::Slot()
                                            .Value(0.55f)
                                            .MinSize(120.0f)
                                            [
                                                SNew(SScrollBox)
                                                    .Orientation(Orient_Vertical)
                                                    + SScrollBox::Slot()
                                                    [
                                                        // Wide matrices scroll sideways inside their pane.
                                                        SNew(SScrollBox)
                                                            .Orientation(Orient_Horizontal)
                                                            + SScrollBox::Slot()
                                                            [
                                                                SAssignNew(_MatrixHost, SBox)
                                                                    .Padding(FMargin(CkStyle::SpaceL, CkStyle::SpaceL, 0.0f, 0.0f))
                                                            ]
                                                    ]
                                            ]
                                ]
                    ]
            ]
    ];

    RefreshFromViewModel();
}

auto
    SCkGoapDebugger_CatalogPanel::
    Reset_ForWorldChange()
    -> void
{
    _LastHash = 0;
    if (_CatalogHost.IsValid()) { _CatalogHost->SetContent(SNullWidget::NullWidget); }
    if (_HealthHost.IsValid())  { _HealthHost->SetContent(SNullWidget::NullWidget); }
    if (_MatrixHost.IsValid())  { _MatrixHost->SetContent(SNullWidget::NullWidget); }
    if (_ViewSwitcher.IsValid()) { _ViewSwitcher->SetActiveWidgetIndex(0); }
}

// ====================================================================================================================
// REFRESH
// ====================================================================================================================

auto
    SCkGoapDebugger_CatalogPanel::
    RefreshFromViewModel()
    -> void
{
    if (NOT _ViewModel.IsValid() || NOT _ViewSwitcher.IsValid()) { return; }

    const auto* Planner = _ViewModel->GetSelectedPlannerInfo();

    auto NewHash = uint32{0};
    if (Planner != nullptr)
    {
        NewHash = HashCombine(NewHash, GetTypeHash(Planner->PlannerHandle));
        const auto FoldTier = [&NewHash](const FCkGoapDebugger_PlannerInfo& InTier)
        {
            for (const auto& Action : InTier.ChildActions)
            {
                NewHash = HashCombine(NewHash, GetTypeHash(Action.ClassName));
                NewHash = HashCombine(NewHash, ::GetTypeHash(Action.Cost));
                NewHash = HashCombine(NewHash, ::GetTypeHash(Action.Preconditions.Num()));
                NewHash = HashCombine(NewHash, ::GetTypeHash(Action.Effects.Num()));
            }
            for (const auto& StepName : InTier.PlanClassNames)
            { NewHash = HashCombine(NewHash, GetTypeHash(StepName)); }
        };
        FoldTier(*Planner);
        for (const auto& Child : Planner->ChildPlanners)
        {
            NewHash = HashCombine(NewHash, GetTypeHash(Child.PlannerHandle));
            FoldTier(Child);
        }
        NewHash = HashCombine(NewHash, ::GetTypeHash(Planner->KeyUsage.Num()));
        NewHash = HashCombine(NewHash, ::GetTypeHash(Planner->DependencyCyclesDisplay.Num()));
        NewHash = HashCombine(NewHash, ::GetTypeHash(Planner->InvalidGoalAuthored.Num()));
    }
    // Card titles + matrix headers run through the shared name-depth tuner.
    NewHash = HashCombine(NewHash, ::GetTypeHash(_ViewModel->Get_NameDepth()));

    if (NewHash == _LastHash) { return; }
    _LastHash = NewHash;

    if (Planner == nullptr)
    {
        _ViewSwitcher->SetActiveWidgetIndex(0);
        return;
    }

    _ViewSwitcher->SetActiveWidgetIndex(1);
    DoRebuild(*Planner);
}

auto
    SCkGoapDebugger_CatalogPanel::
    DoRebuild(
        const FCkGoapDebugger_PlannerInfo& InPlanner)
    -> void
{
    // Fill the persistent splitter hosts — the splitters themselves (and the
    // user's drag positions) survive this rebuild untouched.
    auto LeftColumn = SNew(SVerticalBox);

    LeftColumn->AddSlot()
        .AutoHeight()
        [
            DoBuildCardSection(InPlanner, FString::Printf(
                TEXT("Action catalog · %s — %d actions"),
                *InPlanner.DisplayName, InPlanner.ChildActions.Num()))
        ];

    for (const auto& Child : InPlanner.ChildPlanners)
    {
        LeftColumn->AddSlot()
            .AutoHeight()
            [
                DoBuildCardSection(Child, FString::Printf(
                    TEXT("» Sub-planner · %s — %d actions"),
                    *Child.DisplayName, Child.ChildActions.Num()))
            ];
    }

    if (_CatalogHost.IsValid()) { _CatalogHost->SetContent(LeftColumn); }
    if (_HealthHost.IsValid())  { _HealthHost->SetContent(DoBuildHealthChecks(InPlanner)); }
    if (_MatrixHost.IsValid())  { _MatrixHost->SetContent(DoBuildMatrix(InPlanner)); }
}

// ====================================================================================================================
// BUILD — ACTION CARD SECTIONS
// ====================================================================================================================

auto
    SCkGoapDebugger_CatalogPanel::
    DoBuildCardSection(
        const FCkGoapDebugger_PlannerInfo& InTier,
        const FString& InLabel)
    -> TSharedRef<SWidget>
{
    // Full-width card rows (mockup left panel) — titles get room, chips wrap.
    auto Cards = SNew(SVerticalBox);

    for (const auto& Action : InTier.ChildActions)
    {
        Cards->AddSlot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 0.0f, 0.0f, CkStyle::SpaceS))
            [
                DoBuildActionCard(InTier, Action)
            ];
    }

    return SNew(SVerticalBox)

        + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 0.0f, 0.0f, CkStyle::SpaceS))
            [
                SNew(SCkDebug_SelectableLabel)
                    .Text(FText::FromString(InLabel.ToUpper()))
                    .Font(CkStyle::BoldFont(CkStyle::FontSizeMicro()))
                    .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
            ]

        + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 0.0f, 0.0f, CkStyle::SpaceL))
            [
                Cards
            ];
}

auto
    SCkGoapDebugger_CatalogPanel::
    DoBuildActionCard(
        const FCkGoapDebugger_PlannerInfo& InTier,
        const FCkGoapDebugger_ActionInfo& InAction)
    -> TSharedRef<SWidget>
{
    using namespace ck_goap_debugger_catalog_panel;

    const auto IsFallback = InAction.Cost >= k_FallbackCostFloor && InAction.Preconditions.Num() == 0;
    const auto NameDepth  = _ViewModel.IsValid() ? _ViewModel->Get_NameDepth() : 1;

    auto HeaderRow = SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(SCkDebug_NameLabel)
                    .FullName(InAction.ClassName)
                    .Prefix(FString::Printf(TEXT("%s "),
                        InAction.IsPlannerRole ? TEXT("◆●") : TEXT("●")))
                    .NameDepth(NameDepth)
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
                        &SCkGoapDebugger_CatalogPanel::HandleCostDelta,
                        InTier.PlannerHandle,
                        InAction.ActionClass,
                        InAction.Cost))
            ];

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
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(TEXT("sub-planner")))
                            .Font(CkStyle::BoldFont(CkStyle::FontSizeMicro()))
                            .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                    ]
            ];
    }

    if (IsFallback)
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
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(TEXT("fallback")))
                            .Font(CkStyle::BoldFont(CkStyle::FontSizeMicro()))
                            .ColorAndOpacity(FSlateColor(CkStyle::Warn()))
                    ]
            ];
    }

    // ⋯ — runtime catalog mutation affordance (AddAction / Request_RemoveAction).
    {
        const auto PlannerHandle = InTier.PlannerHandle;
        const auto ActionClass = InAction.ActionClass;
        const auto ClassName = InAction.ClassName;

        HeaderRow->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f))
            [
                SNew(SComboButton)
                    .HasDownArrow(false)
                    .ContentPadding(FMargin(CkStyle::SpaceS, 0.0f))
                    .ToolTipText(FText::FromString(TEXT(
                        "Runtime catalog mutation.\nAddAction / Request_RemoveAction — new actions need 1-3 frames of CDO extraction before the next Request_Plan.")))
                    .ButtonContent()
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(TEXT("…")))
                            .Font(CkStyle::BoldFont(CkStyle::FontSizeSmall()))
                            .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                    ]
                    .OnGetMenuContent_Lambda([PlannerHandle, ActionClass, ClassName]() -> TSharedRef<SWidget>
                    {
                        auto MenuBuilder = FMenuBuilder(true /* close after selection */, nullptr);

                        MenuBuilder.BeginSection(NAME_None, FText::FromString(TEXT("Runtime catalog")));
                        MenuBuilder.AddMenuEntry(
                            FText::FromString(FString::Printf(TEXT("Remove %s from catalog"), *ClassName)),
                            FText::FromString(TEXT("Request_RemoveAction — removes this child Action and replans. Recoverable only by an AddAction from gameplay code.")),
                            FSlateIcon(),
                            FUIAction(FExecuteAction::CreateLambda([PlannerHandle, ActionClass]()
                            {
                                if (ck::Is_NOT_Valid(PlannerHandle)) { return; }
                                if (ActionClass == nullptr)          { return; }
                                auto MutablePlanner = PlannerHandle;
                                UCk_Utils_Goap_Planner_UE::Request_RemoveAction(MutablePlanner, ActionClass, {});
                            })));
                        MenuBuilder.AddMenuEntry(
                            FText::FromString(TEXT("Add action\x2026 (from gameplay code)")),
                            FText::FromString(TEXT("AddAction registers a new child at runtime; the new action needs 1-3 frames of CDO extraction before the next Request_Plan sees it.")),
                            FSlateIcon(),
                            FUIAction(
                                FExecuteAction::CreateLambda([]() {}),
                                FCanExecuteAction::CreateLambda([]() { return false; })));
                        MenuBuilder.EndSection();

                        return MenuBuilder.MakeWidget();
                    })
            ];
    }

    auto Chips = SNew(SWrapBox)
        .UseAllottedSize(true)
        .InnerSlotPadding(FVector2D(CkStyle::SpaceXS, CkStyle::SpaceXS));

    // Cross-pane key trace: chips highlight when their key is traced and a
    // click toggles the trace (same channel as WS-rail rows / matrix keys).
    const auto WeakPanel = TWeakPtr<SCkGoapDebugger_CatalogPanel>(SharedThis(this));
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
        auto Label = LeafOfTag(Pre.Key);
        if (NOT Pre.Value) { Label += TEXT(" = false"); }
        Chips->AddSlot()
        [
            SNew(SCkDebug_Chip)
                .Text(FText::FromString(Label))
                .ToolTipText(FText::FromString(FString::Printf(TEXT("%s. Click to trace this key."), *Pre.Key.ToString())))
                .Kind(ECkDebug_ChipKind::Neutral)
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

    return SNew(SBorder)
        .BorderImage(CkStyle::GetRoundedBrush())
        .BorderBackgroundColor(FSlateColor(CkStyle::Bg2()))
        .Padding(FMargin(CkStyle::SpaceM, CkStyle::SpaceS))
        [
            SNew(SVerticalBox)

                + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        HeaderRow
                    ]

                // Class · derived tag line.
                + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(FMargin(0.0f, 2.0f, 0.0f, 0.0f))
                    [
                        SNew(SCkDebug_SelectableLabel)
                            .Text(FText::FromString(FString::Printf(TEXT("%s · tag %s"),
                                *InAction.ClassName,
                                InAction.ActionTag.IsValid() ? *InAction.ActionTag.ToString() : TEXT("(none)"))))
                            .Font(CkStyle::MonoFont(CkStyle::FontSizeMicro()))
                            .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
                    ]

                + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(FMargin(0.0f, CkStyle::SpaceS, 0.0f, 0.0f))
                    [
                        Chips
                    ]
        ];
}

// ====================================================================================================================
// BUILD — HEALTH CHECKS
// ====================================================================================================================

auto
    SCkGoapDebugger_CatalogPanel::
    DoBuildHealthChecks(
        const FCkGoapDebugger_PlannerInfo& InPlanner)
    -> TSharedRef<SWidget>
{
    using namespace ck_goap_debugger_catalog_panel;

    // Lint over the top defs + each composite's sub-catalog.
    const auto TopDefs = MakeDefsForLint(InPlanner.ChildActions);
    auto SubDefsByComposite = TMap<int32, TArray<FCkGoapDebugger_ActionDefLite>>{};
    for (const auto& Child : InPlanner.ChildPlanners)
    {
        const auto* AsAction = InPlanner.ChildActions.FindByPredicate(
            [&Child](const FCkGoapDebugger_ActionInfo& In)
            { return static_cast<FCk_Handle>(In.Handle) == static_cast<FCk_Handle>(Child.PlannerHandle); });
        if (AsAction == nullptr) { continue; }
        const auto DefIndex = TopDefs.IndexOfByPredicate(
            [AsAction](const FCkGoapDebugger_ActionDefLite& In) { return In.ClassName == AsAction->ClassName; });
        if (DefIndex != INDEX_NONE)
        { SubDefsByComposite.Add(DefIndex, MakeDefsForLint(Child.ChildActions)); }
    }

    auto GoalMap = TMap<FName, bool>{};
    for (const auto& Cond : InPlanner.GoalResolved) { GoalMap.Add(Cond.Key.GetTagName(), Cond.Value); }

    const auto Findings = Lint(TopDefs, SubDefsByComposite, GoalMap);

    auto Rows = SNew(SVerticalBox);

    const auto AddCheckRow = [&Rows](const FString& InVerdict, ECk_Tone InTone, const FString& InText)
    {
        Rows->AddSlot()
            .AutoHeight()
            .Padding(ck_goap_debugger_axes::Apply_RowDensity(FMargin{0.0f, 1.0f}))
            [
                SNew(SHorizontalBox)

                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(FMargin(0.0f, 0.0f, CkStyle::SpaceS, 0.0f))
                        [
                            SNew(SBox)
                                .MinDesiredWidth(44.0f)
                                .HAlign(HAlign_Center)
                                [
                                    ck_goap_debugger_axes::Make_Chip(
                                        FText::FromString(InVerdict), InTone)
                                ]
                        ]

                    + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .VAlign(VAlign_Center)
                        [
                            SNew(SCkDebug_SelectableLabel)
                                .Text(FText::FromString(InText))
                                .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
                                .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                        ]
            ];
    };

    // Fallback guarantee (lint).
    {
        const auto HasMissing = Findings.ContainsByPredicate(
            [](const FCkGoapDebugger_LintFinding& In) { return In.Kind == ECkGoapDebugger_LintKind::FallbackMissing; });
        if (HasMissing)
        { AddCheckRow(TEXT("FAIL"), ECk_Tone::Err, TEXT("Fallback guarantee — no unconditional goal-covering action; PlanFailed is reachable (always-valid-plan tenet).")); }
        else
        { AddCheckRow(TEXT("PASS"), ECk_Tone::Ok, TEXT("Fallback guarantee — an unconditional goal-covering action exists (always-valid-plan tenet).")); }
    }

    // Dependency cycles (setup diagnostics).
    if (InPlanner.DependencyCyclesDisplay.Num() > 0)
    { AddCheckRow(TEXT("FAIL"), ECk_Tone::Err, FString::Printf(TEXT("Dependency cycles — %d cycle(s) detected at Setup (Tarjan SCC)."), InPlanner.DependencyCyclesDisplay.Num())); }
    else
    { AddCheckRow(TEXT("PASS"), ECk_Tone::Ok, TEXT("No dependency cycles.")); }

    // Goal keys registered.
    if (InPlanner.InvalidGoalAuthored.Num() > 0)
    { AddCheckRow(TEXT("FAIL"), ECk_Tone::Err, FString::Printf(TEXT("Goal keys registered — %d goal condition(s) reference unregistered WS keys."), InPlanner.InvalidGoalAuthored.Num())); }
    else
    { AddCheckRow(TEXT("PASS"), ECk_Tone::Ok, TEXT("Every goal key is registered in the WorldState.")); }

    // Key budget.
    {
        const auto Used = InPlanner.KeyUsage.Num();
        AddCheckRow(
            Used >= ck::goap::WorldState_MaxKeys ? TEXT("FAIL") : TEXT("INFO"),
            Used >= ck::goap::WorldState_MaxKeys ? ECk_Tone::Err : ECk_Tone::Neutral,
            FString::Printf(TEXT("Key budget — %d / %d WS keys referenced by this planner's subtree."),
                Used, ck::goap::WorldState_MaxKeys));

        Rows->AddSlot()
            .AutoHeight()
            .Padding(FMargin(48.0f, 2.0f, 0.0f, CkStyle::SpaceS))
            [
                SNew(SBox)
                    .WidthOverride(200.0f)
                    .HAlign(HAlign_Left)
                    [
                        SNew(SCkDebug_MeterBar)
                            .Fraction(static_cast<float>(Used) / static_cast<float>(ck::goap::WorldState_MaxKeys))
                            .FillColor(Used >= ck::goap::WorldState_MaxKeys ? CkStyle::Err() : CkStyle::Accent())
                    ]
            ];
    }

    // Cross-tier + dead-effect INFO findings.
    for (const auto& Finding : Findings)
    {
        const auto ActionName = TopDefs.IsValidIndex(Finding.DefIndex)
            ? TopDefs[Finding.DefIndex].ClassName
            : FString(TEXT("(unknown)"));

        switch (Finding.Kind)
        {
            case ECkGoapDebugger_LintKind::CrossTierUnreachable:
                AddCheckRow(TEXT("INFO"), ECk_Tone::Neutral, FString::Printf(
                    TEXT("Cross-tier reachability — %s's effect on %s is only consumed inside a sub-catalog; this tier can't see that value."),
                    *ActionName, *Finding.Key.ToString()));
                break;
            case ECkGoapDebugger_LintKind::DeadEffect:
                AddCheckRow(TEXT("INFO"), ECk_Tone::Neutral, FString::Printf(
                    TEXT("Dead effect — %s sets %s, but no precondition or goal key consumes it."),
                    *ActionName, *Finding.Key.ToString()));
                break;
            default:
                break;
        }
    }

    return SNew(SVerticalBox)

        + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 0.0f, 0.0f, CkStyle::SpaceS))
            [
                SNew(SCkDebug_SelectableLabel)
                    .Text(FText::FromString(TEXT("HEALTH CHECKS")))
                    .Font(CkStyle::BoldFont(CkStyle::FontSizeMicro()))
                    .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
            ]

        + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 0.0f, 0.0f, CkStyle::SpaceL))
            [
                Rows
            ];
}

// ====================================================================================================================
// BUILD — KEY ⟷ ACTION MATRIX
// ====================================================================================================================

auto
    SCkGoapDebugger_CatalogPanel::
    DoBuildMatrix(
        const FCkGoapDebugger_PlannerInfo& InPlanner)
    -> TSharedRef<SWidget>
{
    using namespace ck_goap_debugger_catalog_panel;

    // Columns: every action across the top catalog + sub-catalogs (in tier
    // order). Rows: every key in the usage census, sorted by name.
    auto Actions = TArray<const FCkGoapDebugger_ActionInfo*>{};
    auto InChain = TSet<FString>{};
    {
        for (const auto& Action : InPlanner.ChildActions) { Actions.Add(&Action); }
        for (const auto& Child : InPlanner.ChildPlanners)
        {
            for (const auto& Action : Child.ChildActions) { Actions.Add(&Action); }
        }

        const auto* Cursor = &InPlanner;
        while (Cursor != nullptr && NOT Cursor->PlanClassNames.IsEmpty())
        {
            InChain.Add(Cursor->PlanClassNames[0]);
            const FCkGoapDebugger_PlannerInfo* Next = nullptr;
            if (Cursor->PlanHandles.IsValidIndex(0))
            {
                const auto& Step = Cursor->PlanHandles[0];
                Next = Cursor->ChildPlanners.FindByPredicate(
                    [&Step](const FCkGoapDebugger_PlannerInfo& In)
                    { return static_cast<FCk_Handle>(In.PlannerHandle) == static_cast<FCk_Handle>(Step); });
            }
            Cursor = Next;
        }
    }

    auto Keys = TArray<FGameplayTag>{};
    InPlanner.KeyUsage.GetKeys(Keys);
    Keys.Sort([](const FGameplayTag& A, const FGameplayTag& B) { return A.ToString() < B.ToString(); });

    auto GoalKeys = TSet<FName>{};
    for (const auto& Cond : InPlanner.GoalResolved) { GoalKeys.Add(Cond.Key.GetTagName()); }

    const auto WeakPanel = TWeakPtr<SCkGoapDebugger_CatalogPanel>(SharedThis(this));

    auto Grid = SNew(SGridPanel);

    // Header row — -45° rotated action names. RenderTransform is paint-only
    // (it reserves NO layout space), so each header cell is a fixed-height box
    // and the rotation pivots at its bottom-left: the label paints up-right
    // into the reserved band instead of being clipped above the grid.
    constexpr auto HeaderBandHeight = 96.0f;
    const auto HeaderNameDepth = _ViewModel.IsValid() ? _ViewModel->Get_NameDepth() : 1;
    for (auto Col = 0; Col < Actions.Num(); ++Col)
    {
        const auto& Action = *Actions[Col];
        const auto Highlight = InChain.Contains(Action.ClassName);

        Grid->AddSlot(Col + 1, 0)
            .Padding(FMargin(2.0f, 0.0f))
            .VAlign(VAlign_Bottom)
            [
                SNew(SBox)
                    .HeightOverride(HeaderBandHeight)
                    .VAlign(VAlign_Bottom)
                    .HAlign(HAlign_Left)
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(SCkDebug_NameLabel::Get_ShortName(Action.ClassName, HeaderNameDepth)))
                            .ToolTipText(FText::FromString(Action.ClassName))
                            .Font(CkStyle::BoldFont(CkStyle::FontSizeMicro()))
                            .ColorAndOpacity(FSlateColor(Highlight ? CkStyle::Accent() : CkStyle::TextMute()))
                            .RenderTransform(FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(-45.0f))))
                            .RenderTransformPivot(FVector2D(0.0f, 1.0f))
                    ]
            ];
    }

    for (auto RowIdx = 0; RowIdx < Keys.Num(); ++RowIdx)
    {
        const auto& Key = Keys[RowIdx];
        const auto KeyName = Key.GetTagName();
        const auto GridRow = RowIdx + 1;

        // Key label — click traces the key across panes (same channel as the
        // WS rail); goal keys get a badge.
        auto KeyLabel = FString::Printf(TEXT("%s%s"),
            *LeafOfTag(Key), GoalKeys.Contains(KeyName) ? TEXT("  [goal]") : TEXT(""));

        Grid->AddSlot(0, GridRow)
            .Padding(FMargin(0.0f, 1.0f, CkStyle::SpaceS, 1.0f))
            [
                SNew(SButton)
                    .ButtonStyle(FCoreStyle::Get(), "NoBorder")
                    .ContentPadding(FMargin(2.0f, 0.0f))
                    .ToolTipText(FText::FromString(FString::Printf(TEXT("%s\nClick to trace this key across panes."), *Key.ToString())))
                    .OnClicked_Lambda([WeakPanel, Key]() -> FReply
                    {
                        const auto Pinned = WeakPanel.Pin();
                        if (Pinned.IsValid() && Pinned->_ViewModel.IsValid())
                        {
                            const auto Current = Pinned->_ViewModel->Get_TracedWsKey();
                            Pinned->_ViewModel->Set_TracedWsKey(Current == Key ? FGameplayTag{} : Key);
                        }
                        return FReply::Handled();
                    })
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(KeyLabel))
                            .Font(CkStyle::RegularFont(CkStyle::FontSizeMicro()))
                            .ColorAndOpacity_Lambda([WeakPanel, Key]() -> FSlateColor
                            {
                                const auto Pinned = WeakPanel.Pin();
                                const auto Traced = Pinned.IsValid() && Pinned->_ViewModel.IsValid() &&
                                    Pinned->_ViewModel->Get_TracedWsKey() == Key;
                                return FSlateColor(Traced ? CkStyle::Accent() : CkStyle::TextDim());
                            })
                    ]
            ];

        for (auto Col = 0; Col < Actions.Num(); ++Col)
        {
            const auto& Action = *Actions[Col];

            const auto IsPre = Action.Preconditions.ContainsByPredicate(
                [&Key](const FCkGoapDebugger_Condition& In) { return In.Key == Key; });
            const auto IsEff = Action.Effects.ContainsByPredicate(
                [&Key](const FCkGoapDebugger_Condition& In) { return In.Key == Key; });

            if (NOT IsPre && NOT IsEff) { continue; }

            const auto CellText = IsPre ? TEXT("P") : TEXT("E");
            const auto CellColor = IsPre ? CkStyle::Ok() : CkStyle::Accent();
            const auto ColHighlight = InChain.Contains(Action.ClassName);

            Grid->AddSlot(Col + 1, GridRow)
                .Padding(FMargin(2.0f, 1.0f))
                [
                    SNew(SBorder)
                        .BorderImage(CkStyle::GetFilledBrush())
                        .BorderBackgroundColor(FSlateColor(ColHighlight ? CkStyle::AccentDim() : CkStyle::Bg2()))
                        .HAlign(HAlign_Center)
                        .Padding(FMargin(CkStyle::SpaceS, 0.0f))
                        .ToolTipText(FText::FromString(FString::Printf(TEXT("%s %s %s"),
                            *Action.ClassName,
                            IsPre ? TEXT("requires") : TEXT("sets"),
                            *Key.ToString())))
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(CellText))
                                .Font(CkStyle::BoldFont(CkStyle::FontSizeMicro()))
                                .ColorAndOpacity(FSlateColor(CellColor))
                        ]
                ];
        }
    }

    return SNew(SVerticalBox)

        + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 0.0f, 0.0f, CkStyle::SpaceS))
            [
                SNew(SCkDebug_SelectableLabel)
                    .Text(FText::FromString(TEXT("KEY × ACTION MATRIX")))
                    .Font(CkStyle::BoldFont(CkStyle::FontSizeMicro()))
                    .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
            ]

        + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 28.0f, 0.0f, 0.0f))   // headroom for the rotated headers
            [
                SNew(SScrollBox)
                    .Orientation(Orient_Horizontal)

                    + SScrollBox::Slot()
                    [
                        Grid
                    ]
            ];
}

// ====================================================================================================================
// HANDLERS
// ====================================================================================================================

auto
    SCkGoapDebugger_CatalogPanel::
    HandleCostDelta(
        int32 InDelta,
        FCk_Handle_Goap_Planner InPlanner,
        TSubclassOf<UCk_GoapAction_EntityScript> InClass,
        float InCurrentCost)
    -> void
{
    if (NOT ck::IsValid(InPlanner)) { return; }
    if (InClass == nullptr)         { return; }

    const auto NewCost = FMath::Max(0.1f, InCurrentCost + static_cast<float>(InDelta) * 0.5f);

    auto MutablePlanner = InPlanner;
    UCk_Utils_Goap_Planner_UE::Request_SetChildActionCost(MutablePlanner, InClass, NewCost, {});
}

// ====================================================================================================================
