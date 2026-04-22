#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

DECLARE_DELEGATE_OneParam(FCkDebugger_OnSearchTextChanged, const FString&);

// --------------------------------------------------------------------------------------------------------------------
// Single-mode search bar with debounce + clear button. Used by panels that
// only need a "filter this list" affordance (e.g. the inspector's per-component
// search).
//
// For panels that want both a Filter and a Highlight input, use the shared
// SCkDebug_DualSearchBar from CkDebuggerCommon instead.
// --------------------------------------------------------------------------------------------------------------------

class SCkDebuggerWidget_SearchBar : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebuggerWidget_SearchBar)
        : _DebounceDelay(0.3f)
        {}
        SLATE_EVENT(FCkDebugger_OnSearchTextChanged, OnSearchTextChanged)
        SLATE_ARGUMENT(float, DebounceDelay)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    auto Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime) -> void override;

    auto Get_SearchText() const -> FString;
    auto Clear_SearchText() -> void;

private:
    auto OnSearchTextChanged(const FText& InText) -> void;
    auto OnSearchTextCommitted(const FText& InText, ETextCommit::Type InCommitType) -> void;
    auto ProcessDebouncedSearch() -> void;
    auto Get_ClearButtonVisibility() const -> EVisibility;
    auto OnClearButtonClicked() -> FReply;

    FText SearchText;
    FCkDebugger_OnSearchTextChanged OnSearchTextChangedDelegate;

    float DebounceDelay = 0.3f;
    float TimeSinceLastChange = 0.0f;
    bool HasPendingSearch = false;

    TSharedPtr<SEditableTextBox> SearchTextBox;
};
