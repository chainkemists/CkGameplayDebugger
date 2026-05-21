#pragma once

#include "CkGoapDebugger/Data/CkGoapDebugger_Types.h"

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================

class SVerticalBox;
class UWorld;

// ====================================================================================================================
// SCkGoapDebugger_InspectorGateway — D7 — Goap summary card embedded inside
// the CkEcsDebugger entity inspector.
//
// When CkEcsDebugger selects an entity that carries a Goap root, the gateway
// renders a compact card showing:
//   - Header  : "GOAP <root badge>" + "Open in Goap Debugger" button
//   - Section : ActionSet list (status dot + name + count of active actions)
//   - Section : Active chain (Strategic ▸ OperateShop ▸ ServeCustomer) with
//               leaf highlighted; tag chain on a sub-line
//   - Section : Leaf action — status / cost / plan length
//   - Section : Plan preview — first 3 entries + "K more" footer
//
// Data source:
//   FCkGoapDebugger_DataCollector::CollectSnapshots(World) — same backend the
//   standalone window uses. Refreshed on Tick from the cached PIE world.
//
// Lifetime:
//   The gateway holds only an FCk_Handle entity by value; it does NOT subscribe
//   to the standalone GoapDebugger's ViewModel (would risk a refresh loop).
//   Selection is driven by CkEcsDebugger's selection model, threaded in via
//   Construct.
// ====================================================================================================================

class CKGOAPDEBUGGER_API SCkGoapDebugger_InspectorGateway : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkGoapDebugger_InspectorGateway) {}
        SLATE_ARGUMENT(FCk_Handle, Entity)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    // Used by the inspector wrapper when selection changes — swaps the entity
    // and forces a rebuild without re-allocating the widget.
    auto Set_Entity(const FCk_Handle& InEntity) -> void;

private:
    auto Resolve_World() const -> UWorld*;

    // Rebuild the inner Slate tree from the latest snapshot.
    auto Rebuild() -> void;

    // Section helpers — separate so the rebuild path stays readable.
    auto Build_Header(const FCkGoapDebugger_EntitySnapshot& InSnapshot)         -> TSharedRef<SWidget>;
    auto Build_ActionSetList(const FCkGoapDebugger_EntitySnapshot& InSnapshot)  -> TSharedRef<SWidget>;
    auto Build_ActiveChain(const FCkGoapDebugger_ActionSetInfo& InActionSet)    -> TSharedRef<SWidget>;
    auto Build_LeafAction(const FCkGoapDebugger_ActionInfo& InLeaf)             -> TSharedRef<SWidget>;
    auto Build_PlanPreview(const FCkGoapDebugger_ActionInfo& InLeaf)            -> TSharedRef<SWidget>;
    auto Build_EmptyStub()                                                       -> TSharedRef<SWidget>;

    // Open the standalone GoapDebugger and select this entity.
    auto OnClicked_OpenInGoapDebugger() -> FReply;

    // Pick the ActionSet whose chain we render. Priority: PlanFound > Planning >
    // any enabled with a non-empty chain > first ActionSet. Returns nullptr if
    // the snapshot has no ActionSets.
    auto Pick_DisplayActionSet(const FCkGoapDebugger_EntitySnapshot& InSnapshot) const
        -> const FCkGoapDebugger_ActionSetInfo*;

private:
    FCk_Handle                    _Entity;
    TSharedPtr<SVerticalBox>      _ContentBox;

    // Coarse hash of the last rendered snapshot — keeps Tick from rebuilding
    // every frame.
    uint32                        _LastBuiltHash = 0;
    bool                          _HasBuilt      = false;
};

// ====================================================================================================================
