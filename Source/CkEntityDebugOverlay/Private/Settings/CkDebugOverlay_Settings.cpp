#include "CkEntityDebugOverlay/Settings/CkDebugOverlay_Settings.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"

#include <initializer_list>

// --------------------------------------------------------------------------------------------------------------------
// Provider tags are defined natively by the individual provider source files (Task 11+).  Their
// UE_DEFINE_GAMEPLAY_TAG static initialisers run at DLL-load time, before any CDO constructor
// executes.  Therefore: request each provider tag by string with ErrorIfNotFound=false so that
// building this module in isolation (before the providers exist) silently leaves EnabledProviders
// empty rather than hard-erroring.  Once the provider modules are compiled and linked, the tags are
// registered at static-init and the CDO constructor picks them up correctly.
// --------------------------------------------------------------------------------------------------------------------

namespace
{
    // Helper: request a provider tag by string, returning an invalid tag if not yet registered.
    FORCEINLINE FGameplayTag Provider(const TCHAR* InTagName)
    {
        return FGameplayTag::RequestGameplayTag(FName(InTagName), /*ErrorIfNotFound=*/false);
    }

    // Build a layout with the given LayoutTag, Density, and an optional list of provider tag strings.
    FCk_DebugOverlay_Layout MakeLayout(
        const FGameplayTag&                InLayoutTag,
        ECk_DebugOverlay_Density           InDensity,
        std::initializer_list<const TCHAR*> InProviders)
    {
        FCk_DebugOverlay_Layout Layout;
        Layout.LayoutTag                 = InLayoutTag;
        Layout.DefaultStyle.Density      = InDensity;

        for (const TCHAR* ProviderName : InProviders)
        {
            const FGameplayTag Tag = Provider(ProviderName);
            if (Tag.IsValid())
            {
                Layout.EnabledProviders.AddTag(Tag);
            }
        }

        return Layout;
    }
}

// --------------------------------------------------------------------------------------------------------------------

