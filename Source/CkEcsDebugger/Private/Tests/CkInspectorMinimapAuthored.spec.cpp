#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEcsDebugger/Inspectors/CkInspector_Minimap.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"
#define private public
#include "CkMinimap/CkMinimap_Fragment.h"
#undef private
#include "CkMinimap/CkMinimap_Utils.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Utils/CkDebug_InspectorEditGuard.h"
#include "CkSlateLayout/SCkUiSurface.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SWindow.h"

namespace ck_inspector_minimap_authored_test
{
    auto CreateMinimap(
        const FCk_Handle& InLifetimeOwner,
        const float InViewExtent,
        const ECk_Minimap_ProjectionMode InProjection,
        const ECk_Minimap_RotationMode InRotation,
        const ECk_Minimap_FrameShape InFrame,
        const FVector& InOrigin,
        const float InYaw,
        const int32 InMaxEntries,
        const int32 InEntryCount) -> FCk_Handle
    {
        auto Entity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InLifetimeOwner);
        if (ck::Is_NOT_Valid(Entity))
        { return {}; }

        auto Params = FCk_Fragment_Minimap_ParamsData{InViewExtent};
        Params.Set_ProjectionMode(InProjection);
        Params.Set_RotationMode(InRotation);
        Params.Set_FrameShape(InFrame);
        Params.Set_MaxEntries(InMaxEntries);
        Params.Set_FixedBounds(FCk_Minimap_WorldBounds{FVector2D{100.0, 200.0}, FVector2D{300.0, 400.0}});
        Entity.Add<ck::FFragment_Minimap_Params>(Params);
        auto& Current = Entity.Add<ck::FFragment_Minimap_Current>();
        Current._Observer = Entity;
        Current._ViewExtent = InViewExtent;
        Current._RotationMode = InRotation;
        Current._ViewOrigin = InOrigin;
        Current._ViewYawDegrees = InYaw;
        Current._Entries.SetNum(InEntryCount);
        return Entity;
    }

    auto FindTaggedWidget(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SWidget>
    {
        if (InRoot->GetTag() == InTag)
        { return InRoot; }

        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SWidget> Found = FindTaggedWidget(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag); Found.IsValid())
            { return Found; }
        }
        return nullptr;
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

    auto Tick(FSlateApplication& InSlate) -> void
    {
        InSlate.PumpMessages();
        InSlate.Tick();
    }

    auto SetAndCommit(
        FSlateApplication& InSlate,
        const TSharedRef<SEditableTextBox>& InInput,
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
        ~FWindowScope()
        {
            if (Window.IsValid())
            { Slate.DestroyWindowImmediately(Window.ToSharedRef()); }
        }

        FSlateApplication& Slate;
        TSharedPtr<SWindow> Window;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorMinimapAuthored,
    "Ck.UiAuthoring.EcsDebugger.MinimapInspector.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkInspectorMinimapAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_inspector_minimap_authored_test;

    UCkDebuggerStyleSettings* StyleSettings = GetMutableDefault<UCkDebuggerStyleSettings>();
    if (NOT TestNotNull(TEXT("debugger style settings are available"), StyleSettings))
    { return false; }
    const ECkDebugAxis_EditControlStyle PreviousEditControlStyle = StyleSettings->Selection.EditControlStyle;
    ON_SCOPE_EXIT { StyleSettings->Selection.EditControlStyle = PreviousEditControlStyle; };
    StyleSettings->Selection.EditControlStyle = ECkDebugAxis_EditControlStyle::Inline;

    FCkInspector_Minimap Inspector;
    const TSharedRef<SWidget> InvalidA = Inspector.Build_Inspector(FCk_Handle{});
    const TSharedRef<SWidget> InvalidB = Inspector.Build_Inspector(FCk_Handle{});
    if (NOT TestEqual(TEXT("invalid Minimap state still composes the authored shell"),
        InvalidA->GetTypeAsString(), FString{TEXT("SCkInspector_MinimapAuthored")}))
    { AddError(Inspector.Get_LastAuthoredLoadError()); return false; }

    const TSharedRef<SCkInspector_MinimapAuthored> AuthoredA = StaticCastSharedRef<SCkInspector_MinimapAuthored>(InvalidA);
    const TSharedRef<SCkInspector_MinimapAuthored> AuthoredB = StaticCastSharedRef<SCkInspector_MinimapAuthored>(InvalidB);
    TSharedPtr<FCkUiView> ViewA = AuthoredA->Get_View();
    TSharedPtr<FCkUiView> ViewB = AuthoredB->Get_View();
    TestTrue(TEXT("invalid Minimap state fails closed without a partial composition"),
        AuthoredA->Is_Mounted() && NOT AuthoredA->Get_IsAvailable()
            && AuthoredA->Get_ProjectionText() == TEXT("--")
            && AuthoredA->Get_RotationText() == TEXT("--")
            && AuthoredA->Get_FrameText() == TEXT("--")
            && AuthoredA->Get_ViewOriginText() == TEXT("--")
            && AuthoredA->Get_ViewYawText() == TEXT("--")
            && AuthoredA->Get_EntriesText() == TEXT("--")
            && AuthoredA->Get_FixedBoundsText() == TEXT("--"));
    TestTrue(TEXT("each authored Minimap build owns an independent view"),
        ViewA.IsValid() && ViewB.IsValid() && ViewA != ViewB
            && &ViewA->GetRegion(TEXT("main")).Get() != &ViewB->GetRegion(TEXT("main")).Get());

    auto World = ck::FEcsWorld{};
    auto LifetimeOwner = UCk_Utils_EntityLifetime_UE::Get_TransientEntity(World.Get_Registry());
    UWorld* const TestWorld = GWorld;
    if (NOT TestTrue(TEXT("fixture has a transient owner and automation world"),
        ck::IsValid(LifetimeOwner) && TestWorld != nullptr))
    { return false; }
    LifetimeOwner.Add<TWeakObjectPtr<UWorld>>(TestWorld);
    auto EntityA = CreateMinimap(LifetimeOwner, 1500.0f,
        ECk_Minimap_ProjectionMode::ObserverCentric,
        ECk_Minimap_RotationMode::NorthLocked,
        ECk_Minimap_FrameShape::Rectangle,
        FVector{10.0, 20.0, 30.0}, 45.0f, 8, 2);
    auto EntityB = CreateMinimap(LifetimeOwner, 3000.0f,
        ECk_Minimap_ProjectionMode::FixedBounds,
        ECk_Minimap_RotationMode::RotateWithObserver,
        ECk_Minimap_FrameShape::Circle,
        FVector{40.0, 50.0, 60.0}, 90.0f, 16, 4);
    if (NOT TestTrue(TEXT("fixture composes two independent complete Minimap states"),
        ck::IsValid(EntityA) && ck::IsValid(EntityB) && EntityA != EntityB
            && UCk_Utils_Minimap_UE::Has(EntityA) && UCk_Utils_Minimap_UE::Has(EntityB)))
    { return false; }

    const TSharedRef<FCkInspectorEditGuard> EditGuard = MakeShared<FCkInspectorEditGuard>();
    auto ValidInspector = FCkInspector_Minimap{};
    ValidInspector.Set_EditGuard(EditGuard);
    const TSharedRef<SWidget> RenderedA = ValidInspector.Build_Inspector(EntityA);
    const TSharedRef<SWidget> RenderedB = ValidInspector.Build_Inspector(EntityB);
    if (NOT TestEqual(TEXT("A mounts the authored Minimap inspector"),
            RenderedA->GetTypeAsString(), FString{TEXT("SCkInspector_MinimapAuthored")})
        || NOT TestEqual(TEXT("B mounts the authored Minimap inspector"),
            RenderedB->GetTypeAsString(), FString{TEXT("SCkInspector_MinimapAuthored")}))
    {
        AddError(ValidInspector.Get_LastAuthoredLoadError());
        return false;
    }
    const TSharedRef<SCkInspector_MinimapAuthored> ValidAuthoredA =
        StaticCastSharedRef<SCkInspector_MinimapAuthored>(RenderedA);
    const TSharedRef<SCkInspector_MinimapAuthored> ValidAuthoredB =
        StaticCastSharedRef<SCkInspector_MinimapAuthored>(RenderedB);
    TestTrue(TEXT("authored Minimap projects all live scalar and entity values"),
        ValidAuthoredA->Get_IsAvailable() && ValidAuthoredB->Get_IsAvailable()
            && ValidAuthoredA->Get_ProjectionText() == TEXT("Observer Centric")
            && ValidAuthoredB->Get_ProjectionText() == TEXT("Fixed Bounds")
            && ValidAuthoredA->Get_RotationText() == TEXT("North Locked")
            && ValidAuthoredB->Get_RotationText() == TEXT("Rotate With Observer")
            && ValidAuthoredA->Get_FrameText() == TEXT("Rectangle")
            && ValidAuthoredB->Get_FrameText() == TEXT("Circle")
            && FMath::IsNearlyEqual(ValidAuthoredA->Get_ViewExtent(), 1500.0f)
            && ValidAuthoredA->Get_ViewOriginText() == TEXT("10, 20, 30")
            && ValidAuthoredA->Get_ViewYawText() == TEXT("45.0°")
            && ValidAuthoredA->Get_Observer() == EntityA
            && ValidAuthoredA->Get_EntriesText() == TEXT("2 / 8")
            && FMath::IsNearlyEqual(ValidAuthoredA->Get_EntriesFraction(), 0.25f)
            && ValidAuthoredA->Get_FixedBoundsText() == TEXT("(observer-centric)")
            && ValidAuthoredB->Get_FixedBoundsText() == TEXT("C(100, 200)  HE(300, 400)"));

    auto ValidRowsA = TMap<FString, FString>{};
    auto ValidRowsB = TMap<FString, FString>{};
    { const FCkInspector_RowCaptureScope Capture; ValidInspector.Build_Inspector(EntityA); ValidRowsA = Capture.Get_Rows(); }
    { const FCkInspector_RowCaptureScope Capture; ValidInspector.Build_Inspector(EntityB); ValidRowsB = Capture.Get_Rows(); }
    const TSet<FString> Differing = FCkInspectorWidgetBuilder::Compute_DifferingLabels({ValidRowsA, ValidRowsB});
    TestTrue(TEXT("native Minimap capture preserves exact nine-row values and diff authority"),
        ValidRowsA.Num() == 9 && ValidRowsB.Num() == 9
            && ValidRowsA.FindRef(TEXT("Projection:")) == TEXT("Observer Centric")
            && ValidRowsB.FindRef(TEXT("Projection:")) == TEXT("Fixed Bounds")
            && ValidRowsA.FindRef(TEXT("Frame:")) == TEXT("Rectangle")
            && ValidRowsB.FindRef(TEXT("Frame:")) == TEXT("Circle")
            && ValidRowsA.FindRef(TEXT("View Origin:")) == TEXT("10  20  30")
            && ValidRowsA.FindRef(TEXT("Entries:")) == TEXT("2 / 8")
            && Differing.Contains(TEXT("Projection:")) && Differing.Contains(TEXT("Rotation:"))
            && Differing.Contains(TEXT("Frame:")) && Differing.Contains(TEXT("View Extent:"))
            && Differing.Contains(TEXT("View Origin:")) && Differing.Contains(TEXT("View Yaw:"))
            && Differing.Contains(TEXT("Observer:")) && Differing.Contains(TEXT("Entries:"))
            && Differing.Contains(TEXT("Fixed Bounds:")));
    TSharedPtr<SCkInspector_MinimapAuthored> DiffAuthored;
    {
        const FCkInspector_DiffMarkScope DiffScope{&Differing};
        DiffAuthored = StaticCastSharedRef<SCkInspector_MinimapAuthored>(ValidInspector.Build_Inspector(EntityA));
    }
    TestTrue(TEXT("authored Minimap receives Frame and every other native diff mark"),
        DiffAuthored.IsValid() && DiffAuthored->Is_DiffMarked(TEXT("Projection:"))
            && DiffAuthored->Is_DiffMarked(TEXT("Rotation:"))
            && DiffAuthored->Is_DiffMarked(TEXT("Frame:"))
            && DiffAuthored->Is_DiffMarked(TEXT("View Extent:"))
            && DiffAuthored->Is_DiffMarked(TEXT("View Origin:"))
            && DiffAuthored->Is_DiffMarked(TEXT("View Yaw:"))
            && DiffAuthored->Is_DiffMarked(TEXT("Observer:"))
            && DiffAuthored->Is_DiffMarked(TEXT("Entries:"))
            && DiffAuthored->Is_DiffMarked(TEXT("Fixed Bounds:")));

    const TSharedPtr<SWidget> RotationInput = FindTaggedWidget(RenderedA, TEXT("minimap-rotation-input"));
    const TSharedPtr<SWidget> ExtentInput = FindTaggedWidget(RenderedA, TEXT("minimap-view-extent-input"));
    const TSharedPtr<SEditableTextBox> ExtentTextInput = ExtentInput.IsValid()
        ? FindEditor(ExtentInput.ToSharedRef()) : nullptr;
    if (NOT TestTrue(TEXT("authored Minimap retains canonical physical rotation and extent controls"),
        RotationInput.IsValid() && ExtentInput.IsValid() && ExtentTextInput.IsValid()))
    { return false; }
    RenderedA->SlatePrepass();
    TestTrue(TEXT("world-bound Minimap enables both CosmeticOnly edit ports"),
        RotationInput->IsEnabled() && ExtentInput->IsEnabled());

    ValidAuthoredA->Commit_Rotation(1);
    const auto* RotationRequests = EntityA.Has<ck::FFragment_Minimap_Requests>()
        ? &EntityA.Get<ck::FFragment_Minimap_Requests>() : nullptr;
    const auto* RotationRequest = RotationRequests != nullptr && RotationRequests->Get_Requests().Num() == 1
        ? std::get_if<FCk_Request_Minimap_SetRotationMode>(&RotationRequests->Get_Requests()[0]) : nullptr;
    if (TestNotNull(TEXT("authorized rotation commit queues exactly one typed request"), RotationRequest))
    {
        TestEqual(TEXT("rotation request preserves the selected mode"),
            RotationRequest->Get_RotationMode(), ECk_Minimap_RotationMode::RotateWithObserver);
    }
    EntityA.Try_Remove<ck::FFragment_Minimap_Requests>();

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
    if (NOT TestTrue(TEXT("physical View Extent editor accepts an Enter commit"),
        SetAndCommit(Slate, ExtentTextInput.ToSharedRef(), TEXT("0"))))
    { return false; }
    const auto* ExtentRequests = EntityA.Has<ck::FFragment_Minimap_Requests>()
        ? &EntityA.Get<ck::FFragment_Minimap_Requests>() : nullptr;
    const auto* ExtentRequest = ExtentRequests != nullptr && ExtentRequests->Get_Requests().Num() == 1
        ? std::get_if<FCk_Request_Minimap_SetViewExtent>(&ExtentRequests->Get_Requests()[0]) : nullptr;
    if (TestNotNull(TEXT("physical extent commit queues exactly one typed request"), ExtentRequest))
    {
        TestTrue(TEXT("canonical extent editor clamps to the public minimum"),
            FMath::IsNearlyEqual(ExtentRequest->Get_ViewExtent(), 1.0f));
    }
    TestFalse(TEXT("physical extent commit releases the panel edit guard"), EditGuard->Get_HasActiveEdit());
    EntityA.Try_Remove<ck::FFragment_Minimap_Requests>();

    StyleSettings->Selection.EditControlStyle = ECkDebugAxis_EditControlStyle::Hidden;
    const TSharedRef<SCkInspector_MinimapAuthored> Hidden =
        StaticCastSharedRef<SCkInspector_MinimapAuthored>(ValidInspector.Build_Inspector(EntityB));
    TestTrue(TEXT("Hidden edit style exposes read-only rotation and extent values"),
        FindTaggedWidget(Hidden->Get_View()->GetRegion(TEXT("main")), TEXT("minimap-rotation-read-only")).IsValid()
            && FindTaggedWidget(Hidden->Get_View()->GetRegion(TEXT("main")), TEXT("minimap-view-extent-read-only")).IsValid()
            && NOT FindTaggedWidget(Hidden->Get_View()->GetRegion(TEXT("main")), TEXT("minimap-rotation-input")).IsValid()
            && NOT FindTaggedWidget(Hidden->Get_View()->GetRegion(TEXT("main")), TEXT("minimap-view-extent-input")).IsValid());
    StyleSettings->Selection.EditControlStyle = ECkDebugAxis_EditControlStyle::OnHover;
    const TSharedRef<SCkInspector_MinimapAuthored> Hover =
        StaticCastSharedRef<SCkInspector_MinimapAuthored>(ValidInspector.Build_Inspector(EntityB));
    Hover->Get_View()->GetRegion(TEXT("main"))->SlatePrepass();
    const TSharedPtr<SWidget> HoverRotationInput = FindTaggedWidget(
        Hover->Get_View()->GetRegion(TEXT("main")), TEXT("minimap-rotation-input"));
    const TSharedPtr<SWidget> HoverExtentInput = FindTaggedWidget(
        Hover->Get_View()->GetRegion(TEXT("main")), TEXT("minimap-view-extent-input"));
    TestTrue(TEXT("OnHover edit style retains both controls but hides them at rest"),
        HoverRotationInput.IsValid() && HoverExtentInput.IsValid()
            && HoverRotationInput->GetVisibility() == EVisibility::Hidden
            && HoverExtentInput->GetVisibility() == EVisibility::Hidden);
    StyleSettings->Selection.EditControlStyle = ECkDebugAxis_EditControlStyle::Inline;

    auto RowsA = TMap<FString, FString>{};
    auto RowsB = TMap<FString, FString>{};
    { const FCkInspector_RowCaptureScope Capture; Inspector.Build_Inspector(FCk_Handle{}); RowsA = Capture.Get_Rows(); }
    { const FCkInspector_RowCaptureScope Capture; Inspector.Build_Inspector(FCk_Handle{}); RowsB = Capture.Get_Rows(); }
    TestTrue(TEXT("native Minimap capture remains the nine-row multi-select authority"),
        RowsA.Num() == 9 && RowsA.OrderIndependentCompareEqual(RowsB)
            && RowsA.Contains(TEXT("Projection:")) && RowsA.Contains(TEXT("Rotation:"))
            && RowsA.Contains(TEXT("Frame:"))
            && RowsA.Contains(TEXT("View Extent:")) && RowsA.Contains(TEXT("View Origin:"))
            && RowsA.Contains(TEXT("View Yaw:")) && RowsA.Contains(TEXT("Observer:"))
            && RowsA.Contains(TEXT("Entries:")) && RowsA.Contains(TEXT("Fixed Bounds:")));

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup, Stylesheet;
    const FString Root = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("installed Minimap authored resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(Root, TEXT("EcsInspectorMinimap.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(Root, TEXT("EcsInspectorMinimap.ui.css"))))) { return false; }
    TestTrue(TEXT("HTML owns all nine labels and the three canonical native ports"),
        Markup.Contains(TEXT(">Projection:</text>")) && Markup.Contains(TEXT(">Rotation:</text>"))
            && Markup.Contains(TEXT(">Frame:</text>"))
            && Markup.Contains(TEXT(">View Extent:</text>")) && Markup.Contains(TEXT(">View Origin:</text>"))
            && Markup.Contains(TEXT(">View Yaw:</text>")) && Markup.Contains(TEXT(">Observer:</text>"))
            && Markup.Contains(TEXT(">Entries:</text>")) && Markup.Contains(TEXT(">Fixed Bounds:</text>"))
            && Markup.Contains(TEXT("minimap-rotation-port")) && Markup.Contains(TEXT("minimap-view-extent-port"))
            && Markup.Contains(TEXT("minimap-observer-port")) && Stylesheet.Contains(TEXT(".minimap-row")));

    const int64 Revision = ViewA->GetRevision();
    const int64 RevisionB = ViewB->GetRevision();
    TestTrue(TEXT("compatible Minimap reload is accepted"),
        ViewA->TryReload(Markup, Stylesheet, TEXT("Minimap compatible candidate")).Succeeded);
    TestTrue(TEXT("compatible reload is isolated to its own Minimap view"),
        ViewA->GetRevision() > Revision && ViewB->GetRevision() == RevisionB);
    const TSharedRef<SWidget> MainBefore = ViewB->GetRegion(TEXT("main"));
    const int64 RejectedRevision = ViewB->GetRevision();
    TestFalse(TEXT("missing native port is rejected atomically"), ViewB->TryReload(
        Markup.Replace(TEXT("minimap-view-extent-port"), TEXT("minimap-missing-port")), Stylesheet,
        TEXT("Minimap rejected candidate")).Succeeded);
    TestTrue(TEXT("rejected reload preserves the existing Minimap tree"),
        &ViewB->GetRegion(TEXT("main")).Get() == &MainBefore.Get() && ViewB->GetRevision() == RejectedRevision);

    TSharedPtr<SCkInspector_MinimapAuthored> DestructorAuthored;
    auto DestructorView = TWeakPtr<FCkUiView>{};
    {
        auto DestructorInspector = MakeUnique<FCkInspector_Minimap>();
        DestructorAuthored = StaticCastSharedRef<SCkInspector_MinimapAuthored>(
            DestructorInspector->Build_Inspector(EntityB));
        DestructorView = DestructorAuthored->Get_View();
    }
    TestTrue(TEXT("Minimap inspector destruction releases its retained authored view"),
        DestructorAuthored.IsValid() && DestructorAuthored->Is_Inert()
            && NOT DestructorAuthored->Is_Mounted() && NOT DestructorView.IsValid());

    TestTrue(TEXT("Current-only teardown leaves the entity live but removes complete Minimap inspectability"),
        EntityA.Try_Remove<ck::FFragment_Minimap_Current>()
            && ck::IsValid(EntityA) && EntityA.Has<ck::FFragment_Minimap_Params>()
            && NOT ValidInspector.CanInspect(EntityA));
    ValidAuthoredA->Get_View()->GetRegion(TEXT("main"))->SlatePrepass();
    TestFalse(TEXT("held rotation port disables after Current teardown"), RotationInput->IsEnabled());
    TestFalse(TEXT("held extent port disables after Current teardown"), ExtentInput->IsEnabled());
    ValidAuthoredA->Commit_Rotation(0);
    ValidAuthoredA->Commit_ViewExtent(900.0f);
    TestFalse(TEXT("held edit ports cannot enqueue after Current teardown"),
        EntityA.Has<ck::FFragment_Minimap_Requests>());
    TestTrue(TEXT("Params-only teardown leaves the entity live but removes complete Minimap inspectability"),
        EntityB.Try_Remove<ck::FFragment_Minimap_Params>()
            && ck::IsValid(EntityB) && EntityB.Has<ck::FFragment_Minimap_Current>()
            && NOT ValidInspector.CanInspect(EntityB));

    ValidInspector.OnDeactivated();
    TestTrue(TEXT("Minimap deactivation releases all valid authored views and the active edit scope"),
        ValidAuthoredA->Is_Inert() && ValidAuthoredB->Is_Inert()
            && DiffAuthored->Is_Inert() && Hidden->Is_Inert() && Hover->Is_Inert()
            && NOT ValidAuthoredA->Get_View().IsValid() && NOT ValidAuthoredB->Get_View().IsValid()
            && NOT EditGuard->Get_HasActiveEdit());
    ValidAuthoredA->Commit_Rotation(1);
    ValidAuthoredA->Commit_ViewExtent(1200.0f);
    TestFalse(TEXT("held authored Minimap controls remain inert after inspector release"),
        EntityA.Has<ck::FFragment_Minimap_Requests>());

    Inspector.OnDeactivated();
    TestTrue(TEXT("deactivation releases every authored view and edit scope"),
        AuthoredA->Is_Inert() && AuthoredB->Is_Inert() && NOT AuthoredA->Is_Mounted() && NOT AuthoredB->Is_Mounted());
    return true;
}

#endif
