#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SCkDebuggerWidget_GraphNode : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebuggerWidget_GraphNode)
        : _Label(FText::GetEmpty())
        , _NodeColor(FLinearColor::White)
        , _IsCenter(false)
    {}
        SLATE_ATTRIBUTE(FText, Label)
        SLATE_ATTRIBUTE(FLinearColor, NodeColor)
        SLATE_ATTRIBUTE(bool, IsCenter)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    auto OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) -> void override;
    auto OnMouseLeave(const FPointerEvent& MouseEvent) -> void override;

private:
    auto Get_BackgroundBrush() const -> const FSlateBrush*;

    TAttribute<bool> IsCenterAttribute;
    bool bIsHovered = false;
};
