#pragma once

#include "CkInputDebugger/Data/CkInputDebugger_Snapshot.h"

#include "CoreMinimal.h"
#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"

// --------------------------------------------------------------------------------------------------------------------

class SVerticalBox;
class SHorizontalBox;
class SExpandableArea;
class SCkDebug_CategoryDot;
class STextBlock;
class FCkDebuggerModel_WorldSelector;
class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;

// --------------------------------------------------------------------------------------------------------------------
// Pre-built, in-place-updated row for one action<->key mapping inside a context.
// --------------------------------------------------------------------------------------------------------------------

struct FCkInputDebugger_MappingSlot
{
    TSharedPtr<SWidget> Root;
    FString             SearchText; // "<action> <key>" lower-cased match source
};

// --------------------------------------------------------------------------------------------------------------------
// One mapping-context rung in the stack. Header text is static (set at rebuild);
// only visibility/highlight changes between rebuilds.
// --------------------------------------------------------------------------------------------------------------------

struct FCkInputDebugger_ContextSlot
{
    TSharedPtr<SExpandableArea> ExpandableArea;
    TSharedPtr<STextBlock>      NameText;
    FString                     SearchText;
    TArray<FCkInputDebugger_MappingSlot> MappingSlots;
};

// --------------------------------------------------------------------------------------------------------------------
// One resolved-action row. Value + trigger update live every gated tick.
// --------------------------------------------------------------------------------------------------------------------

struct FCkInputDebugger_ActionSlot
{
    TWeakObjectPtr<const UInputAction> Action;
    TSharedPtr<SWidget>              Root;
    TSharedPtr<SCkDebug_CategoryDot> ActivityDot;
    TSharedPtr<STextBlock> NameText;
    TSharedPtr<STextBlock> ValueText;
    TSharedPtr<STextBlock> TriggerText;
    FString                SearchText;
    ECkInputDebugger_ActionActivity Activity = ECkInputDebugger_ActionActivity::NoInstance;
};

// ====================================================================================================================
// CK Enhanced Input Debugger window.
//
// Shows, for the selected local player:
//   1. The applied mapping-context STACK (priority-ordered) + each context's
//      registered action<->key mappings.
//   2. The resolved (flattened) action bindings with LIVE runtime value +
//      trigger-event state.
//
// Structure is rebuilt only when the snapshot's structural signature changes
// (stack/mapping/player change). Live values + filter/highlight are applied
// in-place each gated tick — no Tick-path widget-tree recreation.
// ====================================================================================================================

class SCkInputDebuggerWindow : public SCkDebugger_WindowBase
{
public:
    static const FName WindowId;

    SLATE_BEGIN_ARGS(SCkInputDebuggerWindow) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    virtual auto Get_WindowId() const -> FName override { return WindowId; }
    virtual auto Get_WindowDisplayName() const -> FText override { return FText::FromString(TEXT("CK Enhanced Input Debugger")); }

protected:
    // Context / action rows are built imperatively, so a style revision re-runs them through the
    // existing signature-driven rebuild instead of mutating the tree from the revision poll.
    virtual auto OnStyleRevisionChanged() -> void override;

private:
    // ---- Construction ----

    auto BuildToolbar() -> TSharedRef<SWidget>;

    // ---- Player resolution ----

    auto ResolveSubsystem(UWorld* InWorld, FString& OutLabel, int32& OutNumLocalPlayers) const -> UEnhancedInputLocalPlayerSubsystem*;
    auto RefreshPlayerSelector(int32 InNumLocalPlayers) -> void;

    // ---- Structure (rebuilt on signature change) ----

    auto RebuildSections(const FCkInputDebugger_Snapshot& InSnapshot) -> void;
    auto BuildContextSlot(const FCkInputDebugger_ContextRow& InContext) -> void;
    auto BuildActionSlot(const FCkInputDebugger_ActionRow& InAction) -> void;

    // ---- In-place update (every gated tick) ----

    auto UpdateLiveValues() -> void;
    auto ApplyFilterAndHighlight() -> void;

    // ---- Search ----

    auto MatchesFilter(const FString& InSearchText) const -> bool;
    auto MatchesHighlight(const FString& InSearchText) const -> bool;

    // ---- Toolbar ----
    TSharedPtr<FCkDebuggerModel_WorldSelector> _WorldModel;
    TSharedPtr<SHorizontalBox>                 _PlayerSelectorBox;
    TSharedPtr<STextBlock>                     _SummaryText;

    // ---- Body containers ----
    TSharedPtr<SVerticalBox> _ContextListBox;
    TSharedPtr<SVerticalBox> _ResolvedListBox;

    // ---- Pre-built slots ----
    TArray<FCkInputDebugger_ContextSlot> _ContextSlots;
    TArray<FCkInputDebugger_ActionSlot>  _ActionSlots;

    // ---- State ----
    TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> _BoundSubsystem;
    int32   _SelectedPlayerIndex = 0;
    int32   _LastNumLocalPlayers = -1;
    FString _LastSignature;
    FString _FilterString;
    FString _HighlightString;
    bool    _ShowActiveActionsOnly = false;
};

// ====================================================================================================================
