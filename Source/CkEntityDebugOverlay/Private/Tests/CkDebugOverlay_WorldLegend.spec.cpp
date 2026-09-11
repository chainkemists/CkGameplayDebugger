#include "Misc/AutomationTest.h"
#include "NativeGameplayTags.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEntityDebugOverlay/Layout/CkDebugOverlay_Layout.h"
#include "CkEntityDebugOverlay/Presentation/CkDebugOverlay_Present.h"
#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Provider.h"

// --------------------------------------------------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG_STATIC(WorldLegend_Provider_Label, "Ck.OnScreenDebugger.Provider.Label")
UE_DEFINE_GAMEPLAY_TAG_STATIC(WorldLegend_Provider_Timer, "Ck.OnScreenDebugger.Provider.Timer")
UE_DEFINE_GAMEPLAY_TAG_STATIC(WorldLegend_Field_Label, "Ck.OnScreenDebugger.Provider.Label.Name")
UE_DEFINE_GAMEPLAY_TAG_STATIC(WorldLegend_Field_Timer, "Ck.OnScreenDebugger.Provider.Timer.Remaining")
UE_DEFINE_GAMEPLAY_TAG_STATIC(WorldLegend_FilterMarker, "Ck.DebugOverlay.Test.WorldLegend.FilterMarker")

// --------------------------------------------------------------------------------------------------------------------

namespace
{
class FWorldLegendTestProvider final : public ICk_DebugOverlay_Provider
{
public:
    FWorldLegendTestProvider(
        const FGameplayTag& InProviderTag,
        const FGameplayTag& InFieldTag,
        const bool          bInSuppressWhenFiltered = false)
        : _ProviderTag{ InProviderTag }
        , _FieldTag{ InFieldTag }
        , _bSuppressWhenFiltered{ bInSuppressWhenFiltered }
    {}

    auto Get_ProviderTag() const -> FGameplayTag override { return _ProviderTag; }

    auto Get_FieldTags() const -> TArray<FCk_DebugOverlay_FieldDesc> override
    {
        return { FCk_DebugOverlay_FieldDesc{ _FieldTag, true } };
    }

    auto Get_SortPriority() const -> int32 override { return 0; }
    auto Get_SupportsEntryFilter() const -> bool override { return true; }
    auto CanProvide(const FCk_Handle&) const -> bool override { return true; }

    auto Collect(
        const FCk_Handle&,
        const FCk_DebugOverlay_ProviderConfig& InConfig,
        FCk_DebugOverlay_Section&              OutSection) -> void override
    {
        ++CollectCalls;
        bSawEntryFilter = bSawEntryFilter || !InConfig.EntryFilter.IsEmpty();

        if (NOT InConfig.EnabledFields.HasTagExact(_FieldTag) ||
            (_bSuppressWhenFiltered && !InConfig.EntryFilter.IsEmpty()))
        { return; }

        OutSection.ProviderTag = _ProviderTag;

        auto Row     = FCk_DebugOverlay_Row{};
        Row.FieldTag = _FieldTag;
        Row.Value    = FText::FromString(TEXT("Behavioral value that must not render in a far legend"));
        OutSection.Rows.Add(MoveTemp(Row));
    }

    auto Get_CompactToken(const FCk_Handle&, const FCk_DebugOverlay_ProviderConfig&) const -> FString override
    {
        ++CompactTokenCalls;
        return TEXT("Lbl:Root");
    }

