#pragma once

#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================
// Underline tab bar — flat labels with a 2px accent underline on the active
// tab (the mockup's top tabs and center Decision/Graph/Search tabs).
//
// Tabs are declared up front; ActiveTabId is attribute-bound (owner is the
// source of truth) and OnTabSelected reports clicks. Per-tab extras:
//   CountText   small mono chip after the label ("6")
//   ShowWarnDot amber dot after the label (Catalog Audit's findings marker)
//   Visibility  live-hide a tab (the nerd-gated Search trace tab)
// ====================================================================================================================

struct FCkDebug_UnderlineTabDesc
{
    FName Id;
    FText Label;
    TAttribute<FText> CountText;
    TAttribute<bool>  ShowWarnDot = false;
    TAttribute<EVisibility> Visibility = EVisibility::Visible;
};

DECLARE_DELEGATE_OneParam(FOnCkDebug_TabSelected, FName /* TabId */);

class CKDEBUGGERCOMMON_API SCkDebug_UnderlineTabs : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_UnderlineTabs)
        : _TabPadding(FMargin(14.0f, 8.0f))
        , _FontSize(0)   // 0 → CkStyle::FontSizeBody()
    {}
        SLATE_ARGUMENT(TArray<FCkDebug_UnderlineTabDesc>, Tabs)
        SLATE_ATTRIBUTE(FName, ActiveTabId)
        SLATE_ARGUMENT(FMargin, TabPadding)
        SLATE_ARGUMENT(int32, FontSize)
        SLATE_EVENT(FOnCkDebug_TabSelected, OnTabSelected)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
};

// ====================================================================================================================
