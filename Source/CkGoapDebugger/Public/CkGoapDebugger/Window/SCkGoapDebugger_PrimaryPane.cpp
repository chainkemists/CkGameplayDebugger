#include "CkGoapDebugger/Window/SCkGoapDebugger_PrimaryPane.h"

#include "CkGoapDebugger/CkGoapDebuggerStyle.h"
#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"
#include "CkGoapDebugger/Window/SCkGoapDebugger_PlanStrip.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_KeyValueRow.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_LabeledGroup.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatPair.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"

#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================
// Internal helpers — file-prefixed (`_PrimaryPane`) to avoid Adaptive-Unity
// anonymous-namespace collisions with sibling .cpp files that share these
// names by convention (MakeBadge, ResolveStatusBadge, etc.).
// ====================================================================================================================

namespace
{
    auto ToneForPlanStatus_PrimaryPane(ECk_GoapPlanStatus InStatus) -> ECkDebug_Tone
    {
        switch (InStatus)
        {
        case ECk_GoapPlanStatus::PlanFound:            return ECkDebug_Tone::Ok;
        case ECk_GoapPlanStatus::Planning:             return ECkDebug_Tone::Info;
        case ECk_GoapPlanStatus::PlanFailed:           return ECkDebug_Tone::Err;
        case ECk_GoapPlanStatus::CostThresholdReached: return ECkDebug_Tone::Warn;
        case ECk_GoapPlanStatus::Idle:
        default:                                       return ECkDebug_Tone::Neutral;
        }
    }

    // Variant that honours the per-Planner _AllowPlanFailed opt-out — when the
    // Planner explicitly tolerates "no plan possible", swap the error tone for
    // a Warn (amber) tone so PlanFailed reads as policy, not bug.
    auto ToneForPlanStatusWithOptOut_PrimaryPane(
        ECk_GoapPlanStatus InStatus,
        bool InAllowPlanFailed) -> ECkDebug_Tone
    {
        if (InStatus == ECk_GoapPlanStatus::PlanFailed && InAllowPlanFailed)
        { return ECkDebug_Tone::Warn; }
        return ToneForPlanStatus_PrimaryPane(InStatus);
    }

    auto LabelForPlanStatus_PrimaryPane(ECk_GoapPlanStatus InStatus) -> FString
    {
        switch (InStatus)
        {
        case ECk_GoapPlanStatus::PlanFound:            return TEXT("PlanFound");
        case ECk_GoapPlanStatus::Planning:             return TEXT("Planning");
        case ECk_GoapPlanStatus::PlanFailed:           return TEXT("PlanFailed");
        case ECk_GoapPlanStatus::CostThresholdReached: return TEXT("CostThresh");
        case ECk_GoapPlanStatus::Idle:
        default:                                       return TEXT("Idle");
        }
    }

    // Resolve a goal condition's satisfied state against the Planner's
    // resolved-WS snapshot (carried on the PlannerInfo when DataCollector
    // populates it; falls back to empty TOptional otherwise).
    auto LookupWsValue_PrimaryPane(
        const FCkGoapDebugger_PlannerInfo& InPlanner,
        const FGameplayTag& InKey) -> TOptional<bool>
    {
        for (const auto& Entry : InPlanner.WorldState)
        {
            if (Entry.Key == InKey)
            { return TOptional<bool>(Entry.Value); }
        }
        return TOptional<bool>{};
    }

    // Goal / condition row: ✓ or ✗ glyph + "Key = Value".
    auto MakeGoalRow_PrimaryPane(
        const FString& InGlyph,
        const FString& InText,
        const FLinearColor& InColor) -> TSharedRef<SWidget>
    {
        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Small, 0.0f)
                [
                    SNew(SCkDebug_SelectableLabel)
                        .Text(FText::FromString(InGlyph))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                        .ColorAndOpacity(FSlateColor(InColor))
                ]
            + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(SCkDebug_SelectableLabel)
                        .Text(FText::FromString(InText))
                        .Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
                        .ColorAndOpacity(FSlateColor(InColor))
                ];
    }

    // Italic muted hint line — used as the first row in a LabeledGroup body to
    // give a short contextual description below the section header. (LabeledGroup
    // doesn't have a built-in hint slot; this is the convention.)
    auto MakeHintRow_PrimaryPane(const FString& InText) -> TSharedRef<SWidget>
    {
        return SNew(SBox)
            .Padding(FMargin(0.0f, 0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Small))
            [
                SNew(SCkDebug_SelectableLabel)
                    .Text(FText::FromString(InText))
                    .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
                    .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Faint))
            ];
    }
}

