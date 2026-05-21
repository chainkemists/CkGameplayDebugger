#include "CkGoapDebugger/Window/SCkGoapDebugger_PrimaryPane.h"

#include "CkGoapDebugger/CkGoapDebuggerStyle.h"
#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"
#include "CkGoapDebugger/Window/SCkGoapDebugger_PlanStrip.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

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
// Internal helpers
// ====================================================================================================================

namespace ck_goap_debugger_primary_internal
{
    auto RoleLabel(ECkGoapDebugger_ActionRole InRole) -> FString
    {
        switch (InRole)
        {
        case ECkGoapDebugger_ActionRole::Root:    return TEXT("root action");
        case ECkGoapDebugger_ActionRole::Mid:     return TEXT("mid action");
        case ECkGoapDebugger_ActionRole::Leaf:    return TEXT("leaf action");
        case ECkGoapDebugger_ActionRole::Catalog: return TEXT("catalog action");
        default:                                  return TEXT("");
        }
    }

    struct FStatusBadge
    {
        FString      Label;
        FLinearColor Color;
        FName        BrushName;
    };

    auto ResolveStatusBadge(ECk_GoapPlanStatus InStatus) -> FStatusBadge
    {
        switch (InStatus)
        {
        case ECk_GoapPlanStatus::PlanFound:
            return {TEXT("PlanFound"), FCkGoapDebuggerStyle::Color_Status_PlanFound, FName(TEXT("CkGoap.Badge.Found"))};
        case ECk_GoapPlanStatus::Planning:
            return {TEXT("Planning"),  FCkGoapDebuggerStyle::Color_Status_Planning,  FName(TEXT("CkGoap.Badge.Planning"))};
        case ECk_GoapPlanStatus::PlanFailed:
            return {TEXT("PlanFailed"), FCkGoapDebuggerStyle::Color_Status_Failed,   FName(TEXT("CkGoap.Badge.Failed"))};
        case ECk_GoapPlanStatus::CostThresholdReached:
            return {TEXT("CostThresh"), FCkGoapDebuggerStyle::Color_Status_Selected, FName(TEXT("CkGoap.Badge.Failed"))};
        case ECk_GoapPlanStatus::Idle:
        default:
            return {TEXT("Idle"), FCkGoapDebuggerStyle::Color_Text_Muted, FName(TEXT("CkGoap.Badge.Idle"))};
        }
    }

    // Find the resolved WS value for a given tag in the ActionSet's snapshot.
    // Returns TOptional empty when the tag isn't in the WS.
    auto LookupWsValue(
        const FCkGoapDebugger_ActionSetInfo& InAs,
        const FGameplayTag& InKey) -> TOptional<bool>
    {
        for (const auto& Entry : InAs.WorldState)
        {
            if (Entry.Key == InKey)
            { return TOptional<bool>(Entry.Value); }
        }
        return TOptional<bool>{};
    }

