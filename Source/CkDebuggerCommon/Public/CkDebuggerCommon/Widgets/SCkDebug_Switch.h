#pragma once

#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================
// Small sliding toggle switch (30×17 track, 11px knob) — the mockup's Sandbox
// and Enabled controls. ON: accent-dim track, accent knob at the right;
// OFF: inset track, muted knob at the left.
//
// State is attribute-bound (the owner is the source of truth); the switch
// only reports intent via OnStateChanged. Keyboard: Space/Enter toggles.
// ====================================================================================================================

DECLARE_DELEGATE_OneParam(FOnCkDebug_SwitchChanged, bool /* NewState */);

class CKDEBUGGERCOMMON_API SCkDebug_Switch : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_Switch)
        : _IsOn(false)
    {}
        SLATE_ATTRIBUTE(bool, IsOn)
        SLATE_EVENT(FOnCkDebug_SwitchChanged, OnStateChanged)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    virtual auto OnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) -> FReply override;
    virtual auto OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) -> FReply override;
    virtual auto SupportsKeyboardFocus() const -> bool override { return true; }

private:
    auto DoToggle() -> FReply;

    TAttribute<bool> _IsOn;
    FOnCkDebug_SwitchChanged _OnStateChanged;
};

// ====================================================================================================================
