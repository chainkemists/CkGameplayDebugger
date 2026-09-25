#include "CkInspector_ProceduralAnimation.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkProceduralAnimation/Debug/CkProceduralAnimation_Debug.h"
#include "CkProceduralAnimation/Gait/CkProceduralGait_Utils.h"
#include "CkProceduralAnimation/Leg/CkProceduralLeg_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"

#include <EngineGlobals.h>

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_ProceduralAnimation)

// --------------------------------------------------------------------------------------------------------------------

namespace ck_inspector_procedural_animation
{
    // One row group owns one cache. Multi-selection diff builds get independent caches, and
    // retained rows keep no additional registry handles. All rows read the same copied solve.
    struct FSnapshotCache
    {
        uint64 Frame = MAX_uint64;
        FCk_ProceduralAnimation_DebugSnapshot Snapshot;

        auto
            Get(
                const FCk_Handle& InEntity)
            -> const FCk_ProceduralAnimation_DebugSnapshot&
        {
            if (ck::Is_NOT_Valid(InEntity))
            {
                Snapshot = {};
                Frame = MAX_uint64;
            }
            else if (Frame != GFrameCounter)
            {
                Snapshot = UCk_Utils_ProceduralAnimation_Debug_UE::Get_Snapshot(InEntity);
                Frame = GFrameCounter;
            }
            return Snapshot;
        }
    };

    auto
        Get_LegRigState(
            const FCk_ProceduralAnimation_DebugLeg& InLeg)
        -> FString
    {
        if (NOT InLeg.Get_Rig().Get_Composed())
        { return TEXT("Not composed"); }

        if (InLeg.Get_Rig().Get_Failure() != ECk_ProceduralRig_Failure::None)
        { return ck::Format_UE(TEXT("Failed: {}"), InLeg.Get_Rig().Get_Failure()); }

        return InLeg.Get_Rig().Get_Ready() ? TEXT("Ready") : TEXT("Waiting for gait");
    }