    // Section header — uppercase muted label with letter-spacing-ish margin.
    auto MakeSectionHeader(const FString& InText) -> TSharedRef<SWidget>
    {
        return SNew(SBox)
            .Padding(FMargin(0.0f, FCkGoapDebuggerStyle::Padding_Small, 0.0f, FCkGoapDebuggerStyle::Padding_XSmall))
            [
                SNew(STextBlock)
                    .Text(FText::FromString(InText.ToUpper()))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
                    .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted))
            ];
    }

    // A small text badge with a brush background.
    auto MakeBadge(const FString& InText, const FLinearColor& InColor, const FName& InBrushName) -> TSharedRef<SWidget>
    {
        return SNew(SBorder)
            .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(InBrushName))
            .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_XSmall))
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(InText))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                    .ColorAndOpacity(FSlateColor(InColor))
            ];
    }

    // Condition row: ✓/✗/⚠ + tag-text + value.
    auto MakeConditionRow(
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
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                        .ColorAndOpacity(FSlateColor(InColor))
                ]
            + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(InText))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                        .ColorAndOpacity(FSlateColor(InColor))
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

                    // ---- Header bar -------------------------------------------------
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

                    // ---- Body: left scrollable column + right rail -----------------
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

    using namespace ck_goap_debugger_primary_internal;

    const auto* AsInfo = _ViewModel->GetSelectedActionSetInfo();
    const auto* SelAction = _ViewModel->GetSelectedActionInfo();

    // Decide which Action to use for the header. Prefer the explicitly-selected
    // Action; fall back to the ActionSet's root.
    const auto* HeaderAction = SelAction;
    if (HeaderAction == nullptr && AsInfo != nullptr)
    {
        HeaderAction = AsInfo->Catalog.FindByPredicate(
            [&](const FCkGoapDebugger_ActionInfo& In) { return In.Handle == AsInfo->RootActionHandle; });
    }

    // ---- Empty state ----------------------------------------------------------
    if (AsInfo == nullptr || HeaderAction == nullptr)
    {
        if (_HeaderHost.IsValid())    { _HeaderHost->SetContent(BuildEmptyState()); }
        if (_LeftBody.IsValid())      { _LeftBody->ClearChildren(); }
        if (_RightRailHost.IsValid()) { _RightRailHost->SetContent(SNew(SSpacer)); }
        return;
    }

    // ---- Header ---------------------------------------------------------------
    if (_HeaderHost.IsValid())
    { _HeaderHost->SetContent(BuildHeader(*HeaderAction)); }

    // ---- Left body ------------------------------------------------------------
    if (_LeftBody.IsValid())
    {
        _LeftBody->ClearChildren();

        // Plan strip section
        _LeftBody->AddSlot()
            .AutoHeight()
            [
                MakeSectionHeader(TEXT("Plan"))
            ];

        _LeftBody->AddSlot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium))
            [
                SAssignNew(_PlanStrip, SCkGoapDebugger_PlanStrip)
                    .ViewModel(_ViewModel)
            ];

        // Goal section
        _LeftBody->AddSlot()
            .AutoHeight()
            [
                BuildGoalSection(*AsInfo, *HeaderAction)
            ];

        // Drilldown — visible only if SelectedAction is set AND it differs from
        // the header action (i.e. user clicked a different node in the plan strip).
        if (SelAction != nullptr && (HeaderAction != SelAction))
        {
            _LeftBody->AddSlot()
                .AutoHeight()
                .Padding(FMargin(0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f, 0.0f))
                [
                    BuildDrilldown(*AsInfo, *SelAction)
                ];
        }
    }

    // ---- Right rail -----------------------------------------------------------
    if (_RightRailHost.IsValid())
    {
        const auto IsFailure = HeaderAction->PlanStatus == ECk_GoapPlanStatus::PlanFailed
                            || HeaderAction->PlanStatus == ECk_GoapPlanStatus::CostThresholdReached;

        _RightRailHost->SetContent(IsFailure
            ? BuildRightRail_Failure(*AsInfo, *HeaderAction)
            : BuildRightRail_Normal(*AsInfo, *HeaderAction));
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
                .Text(FText::FromString(TEXT("Select an entity and an ActionSet to inspect a plan.")))
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
        const FCkGoapDebugger_ActionInfo& InAction)
    -> TSharedRef<SWidget>
{
    using namespace ck_goap_debugger_primary_internal;

    const auto NameColor = (InAction.Role == ECkGoapDebugger_ActionRole::Root)
        ? FCkGoapDebuggerStyle::Color_Status_Selected
        : FCkGoapDebuggerStyle::Color_Text_Primary;

    const auto Badge = ResolveStatusBadge(InAction.PlanStatus);

    return SNew(SBorder)
        .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Bg.Panel")))
        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Large, FCkGoapDebuggerStyle::Padding_Medium))
        [
            SNew(SHorizontalBox)

                // Action name (prominent)
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f)
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(InAction.ClassName.IsEmpty()
                                ? TEXT("(unknown action)") : InAction.ClassName))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
                            .ColorAndOpacity(FSlateColor(NameColor))
                    ]

                // Role label
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f)
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(FString(TEXT("· ")) + RoleLabel(InAction.Role)))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted))
                    ]

                // Tag
                + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(InAction.ActionTag.IsValid()
                                ? InAction.ActionTag.ToString() : FString{}))
                            .Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim))
                    ]

                // Status badge
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(FCkGoapDebuggerStyle::Padding_Medium, 0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f)
                    [
                        MakeBadge(Badge.Label, Badge.Color, Badge.BrushName)
                    ]

                // Cost
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f)
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(FString::Printf(TEXT("Cost: $%d"),
                                FMath::RoundToInt(InAction.PlanCost))))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Status_Selected))
                    ]

                // Attempts
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f)
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(FString::Printf(TEXT("Attempts: %d"),
                                InAction.PlanAttemptCount)))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Secondary))
                    ]

                // Last replan
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(FString::Printf(TEXT("Last replan: %.1fs ago"),
                                InAction.SecondsSinceLastReplan)))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted))
                    ]
        ];
}

