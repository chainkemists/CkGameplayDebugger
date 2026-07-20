#include "CkGoapDebugger/Window/SCkGoapDebugger_SearchTracePanel.h"

#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_Chip.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "CkGoap/EntityScripts/CkGoapAction_EntityScript.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================

namespace ck_goap_debugger_search_trace
{
    auto LeafOfTag(const FGameplayTag& InTag) -> FString
    {
        auto Full = InTag.ToString();
        if (auto Idx = int32{INDEX_NONE}; Full.FindLastChar(TEXT('.'), Idx))
        { return Full.RightChop(Idx + 1); }
        return Full;
    }

    auto LeafOfClass(const TSubclassOf<UCk_GoapAction_EntityScript>& InClass) -> FString
    {
        if (InClass == nullptr) { return FString(TEXT("(goal seed)")); }
        return InClass->GetName();
    }
}

// ====================================================================================================================
// CONSTRUCT / REFRESH
// ====================================================================================================================

auto
    SCkGoapDebugger_SearchTracePanel::
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
    SCkGoapDebugger_SearchTracePanel::
    RefreshFromViewModel()
    -> void
{
    using namespace ck_goap_debugger_search_trace;

    if (NOT _ViewModel.IsValid() || NOT _Body.IsValid()) { return; }

    const auto* Planner = _ViewModel->GetSelectedPlannerInfo();

    auto NewHash = uint32{0};
    if (Planner != nullptr)
    {
        NewHash = HashCombine(NewHash, GetTypeHash(Planner->PlannerHandle));
        NewHash = HashCombine(NewHash, ::GetTypeHash(Planner->PlanAttemptCount));
        NewHash = HashCombine(NewHash, ::GetTypeHash(Planner->SearchDebug.Num()));
        NewHash = HashCombine(NewHash, ::GetTypeHash(Planner->SearchStats.Get_Iterations()));
    }

    if (NewHash == _LastHash) { return; }
    _LastHash = NewHash;

    _Body->ClearChildren();

    if (Planner == nullptr || Planner->SearchDebug.Num() == 0)
    {
        _Body->AddSlot()
            .AutoHeight()
            .Padding(FMargin(0.0f, CkStyle::SpaceXL))
            [
                SNew(STextBlock)
                    .Text(FText::FromString(Planner == nullptr
                        ? TEXT("Select a Planner to see its last search trace.")
                        : TEXT("No search trace yet — this Planner hasn't run a search this session.")))
                    .Font(CkStyle::RegularFont(CkStyle::FontSizeBody()))
                    .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                    .Justification(ETextJustify::Center)
            ];
        return;
    }

    // ---- Explainer + stats strip ----------------------------------------------
    _Body->AddSlot()
        .AutoHeight()
        .Padding(FMargin(0.0f, 0.0f, 0.0f, CkStyle::SpaceS))
        [
            SNew(SCkDebug_SelectableLabel)
                .Text(FText::FromString(TEXT(
                    "Regressive A* — the search walks BACKWARD from the goal. Each row is a constraint set still "
                    "unsatisfied at that state; an action trades it for the set its preconditions demand. Green rows "
                    "are already satisfied by the world state — the search terminates there.")))
                .Font(CkStyle::RegularFont(CkStyle::FontSizeSmall()))
                .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
        ];

    const auto& Stats = Planner->SearchStats;
    _Body->AddSlot()
        .AutoHeight()
        .Padding(FMargin(0.0f, 0.0f, 0.0f, CkStyle::SpaceM))
        [
            SNew(SCkDebug_SelectableLabel)
                .Text(FText::FromString(FString::Printf(
                    TEXT("iterations %d · state pool %d · elapsed %lld µs · plan length %d · cost %.1f · seeded from flattened WS snapshot"),
                    Stats.Get_Iterations(),
                    Stats.Get_StatePoolSize(),
                    Stats.Get_ElapsedMicroseconds(),
                    Stats.Get_PlanLength(),
                    Stats.Get_PlanCost())))
                .Font(CkStyle::MonoFont(CkStyle::FontSizeMicro()))
                .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
        ];

    // ---- Rows ------------------------------------------------------------------
    for (auto Index = 0; Index < Planner->SearchDebug.Num(); ++Index)
    {
        _Body->AddSlot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 0.0f, 0.0f, 2.0f))
            [
                DoBuildRow(Planner->SearchDebug[Index], Index)
            ];
    }

    _Body->AddSlot()
        .AutoHeight()
        .Padding(FMargin(0.0f, CkStyle::SpaceS, 0.0f, 0.0f))
        [
            SNew(STextBlock)
                .Text(FText::FromString(FString::Printf(
                    TEXT("%d explored constraint sets retained from the last search (pool holds every state the search touched)."),
                    Planner->SearchDebug.Num())))
                .Font(CkStyle::RegularFont(CkStyle::FontSizeMicro()))
                .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
        ];
}

