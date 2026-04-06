#include "SGraphNode_EcsEntity.h"

#include "CkEcsDebugNode_Entity.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

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
    SGraphNode_EcsEntity::
    Construct(
        const FArguments& InArgs,
        UCkEcsDebugNode_Entity* InNode)
    -> void
{
    _EntityNode = InNode;
    GraphNode = InNode;

    SetCursor(EMouseCursor::CardinalCross);
    UpdateGraphNode();
}

// --------------------------------------------------------------------------------------------------------------------

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

auto
    SGraphNode_EcsEntity::
    UpdateGraphNode()
    -> void
{
    InputPins.Empty();
    OutputPins.Empty();
    RightNodeBox.Reset();
    LeftNodeBox.Reset();

    auto DisplayName = _EntityNode ? _EntityNode->Get_DisplayName() : TEXT("Unknown");
    auto AccentColor = _EntityNode ? _EntityNode->Get_AccentColor() : FLinearColor::White;
    auto bIsCenter = _EntityNode ? _EntityNode->Get_IsCenterNode() : false;
    auto AccentWidth = 4.0f;
    auto PinPadding = FMargin(8.0f, 4.0f);

    GetOrAddSlot(ENodeZone::Center)
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Center)
        [
            SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush(TEXT("Graph.StateNode.Body")))
                .Padding(0.0f)
                .BorderBackgroundColor(this, &SGraphNode_EcsEntity::GetBorderBackgroundColor)
                [
                    SNew(SOverlay)

                    // Pin area — fills entire node for connection geometry.
                    // HitTestInvisible so pins don't eat clicks — SGraphNode handles those.
                    + SOverlay::Slot()
                        .HAlign(HAlign_Fill)
                        .VAlign(VAlign_Fill)
                        [
                            SAssignNew(RightNodeBox, SVerticalBox)
                                .Visibility(EVisibility::HitTestInvisible)
                        ]

                    // Content area — SelfHitTestInvisible so clicks pass to SGraphNode
                    + SOverlay::Slot()
                        .HAlign(HAlign_Center)
                        .VAlign(VAlign_Center)
                        [
                            SNew(SBorder)
                                .BorderImage(FAppStyle::GetBrush(TEXT("Graph.StateNode.ColorSpill")))
                                .BorderBackgroundColor(
                                    bIsCenter
                                    ? FCkDebuggerStyle::Color_Graph_Node_Center
                                    : FCkDebuggerStyle::Color_Graph_Node_Default)
                                .Padding(PinPadding)
                                .Visibility(EVisibility::SelfHitTestInvisible)
                                [
                                    SNew(SHorizontalBox)

                                    // Accent strip
                                    + SHorizontalBox::Slot()
                                        .AutoWidth()
                                        .VAlign(VAlign_Fill)
                                        .Padding(0.0f, 0.0f, 6.0f, 0.0f)
                                        [
                                            SNew(SBox)
                                                .WidthOverride(AccentWidth)
                                                [
                                                    SNew(SImage)
                                                        .ColorAndOpacity(AccentColor)
                                                ]
                                        ]

                                    // Label
                                    + SHorizontalBox::Slot()
                                        .FillWidth(1.0f)
                                        .VAlign(VAlign_Center)
                                        [
                                            SNew(STextBlock)
                                                .Text(FText::FromString(DisplayName))
                                                .TextStyle(
                                                    bIsCenter
                                                    ? &FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Bold")
                                                    : &FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Normal"))
                                                .ColorAndOpacity(FCkDebuggerStyle::Color_Text_Highlight)
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
    SGraphNode_EcsEntity::
    CreatePinWidgets()
    -> void
{
    if (NOT _EntityNode)
    { return; }

    for (auto* Pin : _EntityNode->Pins)
    {
        if (NOT Pin->bHidden)
        {
            auto NewPin = SNew(SGraphPin, Pin);
            AddPin(NewPin);
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_EcsEntity::
    AddPin(
        const TSharedRef<SGraphPin>& PinToAdd)
    -> void
{
    PinToAdd->SetOwner(SharedThis(this));
    PinToAdd->SetRenderOpacity(0.0f);
    PinToAdd->SetVisibility(EVisibility::HitTestInvisible);

    if (RightNodeBox.IsValid())
    {
        RightNodeBox->AddSlot()
            .HAlign(HAlign_Fill)
            .VAlign(VAlign_Fill)
            .FillHeight(1.0f)
            [
                PinToAdd
            ];
    }

    if (PinToAdd->GetDirection() == EGPD_Input)
    {
        InputPins.Add(PinToAdd);
    }
    else
    {
        OutputPins.Add(PinToAdd);
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_EcsEntity::
    OnMouseButtonDoubleClick(
        const FGeometry& InMyGeometry,
        const FPointerEvent& InMouseEvent)
    -> FReply
{
    if (InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) && _EntityNode)
    {
        OnDoubleClicked.ExecuteIfBound(_EntityNode);
        return FReply::Handled();
    }
    return FReply::Unhandled();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_EcsEntity::
    GetBorderBackgroundColor() const
    -> FSlateColor
{
    if (NOT _EntityNode)
    { return FSlateColor(FCkDebuggerStyle::Color_Graph_Node_Border_Default); }

    if (_EntityNode->Get_IsCenterNode())
    { return FSlateColor(FCkDebuggerStyle::Color_Graph_Node_Border_Center); }

    switch (_EntityNode->Get_EdgeType())
    {
    case ECkEcsDebugEdgeType::LifetimeOwner:
        return FSlateColor(FCkDebuggerStyle::Color_Relationship);
    case ECkEcsDebugEdgeType::ContextOwner:
        return FSlateColor(FCkDebuggerStyle::Color_Reference);
    case ECkEcsDebugEdgeType::LifetimeDependent:
        return FSlateColor(FCkDebuggerStyle::Color_Transform);
    default:
        return FSlateColor(FCkDebuggerStyle::Color_Graph_Node_Border_Default);
    }
}

// --------------------------------------------------------------------------------------------------------------------
