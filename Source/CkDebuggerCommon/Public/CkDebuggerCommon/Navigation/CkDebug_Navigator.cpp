#include "CkDebug_Navigator.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Handle/CkHandle.h"

// ====================================================================================================================

namespace ck::DebugNav
{
    namespace
    {
        // File-local singleton. Slate widgets in CkDebuggerCommon outlive PIE so a
        // module-static is fine; the function is reset to an unbound TFunction in
        // FCkEcsDebuggerModule::ShutdownModule.
        FGotoEntityFn& Get_NavigatorSlot()
        {
            static FGotoEntityFn _NavigatorFn;
            return _NavigatorFn;
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto Register_EntityNavigator(FGotoEntityFn InFn) -> void
    {
        Get_NavigatorSlot() = MoveTemp(InFn);
    }

    auto Goto_Entity(const FCk_Handle& InEntity) -> void
    {
        if (ck::Is_NOT_Valid(InEntity))
        { return; }

        auto& Fn = Get_NavigatorSlot();
        if (NOT Fn)
        { return; }

        Fn(InEntity);
    }

    auto Has_EntityNavigator() -> bool
    {
        return static_cast<bool>(Get_NavigatorSlot());
    }
}

// ====================================================================================================================