// ====================================================================================================================
// BUILD — GOAL SECTION
// ====================================================================================================================

auto
    SCkGoapDebugger_PrimaryPane::
    BuildGoalSection(
        const FCkGoapDebugger_ActionSetInfo& InAs,
        const FCkGoapDebugger_ActionInfo& InAction)
    -> TSharedRef<SWidget>
{
    using namespace ck_goap_debugger_primary_internal;

    auto HeaderLabel = FString(TEXT("Goal"));
    if (InAction.Role == ECkGoapDebugger_ActionRole::Root)
    {
        HeaderLabel = TEXT("Goal (initial — root action)");
    }
    else if (NOT InAction.ParentClassName.IsEmpty())
    {
        HeaderLabel = FString::Printf(TEXT("Goal (injected from %s)"), *InAction.ParentClassName);
    }

    auto Box = SNew(SVerticalBox);

    Box->AddSlot()
        .AutoHeight()
        [
            MakeSectionHeader(HeaderLabel)
        ];

    if (InAction.Goal.Num() == 0 && InAction.InvalidGoal.Num() == 0)
    {
        Box->AddSlot()
            .AutoHeight()
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("(no goal conditions)")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
                    .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim))
            ];
        return Box;
    }

    for (const auto& Cond : InAction.Goal)
    {
        const auto WsOpt = LookupWsValue(InAs, Cond.Key);
        const auto Sat   = WsOpt.IsSet() && (WsOpt.GetValue() == Cond.Value);

        const auto Glyph = Sat ? FString(TEXT("✓")) : FString(TEXT("✗"));
        const auto Color = Sat ? FCkGoapDebuggerStyle::Color_Status_PlanFound
                               : FCkGoapDebuggerStyle::Color_Status_Failed;
        const auto Text  = FString::Printf(TEXT("%s = %s"),
            *Cond.Key.ToString(),
            Cond.Value ? TEXT("true") : TEXT("false"));

        Box->AddSlot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 1.0f))
            [
                MakeConditionRow(Glyph, Text, Color)
            ];
    }

    for (const auto& Cond : InAction.InvalidGoal)
    {
        const auto Text = FString::Printf(TEXT("%s = %s  (unregistered)"),
            *Cond.Key.ToString(),
            Cond.Value ? TEXT("true") : TEXT("false"));

        Box->AddSlot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 1.0f))
            [
                MakeConditionRow(TEXT("⚠"), Text, FCkGoapDebuggerStyle::Color_Status_Selected)
            ];
    }

    return Box;
}

// ====================================================================================================================
// BUILD — DRILLDOWN
// ====================================================================================================================

