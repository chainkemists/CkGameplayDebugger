#include "SGraphNode_SmState.h"

#include "CkSmDebugNode_State.h"
#include "CkSmDebugger/CkSmDebuggerStyle.h"

#include "CkCore/Macros/CkMacros.h"

#include "SGraphPin.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SOverlay.h"
#include "Styling/AppStyle.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_SmState::
    Construct(
        const FArguments& InArgs,
        UCkSmDebugNode_State* InNode)
    -> void
{
    _StateNode = InNode;
    GraphNode = InNode;

    SetCursor(EMouseCursor::CardinalCross);
    UpdateGraphNode();
}

// --------------------------------------------------------------------------------------------------------------------

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

auto
    SGraphNode_SmState::
    UpdateGraphNode()
    -> void
{
    InputPins.Empty();
    OutputPins.Empty();
    RightNodeBox.Reset();
    LeftNodeBox.Reset();

    auto StateName = _StateNode ? _StateNode->Get_StateName() : TEXT("Unknown");
    auto StateColor = _StateNode ? _StateNode->Get_StateColor() : FLinearColor::White;
    auto PinPadding = FCkSmDebuggerStyle::Sm_PinPadding;

    GetOrAddSlot(ENodeZone::Center)
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Center)
        [
            SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush(TEXT("Graph.StateNode.Body")))
                .Padding(0.0f)
                .BorderBackgroundColor(this, &SGraphNode_SmState::GetBorderBackgroundColor)
                [
                    SNew(SOverlay)

                    // --------------------------------------------------------
                    // Slot 0: Pin area — fills entire node for connection
                    // geometry. The output pin widget lives here so
                    // FConnectionDrawingPolicy can find it.
                    // --------------------------------------------------------
                    + SOverlay::Slot()
                        .HAlign(HAlign_Fill)
                        .VAlign(VAlign_Fill)
                        [
                            SAssignNew(RightNodeBox, SVerticalBox)
                        ]

                    // --------------------------------------------------------
                    // Slot 1: Content area — icon + state name.
                    // SelfHitTestInvisible so clicks pass through to pin.
                    // --------------------------------------------------------
                    + SOverlay::Slot()
                        .HAlign(HAlign_Center)
                        .VAlign(VAlign_Center)
                        [
                            SNew(SBorder)
                                .BorderImage(FAppStyle::GetBrush(TEXT("Graph.StateNode.ColorSpill")))
                                .BorderBackgroundColor(FCkSmDebuggerStyle::Color_Sm_TitleShadow)
                                .Padding(PinPadding)
                                .Visibility(EVisibility::SelfHitTestInvisible)
                                [
                                    SNew(SHorizontalBox)

                                    // Colored icon square
                                    + SHorizontalBox::Slot()
                                        .AutoWidth()
                                        .VAlign(VAlign_Center)
                                        .Padding(0.0f, 0.0f, 6.0f, 0.0f)
                                        [
                                            SNew(SBox)
                                                .WidthOverride(8.0f)
                                                .HeightOverride(8.0f)
                                                [
                                                    SNew(SBorder)
                                                        .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                                                        .BorderBackgroundColor(StateColor)
                                                ]
                                        ]

                                    // State name
                                    + SHorizontalBox::Slot()
                                        .AutoWidth()
                                        .VAlign(VAlign_Center)
                                        [
                                            SNew(STextBlock)
                                                .Text(FText::FromString(StateName))
                                                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                                                .ColorAndOpacity(FCkSmDebuggerStyle::Color_Sm_TextPrimary)
                                        ]
                                ]
                        ]
                ]
        ];

    CreatePinWidgets();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_SmState::
    CreatePinWidgets()
    -> void
{
    // Only create a widget for the output pin — it fills the entire node via the
    // overlay. The connection policy resolves wire geometry to the node boundary
    // (not pin position), so the input pin needs no widget.
    if (NOT GraphNode) { return; }

    for (auto* Pin : GraphNode->Pins)
    {
        if (Pin && Pin->Direction == EGPD_Output && NOT Pin->bHidden)
        {
            auto PinWidget = SNew(SGraphPin, Pin);
            PinWidget->SetIsEditable(false);
            AddPin(PinWidget);
            break;
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_SmState::
    AddPin(
        const TSharedRef<SGraphPin>& PinToAdd)
    -> void
{
    PinToAdd->SetOwner(SharedThis(this));

    RightNodeBox->AddSlot()
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Fill)
        .FillHeight(1.0f)
        [
            PinToAdd
        ];

    OutputPins.Add(PinToAdd);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_SmState::
    GetBorderBackgroundColor() const
    -> FSlateColor
{
    if (NOT _StateNode)
    { return FLinearColor::Transparent; }

    if (_StateNode->Get_IsCurrentState())
    { return FCkSmDebuggerStyle::Color_Sm_ActiveStateBody; }

    return FCkSmDebuggerStyle::Color_Sm_InactiveStateBody;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_SmState::
    GetNodeInfoPopups(
        FNodeInfoContext* InContext,
        TArray<FGraphInformationPopupInfo>& OutPopups) const
    -> void
{
    if (NOT _StateNode)
    { return; }

    if (_StateNode->Get_IsCurrentState())
    {
        auto DwellStr = FString::Printf(
            TEXT("100.0%%\nActive for %.2f secs"),
            _StateNode->Get_DwellTimeSeconds());

        OutPopups.Emplace(
            nullptr,
            FCkSmDebuggerStyle::Color_Sm_ActiveStateBorder,
            DwellStr);
    }
}

// --------------------------------------------------------------------------------------------------------------------
