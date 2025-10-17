#include "CkInspector_EntityInfo.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/OwningActor/CkOwningActor_Utils.h"
#include "CkEcs/Handle/CkHandle_Utils.h"

#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Text/STextBlock.h"

auto FCkInspector_EntityInfo::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Entity Info"));
}

auto FCkInspector_EntityInfo::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity);
}

auto FCkInspector_EntityInfo::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    const auto EntityName = UCk_Utils_Handle_UE::Get_DebugName(Entity);
    const auto EntityID = ck::Format_UE(TEXT("{}"), Entity.Get_Entity());

    auto Grid = SNew(SGridPanel)
        .FillColumn(1, 1.0f);

    int32 Row = 0;

    Grid->AddSlot(0, Row)
        .Padding(4.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Name:")))
        ];

    Grid->AddSlot(1, Row++)
        .Padding(4.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(EntityName.ToString()))
        ];

    Grid->AddSlot(0, Row)
        .Padding(4.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("ID:")))
        ];

    Grid->AddSlot(1, Row++)
        .Padding(4.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(EntityID))
            .ColorAndOpacity(FSlateColor(FLinearColor(0.51f, 0.69f, 1.0f)))
        ];

    if (UCk_Utils_OwningActor_UE::Has(Entity))
    {
        const auto ActorName = ck::Format_UE(TEXT("{}"), UCk_Utils_OwningActor_UE::Get_EntityOwningActor(Entity));

        Grid->AddSlot(0, Row)
            .Padding(4.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Actor:")))
            ];

        Grid->AddSlot(1, Row++)
            .Padding(4.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(ActorName))
            ];
    }
    else
    {
        Grid->AddSlot(0, Row)
            .Padding(4.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Actor:")))
            ];

        Grid->AddSlot(1, Row++)
            .Padding(4.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("None")))
                .ColorAndOpacity(FSlateColor(FLinearColor(0.4f, 0.4f, 0.4f)))
            ];
    }

    return Grid;
}

auto FCkInspector_EntityInfo::Create_PropertyRow(const FText& Label, const FText& Value, const FSlateColor& ValueColor) -> TSharedRef<SWidget>
{
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(4.0f)
        [
            SNew(STextBlock)
            .Text(Label)
            .MinDesiredWidth(120.0f)
        ]
        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .Padding(4.0f)
        [
            SNew(STextBlock)
            .Text(Value)
            .ColorAndOpacity(ValueColor)
        ];
}