// ====================================================================================================================
// CONSTRUCT / DESTRUCT
// ====================================================================================================================

auto
    SCkGoapDebugger_PrimaryPane::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _ViewModel = InArgs._ViewModel;

    // PlanStrip is created once and reused — its own hash gate debounces
    // refreshes. Reassigning it on every outer refresh would discard the
    // gate state and re-fire content updates every tick.
    SAssignNew(_PlanStrip, SCkGoapDebugger_PlanStrip)
        .ViewModel(_ViewModel);

    ChildSlot
    [
        SNew(SBorder)
            .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Root")))
            .Padding(FMargin(0.0f))
            [
                SNew(SVerticalBox)

                    // ---- Compact header (title + role pills + tag) ---------------
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

                    // ---- Body: PLAN tile (full width) + 3-tile meta row ----------
                    + SVerticalBox::Slot()
                        .FillHeight(1.0f)
                        [
                            SNew(SBorder)
                                .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Root")))
                                .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium))
                                [
                                    SNew(SScrollBox)
                                        .Orientation(Orient_Vertical)
                                        + SScrollBox::Slot()
                                        [
                                            SNew(SVerticalBox)

                                                // PLAN tile — full width, on its own row
                                                + SVerticalBox::Slot()
                                                    .AutoHeight()
                                                    .Padding(FMargin(0.0f, 0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium))
                                                    [
                                                        SAssignNew(_PlanTileHost, SBox)
                                                    ]

                                                // Meta row — 3 tiles side by side
                                                + SVerticalBox::Slot()
                                                    .AutoHeight()
                                                    [
                                                        SNew(SHorizontalBox)
                                                            + SHorizontalBox::Slot()
                                                                .FillWidth(1.3f)
                                                                .Padding(FMargin(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f))
                                                                [
                                                                    SAssignNew(_IdentityTileHost, SBox)
                                                                ]
                                                            + SHorizontalBox::Slot()
                                                                .FillWidth(1.0f)
                                                                .Padding(FMargin(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f))
                                                                [
                                                                    SAssignNew(_GoalTileHost, SBox)
                                                                ]
                                                            + SHorizontalBox::Slot()
                                                                .FillWidth(1.0f)
                                                                [
                                                                    SAssignNew(_WiringTileHost, SBox)
                                                                ]
                                                    ]
                                        ]
                                ]
                        ]
            ]
    ];

    RefreshFromViewModel();
}

SCkGoapDebugger_PrimaryPane::~SCkGoapDebugger_PrimaryPane() = default;

// ====================================================================================================================
// REFRESH
// ====================================================================================================================

