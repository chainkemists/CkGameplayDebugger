#include "CkVisualLodDebugger/Data/CkVisualLodDebugger_DataCollector.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/Handle/CkHandle.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"

#include "CkEcsExt/Transform/CkTransform_Utils.h"

#include "CkIskmRenderer/Proxy/CkIskmProxy_Fragment.h"
#include "CkIskmRenderer/Renderer/CkIskm_BatchedCrowd_Actor.h"

#include "CkVisualLod/CkVisualLod_Fragment.h"
#include "CkVisualLod/CkVisualLod_Utils.h"
#include "CkVisualLod/CkVisualLodArbiter_Fragment.h"
#include "CkVisualLod/CkVisualLodArbiter_Utils.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_visuallod_debugger_collector
{
    auto DoRead_RenderFlags(
        const UPrimitiveComponent* InComponent,
        const FString&             InDesc)
        -> FCkVisualLodDebugger_RenderFlags
    {
        auto Flags = FCkVisualLodDebugger_RenderFlags{};

        if (ck::Is_NOT_Valid(InComponent))
        { return Flags; }

        Flags.Resolved                      = true;
        Flags.ComponentDesc                 = InDesc;
        Flags.CastShadow                    = InComponent->CastShadow;
        Flags.AffectDynamicIndirectLighting = InComponent->bAffectDynamicIndirectLighting;
        Flags.AffectDistanceFieldLighting   = InComponent->bAffectDistanceFieldLighting;
        Flags.LightingChannel0              = InComponent->LightingChannels.bChannel0;
        Flags.LightingChannel1              = InComponent->LightingChannels.bChannel1;
        Flags.LightingChannel2              = InComponent->LightingChannels.bChannel2;

        return Flags;
    }

    // The crowd actor's root is a plain USceneComponent, so every PRIMITIVE it owns is one of the batched
    // cluster components — the spatial tiles, plus the custom-depth highlight clusters when outlines are
    // in use. The manager applies one rendering setup to every tile it creates, so the first primitive is
    // a faithful readout for the whole crowd, and this reads it through UPrimitiveComponent rather than
    // the cluster type so the debugger does not couple to that component's internals.
    auto DoRead_CrowdRenderFlags(
        const ACk_Iskm_BatchedCrowd_Actor* InCrowd,
        int32                              InCrowdIndex)
        -> FCkVisualLodDebugger_RenderFlags
    {
        if (ck::Is_NOT_Valid(InCrowd))
        { return {}; }

        auto Primitives = TInlineComponentArray<UPrimitiveComponent*>{};
        InCrowd->GetComponents(Primitives);

        for (const auto* Primitive : Primitives)
        {
            if (ck::Is_NOT_Valid(Primitive))
            { continue; }

            return DoRead_RenderFlags(Primitive, FString::Printf(TEXT("ISKM · batched crowd %d"), InCrowdIndex));
        }

        return {};
    }

    // The promoted proxy's base SKMC is the component actually drawing a near member.
    //
    // Read off the fragment rather than through UCk_Utils_IskmProxy_UE: that Utils class exposes no
    // component accessor, deliberately (CkIskmProxy_Fragment.h asks callers not to leak _BaseSKMC so the
    // public API stays implementable from the Plan-2 SOA shape). This is the same read-the-fragment-for-
    // pure-debug-state exception CkAggroDebugger's collector takes, and it is confined to this one
    // display readout. A CkIskmRenderer-side "which component draws this proxy" getter would remove it.
    auto DoRead_ProxyRenderFlags(
        FCk_Handle_IskmProxy InProxy)
        -> FCkVisualLodDebugger_RenderFlags
    {
        if (ck::Is_NOT_Valid(InProxy))
        { return {}; }

        if (NOT InProxy.Has<ck::FFragment_IskmProxy_Current>())
        { return {}; }

        const auto Skmc = InProxy.Get<ck::FFragment_IskmProxy_Current>().Get_BaseSKMC();
        if (ck::Is_NOT_Valid(Skmc))
        { return {}; }

        return DoRead_RenderFlags(Skmc.Get(), FString(TEXT("SKMC · IskmProxy pool")));
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkVisualLodDebugger_DataCollector::
    Collect(
        UWorld* InWorld)
    -> void
{
    _Snapshot = FCkVisualLodDebugger_Snapshot{};

    if (ck::Is_NOT_Valid(InWorld))
    { return; }

    auto TransientEntity = UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(InWorld);
    if (ck::Is_NOT_Valid(TransientEntity))
    { return; }

    _Snapshot.HasWorld = true;

    // Arbiter → index in _Snapshot.Arbiters, so the member pass binds in one hash lookup instead of a
    // linear scan per member (a domain can hold hundreds).
    auto ArbiterIndexByHash = TMap<uint32, int32>{};

    // Crowd actor → its index under its owning arbiter. A batched crowd belongs to exactly one arbiter, so one flat
    // map is unambiguous; this is a raw pointer keyed local to Collect and is never retained past this call.
    auto CrowdIndexByActor = TMap<const ACk_Iskm_BatchedCrowd_Actor*, int32>{};

    // Same iteration shape as FProcessor_VisualLodArbiter_Update::DoTick: the params/current pair plus
    // CK_IGNORE_PENDING_KILL, skipping arbiters that have not finished setup — an arbiter mid-setup has
    // no resolved config, no pools, and no view, and rendering it would read as a broken domain.
    TransientEntity.View<ck::FFragment_VisualLodArbiter_Params, ck::FFragment_VisualLodArbiter_Current, CK_IGNORE_PENDING_KILL>().ForEach(
        [&](FCk_Entity InEntity, const ck::FFragment_VisualLodArbiter_Params&, const ck::FFragment_VisualLodArbiter_Current& InCurrent)
        {
            auto Generic = ck::MakeHandle(InEntity, TransientEntity);
            if (ck::Is_NOT_Valid(Generic))
            { return; }

            if (Generic.Has<ck::FTag_VisualLodArbiter_NeedsSetup>())
            { return; }

            const auto Arbiter = UCk_Utils_VisualLodArbiter_UE::Cast(Generic);
            if (ck::Is_NOT_Valid(Arbiter))
            { return; }

            auto Info = FCkVisualLodDebugger_ArbiterInfo{};
            Info.Entity = Generic;
            Info.Name   = UCk_Utils_Handle_UE::Get_DebugName(Generic).ToString();

            Info.Frozen = UCk_Utils_VisualLodArbiter_UE::Get_IsFrozen(Arbiter);

            Info.PromotedCount           = UCk_Utils_VisualLodArbiter_UE::Get_PromotedCount(Arbiter);
            Info.NearPromotedCount       = UCk_Utils_VisualLodArbiter_UE::Get_NearPromotedCount(Arbiter);
            Info.LockedPromotedCount     = UCk_Utils_VisualLodArbiter_UE::Get_LockedPromotedCount(Arbiter);
            Info.UnbudgetedPromotedCount = UCk_Utils_VisualLodArbiter_UE::Get_UnbudgetedPromotedCount(Arbiter);

            Info.PromotesThisTick = UCk_Utils_VisualLodArbiter_UE::Get_PromotesThisTick(Arbiter);
            Info.DemotesThisTick  = UCk_Utils_VisualLodArbiter_UE::Get_DemotesThisTick(Arbiter);
            Info.PreemptsThisTick = UCk_Utils_VisualLodArbiter_UE::Get_PreemptsThisTick(Arbiter);

            const auto View = UCk_Utils_VisualLodArbiter_UE::Get_LastView(Arbiter);
            Info.ViewValid       = View._IsValid;
            Info.ViewLocation    = View._Location;
            Info.ViewForward     = View._Forward;
            Info.ViewCosHalfCone = View._CosHalfCone;

            Info.Observer            = UCk_Utils_VisualLodArbiter_UE::Get_Observer(Arbiter);
            Info.HasExplicitObserver = ck::IsValid(Info.Observer);

            // No Utils getter for the resolved config asset — the arbiter's public surface exposes the
            // derived counts, not the asset it derived them from. The fragment getter is public and this
            // is a read-only echo of authored values.
            if (const auto* Config = InCurrent.Get_Config().Get())
            {
                Info.HasConfig     = true;
                Info.ConfigName    = Config->GetName();
                Info.DomainTagName = Config->Get_DomainTag().ToString();

                Info.PromoteDistance        = Config->Get_PromoteDistance();
                Info.DemoteDistance         = Config->Get_DemoteDistance();
                Info.AlwaysInViewDistance   = Config->Get_AlwaysInViewDistance();
                Info.PreemptDistanceMargin  = Config->Get_PreemptDistanceMargin();
                Info.LockPromoteMaxDistance = Config->Get_LockPromoteMaxDistance();
                Info.ViewConeMarginDeg      = Config->Get_ViewConeMarginDeg();

                Info.NearBudget         = Config->Get_NearBudget();
                Info.LockBudget         = Config->Get_LockBudget();
                Info.MaxPreemptsPerTick = Config->Get_MaxPreemptsPerTick();

                Info.FadeDurationSeconds = static_cast<float>(Config->Get_FadeDuration().Get_Seconds());
                Info.ExhaustionPolicy    = Config->Get_ExhaustionPolicy();

                Info.FadeCrowdSlot = Config->Get_FadeCustomDataSlot();
                Info.FadeNearSlot  = Config->Get_FadeNearCustomPrimitiveDataSlot();
            }

            if (Info.DomainTagName.IsEmpty())
            { Info.DomainTagName = FString(TEXT("(no domain tag)")); }

            Info.TabId = FName(*Info.DomainTagName);

            const auto NumCrowds = UCk_Utils_VisualLodArbiter_UE::Get_NumCrowds(Arbiter);
            for (auto CrowdIndex = 0; CrowdIndex < NumCrowds; ++CrowdIndex)
            {
                const auto PoolInfo = UCk_Utils_VisualLodArbiter_UE::Get_CrowdPoolDebugInfo(Arbiter, CrowdIndex);

                auto CrowdInfo = FCkVisualLodDebugger_CrowdInfo{};
                CrowdInfo.CrowdIndex = CrowdIndex;
                CrowdInfo.PoolSize   = PoolInfo.PoolSize;
                CrowdInfo.FreeSlots  = PoolInfo.FreeSlots;

                if (const auto* Crowd = PoolInfo.Crowd.Get())
                {
                    CrowdIndexByActor.Add(Crowd, CrowdIndex);

                    CrowdInfo.HasCrowdActor         = true;
                    CrowdInfo.CrowdName             = Crowd->GetName();
                    CrowdInfo.RenderedInstanceCount = Crowd->Get_RenderedInstanceCount();
                    CrowdInfo.TileCount             = Crowd->Get_TileCount();
                    CrowdInfo.Render                = ck_visuallod_debugger_collector::DoRead_CrowdRenderFlags(Crowd, CrowdIndex);
                }

                Info.Crowds.Add(MoveTemp(CrowdInfo));
            }

            ArbiterIndexByHash.Add(GetTypeHash(Generic), _Snapshot.Arbiters.Num());
            _Snapshot.Arbiters.Add(MoveTemp(Info));
        });

    TransientEntity.View<ck::FFragment_VisualLod_Params, ck::FFragment_VisualLod_Current, CK_IGNORE_PENDING_KILL>().ForEach(
        [&](FCk_Entity InEntity, const ck::FFragment_VisualLod_Params&, const ck::FFragment_VisualLod_Current& InCurrent)
        {
            auto Generic = ck::MakeHandle(InEntity, TransientEntity);
            if (ck::Is_NOT_Valid(Generic))
            { return; }

            if (Generic.Has<ck::FTag_VisualLod_NeedsSetup>())
            { return; }

            const auto Member = UCk_Utils_VisualLod_UE::Cast(Generic);
            if (ck::Is_NOT_Valid(Member))
            { return; }

            ++_Snapshot.NumMembers;

            // No Utils getter for the resolved arbiter — the member's public surface never needed to
            // name its domain. The fragment getter is public and the view already handed us the fragment.
            const auto MemberArbiter = InCurrent.Get_Arbiter();
            const auto* ArbiterIndex = ck::IsValid(MemberArbiter)
                ? ArbiterIndexByHash.Find(GetTypeHash(MemberArbiter.ConvertToHandle()))
                : nullptr;

            if (ArbiterIndex == nullptr)
            {
                ++_Snapshot.NumUnassignedMembers;
                return;
            }

            auto Info = FCkVisualLodDebugger_MemberInfo{};
            Info.Entity = Generic;
            Info.Name   = UCk_Utils_Handle_UE::Get_DebugName(Generic).ToString();

            Info.Representation = UCk_Utils_VisualLod_UE::Get_Representation(Member);
            Info.Promoted       = Info.Representation == ECk_VisualLod_Representation::PromotedProxy;

            Info.LastDistance = UCk_Utils_VisualLod_UE::Get_LastDistance(Member);
            Info.LastInView   = UCk_Utils_VisualLod_UE::Get_LastInView(Member);

            Info.FadeAlpha = UCk_Utils_VisualLod_UE::Get_FadeAlpha(Member);
            Info.FadePhase = UCk_Utils_VisualLod_UE::Get_FadePhase(Member);

            Info.SlotIndex        = UCk_Utils_VisualLod_UE::Get_MemberIndex(Member);
            Info.PromoteLockCount = UCk_Utils_VisualLod_UE::Get_PromoteLockCount(Member);
            Info.Hidden           = UCk_Utils_VisualLod_UE::Get_IsHidden(Member);

            Info.PromotedViaLock    = UCk_Utils_VisualLod_UE::Get_PromotedViaLock(Member);
            Info.PromotedUnbudgeted = UCk_Utils_VisualLod_UE::Get_PromotedUnbudgeted(Member);
            Info.PreemptDemote      = UCk_Utils_VisualLod_UE::Get_PreemptDemote(Member);

            Info.ProxySequenceIndex = UCk_Utils_VisualLod_UE::Get_ProxySequenceIndex(Member);
            Info.ProxyRate          = UCk_Utils_VisualLod_UE::Get_ProxyRate(Member);
            Info.FarSequenceIndex   = UCk_Utils_VisualLod_UE::Get_FarSequenceIndex(Member);
            Info.FarRate            = UCk_Utils_VisualLod_UE::Get_FarRate(Member);

            if (Info.Promoted)
            {
                Info.ProxyRender = ck_visuallod_debugger_collector::DoRead_ProxyRenderFlags(
                    UCk_Utils_VisualLod_UE::TryGet_Proxy(Member));
            }

            auto& OwningArbiter = _Snapshot.Arbiters[*ArbiterIndex];

            if (const auto* Crowd = UCk_Utils_VisualLod_UE::Get_Crowd(Member);
                ck::IsValid(Crowd) && Info.SlotIndex != INDEX_NONE)
            {
                if (const auto* FoundCrowdIndex = CrowdIndexByActor.Find(Crowd))
                { Info.CrowdIndex = *FoundCrowdIndex; }
            }

            // Where the marker belongs. A far member's own entity transform is the arbitration input, not what the
            // GPU drew — the crowd instance is, so the marker sits on the thing the reader is looking at. Guarded
            // against the pool size because Get_MemberWorldTransform ENSURES on an out-of-range index, and a debugger
            // that trips an ensure to draw a dot is worse than one that draws nothing.
            const auto CanReadCrowdInstance = NOT Info.Promoted
                && Info.CrowdIndex != INDEX_NONE
                && OwningArbiter.Crowds.IsValidIndex(Info.CrowdIndex)
                && Info.SlotIndex >= 0
                && Info.SlotIndex < OwningArbiter.Crowds[Info.CrowdIndex].PoolSize;

            if (CanReadCrowdInstance)
            {
                Info.HasWorldLocation = true;
                Info.WorldLocation    = UCk_Utils_VisualLod_UE::Get_Crowd(Member)
                    ->Get_MemberWorldTransform(Info.SlotIndex).GetLocation();
            }
            else if (const auto Transform = UCk_Utils_Transform_UE::Cast(Generic);
                ck::IsValid(Transform))
            {
                Info.HasWorldLocation = true;
                Info.WorldLocation    = UCk_Utils_Transform_UE::Get_EntityCurrentLocation(Transform);
            }

            OwningArbiter.Members.Add(MoveTemp(Info));
        });

    _Snapshot.NumArbiters = _Snapshot.Arbiters.Num();

    // Domain-tag ascending so the underline tabs keep a stable order across refreshes — view iteration
    // order is a storage detail and reshuffling tabs under the user's cursor every tick is the same
    // defect class as an unstable list sort.
    _Snapshot.Arbiters.Sort([](const FCkVisualLodDebugger_ArbiterInfo& InA, const FCkVisualLodDebugger_ArbiterInfo& InB)
    {
        if (InA.DomainTagName != InB.DomainTagName)
        { return InA.DomainTagName < InB.DomainTagName; }

        return InA.Name < InB.Name;
    });

    // Nearest-first inside a domain: promotion is ranked in-view-first then nearest-first, so reading the
    // roster top-down matches the order the arbiter itself considers members.
    for (auto& Arbiter : _Snapshot.Arbiters)
    {
        Arbiter.Members.Sort([](const FCkVisualLodDebugger_MemberInfo& InA, const FCkVisualLodDebugger_MemberInfo& InB)
        {
            if (InA.LastInView != InB.LastInView)
            { return InA.LastInView; }

            if (NOT FMath::IsNearlyEqual(InA.LastDistance, InB.LastDistance))
            { return InA.LastDistance < InB.LastDistance; }

            return InA.Name < InB.Name;
        });
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkVisualLodDebugger_DataCollector::
    Reset()
    -> void
{
    _Snapshot = FCkVisualLodDebugger_Snapshot{};
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkVisualLodDebugger_DataCollector::
    Get_Snapshot() const
    -> const FCkVisualLodDebugger_Snapshot&
{
    return _Snapshot;
}

// --------------------------------------------------------------------------------------------------------------------
