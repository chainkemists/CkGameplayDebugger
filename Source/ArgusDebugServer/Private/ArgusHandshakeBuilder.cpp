#include "ArgusHandshakeBuilder.h"

#include "CkEcs/Reflection/CkFragmentReflectionRegistry.h"
#include "CkEcs/Registry/CkRegistry.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"

#include "Engine/Engine.h"
#include "Misc/App.h"
#include "HAL/PlatformProcess.h"
#include "UObject/UnrealType.h"

#include "entt/config/version.h"

DEFINE_LOG_CATEGORY_STATIC(LogArgusHandshake, Log, All);

// --------------------------------------------------------------------------------------------------------------------

namespace Argus
{

auto FHandshakeBuilder::Build(const FHandshakeRequest& InRequest) -> FHandshakeResponse
{
    check(IsInGameThread());

    const auto ServerEnttVersion = static_cast<uint32>(
        (ENTT_VERSION_MAJOR << 16) |
        (ENTT_VERSION_MINOR << 8) |
        ENTT_VERSION_PATCH);

    const auto VersionMatch = (InRequest.ExpectedEnttVersion == ServerEnttVersion);

    FHandshakeResponse Response;
    Response.Accepted      = VersionMatch;
    Response.ServerVersion = SERVER_VERSION;
    Response.ProjectName   = FApp::GetProjectName();
    Response.EngineVersion = FEngineVersion::Current().ToString();
    Response.ProcessId     = FPlatformProcess::GetCurrentProcessId();
    Response.EnttVersion   = ServerEnttVersion;
    Response.Registries    = EnumerateRegistries();
    Response.ComponentTypes = BuildComponentTypes();

    UE_LOG(LogArgusHandshake, Log,
        TEXT("HandshakeResponse: accepted=%s, project=%s, pid=%u, entt=0x%06X, registries=%d, types=%d"),
        Response.Accepted ? TEXT("true") : TEXT("false"),
        *Response.ProjectName,
        Response.ProcessId,
        Response.EnttVersion,
        Response.Registries.Num(),
        Response.ComponentTypes.Num());

    if (NOT VersionMatch)
    {
        UE_LOG(LogArgusHandshake, Warning,
            TEXT("ENTT version mismatch: client expects 0x%06X, server has 0x%06X"),
            InRequest.ExpectedEnttVersion, ServerEnttVersion);
    }

    return Response;
}

// --------------------------------------------------------------------------------------------------------------------

auto FHandshakeBuilder::EnumerateRegistries() -> TArray<FRegistryInfo>
{
    check(IsInGameThread());

    TArray<FRegistryInfo> Registries;

    if (GEngine == nullptr)
    {
        return Registries;
    }

    uint32 WorldIndex = 0;
    const auto& WorldContexts = GEngine->GetWorldContexts();

    for (int32 i = 0; i < WorldContexts.Num(); ++i)
    {
        const auto& Context = WorldContexts[i];
        auto* World = Context.World();

        if (NOT IsValid(World))
        {
            continue;
        }

        // Only expose playable worlds — Game and PIE
        const auto WorldType = World->WorldType;
        const auto IsPlayableWorld = (WorldType == EWorldType::Game || WorldType == EWorldType::PIE);
        if (NOT IsPlayableWorld)
        {
            continue;
        }

        auto* Subsystem = World->GetSubsystem<UCk_EcsWorld_Subsystem_UE>();
        if (Subsystem == nullptr)
        {
            continue;
        }

        const auto& Registry = Subsystem->Get_Registry();

        FRegistryInfo Info;
        Info.WorldId   = WorldIndex++;
        Info.WorldName = World->GetName();

        const auto* RawRegistry = Registry.Get_InternalRegistryRawPtr();
        Info.RegistryAddress = reinterpret_cast<uint64>(RawRegistry);

        // Returns alive and recycled entities
        //Info.EntityCount = static_cast<uint32>(RawRegistry->storage<entt::entity>()->size());

        // Alternative approach to get only alive entities
        Info.EntityCount = static_cast<uint32>(RawRegistry->storage<entt::entity>()->free_list());

        // NM_Standalone=0, NM_DedicatedServer=1, NM_ListenServer=2, NM_Client=3
        Info.NetMode = static_cast<uint8>(World->GetNetMode());

        Registries.Add(MoveTemp(Info));
    }

    return Registries;
}

// --------------------------------------------------------------------------------------------------------------------

auto FHandshakeBuilder::BuildComponentTypes() -> TArray<FComponentTypeInfo>
{
    TArray<FComponentTypeInfo> Types;

    const auto& Entries = ck::FCk_FragmentReflectionRegistry::Get().GetAllEntries();

    for (const auto& [TypeId, FragmentInfo] : Entries)
    {
        if (FragmentInfo.ScriptStruct == nullptr)
        {
            continue;
        }

        FComponentTypeInfo Info;
        Info.TypeName   = FragmentInfo.ScriptStruct->GetName();
        Info.Properties = BuildPropertyList(FragmentInfo.ScriptStruct);

        Types.Add(MoveTemp(Info));
    }

    return Types;
}

// --------------------------------------------------------------------------------------------------------------------

auto FHandshakeBuilder::BuildPropertyList(const UScriptStruct* InStruct) -> TArray<FPropertyInfo>
{
    TArray<FPropertyInfo> Properties;

    if (InStruct == nullptr)
    {
        return Properties;
    }

    for (TFieldIterator<FProperty> It(InStruct); It; ++It)
    {
        const auto* Prop = *It;

        FPropertyInfo Info;
        Info.Name     = Prop->GetName();
        Info.TypeName = Prop->GetCPPType();
        Info.Offset   = static_cast<uint64>(Prop->GetOffset_ForInternal());
        Info.Size     = static_cast<uint64>(Prop->GetSize());

        Properties.Add(MoveTemp(Info));
    }

    return Properties;
}

} // namespace Argus