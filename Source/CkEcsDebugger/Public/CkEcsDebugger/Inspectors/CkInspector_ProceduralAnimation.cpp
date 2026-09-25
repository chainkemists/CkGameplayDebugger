#include "CkInspector_ProceduralAnimation.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkProceduralAnimation/Debug/CkProceduralAnimation_Debug.h"
#include "CkProceduralAnimation/Gait/CkProceduralGait_Utils.h"
#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"

#include <EngineGlobals.h>

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_ProceduralAnimation)

namespace ck_inspector_procedural_animation
{
    // One row group owns one cache. Multi-selection diff builds get independent caches, and
    // retained rows keep no additional registry handles. All rows read the same copied solve.
    struct FSnapshotCache
    {
        uint64 Frame = MAX_uint64;
        FCk_ProceduralAnimation_DebugSnapshot Snapshot;

        auto Get(const FCk_Handle& InEntity) -> const FCk_ProceduralAnimation_DebugSnapshot&
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
}

auto
    FCkInspector_ProceduralAnimation::
    Get_ComponentName() const
    -> FText
{
    return FText::FromString(TEXT("Procedural Animation"));
}

auto
    FCkInspector_ProceduralAnimation::
    CanInspect(
        const FCk_Handle& InEntity) const
    -> bool
{
    return ck::IsValid(InEntity) && UCk_Utils_ProceduralGait_UE::Has(InEntity);
}

auto
    FCkInspector_ProceduralAnimation::
    Build_Inspector(
        const FCk_Handle& InEntity)
    -> TSharedRef<SWidget>
{
    return Build_Inspector(InEntity, {});
}

auto
    FCkInspector_ProceduralAnimation::
    Build_Inspector(
        const FCk_Handle& InEntity,
        const FString& InFilter)
    -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder{};
    const auto Cache = MakeShared<ck_inspector_procedural_animation::FSnapshotCache>();
    using FReader = TFunction<FString(const FCk_ProceduralAnimation_DebugSnapshot&)>;
    const auto Add = [&Builder, Cache](const TCHAR* InLabel, FReader InReader, bool InNeedsSample = true)
    {
        Builder.AddRow(FText::FromString(InLabel),
            [Cache, Reader = MoveTemp(InReader), InNeedsSample](const FCk_Handle& InCurrentEntity)
            {
                const auto& Snapshot = Cache->Get(InCurrentEntity);
                if (NOT Snapshot.Get_Available()) { return FText::FromString(TEXT("Unavailable")); }
                if (InNeedsSample && NOT Snapshot.Get_HasAcceptedSample())
                { return FText::FromString(TEXT("No accepted solve")); }
                return FText::FromString(Reader(Snapshot));
            });
    };
    Add(TEXT("Gait"), [](const auto& InSnapshot)
    {
        return InSnapshot.Get_GaitFailed() ? FString{TEXT("Failed; last accepted solve retained")}
            : InSnapshot.Get_GaitReady() ? FString{TEXT("Ready")}
            : FString{TEXT("Waiting for evaluation")};
    }, false);
    Add(TEXT("Accepted solve"), [](const auto& InSnapshot)
    {
        return ck::Format_UE(TEXT("#{} at {:.3f}s{}"), InSnapshot.Get_Sequence(),
            InSnapshot.Get_Time().Get_Seconds(), InSnapshot.Get_GaitFresh() ? TEXT("") : TEXT(" (retained)"));
    });
    Add(TEXT("Feet"), [](const auto& InSnapshot)
    {
        auto Planted = 0;
        auto Trusted = 0;
        for (const auto& Leg : InSnapshot.Get_Legs())
        {
            Planted += Leg.Get_Planted() ? 1 : 0;
            Trusted += Leg.Get_ContactTrusted() ? 1 : 0;
        }
        return ck::Format_UE(TEXT("{} legs / {} planted / {} trusted"), InSnapshot.Get_Legs().Num(), Planted, Trusted);
    });
    Add(TEXT("Clock / cadence"), [](const auto& InSnapshot)
    {
        return ck::Format_UE(TEXT("{:.3f} / {:.2f}x"), InSnapshot.Get_GaitClock(), InSnapshot.Get_CadenceScale());
    });
    Add(TEXT("Solver airborne"), [](const auto& InSnapshot)
    { return FString{InSnapshot.Get_Airborne() ? TEXT("Yes") : TEXT("No")}; });
    Add(TEXT("Observed speed"), [](const auto& InSnapshot)
    { return ck::Format_UE(TEXT("{:.1f} cm/s"), InSnapshot.Get_Velocity().Size()); });
    Add(TEXT("Surface motion"), [](const auto& InSnapshot)
    {
        if (NOT InSnapshot.Get_HasSurfaceMotion()) { return FString{TEXT("Not composed")}; }
        if (NOT InSnapshot.Get_MotionMatchesGaitFrame()) { return FString{TEXT("Different update frame")}; }
        return InSnapshot.Get_TrustedContact() ? FString{TEXT("Current trusted support")}
            : InSnapshot.Get_Grounded() ? FString{TEXT("Grounded through contact grace")}
            : FString{TEXT("Airborne")};
    });
    Add(TEXT("Rig"), [](const auto& InSnapshot)
    {
        if (NOT InSnapshot.Get_HasRig()) { return FString{TEXT("Not composed")}; }
        if (InSnapshot.Get_RigFailure() != ECk_ProceduralRig_Failure::None)
        { return ck::Format_UE(TEXT("Failed: {}"), InSnapshot.Get_RigFailure()); }
        return InSnapshot.Get_RigReady() ? FString{TEXT("Ready")} : FString{TEXT("Waiting for gait")};
    }, false);
    Add(TEXT("Rig pose"), [](const auto& InSnapshot)
    {
        if (NOT InSnapshot.Get_HasRig()) { return FString{TEXT("Not composed")}; }
        if (NOT InSnapshot.Get_RigMatchesGaitSequence()) { return FString{TEXT("Different solve sequence")}; }
        return FString{InSnapshot.Get_RigPosePending() ? TEXT("Transform requests pending") : TEXT("Applied")};
    });
    return Builder.Build(InEntity, InFilter);
}
