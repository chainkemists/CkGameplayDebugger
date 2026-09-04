#include "CkDebug_SelectionSync.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Classification/CkDebug_DepthTransparency.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"

#include "HAL/IConsoleManager.h"

// ====================================================================================================================

namespace
{
    ck::DebugSelectionSync::FCkDebug_OnGlobalSelection GSelectionDelegate_SelectionSync;
    int32 GApplyDepth_SelectionSync = 0;
    ck::DebugSelectionSync::FGetPrimaryEcsSelection GPrimaryEcsSelectionProvider_SelectionSync;
    uint64 GPrimaryEcsSelectionRegistrationId_SelectionSync = 0;
    uint64 GNextPrimaryEcsSelectionRegistrationId_SelectionSync = 1;

    TAutoConsoleVariable<int32> CVar_OverlayFocusSync_SelectionSync(
        TEXT("ck.Debug.SelectionSync.OverlayFocus"),
        0,
        TEXT("1 = sync the current on-screen overlay focus into every already-open compatible debugger; 0 = disabled."),
        ECVF_Cheat);

    // Owner-chain walk cap. Real chains are a handful deep; the cap is a
    // corruption backstop, not a tuning knob.
    constexpr int32 LineageWalk_MaxDepth = 64;

    auto Is_AncestorOrSelf_SelectionSync(const FCk_Handle& InMaybeAncestor, const FCk_Handle& InLeaf) -> bool
    {
        if (ck::Is_NOT_Valid(InMaybeAncestor) || ck::Is_NOT_Valid(InLeaf))
        { return false; }

        const auto Transient = UCk_Utils_EntityLifetime_UE::Get_TransientEntity(InLeaf);

        auto Cur = InLeaf;
        for (auto Depth = 0; Depth < LineageWalk_MaxDepth && ck::IsValid(Cur); ++Depth)
        {
            if (Cur == InMaybeAncestor) { return true; }
            if (Cur == Transient)       { break; }
            if (NOT Cur.Has<ck::FFragment_LifetimeOwner>()) { break; }
            Cur = UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Cur);
        }
        return false;
    }
}

// ====================================================================================================================

namespace ck::DebugSelectionSync
{
    auto Get_OnSelection() -> FCkDebug_OnGlobalSelection&
    {
        return GSelectionDelegate_SelectionSync;
    }

    auto Broadcast(const FCk_Handle& InSelected, FName InSource) -> void
    {
        if (Is_Applying())                 { return; }
        if (ck::Is_NOT_Valid(InSelected))  { return; }

        GSelectionDelegate_SelectionSync.Broadcast(InSelected, InSource);
    }

    auto Get_IsOverlayFocusSyncEnabled() -> bool
    {
        return CVar_OverlayFocusSync_SelectionSync.GetValueOnGameThread() != 0;
    }

    auto Get_OverlayFocusSyncCVarName() -> const TCHAR*
    {
        return TEXT("ck.Debug.SelectionSync.OverlayFocus");
    }

    auto Register_PrimaryEcsSelectionProvider(FGetPrimaryEcsSelection InProvider) -> uint64
    {
        if (NOT InProvider)
        { return 0; }

        const auto RegistrationId = GNextPrimaryEcsSelectionRegistrationId_SelectionSync++;
        if (GNextPrimaryEcsSelectionRegistrationId_SelectionSync == 0)
        { GNextPrimaryEcsSelectionRegistrationId_SelectionSync = 1; }

        GPrimaryEcsSelectionProvider_SelectionSync = MoveTemp(InProvider);
        GPrimaryEcsSelectionRegistrationId_SelectionSync = RegistrationId;
        return RegistrationId;
    }

    auto Unregister_PrimaryEcsSelectionProvider(uint64 InRegistrationId) -> void
    {
        if (InRegistrationId == 0 || InRegistrationId != GPrimaryEcsSelectionRegistrationId_SelectionSync)
        { return; }

        GPrimaryEcsSelectionProvider_SelectionSync = FGetPrimaryEcsSelection{};
        GPrimaryEcsSelectionRegistrationId_SelectionSync = 0;
    }

    auto Has_PrimaryEcsSelectionProvider() -> bool
    {
        return static_cast<bool>(GPrimaryEcsSelectionProvider_SelectionSync);
    }

    auto Get_PrimaryEcsSelection() -> FCk_Handle
    {
        if (NOT GPrimaryEcsSelectionProvider_SelectionSync)
        { return {}; }

        auto Selection = GPrimaryEcsSelectionProvider_SelectionSync();
        if (ck::Is_NOT_Valid(Selection))
        { return {}; }

        return Selection;
    }

    auto Is_Applying() -> bool
    {
        return GApplyDepth_SelectionSync > 0;
    }

    FApplyGuard::FApplyGuard()  { ++GApplyDepth_SelectionSync; }
    FApplyGuard::~FApplyGuard() { --GApplyDepth_SelectionSync; }

