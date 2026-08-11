#include "CkSaveDebugger_VisualizerRetained.h"

#if WITH_EDITOR

#include "CkCore/EditorOnly/CkEditorOnly_Utils.h"
#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Markers/CkDebug_PmgGizmoSet.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/EntityScript/CkEntityScript.h"
#include "CkEcs/EntityScript/CkEntityScript_Utils.h"
#include "CkEcs/Snapshot/CkSnapshot_HandleWalk.h"
#include "CkEcs/Subsystem/CkEcsEditor_Subsystem.h"

#include "CkEcsExt/Transform/CkTransform_Utils.h"

#include "CkIsmRenderer/Proxy/CkIsmProxy_Utils.h"
#include "CkIsmRenderer/Renderer/CkIsmRenderer_TransientFactory.h"

#include <Components/StaticMeshComponent.h>
#include <CoreGlobals.h>
#include <Engine/BlueprintGeneratedClass.h>
#include <Engine/InheritableComponentHandler.h>
#include <Engine/SCS_Node.h>
#include <Engine/SimpleConstructionScript.h>
#include <Engine/World.h>
#include <GameFramework/Actor.h>
#include <Misc/CoreDelegates.h>
#include <Serialization/MemoryReader.h>
#include <Serialization/ObjectAndNameAsStringProxyArchive.h>
#include <UObject/Package.h>

// --------------------------------------------------------------------------------------------------------------------

namespace ck_save_debugger_visualizer_retained
{
    struct FPendingTransform
    {
        FCk_Handle Entity;
        FTransform Target = FTransform::Identity;
        int32 RemainingAttempts = 300;
    };

    auto GRootEntity = FCk_Handle{};
    auto GArchetypeKeepAlive = TArray<TStrongObjectPtr<UCk_EntityScript_UE>>{};
    auto GPendingTransforms = TArray<FPendingTransform>{};
    auto GPendingDestroyRoots = TArray<FCk_Handle>{};
    auto GEndFrameHandle = FDelegateHandle{};
    auto GGizmoSet = FCkDebug_PmgGizmoSet{};

    // ----------------------------------------------------------------------------------------------------------------

