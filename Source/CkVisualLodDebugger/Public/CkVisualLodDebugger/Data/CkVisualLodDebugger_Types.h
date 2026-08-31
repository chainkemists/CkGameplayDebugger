#pragma once

#include "CkEcs/Handle/CkHandle.h"

#include "CkVisualLod/CkVisualLod_Fragment.h"
#include "CkVisualLod/CkVisualLodArbiter_Fragment_Data.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------
// Plain view structs collected read-only from the world by FCkVisualLodDebugger_DataCollector.
//
// Everything the window paints is already flattened here: tag state is bools, the arbiter's cached
// view is unpacked into a location/forward/cone triple, and the config asset is echoed as scalars.
// The window therefore re-derives nothing per paint, which matters because the meters, sparklines and
// stat cells repaint every frame while the collector only runs on the gated tick.
//
// The one thing these structs DO retain is FCk_Handle: the entity references the window renders as
// clickable pills and hands to the roster. Handles hold the ECS registry by value, so the collector's
// Reset() — called from the window's session/world invalidation reset — is load-bearing, not tidiness.
// --------------------------------------------------------------------------------------------------------------------

// Rendering participation of whatever component is actually drawing a member right now: the promoted
// proxy's base SKMC, or the batched crowd's instanced cluster.
//
// These values come from either the promoted proxy's live primitive or the authored far render profile
// selected by the member's current VisualLod render band. Rendering state remains display-only even though
// the same window can request changes to the arbiter's separate runtime decision tuners.
struct FCkVisualLodDebugger_RenderFlags
{
    bool Resolved = false;

    bool CastShadow                    = false;
    bool AffectDynamicIndirectLighting = false;
    bool AffectDistanceFieldLighting   = false;

    bool LightingChannel0 = true;
    bool LightingChannel1 = false;
    bool LightingChannel2 = false;

    // "SKMC · IskmProxy" / "ISKM · batched crowd 0" — what the reader is actually looking at.
    FString ComponentDesc;

    auto Get_LightingChannelsText() const -> FString
    {
        auto Channels = FString{};
        if (LightingChannel0) { Channels += TEXT("0 "); }
        if (LightingChannel1) { Channels += TEXT("1 "); }
        if (LightingChannel2) { Channels += TEXT("2 "); }
        return Channels.IsEmpty() ? FString(TEXT("none")) : Channels.TrimEnd();
    }
};

// --------------------------------------------------------------------------------------------------------------------

// One authored CkIskmRenderer profile assigned to a crowd's ordered render bands. It is a value
// snapshot: the debugger must not retain a data-asset pointer across a PIE/world teardown.
struct FCkVisualLodDebugger_RenderProfileInfo
{
    int32   ProfileIndex = INDEX_NONE;
    FString ProfileName;

    // Value-only live snapshot. ProfileName is the authored asset identity only; Slate never
    // retains the asset across PIE teardown.
    FCk_IskmRenderer_RuntimeProfileTuners RuntimeTuners;
    FCkVisualLodDebugger_RenderFlags Render;
};

// --------------------------------------------------------------------------------------------------------------------

// One batched crowd pool under an arbiter.
struct FCkVisualLodDebugger_CrowdInfo
{
    int32 CrowdIndex = INDEX_NONE;

    int32 PoolSize  = 0;
    int32 FreeSlots = 0;

    // Live actor state — zero and false while the crowd has not stood up yet (crowds are created
    // lazily, on the first entity that needs a slot), which is a legitimate state and not a fault.
    bool    HasCrowdActor         = false;
    FString CrowdName;
    int32   RenderedInstanceCount = 0;
    int32   TileCount             = 0;
    int32   ProfileBucketCount    = 0;

    // Legacy primitive readout, used only when this crowd has no authored render-band profiles or
    // an individual member's profile cannot be resolved in the collected snapshot.
    FCkVisualLodDebugger_RenderFlags Render;

    // Runtime copy owned by the arbiter. This is deliberately separate from RenderProfiles so
    // crowd animation remains meaningful for legacy crowds without render bands.
    FCk_VisualLod_RuntimeCrowdTuners RuntimeTuners;
    TArray<FCkVisualLodDebugger_RenderProfileInfo> RenderProfiles;

    auto Get_UsedSlots() const -> int32
    {
        return FMath::Max(0, PoolSize - FreeSlots);
    }

    auto Get_IsExhausted() const -> bool
    {
        return PoolSize > 0 && FreeSlots <= 0;
    }

