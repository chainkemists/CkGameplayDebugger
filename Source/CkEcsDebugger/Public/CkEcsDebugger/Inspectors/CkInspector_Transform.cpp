#include "CkInspector_Transform.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Debug/CkDebugDraw_Utils.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcsExt/Transform/CkTransform_Utils.h"

#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Text/STextBlock.h"

auto FCkInspector_Transform::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Transform"));
}

auto FCkInspector_Transform::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity) && UCk_Utils_Transform_UE::Has(Entity);
}

auto FCkInspector_Transform::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Grid = SNew(SGridPanel)
        .FillColumn(1, 1.0f);

    int32 Row = 0;

    Grid->AddSlot(0, Row)
        .Padding(4.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Location:")))
        ];

    Grid->AddSlot(1, Row++)
        .Padding(4.0f)
        [
            SNew(STextBlock)
            .Text(TAttribute<FText>::Create([Entity]()
            {
                if (ck::Is_NOT_Valid(Entity) || NOT UCk_Utils_Transform_UE::Has(Entity))
                { return FText::GetEmpty(); }

                const auto& Transform = UCk_Utils_Transform_TypeUnsafe_UE::Get_EntityCurrentTransform(Entity);
                return FText::FromString(ck::Format_UE(TEXT("{}"), Transform.GetLocation()));
            }))
            .ColorAndOpacity(FSlateColor(FLinearColor(0.76f, 0.91f, 0.55f)))
        ];

    Grid->AddSlot(0, Row)
        .Padding(4.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Rotation:")))
        ];

    Grid->AddSlot(1, Row++)
        .Padding(4.0f)
        [
            SNew(STextBlock)
            .Text(TAttribute<FText>::Create([Entity]()
            {
                if (ck::Is_NOT_Valid(Entity) || NOT UCk_Utils_Transform_UE::Has(Entity))
                { return FText::GetEmpty(); }

                const auto& Transform = UCk_Utils_Transform_TypeUnsafe_UE::Get_EntityCurrentTransform(Entity);
                return FText::FromString(ck::Format_UE(TEXT("{}"), Transform.GetRotation().Rotator()));
            }))
            .ColorAndOpacity(FSlateColor(FLinearColor(0.76f, 0.91f, 0.55f)))
        ];

    Grid->AddSlot(0, Row)
        .Padding(4.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Scale:")))
        ];

    Grid->AddSlot(1, Row++)
        .Padding(4.0f)
        [
            SNew(STextBlock)
            .Text(TAttribute<FText>::Create([Entity]()
            {
                if (ck::Is_NOT_Valid(Entity) || NOT UCk_Utils_Transform_UE::Has(Entity))
                { return FText::GetEmpty(); }

                const auto& Transform = UCk_Utils_Transform_TypeUnsafe_UE::Get_EntityCurrentTransform(Entity);
                return FText::FromString(ck::Format_UE(TEXT("{}"), Transform.GetScale3D()));
            }))
            .ColorAndOpacity(FSlateColor(FLinearColor(0.76f, 0.91f, 0.55f)))
        ];

    return Grid;
}

auto FCkInspector_Transform::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    if (ck::Is_NOT_Valid(Entity) || NOT UCk_Utils_Transform_UE::Has(Entity))
    { return; }

    const auto& Transform = UCk_Utils_Transform_TypeUnsafe_UE::Get_EntityCurrentTransform(Entity);
    const auto EntityWorld = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(Entity);

    if (ck::Is_NOT_Valid(EntityWorld))
    { return; }

    UCk_Utils_DebugDraw_UE::DrawDebugTransformGizmo(EntityWorld, Transform);

    const auto TextLocation = Transform.GetLocation() + FVector(0.0f, 0.0f, 50.0f);
    UCk_Utils_DebugDraw_UE::DrawDebugString(
        EntityWorld,
        TextLocation,
        Entity.ToString(),
        FLinearColor::White,
        0.0f);
}