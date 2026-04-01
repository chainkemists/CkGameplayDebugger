#include "CkDebuggerWidget_GraphNode.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Rendering/DrawElements.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"

#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

auto SCkDebuggerWidget_GraphNode::Construct(const FArguments& InArgs) -> void
{
    IsCenterAttribute = InArgs._IsCenter;
    NodeColorAttribute = InArgs._NodeColor;

    const auto LabelAttribute = InArgs._Label;
    const auto ColorAttribute = InArgs._NodeColor;
    const auto bIsCenter = InArgs._IsCenter.Get(false);

    const auto& TextStyle = bIsCenter
        ? FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Bold")
        : FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Normal");

    const auto AccentColor = ColorAttribute.Get(FLinearColor::White);

    SetCursor(EMouseCursor::GrabHand);

    ChildSlot
    [
        SNew(SBox)
        .WidthOverride(FCkDebuggerStyle::GraphNode_Width)
        .HeightOverride(FCkDebuggerStyle::GraphNode_Height)
        [
            SNew(SBorder)
            .BorderImage_Raw(this, &SCkDebuggerWidget_GraphNode::Get_BackgroundBrush)
            .Padding(0.0f)
            [
                SNew(SHorizontalBox)

            // Colored accent strip on the left
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SBox)
                .WidthOverride(FCkDebuggerStyle::GraphNode_AccentWidth)
                [
                    SNew(SBorder)
                    .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Background.Dark"))
                    .BorderBackgroundColor(FSlateColor(AccentColor))
                ]
            ]

            // Label area
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            .Padding(FCkDebuggerStyle::Padding_Small, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f)
            [
                SNew(STextBlock)
                .TextStyle(&TextStyle)
                .Text(LabelAttribute)
                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                .ToolTipText(LabelAttribute)
            ]
        ]
        ]
    ];
}

auto SCkDebuggerWidget_GraphNode::OnMouseEnter(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent) -> void
{
    SCompoundWidget::OnMouseEnter(MyGeometry, MouseEvent);
    bIsHovered = true;
}

auto SCkDebuggerWidget_GraphNode::OnMouseLeave(const FPointerEvent& MouseEvent) -> void
{
    SCompoundWidget::OnMouseLeave(MouseEvent);
    bIsHovered = false;
}

auto SCkDebuggerWidget_GraphNode::OnPaint(
    const FPaintArgs& Args,
    const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements,
    int32 LayerId,
    const FWidgetStyle& InWidgetStyle,
    bool bParentEnabled) const -> int32
{
    // Draw a colored border behind the node content
    const auto bIsCenter = IsCenterAttribute.Get(false);
    const auto BorderColor = bIsCenter
        ? FCkDebuggerStyle::Color_Graph_Node_Border_Center
        : FCkDebuggerStyle::Color_Graph_Node_Border_Default;
    const auto BorderThickness = bIsCenter
        ? FCkDebuggerStyle::GraphNode_BorderThickness_Center
        : FCkDebuggerStyle::GraphNode_BorderThickness;

    static auto BorderBrush = FSlateRoundedBoxBrush(
        FLinearColor::White, FCkDebuggerStyle::GraphNode_CornerRadius);

    const auto LocalSize = AllottedGeometry.GetLocalSize();

    FSlateDrawElement::MakeBox(
        OutDrawElements,
        LayerId,
        AllottedGeometry.ToPaintGeometry(
            FVector2f(LocalSize),
            FSlateLayoutTransform(FVector2f::ZeroVector)),
        &BorderBrush,
        ESlateDrawEffect::None,
        BorderColor);

    // Draw node content inset by the border thickness
    const auto InsetGeometry = AllottedGeometry.MakeChild(
        FVector2f(LocalSize.X - BorderThickness * 2.0f, LocalSize.Y - BorderThickness * 2.0f),
        FSlateLayoutTransform(FVector2f(BorderThickness, BorderThickness)));

    return SCompoundWidget::OnPaint(
        Args, InsetGeometry, MyCullingRect, OutDrawElements,
        LayerId + 1, InWidgetStyle, bParentEnabled);
}

auto SCkDebuggerWidget_GraphNode::Get_BackgroundBrush() const -> const FSlateBrush*
{
    if (bIsHovered)
    {
        return FCkDebuggerStyle::Get().GetBrush("CkDebugger.Graph.NodeBackground.Hover");
    }

    if (IsCenterAttribute.Get(false))
    {
        return FCkDebuggerStyle::Get().GetBrush("CkDebugger.Graph.NodeBackground.Center");
    }

    return FCkDebuggerStyle::Get().GetBrush("CkDebugger.Graph.NodeBackground");
}
