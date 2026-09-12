#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkSchedulerDebugger/Widgets/SCkSchedulerDebugger_Inspector.h"

#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiRepeat.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SWindow.h"

#include <limits>

namespace ck_scheduler_debugger_inspector_authored_test
{
    auto CountDescendantsByType(const TSharedRef<SWidget>& InRoot, const FString& InType) -> int32
    {
        int32 Count = InRoot->GetTypeAsString() == InType ? 1 : 0;
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            Count += CountDescendantsByType(Children->GetChildAt(Index), InType);
        }
        return Count;
    }

    auto Click(FSlateApplication& InSlate, const TSharedRef<SWidget>& InWidget) -> bool
    {
        const TSharedPtr<SWindow> Window = InSlate.FindWidgetWindow(InWidget);
        if (NOT Window.IsValid() || NOT Window->GetNativeWindow().IsValid()) { return false; }
        InSlate.PumpMessages(); InSlate.Tick();
        const FGeometry Geometry = InWidget->GetCachedGeometry();
        if (Geometry.GetLocalSize().X <= 0.0f || Geometry.GetLocalSize().Y <= 0.0f) { return false; }
        const FVector2D Position = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * .5f);
        const TSet<FKey> None;
        const TSet<FKey> Left{EKeys::LeftMouseButton};
        const FPointerEvent Move(0, FSlateApplication::CursorPointerIndex, Position, Position, None, EKeys::Invalid, 0, FModifierKeysState{});
        const FPointerEvent Down(0, FSlateApplication::CursorPointerIndex, Position, Position, Left, EKeys::LeftMouseButton, 0, FModifierKeysState{});
        const FPointerEvent Up(0, FSlateApplication::CursorPointerIndex, Position, Position, None, EKeys::LeftMouseButton, 0, FModifierKeysState{});
        InSlate.SetCursorPos(Position);
        InSlate.ProcessMouseMoveEvent(Move, true);
        const bool DownHandled = InSlate.ProcessMouseButtonDownEvent(Window->GetNativeWindow(), Down);
        const bool UpHandled = InSlate.ProcessMouseButtonUpEvent(Up);
        InSlate.Tick();
        return DownHandled && UpHandled;
    }

    auto FindButton(const TSharedRef<SWidget>& InRoot) -> TSharedPtr<SButton>
    {
        const FString Type = InRoot->GetTypeAsString();
        if (Type == TEXT("SButton") || Type == TEXT("SCkUiStyledButton"))
        { return StaticCastSharedRef<SButton>(InRoot); }
        const FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SButton> Found = FindButton(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)));
                Found.IsValid())
            { return Found; }
        }
        return {};
    }

    auto DescribeTree(const TSharedRef<SWidget>& InRoot, const int32 InDepth = 0) -> FString
    {
        FString Result = FString::ChrN(InDepth * 2, TEXT(' '))
            + FString::Printf(TEXT("%s tag='%s' children=%d\n"),
                *InRoot->GetTypeAsString(), *InRoot->GetTag().ToString(),
                InRoot->GetChildren() != nullptr ? InRoot->GetChildren()->Num() : 0);
        const FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        { Result += DescribeTree(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InDepth + 1); }
        return Result;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkSchedulerDebugger_InspectorAuthored,
    "Ck.UiAuthoring.SchedulerDebugger.Inspector.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkSchedulerDebugger_InspectorAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_scheduler_debugger_inspector_authored_test;
    if (NOT FSlateApplication::IsInitialized())
    {
        AddError(TEXT("Scheduler Inspector authored test requires Slate."));
        return false;
    }
    FSlateApplication& Slate = FSlateApplication::Get();
    TSharedPtr<FCkSchedulerDebugger_ViewModel> ViewModel = MakeShared<FCkSchedulerDebugger_ViewModel>();
    TSharedPtr<SCkSchedulerDebugger_Inspector> Inspector = SNew(SCkSchedulerDebugger_Inspector).ViewModel(ViewModel);
    TSharedPtr<SWindow> Host = SNew(SWindow).ClientSize(FVector2D{380.0f, 620.0f}).CreateTitleBar(false).HasCloseButton(false)[Inspector.ToSharedRef()];
    ON_SCOPE_EXIT { if (Host.IsValid()) { Slate.DestroyWindowImmediately(Host.ToSharedRef()); } };
    Slate.AddWindow(Host.ToSharedRef(), true);
    Slate.PumpMessages(); Slate.Tick(); Slate.Tick();
    auto View = Inspector->_AuthoredView;
    if (NOT TestTrue(TEXT("Production Scheduler Inspector admits authored HTML/CSS"),
        Inspector->_AuthoredMounted && View.IsValid() && View->GetLastResult().Succeeded))
    {
        AddError(Inspector->_AuthoredLoadError);
        return false;
    }
    TestTrue(TEXT("Empty selection uses authored empty state"), NOT Inspector->_AuthoredHasSelection
        && View->GetScroll(TEXT("scheduler-inspector-scroll")).IsValid()
        && View->GetRepeat(TEXT("scheduler-inspector-dependency-records")).IsValid());
    TestEqual(TEXT("Authored inspector owns the only production scroll surface"),
        CountDescendantsByType(Inspector.ToSharedRef(), TEXT("SCkUiScrollBox")), 1);

    auto Fixture = FCkSchedulerDebugger_ProcessorInfo{};
    Fixture.NodeIndex = 7;
    Fixture.ProcessorName = TEXT("FixtureProcessor");
    Fixture.DisplayName = TEXT("Fixture Processor");
    Fixture.GroupName = TEXT("FixtureGroup");
    Fixture.ExecutionOrder = 3;
    Fixture.HasDirtyMarker = true;
    Fixture.DirtyMarkerName = TEXT("FixtureDirty");
    Fixture.WasDirtyThisFrame = true;
    Fixture.PumpCountThisFrame = 2;
    Fixture.PumpPassTimesMs = {.25, .75};
    Fixture.MainPassTimeMs = 2.5;
    Fixture.TotalTicks = 17;
    Fixture.TickRate = .75;
    Fixture.TimingHistory = {1.0, 3.0, 2.5};
    Fixture.InEdges = {1};
    Fixture.OutEdges = {9};
    Fixture.WriteConflicts.Add({.PeerProcessorName = TEXT("Peer"), .FragmentName = TEXT("Fragment"), .WasAutoResolved = false});

    auto Before = Fixture;
    Before.NodeIndex = 1;
    Before.DisplayName = TEXT("Before Processor");
    Before.InEdges.Reset(); Before.OutEdges = {7};
    auto After = Fixture;
    After.NodeIndex = 9;
    After.DisplayName = TEXT("After Processor");
    After.InEdges = {7}; After.OutEdges.Reset();
    auto& ProductionProcessors = const_cast<TArray<FCkSchedulerDebugger_ProcessorInfo>&>(
        ViewModel->Get_DataCollector().Get_Processors());
    ProductionProcessors = {Before, Fixture, After};
    ViewModel->Set_SelectedProcessorIndex(1);
    if (NOT TestTrue(TEXT("ViewModel selection publishes the production scheduler projection"), Inspector->_AuthoredHasSelection)) { return false; }
    TestTrue(TEXT("Populated projection retains stable fields"), Inspector->_AuthoredHasSelection && Inspector->_AuthoredHasDirty
        && Inspector->_AuthoredHasConflicts && Inspector->_AuthoredName == TEXT("Fixture Processor")
        && Inspector->_AuthoredCurrentTiming == TEXT("2.500 ms") && Inspector->_AuthoredPeakTiming == TEXT("3.000 ms"));
    const auto Records = Inspector->_AuthoredDependencies->GetRecords();
    TestEqual(TEXT("Both dependency directions project"), Records.Num(), 2);
    TestEqual(TEXT("Dirty projection preserves marker, flag, count, and every pump pass"),
        Inspector->_AuthoredDirtyDetails->GetRecords().Num(), 5);
    const TSharedPtr<const FCkUiRecord> LastPumpPass = Inspector->_AuthoredDirtyDetails->GetRecords().Num() == 5
        ? Inspector->_AuthoredDirtyDetails->GetRecords()[4] : nullptr;
    const FCkUiFieldValue* LastPumpPassValue = LastPumpPass.IsValid() ? LastPumpPass->FindField(TEXT("value")) : nullptr;
    TestTrue(TEXT("Dirty projection retains the final pump-pass duration"),
        LastPumpPassValue != nullptr && LastPumpPassValue->Text.ToString() == TEXT("0.750 ms"));
    TestEqual(TEXT("Conflict projection preserves each conflict record"), Inspector->_AuthoredConflicts->GetRecords().Num(), 1);
    const TSharedPtr<const FCkUiRecord> Conflict = Inspector->_AuthoredConflicts->GetRecords().IsEmpty()
        ? nullptr : Inspector->_AuthoredConflicts->GetRecords()[0];
    const FCkUiFieldValue* ConflictPeer = Conflict.IsValid() ? Conflict->FindField(TEXT("peer")) : nullptr;
    const FCkUiFieldValue* ConflictFragment = Conflict.IsValid() ? Conflict->FindField(TEXT("fragment")) : nullptr;
    const FCkUiFieldValue* ConflictResolution = Conflict.IsValid() ? Conflict->FindField(TEXT("resolution")) : nullptr;
    TestTrue(TEXT("Conflict peer, fragment, and resolution retain their exact fields"),
        ConflictPeer != nullptr && ConflictPeer->Text.ToString() == TEXT("Peer")
        && ConflictFragment != nullptr && ConflictFragment->Text.ToString() == TEXT("Fragment")
        && ConflictResolution != nullptr && ConflictResolution->Text.ToString() == TEXT("UNRESOLVED"));

    auto Malformed = Fixture;
    Malformed.PumpPassTimesMs[0] = std::numeric_limits<double>::quiet_NaN();
    TestFalse(TEXT("Malformed numeric data is rejected before presentation mutation"),
        Inspector->DoPresentAuthored(&Malformed));
    TestTrue(TEXT("Rejected numeric data retains the last accepted projection"),
        Inspector->_AuthoredName == TEXT("Fixture Processor")
        && Inspector->_AuthoredDependencies->GetRecords().Num() == 2
        && Inspector->_AuthoredDirtyDetails->GetRecords().Num() == 5);

    Slate.PumpMessages(); Slate.Tick(); Slate.Tick();
    TSharedPtr<SWidget> DependencyItem = View->GetRepeat(
        TEXT("scheduler-inspector-dependency-records"))->GetItemWidget(TEXT("Run After:1"));
    TSharedPtr<SButton> DependencyButton = DependencyItem.IsValid()
        ? FindButton(DependencyItem.ToSharedRef()) : nullptr;
    if (NOT TestTrue(TEXT("Repeated authored dependency mounts a real button"), DependencyButton.IsValid()))
    {
        AddError(DependencyItem.IsValid() ? DescribeTree(DependencyItem.ToSharedRef()) : TEXT("Dependency item is null."));
        return false;
    }
    TSharedPtr<SScrollBox> InspectorScroll = View->GetScroll(TEXT("scheduler-inspector-scroll"));
    if (NOT TestTrue(TEXT("Authored dependency action remains reachable through the production scroll host"),
        InspectorScroll.IsValid())) { return false; }
    InspectorScroll->ScrollDescendantIntoView(
        DependencyButton.ToSharedRef(), false, EDescendantScrollDestination::IntoView);
    Slate.PumpMessages(); Slate.Tick(); Slate.Tick();
    TestTrue(TEXT("Authored dependency button is enabled and visibly arranged"),
        DependencyButton->IsEnabled()
        && DependencyButton->GetCachedGeometry().GetLocalSize().X > 4.0f
        && DependencyButton->GetCachedGeometry().GetLocalSize().Y > 4.0f);
    TestTrue(TEXT("Physical authored dependency button dispatches through the item-action route"),
        Click(Slate, DependencyButton.ToSharedRef()));
    TestEqual(TEXT("Dependency action returns selection authority to the ViewModel"), ViewModel->Get_SelectedProcessorIndex(), 0);
    ViewModel->Set_SelectedProcessorIndex(1);
    TestTrue(TEXT("Authored dependency action is bound for production navigation"),
        View->GetRepeat(TEXT("scheduler-inspector-dependency-records")).IsValid());

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup, Css;
    const FString Directory = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("Scheduler authored resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(Directory, TEXT("SchedulerInspector.ui.html")))
        && FFileHelper::LoadFileToString(Css, *FPaths::Combine(Directory, TEXT("SchedulerInspector.ui.css"))))) { return false; }
    const int64 AcceptedRevision = View->GetRevision();
    TestTrue(TEXT("Compatible reload succeeds"), View->TryReload(Markup, Css, TEXT("Scheduler Inspector compatible candidate")).Succeeded);
    TestTrue(TEXT("Compatible reload preserves the authored view"), Inspector->_AuthoredView == View && View->GetRevision() > AcceptedRevision);
    const int64 RevisionBeforeRejected = View->GetRevision();
    TestFalse(TEXT("Rejected reload fails closed"), View->TryReload(TEXT("<ui version=\"1\"><region name=\"main\"><text id=\"bad\" bind=\"missing\"/></region></ui>"), TEXT(""), TEXT("Scheduler Inspector rejected candidate")).Succeeded);
    TestEqual(TEXT("Rejected reload retains accepted revision"), View->GetRevision(), RevisionBeforeRejected);

    Inspector->DoPresentAuthored(nullptr);
    TestTrue(TEXT("Empty projection releases visible data"), NOT Inspector->_AuthoredHasSelection && Inspector->_AuthoredDependencies->GetRecords().IsEmpty());
    Slate.DestroyWindowImmediately(Host.ToSharedRef()); Host.Reset(); Slate.Tick();
    const TWeakPtr<FCkUiView> ReleasedView = View;
    const TWeakPtr<FCkUiCollection> ReleasedDependencies = Inspector->_AuthoredDependencies;
    DependencyButton.Reset(); DependencyItem.Reset(); InspectorScroll.Reset();
    View.Reset(); Inspector.Reset(); ViewModel.Reset(); Slate.Tick();
    TestFalse(TEXT("Destructor releases authored view"), ReleasedView.IsValid());
    TestFalse(TEXT("Destructor releases authored collection"), ReleasedDependencies.IsValid());
    return true;
}

#endif
