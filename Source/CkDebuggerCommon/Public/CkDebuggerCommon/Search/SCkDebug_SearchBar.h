#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SEditableTextBox;

DECLARE_DELEGATE_OneParam(FCkDebug_OnSearchTextChanged, const FString&);

// ====================================================================================================================
// Single-mode search input: search glyph + debounced text box + clear button.
//
// The light sibling of SCkDebug_DualSearchBar — reach for this when the surface only needs one
// "narrow this list" query (a per-inspector filter, a small side panel). When the user should be
// able to narrow AND highlight independently, use the dual bar instead; see the "Search bars"
// section in CkDebuggerCommon/CLAUDE.md for the two-pass pipeline that goes with it.
//
// Debounce: OnSearchTextChanged fires DebounceDelay seconds after the last keystroke, or
// immediately on Enter / clear. Set DebounceDelay to 0 for keystroke-synchronous filtering.
//
// Promoted from CkEcsDebugger's SCkDebuggerWidget_SearchBar (2026-08-09, U1). Geometry, fonts, and
// brushes are unchanged from that widget under the Classic profile; only the icon size is now
// axis-aware.
// ====================================================================================================================

class CKDEBUGGERCOMMON_API SCkDebug_SearchBar : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_SearchBar)
        : _DebounceDelay(0.3f)
        , _HintText(FText::FromString(TEXT("Search...")))
        , _DesiredHeight(28.0f)
        {}
        SLATE_EVENT(FCkDebug_OnSearchTextChanged, OnSearchTextChanged)
        SLATE_ARGUMENT(float, DebounceDelay)
        SLATE_ARGUMENT(FText, HintText)
        SLATE_ARGUMENT(float, DesiredHeight)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    auto Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime) -> void override;

    auto Get_SearchText() const -> FString;

    /** Clears the box and fires OnSearchTextChanged with an empty string. */
    auto Clear_SearchText() -> void;

    /** Programmatic set (e.g. a card click-through). Fires OnSearchTextChanged immediately. */
    auto Set_SearchText(const FString& InText) -> void;

private:
    auto Handle_TextChanged(const FText& InText) -> void;
    auto Handle_TextCommitted(const FText& InText, ETextCommit::Type InCommitType) -> void;
    auto Do_ProcessDebouncedSearch() -> void;
    auto Get_ClearButtonVisibility() const -> EVisibility;
    auto Handle_ClearClicked() -> FReply;

private:
    FText _SearchText;
    FCkDebug_OnSearchTextChanged _OnSearchTextChanged;

    float _DebounceDelay = 0.3f;
    float _TimeSinceLastChange = 0.0f;
    bool _HasPendingSearch = false;

    TSharedPtr<SEditableTextBox> _SearchTextBox;
};

// ====================================================================================================================
