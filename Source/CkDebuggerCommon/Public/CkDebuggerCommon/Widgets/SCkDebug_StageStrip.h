#pragma once

#include "CkEditorTools/Style/CkIcons_Generated.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "Widgets/SCompoundWidget.h"

// --------------------------------------------------------------------------------------------------------------------

struct CKDEBUGGERCOMMON_API FCkDebug_StageDescriptor
{
    FText Label;
    ECk_Icon IconId = ECk_Icon::None;
    TAttribute<FText> Value;
    TAttribute<FText> Meta;
    TAttribute<ECk_Tone> Tone = ECk_Tone::Neutral;
};

/** Common live value/meta card for one stage in a debugger pipeline. */
class CKDEBUGGERCOMMON_API SCkDebug_StageCard : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_StageCard) {}
        SLATE_ARGUMENT(FCkDebug_StageDescriptor, Stage)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
};

/** Common horizontal pipeline strip; all values remain attribute-bound after construction. */
class CKDEBUGGERCOMMON_API SCkDebug_StageStrip : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCkDebug_StageStrip) {}
        SLATE_ARGUMENT(TArray<FCkDebug_StageDescriptor>, Stages)
    SLATE_END_ARGS()

    auto Construct(const FArguments& InArgs) -> void;
};

// --------------------------------------------------------------------------------------------------------------------
