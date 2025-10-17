#include "CkInspector_Network.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Net/CkNet_Utils.h"

#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Text/STextBlock.h"

auto FCkInspector_Network::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Network"));
}

auto FCkInspector_Network::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity);
}

auto FCkInspector_Network::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    const auto NetMode = UCk_Utils_Net_UE::Get_EntityNetMode(Entity);
    const auto NetRole = UCk_Utils_Net_UE::Get_EntityNetRole(Entity);

    const auto NetModeStr = ck::Format_UE(TEXT("{}"), NetMode);
    const auto NetRoleStr = ck::Format_UE(TEXT("{}"), NetRole);

    auto Grid = SNew(SGridPanel)
        .FillColumn(1, 1.0f);

    int32 Row = 0;

    Grid->AddSlot(0, Row)
        .Padding(4.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("NetMode:")))
        ];

    Grid->AddSlot(1, Row++)
        .Padding(4.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(NetModeStr))
            .ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.8f, 0.01f)))
        ];

    Grid->AddSlot(0, Row)
        .Padding(4.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("NetRole:")))
        ];

    Grid->AddSlot(1, Row++)
        .Padding(4.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(NetRoleStr))
            .ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.8f, 0.01f)))
        ];

    return Grid;
}