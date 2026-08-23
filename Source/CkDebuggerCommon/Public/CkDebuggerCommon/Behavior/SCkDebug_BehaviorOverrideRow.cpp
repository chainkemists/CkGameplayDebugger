#include "SCkDebug_BehaviorOverrideRow.h"

#include "CkDebuggerCommon/Behavior/CkDebug_BehaviorOverrideRegistry.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_ToggleSurface.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

auto SCkDebug_BehaviorOverrideRow::Construct(const FArguments& InArgs) -> void
{
    _OverrideId = InArgs._OverrideId;

    ChildSlot
    [
        SNew(SCkDebug_ToggleSurface)
        .IsOn(this, &SCkDebug_BehaviorOverrideRow::Get_IsActive)
        .IsEnabled(this, &SCkDebug_BehaviorOverrideRow::Get_IsAvailable)
        .AccessibleText(this, &SCkDebug_BehaviorOverrideRow::Get_Label)
        .ToolTipText(this, &SCkDebug_BehaviorOverrideRow::Get_Tooltip)
        .OnStateChanged(this, &SCkDebug_BehaviorOverrideRow::HandleStateChanged)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(STextBlock)
                .Text(this, &SCkDebug_BehaviorOverrideRow::Get_Label)
                .Font(CkStyle::BoldFont(CkStyle::FontSizeSmall()))
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(STextBlock)
                .Text(this, &SCkDebug_BehaviorOverrideRow::Get_Description)
                .Font(CkStyle::RegularFont(CkStyle::FontSizeMicro()))
                .ColorAndOpacity(CkStyle::TextMute())
            ]
        ]
    ];
}

auto SCkDebug_BehaviorOverrideRow::Get_Label() const -> FText
{
    const auto Descriptor = FCkDebug_BehaviorOverrideRegistry::Get().Find(_OverrideId);
    return Descriptor.IsSet() ? Descriptor->Get_Label() : FText::FromName(_OverrideId);
}

auto SCkDebug_BehaviorOverrideRow::Get_Description() const -> FText
{
    const auto Descriptor = FCkDebug_BehaviorOverrideRegistry::Get().Find(_OverrideId);
    return Descriptor.IsSet() ? Descriptor->Get_Description() : FText::GetEmpty();
}

auto SCkDebug_BehaviorOverrideRow::Get_Tooltip() const -> FText
{
    if (NOT _LastFailure.IsEmpty()) { return _LastFailure; }
    const auto Descriptor = FCkDebug_BehaviorOverrideRegistry::Get().Find(_OverrideId);
    if (NOT Descriptor.IsSet())
    { return FText::FromString(TEXT("The behavior provider is no longer registered.")); }

    const auto State = Descriptor->Query();
    return State.Reason.IsEmpty() ? Descriptor->Get_Description() : State.Reason;
}

auto SCkDebug_BehaviorOverrideRow::Get_IsAvailable() const -> bool
{
    const auto Descriptor = FCkDebug_BehaviorOverrideRegistry::Get().Find(_OverrideId);
    return Descriptor.IsSet() && Descriptor->Query().IsAvailable;
}

auto SCkDebug_BehaviorOverrideRow::Get_IsActive() const -> bool
{
    const auto Descriptor = FCkDebug_BehaviorOverrideRegistry::Get().Find(_OverrideId);
    return Descriptor.IsSet() && Descriptor->Query().IsActive;
}

auto SCkDebug_BehaviorOverrideRow::HandleStateChanged(bool InShouldActivate) -> void
{
    const auto Descriptor = FCkDebug_BehaviorOverrideRegistry::Get().Find(_OverrideId);
    if (Descriptor.IsSet())
    {
        const auto Result = Descriptor->Set(InShouldActivate);
        _LastFailure = Result.Succeeded ? FText::GetEmpty() : Result.Reason;
    }
}

// --------------------------------------------------------------------------------------------------------------------
