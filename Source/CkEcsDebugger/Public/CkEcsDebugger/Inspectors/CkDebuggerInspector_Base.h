#pragma once

#include "CkEcs/Handle/CkHandle.h"
#include "Widgets/SCompoundWidget.h"

class FCkDebuggerModel_EntitySelection;

class ICkDebuggerComponentInspector_Base
{
public:
    virtual ~ICkDebuggerComponentInspector_Base() = default;

    virtual auto Get_ComponentName() const -> FText = 0;
    virtual auto CanInspect(const FCk_Handle& Entity) const -> bool = 0;
    virtual auto Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget> = 0;
    virtual auto Get_SortPriority() const -> int32 = 0;
    virtual auto Tick(const FCk_Handle& Entity, float InDeltaTime) -> void = 0;

    /**
     * Allows a rendered inspector to receive one or more cleanup ticks after its normal
     * CanInspect predicate becomes false. The default remains fail-closed: inspectors
     * that do not explicitly opt in are never ticked for an inapplicable entity.
     */
    virtual auto Wants_TickWhenNotInspectable(const FCk_Handle& Entity) const -> bool { return false; }

    /**
     * Icon + accent color identifying this inspector's feature across the debugger —
     * entity-tree badge strips, the feature rail, and inspector section headers all
     * render the same glyph/color (one icon language; redesign spec §2.4).
     *
     * IconName resolves via FCkDebuggerStyle::Get_IconBrush ("CkDebugger.Icon.<Name>").
     * NAME_None = no dedicated glyph; surfaces fall back to a generic dot. An unset
     * color keeps the InspectorFilter model's existing curated/hash badge color.
     */
    virtual auto Get_IconName() const -> FName { return NAME_None; }
    virtual auto Get_FeatureColor() const -> TOptional<FLinearColor> { return {}; }

    /**
     * Feature id in the CkEcs debug flag cache (ck::debug_feature_flags) whose bit is
     * exactly equivalent to CanInspect(). When set, the inspector filter answers
     * Test_Entity from the bit cache — O(1) per entity — instead of instantiating this
     * inspector per entity per tree pass.
     *
     * PARITY CONTRACT: override ONLY when the registered marker fragment's presence is
     * observably identical to CanInspect() at any point the debugger can run — the
     * fragments CanInspect checks are added/removed atomically with the marker, and
     * nothing removes one without the other (verified per feature in PROGRESS.md,
     * "Chunk 2 parity findings"). Has_Any gates over multiple features and owner-side
     * record checks must NOT be wired. NAME_None = always use the instantiation path.
     */
    virtual auto Get_FeatureFlagId() const -> FName { return NAME_None; }

    virtual auto IsFilterable() const -> bool { return false; }
    virtual auto Build_Inspector(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget> { return Build_Inspector(Entity); }

    /**
     * Called when the inspector stops being active for an entity. This happens when:
     * - The selected entity changes (previous entity's inspectors are deactivated)
     * - The selection is cleared
     * - The inspector panel is destroyed
     *
     * Use this to clean up any per-entity state (debug draw, world markers, etc.).
     * The entity may no longer be valid when this is called.
     */
    virtual auto OnDeactivated() -> void {}

    struct FInspectorSection
    {
        FText Name;
        TSharedRef<SWidget> Widget;
    };

    /**
     * Override to produce multiple top-level collapsible sections from a single inspector.
     * Each section appears as its own entry in the inspector panel.
     * Only called when IsMultiSection() returns true.
     */
    virtual auto Get_InspectorSections(const FCk_Handle& Entity) -> TArray<FInspectorSection> { return {}; }

    /**
     * Returns true if this inspector produces multiple top-level sections.
     * When true, the panel calls Get_InspectorSections instead of Build_Inspector.
     */
    virtual auto IsMultiSection() const -> bool { return false; }

    /**
     * Returns true if the inspector wants to be fully rebuilt this frame.
     * The panel checks this after Tick and rebuilds if set.
     * The flag is automatically cleared after the rebuild.
     */
    auto NeedsRebuild() const -> bool { return IsRebuildPending; }
    auto ClearRebuildFlag() -> void { IsRebuildPending = false; }

    virtual auto Set_SelectionModel(TSharedPtr<FCkDebuggerModel_EntitySelection> InModel) -> void { SelectionModel = MoveTemp(InModel); }
    auto Get_SelectionModel() const -> TSharedPtr<FCkDebuggerModel_EntitySelection> { return SelectionModel; }

protected:
    /** Call this from Tick when the inspector detects its data has structurally changed */
    auto RequestRebuild() -> void { IsRebuildPending = true; }

    TSharedPtr<FCkDebuggerModel_EntitySelection> SelectionModel;

private:
    bool IsRebuildPending = false;
};
