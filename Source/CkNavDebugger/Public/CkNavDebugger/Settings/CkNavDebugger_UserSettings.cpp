#include "CkNavDebugger_UserSettings.h"

#include "CkCore/Validation/CkIsValid.h"

#include "HAL/IConsoleManager.h"

// --------------------------------------------------------------------------------------------------------------------
// CVars MUST be registered before UCk_NavDebugger_UserSettings_UE's CDO is
// constructed, because UDeveloperSettings::ImportConsoleVariableValues runs in
// PostInitProperties and Fatal-logs when a `meta = (ConsoleVariable = ...)`
// property points at a CVar that doesn't yet exist. Static initializer is the
// only place that runs early enough — module StartupModule fires after CDOs.
//
// Defaults here must match the property defaults on the settings class. The
// settings system will overwrite the CVar with the persisted value via Import
// during CDO construction, so these are "init defaults if no config exists yet".
// --------------------------------------------------------------------------------------------------------------------

namespace ck::nav_debugger::settings_cvars
{
    static TAutoConsoleVariable<int32> CVar_AgentCapsuleMode(
        TEXT("Ck.NavDebugger.AgentCapsuleMode"),
        static_cast<int32>(ECk_NavDebugger_DrawMode::All),
        TEXT("CkNavDebugger: Which agents get a capsule visual: 0=None, 1=Selected, 2=All. Bound to UCk_NavDebugger_UserSettings_UE."),
        ECVF_Default);

    static TAutoConsoleVariable<int32> CVar_AgentPathMode(
        TEXT("Ck.NavDebugger.AgentPathMode"),
        static_cast<int32>(ECk_NavDebugger_DrawMode::All),
        TEXT("CkNavDebugger: Which agents get path-line visuals: 0=None, 1=Selected, 2=All. Bound to UCk_NavDebugger_UserSettings_UE."),
        ECVF_Default);

    static TAutoConsoleVariable<bool> CVar_DrawProjections(
        TEXT("Ck.NavDebugger.DrawProjections"),
        true,
        TEXT("CkNavDebugger: Draw vertical lines + spheres at agent / target navmesh-projection points. Bound to UCk_NavDebugger_UserSettings_UE."),
        ECVF_Default);

    static TAutoConsoleVariable<bool> CVar_DrawNavmeshBounds(
        TEXT("Ck.NavDebugger.DrawNavmeshBounds"),
        false,
        TEXT("CkNavDebugger: Draw the navmesh AABB as a wireframe box. Bound to UCk_NavDebugger_UserSettings_UE."),
        ECVF_Default);
}

// --------------------------------------------------------------------------------------------------------------------

namespace
{
    auto GetSettings() -> const UCk_NavDebugger_UserSettings_UE*
    {
        return GetDefault<UCk_NavDebugger_UserSettings_UE>();
    }

    auto GetMutableSettings() -> UCk_NavDebugger_UserSettings_UE*
    {
        return GetMutableDefault<UCk_NavDebugger_UserSettings_UE>();
    }

