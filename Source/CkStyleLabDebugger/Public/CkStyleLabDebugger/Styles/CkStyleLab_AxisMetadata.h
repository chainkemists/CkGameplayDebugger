#pragma once

#include "CoreMinimal.h"

// ====================================================================================================================

/**
 * Runtime presentation contract for FCkDebuggerStyleSelection's reflected axes.
 *
 * UProperty display metadata is editor-only, while the Style Lab also ships in Development builds.
 * Keep the authored axis labels, tooltips, and option labels here so both targets present the same
 * controls without asking a stripped FProperty/UEnum for presentation data. Property reflection
 * still owns the axis values and order.
 */
struct FCkStyleLab_AxisMetadata
{
    FName PropertyName;
    FText DisplayName;
    FText ToolTip;
};

struct FCkStyleLab_AxisOptionMetadata
{
    FName PropertyName;
    int64 Value;
    FText DisplayName;
};

namespace ck::style_lab
{
    CKSTYLELABDEBUGGER_API auto Find_AxisMetadata(FName InPropertyName) -> const FCkStyleLab_AxisMetadata*;
    CKSTYLELABDEBUGGER_API auto Find_AxisOptionLabel(FName InPropertyName, int64 InValue) -> const FText*;
}

// ====================================================================================================================