UCk_DebugOverlay_Settings::UCk_DebugOverlay_Settings()
{
    // --------------------------------------------------
    // Pre-tuned default layouts (spec §15).
    // Provider tags are requested by string so this constructor compiles and runs correctly
    // before provider modules are present; see file-level comment for the full rationale.
    // --------------------------------------------------

    // "All" — every ported provider enabled with EVERY field on (bEnableAllFields
    // overrides per-field DefaultEnabled). Gyms exercise one feature at a time, so any
    // narrower layout shows mostly-empty cards there; this is the default starting view.
    // NOTE: as more inspectors are ported, add their provider tags here.
    {
        auto AllLayout = MakeLayout(
            TAG_Ck_OnScreenDebugger_Layout_All,
            ECk_DebugOverlay_Density::Ultra,
            {
                TEXT("Ck.OnScreenDebugger.Provider.EntityInfo"),
                TEXT("Ck.OnScreenDebugger.Provider.Transform"),
                TEXT("Ck.OnScreenDebugger.Provider.StateMachine"),
                TEXT("Ck.OnScreenDebugger.Provider.Goap"),
                TEXT("Ck.OnScreenDebugger.Provider.Physics"),
                TEXT("Ck.OnScreenDebugger.Provider.Jolt"),
                TEXT("Ck.OnScreenDebugger.Provider.FloatAttributes"),
                TEXT("Ck.OnScreenDebugger.Provider.IntegerAttributes"),
                TEXT("Ck.OnScreenDebugger.Provider.ByteAttributes"),
                TEXT("Ck.OnScreenDebugger.Provider.VectorAttributes"),
                TEXT("Ck.OnScreenDebugger.Provider.RotatorAttributes"),
                TEXT("Ck.OnScreenDebugger.Provider.Crowd"),
                TEXT("Ck.OnScreenDebugger.Provider.PathNetworkFollower"),
                TEXT("Ck.OnScreenDebugger.Provider.Inventory"),
                TEXT("Ck.OnScreenDebugger.Provider.InteractTarget"),
                TEXT("Ck.OnScreenDebugger.Provider.Aggro"),
                TEXT("Ck.OnScreenDebugger.Provider.Team"),
                TEXT("Ck.OnScreenDebugger.Provider.Objective"),
                TEXT("Ck.OnScreenDebugger.Provider.TagSet"),
                TEXT("Ck.OnScreenDebugger.Provider.EntityCollection"),
                TEXT("Ck.OnScreenDebugger.Provider.AnimPlans"),
                TEXT("Ck.OnScreenDebugger.Provider.Timer"),
                TEXT("Ck.OnScreenDebugger.Provider.Label"),
                TEXT("Ck.OnScreenDebugger.Provider.Variables"),
            });
        AllLayout.bEnableAllFields = true;
        Layouts.Add(MoveTemp(AllLayout));
    }

    Layouts.Add(MakeLayout(
        TAG_Ck_OnScreenDebugger_Layout_AI,
        ECk_DebugOverlay_Density::Ultra,
        {
            // EntityInfo removed — entity identity is now shown via SCkDebug_EntityRef in the card header.
            TEXT("Ck.OnScreenDebugger.Provider.StateMachine"),
            TEXT("Ck.OnScreenDebugger.Provider.Goap"),
            TEXT("Ck.OnScreenDebugger.Provider.Aggro"),
            TEXT("Ck.OnScreenDebugger.Provider.AStar"),
            TEXT("Ck.OnScreenDebugger.Provider.Crowd"),
            TEXT("Ck.OnScreenDebugger.Provider.PathNetworkFollower"),
            TEXT("Ck.OnScreenDebugger.Provider.Objective"),
            TEXT("Ck.OnScreenDebugger.Provider.InteractTarget"),
            // Attribute families stay available in All.  The AI default is intentionally
            // signal-first so an attribute flood cannot displace GOAP/navigation diagnostics.
            TEXT("Ck.OnScreenDebugger.Provider.Transform"),
        }));

    Layouts.Add(MakeLayout(
        TAG_Ck_OnScreenDebugger_Layout_Animation,
        ECk_DebugOverlay_Density::Compact,
        {
            // EntityInfo removed — entity identity is now shown via SCkDebug_EntityRef in the card header.
            TEXT("Ck.OnScreenDebugger.Provider.AnimPlans"),
            TEXT("Ck.OnScreenDebugger.Provider.MontagePlayer"),
            TEXT("Ck.OnScreenDebugger.Provider.StateMachine"),
            TEXT("Ck.OnScreenDebugger.Provider.Transform"),
        }));

    Layouts.Add(MakeLayout(
        TAG_Ck_OnScreenDebugger_Layout_Movement,
        ECk_DebugOverlay_Density::Ultra,
        {
            // EntityInfo removed — entity identity is now shown via SCkDebug_EntityRef in the card header.
            TEXT("Ck.OnScreenDebugger.Provider.Physics"),
            TEXT("Ck.OnScreenDebugger.Provider.Jolt"),
            TEXT("Ck.OnScreenDebugger.Provider.OverlapBody"),
            TEXT("Ck.OnScreenDebugger.Provider.Shapes"),
            TEXT("Ck.OnScreenDebugger.Provider.Transform"),
            TEXT("Ck.OnScreenDebugger.Provider.SceneNode"),
        }));

    Layouts.Add(MakeLayout(
        TAG_Ck_OnScreenDebugger_Layout_Combat,
        ECk_DebugOverlay_Density::Ultra,
        {
            // EntityInfo removed — entity identity is now shown via SCkDebug_EntityRef in the card header.
            TEXT("Ck.OnScreenDebugger.Provider.Aggro"),
            TEXT("Ck.OnScreenDebugger.Provider.FloatAttributes"),
            TEXT("Ck.OnScreenDebugger.Provider.IntegerAttributes"),
            TEXT("Ck.OnScreenDebugger.Provider.InteractTarget"),
            TEXT("Ck.OnScreenDebugger.Provider.StateMachine"),
        }));

    Layouts.Add(MakeLayout(
        TAG_Ck_OnScreenDebugger_Layout_Overview,
        ECk_DebugOverlay_Density::Ultra,
        {
            // EntityInfo removed — entity identity is now shown via SCkDebug_EntityRef in the card header.
            TEXT("Ck.OnScreenDebugger.Provider.StateMachine"),
            TEXT("Ck.OnScreenDebugger.Provider.Goap"),
            TEXT("Ck.OnScreenDebugger.Provider.FloatAttributes"),
        }));

    // "All" layout is the default starting view — gyms are too sparse for the narrower ones.
    StartingLayout = TAG_Ck_OnScreenDebugger_Layout_All;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_DebugOverlay_Settings::
    Get_PassesAttributeFilter(
        const FString& InAttributeName)
    -> bool
{
    const auto* Settings = GetDefault<UCk_DebugOverlay_Settings>();
    if (Settings == nullptr || Settings->AttributeFilterPatterns.IsEmpty())
    { return true; }   // empty list = no filtering in either mode

    auto MatchesAny = false;
    for (const auto& Pattern : Settings->AttributeFilterPatterns)
    {
        if (Pattern.IsEmpty())
        { continue; }

        if (InAttributeName.Contains(Pattern))   // ESearchCase::IgnoreCase default
        {
            MatchesAny = true;
            break;
        }
    }

    return Settings->bAttributeFilterIsExclusion ? !MatchesAny : MatchesAny;
}

// --------------------------------------------------------------------------------------------------------------------
