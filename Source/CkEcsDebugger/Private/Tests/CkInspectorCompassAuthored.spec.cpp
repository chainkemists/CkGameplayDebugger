#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEcsDebugger/Inspectors/CkInspector_Compass.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"

#define private public
#include "CkCompass/CkCompass_Fragment.h"
#undef private

#include "CkCompass/CkCompass_Utils.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Utils/CkDebug_InspectorEditGuard.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SWindow.h"

namespace ck_inspector_compass_authored_test
{
    auto CreateCompass(
        ck::FEcsWorld& InWorld,
        const float InHeading,
        const float InArc,
        const int32 InMaxEntries,
        const int32 InEntryCount,
        const ECk_Compass_HeadingSource InSource,
        const FCk_Time InInterval) -> FCk_Handle
    {
        auto Entity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InWorld.Get_Registry());
        if (ck::Is_NOT_Valid(Entity)) { return {}; }
        // A direct, null world fragment is a valid no-world fixture. It keeps request-gate lookup
        // fail-closed without asking EntityLifetime to walk an intentionally absent owner chain.
        Entity.Add<TWeakObjectPtr<UWorld>>();

        auto Params = FCk_Fragment_Compass_ParamsData{InArc};
        Params.Set_MaxEntries(InMaxEntries);
        Params.Set_HeadingSource(InSource);
        Params.Set_UpdateInterval(InInterval);
        Entity.Add<ck::FFragment_Compass_Params>(Params);
        auto& Current = Entity.Add<ck::FFragment_Compass_Current>();
        Current._Observer = Entity;
        Current._HeadingDegrees = InHeading;
        Current._Entries.SetNum(InEntryCount);
        return Entity;
    }

    auto FindEditor(const TSharedRef<SWidget>& InRoot) -> TSharedPtr<SEditableTextBox>
    {
        if (InRoot->GetTypeAsString() == TEXT("SEditableTextBox"))
        { return StaticCastSharedRef<SEditableTextBox>(InRoot); }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SEditableTextBox> Found = FindEditor(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index))); Found.IsValid())
            { return Found; }
        }
        return nullptr;
    }

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

    auto Tick(FSlateApplication& InSlate) -> void
    {
        InSlate.PumpMessages();
        InSlate.Tick();
        InSlate.Tick();
    }

    auto SetAndCommit(FSlateApplication& InSlate, const TSharedRef<SEditableTextBox>& InInput,
        const FString& InText) -> bool
    {
        InSlate.SetUserFocus(0, InInput, EFocusCause::SetDirectly);
        Tick(InSlate);
        InInput->SetText(FText::FromString(InText));
        if (NOT InSlate.ProcessKeyDownEvent(FKeyEvent{EKeys::Enter, FModifierKeysState{}, 0, false, 0, 0}))
        { return false; }
        Tick(InSlate);
        return true;
    }

    struct FWindowScope final
    {
        explicit FWindowScope(FSlateApplication& InSlate) : Slate(InSlate) {}
        ~FWindowScope() { if (Window.IsValid()) { Slate.DestroyWindowImmediately(Window.ToSharedRef()); } }
        FSlateApplication& Slate;
        TSharedPtr<SWindow> Window;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorCompassAuthored,
    "Ck.UiAuthoring.EcsDebugger.CompassInspector.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkInspectorCompassAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_inspector_compass_authored_test;

    UCkDebuggerStyleSettings* StyleSettings = GetMutableDefault<UCkDebuggerStyleSettings>();
    if (NOT TestNotNull(TEXT("debugger style settings are available"), StyleSettings)) { return false; }
    const ECkDebugAxis_EditControlStyle PreviousEditControlStyle = StyleSettings->Selection.EditControlStyle;
    ON_SCOPE_EXIT { StyleSettings->Selection.EditControlStyle = PreviousEditControlStyle; };
    StyleSettings->Selection.EditControlStyle = ECkDebugAxis_EditControlStyle::Inline;

    auto InvalidInspector = FCkInspector_Compass{};
    const TSharedRef<SWidget> InvalidRendered = InvalidInspector.Build_Inspector(FCk_Handle{});
    if (NOT TestEqual(TEXT("default-invalid build still mounts the authored Compass shell safely"),
        InvalidRendered->GetTypeAsString(), FString{TEXT("SCkInspector_CompassAuthored")}))
    { AddError(InvalidInspector.Get_LastAuthoredLoadError()); return false; }
    const TSharedRef<SCkInspector_CompassAuthored> InvalidAuthored =
        StaticCastSharedRef<SCkInspector_CompassAuthored>(InvalidRendered);
    const TWeakPtr<FCkUiView> InvalidView = InvalidAuthored->Get_View();
    TestTrue(TEXT("default-invalid Compass projection fails closed"),
        InvalidAuthored->Is_Mounted() && NOT InvalidAuthored->Get_IsAvailable()
            && InvalidAuthored->Get_HeadingText() == TEXT("--")
            && InvalidAuthored->Get_ManualHeadingText() == TEXT("--")
            && InvalidAuthored->Get_SourceText() == TEXT("--")
            && ck::Is_NOT_Valid(InvalidAuthored->Get_Observer())
            && InvalidAuthored->Get_ArcText() == TEXT("--")
            && InvalidAuthored->Get_EntriesText() == TEXT("--")
            && FMath::IsNearlyZero(InvalidAuthored->Get_EntriesFraction())
            && InvalidAuthored->Get_FilterText() == TEXT("--")
            && InvalidAuthored->Get_IntervalText() == TEXT("--")
            && NOT InvalidAuthored->Get_CanEditManualHeading());
    InvalidInspector.OnDeactivated();
    TestTrue(TEXT("default-invalid Compass shell releases on deactivation"),
        InvalidAuthored->Is_Inert() && NOT InvalidAuthored->Is_Mounted() && NOT InvalidView.IsValid());

    auto World = ck::FEcsWorld{};
    FCk_Handle EntityA = CreateCompass(World, 12.5f, 180.0f, 4, 1,
        ECk_Compass_HeadingSource::Manual, FCk_Time::ZeroSecond());
    const FCk_Handle EntityB = CreateCompass(World, 100.0f, 90.0f, 8, 2,
        ECk_Compass_HeadingSource::Auto, FCk_Time{0.25f});
    if (NOT TestTrue(TEXT("fixture composes two independent Compass states"),
        ck::IsValid(EntityA) && ck::IsValid(EntityB) && EntityA != EntityB
            && UCk_Utils_Compass_UE::Has(EntityA) && UCk_Utils_Compass_UE::Has(EntityB)))
    { return false; }

    const TSharedRef<FCkInspectorEditGuard> EditGuard = MakeShared<FCkInspectorEditGuard>();
    auto Inspector = FCkInspector_Compass{};
    Inspector.Set_EditGuard(EditGuard);
    const TSharedRef<SWidget> RenderedA = Inspector.Build_Inspector(EntityA);
    const TSharedRef<SWidget> RenderedB = Inspector.Build_Inspector(EntityB);
    if (NOT TestEqual(TEXT("A mounts the authored Compass inspector"),
            RenderedA->GetTypeAsString(), FString{TEXT("SCkInspector_CompassAuthored")})
        || NOT TestEqual(TEXT("B mounts the authored Compass inspector"),
            RenderedB->GetTypeAsString(), FString{TEXT("SCkInspector_CompassAuthored")}))
    { AddError(Inspector.Get_LastAuthoredLoadError()); return false; }

    const TSharedRef<SCkInspector_CompassAuthored> AuthoredA = StaticCastSharedRef<SCkInspector_CompassAuthored>(RenderedA);
    const TSharedRef<SCkInspector_CompassAuthored> AuthoredB = StaticCastSharedRef<SCkInspector_CompassAuthored>(RenderedB);
    TSharedPtr<FCkUiView> ViewA = AuthoredA->Get_View();
    TSharedPtr<FCkUiView> ViewB = AuthoredB->Get_View();
    if (NOT TestTrue(TEXT("each Compass build owns an independent accepted authored view"),
        AuthoredA->Is_Mounted() && AuthoredB->Is_Mounted() && AuthoredA->Get_IsAvailable()
            && AuthoredB->Get_IsAvailable() && ViewA.IsValid() && ViewB.IsValid() && ViewA != ViewB
            && &ViewA->GetRegion(TEXT("main")).Get() != &ViewB->GetRegion(TEXT("main")).Get()
            && ViewA->GetLastResult().Succeeded && ViewB->GetLastResult().Succeeded))
    { AddError(Inspector.Get_LastAuthoredLoadError()); return false; }

    TestTrue(TEXT("authored Compass projects every existing row without flattening the observer"),
        AuthoredA->Get_HeadingText().StartsWith(TEXT("12.5°"))
            && AuthoredB->Get_HeadingText().StartsWith(TEXT("100.0°"))
            && FMath::IsNearlyEqual(AuthoredA->Get_ManualHeading(), 12.5f)
            && AuthoredA->Get_ManualHeadingText() == TEXT("12.5")
            && AuthoredA->Get_SourceText() == TEXT("Manual") && AuthoredB->Get_SourceText() == TEXT("Auto")
            && AuthoredA->Get_Observer() == EntityA && AuthoredB->Get_Observer() == EntityB
            && AuthoredA->Get_ArcText() == TEXT("180°") && AuthoredB->Get_ArcText() == TEXT("90°")
            && AuthoredA->Get_EntriesText() == TEXT("1 / 4") && AuthoredB->Get_EntriesText() == TEXT("2 / 8")
            && FMath::IsNearlyEqual(AuthoredA->Get_EntriesFraction(), 0.25f)
            && FMath::IsNearlyEqual(AuthoredB->Get_EntriesFraction(), 0.25f)
            && AuthoredA->Get_FilterText() == TEXT("(accepts all)")
            && AuthoredA->Get_IntervalText() == TEXT("0 (every frame)")
            && AuthoredB->Get_IntervalText() == TEXT("0.25s"));

    const TSharedPtr<SWidget> HeadingInput = FindTaggedWidget(RenderedA, TEXT("compass-manual-heading-input"));
    if (NOT TestTrue(TEXT("authored Manual Heading is a physical canonical numeric editor"), HeadingInput.IsValid()))
    { return false; }
    const TSharedPtr<SEditableTextBox> HeadingTextInput = FindEditor(HeadingInput.ToSharedRef());
    if (NOT TestTrue(TEXT("canonical Manual Heading editor exposes physical text entry"), HeadingTextInput.IsValid()))
    { return false; }
    FSlateApplication& Slate = FSlateApplication::Get();
    FWindowScope WindowScope{Slate};
    WindowScope.Window = SNew(SWindow)
        .AutoCenter(EAutoCenter::None)
        .ClientSize(FVector2D{520.0f, 300.0f})
        .CreateTitleBar(false)
        .HasCloseButton(false)
        [RenderedA];
    Slate.AddWindow(WindowScope.Window.ToSharedRef(), true);
    Tick(Slate);
    HeadingInput->SlatePrepass();
    TestTrue(TEXT("hermetic no-world fixture exercises the cosmetic request gate fail-closed"),
        NOT AuthoredA->Get_CanEditManualHeading() && NOT HeadingInput->IsEnabled()
            && NOT AuthoredA->Get_ManualHeadingDisabledReason().IsEmpty());
    AuthoredA->Commit_ManualHeading(270.0f);
    TestFalse(TEXT("gated authored commit does not publish a Compass request"),
        EntityA.Has<ck::FFragment_Compass_Requests>());

    UWorld* StandaloneWorld = UWorld::CreateWorld(EWorldType::Game, false);
    ON_SCOPE_EXIT { if (StandaloneWorld != nullptr) { StandaloneWorld->DestroyWorld(false); } };
    EntityA.Replace<TWeakObjectPtr<UWorld>>(StandaloneWorld);
    HeadingInput->SlatePrepass();
    TestTrue(TEXT("standalone-world Compass enables the authored cosmetic request path"),
        StandaloneWorld != nullptr && AuthoredA->Get_CanEditManualHeading() && HeadingInput->IsEnabled());
    if (NOT TestTrue(TEXT("physical Manual Heading editor accepts an Enter commit"),
        SetAndCommit(Slate, HeadingTextInput.ToSharedRef(), TEXT("999"))))
    { return false; }
    const auto* QueuedRequests = EntityA.Has<ck::FFragment_Compass_Requests>()
        ? &EntityA.Get<ck::FFragment_Compass_Requests>() : nullptr;
    const auto* ManualRequest = QueuedRequests != nullptr && QueuedRequests->Get_Requests().Num() == 1
        ? std::get_if<FCk_Request_Compass_SetManualHeading>(&QueuedRequests->Get_Requests()[0]) : nullptr;
    if (TestNotNull(TEXT("authorized physical commit queues exactly one manual-heading request"), ManualRequest))
    {
        TestTrue(TEXT("physical Manual Heading commit clamps through the canonical editor"),
            FMath::IsNearlyEqual(ManualRequest->Get_HeadingDegrees(), 360.0f));
    }
    TestFalse(TEXT("physical Manual Heading commit releases the panel edit guard"),
        EditGuard->Get_HasActiveEdit());
    EntityA.Try_Remove<ck::FFragment_Compass_Requests>();
    EntityA.Replace<TWeakObjectPtr<UWorld>>();

    auto RowsA = TMap<FString, FString>{};
    auto RowsB = TMap<FString, FString>{};
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(EntityA); RowsA = Capture.Get_Rows(); }
    { const auto Capture = FCkInspector_RowCaptureScope{}; Inspector.Build_Inspector(EntityB); RowsB = Capture.Get_Rows(); }
    const TSet<FString> Differing = FCkInspectorWidgetBuilder::Compute_DifferingLabels({RowsA, RowsB});
    TestTrue(TEXT("native Compass capture remains the eight-row multi-selection authority"),
        RowsA.Num() == 8 && RowsB.Num() == 8
            && RowsA.FindRef(TEXT("Heading:")) != RowsB.FindRef(TEXT("Heading:"))
            && RowsA.FindRef(TEXT("Source:")) == TEXT("Manual") && RowsB.FindRef(TEXT("Source:")) == TEXT("Auto")
            && RowsA.FindRef(TEXT("Arc:")) == TEXT("180°") && RowsB.FindRef(TEXT("Arc:")) == TEXT("90°")
            && RowsA.FindRef(TEXT("Entries:")) == TEXT("1 / 4") && RowsB.FindRef(TEXT("Entries:")) == TEXT("2 / 8")
            && RowsA.FindRef(TEXT("Filter:")) == RowsB.FindRef(TEXT("Filter:"))
            && RowsA.FindRef(TEXT("Interval:")) != RowsB.FindRef(TEXT("Interval:"))
            && Differing.Contains(TEXT("Heading:")) && Differing.Contains(TEXT("Manual Heading:"))
            && Differing.Contains(TEXT("Source:")) && Differing.Contains(TEXT("Arc:"))
            && Differing.Contains(TEXT("Entries:")) && Differing.Contains(TEXT("Interval:")));

    TSharedPtr<SCkInspector_CompassAuthored> DiffAuthored;
    {
        const auto DiffScope = FCkInspector_DiffMarkScope{&Differing};
        DiffAuthored = StaticCastSharedRef<SCkInspector_CompassAuthored>(Inspector.Build_Inspector(EntityA));
    }
    TestTrue(TEXT("authored Compass receives the exact native diff-label set"), DiffAuthored.IsValid()
        && DiffAuthored->Is_HeadingDiffMarked() == Differing.Contains(TEXT("Heading:"))
        && DiffAuthored->Is_ManualHeadingDiffMarked() == Differing.Contains(TEXT("Manual Heading:"))
        && DiffAuthored->Is_SourceDiffMarked() == Differing.Contains(TEXT("Source:"))
        && DiffAuthored->Is_ObserverDiffMarked() == Differing.Contains(TEXT("Observer:"))
        && DiffAuthored->Is_ArcDiffMarked() == Differing.Contains(TEXT("Arc:"))
        && DiffAuthored->Is_EntriesDiffMarked() == Differing.Contains(TEXT("Entries:"))
        && DiffAuthored->Is_FilterDiffMarked() == Differing.Contains(TEXT("Filter:"))
        && DiffAuthored->Is_IntervalDiffMarked() == Differing.Contains(TEXT("Interval:")));

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup, Stylesheet;
    const FString ResourceRoot = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("installed Compass HTML and CSS resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorCompass.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorCompass.ui.css")))))
    { return false; }
    TestTrue(TEXT("Compass HTML/CSS owns every label and places the numeric editor, observer port, and meter"),
        Markup.Contains(TEXT(">Heading:</text>")) && Markup.Contains(TEXT(">Manual Heading:</text>"))
            && Markup.Contains(TEXT(">Source:</text>")) && Markup.Contains(TEXT(">Observer:</text>"))
            && Markup.Contains(TEXT(">Arc:</text>")) && Markup.Contains(TEXT(">Entries:</text>"))
            && Markup.Contains(TEXT(">Filter:</text>")) && Markup.Contains(TEXT(">Interval:</text>"))
            && Markup.Contains(TEXT("<native id=\"compass-manual-heading-native\""))
            && Markup.Contains(TEXT("<native id=\"compass-observer-native\""))
            && Markup.Contains(TEXT("<debug-meter id=\"compass-entries-meter\""))
            && Stylesheet.Contains(TEXT(".compass-row")) && Stylesheet.Contains(TEXT(".compass-input")));

    const int64 RevisionABefore = ViewA->GetRevision();
    const int64 RevisionBBefore = ViewB->GetRevision();
    TestTrue(TEXT("compatible Compass reload is accepted by A"),
        ViewA->TryReload(Markup, Stylesheet, TEXT("Compass A compatible candidate")).Succeeded);
    TestTrue(TEXT("compatible reload retains A view and numeric editor identity without mutating B"),
        AuthoredA->Get_View() == ViewA && ViewA->GetRevision() > RevisionABefore
            && ViewB->GetRevision() == RevisionBBefore
            && FindTaggedWidget(RenderedA, TEXT("compass-manual-heading-input")) == HeadingInput);
    const TSharedRef<SWidget> MainBBefore = ViewB->GetRegion(TEXT("main"));
    const int64 RevisionBBeforeRejected = ViewB->GetRevision();
    TestFalse(TEXT("missing Compass native binding is rejected atomically by B"), ViewB->TryReload(
        Markup.Replace(TEXT("bind=\"compass-manual-heading-port\""), TEXT("bind=\"compass-missing\"")),
        Stylesheet, TEXT("Compass B rejected candidate")).Succeeded);
    TestTrue(TEXT("rejected Compass reload retains B tree and revision"),
        &ViewB->GetRegion(TEXT("main")).Get() == &MainBBefore.Get() && ViewB->GetRevision() == RevisionBBeforeRejected);

    StyleSettings->Selection.EditControlStyle = ECkDebugAxis_EditControlStyle::Hidden;
    const TSharedRef<SCkInspector_CompassAuthored> Hidden =
        StaticCastSharedRef<SCkInspector_CompassAuthored>(Inspector.Build_Inspector(EntityB));
    const TSharedPtr<SWidget> HiddenReadOnlyNode = FindTaggedWidget(
        Hidden->Get_View()->GetRegion(TEXT("main")), TEXT("compass-manual-heading-read-only"));
    Hidden->Get_View()->GetRegion(TEXT("main"))->SlatePrepass();
    TestTrue(TEXT("Hidden edit style collapses the authored input and exposes its read-only value"),
        NOT FindTaggedWidget(Hidden->Get_View()->GetRegion(TEXT("main")), TEXT("compass-manual-heading-input")).IsValid()
            && HiddenReadOnlyNode.IsValid() && HiddenReadOnlyNode->GetVisibility() == EVisibility::Visible);

    StyleSettings->Selection.EditControlStyle = ECkDebugAxis_EditControlStyle::OnHover;
    const TSharedRef<SCkInspector_CompassAuthored> Hover =
        StaticCastSharedRef<SCkInspector_CompassAuthored>(Inspector.Build_Inspector(EntityB));
    const TSharedPtr<SWidget> HoverInputNode = FindTaggedWidget(
        Hover->Get_View()->GetRegion(TEXT("main")), TEXT("compass-manual-heading-input"));
    const TSharedPtr<SWidget> HoverReadOnlyNode = FindTaggedWidget(
        Hover->Get_View()->GetRegion(TEXT("main")), TEXT("compass-manual-heading-read-only"));
    Hover->Get_View()->GetRegion(TEXT("main"))->SlatePrepass();
    TestTrue(TEXT("OnHover edit style preserves the stable value footprint and hides the editor at rest"),
        HoverInputNode.IsValid() && HoverReadOnlyNode.IsValid()
            && HoverInputNode->GetVisibility() == EVisibility::Hidden
            && HoverReadOnlyNode->GetVisibility() == EVisibility::Visible);
    StyleSettings->Selection.EditControlStyle = ECkDebugAxis_EditControlStyle::Inline;

    const TSharedRef<FCkInspectorEditGuard> InteractionGuard = MakeShared<FCkInspectorEditGuard>();
    auto InteractionInspector = FCkInspector_Compass{};
    InteractionInspector.Set_EditGuard(InteractionGuard);
    const TSharedRef<SCkInspector_CompassAuthored> InteractionAuthored =
        StaticCastSharedRef<SCkInspector_CompassAuthored>(InteractionInspector.Build_Inspector(EntityB));
    const TSharedPtr<SWidget> InteractionEditor = FindTaggedWidget(
        InteractionAuthored->Get_View()->GetRegion(TEXT("main")), TEXT("compass-manual-heading-input"));
    const TSharedPtr<SEditableTextBox> InteractionTextBox = InteractionEditor.IsValid()
        ? FindEditor(InteractionEditor.ToSharedRef()) : nullptr;
    if (TestTrue(TEXT("canonical authored numeric editor exposes its physical text entry"), InteractionTextBox.IsValid()))
    {
        InteractionTextBox->SetText(FText::FromString(TEXT("123.4")));
        TestTrue(TEXT("authored numeric typing claims the panel edit guard synchronously"),
            InteractionGuard->Get_HasActiveEdit());
    }
    InteractionInspector.OnDeactivated();
    TestFalse(TEXT("Compass deactivation releases an active authored edit guard"),
        InteractionGuard->Get_HasActiveEdit());

    TSharedPtr<SCkInspector_CompassAuthored> DestructorAuthored;
    {
        auto DestructorInspector = MakeUnique<FCkInspector_Compass>();
        DestructorAuthored = StaticCastSharedRef<SCkInspector_CompassAuthored>(DestructorInspector->Build_Inspector(EntityB));
    }
    TestTrue(TEXT("Compass inspector destruction makes its authored build inert"),
        DestructorAuthored.IsValid() && DestructorAuthored->Is_Inert() && NOT DestructorAuthored->Is_Mounted());

    TestTrue(TEXT("removing Compass Current after mount leaves the entity live"),
        EntityA.Try_Remove<ck::FFragment_Compass_Current>() && ck::IsValid(EntityA) && NOT Inspector.CanInspect(EntityA));
    ViewA->GetRegion(TEXT("main"))->SlatePrepass();
    TestTrue(TEXT("post-mount composition loss fails every Compass getter and editor closed"),
        NOT AuthoredA->Get_IsAvailable() && AuthoredA->Get_HeadingText() == TEXT("--")
            && AuthoredA->Get_SourceText() == TEXT("--") && ck::Is_NOT_Valid(AuthoredA->Get_Observer())
            && AuthoredA->Get_EntriesText() == TEXT("--") && NOT AuthoredA->Get_CanEditManualHeading()
            && NOT HeadingInput->IsEnabled());
    AuthoredA->Commit_ManualHeading(45.0f);
    TestFalse(TEXT("held authored numeric path cannot publish after Compass removal"),
        EntityA.Has<ck::FFragment_Compass_Requests>());

    const TWeakPtr<FCkUiView> ReleasedViewA = ViewA;
    const TWeakPtr<FCkUiView> ReleasedViewB = ViewB;
    Inspector.OnDeactivated();
    TestTrue(TEXT("Compass deactivation makes every retained authored build inert"),
        AuthoredA->Is_Inert() && AuthoredB->Is_Inert() && DiffAuthored->Is_Inert() && Hidden->Is_Inert() && Hover->Is_Inert()
            && NOT AuthoredA->Is_Mounted() && NOT AuthoredB->Is_Mounted());
    ViewA.Reset();
    ViewB.Reset();
    TestFalse(TEXT("Compass deactivation releases every per-build view"),
        ReleasedViewA.IsValid() || ReleasedViewB.IsValid());
    return true;
}

#endif
