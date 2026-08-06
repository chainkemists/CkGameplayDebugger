#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SWidgetSwitcher;

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
        FText InToolTip,
        TAttribute<bool> InIsOn,
        FOnCkDebug_IconToggleChanged InOnStateChanged,
        TAttribute<bool> InIsEnabled = true);

    auto IsValid() const -> bool;

    FName Id;
    FName IconId;
    FText Label;
    FText ToolTip;
    TAttribute<bool> IsOn;
    FOnCkDebug_IconToggleChanged OnStateChanged;
    TAttribute<bool> IsEnabled = true;
};

// Pure, testable result used by the responsive toolbar. Invalid, missing-icon,
// or duplicate descriptors reject the whole layout so no partial bar ships.
struct CKDEBUGGERCOMMON_API FCkDebug_IconToolbarPartition
{
    int32 DirectCount = 0;
    int32 OverflowCount = 0;

    static auto TryBuild(
        const TArray<FCkDebug_IconToggleAction>& InActions,
        int32 InDirectLimit,
        FCkDebug_IconToolbarPartition& OutPartition)
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
        SLATE_ARGUMENT(FText, ToolTip)
        SLATE_ATTRIBUTE(bool, IsOn)
        SLATE_EVENT(FOnCkDebug_IconToggleChanged, OnStateChanged)
        SLATE_ATTRIBUTE(bool, IsEnabled)
        SLATE_ARGUMENT(bool, ShowLabel)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

private:
    auto Get_ToolTipText() const -> FText;

    FText _Label;
    FText _ToolTip;
    TAttribute<bool> _IsOn;
};

// ====================================================================================================================

// Shared direct-action row with a labeled overflow menu. Wide and compact
// widget trees are built once; Tick only selects the appropriate prebuilt row.
class CKDEBUGGERCOMMON_API SCkDebug_IconToolbar : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_IconToolbar)
        : _WideDirectCount(6)
        , _CompactDirectCount(3)
        , _CompactWidthThreshold(360.0f)
    {}
        SLATE_ARGUMENT(TArray<FCkDebug_IconToggleAction>, Actions)
        SLATE_ARGUMENT(int32, WideDirectCount)
        SLATE_ARGUMENT(int32, CompactDirectCount)
        SLATE_ARGUMENT(float, CompactWidthThreshold)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    virtual auto Tick(
        const FGeometry& InAllottedGeometry,
        const double InCurrentTime,
        const float InDeltaTime)
        -> void override;

private:
    auto Build_Row(int32 InDirectCount) -> TSharedRef<SWidget>;
    auto Build_Overflow(int32 InDirectCount) -> TSharedRef<SWidget>;
    auto Build_Action(const FCkDebug_IconToggleAction& InAction, bool InShowLabel) const -> TSharedRef<SWidget>;

    TArray<FCkDebug_IconToggleAction> _Actions;
    TSharedPtr<SWidgetSwitcher> _LayoutSwitcher;
    float _CompactWidthThreshold = 360.0f;
    bool _IsCompact = false;
};

// ====================================================================================================================
