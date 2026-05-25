#include "SCkDebug_StatPair.h"

#include "SCkDebug_SelectableLabel.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"

#include "Widgets/SBoxPanel.h"
#include "Styling/CoreStyle.h"

// ====================================================================================================================

namespace
{
    struct FStatPairFontDefaults
    {
        int32 ValueSize;
        int32 LabelSize;
        const char* LabelStyle;   // "Bold" or "Regular"
    };

    auto Defaults_StatPair(ECkDebug_StatPairLayout InLayout) -> FStatPairFontDefaults
    {
        switch (InLayout)
        {
            case ECkDebug_StatPairLayout::Inline_LabelFirst:
                // Label-first inline rows look best with a same-size, lower-case
                // label rendered in the secondary text tone (table-row feel).
                return { 10, 10, "Regular" };

            case ECkDebug_StatPairLayout::Stacked_ValueOnTop:
                // Stat-card: prominent value, tiny ALL-CAPS label beneath.
                return { 16, 8, "Bold" };

            case ECkDebug_StatPairLayout::Inline_ValueFirst:
            default:
                // Inline call-out: bold value, small uppercase muted label
                // immediately to the right.
                return { 14, 8, "Bold" };
        }
    }
}

// ====================================================================================================================

auto
    SCkDebug_StatPair::
    Construct(const FArguments& InArgs)
    -> void
{
    const auto Layout    = InArgs._Layout;
    const auto Defaults  = Defaults_StatPair(Layout);
    const auto ValueSize = InArgs._ValueFontSize > 0 ? InArgs._ValueFontSize : Defaults.ValueSize;
    const auto LabelSize = InArgs._LabelFontSize > 0 ? InArgs._LabelFontSize : Defaults.LabelSize;

    // Default value color = primary text. Override via attribute when the
    // call-out matters (e.g. amber for cost, red for errors).
    auto ValueColor = InArgs._ValueColor;
    if (NOT ValueColor.IsSet() && NOT ValueColor.IsBound())
    {
        ValueColor = FSlateColor(CkDebugStyle::Text());
    }

    // Label tone differs by layout: Inline_LabelFirst uses the muted-secondary
    // tone (table label feel); the other two use TextMute (uppercase chip).
    const auto LabelColor = Layout == ECkDebug_StatPairLayout::Inline_LabelFirst
        ? FSlateColor(CkDebugStyle::TextDim())
        : FSlateColor(CkDebugStyle::TextMute());

    const auto ValueFont = FCoreStyle::GetDefaultFontStyle("Bold", ValueSize);
    const auto LabelFont = FCoreStyle::GetDefaultFontStyle(Defaults.LabelStyle, LabelSize);

    SAssignNew(_ValueText, SCkDebug_SelectableLabel)
        .Text(InArgs._Value)
        .Font(ValueFont)
        .ColorAndOpacity(ValueColor);

    SAssignNew(_LabelText, SCkDebug_SelectableLabel)
        .Text(InArgs._Label)
        .Font(LabelFont)
        .ColorAndOpacity(LabelColor);

    switch (Layout)
    {
        case ECkDebug_StatPairLayout::Inline_LabelFirst:
        {
            ChildSlot
            [
                SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .VAlign(VAlign_Center)
                        [
                            _LabelText.ToSharedRef()
                        ]
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            _ValueText.ToSharedRef()
                        ]
            ];
            break;
        }

        case ECkDebug_StatPairLayout::Stacked_ValueOnTop:
        {
            ChildSlot
            [
                SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        .HAlign(HAlign_Center)
                        [
                            _ValueText.ToSharedRef()
                        ]
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        .HAlign(HAlign_Center)
                        [
                            _LabelText.ToSharedRef()
                        ]
            ];
            break;
        }

        case ECkDebug_StatPairLayout::Inline_ValueFirst:
        default:
        {
            ChildSlot
            [
                SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
                        [
                            _ValueText.ToSharedRef()
                        ]
                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            _LabelText.ToSharedRef()
                        ]
            ];
            break;
        }
    }
}

auto
    SCkDebug_StatPair::
    SetValue(const FText& InText)
    -> void
{
    if (_ValueText.IsValid()) { _ValueText->SetText(InText); }
}

auto
    SCkDebug_StatPair::
    SetLabel(const FText& InText)
    -> void
{
    if (_LabelText.IsValid()) { _LabelText->SetText(InText); }
}

auto
    SCkDebug_StatPair::
    SetValueColor(const FSlateColor& InColor)
    -> void
{
    // Tints just the value subtree — leaves the label's color alone. Use this
    // for status-driven color flips (e.g. "iterations" colored by SearchStatus).
    // SCompoundWidget::SetColorAndOpacity expects FLinearColor; FSlateColor's
    // accessor handles both specified and "use foreground" cases cleanly.
    if (_ValueText.IsValid()) { _ValueText->SetColorAndOpacity(InColor.GetSpecifiedColor()); }
}

// ====================================================================================================================
