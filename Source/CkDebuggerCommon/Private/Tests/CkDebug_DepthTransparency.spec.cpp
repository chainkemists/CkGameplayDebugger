#include "CkDebuggerCommon/Classification/CkDebug_DepthTransparency.h"
#include "CkDebuggerCommon/Settings/CkDebuggerSettings.h"

#include "Misc/AutomationTest.h"

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebugDepthTransparency_SettingsGateAndInvalidHandles,
    "Ck.DebuggerCommon.DepthTransparency.SettingsGateAndInvalidHandles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// --------------------------------------------------------------------------------------------------------------------

namespace ck_debug_depth_transparency_spec
{
    // RAII toggle of the per-user setting so the test never leaves it flipped.
    struct FScopedTransparency
    {
        explicit FScopedTransparency(bool InEnabled)
        {
            auto* Settings = GetMutableDefault<UCkDebuggerSettings>();
            _Previous = Settings->bActorRelayDepthTransparency;
            Settings->bActorRelayDepthTransparency = InEnabled;
        }

        ~FScopedTransparency()
        {
            GetMutableDefault<UCkDebuggerSettings>()->bActorRelayDepthTransparency = _Previous;
        }

    private:
        bool _Previous = true;
    };
} // namespace ck_debug_depth_transparency_spec

// --------------------------------------------------------------------------------------------------------------------

bool FCkDebugDepthTransparency_SettingsGateAndInvalidHandles::RunTest(const FString&)
{
    using namespace ck_debug_depth_transparency_spec;

    const auto Invalid  = FCk_Handle{};
    const auto Original = UCkDebuggerSettings::Get()->bActorRelayDepthTransparency;

    TestFalse(TEXT("invalid handle is not a relay entity"), ck::DebugDepthTransparency::Get_IsRelayEntity(Invalid));

    {
        auto Scoped = FScopedTransparency{false};
        TestFalse(
            TEXT("transparency off — no owner is transparent"),
            ck::DebugDepthTransparency::Get_IsTransparentOwner(Invalid));
    }

    {
        auto Scoped = FScopedTransparency{true};
        TestFalse(
            TEXT("transparency on — invalid owner is still not transparent"),
            ck::DebugDepthTransparency::Get_IsTransparentOwner(Invalid));
    }

    TestTrue(
        TEXT("setting restored after scopes"),
        UCkDebuggerSettings::Get()->bActorRelayDepthTransparency == Original);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------
