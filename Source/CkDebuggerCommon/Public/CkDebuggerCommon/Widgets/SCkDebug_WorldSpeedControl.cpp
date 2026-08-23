#include "SCkDebug_WorldSpeedControl.h"

#include "CkDebuggerCommon/Utils/CkDebug_WorldSpeed.h"

#include "GameFramework/WorldSettings.h"
#include "CkEditorTools/Style/CkStyle.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

auto SCkDebug_WorldSpeedControl::Construct(const FArguments& InArgs) -> void
{
    using FSpeedControl = SSegmentedControl<float>;

    ChildSlot
    [
        SNew(SHorizontalBox)
        .IsEnabled(this, &SCkDebug_WorldSpeedControl::Get_IsEnabled)
        .ToolTipText(this, &SCkDebug_WorldSpeedControl::Get_Tooltip)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceXS, 0.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("World speed")))
            .Font(CkStyle::RegularFont(CkStyle::FontSizeMicro()))
            .ColorAndOpacity(CkStyle::TextMute())
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(FSpeedControl)
                .Value(this, &SCkDebug_WorldSpeedControl::Get_Value)
                .OnValueChanged(this, &SCkDebug_WorldSpeedControl::HandleValueChanged)
                + FSpeedControl::Slot(0.1f).Text(FText::FromString(TEXT("0.1x")))
                + FSpeedControl::Slot(0.25f).Text(FText::FromString(TEXT("0.25x")))
                + FSpeedControl::Slot(0.5f).Text(FText::FromString(TEXT("0.5x")))
                + FSpeedControl::Slot(1.0f).Text(FText::FromString(TEXT("1x")))
        ]
    ];
}

auto SCkDebug_WorldSpeedControl::Get_Value() const -> float
{
    return ck::DebugWorldSpeed::Get_Multiplier().Get(1.0f);
}

auto SCkDebug_WorldSpeedControl::Get_Tooltip() const -> FText
{
    const auto Target = ck::DebugWorldSpeed::Resolve_AuthorityWorld();
    if (NOT Target.CanMutate())
    { return Target.Reason; }

    const auto Multiplier = ck::DebugWorldSpeed::Get_Multiplier().Get(1.0f);
    return FText::Format(
        FText::FromString(TEXT(
            "Authority world speed: {0}x. Changes replicate from WorldSettings to listen-server clients.")),
        FText::AsNumber(Multiplier));
}

auto SCkDebug_WorldSpeedControl::Get_IsEnabled() const -> bool
{
    return ck::DebugWorldSpeed::Resolve_AuthorityWorld().CanMutate();
}

auto SCkDebug_WorldSpeedControl::HandleValueChanged(float InMultiplier) -> void
{
    auto FailureReason = FText::GetEmpty();
    ck::DebugWorldSpeed::Try_SetMultiplier(InMultiplier, FailureReason);
}

// --------------------------------------------------------------------------------------------------------------------
