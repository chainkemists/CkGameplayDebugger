#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================
// Keyboard + mouse + gamepad visual for the Input Debugger: every cell is one physical FKey whose
// color tracks the window's live state through the queried delegates (pressed / mapped / rebound /
// filtered), OBS-keystroke-overlay style. Clicking a cell toggles the window's key filter; the
// tooltip resolves lazily on hover.
//
// The widget is built ONCE — all state flows through per-paint attribute lambdas, so there is no
// refresh path to gate and no rebuild flicker (ck-slate-tools §1).
// ====================================================================================================================

DECLARE_DELEGATE_RetVal_OneParam(bool, FCkInputDebugger_KeyPredicate, const FKey&);
DECLARE_DELEGATE_RetVal_OneParam(FText, FCkInputDebugger_KeyTooltip, const FKey&);
DECLARE_DELEGATE_OneParam(FCkInputDebugger_KeyClicked, const FKey&);

class SCkInputDebugger_DeviceVisual : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkInputDebugger_DeviceVisual) {}
        SLATE_EVENT(FCkInputDebugger_KeyPredicate, IsKeyPressed)
        SLATE_EVENT(FCkInputDebugger_KeyPredicate, IsKeyMapped)
        SLATE_EVENT(FCkInputDebugger_KeyPredicate, IsKeyRebound)
        SLATE_EVENT(FCkInputDebugger_KeyPredicate, IsKeyFiltered)
        SLATE_EVENT(FCkInputDebugger_KeyTooltip,   KeyTooltip)
        SLATE_EVENT(FCkInputDebugger_KeyClicked,   OnKeyClicked)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

private:
    auto BuildKeyboard() -> TSharedRef<SWidget>;
    auto BuildMouse() -> TSharedRef<SWidget>;
    auto BuildGamepad() -> TSharedRef<SWidget>;
    auto BuildLegend() -> TSharedRef<SWidget>;

    auto BuildKeyCell(const FString& InLabel, const FKey& InKey, float InWidthFactor) -> TSharedRef<SWidget>;
    auto BuildDeviceBox(const FString& InLabel, const TSharedRef<SWidget>& InContent) -> TSharedRef<SWidget>;

    auto Get_CellBackground(FKey InKey) const -> FSlateColor;
    auto Get_CellForeground(FKey InKey) const -> FSlateColor;

private:
    FCkInputDebugger_KeyPredicate _IsKeyPressed;
    FCkInputDebugger_KeyPredicate _IsKeyMapped;
    FCkInputDebugger_KeyPredicate _IsKeyRebound;
    FCkInputDebugger_KeyPredicate _IsKeyFiltered;
    FCkInputDebugger_KeyTooltip   _KeyTooltip;
    FCkInputDebugger_KeyClicked   _OnKeyClicked;
};

// ====================================================================================================================
