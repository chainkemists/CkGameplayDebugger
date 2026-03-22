#include "CkDebuggerWidget_GraphNode.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"

#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

auto SCkDebuggerWidget_GraphNode::Construct(const FArguments& InArgs) -> void
{
    OnClickedDelegate = InArgs._OnClicked;
    IsCenterAttribute = InArgs._IsCenter;

    const auto LabelAttribute = InArgs._Label;
    const auto ColorAttribute = InArgs._NodeColor;
    const auto bIsCenter = InArgs._IsCenter.Get(false);

    const auto& TextStyle = bIsCenter
        ? FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Bold")
        : FCkDebuggerStyle::Get().GetWidgetStyle<FTextBlockStyle>("CkDebugger.Text.Normal");

    const auto AccentColor = ColorAttribute.Get(FLinearColor::White);

    SetCursor(EMouseCursor::Hand);

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

auto SCkDebuggerWidget_GraphNode::OnMouseButtonDown(
    const FGeometry& MyGeometry,
    const FPointerEvent& MouseEvent) -> FReply
{
    if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        OnClickedDelegate.ExecuteIfBound();
        return FReply::Handled();
    }

    return FReply::Unhandled();
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
