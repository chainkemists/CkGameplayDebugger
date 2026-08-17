#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================

DECLARE_DELEGATE_OneParam(FOnCkDebug_IconToggleChanged, bool /* NewState */);

// One debugger boolean bound to its existing state owner. The descriptor is
// deliberately presentation-only: it never stores or persists the setting.
struct CKDEBUGGERCOMMON_API FCkDebug_IconToggleAction
{
    FCkDebug_IconToggleAction() = default;

    FCkDebug_IconToggleAction(
        FName InId,
        FName InIconId,
        FText InLabel,
        TAttribute<FText> InToolTip,
        TAttribute<bool> InIsOn,
        FOnCkDebug_IconToggleChanged InOnStateChanged,
        TAttribute<bool> InIsEnabled = true);

    auto IsValid() const -> bool;

    FName Id;
    FName IconId;
    FText Label;

    /** An ATTRIBUTE rather than a baked string, because a toolbar is built once per window and its tooltips would
     *  otherwise be frozen at that moment. A toggle that narrows a list is exactly the control a reader wants a live
     *  count on ("9 findings match the other filters"), and that number changes on every filter change without the
     *  toolbar being rebuilt. Passing a plain `FText` still works and still means "this never changes". */
    TAttribute<FText> ToolTip;

    TAttribute<bool> IsOn;
    FOnCkDebug_IconToggleChanged OnStateChanged;
    TAttribute<bool> IsEnabled = true;
};

// Pure, testable result used by the direct-action toolbar. Invalid, missing-icon,
// or duplicate descriptors reject the whole layout so no partial bar ships.
struct CKDEBUGGERCOMMON_API FCkDebug_IconToolbarLayout
{
    int32 ActionCount = 0;

    static auto TryBuild(
        const TArray<FCkDebug_IconToggleAction>& InActions,
        FCkDebug_IconToolbarLayout& OutLayout)
        -> bool;
};

// ====================================================================================================================

// Canonical icon-backed boolean control. It is keyboard-focusable through
// SCheckBox and exposes the label, explanation, and live state in its tooltip.
class CKDEBUGGERCOMMON_API SCkDebug_IconToggle : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_IconToggle)
        : _IconId(NAME_None)
        , _IsOn(false)
        , _IsEnabled(true)
        , _ShowLabel(false)
    {}
        SLATE_ARGUMENT(FName, IconId)
        SLATE_ARGUMENT(FText, Label)
        SLATE_ATTRIBUTE(FText, ToolTip)
        SLATE_ATTRIBUTE(bool, IsOn)
        SLATE_EVENT(FOnCkDebug_IconToggleChanged, OnStateChanged)
        SLATE_ATTRIBUTE(bool, IsEnabled)
        SLATE_ARGUMENT(bool, ShowLabel)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

private:
    auto Get_ToolTipText() const -> FText;

    FText _Label;
    TAttribute<FText> _ToolTip;
    TAttribute<bool> _IsOn;
};

// ====================================================================================================================

// Shared debugger menu row. Actions always remain on one horizontal line;
// the owning command lane provides horizontal scrolling when width is tight.
class CKDEBUGGERCOMMON_API SCkDebug_IconToolbar : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_IconToolbar)
    {}
        SLATE_ARGUMENT(TArray<FCkDebug_IconToggleAction>, Actions)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

private:
    auto Build_Row() -> TSharedRef<SWidget>;
    auto Build_Action(const FCkDebug_IconToggleAction& InAction) const -> TSharedRef<SWidget>;

    TArray<FCkDebug_IconToggleAction> _Actions;
};

// ====================================================================================================================
