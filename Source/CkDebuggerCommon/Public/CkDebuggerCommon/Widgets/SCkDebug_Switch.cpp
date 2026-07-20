#include "SCkDebug_Switch.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "InputCoreTypes.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"

// ====================================================================================================================

namespace ck_debug_switch
{
    constexpr auto TrackWidth  = 30.0f;
    constexpr auto TrackHeight = 17.0f;
    constexpr auto KnobSize    = 11.0f;
    constexpr auto KnobInset   = 2.0f;
}

// ====================================================================================================================

auto
    SCkDebug_Switch::
    Construct(const FArguments& InArgs)
    -> void
{
    _IsOn = InArgs._IsOn;
    _OnStateChanged = InArgs._OnStateChanged;

    const auto IsOn = _IsOn;

    SetCursor(EMouseCursor::Hand);

    using namespace ck_debug_switch;

    ChildSlot
    [
        SNew(SBox)
        .WidthOverride(TrackWidth)
        .HeightOverride(TrackHeight)
        [
            SNew(SBorder)
            .BorderImage(CkStyle::GetRoundedBrush_Pill())
            .BorderBackgroundColor_Lambda([this, IsOn]
            {
                // Hover feedback — the 30×17 track is a small target; the accent
                // ring tells the user it is live before they commit the click.
                if (IsHovered())
                {
                    auto Border = CkStyle::Accent(); Border.A = IsOn.Get(false) ? 0.9f : 0.5f;
                    return FSlateColor{Border};
                }
                if (IsOn.Get(false))
                {
                    auto Border = CkStyle::Accent(); Border.A = 0.6f;
                    return FSlateColor{Border};
                }
                return FSlateColor{CkStyle::Border()};
            })
            .Padding(FMargin{1.0f})
            [
                SNew(SBorder)
                .BorderImage(CkStyle::GetRoundedBrush_Pill())
                .BorderBackgroundColor_Lambda([IsOn]
                {
                    return FSlateColor{IsOn.Get(false) ? CkStyle::AccentDim() : CkStyle::NeutralDim()};
                })
                .Padding(TAttribute<FMargin>::CreateLambda([IsOn]
                {
                    return IsOn.Get(false)
                        ? FMargin{TrackWidth - KnobSize - KnobInset - 2.0f, KnobInset, KnobInset, KnobInset}
                        : FMargin{KnobInset, KnobInset, TrackWidth - KnobSize - KnobInset - 2.0f, KnobInset};
                }))
                [
                    SNew(SBox)
                    .WidthOverride(KnobSize)
                    .HeightOverride(KnobSize)
                    [
                        SNew(SImage)
                        .Image(CkStyle::GetRoundedBrush_Pill())
                        .ColorAndOpacity_Lambda([IsOn]
                        {
                            return FSlateColor{IsOn.Get(false) ? CkStyle::Accent() : CkStyle::TextMute()};
                        })
                    ]
                ]
            ]
        ]
    ];
}

auto
    SCkDebug_Switch::
    OnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
    -> FReply
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    { return DoToggle(); }

    return SCompoundWidget::OnMouseButtonDown(InGeometry, InMouseEvent);
}

auto
    SCkDebug_Switch::
    OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
    -> FReply
{
    if (InKeyEvent.GetKey() == EKeys::Enter || InKeyEvent.GetKey() == EKeys::SpaceBar)
    { return DoToggle(); }

    return SCompoundWidget::OnKeyDown(InGeometry, InKeyEvent);
}

auto
    SCkDebug_Switch::
    DoToggle()
    -> FReply
{
    if (_OnStateChanged.IsBound())
    { _OnStateChanged.Execute(NOT _IsOn.Get(false)); }

    return FReply::Handled();
}

// ====================================================================================================================
