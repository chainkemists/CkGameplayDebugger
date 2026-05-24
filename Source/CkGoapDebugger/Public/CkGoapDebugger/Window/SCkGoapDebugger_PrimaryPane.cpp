#include "CkGoapDebugger/Window/SCkGoapDebugger_PrimaryPane.h"

#include "CkGoapDebugger/CkGoapDebuggerStyle.h"
#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"
#include "CkGoapDebugger/Window/SCkGoapDebugger_PlanStrip.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"

#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================
// Internal helpers — file-prefixed (`_PrimaryPane`) to avoid Adaptive-Unity
// anonymous-namespace collisions with the Sidebar / PlanStrip / Breadcrumb
// helpers that share these names by convention (MakeBadge, ResolveStatusBadge,
// etc.).
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

    auto MakeSectionHeader_PrimaryPane(const FString& InText) -> TSharedRef<SWidget>
    {
        return SNew(SBox)
            .Padding(FMargin(0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f, FCkGoapDebuggerStyle::Padding_Small))
            [
                SNew(STextBlock)
                    .Text(FText::FromString(InText.ToUpper()))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
                    .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted))
            ];
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
                    SNew(STextBlock)
                        .Text(FText::FromString(InGlyph))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                        .ColorAndOpacity(FSlateColor(InColor))
                ]
            + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(InText))
                        .Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
                        .ColorAndOpacity(FSlateColor(InColor))
                ];
    }

    auto MakeRailBlock_PrimaryPane(
        const FString& InLabel,
        const FString& InValue,
        const FLinearColor& InValueColor) -> TSharedRef<SWidget>
    {
        return SNew(SVerticalBox)
            + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, 0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_XSmall)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(InLabel.ToUpper()))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
                        .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted))
                ]
            + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(InValue.IsEmpty() ? FString(TEXT("(none)")) : InValue))
                        .Font(InValue.IsEmpty()
                            ? FCoreStyle::GetDefaultFontStyle("Italic", 10)
                            : FCoreStyle::GetDefaultFontStyle("Regular", 10))
                        .ColorAndOpacity(FSlateColor(InValueColor))
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

    ChildSlot
    [
        SNew(SBorder)
            .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Root")))
            .Padding(FMargin(0.0f))
            [
                SNew(SVerticalBox)

                    // ---- Header bar (title + role badges + sub-head) -----------------
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

                    // ---- Body: left scrollable column + right rail -------------------
                    + SVerticalBox::Slot()
                        .FillHeight(1.0f)
                        [
                            SNew(SHorizontalBox)

                                + SHorizontalBox::Slot()
                                    .FillWidth(1.0f)
                                    [
                                        SNew(SBorder)
                                            .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Root")))
                                            .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Large,
                                                             FCkGoapDebuggerStyle::Padding_Medium))
                                            [
                                                SNew(SScrollBox)
                                                    .Orientation(Orient_Vertical)
                                                    + SScrollBox::Slot()
                                                    [
                                                        SAssignNew(_LeftBody, SVerticalBox)
                                                    ]
                                            ]
                                    ]

                                + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    [
                                        SNew(SBox)
                                            .WidthOverride(280.0f)
                                            [
                                                SAssignNew(_RightRailHost, SBox)
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
    // Re-render when name-depth verbosity changes — pickups any class-name
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
        if (_HeaderHost.IsValid())    { _HeaderHost->SetContent(BuildEmptyState()); }
        if (_LeftBody.IsValid())      { _LeftBody->ClearChildren(); }
        if (_RightRailHost.IsValid()) { _RightRailHost->SetContent(SNew(SSpacer)); }
        return;
    }

    // ---- Header ---------------------------------------------------------------
    if (_HeaderHost.IsValid())
    { _HeaderHost->SetContent(BuildHeader(*Planner)); }

    // ---- Left body ------------------------------------------------------------
    if (_LeftBody.IsValid())
    {
        _LeftBody->ClearChildren();

        // Status row (status pill + cost + attempts + replan)
        _LeftBody->AddSlot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Small))
            [
                BuildStatusRow(*Planner)
            ];

        // PLAN section header
        _LeftBody->AddSlot()
            .AutoHeight()
            [
                MakeSectionHeader_PrimaryPane(TEXT("Plan (multi-step possible; Plan[0] is the active step)"))
            ];

        // Embedded plan strip
        _LeftBody->AddSlot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium))
            [
                SAssignNew(_PlanStrip, SCkGoapDebugger_PlanStrip)
                    .ViewModel(_ViewModel)
            ];

        // GOAL section
        _LeftBody->AddSlot()
            .AutoHeight()
            [
                BuildGoalSection(*Planner)
            ];
    }

    // ---- Right rail -----------------------------------------------------------
    if (_RightRailHost.IsValid())
    {
        _RightRailHost->SetContent(BuildRightRail(*Planner));
    }
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
            SNew(STextBlock)
                .Text(FText::FromString(TEXT("Select an entity and a Planner to inspect a plan.")))
                .Font(FCoreStyle::GetDefaultFontStyle("Italic", 10))
                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim))
        ];
}