auto
    SCkGoapDebugger_PrimaryPane::
    RefreshFromViewModel()
    -> void
{
    if (NOT _ViewModel.IsValid()) { return; }

    const auto* Planner = _ViewModel->GetSelectedPlannerInfo();

    // Structural hash — every field the build paths read. Plan content is
    // delegated to the embedded PlanStrip (which has its own hash gate), so
    // PlanHandles/Names don't contribute here.
    auto NewHash = uint32{0};
    if (Planner != nullptr)
    {
        NewHash = HashCombine(NewHash, ::GetTypeHash(static_cast<FCk_Handle>(Planner->PlannerHandle)));
        NewHash = HashCombine(NewHash, ::GetTypeHash(static_cast<uint8>(Planner->PlanStatus)));
        NewHash = HashCombine(NewHash, ::GetTypeHash(Planner->PlanCost));
        NewHash = HashCombine(NewHash, ::GetTypeHash(Planner->PlanAttemptCount));
        NewHash = HashCombine(NewHash, ::GetTypeHash(Planner->IsActionRole));
        NewHash = HashCombine(NewHash, ::GetTypeHash(Planner->AllowPlanFailed));
        NewHash = HashCombine(NewHash, ::GetTypeHash(Planner->ChildActions.Num()));
        NewHash = HashCombine(NewHash, ::GetTypeHash(Planner->GoalResolved.Num()));
        NewHash = HashCombine(NewHash, ::GetTypeHash(Planner->InvalidGoalAuthored.Num()));
        NewHash = HashCombine(NewHash, ::GetTypeHash(Planner->DependencyCyclesDisplay.Num()));
        NewHash = HashCombine(NewHash, GetTypeHash(Planner->WorldStateSourceLabel));

        // Fold WS values into the hash so goal sat/unsat flips re-render.
        for (const auto& Ws : Planner->WorldState)
        {
            auto Pair = GetTypeHash(Ws.Key);
            Pair = HashCombine(Pair, ::GetTypeHash(Ws.Value ? 1 : 0));
            NewHash ^= Pair;
        }
    }
    // Re-render when name-depth verbosity changes — picks up any class-name-
    // dependent strings nested in build paths below.
    NewHash = HashCombine(NewHash, ::GetTypeHash(_ViewModel->Get_NameDepth()));

    // Always forward to the plan strip — it has its own gate.
    if (_PlanStrip.IsValid())
    { _PlanStrip->RefreshFromViewModel(); }

    if (_HasMaterialized && NewHash == _LastContentHash) { return; }
    _LastContentHash = NewHash;
    _HasMaterialized = true;

    if (Planner == nullptr)
    {
        if (_HeaderHost.IsValid())     { _HeaderHost->SetContent(BuildEmptyState()); }
        if (_PlanTileHost.IsValid())   { _PlanTileHost->SetContent(SNew(SSpacer)); }
        if (_IdentityTileHost.IsValid()) { _IdentityTileHost->SetContent(SNew(SSpacer)); }
        if (_GoalTileHost.IsValid())   { _GoalTileHost->SetContent(SNew(SSpacer)); }
        if (_WiringTileHost.IsValid()) { _WiringTileHost->SetContent(SNew(SSpacer)); }
        return;
    }

    if (_HeaderHost.IsValid())       { _HeaderHost->SetContent(BuildHeader(*Planner)); }
    if (_PlanTileHost.IsValid())     { _PlanTileHost->SetContent(BuildPlanTile(*Planner)); }
    if (_IdentityTileHost.IsValid()) { _IdentityTileHost->SetContent(BuildIdentityStatusTile(*Planner)); }
    if (_GoalTileHost.IsValid())     { _GoalTileHost->SetContent(BuildGoalTile(*Planner)); }
    if (_WiringTileHost.IsValid())   { _WiringTileHost->SetContent(BuildWiringTile(*Planner)); }
}

// ====================================================================================================================
// BUILD — EMPTY STATE
// ====================================================================================================================

auto
    SCkGoapDebugger_PrimaryPane::
    BuildEmptyState()
    -> TSharedRef<SWidget>
{
    return SNew(SBorder)
        .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Surface")))
        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Large))
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        [
            SNew(SCkDebug_SelectableLabel)
                .Text(FText::FromString(TEXT("Select an entity and a Planner to inspect a plan.")))
                .Font(FCoreStyle::GetDefaultFontStyle("Italic", 10))
                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim))
        ];
}

// ====================================================================================================================
// BUILD — COMPACT HEADER  (title + role pills + tag, single row)
// ====================================================================================================================

