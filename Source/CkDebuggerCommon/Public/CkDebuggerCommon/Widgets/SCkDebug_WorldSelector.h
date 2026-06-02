#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FCkDebuggerModel_WorldSelector;
class SHorizontalBox;
class SBox;

// ====================================================================================================================
// Shared "World Selection" widget — a button-strip with one button per available
// PIE/Game world, labelled by NetMode. The selected world is highlighted; clicking
// a button switches the inspected world via FCkDebuggerModel_WorldSelector.
//
// Originally lived inside the ECS debugger's entity-list panel; lifted here so every
// CK debugger window can share one consistent control instead of hand-rolling world
// resolution.
//
// Two layout forms (via .ShowHeaderLabel):
//   - true  → boxed vertical form with a "World Selection" title (ECS left sidebar).
//   - false → compact horizontal strip with no title (toolbars of the other debuggers).
//
// The strip structure is rebuilt only when the worlds LIST changes by identity (polled
// at 1 Hz in Tick — a PIE stop→restart can reuse the same count with new UWorld*).
// The selected-state highlight is a per-paint TAttribute binding, so a click never
// triggers a structural rebuild.
//
// Right-click any world button → "Copy Text" (the NetMode label).
// ====================================================================================================================

class CKDEBUGGERCOMMON_API SCkDebug_WorldSelector : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_WorldSelector)
        : _ShowHeaderLabel(false)
        {}
        SLATE_ARGUMENT(bool, ShowHeaderLabel)
        // Fired whenever the selected world actually changes (click or auto-select).
        SLATE_EVENT(FSimpleDelegate, OnWorldChanged)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs, TSharedPtr<FCkDebuggerModel_WorldSelector> InModel) -> void;

    virtual auto Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime) -> void override;

private:
    auto Build_ButtonStrip() -> TSharedRef<SWidget>;
    auto OnWorldButtonClicked(TWeakObjectPtr<UWorld> InWorldWeak) -> FReply;
    auto OnWorldButtonRightClicked(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, FString InLabel) -> FReply;

    TSharedPtr<FCkDebuggerModel_WorldSelector> _Model;
    TSharedPtr<SBox> _StripContainer;
    FSimpleDelegate _OnWorldChanged;

    // Sidebar form (true) fills its container width evenly; compact toolbar form
    // (false) sizes each button to its label so long NetMode names aren't clipped.
    bool _ShowHeaderLabel = false;

    TArray<TWeakObjectPtr<UWorld>> _LastKnownWorlds;
    float _TimeSinceWorldCheck = 0.0f;
    static constexpr float WorldCheckInterval = 1.0f;
};

// ====================================================================================================================
