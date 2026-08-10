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

    auto Builder = FCkInspectorWidgetBuilder();
    Builder.SetEditGuard(Get_EditGuard());

    // Recorded here, not at the end: an early-out below would otherwise leave Tick comparing against a
    // stale value and rebuilding every frame.
    _HadInterpolation = UCk_Utils_TransformInterpolation_UE::Has(Entity);

    Builder
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
                }));

    auto MutableEntity = Entity;
    const auto CapturedTransform = UCk_Utils_Transform_UE::Cast(MutableEntity);

    if (ck::Is_NOT_Valid(CapturedTransform))
    { return Builder.Build(Entity); }

    // ---- Edit ----
    //
    // Every row writes through the public Transform Utils with the typed handle captured BY VALUE and
    // re-validated on fire; the read-only rows above stay the display, so nothing here reads a request
    // back. All LocalOk: FProcessor_Transform_HandleRequests runs on every net mode, and a replicated
    // transform simply gets server-stomped on the next update, which is a legitimate local experiment.

    Builder.AddHeader(FText::FromString(TEXT("Edit")));

    // One space for all five verbs below — every Transform request struct carries the same
    // ECk_LocalWorld field, so a single dropdown is honest rather than five identical ones.
    Builder.AddEnumDropdownRow(
        FText::FromString(TEXT("Space:")),
        {
            FText::FromString(TEXT("Local")),
            FText::FromString(TEXT("World"))
        },
        TAttribute<int32>::CreateLambda([Space = _EditSpace]()
        {
            return static_cast<int32>(*Space);
        }),
        [Space = _EditSpace](int32 InIndex)
        {
            *Space = static_cast<ECk_LocalWorld>(InIndex);
        });

    Builder.AddVectorRow(
        FText::FromString(TEXT("Set Location:")),
        TAttribute<FVector>::CreateLambda([CapturedTransform]()
        {
            if (ck::Is_NOT_Valid(CapturedTransform))
            { return FVector::ZeroVector; }

            return UCk_Utils_Transform_UE::Get_EntityCurrentLocation(CapturedTransform);
        }),
        [CapturedTransform, Space = _EditSpace](const FVector& InLocation)
        {
            auto Handle = CapturedTransform;
            if (ck::Is_NOT_Valid(Handle))
            { return; }

            UCk_Utils_Transform_UE::Request_SetLocation(
                Handle,
                FCk_Request_Transform_SetLocation{InLocation}.Set_LocalWorld(*Space),
                {});
        });

    Builder.AddRotatorRow(
        FText::FromString(TEXT("Set Rotation (R,P,Y):")),
        TAttribute<FRotator>::CreateLambda([CapturedTransform]()
        {
            if (ck::Is_NOT_Valid(CapturedTransform))
            { return FRotator::ZeroRotator; }

            return UCk_Utils_Transform_UE::Get_EntityCurrentRotation(CapturedTransform);
        }),
        [CapturedTransform, Space = _EditSpace](const FRotator& InRotation)
        {
            auto Handle = CapturedTransform;
            if (ck::Is_NOT_Valid(Handle))
            { return; }

            UCk_Utils_Transform_UE::Request_SetRotation(
                Handle,
                FCk_Request_Transform_SetRotation{InRotation}.Set_LocalWorld(*Space),
                {});
        });

    // Scale supersedes rather than queues (CkTransform_Utils.h:191) — a second request replaces a
    // still-pending one, which completes Failed. Nothing to code around: the numeric editors commit on
    // enter / lost focus, so one edit produces one request instead of a per-keystroke storm.
    Builder.AddVectorRow(
        FText::FromString(TEXT("Set Scale:")),
        TAttribute<FVector>::CreateLambda([CapturedTransform]()
        {
            if (ck::Is_NOT_Valid(CapturedTransform))
            { return FVector::OneVector; }

            return UCk_Utils_Transform_UE::Get_EntityCurrentScale(CapturedTransform);
        }),
        [CapturedTransform, Space = _EditSpace](const FVector& InScale)
        {
            auto Handle = CapturedTransform;
            if (ck::Is_NOT_Valid(Handle))
            { return; }

            UCk_Utils_Transform_UE::Request_SetScale(
                Handle,
                FCk_Request_Transform_SetScale{InScale}.Set_LocalWorld(*Space),
                {});
        });

    // Offsets are DELTAS, so there is no live value to read back: these two rows hold the delta this
    // inspector will send, and the Apply row is what sends it. Committing a component deliberately does
    // NOT fire — an offset row that fired on every component edit would re-apply the components already
    // typed. The delta is kept after applying, so clicking Apply again steps by the same amount.
    Builder.AddVectorRow(
        FText::FromString(TEXT("Location Offset:")),
        TAttribute<FVector>::CreateLambda([Offset = _LocationOffset]() { return *Offset; }),
        [Offset = _LocationOffset](const FVector& InOffset) { *Offset = InOffset; });

    Builder.AddRotatorRow(
        FText::FromString(TEXT("Rotation Offset (R,P,Y):")),
        TAttribute<FRotator>::CreateLambda([Offset = _RotationOffset]() { return *Offset; }),
        [Offset = _RotationOffset](const FRotator& InOffset) { *Offset = InOffset; });

    Builder.AddActionRow(
        FText::FromString(TEXT("Apply:")),
        {
            FCkInspector_Action
            {
                FText::FromString(TEXT("Add Location")),
                FText::FromString(TEXT("Request_AddLocationOffset with the Location Offset above, in the selected space")),
                [CapturedTransform, Offset = _LocationOffset, Space = _EditSpace]
                {
                    auto Handle = CapturedTransform;
                    if (ck::Is_NOT_Valid(Handle))
                    { return; }

                    UCk_Utils_Transform_UE::Request_AddLocationOffset(
                        Handle,
                        FCk_Request_Transform_AddLocationOffset{*Offset}.Set_LocalWorld(*Space),
                        {});
                }
            },
            FCkInspector_Action
            {
                FText::FromString(TEXT("Add Rotation")),
                FText::FromString(TEXT("Request_AddRotationOffset with the Rotation Offset above, in the selected space")),
                [CapturedTransform, Offset = _RotationOffset, Space = _EditSpace]
                {
                    auto Handle = CapturedTransform;
                    if (ck::Is_NOT_Valid(Handle))
                    { return; }

                    UCk_Utils_Transform_UE::Request_AddRotationOffset(
                        Handle,
                        FCk_Request_Transform_AddRotationOffset{*Offset}.Set_LocalWorld(*Space),
                        {});
                }
            },
            FCkInspector_Action
            {
                FText::FromString(TEXT("Force Refresh")),
                FText::FromString(TEXT("Request_ForceRefresh — re-broadcasts the transform to every listener and sync target")),
                [CapturedTransform]
                {
                    auto Handle = CapturedTransform;
                    if (ck::Is_NOT_Valid(Handle))
                    { return; }

                    UCk_Utils_Transform_UE::Request_ForceRefresh(Handle, {});
                }
            }
        });

    // ---- Interpolation ----
    //
    // Structural: present only while the sibling TransformInterpolation feature is. Tick watches for
    // that flip and asks for a rebuild, which is the only rebuild this inspector requests.
    auto MutableInterpEntity = Entity;
    if (const auto CapturedInterp = UCk_Utils_TransformInterpolation_UE::Cast(MutableInterpEntity);
        ck::IsValid(CapturedInterp))
    {
        Builder.AddHeader(FText::FromString(TEXT("Interpolation")));

        // The goal offsets are write-only in the Utils surface — there is no getter — so these rows
        // display what was last sent from here rather than a live read.
        Builder.AddVectorRow(
            FText::FromString(TEXT("Goal Location Offset:")),
            TAttribute<FVector>::CreateLambda([Goal = _InterpGoalLoc]() { return *Goal; }),
            [CapturedInterp, Goal = _InterpGoalLoc](const FVector& InOffset)
            {
                *Goal = InOffset;

                auto Handle = CapturedInterp;
                if (ck::Is_NOT_Valid(Handle))
                { return; }

                UCk_Utils_TransformInterpolation_UE::Request_SetInterpolationGoal_LocationOffset(Handle, InOffset);
            });

        Builder.AddRotatorRow(
            FText::FromString(TEXT("Goal Rotation Offset (R,P,Y):")),
            TAttribute<FRotator>::CreateLambda([Goal = _InterpGoalRot]() { return *Goal; }),
            [CapturedInterp, Goal = _InterpGoalRot](const FRotator& InOffset)
            {
                *Goal = InOffset;

                auto Handle = CapturedInterp;
                if (ck::Is_NOT_Valid(Handle))
                { return; }

                UCk_Utils_TransformInterpolation_UE::Request_SetInterpolationGoal_RotationOffset(Handle, InOffset);
            });
    }

    return Builder.Build(Entity);
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

    // The only structural change this inspector has: the Interpolation section exists exactly while the
    // sibling feature does. Everything else in the panel is attribute-driven and never needs a rebuild.
    if (const auto HasInterpolation = UCk_Utils_TransformInterpolation_UE::Has(Entity);
        HasInterpolation != _HadInterpolation)
    {
        _HadInterpolation = HasInterpolation;
        RequestRebuild();
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