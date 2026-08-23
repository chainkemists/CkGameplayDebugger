#include "CkTextureDebugger/Window/SCkTextureDebugger_SceneAuditTable.h"
#include "CkTextureDebugger/Window/SCkTextureDebugger_TextureHealthTable.h"

#include "CkDebuggerCommon/Models/CkDebuggerModel_WorldSelector.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ck_texture_debugger_ux_tests
{
    auto Make_ComponentRow(
        UStaticMeshComponent* InComponent,
        UTexture2D* InTexture,
        FString InTextureName) -> FCkTextureDebugger_ComponentRow
    {
        auto TextureRow = FCkTextureDebugger_TextureRow{};
        TextureRow.NavigationTarget = InTexture;
        TextureRow.Health.DisplayName = MoveTemp(InTextureName);
        TextureRow.Health.AssetPath = FSoftObjectPath{InTexture};
        TextureRow.Health.CookedWidth = 1024;
        TextureRow.Health.CookedHeight = 1024;

        auto Slot = FCkTextureDebugger_MaterialSlotRow{};
        Slot.SlotIndex = 0;
        Slot.DisplayName = TEXT("SharedMaterial");
        Slot.Textures.Add(MoveTemp(TextureRow));

        auto Component = FCkTextureDebugger_ComponentRow{};
        Component.NavigationTarget = InComponent;
        Component.ActorPath = FSoftObjectPath{TEXT("/Game/Test.SharedActor")};
        Component.ActorDisplayName = TEXT("SharedActor");
        Component.ComponentDisplayName = TEXT("SharedComponent");
        Component.ComponentClassName = TEXT("StaticMeshComponent");
        Component.Kind = ECkTextureDebugger_ComponentKind::StaticMesh;
        Component.SupportsCheckerOverride = true;
        Component.MaterialSlots.Add(MoveTemp(Slot));
        return Component;
    }
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTextureDebugger_Ux_EditorWorldCatalog,
    "Ck.TextureDebugger.Ux.EditorWorldCatalog",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTextureDebugger_Ux_EditorWorldCatalog::RunTest(const FString& Parameters)
{
    auto Model = FCkDebuggerModel_WorldSelector{};
    Model.Set_IncludeEditorWorld(true);

    auto* EditorWorld = static_cast<UWorld*>(nullptr);
    if (GEngine != nullptr)
    {
        for (const auto& Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType == EWorldType::Editor && Context.World() != nullptr)
            {
                EditorWorld = Context.World();
                break;
            }
        }
    }

    TestNotNull(TEXT("Editor-context automation has an Editor world"), EditorWorld);
    if (EditorWorld == nullptr)
    { return false; }

    const auto Available = Model.Get_AvailableWorlds();
    TestTrue(TEXT("Editor-capable selector exposes the Editor world"), Available.Contains(EditorWorld));

    Model.Ensure_AutoSelect();
    TestNotNull(TEXT("Editor-capable selector auto-selects an inspectable world"), Model.Get_SelectedWorld());
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTextureDebugger_Ux_StableTables,
    "Ck.TextureDebugger.Ux.StableTables",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTextureDebugger_Ux_StableTables::RunTest(const FString& Parameters)
{
    auto ComponentA = TStrongObjectPtr<UStaticMeshComponent>{NewObject<UStaticMeshComponent>()};
    auto ComponentB = TStrongObjectPtr<UStaticMeshComponent>{NewObject<UStaticMeshComponent>()};
    auto Texture = TStrongObjectPtr<UTexture2D>{NewObject<UTexture2D>()};

    auto Snapshot = FCkTextureDebugger_LoadedWorldSnapshot{};
    Snapshot.Components.Add(ck_texture_debugger_ux_tests::Make_ComponentRow(
        ComponentA.Get(), Texture.Get(), TEXT("SharedTexture")));
    Snapshot.Components.Add(ck_texture_debugger_ux_tests::Make_ComponentRow(
        ComponentB.Get(), Texture.Get(), TEXT("SharedTexture")));

    auto AuditTable = SNew(SCkTextureDebugger_SceneAuditTable);
    AuditTable->SetSnapshot(Snapshot);
    TestEqual(TEXT("Same-named components remain distinct audit rows"), AuditTable->Get_TotalRowCount(), 2);
    TestEqual(TEXT("Both audit rows are visible without a filter"), AuditTable->Get_VisibleRowCount(), 2);

    AuditTable->SetSelectedComponent(ComponentA.Get());
    TestEqual(TEXT("Programmatic audit selection is singular"), AuditTable->Get_SelectedRowCount(), 1);
    AuditTable->SetSelectedComponent(ComponentB.Get());
    TestEqual(TEXT("Changing audit selection replaces the previous row"), AuditTable->Get_SelectedRowCount(), 1);
    AuditTable->SetSelectedComponent({});
    TestEqual(TEXT("Audit selection can be cleared"), AuditTable->Get_SelectedRowCount(), 0);

    auto HealthTable = SNew(SCkTextureDebugger_TextureHealthTable);
    HealthTable->Set_Snapshot(Snapshot);
    TestEqual(TEXT("Health keeps contextual references from distinct components"), HealthTable->Get_TotalRowCount(), 2);
    TestEqual(TEXT("Health initially shows the full loaded-world inventory"), HealthTable->Get_VisibleRowCount(), 2);

    HealthTable->Set_Snapshot(Snapshot, ComponentA.Get());
    TestEqual(TEXT("Component context highlights without filtering Health"), HealthTable->Get_VisibleRowCount(), 2);
    return true;
}

#endif