    mutable int32 CompactTokenCalls = 0;
    int32         CollectCalls      = 0;
    bool          bSawEntryFilter   = false;

private:
    FGameplayTag _ProviderTag;
    FGameplayTag _FieldTag;
    bool         _bSuppressWhenFiltered = false;
};

auto WorldLegend_HasBadge(const TArray<FCk_DebugOverlay_WorldTagBadge>& InBadges, const TCHAR* InText) -> bool
{
    return InBadges.ContainsByPredicate([InText](const auto& Badge) { return Badge.Text == InText; });
}
} // namespace

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugOverlay_WorldLegend_Test,
    "Ck.DebugOverlay.Presentation.WorldLegend",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugOverlay_WorldLegend_Test::RunTest(const FString&)
{
    // A label-only candidate at far distance must use the feature legend, never Label's
    // behavioral compact token. Build_WorldTags consumes this same helper for its far plate.
    {
        auto LabelProvider = MakeShared<FWorldLegendTestProvider>(
            WorldLegend_Provider_Label, WorldLegend_Field_Label);
        auto Providers = TArray<TSharedPtr<ICk_DebugOverlay_Provider>>{ LabelProvider };
        auto Layout = FCk_DebugOverlay_Layout{};
        Layout.EnabledProviders.AddTag(WorldLegend_Provider_Label);

        const auto Badges = ck_debugoverlay::Build_WorldTagBadges(FCk_Handle{}, Providers, Layout);
        TestEqual(TEXT("label-only far legend emits one badge"), Badges.Num(), 1);
        TestTrue(TEXT("label-only far legend uses LABL"), WorldLegend_HasBadge(Badges, TEXT("LABL")));
        TestFalse(TEXT("label-only far legend never uses Lbl:Root"), WorldLegend_HasBadge(Badges, TEXT("Lbl:Root")));
        TestEqual(TEXT("far legend does not request compact behavioral tokens"), LabelProvider->CompactTokenCalls, 0);
    }

    // Provider registration is defensive: duplicate provider instances cannot duplicate a badge.
    {
        auto First = MakeShared<FWorldLegendTestProvider>(WorldLegend_Provider_Timer, WorldLegend_Field_Timer);
        auto Duplicate = MakeShared<FWorldLegendTestProvider>(WorldLegend_Provider_Timer, WorldLegend_Field_Timer);
        auto Providers = TArray<TSharedPtr<ICk_DebugOverlay_Provider>>{ First, Duplicate };
        auto Layout = FCk_DebugOverlay_Layout{};
        Layout.EnabledProviders.AddTag(WorldLegend_Provider_Timer);

        const auto Badges = ck_debugoverlay::Build_WorldTagBadges(FCk_Handle{}, Providers, Layout);
        TestEqual(TEXT("duplicate providers emit one legend badge"), Badges.Num(), 1);
        TestTrue(TEXT("duplicate provider badge uses TIMER legend"), WorldLegend_HasBadge(Badges, TEXT("TIMR")));
        TestEqual(TEXT("first duplicate collects once"), First->CollectCalls, 1);
        TestEqual(TEXT("second duplicate is not collected"), Duplicate->CollectCalls, 0);
    }

    // The world legend follows both layout field enablement and the provider's filter-aware
    // collection decision; it does not advertise data which the detailed card hides.
    {
        auto TimerProvider = MakeShared<FWorldLegendTestProvider>(
            WorldLegend_Provider_Timer, WorldLegend_Field_Timer, /*bInSuppressWhenFiltered=*/true);
        auto Providers = TArray<TSharedPtr<ICk_DebugOverlay_Provider>>{ TimerProvider };

        const auto DisabledLayout = FCk_DebugOverlay_Layout{};
        const auto DisabledBadges = ck_debugoverlay::Build_WorldTagBadges(FCk_Handle{}, Providers, DisabledLayout);
        TestTrue(TEXT("provider omitted by layout has no world legend badge"), DisabledBadges.IsEmpty());
        TestEqual(TEXT("disabled provider is not collected"), TimerProvider->CollectCalls, 0);

        auto FilteredLayout = FCk_DebugOverlay_Layout{};
        auto Entry = FCk_DebugOverlay_ProviderEntry{};
        Entry.ProviderTag = WorldLegend_Provider_Timer;
        Entry.EnabledFields.AddTag(WorldLegend_Field_Timer);
        auto FilterExpression = FGameplayTagQueryExpression{};
        FilterExpression.AllTagsMatch().AddTag(WorldLegend_FilterMarker);
        Entry.EntryFilter.Build(FilterExpression);
        FilteredLayout.Entries.Add(MoveTemp(Entry));

        const auto FilteredBadges = ck_debugoverlay::Build_WorldTagBadges(FCk_Handle{}, Providers, FilteredLayout);
        TestTrue(TEXT("provider-filtered row has no world legend badge"), FilteredBadges.IsEmpty());
        TestTrue(TEXT("entry filter reaches provider collection"), TimerProvider->bSawEntryFilter);
    }

    // An empty provider registry remains a valid, empty legend model.
    {
        const auto Badges = ck_debugoverlay::Build_WorldTagBadges(
            FCk_Handle{}, TArray<TSharedPtr<ICk_DebugOverlay_Provider>>{}, FCk_DebugOverlay_Layout{});
        TestTrue(TEXT("zero providers is safe and empty"), Badges.IsEmpty());
    }

    return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
