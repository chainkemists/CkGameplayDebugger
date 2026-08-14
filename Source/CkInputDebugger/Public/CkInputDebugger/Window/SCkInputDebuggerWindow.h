#pragma once

#include "CkInputDebugger/Data/CkInputDebugger_Snapshot.h"
#include "CkInputDebugger/Data/CkInputDebugger_Bindings.h"
#include "CkInputDebugger/Data/CkInputDebugger_KeyActivity.h"

#include "CkDebuggerCommon/Devices/CkDebug_DeviceTypes.h"

#include "CoreMinimal.h"
#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"

// --------------------------------------------------------------------------------------------------------------------

class SVerticalBox;
class SHorizontalBox;
class SBorder;
class SBox;
class SExpandableArea;
class SCkDebug_CategoryDot;
class SCkDebug_EventTimeline;
class STextBlock;
class FCkDebuggerModel_WorldSelector;
class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;
class APlayerController;

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

// --------------------------------------------------------------------------------------------------------------------
// The bindings pane's row filter — which profile rows show.
// --------------------------------------------------------------------------------------------------------------------

enum class ECkInputDebugger_BindingsFilterMode
{
    All,
    ReboundOnly,
    DefaultOnly,
};

// ====================================================================================================================
// CK Enhanced Input Debugger window.
//
// Shows, for the selected local player:
//   1. A live held/recent key strip + the shared device visualizers (keyboard / mouse / gamepad).
//   2. The player's mappable-key profile, default vs current, filterable to rebound/default rows.
//   3. The applied mapping-context STACK (priority-ordered) + each context's
//      registered action<->key mappings.
//   4. The resolved (flattened) action bindings with LIVE runtime value +
//      trigger-event state.
//
// Structure is rebuilt only when the snapshot's structural signature changes
// (stack/mapping/player change). Live values + filter/highlight are applied
// in-place each gated tick — no Tick-path widget-tree recreation. The key strip
// updates a pre-built chip pool in place for the same reason: tearing widgets
// down on every key press causes a one-frame layout/font smear.
// ====================================================================================================================

class SCkInputDebuggerWindow : public SCkDebugger_WindowBase
{
public:
    static const FName WindowId;

    SLATE_BEGIN_ARGS(SCkInputDebuggerWindow) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    virtual ~SCkInputDebuggerWindow();
    auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    virtual auto Get_WindowId() const -> FName override { return WindowId; }
    virtual auto Get_WindowDisplayName() const -> FText override { return FText::FromString(TEXT("Enhanced Input")); }

protected:
    // Context / action rows are built imperatively, so a style revision re-runs them through the
    // existing signature-driven rebuild instead of mutating the tree from the revision poll.
    virtual auto OnStyleRevisionChanged() -> void override;

private:
    // ---- Construction ----

    auto BuildToolbar() -> TSharedRef<SWidget>;
    auto BuildDevicesSection() -> TSharedRef<SWidget>;
    auto BuildBindingsHeader() -> TSharedRef<SWidget>;
    auto BuildSection(const FText& InLabel, const TSharedRef<SWidget>& InHeaderExtra, const TSharedRef<SWidget>& InBody) -> TSharedRef<SWidget>;

    // ---- Player resolution ----

    auto ResolveSubsystem(UWorld* InWorld, FString& OutLabel, int32& OutNumLocalPlayers) const -> UEnhancedInputLocalPlayerSubsystem*;
    auto RefreshPlayerSelector(int32 InNumLocalPlayers) -> void;

    // ---- Structure (rebuilt on signature change) ----

    auto RebuildSections(const FCkInputDebugger_Snapshot& InSnapshot) -> void;
    auto BuildContextSlot(const FCkInputDebugger_ContextRow& InContext) -> void;
    auto BuildActionSlot(const FCkInputDebugger_ActionRow& InAction) -> void;
    auto RebuildBindings(const FCkInputDebugger_BindingsSnapshot& InBindings) -> void;

    // ---- In-place update (every gated tick) ----

    auto UpdateKeyStrip() -> void;
    auto EnsureKeyStripChips(int32 InCount) -> void;
    auto UpdateTimeline() -> void;
    auto UpdateLiveValues() -> void;
    auto ApplyFilterAndHighlight() -> void;