    auto
    Get_IsMutationSafe(
        const UWorld* InWorld) -> bool
    {
        if (ck::Is_NOT_Valid(InWorld))
        { return false; }

        const auto* EditorSubsystem = InWorld->GetSubsystem<UCk_EditorEcsWorld_Subsystem_UE>();
        return ck::IsValid(EditorSubsystem, ck::IsValid_Policy_NullptrOnly{})
            && EditorSubsystem->Get_IsEditorEcsMutationSafe();
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
    UnhookEndFrameIfIdle() -> void
    {
        if (GPendingTransforms.IsEmpty() && GPendingDestroyRoots.IsEmpty() && GEndFrameHandle.IsValid())
        {
            FCoreDelegates::OnEndFrame.Remove(GEndFrameHandle);
            GEndFrameHandle = FDelegateHandle{};
        }
    }

    auto
    OnEndFrame_DrainPending() -> void
    {
        for (auto Index = GPendingDestroyRoots.Num() - 1; Index >= 0; --Index)
        {
            auto& Root = GPendingDestroyRoots[Index];
            if (ck::Is_NOT_Valid(Root))
            {
                GPendingDestroyRoots.RemoveAt(Index);
                continue;
            }

            const auto* World = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(Root);
            if (NOT Get_IsMutationSafe(World))
            { continue; }

            UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(Root);
            GPendingDestroyRoots.RemoveAt(Index);
        }

        for (auto Index = GPendingTransforms.Num() - 1; Index >= 0; --Index)
        {
            auto& Pending = GPendingTransforms[Index];

            if (ck::Is_NOT_Valid(Pending.Entity) || --Pending.RemainingAttempts <= 0)
            {
                GPendingTransforms.RemoveAt(Index);
                continue;
            }

            if (NOT UCk_Utils_Transform_UE::Has(Pending.Entity))
            { continue; }

            auto TransformHandle = UCk_Utils_Transform_UE::CastChecked(Pending.Entity);
            UCk_Utils_Transform_UE::Request_SetTransform(
                TransformHandle, FCk_Request_Transform_SetTransform{Pending.Target}, {});

            GPendingTransforms.RemoveAt(Index);
        }

        UCk_Utils_EditorOnly_UE::Request_RedrawLevelEditingViewports();
        UnhookEndFrameIfIdle();
    }

    auto
    HookEndFrame() -> void
    {
        if (NOT GEndFrameHandle.IsValid())
        { GEndFrameHandle = FCoreDelegates::OnEndFrame.AddStatic(&OnEndFrame_DrainPending); }
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** The read-side mirror of the capture's spawn-params serialization: FInstancedStruct through a
     *  name-as-string proxy, then the handle walk to consume the trailing saved-id stream. Handles are then
     *  NULLED rather than left as raw saved ids — a raw id would alias whatever unrelated entity happens to
     *  occupy that slot in the editor registry, which is strictly worse than an invalid handle. */
    auto
    TryDecode_SpawnParams(
        const TArray<uint8>& InBytes,
        FInstancedStruct& OutParams) -> bool
    {
        if (InBytes.IsEmpty())
        { return true; }

        auto Reader = FMemoryReader{InBytes, /*bIsPersistent=*/true};
        constexpr auto LoadIfFindFails = true;
        auto Proxy = FObjectAndNameAsStringProxyArchive{Reader, LoadIfFindFails};
        Proxy.ArIsSaveGame = false;
        Proxy.SetIsPersistent(true);

        OutParams.Serialize(Proxy);

        if (NOT OutParams.IsValid() || Reader.IsError())
        {
            OutParams.Reset();
            return false;
        }

        auto Context = ck::FSnapshotContext{};
        ck::snapshot::RemapHandles(OutParams.GetScriptStruct(), OutParams.GetMutableMemory(), Proxy, Context);

        ck::snapshot::ForEachHandle(OutParams.GetScriptStruct(), OutParams.GetMutableMemory(),
            [](FCk_Handle& InHandle) -> void
            {
                InHandle = FCk_Handle{};
            });

        return true;
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** The spawner's transform-property fallback list (ck::entityspawner::TryResolveDefaultTransformProperty):
     *  scripts that seed their Transform fragment from a spawn-transform property compose their children at the
     *  right place only when the value is set BEFORE Construct runs. */
    auto
    TryInject_SpawnTransform(
        UCk_EntityScript_UE* InArchetype,
        const FTransform& InTransform) -> bool
    {
        static const FName PropertyNames[] =
        {
            FName{TEXT("SpawnTransform")},
            FName{TEXT("_SpawnTransform")},
            FName{TEXT("InitialTransform")},
            FName{TEXT("_InitialTransform")},
        };

        for (const auto& PropertyName : PropertyNames)
        {
            const auto* Property = CastField<FStructProperty>(InArchetype->GetClass()->FindPropertyByName(PropertyName));
            if (Property == nullptr || Property->Struct != TBaseStructure<FTransform>::Get())
            { continue; }

            *Property->ContainerPtrToValuePtr<FTransform>(InArchetype) = InTransform;
            return true;
        }

        return false;
    }

    // ----------------------------------------------------------------------------------------------------------------

    struct FMeshTemplate
    {
        UStaticMesh* Mesh = nullptr;
        FTransform RelativeTransform = FTransform::Identity;
    };

    auto
    ResolveEffectiveTemplate(
        const USCS_Node* InNode,
        const UBlueprintGeneratedClass* InLeafClass) -> const UActorComponent*
    {
        for (const auto* Walk = InLeafClass;
             Walk != nullptr;
             Walk = Cast<UBlueprintGeneratedClass>(Walk->GetSuperClass()))
        {
            constexpr auto CreateIfNecessary = false;
            if (auto* Handler = const_cast<UBlueprintGeneratedClass*>(Walk)->GetInheritableComponentHandler(CreateIfNecessary))
            {
                if (const auto* Override = Handler->GetOverridenComponentTemplate(
                        FComponentKey{const_cast<USCS_Node*>(InNode)}))
                { return Override; }
            }
        }

        return InNode->ComponentTemplate;
    }

    auto
    VisitScsNode(
        const USCS_Node* InNode,
        const UBlueprintGeneratedClass* InLeafClass,
        const FTransform& InParentTransform,
        TSet<const UActorComponent*>& InOutSeenTemplates,
        TArray<FMeshTemplate>& OutTemplates) -> void
    {
        if (InNode == nullptr)
        { return; }

        const auto* Template = ResolveEffectiveTemplate(InNode, InLeafClass);

        auto NodeTransform = InParentTransform;
        if (const auto* SceneTemplate = Cast<USceneComponent>(Template))
        { NodeTransform = SceneTemplate->GetRelativeTransform() * InParentTransform; }

        auto AlreadySeen = false;
        if (Template != nullptr)
        { InOutSeenTemplates.Add(Template, &AlreadySeen); }

        if (NOT AlreadySeen)
        {
            if (const auto* MeshTemplate = Cast<UStaticMeshComponent>(Template);
                MeshTemplate != nullptr && MeshTemplate->GetStaticMesh() != nullptr)
            { OutTemplates.Add(FMeshTemplate{MeshTemplate->GetStaticMesh(), NodeTransform}); }
        }

        for (const auto* Child : InNode->GetChildNodes())
        { VisitScsNode(Child, InLeafClass, NodeTransform, InOutSeenTemplates, OutTemplates); }
    }

    /** Static-mesh component templates of an actor class, read PASSIVELY: native CDO components with their attach
     *  chains, then every Blueprint generation's SimpleConstructionScript up the super chain, closest
     *  inherited-component override winning (the CkBlueprintExporter walk, filtered to meshes). Approximation by
     *  design — a UserConstructionScript that adds or moves components at runtime is invisible here. */
    auto
    Collect_StaticMeshTemplates(
        const UClass* InActorClass) -> TArray<FMeshTemplate>
    {
        auto Templates = TArray<FMeshTemplate>{};
        auto SeenTemplates = TSet<const UActorComponent*>{};

        const auto* ActorCdo = Cast<AActor>(InActorClass->GetDefaultObject());
        if (ActorCdo == nullptr)
        { return Templates; }

        auto NativeMeshes = TArray<const UStaticMeshComponent*>{};
        ActorCdo->ForEachComponent<UStaticMeshComponent>(/*bIncludeFromChildActors=*/false,
            [&NativeMeshes](const UStaticMeshComponent* InComponent) -> void
            {
                NativeMeshes.Add(InComponent);
            });

        const auto* RootComponent = ActorCdo->GetRootComponent();
        for (const auto* MeshComponent : NativeMeshes)
        {
            if (MeshComponent->GetStaticMesh() == nullptr)
            { continue; }

            auto Relative = FTransform::Identity;
            for (const auto* Walk = static_cast<const USceneComponent*>(MeshComponent);
                 Walk != nullptr && Walk != RootComponent;
                 Walk = Walk->GetAttachParent())
            { Relative = Relative * Walk->GetRelativeTransform(); }

            SeenTemplates.Add(MeshComponent);
            Templates.Add(FMeshTemplate{MeshComponent->GetStaticMesh(), Relative});
        }

        const auto* LeafClass = Cast<UBlueprintGeneratedClass>(InActorClass);
        for (const auto* Walk = LeafClass;
             Walk != nullptr;
             Walk = Cast<UBlueprintGeneratedClass>(Walk->GetSuperClass()))
        {
            const USimpleConstructionScript* Scs = Walk->SimpleConstructionScript;
            if (Scs == nullptr)
            { continue; }

            for (const auto* RootNode : Scs->GetRootNodes())
            { VisitScsNode(RootNode, LeafClass, FTransform::Identity, SeenTemplates, Templates); }
        }

        return Templates;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
    Resolve_Class(
        const FString& InClassPath,
        UClass* InRequiredBase) -> UClass*
    {
        auto* Found = FindObject<UClass>(nullptr, *InClassPath);
        if (Found == nullptr)
        {
            // Visualize is an explicit user action, same standing as the level-load prompt and the decode UX's
            // Try Load Type — resolving here is the whole point of clicking it.
            Found = LoadClass<UObject>(nullptr, *InClassPath);
        }

        if (Found == nullptr || NOT Found->IsChildOf(InRequiredBase))
        { return nullptr; }

        return Found;
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::save_debugger_viz_retained
{
    auto
        Rebuild(
            UWorld* InEditorWorld,
            const FCk_SnapshotInspection_Document& InDocument,
            const TArray<FCkSaveDebugger_VisualizationRow>& InRows)
        -> TOptional<FRebuildStats>
    {
        using namespace ck_save_debugger_visualizer_retained;

        if (NOT Get_IsMutationSafe(InEditorWorld))
        { return {}; }

        Clear();

        auto Stats = FRebuildStats{};

        GRootEntity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity_TransientOwner(InEditorWorld);
        if (ck::Is_NOT_Valid(GRootEntity))
        { return Stats; }

        const auto& Entities = InDocument.Get_Entities();

        for (const auto& Row : InRows)
        {
            switch (Row.VisualKind)
            {
                case ECkSaveDebugger_VisualKind::MeshGhost:
                {
                    const auto* ActorClass = Resolve_Class(Row.ActorClassPath, AActor::StaticClass());
                    if (ActorClass == nullptr)
                    {
                        ++Stats.UnresolvedClassCount;
                        if (Stats.UnresolvedClassSamples.Num() < 3)
                        { Stats.UnresolvedClassSamples.AddUnique(Row.ActorClassPath); }
                        break;
                    }

                    const auto Templates = Collect_StaticMeshTemplates(ActorClass);
                    if (Templates.IsEmpty())
                    {
                        ++Stats.GhostsWithoutMeshCount;
                        break;
                    }

                    for (const auto& Template : Templates)
                    {
                        auto* RendererData = UCk_Utils_IsmRenderer_TransientFactory_UE::GetOrCreate_ForMesh(
                            InEditorWorld, Template.Mesh, ECk_Mobility::Movable);
                        if (RendererData == nullptr)
                        { continue; }

                        UCk_Utils_IsmProxy_UE::Create(
                            GRootEntity,
                            Template.RelativeTransform * Row.WorldTransform,
                            FCk_Fragment_IsmProxy_ParamsData{RendererData});
                        ++Stats.GhostMeshCount;
                    }
                    break;
                }

                case ECkSaveDebugger_VisualKind::ConstructionPreview:
                {
                    auto* ScriptClass = Resolve_Class(Row.ScriptClassPath, UCk_EntityScript_UE::StaticClass());
                    if (ScriptClass == nullptr)
                    {
                        ++Stats.UnresolvedClassCount;
                        if (Stats.UnresolvedClassSamples.Num() < 3)
                        { Stats.UnresolvedClassSamples.AddUnique(Row.ScriptClassPath); }
                        break;
                    }

                    auto SpawnParams = FInstancedStruct{};
                    if (Entities.IsValidIndex(Row.EntityIndex))
                    {
                        if (NOT TryDecode_SpawnParams(
                                Entities[Row.EntityIndex].Get_Entry().Get_SpawnParamsBytes(), SpawnParams))
                        { ++Stats.ParamsDecodeFailureCount; }
                    }

                    // A transient per-row archetype instance, so the saved transform can be injected without ever
                    // touching the class CDO. Requests are not GC-traced, so the instance is pinned until Clear.
                    auto* Archetype = NewObject<UCk_EntityScript_UE>(
                        GetTransientPackage(), ScriptClass, NAME_None, RF_Transient);
                    GArchetypeKeepAlive.Emplace(Archetype);

                    TryInject_SpawnTransform(Archetype, Row.WorldTransform);

                    const auto Pending = UCk_Utils_EntityScript_UE::Request_SpawnEntity_Archetype(
                        GRootEntity, Archetype, SpawnParams, {});

                    // The loader's own semantic: the saved world transform corrects post-spawn drift regardless of
                    // what construction seeded. Applied once the preview's Transform fragment exists.
                    if (auto UnderConstruction = Pending.Get_EntityUnderConstruction();
                        ck::IsValid(UnderConstruction))
                    { GPendingTransforms.Add(FPendingTransform{UnderConstruction, Row.WorldTransform}); }

                    ++Stats.PreviewCount;
                    break;
                }

                default:
                    break;
            }
        }

        if (GPendingTransforms.Num() > 0)
        { HookEndFrame(); }

        UCk_Utils_EditorOnly_UE::Request_RedrawLevelEditingViewports();
        return Stats;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Clear()
        -> void
    {
        using namespace ck_save_debugger_visualizer_retained;

        GPendingTransforms.Reset();
        GArchetypeKeepAlive.Reset();
        GGizmoSet.Reset();

        if (ck::IsValid(GRootEntity))
        {
            const auto* World = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(GRootEntity);
            if (Get_IsMutationSafe(World))
            { UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(GRootEntity); }
            else
            {
                GPendingDestroyRoots.Add(GRootEntity);
                HookEndFrame();
            }
        }

        GRootEntity = FCk_Handle{};
        UnhookEndFrameIfIdle();
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Update_SelectionGizmo(
            UWorld* InEditorWorld,
            const TOptional<FTransform>& InTransform)
        -> void
    {
        using namespace ck_save_debugger_visualizer_retained;

        // One gizmo, one slot: the key is only a map key to the set, never dereferenced.
        const auto GizmoKey = FCk_Handle{};

        if (NOT InTransform.IsSet() || ck::Is_NOT_Valid(InEditorWorld))
        {
            GGizmoSet.Remove(GizmoKey);
            return;
        }

        GGizmoSet.UpdateGizmo(InEditorWorld, GizmoKey, *InTransform);
    }
}

#endif // WITH_EDITOR
