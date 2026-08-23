#include "CkTextureDebugger/Data/CkTextureDebugger_TextureHealth.h"
#include "CkTextureDebugger/Model/CkTextureDebugger_CheckerOverrideSession.h"

#include "Engine/Texture2D.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTextureDebugger_Core_InvalidAdmission,
    "Ck.TextureDebugger.Core.InvalidAdmission",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTextureDebugger_Core_InvalidAdmission::RunTest(const FString& Parameters)
{
    auto Session = FCkTextureDebugger_CheckerOverrideSession{};

    const auto NoWorld = Session.Apply(nullptr, nullptr, {});
    TestEqual(TEXT("No world is rejected before any session is published"),
        static_cast<int32>(NoWorld.Result),
        static_cast<int32>(ECkTextureDebugger_CheckerSessionResult::InvalidWorld));
    TestFalse(TEXT("Rejected admission leaves no active session"), Session.HasActiveSession());

    const auto NoRestore = Session.TryRestore();
    TestEqual(TEXT("Restore with no session is explicit"),
        static_cast<int32>(NoRestore.Result),
        static_cast<int32>(ECkTextureDebugger_CheckerSessionResult::NoActiveSession));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkTextureDebugger_Core_HealthUnavailableIsNotZeroFact,
    "Ck.TextureDebugger.Core.HealthUnavailableIsNotZeroFact",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkTextureDebugger_Core_HealthUnavailableIsNotZeroFact::RunTest(const FString& Parameters)
{
    auto Texture = TStrongObjectPtr<UTexture2D>{NewObject<UTexture2D>()};
    const auto Health = ck::texture_debugger::health::Describe(
        Texture.Get(), ECkTextureDebugger_StreamingAvailability::ManagerUnavailable);

    TestEqual(TEXT("Unavailable manager remains the provenance"),
        static_cast<int32>(Health.StreamingAvailability),
        static_cast<int32>(ECkTextureDebugger_StreamingAvailability::ManagerUnavailable));
    TestFalse(TEXT("Unavailable manager does not manufacture mip metrics"), Health.HasStreamingMetrics);
    TestEqual(TEXT("Unavailable manager does not convert unknown resident mips into a measurement"),
        Health.ResidentMipCount, 0);
    TestEqual(TEXT("Unavailable manager does not convert unknown requested mips into a measurement"),
        Health.RequestedMipCount, 0);

    const auto NullHealth = ck::texture_debugger::health::Describe(
        nullptr, ECkTextureDebugger_StreamingAvailability::StreamingDisabled);
    TestEqual(TEXT("Null texture keeps its supplied unavailable provenance"),
        static_cast<int32>(NullHealth.StreamingAvailability),
        static_cast<int32>(ECkTextureDebugger_StreamingAvailability::StreamingDisabled));
    TestFalse(TEXT("Null texture has no streaming metrics"), NullHealth.HasStreamingMetrics);

    return true;
}

#endif
