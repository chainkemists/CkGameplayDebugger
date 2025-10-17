#include "CkDebuggerWidget_SearchBar.h"
#include "SlateIM/Public/SlateIM.h"

auto FCkDebuggerWidget_SearchBar::Draw() -> void
{
    SlateIM::BeginHorizontalStack();
    {
        // Search input field
        SlateIM::Fill();
        SlateIM::Padding(FMargin(4.0f, 2.0f));

        if (SlateIM::EditableText(FilterText, TEXT("Search...")))
        {
            // Text changed - apply immediately (no debounce for now, can add later if needed)
            OnSearchChanged.ExecuteIfBound(FilterText);

            // Add to history if not empty
            if (!FilterText.IsEmpty() &&
                (SearchHistory.Num() == 0 || SearchHistory.Last() != FilterText))
            {
                SearchHistory.Add(FilterText);
                if (SearchHistory.Num() > MaxHistorySize)
                {
                    SearchHistory.RemoveAt(0);
                }
            }
        }

        // Clear button
        if (!FilterText.IsEmpty())
        {
            SlateIM::AutoSize();
            SlateIM::Padding(FMargin(2.0f));
            if (SlateIM::Button(TEXT("X")))
            {
                Clear();
            }
        }
    }
    SlateIM::EndHorizontalStack();
}

auto FCkDebuggerWidget_SearchBar::PassFilter(const FString& InText) const -> bool
{
    if (!IsActive())
    {
        return true;
    }

    if (bCaseSensitive)
    {
        return InText.Contains(FilterText);
    }

    return InText.ToLower().Contains(FilterText.ToLower());
}