// ====================================================================================================================
// BUILD — ROW
// ====================================================================================================================

auto
    SCkGoapDebugger_SearchTracePanel::
    DoBuildRow(
        const FCk_Goap_SearchDebugRow& InRow,
        int32 InIndex)
    -> TSharedRef<SWidget>
{
    using namespace ck_goap_debugger_search_trace;

    const auto Satisfied = InRow.Get_SatisfiedByWorldState();

    auto Chips = SNew(SWrapBox)
        .UseAllottedSize(true)
        .InnerSlotPadding(FVector2D(CkStyle::SpaceXS, CkStyle::SpaceXS));

    if (InRow.Get_Conditions().Num() == 0)
    {
        Chips->AddSlot()
        [
            SNew(SCkDebug_Chip)
                .Text(FText::FromString(TEXT("(empty set)")))
                .Kind(ECkDebug_ChipKind::Satisfied)
        ];
    }

    for (const auto& Cond : InRow.Get_Conditions())
    {
        auto Label = LeafOfTag(Cond.Get_Key());
        if (NOT Cond.Get_Value()) { Label += TEXT(" = false"); }

        Chips->AddSlot()
        [
            SNew(SCkDebug_Chip)
                .Text(FText::FromString(Label))
                .ToolTipText(FText::FromString(Cond.Get_Key().ToString()))
                .Kind(Satisfied ? ECkDebug_ChipKind::Satisfied : ECkDebug_ChipKind::Neutral)
        ];
    }

    return SNew(SBorder)
        .BorderImage(CkStyle::GetRoundedBrush())
        .BorderBackgroundColor(FSlateColor(Satisfied ? CkStyle::OkDim() : CkStyle::Bg2()))
        .Padding(FMargin(CkStyle::SpaceM, CkStyle::SpaceS))
        [
            SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(FMargin(0.0f, 0.0f, CkStyle::SpaceM, 0.0f))
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(FString::Printf(TEXT("%02d"), InIndex)))
                            .Font(CkStyle::MonoFont(CkStyle::FontSizeMicro()))
                            .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
                    ]

                + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    [
                        Chips
                    ]

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(FMargin(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f))
                    [
                        SNew(SCkDebug_SelectableLabel)
                            .Text(FText::FromString(FString::Printf(TEXT("via %s"), *LeafOfClass(InRow.Get_ViaActionClass()))))
                            .ToolTipText(FText::FromString(TEXT("The action whose preconditions introduced this constraint set (regressive step).")))
                            .Font(CkStyle::RegularFont(CkStyle::FontSizeMicro()))
                            .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                    ]

                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(FMargin(CkStyle::SpaceM, 0.0f, 0.0f, 0.0f))
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(FString::Printf(TEXT("h=%d"), InRow.Get_UnsatisfiedCount())))
                            .ToolTipText(FText::FromString(TEXT("Heuristic: unsatisfied-condition count at this state.")))
                            .Font(CkStyle::MonoFont(CkStyle::FontSizeMicro()))
                            .ColorAndOpacity(FSlateColor(Satisfied ? CkStyle::Ok() : CkStyle::TextMute()))
                    ]
        ];
}

// ====================================================================================================================
