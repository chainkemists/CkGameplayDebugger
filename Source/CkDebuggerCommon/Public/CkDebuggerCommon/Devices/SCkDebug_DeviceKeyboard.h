#pragma once

#include "CkDebuggerCommon/Devices/CkDebug_DeviceTypes.h"

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

// --------------------------------------------------------------------------------------------------------------------
// Full-size ANSI keyboard, drawn procedurally from the layout table — no images, crisp at any DPI, restyled by the
// suite tokens. One leaf widget paints every cap, which is what keeps 104 keys from being 104 widgets.
//
// The state vocabulary (shared with the gamepad visualizer):
//   press  = an instant FLASH — the cap snaps to the accent on its press frame and decays over a few hundred ms,
//            so a one-frame tap is still visible to a human;
//   hold   = a FILL — the cap fills toward its hold-verdict threshold when the producer knows one (a full cap IS
//            the matcher declaring a hold), or fills completely when no verdict exists;
//   minted = a brighter bezel — a key the producer carries per-frame history for. A minted key that never lights
//            is a dead pipeline; an unminted one is merely unwatched. The two must not look alike.
//
// All timing is in the SNAPSHOT's frames (LiveFrame minus edge frames) — the widget owns no clock, so it can never
// disagree with the record it renders.
// --------------------------------------------------------------------------------------------------------------------

class CKDEBUGGERCOMMON_API SCkDebug_DeviceKeyboard : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_DeviceKeyboard)
        : _Snapshot(nullptr)
    {}
        // Owned by the producer (a view-model cache); nullptr renders the board idle-dim.
        SLATE_ATTRIBUTE(const FCkDebug_DeviceSnapshot*, Snapshot)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

    virtual auto OnPaint(
        const FPaintArgs& InArgs,
        const FGeometry& InAllottedGeometry,
        const FSlateRect& InCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 InLayerId,
        const FWidgetStyle& InWidgetStyle,
        bool InParentEnabled) const -> int32 override;

    virtual auto ComputeDesiredSize(float InLayoutScaleMultiplier) const -> FVector2D override;

private:
    TAttribute<const FCkDebug_DeviceSnapshot*> _Snapshot;
};

// --------------------------------------------------------------------------------------------------------------------