// ====================================================================================================================
// BUILD — HEADER
// ====================================================================================================================

auto
    SCkGoapDebugger_PrimaryPane::
    BuildHeader(
        const FCkGoapDebugger_PlannerInfo& InPlanner)
    -> TSharedRef<SWidget>
{
    // Title — uses DisplayName; falls back to PlannerTag string.
    const auto TitleText = InPlanner.DisplayName.IsEmpty()
        ? (InPlanner.PlannerTag.IsValid() ? InPlanner.PlannerTag.ToString() : FString(TEXT("(unknown planner)")))
        : InPlanner.DisplayName;

    // Sub-head — "Parent: ..." or "(top-level)".
    const auto IsTopLevel = NOT ck::IsValid(InPlanner.ParentPlanner);
    const auto SubText    = IsTopLevel
        ? FString(TEXT("(top-level Planner)"))
        : FString::Printf(TEXT("sub-Planner · parent: %s"),
            *(InPlanner.ParentPlanner.ToString()));

    // Role badge cluster.
    auto BadgeBox = SNew(SHorizontalBox);

    BadgeBox->AddSlot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(FMargin(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_XSmall, 0.0f))
        [
            SNew(SCkDebug_StatusPill)
                .Text(FText::FromString(TEXT("PLANNER")))
                .Tone(ECkDebug_Tone::Accent)
                .ShowDot(false)
        ];

    if (InPlanner.IsActionRole)
    {
        BadgeBox->AddSlot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SCkDebug_StatusPill)
                    .Text(FText::FromString(TEXT("ACTION")))
                    .Tone(ECkDebug_Tone::Info)
                    .ShowDot(false)
            ];
    }

    return SNew(SBorder)
        .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Panel")))
        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Large, FCkGoapDebuggerStyle::Padding_Medium))
        [
            SNew(SVerticalBox)

                // Title row (name + role badges + tag)
                + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                                .AutoWidth()
                                .VAlign(VAlign_Center)
                                .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f)
                                [
                                    SNew(STextBlock)
                                        .Text(FText::FromString(TitleText))
                                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
                                        .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Status_Selected))
                                ]
                            + SHorizontalBox::Slot()
                                .AutoWidth()
                                .VAlign(VAlign_Center)
                                .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f)
                                [
                                    BadgeBox
                                ]
                            + SHorizontalBox::Slot()
                                .FillWidth(1.0f)
                                .VAlign(VAlign_Center)
                                [
                                    SNew(STextBlock)
                                        .Text(FText::FromString(InPlanner.PlannerTag.IsValid()
                                            ? InPlanner.PlannerTag.ToString() : FString{}))
                                        .Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
                                        .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim))
                                ]
                    ]

                // Sub-head
                + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(FMargin(0.0f, FCkGoapDebuggerStyle::Padding_XSmall, 0.0f, 0.0f))
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(SubText))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Secondary))
                    ]
        ];
}

