#include "CkIntentDebugger/Window/SCkIntentDebugger_DevicesPanel.h"

#include "CkIntentDebugger/ViewModel/CkIntentDebugger_ViewModel.h"

#include "CkDebuggerCommon/Devices/SCkDebug_DevicesPanel.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebugger_DevicesPanel::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _ViewModel = InArgs._ViewModel;

    // One snapshot serves every device — each widget looks up only its own keys, and a device with no producer
    // (or none attached) renders dimmed rather than absent: the maintainer's rule is that every device is always
    // on screen, greyed when unavailable.
    const auto SnapshotLambda = [WeakViewModel = TWeakPtr<FCkIntentDebugger_ViewModel>{_ViewModel}]()
        -> const FCkDebug_DeviceSnapshot*
    {
        const auto ViewModel = WeakViewModel.Pin();
        return ViewModel.IsValid() ? &ViewModel->Get_DeviceSnapshot() : nullptr;
    };

    ChildSlot
    [
        SNew(SCkDebug_DevicesPanel)
            .Snapshot_Lambda(SnapshotLambda)
            .NoteText(FText::FromString(TEXT(
                "Every device edge is captured ungated; sub-frame taps coalesce to one row.")))
    ];
}

// --------------------------------------------------------------------------------------------------------------------
