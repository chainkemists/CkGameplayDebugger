#pragma once

#include "CkDebuggerCommon/Launcher/CkDebuggerToolRegistry.h"

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------
// The rail's quick-open filter as a pure function, so the "what does the user see for this query"
// question is answerable without a Slate widget, a registry, or a live tab manager.
// --------------------------------------------------------------------------------------------------------------------

namespace ck::debugger_launcher
{
    /**
     * Case-insensitive substring match of the query against the tool's DisplayName. An empty or
     * whitespace-only query matches every tool.
     */
    auto Matches_Query(
        const FString& InQuery,
        const FCkDebuggerToolDescriptor& InTool) -> bool;

    /**
     * The visible subset, in the SAME relative order as the input. The rail relies on that order
     * twice: category grouping stays contiguous, and element 0 is the "top visible match" Enter
     * activates.
     */
    auto Filter_Tools(
        const FString& InQuery,
        const TArray<FCkDebuggerToolDescriptor>& InTools) -> TArray<FCkDebuggerToolDescriptor>;
}

// --------------------------------------------------------------------------------------------------------------------
