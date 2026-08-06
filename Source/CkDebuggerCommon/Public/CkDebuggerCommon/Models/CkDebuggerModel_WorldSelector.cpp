#include "CkDebuggerModel_WorldSelector.h"

#include "Engine/Engine.h"
#include "Engine/World.h"

#include "CkCore/Validation/CkIsValid.h"

// ====================================================================================================================

FCkDebuggerModel_WorldSelector::FCkDebuggerModel_WorldSelector()
{
    WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddRaw(
        this, &FCkDebuggerModel_WorldSelector::HandleWorldCleanup);
}

FCkDebuggerModel_WorldSelector::~FCkDebuggerModel_WorldSelector()
{
    if (WorldCleanupHandle.IsValid())
    {
        FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
        WorldCleanupHandle.Reset();
    }
}

// ====================================================================================================================

auto FCkDebuggerModel_WorldSelector::Set_SelectedWorld(UWorld* InWorld) -> void
{
    if (SelectedWorld.Get() == InWorld)
    { return; }

    SelectedWorld = InWorld;
    BroadcastWorldChanged();
}

auto FCkDebuggerModel_WorldSelector::Get_SelectedWorld() const -> UWorld*
{
    return SelectedWorld.Get();
}

auto FCkDebuggerModel_WorldSelector::Get_AvailableWorlds() const -> TArray<UWorld*>
{
    auto Worlds = TArray<UWorld*>{};

    if (ck::Is_NOT_Valid(GEngine))
    { return Worlds; }

    const auto WorldContexts = GEngine->GetWorldContexts();

    for (auto Index = 0; Index < WorldContexts.Num(); ++Index)
    {
        const auto& ContextWorld = WorldContexts[Index].World();

        if (ck::Is_NOT_Valid(ContextWorld))
        { continue; }

        if (auto GameInstance = ContextWorld->GetGameInstance();
            ck::IsValid(GameInstance))
        {
            Worlds.Emplace(ContextWorld);
        }
    }

    return Worlds;
}

auto FCkDebuggerModel_WorldSelector::Ensure_AutoSelect() -> bool
{
    // Already have a live selection — nothing to do.
    if (ck::IsValid(Get_SelectedWorld()))
    { return false; }

    const auto AvailableWorlds = Get_AvailableWorlds();
    if (AvailableWorlds.IsEmpty())
    { return false; }

    Set_SelectedWorld(AvailableWorlds[0]);
    return true;
}

auto FCkDebuggerModel_WorldSelector::BroadcastWorldChanged() -> void
{
    OnWorldChanged.Broadcast(Get_SelectedWorld());
}

auto FCkDebuggerModel_WorldSelector::HandleWorldCleanup(UWorld* InWorld, bool, bool) -> void
{
    if (SelectedWorld.Get() != InWorld)
    { return; }

    // Broadcast while the world still has a stable identity, before any
    // registry-backed state owned by debugger consumers can outlive it.
    SelectedWorld.Reset();
    BroadcastWorldChanged();
}
