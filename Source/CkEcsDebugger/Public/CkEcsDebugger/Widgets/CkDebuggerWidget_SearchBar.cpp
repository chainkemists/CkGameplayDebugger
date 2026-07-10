#include "CkDebuggerWidget_SearchBar.h"

#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/AppStyle.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

auto SCkDebuggerWidget_SearchBar::Construct(const FArguments& InArgs) -> void
{
    OnSearchTextChangedDelegate = InArgs._OnSearchTextChanged;
    DebounceDelay = InArgs._DebounceDelay;

    ChildSlot
    [
        SNew(SBox)
        .HeightOverride(28.0f)
        .Clipping(EWidgetClipping::ClipToBounds)
        [
            SNew(SBorder)
            .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Background.Dark"))
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
                    .ColorAndOpacity(CkStyle::TextMute())
                    .DesiredSizeOverride(FVector2D(16.0f, 16.0f))
                ]

                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SAssignNew(SearchTextBox, SEditableTextBox)
                    .HintText(FText::FromString(TEXT("Search...")))
                    .OnTextChanged(this, &SCkDebuggerWidget_SearchBar::OnSearchTextChanged)
                    .OnTextCommitted(this, &SCkDebuggerWidget_SearchBar::OnSearchTextCommitted)
                    .Style(FAppStyle::Get(), "NormalEditableTextBox")
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FCkDebuggerStyle::Padding_Small, 0.0f, 0.0f, 0.0f)
                [
                    SNew(SBox)
                    .WidthOverride(12.0f)
                    .HeightOverride(12.0f)
                    [
                        SNew(SButton)
                        .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                        .OnClicked(this, &SCkDebuggerWidget_SearchBar::OnClearButtonClicked)
                        .Visibility(this, &SCkDebuggerWidget_SearchBar::Get_ClearButtonVisibility)
                        .ToolTipText(FText::FromString(TEXT("Clear search")))
                        [
                            SNew(SImage)
                            .Image(FAppStyle::GetBrush("Icons.X"))
                            .ColorAndOpacity(CkStyle::TextDim())
                            .DesiredSizeOverride(FVector2D(12.0f, 12.0f))
                        ]
                    ]
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