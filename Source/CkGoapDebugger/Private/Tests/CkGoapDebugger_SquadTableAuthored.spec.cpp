#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"
#include "CkGoapDebugger/Window/SCkGoapDebugger_SquadTable.h"

#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/SWindow.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

namespace ck_goap_debugger_squad_table_authored_tests
{
    auto Tick(FSlateApplication& InSlate) -> void
    {
        InSlate.PumpMessages();
        InSlate.Tick();
        InSlate.Tick();
    }

    auto FindTaggedWidget(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SWidget>
    {
        if (InRoot->GetTag() == InTag) { return InRoot; }

        const FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SWidget> Found = FindTaggedWidget(
                    ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag); Found.IsValid())
            { return Found; }
        }
        return {};
    }

    auto FindSearchBoxes(const TSharedRef<SWidget>& InRoot, TArray<TSharedPtr<SSearchBox>>& OutBoxes) -> void
    {
        if (InRoot->GetTypeAsString() == TEXT("SSearchBox"))
        { OutBoxes.Add(StaticCastSharedRef<SSearchBox>(InRoot)); }

        const FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        { FindSearchBoxes(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), OutBoxes); }
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkGoapDebuggerSquadTable_AuthoredEmpty,
    "Ck.UiAuthoring.GoapDebugger.SquadTable.Authored.Empty",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkGoapDebuggerSquadTable_AuthoredEmpty::RunTest(const FString&) -> bool
{
    using namespace ck_goap_debugger_squad_table_authored_tests;

    if (!FSlateApplication::IsInitialized())
    {
        AddError(TEXT("GOAP Squad authored test requires Slate."));
        return false;
    }

    const TSharedRef<FCkGoapDebugger_ViewModel> ViewModel = MakeShared<FCkGoapDebugger_ViewModel>();
    TSharedPtr<SCkGoapDebugger_SquadTable> Table = SNew(SCkGoapDebugger_SquadTable)
        .ViewModel(ViewModel);

    FSlateApplication& Slate = FSlateApplication::Get();
    TSharedPtr<SWindow> Host = SNew(SWindow)
        .AutoCenter(EAutoCenter::None)
        .ClientSize(FVector2D{1200.0f, 720.0f})
        .CreateTitleBar(false)
        .HasCloseButton(false)
        [Table.ToSharedRef()];
    ON_SCOPE_EXIT
    {
        if (Host.IsValid()) { Slate.DestroyWindowImmediately(Host.ToSharedRef()); }
    };

    Slate.AddWindow(Host.ToSharedRef(), true);
    Tick(Slate);

    TSharedPtr<FCkUiView> View = Table->Get_AuthoredView();
    if (!TestTrue(TEXT("Production Squad table retains a successfully loaded authored view"),
                   View.IsValid() && View->GetLastResult().Succeeded))
    {
        if (View.IsValid()) { AddError(FString::Join(View->GetLastResult().Errors, TEXT("\n"))); }
        else { AddError(Table->Get_AuthoredLoadFailure()); }
        return false;
    }

    const TSharedRef<SWidget> Main = View->GetRegion(TEXT("main"));
    TestTrue(TEXT("Authored Squad main region is mounted in the production table"),
             FindTaggedWidget(Table.ToSharedRef(), TEXT("goap-squad-root")).IsValid());

    const TSharedPtr<const FCkUiCollection> Collection = Table->Get_AuthoredCollection();
    TestTrue(TEXT("Authored Squad collection starts empty for an empty ViewModel"),
             Collection.IsValid() && Collection->GetRecords().IsEmpty());
    const TSharedPtr<SWidget> EmptyState = FindTaggedWidget(Main, TEXT("goap-squad-empty-wrap"));
    TestTrue(TEXT("Empty Squad state is effectively visible"),
             EmptyState.IsValid() && EmptyState->GetVisibility().IsVisible());

    FString Markup;
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    const FString UiDirectory = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    const FString MarkupPath = FPaths::Combine(UiDirectory, TEXT("GoapDebuggerSquad.ui.html"));
    TestTrue(TEXT("Installed Squad resource declares the production Inspect action"),
             Plugin.IsValid() && FFileHelper::LoadFileToString(Markup, *MarkupPath)
                 && Markup.Contains(TEXT("item-action=\"goap-squad-inspect\"")));

    TArray<TSharedPtr<SSearchBox>> Searches;
    FindSearchBoxes(Main, Searches);
    if (!TestTrue(TEXT("Authored Squad mounts both real search controls"), Searches.Num() == 2)) { return false; }
    const TSharedPtr<SSearchBox> Filter = Searches[0];
    const TSharedPtr<SSearchBox> Highlight = Searches[1];
    const TWeakPtr<FCkUiView> ViewBeforeReload = View;
    const TSharedPtr<SWidget> MainBeforeReload = Main;
    Filter->SetText(FText::FromString(TEXT("no-roster-match")));
    Highlight->SetText(FText::FromString(TEXT("no-roster-highlight")));
    Tick(Slate);
    TestTrue(TEXT("Empty authored search controls accept filter and highlight input"),
             Filter->GetText().ToString() == TEXT("no-roster-match")
                 && Highlight->GetText().ToString() == TEXT("no-roster-highlight")
                 && Table->Get_AuthoredView() == ViewBeforeReload.Pin());

    const int64 RevisionBeforeReload = View->GetRevision();
    TestFalse(TEXT("Unchanged Squad authored file poll is deduplicated"), View->PollFiles());
    Tick(Slate);
    TestTrue(TEXT("Unchanged poll retains authored view, region, and revision identity"),
             Table->Get_AuthoredView() == ViewBeforeReload.Pin()
                 && View->GetRegion(TEXT("main")) == MainBeforeReload
                 && View->GetRevision() == RevisionBeforeReload
                 && Filter->GetText().ToString() == TEXT("no-roster-match")
                 && Highlight->GetText().ToString() == TEXT("no-roster-highlight"));

    Table->Reset_ForWorldChange();
    TestTrue(TEXT("Squad reset clears its authored collection safely"),
             Collection->GetRecords().IsEmpty());

    const TWeakPtr<SCkGoapDebugger_SquadTable> WeakTable = Table;
    const TWeakPtr<FCkUiView> WeakView = View;
    Slate.DestroyWindowImmediately(Host.ToSharedRef());
    Host.Reset();
    Tick(Slate);
    View.Reset();
    Table.Reset();
    TestFalse(TEXT("Squad table releases its authored view when the owning widget is released"), WeakTable.IsValid());
    TestFalse(TEXT("Released Squad table does not retain authored view state"), WeakView.IsValid());

    // This empty, no-handle gate intentionally does not claim roster projection, stable planner
    // identities, sparkline samples, alert fields, EntityRef navigation, or Inspect routing. Those
    // require a real PIE ECS composition and belong to the subsequent production-path gate.
    return true;
}

#endif
