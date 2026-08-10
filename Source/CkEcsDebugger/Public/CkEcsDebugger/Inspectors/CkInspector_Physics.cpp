#include "CkInspector_Physics.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkPhysics/Velocity/CkVelocity_Fragment.h"
#include "CkPhysics/Velocity/CkVelocity_Utils.h"
#include "CkPhysics/Acceleration/CkAcceleration_Fragment.h"
#include "CkPhysics/Acceleration/CkAcceleration_Utils.h"
#include "CkPhysics/EulerIntegrator/CkEulerIntegrator_Fragment.h"
#include "CkPhysics/EulerIntegrator/CkEulerIntegrator_Utils.h"
#include "CkPhysics/PredictedVelocity/CkPredictedVelocity_Fragment.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Physics)

// =====================================================================================================================

namespace ck_inspector_physics
{
    // The handle is captured by value in every row attribute (rows are released on rebuild /
    // OnDeactivated), so each read re-validates before touching the registry — the physics
    // fragments are written every tick and the previous snapshot-at-build rows never moved.
    static auto Get_Velocity(const FCk_Handle& InEntity) -> FVector
    {
        if (ck::Is_NOT_Valid(InEntity) || NOT InEntity.Has<ck::FFragment_Velocity_Current>())
        { return FVector::ZeroVector; }

        return InEntity.Get<ck::FFragment_Velocity_Current>().Get_CurrentVelocity();
    }

    static auto Get_Acceleration(const FCk_Handle& InEntity) -> FVector
    {
        if (ck::Is_NOT_Valid(InEntity) || NOT InEntity.Has<ck::FFragment_Acceleration_Current>())
        { return FVector::ZeroVector; }

        return InEntity.Get<ck::FFragment_Acceleration_Current>().Get_CurrentAcceleration();
    }

    static auto Get_PredictedVelocity(const FCk_Handle& InEntity) -> FVector
    {
        if (ck::Is_NOT_Valid(InEntity) || NOT InEntity.Has<ck::FFragment_PredictedVelocity_Current>())
        { return FVector::ZeroVector; }

        return InEntity.Get<ck::FFragment_PredictedVelocity_Current>().Get_CurrentVelocity();
    }

    static auto Get_PredictedPreviousLocation(const FCk_Handle& InEntity) -> FVector
    {
        if (ck::Is_NOT_Valid(InEntity) || NOT InEntity.Has<ck::FFragment_PredictedVelocity_Current>())
        { return FVector::ZeroVector; }

        return InEntity.Get<ck::FFragment_PredictedVelocity_Current>().Get_PreviousLocation();
    }

    static auto Get_DistanceOffset(const FCk_Handle& InEntity) -> FVector
    {
        if (ck::Is_NOT_Valid(InEntity) || NOT InEntity.Has<ck::FFragment_EulerIntegrator_Current>())
        { return FVector::ZeroVector; }

        return InEntity.Get<ck::FFragment_EulerIntegrator_Current>().Get_DistanceOffset();
    }

    // Three fixed-precision components in X/Y/Z order — same 3 decimals FVector::ToString printed —
    // so AddAlignedNumericRow's index-based axis coloring lines up with the Transform inspector.
    static auto Make_AxisComponents(
        const FCk_Handle& InEntity,
        TFunction<FVector(const FCk_Handle&)> InProjector)
        -> TArray<TAttribute<FText>>
    {
        auto Components = TArray<TAttribute<FText>>{};
        Components.Reserve(3);

        for (auto Axis = 0; Axis < 3; ++Axis)
        {
            Components.Emplace(TAttribute<FText>::CreateLambda([InEntity, InProjector, Axis]()
            {
                return FText::FromString(ck::Format_UE(TEXT("{:.3f}"), InProjector(InEntity)[Axis]));
            }));
        }

        return Components;
    }