    auto Is_SameLineage(const FCk_Handle& InA, const FCk_Handle& InB) -> bool
    {
        return Is_AncestorOrSelf_SelectionSync(InA, InB)
            || Is_AncestorOrSelf_SelectionSync(InB, InA);
    }

    auto Resolve_ConceptualTarget(const FCk_Handle& InLeaf) -> FCk_Handle
    {
        if (ck::Is_NOT_Valid(InLeaf))
        { return {}; }

        const auto Transient = UCk_Utils_EntityLifetime_UE::Get_TransientEntity(InLeaf);
        auto Current = InLeaf;
        auto ConceptualTarget = FCk_Handle{};

        for (auto Depth = 0; Depth < LineageWalk_MaxDepth; ++Depth)
        {
            if (ck::Is_NOT_Valid(Current) || Current == Transient)
            { return ConceptualTarget; }

            if (NOT ck::DebugDepthTransparency::Get_IsRelayEntity(Current))
            { ConceptualTarget = Current; }

            const auto HasLifetimeOwner = Current.Has<ck::FFragment_LifetimeOwner>();
            CK_ENSURE_IF_NOT(HasLifetimeOwner,
                TEXT("Cannot resolve conceptual target for [{}]: ownership chain ended before the transient root"),
                InLeaf)
            { return {}; }

            const auto Owner = UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Current);
            const auto HasValidOwner = ck::IsValid(Owner);
            CK_ENSURE_IF_NOT(HasValidOwner,
                TEXT("Cannot resolve conceptual target for [{}]: lifetime owner is invalid"),
                InLeaf)
            { return {}; }

            const auto IsNotSelfOwned = Owner != Current;
            CK_ENSURE_IF_NOT(IsNotSelfOwned,
                TEXT("Cannot resolve conceptual target for [{}]: lifetime ownership chain is self-referential"),
                InLeaf)
            { return {}; }

            Current = Owner;
        }

        const auto DidReachTransientRoot = Current == Transient;
        CK_ENSURE_IF_NOT(DidReachTransientRoot,
            TEXT("Cannot resolve conceptual target for [{}]: lifetime ownership walk exceeded {} hops"),
            InLeaf,
            LineageWalk_MaxDepth)
        { return {}; }

        return ConceptualTarget;
    }

    auto Resolve_ClosestLineageMatch(
        const FCk_Handle& InSelected,
        TFunctionRef<bool(const FCk_Handle&)> InPredicate) -> FCk_Handle
    {
        if (ck::Is_NOT_Valid(InSelected))
        { return {}; }

        auto Best = FCk_Handle{};
        auto BestDepth = TNumericLimits<int32>::Max();
        const auto Consider = [&Best, &BestDepth, &InPredicate](const FCk_Handle& InCandidate, int32 InDepth)
        {
            if (ck::Is_NOT_Valid(InCandidate) || NOT InPredicate(InCandidate))
            { return; }

            const auto IsNearer = InDepth < BestDepth;
            const auto IsStableTie = InDepth == BestDepth &&
                (ck::Is_NOT_Valid(Best) ||
                    InCandidate.Get_Entity().Get_EntityNumber() < Best.Get_Entity().Get_EntityNumber());
            if (IsNearer || IsStableTie)
            {
                Best = InCandidate;
                BestDepth = InDepth;
            }
        };

        Consider(InSelected, 0);
        if (BestDepth == 0)
        { return Best; }

        const auto Transient = UCk_Utils_EntityLifetime_UE::Get_TransientEntity(InSelected);
        auto Ancestor = InSelected;
        for (auto Depth = 1; Depth < LineageWalk_MaxDepth && Depth <= BestDepth; ++Depth)
        {
            if (NOT Ancestor.Has<ck::FFragment_LifetimeOwner>())
            { break; }

            Ancestor = UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Ancestor);
            if (ck::Is_NOT_Valid(Ancestor) || Ancestor == Transient)
            { break; }

            Consider(Ancestor, Depth);
        }

        constexpr auto MaxVisitedEntities = 4096;
        auto Visited = TSet<FCk_Handle>{InSelected};
        auto Frontier = TArray<FCk_Handle>{InSelected};
        for (auto Depth = 1;
             Depth < LineageWalk_MaxDepth && Depth <= BestDepth && NOT Frontier.IsEmpty();
             ++Depth)
        {
            auto Next = TArray<FCk_Handle>{};
            for (const auto& Parent : Frontier)
            {
                if (NOT Parent.Has<ck::FFragment_LifetimeDependents>())
                { continue; }

                for (const auto& Dependent : UCk_Utils_EntityLifetime_UE::Get_LifetimeDependents(Parent))
                {
                    if (Visited.Num() >= MaxVisitedEntities)
                    { break; }
                    if (ck::Is_NOT_Valid(Dependent) || Visited.Contains(Dependent))
                    { continue; }

                    Visited.Add(Dependent);
                    Next.Add(Dependent);
                    Consider(Dependent, Depth);
                }
            }
            Frontier = MoveTemp(Next);
        }

        return Best;
    }
}

// ====================================================================================================================
