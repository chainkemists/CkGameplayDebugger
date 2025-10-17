#include "CkDebuggerWidget_SearchBar.h"

#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/AppStyle.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

auto SCkDebuggerWidget_SearchBar::Construct(const FArguments& InArgs) -> void
{
    OnSearchTextChangedDelegate = InArgs._OnSearchTextChanged;
    DebounceDelay = InArgs._DebounceDelay;

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(new FSlateRoundedBoxBrush(
            FCkDebuggerStyle::Color_Border,
            4.0f,
            FCkDebuggerStyle::Color_Background_Light,
            1.0f
        ))
        .Padding(FMargin(FCkDebuggerStyle::Padding_Small))
        [
            SNew(SHorizontalBox)

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, FCkDebuggerStyle::Padding_Small, 0.0f)
            [
                SNew(SImage)
                .Image(FAppStyle::GetBrush("Icons.Search"))
                .ColorAndOpacity(FCkDebuggerStyle::Color_Text_Muted)
                .DesiredSizeOverride(FVector2D(16.0f, 16.0f))
            ]

            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                SAssignNew(SearchTextBox, SEditableTextBox)
                .HintText(FText::FromString(TEXT("Search entities...")))
                .OnTextChanged(this, &SCkDebuggerWidget_SearchBar::OnSearchTextChanged)
                .OnTextCommitted(this, &SCkDebuggerWidget_SearchBar::OnSearchTextCommitted)
                .Style(FAppStyle::Get(), "NormalEditableTextBox")
            ]

            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FCkDebuggerStyle::Padding_Small, 0.0f, 0.0f, 0.0f)
            [
                SNew(SButton)
                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .OnClicked(this, &SCkDebuggerWidget_SearchBar::OnClearButtonClicked)
                .Visibility(this, &SCkDebuggerWidget_SearchBar::Get_ClearButtonVisibility)
                .ToolTipText(FText::FromString(TEXT("Clear search")))
                [
                    SNew(SImage)
                    .Image(FAppStyle::GetBrush("Icons.X"))
                    .ColorAndOpacity(FCkDebuggerStyle::Color_Text_Secondary)
                    .DesiredSizeOverride(FVector2D(12.0f, 12.0f))
                ]
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