    auto Get_UsedFraction() const -> float
    {
        return PoolSize > 0 ? FMath::Clamp(static_cast<float>(Get_UsedSlots()) / static_cast<float>(PoolSize), 0.0f, 1.0f) : 0.0f;
    }

    auto Get_HasMultipleRenderProfiles() const -> bool
    { return RenderProfiles.Num() > 1; }
};

// --------------------------------------------------------------------------------------------------------------------

// One VisualLod member entity, as of the last gated collect.
struct FCkVisualLodDebugger_MemberInfo
{
    FCk_Handle Entity;
    FString    Name;

    ECk_VisualLod_Representation Representation = ECk_VisualLod_Representation::None;

    // Retained rank inputs (plan D2). -1 distance means "never ranked" — a member that has not yet been
    // seen by an arbiter update, which reads very differently from "ranked at 0".
    float LastDistance = -1.0f;
    bool  LastInView   = false;

    float                     FadeAlpha = 1.0f;
    ck::EVisualLod_FadePhase  FadePhase = ck::EVisualLod_FadePhase::None;

    // INDEX_NONE while the entity holds no crowd member slot.
    int32 SlotIndex = INDEX_NONE;

    // Which of the arbiter's pools the slot belongs to. INDEX_NONE when the member holds no slot, or when its
    // recorded crowd is not one this arbiter reports — the slot index alone is ambiguous across several pools.
    int32 CrowdIndex = INDEX_NONE;

    // INDEX_NONE for legacy crowds with no authored render bands or for a member without a slot.
    int32 RenderBandIndex = INDEX_NONE;

    int32 PromoteLockCount = 0;

    bool Promoted           = false;
    bool PromotedViaLock    = false;
    bool PromotedUnbudgeted = false;
    bool PreemptDemote      = false;
    bool Hidden             = false;

    // Proxy animation cache — meaningful only while promoted.
    int32 ProxySequenceIndex = INDEX_NONE;
    float ProxyRate          = 1.0f;

    // Far (crowd-slot) animation cache — meaningful only while the entity holds a slot.
    int32 FarSequenceIndex = INDEX_NONE;
    float FarRate          = 1.0f;

    // Populated only for promoted members (the proxy's base SKMC).
    FCkVisualLodDebugger_RenderFlags ProxyRender;

    // Authored profile selected for the far crowd representation. This remains meaningful while
    // promoted/fading, so detail shows the profile the member returns to on demotion. For legacy
    // crowds the collector copies the crowd primitive's shared flags into this same value field.
    FCkVisualLodDebugger_RenderFlags FarRender;

    // Where a world marker for this member belongs, resolved by the collector: the rendered crowd instance's
    // transform for a far member, the member entity's own transform otherwise. False when neither resolves.
    bool    HasWorldLocation = false;
    FVector WorldLocation    = FVector::ZeroVector;

    auto Get_IsFading() const -> bool
    {
        return FadePhase != ck::EVisualLod_FadePhase::None;
    }

    // Managed, holding neither a proxy nor a crowd slot, and not deliberately hidden — i.e. nothing is
    // being drawn for this entity, which is the state the Unrendered stat and the pool alert exist for.
    auto Get_IsUnrendered() const -> bool
    {
        return NOT Hidden && Representation == ECk_VisualLod_Representation::None;
    }

    // Which budget the promote is charged against. Reported as text because the three charges are
    // mutually exclusive and the reader wants the name, not three bools.
    auto Get_PromoteChargeText() const -> FString
    {
        if (NOT Promoted)          { return FString(TEXT("—")); }
        if (PromotedViaLock)       { return FString(TEXT("Lock")); }
        if (PromotedUnbudgeted)    { return FString(TEXT("Unbudgeted")); }
        return FString(TEXT("Near"));
    }

    // The roster's Flags cell as one string — also what the filter matches and what a row copy carries, so the user
    // can never filter on something the cell does not show.
    auto Get_FlagsText() const -> FString
    {
        auto Flags = TArray<FString>{};

        if (PromoteLockCount > 0)  { Flags.Add(FString::Printf(TEXT("LOCK×%d"), PromoteLockCount)); }
        if (PromotedUnbudgeted)    { Flags.Add(FString(TEXT("UNBUD"))); }
        if (PreemptDemote)         { Flags.Add(FString(TEXT("PREEMPT"))); }
        if (Hidden)                { Flags.Add(FString(TEXT("HIDDEN"))); }

        return FString::Join(Flags, TEXT(" "));
    }

