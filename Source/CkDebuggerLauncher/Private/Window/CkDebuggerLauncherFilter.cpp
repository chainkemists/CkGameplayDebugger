#include "Window/CkDebuggerLauncherFilter.h"

#include "CkCore/Macros/CkMacros.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck::debugger_launcher
{
    auto
        Matches_Query(
            const FString& InQuery,
            const FCkDebuggerToolDescriptor& InTool)
        -> bool
    {
        const auto Query = InQuery.TrimStartAndEnd();

        if (Query.IsEmpty())
        { return true; }

        return InTool.Get_DisplayName().ToString().Contains(Query, ESearchCase::IgnoreCase);
    }

    auto
        Filter_Tools(
            const FString& InQuery,
            const TArray<FCkDebuggerToolDescriptor>& InTools)
        -> TArray<FCkDebuggerToolDescriptor>
    {
        auto Visible = TArray<FCkDebuggerToolDescriptor>{};
        Visible.Reserve(InTools.Num());

        for (const auto& Tool : InTools)
        {
            if (NOT Matches_Query(InQuery, Tool))
            { continue; }

            Visible.Add(Tool);
        }

        return Visible;
    }
}

// --------------------------------------------------------------------------------------------------------------------