    static auto Make_SpeedText(
        const FCk_Handle& InEntity,
        TFunction<FVector(const FCk_Handle&)> InProjector)
        -> TAttribute<FText>
    {
        return TAttribute<FText>::CreateLambda([InEntity, InProjector]()
        {
            return FText::FromString(ck::Format_UE(TEXT("{:.2f}"), InProjector(InEntity).Size()));
        });
    }

    static auto Make_SpeedSample(
        const FCk_Handle& InEntity,
        TFunction<FVector(const FCk_Handle&)> InProjector)
        -> TAttribute<float>
    {
        return TAttribute<float>::CreateLambda([InEntity, InProjector]()
        {
            return static_cast<float>(InProjector(InEntity).Size());
        });
    }
}

// =====================================================================================================================

auto FCkInspector_Physics::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Physics"));
}

auto FCkInspector_Physics::CanInspect(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity))
    { return false; }

    return Entity.Has_Any<
        ck::FFragment_Velocity_Current,
        ck::FFragment_Acceleration_Current,
        ck::FFragment_EulerIntegrator_Current,
        ck::FFragment_PredictedVelocity_Current>();
}

// =====================================================================================================================

auto FCkInspector_Physics::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    namespace inspector = ck_inspector_physics;

    auto Builder = FCkInspectorWidgetBuilder();
    Builder.SetEditGuard(Get_EditGuard());

    const auto CapturedEntity = Entity;

    // ---- Velocity ----
    if (Entity.Has<ck::FFragment_Velocity_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Velocity")));

        Builder.AddAlignedNumericRow(
            FText::FromString(TEXT("Current:")),
            inspector::Make_AxisComponents(CapturedEntity, &inspector::Get_Velocity));

        Builder.AddSparklineRow(
            FText::FromString(TEXT("Speed:")),
            inspector::Make_SpeedSample(CapturedEntity, &inspector::Get_Velocity),
            ECk_Tone::Accent,
            inspector::Make_SpeedText(CapturedEntity, &inspector::Get_Velocity));

        // Editable override on top of the live read-out above. AuthorityOnly: the velocity pipeline's
        // bulk-modifier processors are authority-gated, so a client-side override is stomped by the
        // next replicated write — grey it with the reason rather than let it look like it took.
        auto MutableVelocityEntity = Entity;
        const auto CapturedVelocity = UCk_Utils_Velocity_UE::Cast(MutableVelocityEntity);

        if (ck::IsValid(CapturedVelocity))
        {
            Builder.AddVectorRow(
                FText::FromString(TEXT("Override:")),
                TAttribute<FVector>::CreateLambda([CapturedVelocity]()
                {
                    if (ck::Is_NOT_Valid(CapturedVelocity)) { return FVector::ZeroVector; }
                    return UCk_Utils_Velocity_UE::Get_CurrentVelocity(CapturedVelocity);
                }),
                [CapturedVelocity](const FVector& InVelocity)
                {
                    auto MutableVelocity = CapturedVelocity;
                    if (ck::Is_NOT_Valid(MutableVelocity)) { return; }
                    UCk_Utils_Velocity_UE::Request_OverrideVelocity(MutableVelocity, InVelocity, {});
                },
                ECk_DebugRequest_Requirement::AuthorityOnly);
        }
    }

    // ---- Acceleration ----
    if (Entity.Has<ck::FFragment_Acceleration_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Acceleration")));

        Builder.AddAlignedNumericRow(
            FText::FromString(TEXT("Current:")),
            inspector::Make_AxisComponents(CapturedEntity, &inspector::Get_Acceleration));

        auto MutableAccelerationEntity = Entity;
        const auto CapturedAcceleration = UCk_Utils_Acceleration_UE::Cast(MutableAccelerationEntity);

        if (ck::IsValid(CapturedAcceleration))
        {
            Builder.AddVectorRow(
                FText::FromString(TEXT("Override:")),
                TAttribute<FVector>::CreateLambda([CapturedAcceleration]()
                {
                    if (ck::Is_NOT_Valid(CapturedAcceleration)) { return FVector::ZeroVector; }
                    return UCk_Utils_Acceleration_UE::Get_CurrentAcceleration(CapturedAcceleration);
                }),
                [CapturedAcceleration](const FVector& InAcceleration)
                {
                    auto MutableAcceleration = CapturedAcceleration;
                    if (ck::Is_NOT_Valid(MutableAcceleration)) { return; }
                    UCk_Utils_Acceleration_UE::Request_OverrideAcceleration(MutableAcceleration, InAcceleration, {});
                },
                ECk_DebugRequest_Requirement::AuthorityOnly);
        }
    }

    // ---- Predicted Velocity ----
    if (Entity.Has<ck::FFragment_PredictedVelocity_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Predicted Velocity")));

        Builder.AddAlignedNumericRow(
            FText::FromString(TEXT("Velocity:")),
            inspector::Make_AxisComponents(CapturedEntity, &inspector::Get_PredictedVelocity));

        Builder.AddSparklineRow(
            FText::FromString(TEXT("Speed:")),
            inspector::Make_SpeedSample(CapturedEntity, &inspector::Get_PredictedVelocity),
            ECk_Tone::Info,
            inspector::Make_SpeedText(CapturedEntity, &inspector::Get_PredictedVelocity));

        Builder.AddAlignedNumericRow(
            FText::FromString(TEXT("Prev Location:")),
            inspector::Make_AxisComponents(CapturedEntity, &inspector::Get_PredictedPreviousLocation));

        Builder.AddRow(
            FText::FromString(TEXT("Prev DeltaTime:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_PredictedVelocity_Current>())
                { return FText::FromString(TEXT("--")); }

                const auto DeltaTime = CapturedEntity.Get<ck::FFragment_PredictedVelocity_Current>().Get_PreviousDeltaTime();
                return FText::FromString(FString::Printf(TEXT("%.3f s"), DeltaTime.Get_Seconds()));
            },
            CkStyle::Value_Numeric());
    }

    // ---- Euler Integrator ----
    if (Entity.Has<ck::FFragment_EulerIntegrator_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Euler Integrator")));

        Builder.AddAlignedNumericRow(
            FText::FromString(TEXT("Distance Offset:")),
            inspector::Make_AxisComponents(CapturedEntity, &inspector::Get_DistanceOffset));

        // LocalOk: the Euler start/stop requests carry no authority gate — they toggle whether this
        // entity's own integrator ticks, which is a legitimate local experiment on any world.
        Builder.AddActionRow(
            FText::FromString(TEXT("Integrator:")),
            {
                FCkInspector_Action
                {
                    FText::FromString(TEXT("Start")),
                    FText::FromString(TEXT("UCk_Utils_EulerIntegrator_UE::Request_Start")),
                    [CapturedEntity]()
                    {
                        auto MutableEntity = CapturedEntity;
                        if (ck::Is_NOT_Valid(MutableEntity)) { return; }
                        UCk_Utils_EulerIntegrator_UE::Request_Start(MutableEntity, {});
                    },
                    ECk_DebugRequest_Requirement::LocalOk
                },
                FCkInspector_Action
                {
                    FText::FromString(TEXT("Stop")),
                    FText::FromString(TEXT("UCk_Utils_EulerIntegrator_UE::Request_Stop")),
                    [CapturedEntity]()
                    {
                        auto MutableEntity = CapturedEntity;
                        if (ck::Is_NOT_Valid(MutableEntity)) { return; }
                        UCk_Utils_EulerIntegrator_UE::Request_Stop(MutableEntity, {});
                    },
                    ECk_DebugRequest_Requirement::LocalOk
                },
            });
    }

    // NO per-modifier Remove rows: this inspector has never listed velocity/acceleration modifiers,
    // and there is no public enumeration to build such a list from — RecordOfVelocityModifiers_Utils /
    // RecordOfAccelerationModifiers_Utils are PRIVATE members of their Utils classes, and
    // UCk_Utils_*Modifier_UE::Remove is addressed by a FGameplayTag the debugger cannot discover.
    // Deferred until the modifier record gets a public ForEach.

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto FCkInspector_Physics::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}

// =====================================================================================================================