auto
    SCkGoapDebugger_PrimaryPane::
    BuildDrilldown(
        const FCkGoapDebugger_ActionSetInfo& InAs,
        const FCkGoapDebugger_ActionInfo& InSelected)
    -> TSharedRef<SWidget>
{
    using namespace ck_goap_debugger_primary_internal;

    // Preconditions column
    auto PreColumn = SNew(SVerticalBox)
        + SVerticalBox::Slot()
            .AutoHeight()
            [
                MakeSectionHeader(TEXT("Preconditions"))
            ];

    if (InSelected.Preconditions.Num() == 0)
    {
        PreColumn->AddSlot()
            .AutoHeight()
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("(none)")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
                    .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim))
            ];
    }
    else
    {
        for (const auto& Cond : InSelected.Preconditions)
        {
            const auto WsOpt = LookupWsValue(InAs, Cond.Key);
            const auto Sat   = WsOpt.IsSet() && (WsOpt.GetValue() == Cond.Value);

            const auto Glyph = Sat ? FString(TEXT("✓")) : FString(TEXT("✗"));
            const auto Color = Sat ? FCkGoapDebuggerStyle::Color_Status_PlanFound
                                   : FCkGoapDebuggerStyle::Color_Status_Failed;
            const auto Text = FString::Printf(TEXT("%s = %s"),
                *Cond.Key.ToString(),
                Cond.Value ? TEXT("true") : TEXT("false"));

            PreColumn->AddSlot()
                .AutoHeight()
                .Padding(FMargin(0.0f, 1.0f))
                [
                    MakeConditionRow(Glyph, Text, Color)
                ];
        }
    }

    // Effects column
    auto EffColumn = SNew(SVerticalBox)
        + SVerticalBox::Slot()
            .AutoHeight()
            [
                MakeSectionHeader(TEXT("Effects"))
            ];

    if (InSelected.Effects.Num() == 0)
    {
        EffColumn->AddSlot()
            .AutoHeight()
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("(none)")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
                    .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim))
            ];
    }
    else
    {
        for (const auto& Eff : InSelected.Effects)
        {
            const auto Text = FString::Printf(TEXT("→ %s := %s"),
                *Eff.Key.ToString(),
                Eff.Value ? TEXT("true") : TEXT("false"));

            EffColumn->AddSlot()
                .AutoHeight()
                .Padding(FMargin(0.0f, 1.0f))
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(Text))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                        .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Status_Planning))
                ];
        }
    }

    // Cost column
    auto CostColumn = SNew(SVerticalBox)
        + SVerticalBox::Slot()
            .AutoHeight()
            [
                MakeSectionHeader(TEXT("Cost"))
            ]
        + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(STextBlock)
                    .Text(FText::FromString(FString::Printf(TEXT("$%d"), FMath::RoundToInt(InSelected.Cost))))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
                    .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Status_Selected))
            ];

    // Contributing-to-goals — simple: list of catalog actions whose Goal
    // overlaps with this action's Effects keys.
    auto Contributes = TArray<FString>{};
    for (const auto& Other : InAs.Catalog)
    {
        if (Other.Handle == InSelected.Handle) { continue; }
        for (const auto& EffKey : InSelected.Effects)
        {
            const auto Hits = Other.Goal.ContainsByPredicate(
                [&](const FCkGoapDebugger_Condition& In) { return In.Key == EffKey.Key; });
            if (Hits) { Contributes.AddUnique(Other.ClassName); break; }
        }
    }

    // Alternative actions — other catalog actions sharing at least one effect key.
    auto Alternatives = TArray<FString>{};
    for (const auto& Other : InAs.Catalog)
    {
        if (Other.Handle == InSelected.Handle) { continue; }
        for (const auto& EffA : InSelected.Effects)
        {
            const auto Match = Other.Effects.ContainsByPredicate(
                [&](const FCkGoapDebugger_Condition& In) { return In.Key == EffA.Key && In.Value == EffA.Value; });
            if (Match) { Alternatives.AddUnique(Other.ClassName); break; }
        }
    }

    auto MakeStringListSection = [](const FString& Title, const TArray<FString>& Items) -> TSharedRef<SWidget>
    {
        auto Box = SNew(SVerticalBox)
            + SVerticalBox::Slot()
                .AutoHeight()
                [
                    MakeSectionHeader(Title)
                ];

        if (Items.Num() == 0)
        {
            Box->AddSlot()
                .AutoHeight()
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("(none)")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
                        .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim))
                ];
        }
        else
        {
            for (const auto& Item : Items)
            {
                Box->AddSlot()
                    .AutoHeight()
                    .Padding(FMargin(0.0f, 1.0f))
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(Item))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Secondary))
                    ];
            }
        }
        return Box;
    };

    return SNew(SBorder)
        .BorderImage(FCkGoapDebuggerStyle::Get().GetBrush(TEXT("CkGoap.Surface.Rounded")))
        .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium))
        [
            SNew(SVerticalBox)

                // Title
                + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(FMargin(0.0f, 0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Small))
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(FString::Printf(TEXT("Selected action: %s"), *InSelected.ClassName)))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Status_Selected))
                    ]

                // Three columns: Pre · Effects · Cost
                + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                                .FillWidth(1.0f)
                                .Padding(FMargin(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f))
                                [
                                    PreColumn
                                ]
                            + SHorizontalBox::Slot()
                                .FillWidth(1.0f)
                                .Padding(FMargin(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f))
                                [
                                    EffColumn
                                ]
                            + SHorizontalBox::Slot()
                                .AutoWidth()
                                [
                                    CostColumn
                                ]
                    ]

                // Mini-sections row: Contributing · Alternatives
                + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(FMargin(0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f, 0.0f))
                    [
                        SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                                .FillWidth(1.0f)
                                .Padding(FMargin(0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f))
                                [
                                    MakeStringListSection(TEXT("Contributing to goals"), Contributes)
                                ]
                            + SHorizontalBox::Slot()
                                .FillWidth(1.0f)
                                [
                                    MakeStringListSection(TEXT("Alternative actions"), Alternatives)
                                ]
                    ]

                // Plan frequency placeholder
                + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(FMargin(0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f, 0.0f))
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(FString::Printf(TEXT("Plan frequency: %d attempts"),
                                InSelected.PlanAttemptCount)))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim))
                    ]
        ];
}

