#include "SCkDebug_ValuePill.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "InputCoreTypes.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================

auto
    SCkDebug_ValuePill::
    Construct(const FArguments& InArgs)
    -> void
{
    _Value    = InArgs._Value;
    _Editable = InArgs._Editable;
    _OnToggled = InArgs._OnToggled;

    const auto Value    = _Value;
    const auto Editable = _Editable;

    SetCursor(TAttribute<TOptional<EMouseCursor::Type>>::CreateLambda([Editable]() -> TOptional<EMouseCursor::Type>
    {
        if (Editable.Get(false)) { return EMouseCursor::Hand; }
        return TOptional<EMouseCursor::Type>{};
    }));
    const auto TrueText  = InArgs._TrueText;
    const auto FalseText = InArgs._FalseText;

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(CkStyle::GetRoundedBrush())
        .BorderBackgroundColor_Lambda([this, Value, Editable]
        {
            if (Editable.Get(false) && IsHovered())
            { return FSlateColor{CkStyle::Accent()}; }

            if (Value.Get(false))
            {
                auto Border = CkStyle::Ok(); Border.A = 0.55f;
                return FSlateColor{Border};
            }
            return FSlateColor{CkStyle::Border()};
        })
        .Padding(FMargin{1.0f})
        [
            SNew(SBorder)
            .BorderImage(CkStyle::GetRoundedBrush())
            .BorderBackgroundColor_Lambda([Value]
            {
                return FSlateColor{Value.Get(false) ? CkStyle::OkDim() : CkStyle::NeutralDim()};
            })
            .Padding(FMargin{8.0f, 3.0f})
            [
                SNew(SBox)
                .MinDesiredWidth(InArgs._MinWidth)
                .HAlign(HAlign_Center)
                [
                    SNew(STextBlock)
                    .Text_Lambda([Value, TrueText, FalseText]
                    {
                        return Value.Get(false) ? TrueText : FalseText;
                    })
                    .Font(CkStyle::MonoFont(CkStyle::FontSizeSmall()))
                    .ColorAndOpacity_Lambda([this, Value, Editable]
                    {
                        if (Editable.Get(false) && IsHovered())
                        { return FSlateColor{CkStyle::Accent()}; }

                        return FSlateColor{Value.Get(false) ? CkStyle::Ok() : CkStyle::TextMute()};
                    })
                ]
            ]
        ]
    ];
}

auto
    SCkDebug_ValuePill::
    OnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
    -> FReply
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && _Editable.Get(false) && _OnToggled.IsBound())
    {
        _OnToggled.Execute(NOT _Value.Get(false));
        return FReply::Handled();
    }

    return SCompoundWidget::OnMouseButtonDown(InGeometry, InMouseEvent);
}

// ====================================================================================================================
