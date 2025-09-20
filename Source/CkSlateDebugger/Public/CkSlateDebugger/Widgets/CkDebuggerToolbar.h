#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SCkSlateDebuggerWindow;

class SCkDebuggerToolbar : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebuggerToolbar) {}
        SLATE_ARGUMENT(TWeakPtr<SCkSlateDebuggerWindow>, DebuggerWindow)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    auto OnRefreshClicked() -> FReply;
    auto OnClearSelectionClicked() -> FReply;
    auto OnOptionsClicked() -> FReply;
    auto OnFocusSelectedClicked() -> FReply;

    auto GetRefreshButtonTooltip() const -> FText;
    auto IsRefreshEnabled() const -> bool;
    auto IsSelectionActionEnabled() const -> bool;

private:
    TWeakPtr<SCkSlateDebuggerWindow> DebuggerWindow;
};