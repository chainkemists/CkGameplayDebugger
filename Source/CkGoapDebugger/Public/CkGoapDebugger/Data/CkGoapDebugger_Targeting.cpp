#include "CkGoapDebugger_Targeting.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Navigation/CkDebug_SelectionSync.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"

#include "CkGoap/Action/CkGoap_Action_Fragment.h"
#include "CkGoap/Planner/CkGoap_Planner_Record_Internal.h"
#include "CkGoap/Planner/CkGoap_Planner_Utils.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_goap_debugger_targeting
{
    auto IsRegisteredPlannerChild(const FCk_Handle& InCandidate) -> bool
    {
        if (ck::Is_NOT_Valid(InCandidate)
            || NOT InCandidate.Has<ck::FFragment_LifetimeOwner>())
        { return false; }

        auto Owner = UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(InCandidate);
        if (ck::Is_NOT_Valid(Owner)
            || NOT Owner.Has<ck::FFragment_RecordOfGoapPlanners>())
        { return false; }

        auto IsRegistered = false;
        ck::goap::internal_planner_record::FRecordOfGoapPlanners_Utils::ForEach_ValidEntry(
            Owner,
            [&IsRegistered, &InCandidate](FCk_Handle_Goap_Planner InPlanner)
            {
                IsRegistered = IsRegistered
                    || static_cast<FCk_Handle>(InPlanner) == InCandidate;
            });
        return IsRegistered;
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck_goap_debugger
{
    auto
        IsGoapRosterEntity(
            const FCk_Handle& InCandidate)
        -> bool
    {
        if (ck::Is_NOT_Valid(InCandidate))
        { return false; }

        if (InCandidate.Has<ck::FFragment_RecordOfGoapPlanners>())
        {
            auto MutableOwner = InCandidate;
            auto HasPlanner = false;
            ck::goap::internal_planner_record::FRecordOfGoapPlanners_Utils::ForEach_ValidEntry(
                MutableOwner,
                [&HasPlanner](FCk_Handle_Goap_Planner) { HasPlanner = true; });
            return HasPlanner;
        }

        return UCk_Utils_Goap_Planner_UE::Has(InCandidate)
            && NOT InCandidate.Has<ck::FFragment_Goap_Action_Definition>()
            && NOT ck_goap_debugger_targeting::IsRegisteredPlannerChild(InCandidate);
    }

    auto
        ResolveGoapTarget(
            const FCk_Handle& InSelected)
        -> FCk_Handle
    {
        return ck::DebugSelectionSync::Resolve_ClosestLineageMatch(InSelected,
            [](const FCk_Handle& InCandidate) { return IsGoapRosterEntity(InCandidate); });
    }
}

// --------------------------------------------------------------------------------------------------------------------
