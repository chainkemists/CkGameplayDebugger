#pragma once

#include "CkSchedulerDebugger/Pages/ICkSchedulerDebuggerPage.h"

class SCkSchedulerDebugger_Timeline;
class SCkSchedulerDebugger_ProcessorTree;
class SCkSchedulerDebugger_Inspector;
class FCkSchedulerDebugger_ViewModel;
class SSplitter;

// --------------------------------------------------------------------------------------------------------------------
// Combined page - tree view + timeline + inspector in a configurable layout.
// --------------------------------------------------------------------------------------------------------------------

class FCkSchedulerDebuggerPage_Combined : public ICkSchedulerDebuggerPage
{
public:
    virtual auto Get_PageName() const -> FText override;
    virtual auto Build_Content(TSharedPtr<FCkSchedulerDebugger_ViewModel> InViewModel) -> TSharedRef<SWidget> override;
    virtual auto Tick(float InDeltaTime) -> void override;
    virtual auto OnSelectionChanged(int32 InProcessorIndex) -> void override;

private:
    auto DoBuildToolbar() -> TSharedRef<SWidget>;
    auto DoOnLayoutChanged(float InTopRatio) -> void;

    TSharedPtr<FCkSchedulerDebugger_ViewModel> _ViewModel;
    TSharedPtr<SSplitter> _MainSplitter;
    float _TopRatio = 0.5f;

    enum class ELayoutPreset : uint8
    {
        Split5050,
        TreeFocus,
        TimelineFocus
    };

    ELayoutPreset _ActivePreset = ELayoutPreset::Split5050;
};

// --------------------------------------------------------------------------------------------------------------------
