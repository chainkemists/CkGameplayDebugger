#include "CkInspector_Transform.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Debug/CkDebugDraw_Utils.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcsExt/Transform/CkTransform_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Text/STextBlock.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Transform)

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
    return FCkInspectorWidgetBuilder()
        .AddRow(
            FText::FromString(TEXT("Location:")),
            [](const FCk_Handle& E)
            {
                if (NOT UCk_Utils_Transform_UE::Has(E)) { return FText::GetEmpty(); }
                return FText::FromString(ck::Format_UE(TEXT("{}"), UCk_Utils_Transform_TypeUnsafe_UE::Get_EntityCurrentTransform(E).GetLocation()));
            },
            FCkDebuggerStyle::Color_Transform)
        .AddRow(
            FText::FromString(TEXT("Rotation:")),
            [](const FCk_Handle& E)
            {
                if (NOT UCk_Utils_Transform_UE::Has(E)) { return FText::GetEmpty(); }
                return FText::FromString(ck::Format_UE(TEXT("{}"), UCk_Utils_Transform_TypeUnsafe_UE::Get_EntityCurrentTransform(E).GetRotation().Rotator()));
            },
            FCkDebuggerStyle::Color_Transform)
        .AddRow(
            FText::FromString(TEXT("Scale:")),
            [](const FCk_Handle& E)
            {
                if (NOT UCk_Utils_Transform_UE::Has(E)) { return FText::GetEmpty(); }
                return FText::FromString(ck::Format_UE(TEXT("{}"), UCk_Utils_Transform_TypeUnsafe_UE::Get_EntityCurrentTransform(E).GetScale3D()));
            },
            FCkDebuggerStyle::Color_Transform)
        .Build(Entity);
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