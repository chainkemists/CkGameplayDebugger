#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

struct FCkDebuggerStyleSelection;
class SBorder;

// ====================================================================================================================
// The Style Lab's LEFT pane: one realistic worst-case debugger document, not a widget grid.
//
// Everything on it — the overlay focus card, the tree rows, the inspector sections — is composed
// through ck::debug_axes, so flipping an axis in the right-hand controls moves this document the
// same way it moves the real debuggers. Every value is a hand-authored constant: no world, no ECS
// handle, no PIE dependency, so the pane renders identically on a cold editor.
//
// Rebuilds happen ONLY on an explicit RequestRebuild (user changed an axis, or the window's gated
// tick saw the settings revision move) — never from the tick path itself.
// ====================================================================================================================

class SCkStyleLab_SamplePane : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkStyleLab_SamplePane) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    auto RequestRebuild() -> void;

    auto Get_ShowAllTones() const -> bool { return _ShowAllTones; }
    auto Set_ShowAllTones(bool InShowAllTones) -> void;

private:
    auto Build_Body() const -> TSharedRef<SWidget>;
    auto Build_FocusCard() const -> TSharedRef<SWidget>;
    auto Build_TreeRows(const FCkDebuggerStyleSelection& InSelection) const -> TSharedRef<SWidget>;
    auto Build_Inspector(const FCkDebuggerStyleSelection& InSelection) const -> TSharedRef<SWidget>;
    auto Build_Separator(const FCkDebuggerStyleSelection& InSelection) const -> TSharedRef<SWidget>;

    TSharedPtr<SBorder> _Root;

    bool _ShowAllTones = false;
};

// ====================================================================================================================
