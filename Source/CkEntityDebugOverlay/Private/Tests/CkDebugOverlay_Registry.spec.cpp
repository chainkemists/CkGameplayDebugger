#include "Misc/AutomationTest.h"
#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugOverlay_Registry_Test,
    "Ck.DebugOverlay.Registry.RegisterAndCreate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// --------------------------------------------------------------------------------------------------------------------

namespace
{
class FFakeProvider : public ICk_DebugOverlay_Provider
{
    auto Get_ProviderTag()  const -> FGameplayTag override
    {
        return FGameplayTag::RequestGameplayTag(TEXT("Ck.OnScreenDebugger.Provider"));
    }
    auto Get_FieldTags()    const -> TArray<FCk_DebugOverlay_FieldDesc> override { return {}; }
    auto Get_SortPriority() const -> int32 override { return 5; }
    auto CanProvide(const FCk_Handle&) const -> bool override { return true; }
    auto Collect(const FCk_Handle&, const FCk_DebugOverlay_ProviderConfig&, FCk_DebugOverlay_Section&) -> void override {}
};
} // namespace

// --------------------------------------------------------------------------------------------------------------------

bool FCkDebugOverlay_Registry_Test::RunTest(const FString&)
{
    auto& Reg = FCk_DebugOverlay_Registry::Get();

    Reg.Register(TEXT("Fake"), [] { return TSharedPtr<ICk_DebugOverlay_Provider>(MakeShared<FFakeProvider>()); });
    const auto All = Reg.CreateAll();
    TestTrue(TEXT("contains fake"), All.ContainsByPredicate([](auto& P) { return P && P->Get_SortPriority() == 5; }));
    Reg.Unregister(TEXT("Fake"));

    return true;
}

// --------------------------------------------------------------------------------------------------------------------