auto
    SCkGoapDebugger_PrimaryPane::
    BuildHeader(
        const FCkGoapDebugger_PlannerInfo& InPlanner)
    -> TSharedRef<SWidget>
{
    const auto TitleText = InPlanner.DisplayName.IsEmpty()
        ? (InPlanner.PlannerTag.IsValid() ? InPlanner.PlannerTag.ToString() : FString(TEXT("(unknown planner)")))
        : InPlanner.DisplayName;

    const auto IsTopLevel = NOT ck::IsValid(InPlanner.ParentPlanner);

    auto BadgeBox = SNew(SHorizontalBox);

    // PLANNER (always) — sub-label distinguishes top-level vs sub.
    BadgeBox->AddSlot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(FMargin(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_XSmall, 0.0f))
        [
            SNew(SCkDebug_StatusPill)
                .Text(FText::FromString(IsTopLevel
                    ? FString(TEXT("PLANNER · TOP-LEVEL"))
                    : FString(TEXT("PLANNER · SUB"))))
                .Tone(ECkDebug_Tone::Accent)
                .ShowDot(false)
        ];

    if (InPlanner.IsActionRole)
    {
        BadgeBox->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_XSmall, 0.0f))
            [
                SNew(SCkDebug_StatusPill)
                    .Text(FText::FromString(TEXT("ACTION")))
                    .Tone(ECkDebug_Tone::Info)
                    .ShowDot(false)
            ];
    }

    if (InPlanner.AllowPlanFailed)
    {
        BadgeBox->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SCkDebug_StatusPill)
                    .Text(FText::FromString(TEXT("OPT-OUT · PlanFailed allowed")))
                    .Tone(ECkDebug_Tone::Warn)
                    .ShowDot(false)
            ];
    }

    return SNew(SBorder)
        .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Panel")))
        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Large, FCkGoapDebuggerStyle::Padding_Medium))
        [
            SNew(SHorizontalBox)

                // Title (large amber)
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(FMargin(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f))
                    [
                        SNew(SCkDebug_SelectableLabel)
                            .Text(FText::FromString(TitleText))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 15))
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Status_Selected))
                    ]

                // Role badges
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(FMargin(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f))
                    [
                        BadgeBox
                    ]

                // Tag (mono, dim, right-aligned)
                + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    .HAlign(HAlign_Right)
                    [
                        SNew(SCkDebug_SelectableLabel)
                            .Text(FText::FromString(InPlanner.PlannerTag.IsValid()
                                ? InPlanner.PlannerTag.ToString() : FString{}))
                            .Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim))
                    ]
        ];
}

// ====================================================================================================================
// BUILD — PLAN TILE  (hosts the PlanStrip widget; full-width row)
// ====================================================================================================================

auto
    SCkGoapDebugger_PrimaryPane::
    BuildPlanTile(
        const FCkGoapDebugger_PlannerInfo& InPlanner)
    -> TSharedRef<SWidget>
{
    const auto StepCount = InPlanner.PlanHandles.Num();
    const auto CandCount = InPlanner.ChildActions.Num();
    const auto CountText = FString::Printf(
        TEXT("%d step%s · %d candidate%s"),
        StepCount, StepCount == 1 ? TEXT("") : TEXT("s"),
        CandCount, CandCount == 1 ? TEXT("") : TEXT("s"));

    auto Group = SNew(SCkDebug_LabeledGroup)
        .Label(FText::FromString(TEXT("Plan")))
        .CountText(FText::FromString(CountText));

    Group->AddChild(MakeHintRow_PrimaryPane(TEXT("multi-step possible · Plan[0] is the active step")));

    if (_PlanStrip.IsValid())
    { Group->AddChild(_PlanStrip.ToSharedRef()); }

    return Group;
}

// ====================================================================================================================
// BUILD — IDENTITY & STATUS TILE
// ====================================================================================================================