    // Mirror enum/bool changes back through the matching CVar so console
    // listeners stay in sync with edits driven from the panel UI. CVar absent
    // (e.g. for color fields) — silent no-op.
    auto Sync_CVarInt(const TCHAR* InName, int32 InValue) -> void
    {
        if (auto* CVar = IConsoleManager::Get().FindConsoleVariable(InName))
        { CVar->Set(InValue, ECVF_SetByConsole); }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto UCk_Utils_NavDebugger_Settings_UE::Get_AgentCapsuleMode()           -> ECk_NavDebugger_DrawMode { const auto* S = GetSettings(); return ck::IsValid(S) ? S->Get_AgentCapsuleMode()           : ECk_NavDebugger_DrawMode::All;     }
auto UCk_Utils_NavDebugger_Settings_UE::Get_AgentCapsuleColor()          -> FLinearColor              { const auto* S = GetSettings(); return ck::IsValid(S) ? S->Get_AgentCapsuleColor()          : FLinearColor::White;              }
auto UCk_Utils_NavDebugger_Settings_UE::Get_AgentCapsuleColor_Selected() -> FLinearColor              { const auto* S = GetSettings(); return ck::IsValid(S) ? S->Get_AgentCapsuleColor_Selected() : FLinearColor::White;              }
auto UCk_Utils_NavDebugger_Settings_UE::Get_AgentPathMode()              -> ECk_NavDebugger_DrawMode { const auto* S = GetSettings(); return ck::IsValid(S) ? S->Get_AgentPathMode()              : ECk_NavDebugger_DrawMode::All;     }
auto UCk_Utils_NavDebugger_Settings_UE::Get_AgentPathColor_Ready()       -> FLinearColor              { const auto* S = GetSettings(); return ck::IsValid(S) ? S->Get_AgentPathColor_Ready()       : FLinearColor::Green;              }
auto UCk_Utils_NavDebugger_Settings_UE::Get_AgentPathColor_Partial()     -> FLinearColor              { const auto* S = GetSettings(); return ck::IsValid(S) ? S->Get_AgentPathColor_Partial()     : FLinearColor::Yellow;             }
auto UCk_Utils_NavDebugger_Settings_UE::Get_AgentPathColor_Failed()      -> FLinearColor              { const auto* S = GetSettings(); return ck::IsValid(S) ? S->Get_AgentPathColor_Failed()      : FLinearColor::Red;                }
auto UCk_Utils_NavDebugger_Settings_UE::Get_AgentPathColor_Selected()    -> FLinearColor              { const auto* S = GetSettings(); return ck::IsValid(S) ? S->Get_AgentPathColor_Selected()    : FLinearColor::White;              }
auto UCk_Utils_NavDebugger_Settings_UE::Get_DrawProjections()            -> bool                      { const auto* S = GetSettings(); return ck::IsValid(S) ? S->Get_DrawProjections()            : true;                              }
auto UCk_Utils_NavDebugger_Settings_UE::Get_DrawNavmeshBounds()          -> bool                      { const auto* S = GetSettings(); return ck::IsValid(S) ? S->Get_DrawNavmeshBounds()          : false;                             }

// --------------------------------------------------------------------------------------------------------------------

auto UCk_Utils_NavDebugger_Settings_UE::Set_AgentCapsuleMode(ECk_NavDebugger_DrawMode InNewMode) -> void
{
    auto* S = GetMutableSettings();
    if (NOT ck::IsValid(S)) { return; }
    S->_AgentCapsuleMode = InNewMode;
    S->SaveConfig();
    Sync_CVarInt(TEXT("Ck.NavDebugger.AgentCapsuleMode"), static_cast<int32>(InNewMode));
}

auto UCk_Utils_NavDebugger_Settings_UE::Set_AgentPathMode(ECk_NavDebugger_DrawMode InNewMode) -> void
{
    auto* S = GetMutableSettings();
    if (NOT ck::IsValid(S)) { return; }
    S->_AgentPathMode = InNewMode;
    S->SaveConfig();
    Sync_CVarInt(TEXT("Ck.NavDebugger.AgentPathMode"), static_cast<int32>(InNewMode));
}

auto UCk_Utils_NavDebugger_Settings_UE::Set_DrawProjections(bool InNewValue) -> void
{
    auto* S = GetMutableSettings();
    if (NOT ck::IsValid(S)) { return; }
    S->_DrawProjections = InNewValue;
    S->SaveConfig();
    Sync_CVarInt(TEXT("Ck.NavDebugger.DrawProjections"), InNewValue ? 1 : 0);
}

auto UCk_Utils_NavDebugger_Settings_UE::Set_DrawNavmeshBounds(bool InNewValue) -> void
{
    auto* S = GetMutableSettings();
    if (NOT ck::IsValid(S)) { return; }
    S->_DrawNavmeshBounds = InNewValue;
    S->SaveConfig();
    Sync_CVarInt(TEXT("Ck.NavDebugger.DrawNavmeshBounds"), InNewValue ? 1 : 0);
}
