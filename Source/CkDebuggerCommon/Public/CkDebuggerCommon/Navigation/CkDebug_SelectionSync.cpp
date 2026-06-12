#include "CkDebug_SelectionSync.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"

// ====================================================================================================================

namespace
{
    ck::DebugSelectionSync::FCkDebug_OnGlobalSelection GSelectionDelegate_SelectionSync;
    int32 GApplyDepth_SelectionSync = 0;

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
}

// ====================================================================================================================