// ====================================================================================================================
// BUILD — RIGHT RAIL (NORMAL)
// ====================================================================================================================

auto
    SCkGoapDebugger_PrimaryPane::
    BuildRightRail_Normal(
        const FCkGoapDebugger_ActionSetInfo& InAs,
        const FCkGoapDebugger_ActionInfo& InAction)
    -> TSharedRef<SWidget>
{
    using namespace ck_goap_debugger_primary_internal;

    auto Box = SNew(SVerticalBox);

    // WS source -------------------------------------------------------------
    Box->AddSlot()
        .AutoHeight()
        [
            MakeSectionHeader(TEXT("WS source"))
        ];

    Box->AddSlot()
        .AutoHeight()
        .Padding(FMargin(0.0f, 1.0f))
        [
            SNew(STextBlock)
                .Text(FText::FromString(FString::Printf(TEXT("Resolved: %s"),
                    InAction.WorldStateSourceLabel.IsEmpty()
                        ? TEXT("(inherited)") : *InAction.WorldStateSourceLabel)))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Status_Selected))
        ];

    Box->AddSlot()
        .AutoHeight()
        .Padding(FMargin(0.0f, 1.0f))
        [
            SNew(STextBlock)
                .Text(FText::FromString(InAction.Role == ECkGoapDebugger_ActionRole::Root
                    ? FString(TEXT("Override: root — explicit WS"))
                    : FString(TEXT("Override: inherits parent"))))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Muted))
        ];

    // Parent action ---------------------------------------------------------
    Box->AddSlot()
        .AutoHeight()
        .Padding(FMargin(0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f, 0.0f))
        [
            MakeSectionHeader(TEXT("Parent action"))
        ];

    if (InAction.ParentClassName.IsEmpty())
    {
        Box->AddSlot()
            .AutoHeight()
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("(none — this is the root)")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
                    .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim))
            ];
    }
    else
    {
        const auto WeakVM = TWeakPtr<FCkGoapDebugger_ViewModel>(_ViewModel);
        const auto ParentHandle = InAction.ParentActionHandle;

        Box->AddSlot()
            .AutoHeight()
            [
                SNew(SButton)
                    .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                    .ContentPadding(FMargin(0.0f, 1.0f))
                    .ToolTipText(FText::FromString(FString::Printf(
                        TEXT("Click to select parent action %s"), *InAction.ParentClassName)))
                    .OnClicked_Lambda([WeakVM, ParentHandle]() -> FReply
                    {
                        if (const auto VM = WeakVM.Pin())
                        {
                            if (ck::IsValid(ParentHandle))
                            { VM->SetSelectedAction(ParentHandle); }
                        }
                        return FReply::Handled();
                    })
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(InAction.ParentClassName))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                            .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Status_Planning))
                    ]
            ];

        Box->AddSlot()
            .AutoHeight()
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("(WS inherited unless overridden)")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Italic", 8))
                    .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim))
            ];
    }

    // Invalid goal keys -----------------------------------------------------
    Box->AddSlot()
        .AutoHeight()
        .Padding(FMargin(0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f, 0.0f))
        [
            MakeSectionHeader(TEXT("Invalid goal keys"))
        ];

    if (InAction.InvalidGoal.Num() == 0)
    {
        Box->AddSlot()
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
        for (const auto& Cond : InAction.InvalidGoal)
        {
            Box->AddSlot()
                .AutoHeight()
                .Padding(FMargin(0.0f, 1.0f))
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(FString::Printf(TEXT("⚠ %s"), *Cond.Key.ToString())))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                        .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Status_Selected))
                ];
        }
    }

    // Dependency cycles -----------------------------------------------------
    Box->AddSlot()
        .AutoHeight()
        .Padding(FMargin(0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f, 0.0f))
        [
            MakeSectionHeader(TEXT("Dependency cycles"))
        ];

    if (InAs.DependencyCycles.Num() == 0)
    {
        Box->AddSlot()
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
        for (const auto& Cycle : InAs.DependencyCycles)
        {
            const auto Joined = FString::Join(Cycle.ActionsInCycle, TEXT(" → "));
            Box->AddSlot()
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
// BUILD — RIGHT RAIL (FAILURE VARIANT)
// ====================================================================================================================

auto
    SCkGoapDebugger_PrimaryPane::
    BuildRightRail_Failure(
        const FCkGoapDebugger_ActionSetInfo& InAs,
        const FCkGoapDebugger_ActionInfo& InAction)
    -> TSharedRef<SWidget>
{
    using namespace ck_goap_debugger_primary_internal;

    auto Box = SNew(SVerticalBox);

    // Header (red) ---------------------------------------------------------
    Box->AddSlot()
        .AutoHeight()
        .Padding(FMargin(0.0f, 0.0f, 0.0f, FCkGoapDebuggerStyle::Padding_Small))
        [
            SNew(STextBlock)
                .Text(FText::FromString(TEXT("⚠ Failure analysis")))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Status_Failed))
        ];

    // Unreachable goal keys -----------------------------------------------
    Box->AddSlot()
        .AutoHeight()
        [
            MakeSectionHeader(TEXT("Unreachable goal keys"))
        ];

    // Collect: explicit invalid goals + unsatisfied conditions.
    auto Unreached = TArray<FString>{};
    for (const auto& Cond : InAction.InvalidGoal)
    { Unreached.AddUnique(Cond.Key.ToString()); }
    for (const auto& Cond : InAction.Goal)
    {
        const auto WsOpt = LookupWsValue(InAs, Cond.Key);
        if (NOT WsOpt.IsSet() || WsOpt.GetValue() != Cond.Value)
        { Unreached.AddUnique(Cond.Key.ToString()); }
    }

    if (Unreached.Num() == 0)
    {
        Box->AddSlot()
            .AutoHeight()
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("(none surfaced)")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
                    .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim))
            ];
    }
    else
    {
        for (const auto& Key : Unreached)
        {
            Box->AddSlot()
                .AutoHeight()
                .Padding(FMargin(0.0f, 1.0f))
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(FString(TEXT("• ")) + Key))
                        .Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
                        .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Status_Failed))
                ];
        }
    }

    // Plan tree (why not?) — placeholder ----------------------------------
    Box->AddSlot()
        .AutoHeight()
        .Padding(FMargin(0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f, 0.0f))
        [
            MakeSectionHeader(TEXT("Plan tree (why not?)"))
        ];

    Box->AddSlot()
        .AutoHeight()
        [
            SNew(STextBlock)
                .Text(FText::FromString(TEXT("(plan-tree introspection — D6 follow-up)")))
                .Font(FCoreStyle::GetDefaultFontStyle("Italic", 9))
                .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Dim))
        ];

    // Suggested fixes ------------------------------------------------------
    Box->AddSlot()
        .AutoHeight()
        .Padding(FMargin(0.0f, FCkGoapDebuggerStyle::Padding_Medium, 0.0f, 0.0f))
        [
            MakeSectionHeader(TEXT("Suggested fixes"))
        ];

    if (Unreached.Num() == 0)
    {
        Box->AddSlot()
            .AutoHeight()
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Force replan when WS satisfies goal.")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                    .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Secondary))
            ];
    }
    else
    {
        for (const auto& Key : Unreached)
        {
            Box->AddSlot()
                .AutoHeight()
                .Padding(FMargin(0.0f, 1.0f))
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(FString::Printf(
                            TEXT("• Verify %s reaches the action's WS."), *Key)))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                        .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Secondary))
                ];
        }
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
