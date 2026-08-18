#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

// ====================================================================================================================

/**
 * Operational Input HUD controls hosted by the Intent Debugger. Visual palette/border authoring stays in Style Lab;
 * this surface owns what QA records and the fast session/project knobs used while diagnosing intent.
 */
class SCkIntentDebugger_InputHudControls : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkIntentDebugger_InputHudControls) {}
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;

private:
    auto Build_ReadoutControls() -> TSharedRef<SWidget>;
    auto Build_SessionControls() -> TSharedRef<SWidget>;
    auto Build_ProjectControls() -> TSharedRef<SWidget>;
    auto OnResetReadout() -> FReply;
};

// ====================================================================================================================
