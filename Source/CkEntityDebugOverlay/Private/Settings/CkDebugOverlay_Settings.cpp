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

    Layouts.Add(MakeLayout(
        TAG_Ck_OnScreenDebugger_Layout_AI,
        ECk_DebugOverlay_Density::Ultra,
        {
            TEXT("Ck.OnScreenDebugger.Provider.StateMachine"),
            TEXT("Ck.OnScreenDebugger.Provider.GOAP"),
            TEXT("Ck.OnScreenDebugger.Provider.Aggro"),
            TEXT("Ck.OnScreenDebugger.Provider.AStar"),
            TEXT("Ck.OnScreenDebugger.Provider.Objective"),
            TEXT("Ck.OnScreenDebugger.Provider.InteractTarget"),
            TEXT("Ck.OnScreenDebugger.Provider.EntityInfo"),
            TEXT("Ck.OnScreenDebugger.Provider.Transform"),
        }));

    Layouts.Add(MakeLayout(
        TAG_Ck_OnScreenDebugger_Layout_Animation,
        ECk_DebugOverlay_Density::Compact,
        {
            TEXT("Ck.OnScreenDebugger.Provider.AnimPlans"),
            TEXT("Ck.OnScreenDebugger.Provider.MontagePlayer"),
            TEXT("Ck.OnScreenDebugger.Provider.StateMachine"),
            TEXT("Ck.OnScreenDebugger.Provider.EntityInfo"),
            TEXT("Ck.OnScreenDebugger.Provider.Transform"),
        }));

    Layouts.Add(MakeLayout(
        TAG_Ck_OnScreenDebugger_Layout_Movement,
        ECk_DebugOverlay_Density::Ultra,
        {
            TEXT("Ck.OnScreenDebugger.Provider.Physics"),
            TEXT("Ck.OnScreenDebugger.Provider.OverlapBody"),
            TEXT("Ck.OnScreenDebugger.Provider.Shapes"),
            TEXT("Ck.OnScreenDebugger.Provider.Transform"),
            TEXT("Ck.OnScreenDebugger.Provider.SceneNode"),
            TEXT("Ck.OnScreenDebugger.Provider.EntityInfo"),
        }));

    Layouts.Add(MakeLayout(
        TAG_Ck_OnScreenDebugger_Layout_Combat,
        ECk_DebugOverlay_Density::Ultra,
        {
            TEXT("Ck.OnScreenDebugger.Provider.Aggro"),
            TEXT("Ck.OnScreenDebugger.Provider.FloatAttributes"),
            TEXT("Ck.OnScreenDebugger.Provider.IntegerAttributes"),
            TEXT("Ck.OnScreenDebugger.Provider.InteractTarget"),
            TEXT("Ck.OnScreenDebugger.Provider.StateMachine"),
            TEXT("Ck.OnScreenDebugger.Provider.EntityInfo"),
        }));

    Layouts.Add(MakeLayout(
        TAG_Ck_OnScreenDebugger_Layout_Overview,
        ECk_DebugOverlay_Density::Ultra,
        {
            TEXT("Ck.OnScreenDebugger.Provider.EntityInfo"),
            TEXT("Ck.OnScreenDebugger.Provider.StateMachine"),
            TEXT("Ck.OnScreenDebugger.Provider.GOAP"),
            TEXT("Ck.OnScreenDebugger.Provider.FloatAttributes"),
        }));

    // AI layout is the default starting view.
    StartingLayout = TAG_Ck_OnScreenDebugger_Layout_AI;
}

// --------------------------------------------------------------------------------------------------------------------
