#include "CkInspector_Goap.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"

#include "Widgets/Text/STextBlock.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Goap)

// =====================================================================================================================
//
// TODO(CkGoap-BundleTierRefactor): full inspector rewrite pending.
//
// The CkGoap planner-per-entity model has been replaced by Bundle/Tier
// (see docs/superpowers/specs/2026-05-19-CkGoap-BundleTierRefactor-design.md).
// This inspector previously read FFragment_Goap_Current / _Params / _Actions /
// _Goals / _Diagnostics directly off planner entities — all of which are
// removed in Phase 2 of the refactor.
//
// The redesigned inspector will surface Bundle catalog views, active-tier
// chain breadcrumbs, per-tier plan inspection, and the new WS-source-
// resolution view. That work lives in a separate follow-up spec authored
// after the refactor implementation lands.
//
// Until then, this stub is registered with the inspector registry but
// CanInspect always returns false — the panel will never appear for GOAP
// entities. Once the refactor exposes Bundle / Tier handles on entities,
// CanInspect will gate on those (currently we have nothing tier-shaped to
// detect).
//
// =====================================================================================================================

auto FCkInspector_Goap::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("GOAP (pending redesign)"));
}

auto FCkInspector_Goap::CanInspect(const FCk_Handle& /*Entity*/) const -> bool
{
    return false;
}

auto FCkInspector_Goap::Build_Inspector(const FCk_Handle& /*Entity*/) -> TSharedRef<SWidget>
{
    return SNew(STextBlock)
        .Text(FText::FromString(TEXT("GOAP inspector pending Bundle/Tier refactor — see CkGoap spec.")));
}

auto FCkInspector_Goap::Tick(const FCk_Handle& /*Entity*/, float /*InDeltaTime*/) -> void
{
    // No-op until redesign.
}
