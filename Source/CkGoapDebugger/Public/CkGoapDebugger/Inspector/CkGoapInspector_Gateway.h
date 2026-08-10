#pragma once

#include "CkEcsDebugger/Inspectors/CkDebuggerInspector_Base.h"

// ====================================================================================================================

class SCkGoapDebugger_InspectorGateway;

// ====================================================================================================================
// FCkGoapInspector_Gateway — D7 — CkEcsDebugger entity-inspector adapter that
// hosts an SCkGoapDebugger_InspectorGateway widget when the selected entity
// carries a Goap root.
//
// Registered against FCkDebuggerInspectorRegistry from
// FCkGoapDebuggerModule::StartupModule so the CkEcsDebugger module stays
// unaware of CkGoapDebugger (one-way dependency).
// ====================================================================================================================

class CKGOAPDEBUGGER_API FCkGoapInspector_Gateway : public ICkDebuggerComponentInspector_Base
{
public:
    auto Get_ComponentName() const -> FText override;
    auto Get_IconName() const -> FName override { return TEXT("Goap"); }
    auto CanInspect(const FCk_Handle& Entity) const -> bool override;
    auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> override;
    auto Get_SortPriority() const -> int32 override { return 65; }
    auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void override;

private:
    TSharedPtr<SCkGoapDebugger_InspectorGateway> _Gateway;
};

// ====================================================================================================================
