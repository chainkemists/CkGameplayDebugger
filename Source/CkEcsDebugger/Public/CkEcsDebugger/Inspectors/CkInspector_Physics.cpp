#include "CkInspector_Physics.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkPhysics/Velocity/CkVelocity_Fragment.h"
#include "CkPhysics/Acceleration/CkAcceleration_Fragment.h"
#include "CkPhysics/EulerIntegrator/CkEulerIntegrator_Fragment.h"
#include "CkPhysics/PredictedVelocity/CkPredictedVelocity_Fragment.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Physics)

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
    auto Builder = FCkInspectorWidgetBuilder();

    // ---- Velocity ----
    if (Entity.Has<ck::FFragment_Velocity_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Velocity")));

        const auto Velocity = Entity.Get<ck::FFragment_Velocity_Current>().Get_CurrentVelocity();

        Builder.AddRow(
            FText::FromString(TEXT("Current:")),
            [Velocity](const FCk_Handle&)
            { return FText::FromString(Velocity.ToString()); },
            CkStyle::Value_Math());
    }

    // ---- Acceleration ----
    if (Entity.Has<ck::FFragment_Acceleration_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Acceleration")));

        const auto Accel = Entity.Get<ck::FFragment_Acceleration_Current>().Get_CurrentAcceleration();

        Builder.AddRow(
            FText::FromString(TEXT("Current:")),
            [Accel](const FCk_Handle&)
            { return FText::FromString(Accel.ToString()); },
            CkStyle::Value_Math());
    }

    // ---- Predicted Velocity ----
    if (Entity.Has<ck::FFragment_PredictedVelocity_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Predicted Velocity")));

        const auto& Frag       = Entity.Get<ck::FFragment_PredictedVelocity_Current>();
        const auto  PrevLoc    = Frag.Get_PreviousLocation();
        const auto  CurVel     = Frag.Get_CurrentVelocity();
        const auto  DeltaTime  = Frag.Get_PreviousDeltaTime();

        Builder.AddRow(
            FText::FromString(TEXT("Velocity:")),
            [CurVel](const FCk_Handle&)
            { return FText::FromString(CurVel.ToString()); },
            CkStyle::Value_Math());

        Builder.AddRow(
            FText::FromString(TEXT("Prev Location:")),
            [PrevLoc](const FCk_Handle&)
            { return FText::FromString(PrevLoc.ToString()); },
            CkStyle::Value_Math());

        Builder.AddRow(
            FText::FromString(TEXT("Prev DeltaTime:")),
            [DeltaTime](const FCk_Handle&)
            { return FText::FromString(FString::Printf(TEXT("%.3f s"), DeltaTime.Get_Seconds())); },
            CkStyle::Value_Numeric());
    }

    // ---- Euler Integrator ----
    if (Entity.Has<ck::FFragment_EulerIntegrator_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Euler Integrator")));

        const auto Offset = Entity.Get<ck::FFragment_EulerIntegrator_Current>().Get_DistanceOffset();

        Builder.AddRow(
            FText::FromString(TEXT("Distance Offset:")),
            [Offset](const FCk_Handle&)
            { return FText::FromString(Offset.ToString()); },
            CkStyle::Value_Math());
    }

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto FCkInspector_Physics::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}

// =====================================================================================================================