// ====================================================================================================================
// BUILD — STATUS ROW
// ====================================================================================================================

auto
    SCkGoapDebugger_PrimaryPane::
    BuildStatusRow(
        const FCkGoapDebugger_PlannerInfo& InPlanner)
    -> TSharedRef<SWidget>
{
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f)
            [
                SNew(SCkDebug_StatusPill)
                    .Text(FText::FromString(LabelForPlanStatus_PrimaryPane(InPlanner.PlanStatus)))
                    .Tone(ToneForPlanStatus_PrimaryPane(InPlanner.PlanStatus))
                    .ShowDot(true)
            ]
        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(FString::Printf(TEXT("Cost: $%d"),
                        FMath::RoundToInt(InPlanner.PlanCost))))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                    .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Status_Selected))
            ]
        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(FString::Printf(TEXT("Attempts: %d"),
                        InPlanner.PlanAttemptCount)))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                    .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Secondary))
            ];
}

// ====================================================================================================================
// BUILD — GOAL SECTION
// ====================================================================================================================

auto
    SCkGoapDebugger_PrimaryPane::
    BuildGoalSection(
        const FCkGoapDebugger_PlannerInfo& InPlanner)
    -> TSharedRef<SWidget>
{
    auto Box = SNew(SVerticalBox);

    Box->AddSlot()
        .AutoHeight()
        [
            MakeSectionHeader_PrimaryPane(TEXT("Goal (this tier — independent of any other tier)"))
        ];

    const auto HasResolved = InPlanner.GoalResolved.Num() > 0;
    const auto HasInvalid  = InPlanner.InvalidGoalAuthored.Num() > 0;

    if (NOT HasResolved && NOT HasInvalid)
    {
        Box->AddSlot()
            .AutoHeight()
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("(no goal set — Planner stays Idle)")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
                    .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim))
            ];
        return Box;
    }

    for (const auto& Cond : InPlanner.GoalResolved)
    {
        const auto WsOpt = LookupWsValue_PrimaryPane(InPlanner, Cond.Key);
        const auto Sat   = WsOpt.IsSet() && (WsOpt.GetValue() == Cond.Value);

        const auto Glyph = Sat ? FString(TEXT("✓")) : FString(TEXT("✗"));
        const auto Color = Sat
            ? FCkGoapDebuggerStyle::Color_Status_PlanFound
            : FCkGoapDebuggerStyle::Color_Status_Failed;

        const auto Text  = FString::Printf(TEXT("%s = %s"),
            *Cond.Key.ToString(),
            Cond.Value ? TEXT("true") : TEXT("false"));

        Box->AddSlot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 1.0f))
            [
                MakeGoalRow_PrimaryPane(Glyph, Text, Color)
            ];
    }

    // _InvalidGoal — authored conditions referencing keys not in the
    // resolved WS registry. Render with a warning glyph.
    for (const auto& Cond : InPlanner.InvalidGoalAuthored)
    {
        const auto Text = FString::Printf(TEXT("%s = %s  (unregistered key)"),
            *Cond.Get_Key().ToString(),
            Cond.Get_Value() ? TEXT("true") : TEXT("false"));

        Box->AddSlot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 1.0f))
            [
                MakeGoalRow_PrimaryPane(TEXT("⚠"), Text, FCkGoapDebuggerStyle::Color_Status_Selected)
            ];
    }

    return Box;
}

// ====================================================================================================================
// BUILD — RIGHT RAIL
// ====================================================================================================================

