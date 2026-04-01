#pragma once

#include "CkUI/Types/CkUI_Types.h"

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Widgets/SCompoundWidget.h"

class SEditableTextBox;
class SExpandableArea;
class SImage;
class UCk_UI_Layout_Subsystem_UE;
class UCk_UI_LayerStack_UE;
class UCk_UI_PrimaryGameLayout_UE;

// --------------------------------------------------------------------------------------------------------------------
// History event
// --------------------------------------------------------------------------------------------------------------------

struct FCkUIDebugger_HistoryEvent
{
    double Timestamp = 0.0;
    FString Description;
};

// --------------------------------------------------------------------------------------------------------------------
// Pre-allocated slot for a single layer's display.
// Created once when the layout binds. Only text/color/visibility updated on events.
// --------------------------------------------------------------------------------------------------------------------

struct FCkUIDebugger_LayerSlot
{
    UCk_UI_LayerStack_UE* Stack = nullptr;

    TSharedPtr<SExpandableArea> ExpandableArea;
    TSharedPtr<SImage> StatusDot;
    TSharedPtr<STextBlock> TagText;
    TSharedPtr<STextBlock> PriorityText;
    TSharedPtr<STextBlock> InputModeText;
    TSharedPtr<STextBlock> WidgetCountText;
    TSharedPtr<SVerticalBox> WidgetListBox;
};

// --------------------------------------------------------------------------------------------------------------------
// Pre-allocated slot for a single widget row inside a layer.
// --------------------------------------------------------------------------------------------------------------------

struct FCkUIDebugger_WidgetSlot
{
    TSharedPtr<SWidget> Root;
    TSharedPtr<SImage> StatusDot;
    TSharedPtr<STextBlock> ClassNameText;
    TSharedPtr<STextBlock> BadgeText;
};

// --------------------------------------------------------------------------------------------------------------------
// CK UI Layer Debugger window — placed inside a NomadTab.
//
// Event-driven: binds to layout delegates and updates pre-allocated widget
// slots in-place. Never destroys/recreates layer widgets at runtime.
// --------------------------------------------------------------------------------------------------------------------

class SCkUIDebuggerWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkUIDebuggerWindow) {}
    SLATE_END_ARGS()

    ~SCkUIDebuggerWindow();

    auto Construct(const FArguments& InArgs) -> void;
    auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

private:
    // ---- Event Binding ----

    auto DoBindLayoutEvents(UCk_UI_PrimaryGameLayout_UE* InLayout) -> void;
    auto DoUnbindLayoutEvents() -> void;

    // ---- Event Handlers ----

    auto HandleWidgetPushed(FGameplayTag InLayerTag, UCommonActivatableWidget* InWidget) -> void;
    auto HandleWidgetPopped(FGameplayTag InLayerTag, UCommonActivatableWidget* InWidget) -> void;
    auto HandleLayerCleared(FGameplayTag InLayerTag) -> void;
    auto HandleActiveLayerChanged(FGameplayTag InNewActiveTag) -> void;
    auto HandleInputModeChanged(ECk_UI_InputMode InNewMode) -> void;

    // ---- Structure (one-time) ----

    auto DoBuildLayerSlots() -> void;
    auto DoBuildHistoryList() -> void;

    // ---- Update (in-place, no widget destruction) ----

    auto DoUpdateAllSlots() -> void;
    auto DoUpdateLayerSlot(FCkUIDebugger_LayerSlot& InSlot, bool InIsActive) -> void;

    // ---- Toolbar Actions ----

    auto DoExpandAll() -> void;
    auto DoCollapseAll() -> void;

    // ---- Search ----

    auto DoMatchesFilter(const FString& InLayerTag) const -> bool;

    // ---- Widgets ----

    TSharedPtr<SVerticalBox> _LayerListBox;
    TSharedPtr<SVerticalBox> _HistoryListBox;
    TSharedPtr<STextBlock> _SummaryText;
    TSharedPtr<SEditableTextBox> _SearchTextBox;
    TSharedPtr<SExpandableArea> _HistoryArea;

    // ---- Pre-allocated layer slots ----

    TArray<FCkUIDebugger_LayerSlot> _LayerSlots;
    static constexpr int32 MaxWidgetsPerLayer = 16;
    TArray<TArray<FCkUIDebugger_WidgetSlot>> _WidgetSlotPools;

    // ---- State ----

    TWeakObjectPtr<UCk_UI_PrimaryGameLayout_UE> _BoundLayout;
    TArray<FCkUIDebugger_HistoryEvent> _HistoryEvents;
    FString _SearchFilter;
    bool _IsDirty = true;
    bool _StructureDirty = true;

    static constexpr int32 MaxHistoryEvents = 100;
};

// --------------------------------------------------------------------------------------------------------------------
