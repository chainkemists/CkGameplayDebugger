#pragma once

#include "CkJoltEditor/Cook/CkJoltCook_MeshShapeAudit.h"

namespace ck::jolt_bake_inspector
{
    /** Pure cursor for a user-started audit. It owns no assets or Jolt state, so cancellation is testable. */
    class FCkJoltBakeInspectorAnalysisQueue
    {
    public:
        auto Start(int32 InTotal) -> void
        {
            _Total = FMath::Max(0, InTotal);
            _Next = 0;
        }

        auto Cancel() -> void { _Total = 0; _Next = 0; }
        auto IsActive() const -> bool { return _Next < _Total; }
        auto Get_Processed() const -> int32 { return _Next; }
        auto Get_Total() const -> int32 { return _Total; }

        /** Returns exactly one work index and advances; exhausted/cancelled queues return unset. */
        auto TryTakeNext() -> TOptional<int32>
        {
            if (NOT IsActive()) { return {}; }
            return _Next++;
        }

    private:
        int32 _Next = 0;
        int32 _Total = 0;
    };

    inline auto Get_IsRepairableBakeAction(ck::jolt::cook::ECk_Jolt_MeshShapeAuditAction InAction, bool InWouldFailBake) -> bool
    {
        using enum ck::jolt::cook::ECk_Jolt_MeshShapeAuditAction;
        return NOT InWouldFailBake && (InAction == CookMissing || InAction == RebuildStale
            || InAction == RebuildCorrupt || InAction == RebuildInsideOut);
    }
}