    // ---- Device visuals + key filter ----

    auto Get_DeviceSnapshot() -> const FCkDebug_DeviceSnapshot*;
    auto Get_KeyTooltip(const FKey& InKey) const -> FText;
    auto HandleDeviceKeyClicked(const FKey& InKey) -> void;
    auto Get_KeyDisplay(const FKey& InKey) const -> FString;

    auto OnEndPIE(bool InIsSimulating) -> void;

    // ---- Search ----

    auto MatchesFilter(const FString& InSearchText) const -> bool;
    auto MatchesHighlight(const FString& InSearchText) const -> bool;

    // ---- Toolbar ----
    TSharedPtr<FCkDebuggerModel_WorldSelector> _WorldModel;
    TSharedPtr<SHorizontalBox>                 _PlayerSelectorBox;
    TSharedPtr<STextBlock>                     _SummaryText;

    // ---- Body containers ----
    TSharedPtr<SVerticalBox>   _ContextListBox;
    TSharedPtr<SVerticalBox>   _ResolvedListBox;
    TSharedPtr<SVerticalBox>   _BindingsListBox;
    TSharedPtr<SHorizontalBox> _KeyStripBox;

    // ---- Pre-built slots ----
    TArray<FCkInputDebugger_ContextSlot> _ContextSlots;
    TArray<FCkInputDebugger_ActionSlot>  _ActionSlots;

    struct FCkInputDebugger_BindingSlot
    {
        TSharedPtr<SBorder>   Root;
        TSharedPtr<STextBlock> NameText;
        FString               SearchText;
        FString               Category;
        bool                  IsRebound = false;
    };
    TArray<FCkInputDebugger_BindingSlot> _BindingSlots;
    TMap<FString, TSharedPtr<SWidget>>   _BindingCategoryHeaders;
    TSharedPtr<STextBlock>               _ReboundCountText;

    // One chip = one held/recent key; the pool is grown on demand and updated in place.
    struct FCkInputDebugger_KeyChip
    {
        TSharedPtr<SBorder>    Root;
        TSharedPtr<SBorder>    KeyBadge;
        TSharedPtr<STextBlock> KeyText;
        TSharedPtr<STextBlock> ActionText;
    };
    TArray<FCkInputDebugger_KeyChip> _KeyStripChips;
    TSharedPtr<SWidget>              _KeyStripEmptyText;

    // ---- Timeline (shared lanes widget, frame axis; rebuilt only when the lane SET changes) ----
    TSharedPtr<SBox>                   _TimelineHost;
    TSharedPtr<SCkDebug_EventTimeline> _Timeline;
    TArray<FString> _TimelineLaneLabels;
    TArray<FKey>    _TimelineLaneKeys;
    uint32          _TimelineStructureHash = 0;

    // ---- State ----
    TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> _BoundSubsystem;
    TWeakObjectPtr<APlayerController>                  _BoundPlayerController;
    int32   _SelectedPlayerIndex = 0;
    int32   _LastNumLocalPlayers = -1;
    FString _LastSignature;
    FString _LastBindingsSignature;
    FString _FilterString;
    FString _HighlightString;
    bool    _ShowActiveActionsOnly = false;
    ECkInputDebugger_BindingsFilterMode _BindingsFilterMode = ECkInputDebugger_BindingsFilterMode::All;

    // ---- Live key activity (passive observer; holds keys only, never handles) ----
    TSharedPtr<FCkInputDebugger_KeyActivityObserver> _KeyObserver;
    int32 _LastActivityRevision = -1;
    FKey  _KeyFilter;
    TSet<FKey> _MappedKeys;
    TSet<FKey> _ReboundKeys;
    TMap<FKey, TArray<FString>> _ActionsByKey;

    // Composed on demand (at most once per frame) from the observer's physical edges + this
    // window's presentation overlays (mapped / rebound / click-filter highlight).
    FCkDebug_DeviceSnapshot _DeviceSnapshot;
    uint64                  _DeviceSnapshotFrame = 0;

    FDelegateHandle _EndPIEHandle;
};

// ====================================================================================================================
