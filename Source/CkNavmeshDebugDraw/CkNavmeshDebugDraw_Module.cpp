#include "CkNavmeshDebugDraw_Module.h"

#include "CkNavmeshDebugDraw/CkNavmeshDebugDraw_Log.h"
#include "CkNavmeshDebugDraw/Subsystem/CkNavmeshDebugDraw_Subsystem.h"

#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/Validation/CkIsValid.h"

#include <Engine/World.h>
#include <HAL/IConsoleManager.h>
#include <UObject/UObjectIterator.h>

// --------------------------------------------------------------------------------------------------------------------

struct FCkNavmeshDebugDrawModule::FImpl
{
#if WITH_CK_NAVMESH_DEBUG_DRAW
    TUniquePtr<FAutoConsoleCommandWithWorldAndArgs> _Command;
#endif
};

// --------------------------------------------------------------------------------------------------------------------

#if WITH_CK_NAVMESH_DEBUG_DRAW
namespace ck_navmesh_debug_draw_module
{
auto
LogStatus(UCk_NavmeshDebugDraw_Subsystem_UE& InSubsystem) -> void
{
    UE_LOG(CkNavmeshDebugDraw, Display,
           TEXT("Ck navmesh draw: enabled=%d scanning=%d preview_scan=%d settling=%d ticker=%d preview_ticker=%d settle_ticker=%d ghost_fade_ticker=%d buckets=%d stale_buckets=%d preview_triangles=%d ghost_triangles=%d processed_tiles=%llu proxy_replacements=%llu"),
           static_cast<int32>(InSubsystem.Get_IsEnabled()),
           static_cast<int32>(InSubsystem.Get_IsScanning()),
           static_cast<int32>(InSubsystem.Get_IsAreaPreviewScanning()),
           static_cast<int32>(InSubsystem.Get_IsWaitingForNavigationSettle()),
           static_cast<int32>(InSubsystem.Get_HasActiveWorkTicker()),
           static_cast<int32>(InSubsystem.Get_HasActiveAreaPreviewTicker()),
           static_cast<int32>(InSubsystem.Get_HasActiveNavigationSettleTicker()),
           static_cast<int32>(InSubsystem.Get_HasActiveGhostFadeTicker()),
           InSubsystem.Get_RetainedBucketCount(),
           InSubsystem.Get_StaleBucketCount(),
           InSubsystem.Get_AreaPreviewTriangleCount(),
           InSubsystem.Get_GhostTriangleCount(),
           static_cast<unsigned long long>(InSubsystem.Get_ProcessedTileCount()),
           static_cast<unsigned long long>(InSubsystem.Get_ProxyReplacementCount()));
}

auto
HandleCommand(
    const TArray<FString>& InArgs,
    UWorld* InWorld) -> void
{
    const auto WorldIsValid = ck::IsValid(InWorld) && InWorld->IsGameWorld();
    CK_ENSURE_IF_NOT(WorldIsValid, TEXT("ck.Navmesh.DebugDraw requires an active game world"))
    {
        return;
    }

    auto* Subsystem = InWorld->GetSubsystem<UCk_NavmeshDebugDraw_Subsystem_UE>();
    const auto SubsystemIsValid = ck::IsValid(Subsystem);
    CK_ENSURE_IF_NOT(SubsystemIsValid,
                     TEXT("ck.Navmesh.DebugDraw could not resolve its subsystem in world [{}]"),
                     InWorld)
    {
        return;
    }

    const auto Argument = InArgs.IsEmpty() ? FString{TEXT("toggle")} : InArgs[0].ToLower();
    if (Argument == TEXT("1") || Argument == TEXT("on"))
    {
        Subsystem->Set_IsEnabled(true);
    }
    else if (Argument == TEXT("0") || Argument == TEXT("off"))
    {
        Subsystem->Set_IsEnabled(false);
    }
    else if (Argument == TEXT("toggle"))
    {
        Subsystem->Set_IsEnabled(NOT Subsystem->Get_IsEnabled());
    }
    else if (Argument == TEXT("refresh"))
    {
        Subsystem->Request_Refresh();
    }
    else if (Argument != TEXT("status"))
    {
        UE_LOG(CkNavmeshDebugDraw, Warning,
               TEXT("Usage: ck.Navmesh.DebugDraw [on|off|toggle|refresh|status]"));
        return;
    }

    LogStatus(*Subsystem);
}
} // namespace ck_navmesh_debug_draw_module
#endif

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkNavmeshDebugDrawModule::
    StartupModule()
    -> void
{
    _Impl = MakeUnique<FImpl>();

#if WITH_CK_NAVMESH_DEBUG_DRAW
    _Impl->_Command = MakeUnique<FAutoConsoleCommandWithWorldAndArgs>(
        TEXT("ck.Navmesh.DebugDraw"),
        TEXT("Draw retained Recast walkable surfaces. Args: on, off, toggle, refresh, status."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ck_navmesh_debug_draw_module::HandleCommand));
#endif
}

auto
    FCkNavmeshDebugDrawModule::
    ShutdownModule()
    -> void
{
#if WITH_CK_NAVMESH_DEBUG_DRAW
    for (TObjectIterator<UCk_NavmeshDebugDraw_Subsystem_UE> It; It; ++It)
    {
        if (NOT It->HasAnyFlags(RF_ClassDefaultObject))
        {
            It->Set_IsEnabled(false);
        }
    }
    if (_Impl.IsValid())
    {
        _Impl->_Command.Reset();
    }
#endif

    _Impl.Reset();
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_MODULE(FCkNavmeshDebugDrawModule, CkNavmeshDebugDraw)
