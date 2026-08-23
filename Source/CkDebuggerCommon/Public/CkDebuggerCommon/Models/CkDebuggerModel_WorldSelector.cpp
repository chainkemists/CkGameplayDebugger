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

auto
    FCkDebuggerModel_WorldSelector::
    Set_SelectedWorld(
        UWorld* InWorld)
    -> void
{
    if (SelectedWorld.Get() == InWorld)
    {
        SelectedWorldIsAutomatic = false;
        return;
    }

    SelectedWorld = InWorld;
    SelectedWorldIsAutomatic = false;
    BroadcastWorldChanged();
}

auto
    FCkDebuggerModel_WorldSelector::
    Get_SelectedWorld() const
    -> UWorld*
{
    return SelectedWorld.Get();
}

auto
    FCkDebuggerModel_WorldSelector::
    Get_AvailableWorlds() const
    -> TArray<UWorld*>
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

        const auto HasGameInstance = ck::IsValid(ContextWorld->GetGameInstance());
        const auto IsInspectableEditorWorld = IncludeEditorWorld && WorldContexts[Index].WorldType == EWorldType::Editor;

        if (HasGameInstance || IsInspectableEditorWorld)
        {
            Worlds.Emplace(ContextWorld);
        }
    }

    return Worlds;
}

auto
    FCkDebuggerModel_WorldSelector::
    Set_IncludeEditorWorld(
        bool InIncludeEditorWorld)
    -> void
{
    IncludeEditorWorld = InIncludeEditorWorld;
}

auto
    FCkDebuggerModel_WorldSelector::
    Ensure_AutoSelect()
    -> bool
{
    // Preserve the legacy game-world-only policy for existing debugger consumers.
    if (NOT IncludeEditorWorld && ck::IsValid(Get_SelectedWorld()))
    { return false; }

    const auto AvailableWorlds = Get_AvailableWorlds();
    if (AvailableWorlds.IsEmpty())
    { return false; }

    auto* Selected = Get_SelectedWorld();
    const auto* FirstPlayableWorldEntry = AvailableWorlds.FindByPredicate([](const UWorld* InWorld)
    {
        return ck::IsValid(InWorld) && ck::IsValid(InWorld->GetGameInstance());
    });
    auto* FirstPlayableWorld = FirstPlayableWorldEntry != nullptr ? *FirstPlayableWorldEntry : nullptr;

    // An Editor-capable tool follows PIE/Game as soon as it appears, but it never
    // steals a user's explicit selection between multiple live playable worlds.
    if (FirstPlayableWorld != nullptr
        && (ck::Is_NOT_Valid(Selected)
            || (SelectedWorldIsAutomatic && ck::Is_NOT_Valid(Selected->GetGameInstance()))))
    {
        Set_AutoSelectedWorld(FirstPlayableWorld);
        return true;
    }

    if (ck::IsValid(Selected) && AvailableWorlds.Contains(Selected))
    { return false; }

    Set_AutoSelectedWorld(AvailableWorlds[0]);
    return true;
}

auto
    FCkDebuggerModel_WorldSelector::
    Set_AutoSelectedWorld(
        UWorld* InWorld)
    -> void
{
    if (SelectedWorld.Get() == InWorld)
    {
        SelectedWorldIsAutomatic = true;
        return;
    }

    SelectedWorld = InWorld;
    SelectedWorldIsAutomatic = true;
    BroadcastWorldChanged();
}

auto
    FCkDebuggerModel_WorldSelector::
    BroadcastWorldChanged()
    -> void
{
    OnWorldChanged.Broadcast(Get_SelectedWorld());
}

auto
    FCkDebuggerModel_WorldSelector::
    HandleWorldCleanup(
        UWorld* InWorld,
        bool,
        bool)
    -> void
{
    if (SelectedWorld.Get() != InWorld)
    { return; }

    // Broadcast while the world still has a stable identity, before any
    // registry-backed state owned by debugger consumers can outlive it.
    SelectedWorld.Reset();
    SelectedWorldIsAutomatic = false;
    BroadcastWorldChanged();
}
