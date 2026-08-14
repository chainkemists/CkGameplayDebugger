#pragma once

#include "CoreMinimal.h"
#include "CkDebuggerCommon/Window/SCkDebugger_WindowBase.h"

class SCkStyleLab_ControlsPane;
class SCkStyleLab_SamplePane;

// --------------------------------------------------------------------------------------------------------------------
// CK Debugger Style Lab — the iteration vehicle for the debugger-wide style axes.
//
// Left: one canned worst-case document rendered through ck::debug_axes. Right: every axis plus the
// curated profiles. Changing an axis writes the shared per-user settings, so real debugger tabs
// follow live; the Lab's own sample rebuilds on the same signal.
//
// No ECS handles, no PIE data, nothing cached that outlives a session — so there is no EndPIE
// clearing to do. The gated Tick exists only to notice a settings revision that came from
// somewhere else (Editor Preferences), never to rebuild on a timer.
// --------------------------------------------------------------------------------------------------------------------

class SCkStyleLabWindow : public SCkDebugger_WindowBase
{
public:
    static const FName WindowId;

    SLATE_BEGIN_ARGS(SCkStyleLabWindow) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
    auto Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void override;

    virtual auto Get_WindowId() const -> FName override { return WindowId; }
    virtual auto Get_WindowDisplayName() const -> FText override { return FText::FromString(TEXT("Style Lab")); }

private:
    auto Build_MenuActions() -> TSharedRef<SWidget>;
    auto Get_StatusText() const -> FText;
    auto OnSampleSelectionChanged() -> void;

    auto Get_ShowAllTones() const -> bool;
    auto Set_ShowAllTones(bool InShowAllTones) -> void;

    TSharedPtr<SCkStyleLab_SamplePane>   _SamplePane;
    TSharedPtr<SCkStyleLab_ControlsPane> _ControlsPane;

    uint32 _LastSeenRevision = 0;
};

// --------------------------------------------------------------------------------------------------------------------
