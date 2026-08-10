#include "SCkDebug_SearchBar.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"

// ====================================================================================================================

namespace ck_debug_search_bar
{
    // Both glyphs ride the IconSize axis live. The search icon sits AT the axis' own scale
    // (12/16/20, Medium == today's 16); the clear glyph is a deliberately smaller affordance, so it
    // rides the axis as a DELTA off its shipped 12 — Classic renders 16 and 12 exactly as before.
    constexpr auto ClearGlyphSizeBase = 12.0f;

    auto Get_SearchGlyphSize() -> TOptional<FVector2D>
    {
        const auto Size = ck::debug_axes::Get_IconSize(UCkDebuggerStyleSettings::Get_Selection());
        return TOptional<FVector2D>{FVector2D{Size, Size}};
    }

    auto Get_ClearGlyphExtent() -> float
    {
        return ck::debug_axes::Apply_IconSize(ClearGlyphSizeBase);
    }

    auto Get_ClearGlyphBoxSize() -> FOptionalSize
    {
        return FOptionalSize{Get_ClearGlyphExtent()};
    }

    auto Get_ClearGlyphSize() -> TOptional<FVector2D>
    {
        const auto Size = Get_ClearGlyphExtent();
        return TOptional<FVector2D>{FVector2D{Size, Size}};
    }
}

// ====================================================================================================================

auto
    SCkDebug_SearchBar::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _OnSearchTextChanged = InArgs._OnSearchTextChanged;
    _DebounceDelay = FMath::Max(0.0f, InArgs._DebounceDelay);

    ChildSlot
    [
        SNew(SBox)
        .HeightOverride(InArgs._DesiredHeight)
        .Clipping(EWidgetClipping::ClipToBounds)
        [
            SNew(SBorder)
            .BorderImage(FCkDebuggerStyle::Get().GetBrush("CkDebugger.Background.Dark"))
            .Padding(FMargin{CkStyle::SpaceS})
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                [
                    SNew(SImage)
                    .Image(FAppStyle::GetBrush("Icons.Search"))
                    .ColorAndOpacity(FSlateColor{CkStyle::TextMute()})
                    .DesiredSizeOverride_Static(&ck_debug_search_bar::Get_SearchGlyphSize)
                ]

                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SAssignNew(_SearchTextBox, SEditableTextBox)
                    .HintText(InArgs._HintText)
                    .OnTextChanged(this, &SCkDebug_SearchBar::Handle_TextChanged)
                    .OnTextCommitted(this, &SCkDebug_SearchBar::Handle_TextCommitted)
                    .Style(FAppStyle::Get(), "NormalEditableTextBox")
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
                [
                    SNew(SBox)
                    .WidthOverride_Static(&ck_debug_search_bar::Get_ClearGlyphBoxSize)
                    .HeightOverride_Static(&ck_debug_search_bar::Get_ClearGlyphBoxSize)
                    [
                        SNew(SButton)
                        .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                        .OnClicked(this, &SCkDebug_SearchBar::Handle_ClearClicked)
                        .Visibility(this, &SCkDebug_SearchBar::Get_ClearButtonVisibility)
                        .ToolTipText(FText::FromString(TEXT("Clear search")))
                        [
                            SNew(SImage)
                            .Image(FAppStyle::GetBrush("Icons.X"))
                            .ColorAndOpacity(FSlateColor{CkStyle::TextDim()})
                            .DesiredSizeOverride_Static(&ck_debug_search_bar::Get_ClearGlyphSize)
                        ]
                    ]
                ]
            ]
        ]
    ];
}

// ====================================================================================================================

auto
    SCkDebug_SearchBar::
    Tick(
        const FGeometry& InAllottedGeometry,
        const double InCurrentTime,
        const float InDeltaTime)
    -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    if (NOT _HasPendingSearch)
    { return; }

    _TimeSinceLastChange += InDeltaTime;

    if (_TimeSinceLastChange >= _DebounceDelay)
    {
        Do_ProcessDebouncedSearch();
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_SearchBar::
    Get_SearchText() const
    -> FString
{
    return _SearchText.ToString();
}

auto
    SCkDebug_SearchBar::
    Clear_SearchText()
    -> void
{
    _SearchText = FText::GetEmpty();

    if (_SearchTextBox.IsValid())
    { _SearchTextBox->SetText(_SearchText); }

    _HasPendingSearch = false;
    _TimeSinceLastChange = 0.0f;

    _OnSearchTextChanged.ExecuteIfBound(FString{});
}

auto
    SCkDebug_SearchBar::
    Set_SearchText(
        const FString& InText)
    -> void
{
    _SearchText = FText::FromString(InText);

    if (_SearchTextBox.IsValid())
    { _SearchTextBox->SetText(_SearchText); }

    Do_ProcessDebouncedSearch();
}

// ====================================================================================================================

auto
    SCkDebug_SearchBar::
    Handle_TextChanged(
        const FText& InText)
    -> void
{
    _SearchText = InText;
    _HasPendingSearch = true;
    _TimeSinceLastChange = 0.0f;
}

auto
    SCkDebug_SearchBar::
    Handle_TextCommitted(
        const FText& InText,
        ETextCommit::Type InCommitType)
    -> void
{
    _SearchText = InText;
    Do_ProcessDebouncedSearch();
}

auto
    SCkDebug_SearchBar::
    Do_ProcessDebouncedSearch()
    -> void
{
    _HasPendingSearch = false;
    _TimeSinceLastChange = 0.0f;

    _OnSearchTextChanged.ExecuteIfBound(_SearchText.ToString());
}

auto
    SCkDebug_SearchBar::
    Get_ClearButtonVisibility() const
    -> EVisibility
{
    return _SearchText.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
}

auto
    SCkDebug_SearchBar::
    Handle_ClearClicked()
    -> FReply
{
    Clear_SearchText();
    return FReply::Handled();
}

// ====================================================================================================================
