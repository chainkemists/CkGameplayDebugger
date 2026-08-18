#include "Misc/AutomationTest.h"
#include "CkEditorTools/Style/CkIconStyle.h"

#include "CkCore/Format/CkFormat.h"

#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"

// --------------------------------------------------------------------------------------------------------------------
// Icon completeness guard, modelled on the launcher's catalog census
// (CkDebuggerLauncher/Private/Tests/CkDebuggerLauncherCatalog.spec.cpp:69).
//
// Two silent failure modes this makes loud, permanently:
//   1. An inspector ships with no Get_IconName override at all — the base returns NAME_None and
//      SCkDebug_InspectorPanel simply omits the glyph slot, so the header reads as "different"
//      rather than "broken".
//   2. An inspector declares an id whose SVG does not exist on disk (a typo, or a file that was
//      renamed/never authored). Get_IconBrush returns nullptr and the slot is omitted exactly the
//      same way — PathNetwork and PathNetworkFollower lived in that state undetected.
//
// The registry is global and populated by the static auto-registrars at module load, so no world,
// PIE, or entity is needed. Icons resolve through the typed FCkIconStyle registry.
// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkEcsDebuggerInspectorIcons_EveryInspectorHasAResolvableIcon,
    "Ck.EcsDebugger.Inspectors.EveryInspectorHasAResolvableIcon",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// --------------------------------------------------------------------------------------------------------------------

bool FCkEcsDebuggerInspectorIcons_EveryInspectorHasAResolvableIcon::RunTest(const FString&)
{
    const auto AllMetadata = FCkDebuggerInspectorRegistry::Get().Get_AllMetadata();

    // Without this the whole spec is vacuously green if registration ever stops running.
    TestTrue(TEXT("The inspector registry is populated"), AllMetadata.Num() > 0);

    for (const auto& Metadata : AllMetadata)
    {
        const auto DeclaredMessage = ck::Format_UE(TEXT("Icon id declared: {}"), Metadata.ID);
        const auto BrushMessage    = ck::Format_UE(
            TEXT("Icon '{}' declared by inspector '{}' resolves to a registered brush"),
            Metadata.Icon, Metadata.ID);

        TestFalse(*DeclaredMessage, Metadata.Icon == ECk_Icon::None);
        TestNotNull(*BrushMessage, FCkIconStyle::Get_Brush(Metadata.Icon, ECk_Icon_BrushSize::Size_16x16));
    }

    return true;
}

// --------------------------------------------------------------------------------------------------------------------