    // Sort key for the Flags column — most-remarkable first, so one click puts the members a reader is hunting
    // (unbudgeted promotes, preempt victims) at one end of the list.
    auto Get_FlagWeight() const -> int32
    {
        return (PromotedUnbudgeted ? 8 : 0)
            + (PreemptDemote ? 4 : 0)
            + (PromoteLockCount > 0 ? 2 : 0)
            + (Hidden ? 1 : 0);
    }
};

// --------------------------------------------------------------------------------------------------------------------

// One LOD domain: the arbiter entity, its resolved view, its config echo, its pools, and its members.
struct FCkVisualLodDebugger_ArbiterInfo
{
    FCk_Handle Entity;
    FString    Name;

    // The domain identity members resolve their arbiter tag against — also the underline tab's label.
    FString DomainTagName;
    FName   TabId;

    bool Frozen = false;

    int32 PromotedCount           = 0;
    int32 NearPromotedCount       = 0;
    int32 LockedPromotedCount     = 0;
    int32 UnbudgetedPromotedCount = 0;

    // Reset at the top of each arbiter update, so these read 0 on a tick with no flips — and always 0
    // while frozen (plan D1/D4).
    int32 PromotesThisTick = 0;
    int32 DemotesThisTick  = 0;
    int32 PreemptsThisTick = 0;

    // ---- cached local view (plan D3) ----

    bool    ViewValid       = false;
    FVector ViewLocation    = FVector::ZeroVector;
    FVector ViewForward     = FVector::ForwardVector;
    float   ViewCosHalfCone = -1.0f;

    // An explicit observer was set (Request_SetObserver); otherwise the arbiter falls back to local-view
    // discovery, and saying which one produced the numbers is half the diagnosis when they look wrong.
    bool       HasExplicitObserver = false;
    FCk_Handle Observer;

    // ---- config echo ----

    bool    HasConfig = false;
    FString ConfigName;

    // Authored defaults remain separate from the live copy so the tuner popover can show whether
    // this arbiter has session overrides and restore them without mutating the data asset.
    FCk_VisualLodArbiter_RuntimeTuners AuthoredRuntimeTuners;
    FCk_VisualLodArbiter_RuntimeTuners RuntimeTuners;

    float PromoteDistance        = 0.0f;
    float DemoteDistance         = 0.0f;
    float AlwaysInViewDistance   = 0.0f;
    float PreemptDistanceMargin  = 0.0f;
    float LockPromoteMaxDistance = 0.0f;
    float ViewConeMarginDeg      = 0.0f;

    int32 NearBudget         = 0;
    int32 LockBudget         = 0;
    int32 MaxPreemptsPerTick = 0;

    float FadeDurationSeconds = 0.0f;

    ECk_VisualLod_PoolExhaustionPolicy ExhaustionPolicy = ECk_VisualLod_PoolExhaustionPolicy::PromoteInstead;

    // The dithered-crossfade data channels: the crowd instance's per-instance custom data slot and the
    // near mesh's custom primitive data slot. Both carry the same fade alpha; the materials mask
    // complementarily — a material that ignores its channel pops, visibly and by design.
    int32 FadeCrowdSlot = 0;
    int32 FadeNearSlot  = 0;

    TArray<FCkVisualLodDebugger_CrowdInfo>  Crowds;
    TArray<FCkVisualLodDebugger_MemberInfo> Members;

    // ---- derived tallies (the stat strip reads these; nothing else re-walks the member array) ----

    auto Get_CountWhere(TFunctionRef<bool(const FCkVisualLodDebugger_MemberInfo&)> InPredicate) const -> int32
    {
        auto Count = 0;
        for (const auto& Member : Members)
        {
            if (InPredicate(Member))
            { ++Count; }
        }
        return Count;
    }

    auto Get_InViewCount() const -> int32
    { return Get_CountWhere([](const auto& InMember) { return InMember.LastInView; }); }

    auto Get_ProxyCount() const -> int32
    { return Get_CountWhere([](const auto& InMember) { return InMember.Representation == ECk_VisualLod_Representation::PromotedProxy; }); }

    auto Get_FarCount() const -> int32
    { return Get_CountWhere([](const auto& InMember) { return InMember.Representation == ECk_VisualLod_Representation::FarMember; }); }

    auto Get_FadingCount() const -> int32
    { return Get_CountWhere([](const auto& InMember) { return InMember.Get_IsFading(); }); }

    auto Get_HiddenCount() const -> int32
    { return Get_CountWhere([](const auto& InMember) { return InMember.Hidden; }); }

    auto Get_UnrenderedCount() const -> int32
    { return Get_CountWhere([](const auto& InMember) { return InMember.Get_IsUnrendered(); }); }


