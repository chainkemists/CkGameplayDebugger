#pragma once

#include "CoreMinimal.h"

struct FCkDebuggerWidget_SearchBar
{
public:
    auto Draw() -> void;

    auto Get_FilterText() const -> const FString&
    {
        return FilterText;
    }

    auto IsActive() const -> bool
    {
        return !FilterText.IsEmpty();
    }

    auto Clear() -> void
    {
        FilterText.Empty();
        OnSearchChanged.ExecuteIfBound(FilterText);
    }

    auto PassFilter(const FString& InText) const -> bool;

    DECLARE_DELEGATE_OneParam(FOnSearchChanged, const FString&);
    FOnSearchChanged OnSearchChanged;

private:
    FString FilterText;

    // Search history
    TArray<FString> SearchHistory;
    int32 HistoryIndex = -1;
    static constexpr int32 MaxHistorySize = 20;

    bool bCaseSensitive = false;
};