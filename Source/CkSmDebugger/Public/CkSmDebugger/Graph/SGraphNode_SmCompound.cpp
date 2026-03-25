#include "SGraphNode_SmCompound.h"

#include "CkSmDebugNode_Compound.h"
#include "CkSmDebugGraph.h"
#include "CkSmDebugger/CkSmDebuggerStyle.h"

#include "CkCore/Macros/CkMacros.h"

#include "SGraphPin.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SOverlay.h"
#include "Rendering/DrawElements.h"
#include "Brushes/SlateRoundedBoxBrush.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_SmCompound::
    Construct(
        const FArguments& InArgs,
        UCkSmDebugNode_Compound* InNode)
    -> void
{
    _CompoundNode = InNode;
    GraphNode = InNode;

    SetCursor(EMouseCursor::CardinalCross);
    UpdateGraphNode();
}

// --------------------------------------------------------------------------------------------------------------------

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

auto
    SGraphNode_SmCompound::
    UpdateGraphNode()
    -> void
{
    InputPins.Empty();
    OutputPins.Empty();
    RightNodeBox.Reset();
    LeftNodeBox.Reset();

    auto CompoundWidth = _CompoundNode ? _CompoundNode->Get_CompoundWidth() : 200.0f;
    auto CompoundHeight = _CompoundNode ? _CompoundNode->Get_CompoundHeight() : 150.0f;

    auto Label = _CompoundNode ? _CompoundNode->Get_CompoundLabel() : TEXT("Sub-SM");
    auto NameDepth = 1;
    if (_CompoundNode)
    {
        if (auto* Graph = Cast<UCkSmDebugGraph>(_CompoundNode->GetGraph()))
        { NameDepth = Graph->LayoutParams.NameDepth; }
    }
    auto DisplayLabel = FCkSmLayoutParams::ComputeDisplayName(Label, NameDepth);

    GetOrAddSlot(ENodeZone::Center)
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Fill)
        [
            SNew(SBox)
                .WidthOverride(CompoundWidth)
                .HeightOverride(CompoundHeight)
                [
                    SNew(SOverlay)

                    // Hidden pin area for connection geometry
                    + SOverlay::Slot()
                        .HAlign(HAlign_Fill)
                        .VAlign(VAlign_Fill)
                        [
                            SAssignNew(RightNodeBox, SVerticalBox)
                        ]

                    // Label at top-left
                    + SOverlay::Slot()
                        .HAlign(HAlign_Left)
                        .VAlign(VAlign_Top)
                        .Padding(8.0f, 4.0f)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(DisplayLabel))
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                                .ColorAndOpacity_Lambda([CompoundNodePtr = _CompoundNode]()
                                {
                                    auto Color = FCkSmDebuggerStyle::Color_Sm_SubSmLabel;
                                    if (CompoundNodePtr && NOT CompoundNodePtr->Get_IsParentStateActive())
                                    { Color.A *= 0.35f; }
                                    return FSlateColor(Color);
                                })
                        ]
                ]
        ];

    CreatePinWidgets();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_SmCompound::
    CreatePinWidgets()
    -> void
{
    if (NOT GraphNode) { return; }

    for (auto* Pin : GraphNode->Pins)
    {
        if (Pin && NOT Pin->bHidden)
        {
            auto PinWidget = SNew(SGraphPin, Pin);
            PinWidget->SetIsEditable(false);
            PinWidget->SetVisibility(EVisibility::Collapsed);
            AddPin(PinWidget);
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_SmCompound::
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

    InputPins.Add(PinToAdd);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SGraphNode_SmCompound::
    OnPaint(
        const FPaintArgs& Args,
        const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FWidgetStyle& InWidgetStyle,
        bool bParentEnabled) const
    -> int32
{
    auto ContainerLayer = LayerId - 2;

    auto Size = AllottedGeometry.GetLocalSize();
    auto W = static_cast<float>(Size.X);
    auto H = static_cast<float>(Size.Y);

    constexpr auto CornerRadius = 8.0f;
    constexpr auto BorderThickness = 1.0f;

    auto IsParentActive = _CompoundNode && _CompoundNode->Get_IsParentStateActive();
    constexpr auto ActiveBorderAlpha = 0.4f;
    constexpr auto InactiveBorderAlpha = 0.15f;
    constexpr auto ActiveFillAlpha = 0.02f;
    constexpr auto InactiveFillAlpha = 0.01f;

    auto BorderAlpha = IsParentActive ? ActiveBorderAlpha : InactiveBorderAlpha;
    auto FillAlpha = IsParentActive ? ActiveFillAlpha : InactiveFillAlpha;

    auto BorderColor = (_CompoundNode && _CompoundNode->Get_HasActiveSubState())
        ? FCkSmDebuggerStyle::Color_Sm_SubSmCurrentBorder
        : FCkSmDebuggerStyle::Color_Sm_SubSmInactiveBorder;
    BorderColor.A = BorderAlpha;

    // --- Border ---
    {
        static auto BorderBrush = FSlateRoundedBoxBrush(FLinearColor::White, CornerRadius);

        FSlateDrawElement::MakeBox(
            OutDrawElements, ContainerLayer,
            AllottedGeometry.ToPaintGeometry(),
            &BorderBrush, ESlateDrawEffect::None, BorderColor);
    }

    // --- Near-transparent fill ---
    {
        static auto FillBrush = FSlateRoundedBoxBrush(FLinearColor::White, CornerRadius - 1.0f);
        auto FillGeom = AllottedGeometry.MakeChild(
            FVector2f(W - BorderThickness * 2, H - BorderThickness * 2),
            FSlateLayoutTransform(FVector2f(BorderThickness, BorderThickness)));

        auto FillColor = FCkSmDebuggerStyle::Color_Sm_SubSmNodeBackground;
        FillColor.A = FillAlpha;

        FSlateDrawElement::MakeBox(
            OutDrawElements, ContainerLayer + 1,
            FillGeom.ToPaintGeometry(),
            &FillBrush, ESlateDrawEffect::None, FillColor);
    }

    return SGraphNode::OnPaint(
        Args, AllottedGeometry, MyCullingRect,
        OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

// --------------------------------------------------------------------------------------------------------------------
