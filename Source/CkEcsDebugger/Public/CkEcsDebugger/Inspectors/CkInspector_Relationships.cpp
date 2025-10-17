#include "CkInspector_Relationships.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/ContextOwner/CkContextOwner_Utils.h"
#include "CkRelationship/Team/CkTeam_Utils.h"

#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Text/STextBlock.h"

auto FCkInspector_Relationships::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Relationships"));
}

auto FCkInspector_Relationships::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity);
}

auto FCkInspector_Relationships::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Grid = SNew(SGridPanel)
        .FillColumn(1, 1.0f);

    int32 Row = 0;

    // Team Information
    if (const auto TeamEntity = UCk_Utils_Team_UE::Cast(Entity);
        ck::IsValid(TeamEntity))
    {
        const auto TeamID = UCk_Utils_Team_UE::Get_ID(TeamEntity);

        Grid->AddSlot(0, Row)
            .Padding(4.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Team:")))
            ];

        Grid->AddSlot(1, Row++)
            .Padding(4.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(ck::Format_UE(TEXT("{} (Starts from ZERO)"), TeamID)))
                .ColorAndOpacity(FSlateColor(FLinearColor(0.97f, 0.73f, 0.85f)))
            ];
    }
    else
    {
        Grid->AddSlot(0, Row)
            .Padding(4.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Team:")))
            ];

        Grid->AddSlot(1, Row++)
            .Padding(4.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Unknown")))
                .ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.34f, 0.13f)))
            ];
    }

    // Context Owner Information
    if (UCk_Utils_ContextOwner_UE::Has(Entity))
    {
        const auto ContextOwner = UCk_Utils_ContextOwner_UE::Get_ContextOwner(Entity);
        const auto OwnerName = UCk_Utils_Handle_UE::Get_DebugName(ContextOwner);

        Grid->AddSlot(0, Row)
            .Padding(4.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Context Owner:")))
            ];

        Grid->AddSlot(1, Row++)
            .Padding(4.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(ck::Format_UE(TEXT("{} | {}"), OwnerName, ContextOwner)))
                .ColorAndOpacity(FSlateColor(FLinearColor(0.51f, 0.69f, 1.0f)))
            ];
    }
    else
    {
        Grid->AddSlot(0, Row)
            .Padding(4.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Context Owner:")))
            ];

        Grid->AddSlot(1, Row++)
            .Padding(4.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("None")))
                .ColorAndOpacity(FSlateColor(FLinearColor(0.4f, 0.4f, 0.4f)))
            ];
    }

    // Lifetime Owner Information
    if (Entity.Has<ck::FFragment_LifetimeOwner>())
    {
        const auto& LifetimeOwner = Entity.Get<ck::FFragment_LifetimeOwner>().Get_Entity();
        const auto OwnerName = UCk_Utils_Handle_UE::Get_DebugName(LifetimeOwner);

        Grid->AddSlot(0, Row)
            .Padding(4.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Lifetime Owner:")))
            ];

        Grid->AddSlot(1, Row++)
            .Padding(4.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(ck::Format_UE(TEXT("{} | {}"), OwnerName, LifetimeOwner)))
                .ColorAndOpacity(FSlateColor(FLinearColor(0.51f, 0.69f, 1.0f)))
            ];
    }
    else
    {
        Grid->AddSlot(0, Row)
            .Padding(4.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Lifetime Owner:")))
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