    auto Get_AnyPoolExhausted() const -> bool
    {
        for (const auto& Crowd : Crowds)
        {
            if (Crowd.Get_IsExhausted())
            { return true; }
        }
        return false;
    }

    auto Get_UsedSlotsTotal() const -> int32
    {
        auto Used = 0;
        for (const auto& Crowd : Crowds)
        { Used += Crowd.Get_UsedSlots(); }
        return Used;
    }

    auto Set_RuntimeTuners(const FCk_VisualLodArbiter_RuntimeTuners& InTuners) -> void
    {
        RuntimeTuners = InTuners;
        PromoteDistance        = InTuners.Get_PromoteDistance();
        DemoteDistance         = InTuners.Get_DemoteDistance();
        AlwaysInViewDistance   = InTuners.Get_AlwaysInViewDistance();
        PreemptDistanceMargin  = InTuners.Get_PreemptDistanceMargin();
        LockPromoteMaxDistance = InTuners.Get_LockPromoteMaxDistance();
        ViewConeMarginDeg      = InTuners.Get_ViewConeMarginDeg();
        NearBudget             = InTuners.Get_NearBudget();
        LockBudget             = InTuners.Get_LockBudget();
        MaxPreemptsPerTick     = InTuners.Get_MaxPreemptsPerTick();
        FadeDurationSeconds    = static_cast<float>(InTuners.Get_FadeDuration().Get_Seconds());
        ExhaustionPolicy       = InTuners.Get_ExhaustionPolicy();
    }

