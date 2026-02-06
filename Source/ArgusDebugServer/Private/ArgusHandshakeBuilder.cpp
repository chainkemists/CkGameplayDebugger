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

    FHandshakeResponse Response;

    // --- ENTT version ---
    const uint32 ServerEnttVersion =
        (ENTT_VERSION_MAJOR << 16) |
        (ENTT_VERSION_MINOR << 8) |
        ENTT_VERSION_PATCH;

    const bool bVersionMatch = (InRequest.ExpectedEnttVersion == ServerEnttVersion);

    // --- Populate response ---
    Response.bAccepted     = bVersionMatch;
    Response.ServerVersion = SERVER_VERSION;
    Response.ProjectName   = FApp::GetProjectName();
    Response.EngineVersion = FEngineVersion::Current().ToString();
    Response.ProcessId     = FPlatformProcess::GetCurrentProcessId();
    Response.EnttVersion   = ServerEnttVersion;

    // Always populate registries and types even if version mismatch,
    // so the client can display diagnostic info
    Response.Registries     = EnumerateRegistries();
    Response.ComponentTypes = BuildComponentTypes();

    UE_LOG(LogArgusHandshake, Log,
        TEXT("HandshakeResponse: accepted=%s, project=%s, engine=%s, pid=%u, entt=0x%06X, registries=%d, types=%d"),
        Response.bAccepted ? TEXT("true") : TEXT("false"),
        *Response.ProjectName,
        *Response.EngineVersion,
        Response.ProcessId,
        Response.EnttVersion,
        Response.Registries.Num(),
        Response.ComponentTypes.Num());

    if (!bVersionMatch)
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
        UWorld* World = Context.World();

        if (!IsValid(World))
        {
            continue;
        }

        // Skip worlds without a game instance (editor preview, etc.)
        if (World->GetGameInstance() == nullptr && World->WorldType != EWorldType::PIE && World->WorldType != EWorldType::Game)
        {
            continue;
        }

        auto* Subsystem = World->GetSubsystem<UCk_EcsWorld_Subsystem_UE>();
        if (Subsystem == nullptr)
        {
            continue;
        }

        const FCk_Registry& Registry = Subsystem->Get_Registry();

        FRegistryInfo Info;
        Info.WorldId   = WorldIndex++;
        Info.WorldName = World->GetName();

        // Get the raw entt::registry* pointer for ReadProcessMemory
        const auto* RawRegistry = Registry.Get_InternalRegistryRawPtr();
        Info.RegistryAddress = reinterpret_cast<uint64>(RawRegistry);

        // Entity count
        if (RawRegistry != nullptr)
        {
            Info.EntityCount = static_cast<uint32>(RawRegistry->alive());
        }
        else
        {
            Info.EntityCount = 0;
        }

        // Net mode: NM_Standalone=0, NM_DedicatedServer=1, NM_ListenServer=2, NM_Client=3
        Info.NetMode = static_cast<uint8>(World->GetNetMode());

        UE_LOG(LogArgusHandshake, Verbose,
            TEXT("  Registry[%u]: '%s' @ 0x%016llX (%u entities, netmode=%u)"),
            Info.WorldId, *Info.WorldName, Info.RegistryAddress, Info.EntityCount, Info.NetMode);

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
        // Skip fragments without UE reflection (not yet converted to USTRUCT)
        if (FragmentInfo.ScriptStruct == nullptr)
        {
            continue;
        }

        FComponentTypeInfo Info;
        Info.TypeName   = FragmentInfo.ScriptStruct->GetName();
        Info.Properties = BuildPropertyList(FragmentInfo.ScriptStruct);

        UE_LOG(LogArgusHandshake, Verbose,
            TEXT("  ComponentType: '%s' (%d properties)"),
            *Info.TypeName, Info.Properties.Num());

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
        const FProperty* Prop = *It;

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