auto
    SCkGoapDebugger_PrimaryPane::
    BuildIdentityStatusTile(
        const FCkGoapDebugger_PlannerInfo& InPlanner)
    -> TSharedRef<SWidget>
{
    auto Group = SNew(SCkDebug_LabeledGroup)
        .Label(FText::FromString(TEXT("Identity & status")));

    auto Stats = SNew(SHorizontalBox)
        // Status pill
        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Large, 0.0f))
            [
                SNew(SCkDebug_StatusPill)
                    .Text(FText::FromString(LabelForPlanStatus_PrimaryPane(InPlanner.PlanStatus)))
                    .Tone(ToneForPlanStatusWithOptOut_PrimaryPane(
                        InPlanner.PlanStatus, InPlanner.AllowPlanFailed))
                    .ShowDot(true)
            ]
        // Cost (amber)
        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Large, 0.0f))
            [
                SNew(SCkDebug_StatPair)
                    .Value(FText::FromString(FString::Printf(TEXT("$%d"), FMath::RoundToInt(InPlanner.PlanCost))))
                    .Label(FText::FromString(TEXT("COST")))
                    .ValueColor(FSlateColor(FCkGoapDebuggerStyle::Color_Status_Selected))
            ]
        // Attempts
        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Large, 0.0f))
            [
                SNew(SCkDebug_StatPair)
                    .Value(FText::FromString(FString::Printf(TEXT("%d"), InPlanner.PlanAttemptCount)))
                    .Label(FText::FromString(InPlanner.PlanAttemptCount == 1
                        ? FString(TEXT("ATTEMPT")) : FString(TEXT("ATTEMPTS"))))
            ]
        // Children
        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SCkDebug_StatPair)
                    .Value(FText::FromString(FString::Printf(TEXT("%d"), InPlanner.ChildActions.Num())))
                    .Label(FText::FromString(InPlanner.ChildActions.Num() == 1
                        ? FString(TEXT("CHILD")) : FString(TEXT("CHILDREN"))))
            ];

    Group->AddChild(Stats);
    return Group;
}

// ====================================================================================================================
// BUILD — GOAL TILE
// ====================================================================================================================

auto
    SCkGoapDebugger_PrimaryPane::
    BuildGoalTile(
        const FCkGoapDebugger_PlannerInfo& InPlanner)
    -> TSharedRef<SWidget>
{
    auto Group = SNew(SCkDebug_LabeledGroup)
        .Label(FText::FromString(TEXT("Goal")));

    Group->AddChild(MakeHintRow_PrimaryPane(TEXT("this tier — independent of any other tier")));

    const auto HasResolved = InPlanner.GoalResolved.Num() > 0;
    const auto HasInvalid  = InPlanner.InvalidGoalAuthored.Num() > 0;

    if (NOT HasResolved && NOT HasInvalid)
    {
        Group->AddChild(
            SNew(SCkDebug_SelectableLabel)
                .Text(FText::FromString(TEXT("(no goal set — Planner stays Idle)")))
                .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim)));
        return Group;
    }

    for (const auto& Cond : InPlanner.GoalResolved)
    {
        const auto WsOpt = LookupWsValue_PrimaryPane(InPlanner, Cond.Key);
        const auto Sat   = WsOpt.IsSet() && (WsOpt.GetValue() == Cond.Value);

        const auto Glyph = Sat ? FString(TEXT("✓")) : FString(TEXT("✗"));  // ✓ / ✗
        const auto Color = Sat
            ? FCkGoapDebuggerStyle::Color_Status_PlanFound
            : FCkGoapDebuggerStyle::Color_Status_Failed;

        const auto Text = FString::Printf(TEXT("%s = %s"),
            *Cond.Key.ToString(),
            Cond.Value ? TEXT("true") : TEXT("false"));

        Group->AddChild(MakeGoalRow_PrimaryPane(Glyph, Text, Color));
    }

    // _InvalidGoal — authored conditions referencing keys not in the
    // resolved WS registry. Render with a warning glyph.
    for (const auto& Cond : InPlanner.InvalidGoalAuthored)
    {
        const auto Text = FString::Printf(TEXT("%s = %s  (unregistered key)"),
            *Cond.Get_Key().ToString(),
            Cond.Get_Value() ? TEXT("true") : TEXT("false"));

        Group->AddChild(MakeGoalRow_PrimaryPane(TEXT("⚠"), Text, FCkGoapDebuggerStyle::Color_Status_Selected));  // ⚠
    }

    return Group;
}

// ====================================================================================================================
// BUILD — DIAGNOSTICS & WIRING TILE
// ====================================================================================================================

