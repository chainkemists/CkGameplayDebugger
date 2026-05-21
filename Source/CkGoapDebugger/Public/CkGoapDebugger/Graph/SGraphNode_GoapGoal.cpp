#include "SGraphNode_GoapGoal.h"

#include "CkGoapDebugNode_Goal.h"
#include "CkGoapDebugger/CkGoapDebuggerStyle.h"

#include "CkCore/Macros/CkMacros.h"

#include "SGraphPin.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"

// ====================================================================================================================

namespace
{
    auto Compute_TagLeaf_Goal(const FGameplayTag& InTag) -> FString
    {
        const auto Full = InTag.ToString();
        int32 Idx = INDEX_NONE;
        if (Full.FindLastChar(TEXT('.'), Idx) && Idx != INDEX_NONE)
        { return Full.Mid(Idx + 1); }
        return Full;
    }
}

// ====================================================================================================================

auto
    SGraphNode_GoapGoal::
    Construct(
        const FArguments& /*InArgs*/,
        UCkGoapDebugNode_Goal* InNode)
    -> void
{
    _GoalNode = InNode;
    GraphNode = InNode;

    SetCursor(EMouseCursor::CardinalCross);
    UpdateGraphNode();
}

// ====================================================================================================================

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

auto
    SGraphNode_GoapGoal::
    UpdateGraphNode()
    -> void
{
    InputPins.Empty();
    OutputPins.Empty();
    LeftNodeBox.Reset();
    RightNodeBox.Reset();

    auto Conditions = SNew(SVerticalBox);

    if (_GoalNode != nullptr)
    {
        for (const auto& Cond : _GoalNode->Get_GoalConditions())
        {
            const auto Line = FString::Printf(TEXT("%s = %s"),
                *Compute_TagLeaf_Goal(Cond.Key),
                Cond.Value ? TEXT("true") : TEXT("false"));

            Conditions->AddSlot()
                .AutoHeight()
                .Padding(0.0f, 1.0f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(Line))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                        .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Text_Secondary))
                        .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                ];
        }
    }

    auto Header = SNew(STextBlock)
        .Text(FText::FromString(TEXT("\x2605 Goal")))   // ★ Goal
        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
        .ColorAndOpacity(FSlateColor(FCkGoapDebuggerStyle::Color_Status_Selected));

    auto Card = SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
        .BorderBackgroundColor(FCkGoapDebuggerStyle::Color_Status_Selected)
        .Padding(FMargin(2.0f))
        [
            SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                .BorderBackgroundColor(FCkGoapDebuggerStyle::Color_Bg_Surface)
                .Padding(FMargin(FCkGoapDebuggerStyle::Padding_Medium, FCkGoapDebuggerStyle::Padding_Small))
                [
                    SNew(SVerticalBox)
                        + SVerticalBox::Slot()
                            .AutoHeight()
                            [ Header ]
                        + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, FCkGoapDebuggerStyle::Padding_Small, 0.0f, 0.0f)
                            [ Conditions ]
                ]
        ];

    GetOrAddSlot(ENodeZone::Center)
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Center)
        [
            SNew(SBox)
                .WidthOverride(FCkGoapDebuggerStyle::GraphNode_Width)
                [
                    SNew(SOverlay)

                        + SOverlay::Slot()
                            .HAlign(HAlign_Fill)
                            .VAlign(VAlign_Fill)
                            [
                                SAssignNew(LeftNodeBox, SVerticalBox)
                                    .Visibility(EVisibility::HitTestInvisible)
                            ]

                        + SOverlay::Slot()
                            .HAlign(HAlign_Fill)
                            .VAlign(VAlign_Fill)
                            [
                                Card
                            ]
                ]
        ];

    CreatePinWidgets();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

// ====================================================================================================================

auto
    SGraphNode_GoapGoal::
    CreatePinWidgets()
    -> void
{
    if (NOT GraphNode) { return; }

    for (auto* Pin : GraphNode->Pins)
    {
        if (Pin && Pin->Direction == EGPD_Input && NOT Pin->bHidden)
        {
            auto PinWidget = SNew(SGraphPin, Pin);
            PinWidget->SetIsEditable(false);
            PinWidget->SetVisibility(EVisibility::HitTestInvisible);
            AddPin(PinWidget);
            break;
        }
    }
}

// ====================================================================================================================

auto
    SGraphNode_GoapGoal::
    AddPin(
        const TSharedRef<SGraphPin>& InPinToAdd)
    -> void
{
    InPinToAdd->SetOwner(SharedThis(this));

    LeftNodeBox->AddSlot()
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Fill)
        .FillHeight(1.0f)
        [
            InPinToAdd
        ];

    InputPins.Add(InPinToAdd);
}

// ====================================================================================================================
