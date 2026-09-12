#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FCkUiView;

// ====================================================================================================================

/** Authored operational Input HUD controls hosted by the Intent Debugger popup. */
class CKINTENTDEBUGGER_API SCkIntentDebugger_InputHudControls : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkIntentDebugger_InputHudControls) {}
        SLATE_ATTRIBUTE(bool, CanDispatchEvents)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    auto Get_ControlsView() const -> TSharedPtr<FCkUiView>
    {
        return _ControlsView;
    }

private:
    auto Build_Controls() -> TSharedRef<SWidget>;
    auto Get_ControlsLayoutError() const -> FText;

private:
    TSharedPtr<FCkUiView> _ControlsView;
    FString _ControlsPublicationError;
    TAttribute<bool> _CanDispatchEvents;
};

// ====================================================================================================================