auto
    SCkGoapDebugger_PrimaryPane::
    BuildWiringTile(
        const FCkGoapDebugger_PlannerInfo& InPlanner)
    -> TSharedRef<SWidget>
{
    auto Group = SNew(SCkDebug_LabeledGroup)
        .Label(FText::FromString(TEXT("Diagnostics & wiring")));

    // ---- Worldstate source ----
    const auto WsLabel = InPlanner.WorldStateSourceLabel.IsEmpty()
        ? FString(TEXT("(inherited)"))
        : InPlanner.WorldStateSourceLabel;
    Group->AddChild(
        SNew(SCkDebug_KeyValueRow)
            .KeyText(FText::FromString(TEXT("Worldstate")))
            .ValueText(FText::FromString(WsLabel))
            .Tone(ECkDebug_KeyValueTone::Custom)
            .CustomValueColor(FCkGoapDebuggerStyle::Color_Status_Selected));

    // ---- Parent planner ----
    const auto ParentText = ck::IsValid(InPlanner.ParentPlanner)
        ? InPlanner.ParentPlanner.ToString()
        : FString(TEXT("none — top-level"));
    Group->AddChild(
        SNew(SCkDebug_KeyValueRow)
            .KeyText(FText::FromString(TEXT("Parent")))
            .ValueText(FText::FromString(ParentText))
            .Tone(ECkDebug_KeyValueTone::Custom)
            .CustomValueColor(ck::IsValid(InPlanner.ParentPlanner)
                ? FCkGoapDebuggerStyle::Color_Text_Primary
                : FCkGoapDebuggerStyle::Color_Text_Muted));

    // ---- Invalid goal keys ----
    if (InPlanner.InvalidGoalAuthored.Num() == 0)
    {
        Group->AddChild(
            SNew(SCkDebug_KeyValueRow)
                .KeyText(FText::FromString(TEXT("Invalid keys")))
                .ValueText(FText::FromString(TEXT("✓ none")))  // ✓ none
                .Tone(ECkDebug_KeyValueTone::Custom)
                .CustomValueColor(FCkGoapDebuggerStyle::Color_Status_PlanFound));
    }
    else
    {
        auto KeysBox = SNew(SVerticalBox);
        for (const auto& Cond : InPlanner.InvalidGoalAuthored)
        {
            KeysBox->AddSlot()
                .AutoHeight()
                .Padding(FMargin(0.0f, 0.0f, 0.0f, 1.0f))
                [
                    SNew(SCkDebug_SelectableLabel)
                        .Text(FText::FromString(FString::Printf(TEXT("⚠ %s"),
                            *Cond.Get_Key().ToString())))  // ⚠
                        .Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
                        .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Status_Selected))
                ];
        }
        Group->AddChild(
            SNew(SCkDebug_KeyValueRow)
                .KeyText(FText::FromString(TEXT("Invalid keys")))
                .Tone(ECkDebug_KeyValueTone::Custom)
                .ValueWidget()
                [
                    KeysBox
                ]);
    }

    // ---- Dependency cycles ----
    if (InPlanner.DependencyCyclesDisplay.Num() == 0)
    {
        Group->AddChild(
            SNew(SCkDebug_KeyValueRow)
                .KeyText(FText::FromString(TEXT("Dep. cycles")))
                .ValueText(FText::FromString(TEXT("✓ none")))  // ✓ none
                .Tone(ECkDebug_KeyValueTone::Custom)
                .CustomValueColor(FCkGoapDebuggerStyle::Color_Status_PlanFound));
    }
    else
    {
        auto CyclesBox = SNew(SVerticalBox);
        for (const auto& Cycle : InPlanner.DependencyCyclesDisplay)
        {
            const auto Joined = FString::Join(Cycle.ActionsInCycle, TEXT(" -> "));
            CyclesBox->AddSlot()
                .AutoHeight()
                .Padding(FMargin(0.0f, 0.0f, 0.0f, 1.0f))
                [
                    SNew(SCkDebug_SelectableLabel)
                        .Text(FText::FromString(Joined))
                        .Font(FCoreStyle::GetDefaultFontStyle("Mono", 8))
                        .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Status_Failed))
                ];
        }
        Group->AddChild(
            SNew(SCkDebug_KeyValueRow)
                .KeyText(FText::FromString(TEXT("Dep. cycles")))
                .Tone(ECkDebug_KeyValueTone::Custom)
                .ValueWidget()
                [
                    CyclesBox
                ]);
    }

    return Group;
}

// ====================================================================================================================
