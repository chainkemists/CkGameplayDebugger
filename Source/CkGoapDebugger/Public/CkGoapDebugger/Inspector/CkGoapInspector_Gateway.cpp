#include "CkGoapDebugger/Inspector/CkGoapInspector_Gateway.h"

#include "CkGoapDebugger/Window/SCkGoapDebugger_InspectorGateway.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkGoap/CkGoap_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"

#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================

CK_REGISTER_DEBUGGER_INSPECTOR(FCkGoapInspector_Gateway)

// ====================================================================================================================

auto
    FCkGoapInspector_Gateway::
    Get_ComponentName() const
    -> FText
{
    return FText::FromString(TEXT("GOAP"));
}

auto
    FCkGoapInspector_Gateway::
    CanInspect(
        const FCk_Handle& Entity) const
    -> bool
{
    if (ck::Is_NOT_Valid(Entity))
    { return false; }

    // Match the Add-on-owner shape (Goap fragments directly on the entity).
    // The Create-as-child shape (planner on a child entity) is not surfaced
    // here yet — the gateway always renders for the entity selected in the
    // ECS inspector, so the user should select the planner-bearing entity.
    return UCk_Utils_Goap_UE::Has(Entity);
}

auto
    FCkGoapInspector_Gateway::
    Build_Inspector(
        const FCk_Handle& Entity)
    -> TSharedRef<SWidget>
{
    if (NOT _Gateway.IsValid())
    {
        SAssignNew(_Gateway, SCkGoapDebugger_InspectorGateway)
            .Entity(Entity);
    }
    else
    {
        _Gateway->Set_Entity(Entity);
    }

    return _Gateway.ToSharedRef();
}

auto
    FCkGoapInspector_Gateway::
    Tick(
        const FCk_Handle& /*Entity*/,
        float /*InDeltaTime*/)
    -> void
{
    // The gateway widget ticks itself (re-pulls snapshots, hash-debounces
    // rebuilds), so nothing to do here.
}

// ====================================================================================================================
