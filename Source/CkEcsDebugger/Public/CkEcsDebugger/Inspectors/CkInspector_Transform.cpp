#include "CkInspector_Transform.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Debug/CkDebugDraw_Utils.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcsExt/Transform/CkTransform_Utils.h"

#include "CkDebuggerCommon/Navigation/CkDebug_ViewportView.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_OrientationCube.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Text/STextBlock.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Transform)

// --------------------------------------------------------------------------------------------------------------------

namespace ck_inspector_transform
{
    // Live read of the inspected entity's transform. The handle is captured by value in every row
    // attribute (the inspector pattern — rows are released on rebuild / OnDeactivated), so each
    // read re-validates before touching the registry and falls back to identity otherwise.
    static auto Get_CurrentTransform(
        const FCk_Handle& InEntity)
        -> FTransform
    {
        if (ck::Is_NOT_Valid(InEntity) || NOT UCk_Utils_Transform_UE::Has(InEntity))
        { return FTransform::Identity; }

        return UCk_Utils_Transform_TypeUnsafe_UE::Get_EntityCurrentTransform(InEntity);
    }

    // Three fixed-precision components in X/Y/Z order, so AddAlignedNumericRow's index-based axis
    // coloring lines up with the orientation cube's axis edges.
    static auto Make_AxisComponents(
        const FCk_Handle& InEntity,
        TFunction<FVector(const FTransform&)> InProjector)
        -> TArray<TAttribute<FText>>
    {
        auto Components = TArray<TAttribute<FText>>{};
        Components.Reserve(3);

        for (auto Axis = 0; Axis < 3; ++Axis)
        {
            Components.Emplace(TAttribute<FText>::CreateLambda([InEntity, InProjector, Axis]()
            {
                const auto Value = InProjector(Get_CurrentTransform(InEntity));
                return FText::FromString(ck::Format_UE(TEXT("{:.3f}"), Value[Axis]));
            }));
        }

        return Components;
    }
}

// --------------------------------------------------------------------------------------------------------------------

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
    const auto CapturedEntity = Entity;

    return FCkInspectorWidgetBuilder()
        .AddAlignedNumericRow(
            FText::FromString(TEXT("Location:")),
            ck_inspector_transform::Make_AxisComponents(CapturedEntity,
                [](const FTransform& InTransform) { return InTransform.GetLocation(); }))
        .AddAlignedNumericRow(
            FText::FromString(TEXT("Rotation (R,P,Y):")),
            ck_inspector_transform::Make_AxisComponents(CapturedEntity,
                [](const FTransform& InTransform)
                {
                    // Euler degrees, same values FRotator printed before — reordered to the axis
                    // each angle turns about (Roll=X, Pitch=Y, Yaw=Z) so the row's X/Y/Z coloring
                    // agrees with the cube's axis edges. The label states the order.
                    const auto Rotator = InTransform.GetRotation().Rotator();
                    return FVector{Rotator.Roll, Rotator.Pitch, Rotator.Yaw};
                }))
        .AddAlignedNumericRow(
            FText::FromString(TEXT("Scale:")),
            ck_inspector_transform::Make_AxisComponents(CapturedEntity,
                [](const FTransform& InTransform) { return InTransform.GetScale3D(); }))
        .AddWidgetRow(
            FText::FromString(TEXT("Orientation:")),
            SNew(SCkDebug_OrientationCube)
                .Rotation_Lambda([CapturedEntity]()
                {
                    return ck_inspector_transform::Get_CurrentTransform(CapturedEntity).GetRotation();
                })
                .Scale_Lambda([CapturedEntity]()
                {
                    return ck_inspector_transform::Get_CurrentTransform(CapturedEntity).GetScale3D();
                }))
        .Build(Entity);
}

auto FCkInspector_Transform::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    if (ck::Is_NOT_Valid(Entity) || NOT UCk_Utils_Transform_UE::Has(Entity))
    {
        _Gizmos.Remove(Entity);
        return;
    }

    // Possessed first person + inspecting your own pawn: the triad + floating label
    // sit inside the camera. Suppressed until ejected (mirrors the overlay's
    // self-marker suppression).
    if (ck::DebugViewportView::Get_IsLocalPlayerSelf(Entity))
    {
        _Gizmos.Remove(Entity);
        return;
    }

    const auto& Transform = UCk_Utils_Transform_TypeUnsafe_UE::Get_EntityCurrentTransform(Entity);
    const auto EntityWorld = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(Entity);

    if (ck::Is_NOT_Valid(EntityWorld))
    { return; }

    // Persistent PMG triad instead of per-tick DrawDebugTransformGizmo — one-frame
    // debug lines blink whenever the inspector refresh gate caps below frame rate.
    _Gizmos.UpdateGizmo(EntityWorld, Entity, Transform);

    const auto TextLocation = Transform.GetLocation() + FVector(0.0f, 0.0f, 50.0f);
    UCk_Utils_DebugDraw_UE::DrawDebugString(
        EntityWorld,
        TextLocation,
        Entity.ToString(),
        FLinearColor::White,
        0.0f);
}

auto FCkInspector_Transform::OnDeactivated() -> void
{
    _Gizmos.Reset();
}