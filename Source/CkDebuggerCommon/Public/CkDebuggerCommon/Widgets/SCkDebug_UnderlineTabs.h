#pragma once

#include "CkEditorTools/Style/CkIcons_Generated.h"

#include "Widgets/SCompoundWidget.h"

class SComboButton;
class SHorizontalBox;

// ====================================================================================================================
// Underline tab bar -- flat labels with a 2px accent underline on the active
// tab (the mockup's top tabs and center Decision/Graph/Search tabs).
//
// Tabs are declared up front; ActiveTabId is attribute-bound (owner is the
// source of truth) and OnTabSelected reports clicks. Per-tab extras:
//   IconId      optional glyph drawn before the label
//   SortOrder   ascending, applied once at construct; ties keep given order
//   CountText   small mono chip after the label ("6")
//   ShowWarnDot amber dot after the label (Catalog Audit's findings marker)
//   Visibility  live-hide a tab (the nerd-gated Search trace tab)
//
// The strip is ONE physical line and never wraps. Past the allotted width the
// tabs that do not fit move into a trailing overflow control whose menu lists
// them -- so a tab is never merely clipped and unreachable. The active tab is
// always on the visible line.
// ====================================================================================================================

struct FCkDebug_UnderlineTabDesc
{
    FName Id;
    FText Label;
    /** Optional glyph before the label. None draws nothing. */
    ECk_Icon IconId = ECk_Icon::None;
    /** Ascending. Applied once at construct with a STABLE sort, so ties keep the declared order. */
    int32 SortOrder = 0;
    TAttribute<FText> CountText;
    TAttribute<bool>  ShowWarnDot = false;
    TAttribute<EVisibility> Visibility = EVisibility::Visible;
};

// ====================================================================================================================

/**
 * The pure, testable half of the overflow decision -- no Slate, no widget state, so the
 * "every tab stays reachable" guarantee can be pinned by a spec instead of by inspection.
 *
 * Contract:
 *  - VisibleIndices and OverflowIndices together cover EVERY input index exactly once.
 *  - Both lists preserve the input order.
 *  - A valid ActiveIndex is always in VisibleIndices; trailing inactive tabs are evicted
 *    to make room for it.
 *  - Everything fitting means an EMPTY OverflowIndices (the host hides its overflow control).
 *  - Degenerate input (non-finite or non-positive widths, non-positive available width) fails
 *    CLOSED to "one anchor tab visible, everything else in overflow" rather than guessing.
 */
struct CKDEBUGGERCOMMON_API FCkDebug_UnderlineTabLayout
{
    TArray<int32> VisibleIndices;
    TArray<int32> OverflowIndices;

    auto Get_IsEquivalentTo(const FCkDebug_UnderlineTabLayout& InOther) const -> bool;

    static auto Compute(
        float InAvailableWidth,
        const TArray<float>& InDesiredWidths,
        float InOverflowButtonWidth,
        int32 InActiveIndex)
        -> FCkDebug_UnderlineTabLayout;
};

// ====================================================================================================================

DECLARE_DELEGATE_OneParam(FOnCkDebug_TabSelected, FName /* TabId */);

class CKDEBUGGERCOMMON_API SCkDebug_UnderlineTabs : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_UnderlineTabs)
        : _TabPadding(FMargin(14.0f, 8.0f))
        , _FontSize(0)   // 0 -> CkStyle::FontSizeBody()
    {}
        SLATE_ARGUMENT(TArray<FCkDebug_UnderlineTabDesc>, Tabs)
        SLATE_ATTRIBUTE(FName, ActiveTabId)
        SLATE_ARGUMENT(FMargin, TabPadding)
        SLATE_ARGUMENT(int32, FontSize)
        SLATE_EVENT(FOnCkDebug_TabSelected, OnTabSelected)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

private:
    auto Build_Tab(const FCkDebug_UnderlineTabDesc& InTab) const -> TSharedRef<SWidget>;
    auto Build_OverflowMenu() -> TSharedRef<SWidget>;
    auto Build_TabDecorations(const TSharedRef<SHorizontalBox>& InRow, const FCkDebug_UnderlineTabDesc& InTab) const -> void;

    auto Get_ActiveIndex() const -> int32;
    auto Refresh_Measurements() -> void;
    auto Refresh_Layout(float InAvailableWidth) -> void;
    auto Rebuild_Row() -> void;

    TArray<FCkDebug_UnderlineTabDesc> _Tabs;
    TArray<TSharedPtr<SWidget>> _TabWidgets;
    TArray<float> _MeasuredWidths;

    TAttribute<FName> _ActiveTabId;
    FOnCkDebug_TabSelected _OnTabSelected;
    FMargin _TabPadding;
    int32 _FontSize = 0;

    TSharedPtr<SHorizontalBox> _Row;
    TSharedPtr<SComboButton> _OverflowButton;

    FCkDebug_UnderlineTabLayout _Layout;
    float _OverflowButtonWidth = 0.0f;
    float _LastLayoutWidth = -1.0f;
    float _LastLayoutWidthSum = -1.0f;
    int32 _LastLayoutActiveIndex = INDEX_NONE;
    uint32 _LastLayoutVisibilitySignature = 0;
};

// ====================================================================================================================