auto
    SCkGoapDebugger_PrimaryPane::
    BuildRightRail(
        const FCkGoapDebugger_PlannerInfo& InPlanner)
    -> TSharedRef<SWidget>
{
    auto Box = SNew(SVerticalBox);

    // WS source -----------------------------------------------------------------
    Box->AddSlot()
        .AutoHeight()
        .Padding(FMargin(0.0f, 0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium))
        [
            MakeRailBlock_PrimaryPane(
                TEXT("WorldState source"),
                InPlanner.WorldStateSourceLabel.IsEmpty()
                    ? FString(TEXT("(inherited)"))
                    : InPlanner.WorldStateSourceLabel,
                FCkGoapDebuggerStyle::Color_Status_Selected)
        ];

    // Parent planner ------------------------------------------------------------
    const auto ParentText = ck::IsValid(InPlanner.ParentPlanner)
        ? InPlanner.ParentPlanner.ToString()
        : FString(TEXT("(none — top-level)"));
    const auto ParentColor = ck::IsValid(InPlanner.ParentPlanner)
        ? FCkGoapDebuggerStyle::Color_Text_Primary
        : FCkGoapDebuggerStyle::Color_Text_Dim;

    Box->AddSlot()
        .AutoHeight()
        .Padding(FMargin(0.0f, 0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium))
        [
            MakeRailBlock_PrimaryPane(TEXT("Parent planner"), ParentText, ParentColor)
        ];

    // Role role-blocks ----------------------------------------------------------
    if (InPlanner.IsActionRole)
    {
        Box->AddSlot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium))
            [
                SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("ACTION ROLE")))
                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
                                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted))
                        ]
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("Picked by parent Planner when its plan includes this Action.")))
                                .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
                                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Secondary))
                                .AutoWrapText(true)
                        ]
            ];
    }

    {
        const auto ChildText = FString::Printf(TEXT("%d child Action%s registered."),
            InPlanner.ChildActions.Num(),
            InPlanner.ChildActions.Num() == 1 ? TEXT("") : TEXT("s"));

        Box->AddSlot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium))
            [
                SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("PLANNER ROLE")))
                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
                                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted))
                        ]
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(ChildText))
                                .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
                                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Secondary))
                        ]
            ];
    }

    // Invalid goal keys ---------------------------------------------------------
    {
        auto Block = SNew(SVerticalBox)
            + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("INVALID GOAL KEYS")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
                        .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted))
                ];

        if (InPlanner.InvalidGoalAuthored.Num() == 0)
        {
            Block->AddSlot()
                .AutoHeight()
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("None ✓")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                        .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Status_PlanFound))
                ];
        }
        else
        {
            for (const auto& Cond : InPlanner.InvalidGoalAuthored)
            {
                Block->AddSlot()
                    .AutoHeight()
                    .Padding(FMargin(0.0f, 1.0f))
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(FString::Printf(TEXT("⚠ %s"),
                                *Cond.Get_Key().ToString())))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Status_Selected))
                    ];
            }
        }

        Box->AddSlot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium))
            [
                Block
            ];
    }

    // Dependency cycles ---------------------------------------------------------
    {
        auto Block = SNew(SVerticalBox)
            + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("DEPENDENCY CYCLES")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
                        .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted))
                ];

        if (InPlanner.DependencyCyclesDisplay.Num() == 0)
        {
            Block->AddSlot()
                .AutoHeight()
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("None ✓")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                        .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Status_PlanFound))
                ];
        }
        else
        {
            for (const auto& Cycle : InPlanner.DependencyCyclesDisplay)
            {
                const auto Joined = FString::Join(Cycle.ActionsInCycle, TEXT(" → "));
                Block->AddSlot()
                    .AutoHeight()
                    .Padding(FMargin(0.0f, 1.0f))
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(Joined))
                            .Font(FCoreStyle::GetDefaultFontStyle("Mono", 8))
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Status_Failed))
                    ];
            }
        }

        Box->AddSlot()
            .AutoHeight()
            [
                Block
            ];
    }

    return SNew(SBorder)
        .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Panel")))
        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Large, FCkGoapDebuggerStyle::Padding_Medium))
        [
            SNew(SScrollBox)
                .Orientation(Orient_Vertical)
                + SScrollBox::Slot()
                [
                    Box
                ]
        ];
}

// ====================================================================================================================
