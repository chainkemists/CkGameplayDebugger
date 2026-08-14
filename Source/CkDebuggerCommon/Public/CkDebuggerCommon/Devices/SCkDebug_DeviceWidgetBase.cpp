#include "CkDebuggerCommon/Devices/SCkDebug_DeviceWidgetBase.h"

#include "CkCore/Macros/CkMacros.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_DeviceWidgetBase::
    DoConstruct_DeviceCommon(
        const TAttribute<const FCkDebug_DeviceSnapshot*>& InSnapshot,
        const FCkDebug_DeviceKeyClicked& InOnKeyClicked,
        const FCkDebug_DeviceKeyTooltip& InKeyTooltip)
    -> void
{
    _Snapshot = InSnapshot;
    _OnKeyClicked = InOnKeyClicked;
    _KeyTooltip = InKeyTooltip;
    SetCanTick(false);

    if (_KeyTooltip.IsBound())
    {
        SetToolTipText(TAttribute<FText>::CreateLambda([this]() -> FText
        {
            return _HoveredKey.IsValid() ? _KeyTooltip.Execute(_HoveredKey) : FText::GetEmpty();
        }));
    }

    if (_OnKeyClicked.IsBound())
    {
        SetCursor(TAttribute<TOptional<EMouseCursor::Type>>::CreateLambda([this]() -> TOptional<EMouseCursor::Type>
        {
            return _HoveredKey.IsValid() ? EMouseCursor::Hand : EMouseCursor::Default;
        }));
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDebug_DeviceWidgetBase::
    OnMouseButtonDown(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent)
    -> FReply
{
    if (NOT _OnKeyClicked.IsBound() || InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
    { return FReply::Unhandled(); }

    const auto Key = Get_KeyAtPosition(InGeometry, InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()));

    if (NOT Key.IsValid())
    { return FReply::Unhandled(); }

    _OnKeyClicked.Execute(Key);
    return FReply::Handled();
}

auto
    SCkDebug_DeviceWidgetBase::
    OnMouseMove(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent)
    -> FReply
{
    _HoveredKey = Get_KeyAtPosition(InGeometry, InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()));
    return FReply::Unhandled();
}

auto
    SCkDebug_DeviceWidgetBase::
    OnMouseLeave(
        const FPointerEvent& InMouseEvent)
    -> void
{
    _HoveredKey = FKey{};
    SLeafWidget::OnMouseLeave(InMouseEvent);
}

// --------------------------------------------------------------------------------------------------------------------