    auto
        Get_RigState(
            const FCk_Handle& InEntity,
            const FCk_ProceduralAnimation_DebugSnapshot& InSnapshot)
        -> FString
    {
        const auto EntityId = InEntity.Get_Entity().ToString();
        const auto* InspectedLeg = InSnapshot.Get_Legs().FindByPredicate([&](const FCk_ProceduralAnimation_DebugLeg& InLeg)
        {
            return InLeg.Get_LegEntityId() == EntityId;
        });

        if (InspectedLeg != nullptr)
        { return Get_LegRigState(*InspectedLeg); }

        if (NOT InSnapshot.Get_Status().Get_HasRig())
        { return TEXT("Not composed"); }

        auto Rigged = 0;
        auto Ready = 0;
        auto Failed = 0;
        for (const auto& Leg : InSnapshot.Get_Legs())
        {
            if (NOT Leg.Get_Rig().Get_Composed())
            { continue; }

            ++Rigged;
            Ready += Leg.Get_Rig().Get_Ready() ? 1 : 0;
            Failed += Leg.Get_Rig().Get_Failure() != ECk_ProceduralRig_Failure::None ? 1 : 0;
        }

        return Failed > 0
            ? ck::Format_UE(TEXT("{}/{} legs ready, {} failed ({})"), Ready, Rigged, Failed, InSnapshot.Get_Status().Get_RigFailure())
            : ck::Format_UE(TEXT("{}/{} legs ready"), Ready, Rigged);
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkInspector_ProceduralAnimation::
    Get_ComponentName() const
    -> FText
{
    return FText::FromString(TEXT("Procedural Animation"));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkInspector_ProceduralAnimation::
    CanInspect(
        const FCk_Handle& InEntity) const
    -> bool
{
    return ck::IsValid(InEntity)
        && (UCk_Utils_ProceduralGait_UE::Has(InEntity) || UCk_Utils_ProceduralLeg_UE::Has(InEntity));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkInspector_ProceduralAnimation::
    Build_Inspector(
        const FCk_Handle& InEntity)
    -> TSharedRef<SWidget>
{
    return Build_Inspector(InEntity, {});
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkInspector_ProceduralAnimation::
    Build_Inspector(
        const FCk_Handle& InEntity,
        const FString& InFilter)
    -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder{};
    const auto Cache = MakeShared<ck_inspector_procedural_animation::FSnapshotCache>();
    using FReader = TFunction<FString(const FCk_Handle&, const FCk_ProceduralAnimation_DebugSnapshot&)>;
    const auto Add = [&Builder, Cache](const TCHAR* InLabel, FReader InReader, bool InNeedsSample)
    {
        Builder.AddRow(FText::FromString(InLabel),
            [Cache, Reader = MoveTemp(InReader), InNeedsSample](const FCk_Handle& InCurrentEntity)
            {
                const auto& Snapshot = Cache->Get(InCurrentEntity);
                if (NOT Snapshot.Get_Status().Get_Available())
                { return FText::FromString(TEXT("Unavailable")); }

                if (InNeedsSample && NOT Snapshot.Get_Status().Get_HasAcceptedSample())
                { return FText::FromString(TEXT("No accepted solve")); }

                return FText::FromString(Reader(InCurrentEntity, Snapshot));
            });
    };

    constexpr auto NeedsAcceptedSample = true;
    constexpr auto ReadableBeforeFirstSolve = false;

    Add(TEXT("Gait"), [](const FCk_Handle&, const FCk_ProceduralAnimation_DebugSnapshot& InSnapshot)
    {
        return InSnapshot.Get_Status().Get_GaitFailed() ? FString{TEXT("Failed; last accepted solve retained")}
            : InSnapshot.Get_Status().Get_GaitReady() ? FString{TEXT("Ready")}
            : FString{TEXT("Waiting for evaluation")};
    }, ReadableBeforeFirstSolve);

    Add(TEXT("Accepted solve"), [](const FCk_Handle&, const FCk_ProceduralAnimation_DebugSnapshot& InSnapshot)
    {
        return ck::Format_UE(TEXT("#{} at {:.3f}s{}"), InSnapshot.Get_Sample().Get_Sequence(),
            InSnapshot.Get_Sample().Get_Time().Get_Seconds(), InSnapshot.Get_Freshness().Get_GaitFresh() ? TEXT("") : TEXT(" (retained)"));
    }, NeedsAcceptedSample);

    Add(TEXT("Feet"), [](const FCk_Handle&, const FCk_ProceduralAnimation_DebugSnapshot& InSnapshot)
    {
        auto Present = 0;
        auto Enabled = 0;
        auto Planted = 0;
        auto Trusted = 0;
        for (const auto& Leg : InSnapshot.Get_Legs())
        {
            if (Leg.Get_LegEntityId().IsEmpty())
            { continue; }

            ++Present;
            Enabled += Leg.Get_Enabled() ? 1 : 0;
            Planted += Leg.Get_Foot().Get_Planted() ? 1 : 0;
            Trusted += Leg.Get_Foot().Get_ContactTrusted() ? 1 : 0;
        }
        return ck::Format_UE(TEXT("{} legs / {} enabled / {} planted / {} trusted"), Present, Enabled, Planted, Trusted);
    }, NeedsAcceptedSample);

    Add(TEXT("Clock / cadence"), [](const FCk_Handle&, const FCk_ProceduralAnimation_DebugSnapshot& InSnapshot)
    {
        return ck::Format_UE(TEXT("{:.3f} / {:.2f}x"), InSnapshot.Get_Gait().Get_Clock(), InSnapshot.Get_Gait().Get_CadenceScale());
    }, NeedsAcceptedSample);

    Add(TEXT("Solver airborne"), [](const FCk_Handle&, const FCk_ProceduralAnimation_DebugSnapshot& InSnapshot)
    {
        return FString{InSnapshot.Get_Gait().Get_Airborne() ? TEXT("Yes") : TEXT("No")};
    }, NeedsAcceptedSample);

    Add(TEXT("Observed speed"), [](const FCk_Handle&, const FCk_ProceduralAnimation_DebugSnapshot& InSnapshot)
    {
        return ck::Format_UE(TEXT("{:.1f} cm/s"), InSnapshot.Get_Gait().Get_Velocity().Size());
    }, NeedsAcceptedSample);

    Add(TEXT("Surface motion"), [](const FCk_Handle&, const FCk_ProceduralAnimation_DebugSnapshot& InSnapshot)
    {
        if (NOT InSnapshot.Get_Status().Get_HasSurfaceMotion())
        { return FString{TEXT("Not composed")}; }

        if (NOT InSnapshot.Get_Freshness().Get_MotionMatchesGaitFrame())
        { return FString{TEXT("Different update frame")}; }

        return InSnapshot.Get_Motion().Get_TrustedContact() ? FString{TEXT("Current trusted support")}
            : InSnapshot.Get_Motion().Get_Grounded() ? FString{TEXT("Grounded through contact grace")}
            : FString{TEXT("Airborne")};
    }, NeedsAcceptedSample);

    Add(TEXT("Rig"), [](const FCk_Handle& InCurrentEntity, const FCk_ProceduralAnimation_DebugSnapshot& InSnapshot)
    {
        return ck_inspector_procedural_animation::Get_RigState(InCurrentEntity, InSnapshot);
    }, ReadableBeforeFirstSolve);

    Add(TEXT("Rig pose"), [](const FCk_Handle&, const FCk_ProceduralAnimation_DebugSnapshot& InSnapshot)
    {
        if (NOT InSnapshot.Get_Status().Get_HasRig())
        { return FString{TEXT("Not composed")}; }

        if (NOT InSnapshot.Get_Freshness().Get_RigMatchesGaitSequence())
        { return FString{TEXT("Different solve sequence")}; }

        return FString{InSnapshot.Get_Freshness().Get_RigPosePending() ? TEXT("Transform requests pending") : TEXT("Applied")};
    }, NeedsAcceptedSample);

    return Builder.Build(InEntity, InFilter);
}

// --------------------------------------------------------------------------------------------------------------------
