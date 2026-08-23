#include "SCkDebug_ViewportComponentPickerControls.h"

#include "CkDebuggerCommon/Picker/CkDebug_ViewportComponentPicker.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debug_viewport_component_picker_controls
{
    auto Get_BodyFont() -> FSlateFontInfo
    {
        return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeBody());
    }
}

// =====================================================================================================================

auto
    SCkDebug_ViewportComponentPickerControls::
    Construct(
        const FArguments& InArgs) -> void
{
    _Picker = InArgs._Picker;
    _PickTooltip = InArgs._PickTooltip;

    ChildSlot
    [
        SNew(SButton)
        .ButtonColorAndOpacity_Lambda([this]() -> FLinearColor
        {
            return _Picker.IsValid() && _Picker->IsActive()
                ? CkStyle::Selection()
                : CkStyle::Bg2();
        })
        .ForegroundColor_Lambda([this]() -> FSlateColor
        {
            return _Picker.IsValid() && _Picker->IsActive()
                ? FSlateColor{CkStyle::TextStrong()}
                : FSlateColor{CkStyle::TextDim()};
        })
        .IsEnabled_Lambda([this]() -> bool
        {
            return _Picker.IsValid() && (_Picker->IsActive() || _Picker->CanActivate());
        })
        .ToolTipText_Lambda([this]() -> FText
        {
            if (NOT _Picker.IsValid())
            { return FText::GetEmpty(); }
            if (_Picker->IsActive())
            { return FText::FromString(TEXT("Exit component pick mode (Esc)")); }
            if (NOT _Picker->CanActivate())
            { return FText::FromString(TEXT("Pick mode requires an active Editor, PIE, or Game world.")); }
            return _PickTooltip.IsEmpty()
                ? FText::FromString(TEXT("Pick a rendered component in the active viewport."))
                : _PickTooltip;
        })
        .OnClicked_Lambda([this]() -> FReply
        {
            if (_Picker.IsValid())
            {
                _Picker->Toggle();
            }
            return FReply::Handled();
        })
        [
            SNew(STextBlock)
            .Text_Lambda([this]() -> FText
            {
                return _Picker.IsValid() && _Picker->IsActive()
                    ? FText::FromString(TEXT("Picking..."))
                    : FText::FromString(TEXT("Pick"));
            })
            .Font_Static(&ck_debug_viewport_component_picker_controls::Get_BodyFont)
        ]
    ];
}

// --------------------------------------------------------------------------------------------------------------------
