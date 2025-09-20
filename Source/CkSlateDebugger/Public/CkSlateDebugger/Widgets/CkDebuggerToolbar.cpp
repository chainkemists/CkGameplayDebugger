#include "CkDebuggerToolbar.h"

#include "CkSlateDebugger/CkSlateDebuggerStyle.h"
#include "CkSlateDebugger/CkSlateDebuggerWindow.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SSeparator.h"

#define LOCTEXT_NAMESPACE "SCkDebuggerToolbar"

void SCkDebuggerToolbar::Construct(const FArguments& InArgs)
{
    DebuggerWindow = InArgs._DebuggerWindow;

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FCkSlateDebuggerStyle::Get().GetBrush("CkDebugger.Panel"))
        .Padding(FMargin(8, 4))
        [
            SNew(SHorizontalBox)

            // Refresh button
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2, 0)
            [
                SNew(SButton)
                .ButtonStyle(&FCkSlateDebuggerStyle::Get().GetWidgetStyle<FButtonStyle>("CkDebugger.Button"))
                .OnClicked(this, &SCkDebuggerToolbar::OnRefreshClicked)
                .IsEnabled(this, &SCkDebuggerToolbar::IsRefreshEnabled)
                .ToolTipText(this, &SCkDebuggerToolbar::GetRefreshButtonTooltip)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(4, 2)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("🔄")))
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(4, 2)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("Refresh", "Refresh"))
                    ]
                ]
            ]

            // Focus selected button
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2, 0)
            [
                SNew(SButton)
                .ButtonStyle(&FCkSlateDebuggerStyle::Get().GetWidgetStyle<FButtonStyle>("CkDebugger.Button"))
                .OnClicked(this, &SCkDebuggerToolbar::OnFocusSelectedClicked)
                .IsEnabled(this, &SCkDebuggerToolbar::IsSelectionActionEnabled)
                .ToolTipText(LOCTEXT("FocusSelectedTooltip", "Focus camera on selected entity"))
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(4, 2)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("🎯")))
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(4, 2)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("FocusSelected", "Focus Selected"))
                    ]
                ]
            ]

            // Clear selection button
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2, 0)
            [
                SNew(SButton)
                .ButtonStyle(&FCkSlateDebuggerStyle::Get().GetWidgetStyle<FButtonStyle>("CkDebugger.Button"))
                .OnClicked(this, &SCkDebuggerToolbar::OnClearSelectionClicked)
                .IsEnabled(this, &SCkDebuggerToolbar::IsSelectionActionEnabled)
                .ToolTipText(LOCTEXT("ClearSelectionTooltip", "Clear all selected entities"))
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(4, 2)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("🗑️")))
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(4, 2)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("ClearSelection", "Clear Selection"))
                    ]
                ]
            ]

            // Separator
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(8, 0)
            [
                SNew(SSeparator)
                .Orientation(Orient_Vertical)
            ]

            // Options button
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2, 0)
            [
                SNew(SButton)
                .ButtonStyle(&FCkSlateDebuggerStyle::Get().GetWidgetStyle<FButtonStyle>("CkDebugger.Button"))
                .OnClicked(this, &SCkDebuggerToolbar::OnOptionsClicked)
                .ToolTipText(LOCTEXT("OptionsTooltip", "Open debugger options"))
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(4, 2)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("⚙️")))
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(4, 2)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("Options", "Options"))
                    ]
                ]
            ]

            // Spacer
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                SNullWidget::NullWidget
            ]
        ]
    ];
}

auto SCkDebuggerToolbar::OnRefreshClicked() -> FReply
{
    if (auto Window = DebuggerWindow.Pin())
    {
        Window->RefreshCurrentView();
    }
    return FReply::Handled();
}

auto SCkDebuggerToolbar::OnClearSelectionClicked() -> FReply
{
    if (auto Window = DebuggerWindow.Pin())
    {
        Window->ClearSelectedEntities();
    }
    return FReply::Handled();
}

auto SCkDebuggerToolbar::OnOptionsClicked() -> FReply
{
    // TODO: Show options menu
    return FReply::Handled();
}

auto SCkDebuggerToolbar::OnFocusSelectedClicked() -> FReply
{
    // TODO: Focus camera on selected entity
    return FReply::Handled();
}

auto SCkDebuggerToolbar::GetRefreshButtonTooltip() const -> FText
{
    return LOCTEXT("RefreshTooltip", "Refresh the current view");
}

auto SCkDebuggerToolbar::IsRefreshEnabled() const -> bool
{
    if (auto Window = DebuggerWindow.Pin())
    {
        return Window->GetSelectedWorld() != nullptr;
    }
    return false;
}

auto SCkDebuggerToolbar::IsSelectionActionEnabled() const -> bool
{
    if (auto Window = DebuggerWindow.Pin())
    {
        return Window->GetSelectedEntities().Num() > 0;
    }
    return false;
}

#undef LOCTEXT_NAMESPACE