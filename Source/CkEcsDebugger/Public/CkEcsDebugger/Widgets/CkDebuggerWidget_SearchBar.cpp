#include "CkDebuggerWidget_SearchBar.h"

#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Styling/AppStyle.h"

auto SCkDebuggerWidget_SearchBar::Construct(const FArguments& InArgs) -> void
{
    OnSearchTextChangedDelegate = InArgs._OnSearchTextChanged;
    DebounceDelay = InArgs._DebounceDelay;

    ChildSlot
    [
        SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        [
            SAssignNew(SearchTextBox, SEditableTextBox)
            .HintText(FText::FromString(TEXT("Search entities...")))
            .OnTextChanged(this, &SCkDebuggerWidget_SearchBar::OnSearchTextChanged)
            .OnTextCommitted(this, &SCkDebuggerWidget_SearchBar::OnSearchTextCommitted)
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(4.0f, 0.0f, 0.0f, 0.0f)
        [
            SNew(SButton)
            .ButtonStyle(FAppStyle::Get(), "SimpleButton")
            .OnClicked(this, &SCkDebuggerWidget_SearchBar::OnClearButtonClicked)
            .Visibility(this, &SCkDebuggerWidget_SearchBar::Get_ClearButtonVisibility)
            .ToolTipText(FText::FromString(TEXT("Clear search")))
            [
                SNew(SImage)
                .Image(FAppStyle::GetBrush("Icons.X"))
                .ColorAndOpacity(FSlateColor::UseForeground())
            ]
        ]
    ];
}

auto SCkDebuggerWidget_SearchBar::Tick(
    const FGeometry& InAllottedGeometry,
    const double InCurrentTime,
    const float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    if (NOT HasPendingSearch)
    { return; }

    TimeSinceLastChange += InDeltaTime;

    if (TimeSinceLastChange >= DebounceDelay)
    {
        ProcessDebouncedSearch();
    }
}

auto SCkDebuggerWidget_SearchBar::Get_SearchText() const -> FString
{
    return SearchText.ToString();
}

auto SCkDebuggerWidget_SearchBar::Clear_SearchText() -> void
{
    SearchText = FText::GetEmpty();
    if (SearchTextBox.IsValid())
    {
        SearchTextBox->SetText(SearchText);
    }

    HasPendingSearch = false;
    TimeSinceLastChange = 0.0f;

    OnSearchTextChangedDelegate.ExecuteIfBound(FString());
}

auto SCkDebuggerWidget_SearchBar::OnSearchTextChanged(const FText& InText) -> void
{
    SearchText = InText;
    HasPendingSearch = true;
    TimeSinceLastChange = 0.0f;
}

auto SCkDebuggerWidget_SearchBar::OnSearchTextCommitted(
    const FText& InText,
    ETextCommit::Type InCommitType) -> void
{
    SearchText = InText;
    ProcessDebouncedSearch();
}

auto SCkDebuggerWidget_SearchBar::ProcessDebouncedSearch() -> void
{
    HasPendingSearch = false;
    TimeSinceLastChange = 0.0f;

    OnSearchTextChangedDelegate.ExecuteIfBound(SearchText.ToString());
}

auto SCkDebuggerWidget_SearchBar::Get_ClearButtonVisibility() const -> EVisibility
{
    return SearchText.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
}

auto SCkDebuggerWidget_SearchBar::OnClearButtonClicked() -> FReply
{
    Clear_SearchText();
    return FReply::Handled();
}