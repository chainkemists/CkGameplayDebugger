#pragma once

#include "CkUICore/Types/CkUI_Types.h"

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"

class SCkDebug_CategoryDot;
class SCkDebug_StatusPill;
class FCkUiCollection;
class FCkUiTreeCollection;
class FCkUiView;
class SBox;
class UCk_UI_Layout_Subsystem_UE;
class UCk_UI_LayerStack_UE;
class UCk_UI_PrimaryGameLayout_UE;

// --------------------------------------------------------------------------------------------------------------------
// History event
// --------------------------------------------------------------------------------------------------------------------

struct FCkUIDebugger_HistoryEvent
{
    uint64 Key = 0;
    double Timestamp = 0.0;
    FString Description;
};

// --------------------------------------------------------------------------------------------------------------------
// CK UI Layer Debugger window — placed inside a NomadTab.
//
// Event-driven: binds to layout delegates and publishes retained authored tree nodes.
// --------------------------------------------------------------------------------------------------------------------

class CKUIDEBUGGER_API SCkUIDebuggerWindow : public SCkDebugger_WindowBase
{
public:
    static const FName WindowId;

    SLATE_BEGIN_ARGS(SCkUIDebuggerWindow) {}
    SLATE_END_ARGS()

    ~SCkUIDebuggerWindow();

    auto Construct(const FArguments& InArgs) -> void;
    auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    virtual auto Get_WindowId() const -> FName override { return WindowId; }
    virtual auto Get_WindowDisplayName() const -> FText override { return FText::FromString(TEXT("UI")); }
    auto Get_HistoryView() const -> TSharedPtr<FCkUiView> { return _HistoryView; }
    auto Get_HistoryCollection() const -> TSharedPtr<const FCkUiCollection> { return _HistoryCollection; }
    auto Get_LayerView() const -> TSharedPtr<FCkUiView> { return _LayerView; }
    auto Get_LayerCollection() const -> TSharedPtr<const FCkUiTreeCollection> { return _LayerCollection; }
    auto Get_SummaryView() const -> TSharedPtr<FCkUiView> { return _SummaryView; }
    auto Get_CommandView() const -> TSharedPtr<FCkUiView> { return _CommandView; }
    auto Get_ForcedLayerRefreshGeneration() const -> uint64 { return _ForcedLayerRefreshGeneration; }

protected:
    virtual auto OnStyleRevisionChanged() -> void override;

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

    auto DoBuildLayerView() -> void;
    auto DoPublishLayerNodes() -> void;
    auto DoPollLayerFiles(double InCurrentTime) -> void;
    auto DoBuildSummaryView() -> void;
    auto DoPollSummaryFiles(double InCurrentTime) -> void;
    auto DoBuildHistoryView() -> void;
    auto DoPublishHistoryRecords() -> void;
    auto DoPollHistoryFiles(double InCurrentTime) -> void;
    auto DoAppendHistoryEvent(FString InDescription) -> void;
    auto DoBuildCommandView() -> void;
    auto DoPollCommandFiles(double InCurrentTime) -> void;

    auto DoUpdateAllSlots() -> void;

    // ---- Toolbar Actions ----

    auto DoExpandAll() -> void;
    auto DoCollapseAll() -> void;
    auto DoCycleNameDepth(int32 InDirection) -> void;

    // ---- Search ----

    auto DoMatchesFilter(const FString& InLayerTag) const -> bool;

    // ---- Name shortening ----

    // Depth-shortens via SCkDebug_NameLabel::Get_ShortName and tracks the
    // largest segment count seen so the command-bar cycler's Max stays honest.
    auto DoShortName(const FString& InFullName) -> FString;

    // ---- Widgets ----

    TSharedPtr<SBox> _LayerHost;
    TSharedPtr<SBox> _HistoryHost;
    TSharedPtr<SBox> _SummaryHost;
    TSharedPtr<SBox> _CommandHost;

    static constexpr int32 MaxWidgetsPerLayer = 16;

    // ---- State ----

    TWeakObjectPtr<UCk_UI_PrimaryGameLayout_UE> _BoundLayout;
    TArray<FCkUIDebugger_HistoryEvent> _HistoryEvents;
    TSharedPtr<FCkUiTreeCollection> _LayerCollection;
    TSharedPtr<FCkUiView> _LayerView;
    TSharedPtr<FCkUiCollection> _HistoryCollection;
    TSharedPtr<FCkUiView> _HistoryView;
    TSharedPtr<FCkUiView> _SummaryView;
    TSharedPtr<FCkUiView> _CommandView;
    FString _SearchFilter;
    bool _IsDirty = true;
    bool _IsPostLayerTransitionRefreshPending = false;
    bool _StructureDirty = true;
    bool _ShowActiveLayerOnly = false;
    uint64 _ForcedLayerRefreshGeneration = 0;
    TSet<FString> _InitializedLayerRootKeys;
    uint64 _NextHistoryKey = 1;
    double _NextLayerPollSeconds = 0.0;
    double _NextHistoryPollSeconds = 0.0;
    double _NextSummaryPollSeconds = 0.0;
    double _NextCommandPollSeconds = 0.0;

    // Updated only through the existing refresh-gated DoUpdateAllSlots path.
    bool _HasActiveLayout = false;
    FText _SummaryActiveTag;
    FText _SummaryInputMode;
    FText _SummaryLayerCount;

    // Display-name verbosity for widget class names + layer tags (0 = full).
    int32 _NameDepth = 1;
    int32 _MaxNameSegments = 1;

    static constexpr int32 MaxHistoryEvents = 100;
};

// --------------------------------------------------------------------------------------------------------------------
