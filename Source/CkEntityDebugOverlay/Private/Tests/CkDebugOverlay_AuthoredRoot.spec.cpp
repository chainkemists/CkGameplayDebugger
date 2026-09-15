#include "CkEntityDebugOverlay/Slate/SCkDebugOverlay_Root.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CkSlateLayout/SCkUiSurface.h"

#include "Framework/Application/SlateApplication.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "Misc/AutomationTest.h"

namespace ck_debug_overlay_authored_spec
{
    auto FindTaggedWidget(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SWidget>
    {
        if (InRoot->GetTag() == InTag) { return InRoot; }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SWidget> Found = FindTaggedWidget(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag); Found.IsValid())
            { return Found; }
        }
        return nullptr;
    }

    auto ContainsWidget(const TSharedRef<SWidget>& InRoot, const TSharedRef<SWidget>& InTarget) -> bool
    {
        if (InRoot == InTarget) { return true; }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (ContainsWidget(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTarget)) { return true; }
        }
        return false;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebugOverlay_AuthoredRoot_Test,
    "Ck.DebugOverlay.Authored.Root",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugOverlay_AuthoredRoot_Test::RunTest(const FString&)
{
    using namespace ck_debug_overlay_authored_spec;
    if (NOT FSlateApplication::IsInitialized())
    { AddError(TEXT("Entity Debug Overlay authored-root test requires Slate.")); return false; }

    const TSharedRef<SCkDebugOverlay_Root> InstalledRoot = SNew(SCkDebugOverlay_Root);
    const TSharedPtr<FCkUiView> InstalledView = InstalledRoot->Get_AuthoredView();
    if (NOT TestTrue(TEXT("installed resources admit the authored multi-port overlay"),
        InstalledView.IsValid() && InstalledView->GetLastResult().Succeeded && NOT InstalledRoot->Get_UsesNativeFallback()))
    { return false; }
    const TSharedRef<SWidget> InstalledMain = InstalledView->GetRegion(TEXT("main"));
    if (NOT TestTrue(TEXT("installed shell exposes explicit root, world-tag, card, and ordinary-hint IDs"),
        FindTaggedWidget(InstalledMain, TEXT("entity-debug-overlay-root")).IsValid() &&
        FindTaggedWidget(InstalledMain, TEXT("entity-debug-overlay-world-tags")).IsValid() &&
        FindTaggedWidget(InstalledMain, TEXT("entity-debug-overlay-cards")).IsValid() &&
        FindTaggedWidget(InstalledMain, TEXT("entity-debug-overlay-hints-left")).IsValid() &&
        FindTaggedWidget(InstalledMain, TEXT("entity-debug-overlay-hints-left-text")).IsValid() &&
        FindTaggedWidget(InstalledMain, TEXT("entity-debug-overlay-hints-right")).IsValid() &&
        FindTaggedWidget(InstalledMain, TEXT("entity-debug-overlay-hints-right-text")).IsValid()))
    { return false; }
    InstalledRoot->Release_OwnerInteractions();

    const auto Markup = TEXT("<ui version=\"1\"><region name=\"main\"><overlay id=\"root\"><native id=\"world-tags\" bind=\"entity-debug-overlay-world-tags\" /><native id=\"cards\" bind=\"entity-debug-overlay-cards\" /><row id=\"hint-left\" visible=\"entity-debug-overlay-hints-left-visible\"><text id=\"hint-left-text\" bind=\"entity-debug-overlay-hints-text\" /></row><row id=\"hint-right\" visible=\"entity-debug-overlay-hints-right-visible\"><text id=\"hint-right-text\" bind=\"entity-debug-overlay-hints-text\" /></row></overlay></region></ui>");
    const auto Css = TEXT(".world-tags { min-width: 0; min-height: 0; flex-grow: 1; } .cards { min-width: 0; min-height: 0; flex-grow: 1; } .hint-left { horizontal-align: left; vertical-align: top; } .hint-right { horizontal-align: right; vertical-align: top; }");
    const auto BadMarkup = TEXT("<ui version=\"1\"><region name=\"main\"><overlay id=\"root\"><native id=\"world-tags\" bind=\"entity-debug-overlay-world-tags\" /><row id=\"hint-left\" visible=\"entity-debug-overlay-hints-left-visible\"><text id=\"hint-left-text\" bind=\"entity-debug-overlay-hints-text\" /></row><row id=\"hint-right\" visible=\"entity-debug-overlay-hints-right-visible\"><text id=\"hint-right-text\" bind=\"entity-debug-overlay-hints-text\" /></row></overlay></region></ui>");
    const auto Token = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const auto MarkupPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CkDebugOverlayAuthored_") + Token + TEXT(".ui.html"));
    const auto CssPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CkDebugOverlayAuthored_") + Token + TEXT(".ui.css"));

    if (NOT TestTrue(TEXT("writes isolated authored-root fixture"),
        FFileHelper::SaveStringToFile(BadMarkup, *MarkupPath) && FFileHelper::SaveStringToFile(Css, *CssPath)))
    { return false; }

    const TSharedRef<SCkDebugOverlay_Root> Root = SNew(SCkDebugOverlay_Root)
        .AuthoredMarkupPathOverride(MarkupPath)
        .AuthoredStylesheetPathOverride(CssPath);
    const TSharedPtr<FCkUiView> View = Root->Get_AuthoredView();
    if (NOT TestTrue(TEXT("missing native port falls back without publishing a partial shell"),
        View.IsValid() && Root->Get_UsesNativeFallback() && NOT Root->Get_AuthoredFailure().IsEmpty()))
    { return false; }

    const auto RevisionBeforeRecovery = View->GetRevision();
    if (NOT TestTrue(TEXT("same path can recover after the fixture is repaired"), FFileHelper::SaveStringToFile(Markup, *MarkupPath)))
    { return false; }
    Root->Tick(FGeometry{}, 1.0, 0.0f);
    if (NOT TestTrue(TEXT("recovery mounts the retained view and native port"),
        NOT Root->Get_UsesNativeFallback() && View->GetLastResult().Succeeded && View->GetRevision() > RevisionBeforeRecovery))
    { return false; }

    const TSharedRef<SWidget> Main = View->GetRegion(TEXT("main"));
    const TSharedPtr<SWidget> AuthoredRoot = FindTaggedWidget(Main, TEXT("root"));
    const TSharedPtr<SWidget> WorldTags = FindTaggedWidget(Main, TEXT("world-tags"));
    const TSharedPtr<SWidget> Cards = FindTaggedWidget(Main, TEXT("cards"));
    const TSharedPtr<SWidget> HintLeft = FindTaggedWidget(Main, TEXT("hint-left"));
    const TSharedPtr<SWidget> HintRight = FindTaggedWidget(Main, TEXT("hint-right"));
    const TSharedPtr<SBox> WorldTagPort = Root->Get_WorldTagPort();
    const TSharedPtr<SBox> CardPort = Root->Get_CardPort();
    if (NOT TestTrue(TEXT("exactly two mechanical production ports exist before authored mounting"),
        WorldTagPort.IsValid() && CardPort.IsValid()))
    { return false; }
    if (NOT TestTrue(TEXT("authored overlay owns explicit root, world-tag, card, and ordinary-hint topology"),
        AuthoredRoot.IsValid() && WorldTags.IsValid() && Cards.IsValid() && HintLeft.IsValid() && HintRight.IsValid() &&
        ContainsWidget(Main, WorldTagPort.ToSharedRef()) && ContainsWidget(Main, CardPort.ToSharedRef())))
    { return false; }
    Root->Update_KeyHints(TEXT("compact hint"), TEXT("full hint"), false, true);
    Main->SlatePrepass();
    if (NOT TestTrue(TEXT("authored hint visibility binding selects the ordinary left row"),
        HintLeft->GetVisibility() != EVisibility::Collapsed && HintRight->GetVisibility() == EVisibility::Collapsed))
    { return false; }
    Root->Set_PlateLayout(ECk_DebugOverlay_PlateAnchor::TopLeft, 720.0f);
    Main->SlatePrepass();
    if (NOT TestTrue(TEXT("authored hint visibility binding moves to the opposing configured corner"),
        HintLeft->GetVisibility() == EVisibility::Collapsed && HintRight->GetVisibility() != EVisibility::Collapsed))
    { return false; }

    const auto RevisionBeforeCompatibleReload = View->GetRevision();
    TestTrue(TEXT("compatible reload retains the same view identity"),
        View->TryReload(Markup, Css, TEXT("compatible overlay shell")).Succeeded &&
        Root->Get_AuthoredView() == View && View->GetRevision() > RevisionBeforeCompatibleReload);
    const auto RevisionBeforeRejectedReload = View->GetRevision();
    TestFalse(TEXT("malformed replacement cannot orphan the native port"),
        View->TryReload(BadMarkup, Css, TEXT("missing overlay port")).Succeeded);
    TestEqual(TEXT("rejected reload preserves the accepted revision"), View->GetRevision(), RevisionBeforeRejectedReload);

    const EVisibility HeldLeftVisibility = HintLeft->GetVisibility();
    const EVisibility HeldRightVisibility = HintRight->GetVisibility();
    Root->Release_OwnerInteractions();
    Root->Update_KeyHints(TEXT("released compact"), TEXT("released full"), true, false);
    Root->Set_PlateLayout(ECk_DebugOverlay_PlateAnchor::TopRight, 320.0f);
    TestTrue(TEXT("released overlay drops its authored view and both native ports"),
        !Root->Get_AuthoredView().IsValid() && !Root->Get_WorldTagPort().IsValid() && !Root->Get_CardPort().IsValid());
    TestTrue(TEXT("held ordinary hint widgets remain inert after owner release"),
        HintLeft->GetVisibility() == HeldLeftVisibility && HintRight->GetVisibility() == HeldRightVisibility);
    TestTrue(TEXT("released overlay publishes no admitted world-tag state"), Root->Get_AdmittedWorldTagKeys().IsEmpty());
    IFileManager::Get().Delete(*MarkupPath, false, true);
    IFileManager::Get().Delete(*CssPath, false, true);
    return true;
}

#endif