    auto Get_RuntimeTunersDifferFromAuthored() const -> bool
    {
        const auto AreProfilesEqual = [](const FCk_IskmRenderer_RuntimeProfileTuners& A,
                                         const FCk_IskmRenderer_RuntimeProfileTuners& B)
        {
            const auto& AR = A.Get_RenderingInfo();
            const auto& BR = B.Get_RenderingInfo();
            const auto& AC = A.Get_LightingChannels();
            const auto& BC = B.Get_LightingChannels();
            if (AR.Get_bCastDynamicShadow() != BR.Get_bCastDynamicShadow()
                || AR.Get_bRenderInMainPass() != BR.Get_bRenderInMainPass()
                || AR.Get_bRenderInDepthPass() != BR.Get_bRenderInDepthPass()
                || AR.Get_bReceivesDecals() != BR.Get_bReceivesDecals()
                || AR.Get_bUseAsOccluder() != BR.Get_bUseAsOccluder()
                || AR.Get_bRenderCustomDepth() != BR.Get_bRenderCustomDepth()
                || AR.Get_bCastContactShadow() != BR.Get_bCastContactShadow()
                || AR.Get_bAffectDynamicIndirectLighting() != BR.Get_bAffectDynamicIndirectLighting()
                || AR.Get_bAffectDistanceFieldLighting() != BR.Get_bAffectDistanceFieldLighting()
                || AR.Get_bVisibleInRayTracing() != BR.Get_bVisibleInRayTracing()
                || AR.Get_bOutputVelocity() != BR.Get_bOutputVelocity()
                || A.Get_MinDrawDistance() != B.Get_MinDrawDistance()
                || A.Get_MaxDrawDistance() != B.Get_MaxDrawDistance()
                || A.Get_MinLOD() != B.Get_MinLOD()
                || A.Get_BoundsScale() != B.Get_BoundsScale()
                || AC.bChannel0 != BC.bChannel0 || AC.bChannel1 != BC.bChannel1 || AC.bChannel2 != BC.bChannel2
                || A.Get_FarAnimationUpdateInterval().Get_Seconds() != B.Get_FarAnimationUpdateInterval().Get_Seconds()
                || A.Get_FreezeFarAnimation() != B.Get_FreezeFarAnimation())
            { return false; }
            return true;
        };

        const auto AreCrowdsEqual = [&AreProfilesEqual](const FCk_VisualLod_RuntimeCrowdTuners& A,
                                                         const FCk_VisualLod_RuntimeCrowdTuners& B)
        {
            if (A.Get_IdleSequenceIndex() != B.Get_IdleSequenceIndex()
                || A.Get_MoveSequenceIndex() != B.Get_MoveSequenceIndex()
                || A.Get_MoveSpeedThreshold() != B.Get_MoveSpeedThreshold()
                || A.Get_MoveAuthoredSpeed() != B.Get_MoveAuthoredSpeed()
                || A.Get_MoveRateClamp().Get_Min() != B.Get_MoveRateClamp().Get_Min()
                || A.Get_MoveRateClamp().Get_Max() != B.Get_MoveRateClamp().Get_Max()
                || A.Get_RenderBands().Num() != B.Get_RenderBands().Num())
            { return false; }
            for (auto Index = 0; Index < A.Get_RenderBands().Num(); ++Index)
            {
                const auto& ABand = A.Get_RenderBands()[Index];
                const auto& BBand = B.Get_RenderBands()[Index];
                if (ABand.Get_DistanceThreshold() != BBand.Get_DistanceThreshold()
                    || ABand.Get_ReturnHysteresis() != BBand.Get_ReturnHysteresis()
                    || NOT AreProfilesEqual(ABand.Get_ProfileTuners(), BBand.Get_ProfileTuners()))
                { return false; }
            }
            return true;
        };

        if (RuntimeTuners.Get_PromoteDistance() != AuthoredRuntimeTuners.Get_PromoteDistance()
            || RuntimeTuners.Get_DemoteDistance() != AuthoredRuntimeTuners.Get_DemoteDistance()
            || RuntimeTuners.Get_NearBudget() != AuthoredRuntimeTuners.Get_NearBudget()
            || RuntimeTuners.Get_LockBudget() != AuthoredRuntimeTuners.Get_LockBudget()
            || RuntimeTuners.Get_LockPromoteMaxDistance() != AuthoredRuntimeTuners.Get_LockPromoteMaxDistance()
            || RuntimeTuners.Get_ExhaustionPolicy() != AuthoredRuntimeTuners.Get_ExhaustionPolicy()
            || RuntimeTuners.Get_ViewConeMarginDeg() != AuthoredRuntimeTuners.Get_ViewConeMarginDeg()
            || RuntimeTuners.Get_AlwaysInViewDistance() != AuthoredRuntimeTuners.Get_AlwaysInViewDistance()
            || RuntimeTuners.Get_PreemptDistanceMargin() != AuthoredRuntimeTuners.Get_PreemptDistanceMargin()
            || RuntimeTuners.Get_MaxPreemptsPerTick() != AuthoredRuntimeTuners.Get_MaxPreemptsPerTick()
            || RuntimeTuners.Get_FadeDuration().Get_Seconds() != AuthoredRuntimeTuners.Get_FadeDuration().Get_Seconds()
            || RuntimeTuners.Get_FadeAnchorLeadFrames() != AuthoredRuntimeTuners.Get_FadeAnchorLeadFrames()
            || RuntimeTuners.Get_FadeAnchorBakeLagIntervals() != AuthoredRuntimeTuners.Get_FadeAnchorBakeLagIntervals()
            || RuntimeTuners.Get_CrowdTuners().Num() != AuthoredRuntimeTuners.Get_CrowdTuners().Num())
        { return true; }

        for (auto Index = 0; Index < RuntimeTuners.Get_CrowdTuners().Num(); ++Index)
        {
            if (NOT AreCrowdsEqual(RuntimeTuners.Get_CrowdTuners()[Index], AuthoredRuntimeTuners.Get_CrowdTuners()[Index]))
            { return true; }
        }
        return false;
    }

    // Half-angle of the view cone in DEGREES. The arbiter stores the cosine so its per-entity test is a
    // dot-product compare with no trig; the reader wants the angle back.
    auto Get_ViewConeDegrees() const -> float
    {
        const auto Clamped = FMath::Clamp(ViewCosHalfCone, -1.0f, 1.0f);
        return FMath::RadiansToDegrees(FMath::Acos(Clamped)) * 2.0f;
    }
};

// --------------------------------------------------------------------------------------------------------------------

struct FCkVisualLodDebugger_Snapshot
{
    bool  HasWorld    = false;
    int32 NumArbiters = 0;
    int32 NumMembers  = 0;

    // Members whose arbiter has not resolved (bad tag, arbiter not composed yet, or explicitly
    // unmanaged). They belong to no domain tab, so they are counted rather than shown — an unexplained
    // gap between "entities I added" and "members in the tabs" is the thing this number answers.
    int32 NumUnassignedMembers = 0;

    TArray<FCkVisualLodDebugger_ArbiterInfo> Arbiters;  // domain-tag ascending, so tab order is stable

    auto Find_ByTabId(FName InTabId) const -> const FCkVisualLodDebugger_ArbiterInfo*
    {
        for (const auto& Arbiter : Arbiters)
        {
            if (Arbiter.TabId == InTabId)
            { return &Arbiter; }
        }
        return nullptr;
    }
};

// --------------------------------------------------------------------------------------------------------------------
