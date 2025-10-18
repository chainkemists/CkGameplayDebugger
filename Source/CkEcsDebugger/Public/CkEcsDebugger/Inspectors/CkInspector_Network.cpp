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
            .Text(TAttribute<FText>::Create([Entity]()
            {
                if (ck::Is_NOT_Valid(Entity))
                { return FText::GetEmpty(); }

                const auto NetMode = UCk_Utils_Net_UE::Get_EntityNetMode(Entity);
                return FText::FromString(ck::Format_UE(TEXT("{}"), NetMode));
            }))
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
            .Text(TAttribute<FText>::Create([Entity]()
            {
                if (ck::Is_NOT_Valid(Entity))
                { return FText::GetEmpty(); }

                const auto NetRole = UCk_Utils_Net_UE::Get_EntityNetRole(Entity);
                return FText::FromString(ck::Format_UE(TEXT("{}"), NetRole));
            }))
            .ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.8f, 0.01f)))
        ];

    return Grid;
}

auto FCkInspector_Network::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    // No tick logic needed for network info
}