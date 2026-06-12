#include "CkDebug_NameClean_Utils.h"

#include "CkDebuggerCommon/Settings/CkDebuggerSettings.h"

// ====================================================================================================================

namespace ck::DebugNameClean
{

auto
    Get_CleanName(
        const FString& InRaw)
    -> FString
{
    auto Clean = InRaw;

    // User-configured strip patterns FIRST (Project Settings → Ck → Debugger),
    // before the built-in strips below — so a full-prefix pattern like
    // "Default__Ck_EntityScript_GoapGym_" still finds its text intact.
    if (const auto* Settings = UCkDebuggerSettings::Get())
    {
        for (const auto& Pattern : Settings->EntityNameStripPatterns)
        {
            if (Pattern.IsEmpty()) { continue; }
            Clean.ReplaceInline(*Pattern, TEXT(""), ESearchCase::CaseSensitive);
        }
    }

    // Strip ANYWHERE in the string — debug names frequently embed other names
    // (e.g. "SceneNode(Default__Ck_EntityScript_GymStation)").
    Clean.ReplaceInline(TEXT("Default__"), TEXT(""), ESearchCase::CaseSensitive);
    Clean.ReplaceInline(TEXT("Ck_EntityScript_"), TEXT(""), ESearchCase::CaseSensitive);

    // Strip the "_0" first-instance suffix and the "_C" generated-class suffix,
    // in either nesting order ("Foo_C_0" and "Foo_0_C" both reduce to "Foo").
    // Higher instance indices ("_1", "_2", …) are kept — they disambiguate
    // multi-instance PIE.
    for (auto Pass = 0; Pass < 2; ++Pass)
    {
        Clean.RemoveFromEnd(TEXT("_0"));
        Clean.RemoveFromEnd(TEXT("_C"));
    }

    return Clean;
}

} // namespace ck::DebugNameClean

// ====================================================================================================================
