#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEcsDebugger/Inspectors/CkInspector_IskmProxy.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_Switch.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Net/CkNet_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkIskmRenderer/Proxy/CkIskmProxy_Fragment.h"
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
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"

#include <variant>

namespace ck_inspector_iskm_proxy_authored_test
{
    auto CreateCompleteProxy(const FCk_Handle& InOwner) -> FCk_Handle
    {
        FCk_Handle Entity = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InOwner);
        if (ck::Is_NOT_Valid(Entity)) { return {}; }
        Entity.Add<ck::FFragment_IskmProxy_Params>(FCk_Fragment_IskmProxy_ParamsData{});
        Entity.Add<ck::FFragment_IskmProxy_Current>();
        Entity.Add<ck::FFragment_IskmProxy_AnimState>();
        Entity.Add<ck::FFragment_IskmProxy_PoseSource>();
        Entity.Add<ck::FFragment_IskmProxy_CustomData>();
        Entity.Add<ck::FFragment_IskmProxy_MaterialOverrides>();
        Entity.Add<ck::FFragment_IskmProxy_MorphTargets>();
        Entity.Add<ck::FFragment_IskmProxy_Requests>();
        return Entity;
    }

    auto FindTagged(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SWidget>
    {
        if (InRoot->GetTag() == InTag) { return InRoot; }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SWidget> Found = FindTagged(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag); Found.IsValid())
            { return Found; }
        }
        return nullptr;
    }

    auto FindButton(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SButton>
    {
        if (InRoot->GetTypeAsString() == TEXT("SButton") && InRoot->GetTag() == InTag)
        { return StaticCastSharedRef<SButton>(InRoot); }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SButton> Found = FindButton(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag); Found.IsValid())
            { return Found; }
        }
        return nullptr;
    }

    auto FindSwitch(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SCkDebug_Switch>
    {
        if (InRoot->GetTypeAsString() == TEXT("SCkDebug_Switch") && InRoot->GetTag() == InTag)
        { return StaticCastSharedRef<SCkDebug_Switch>(InRoot); }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SCkDebug_Switch> Found = FindSwitch(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag); Found.IsValid())
            { return Found; }
        }
        return nullptr;
    }

    auto FindEditor(const TSharedRef<SWidget>& InRoot) -> TSharedPtr<SEditableTextBox>
    {
        if (InRoot->GetTypeAsString() == TEXT("SEditableTextBox")
            || InRoot->GetTypeAsString() == TEXT("SCkUiTextInputBox"))
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

    auto FindEditorUnderTag(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SEditableTextBox>
    {
        const TSharedPtr<SWidget> Tagged = FindTagged(InRoot, InTag);
        return Tagged.IsValid() ? FindEditor(Tagged.ToSharedRef()) : nullptr;
    }

    auto MakeClick() -> FPointerEvent
    {
        return FPointerEvent{
            0, FVector2D{5.0f, 5.0f}, FVector2D{5.0f, 5.0f}, TSet<FKey>{EKeys::LeftMouseButton},
            EKeys::LeftMouseButton, 0.0f, FModifierKeysState{}};
    }

    auto Tick(FSlateApplication& InSlate) -> void
    {
        InSlate.PumpMessages();
        InSlate.Tick();
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorIskmProxyAuthored,
    "Ck.UiAuthoring.EcsDebugger.IskmProxyInspector.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkInspectorIskmProxyAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_inspector_iskm_proxy_authored_test;

    if (NOT FSlateApplication::IsInitialized())
    { AddError(TEXT("ISKM Proxy authored inspector test requires Slate.")); return false; }

    auto InvalidInspector = FCkInspector_IskmProxy{};
    const TSharedRef<SWidget> InvalidRendered = InvalidInspector.Build_Inspector(FCk_Handle{});
    if (NOT TestEqual(TEXT("default-invalid build mounts the authored ISKM Proxy shell safely"),
        InvalidRendered->GetTypeAsString(), FString{TEXT("SCkInspector_IskmProxyAuthored")}))
    { AddError(InvalidInspector.Get_LastAuthoredLoadError()); return false; }
    const TSharedRef<SCkInspector_IskmProxyAuthored> InvalidAuthored =
        StaticCastSharedRef<SCkInspector_IskmProxyAuthored>(InvalidRendered);
    TestTrue(TEXT("default-invalid ISKM Proxy shell fails closed before fragment reads"),
        InvalidAuthored->Is_Mounted() && NOT InvalidAuthored->Get_IsAvailable()
            && NOT InvalidAuthored->Get_CanRequest() && InvalidAuthored->Get_PoseSourceText() == TEXT("--")
            && InvalidAuthored->Get_PlayingAnimationText() == TEXT("--")
            && InvalidAuthored->Get_CustomDataSlotZeroText() == TEXT("--"));
    InvalidAuthored->Set_VisibilityIntent(false);
    InvalidAuthored->Commit_PlayRate(2.0f);
    InvalidAuthored->Commit_MorphName(TEXT("Ignored"));
    InvalidAuthored->Request_StopAnimation();
    TestTrue(TEXT("default-invalid direct bindings remain inert"),
        InvalidAuthored->Get_VisibilityIntent()
            && FMath::IsNearlyEqual(InvalidAuthored->Get_PlayRateIntent(), 1.0f)
            && InvalidAuthored->Get_MorphNameText() == TEXT("(none)"));

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup;
    FString Stylesheet;
    const FString ResourceRoot = Plugin.IsValid()
        ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("installed ISKM Proxy resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(
            Markup, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorIskmProxy.ui.html")))
        && FFileHelper::LoadFileToString(
            Stylesheet, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorIskmProxy.ui.css")))))
    { return false; }
    TestTrue(TEXT("resource authors status, meter, guarded actions, and every editor"),
        Markup.Contains(TEXT("<debug-status id=\"iskm-proxy-pose-status\""))
            && Markup.Contains(TEXT("<debug-status id=\"iskm-proxy-ragdoll-status\""))
            && Markup.Contains(TEXT("fraction-bind=\"iskm-proxy-time-fraction\""))
            && Markup.Contains(TEXT("iskm-proxy-visible-switch"))
            && Markup.Contains(TEXT("iskm-proxy-play-rate-input"))
            && Markup.Contains(TEXT("iskm-proxy-morph-input"))
            && Markup.Contains(TEXT("iskm-proxy-weight-input"))
            && Markup.Contains(TEXT("iskm-proxy-custom-slot-input"))
            && Markup.Contains(TEXT("iskm-proxy-custom-value-input"))
            && Markup.Contains(TEXT("enabled-bind=\"iskm-proxy-can-end-ragdoll\"")));

    TSharedPtr<FCkUiView> InvalidView = InvalidAuthored->Get_View();
    if (NOT InvalidView.IsValid()) { return false; }
    TSharedPtr<SWidget> InvalidMainBefore = InvalidView->GetRegion(TEXT("main"));
    const int64 InitialRevision = InvalidView->GetRevision();
    TestTrue(TEXT("compatible ISKM Proxy reload preserves the authored tree"),
        InvalidView->TryReload(Markup, Stylesheet, TEXT("ISKM Proxy compatible candidate")).Succeeded
            && InvalidView->GetRevision() > InitialRevision
            && InvalidView->GetRegion(TEXT("main")) == InvalidMainBefore);
    const int64 AcceptedRevision = InvalidView->GetRevision();
    const FString MissingPoseBinding = Markup.Replace(
        TEXT("label-bind=\"iskm-proxy-pose\""), TEXT("label-bind=\"iskm-proxy-missing\""));
    TestFalse(TEXT("missing status binding is rejected atomically"),
        InvalidView->TryReload(
            MissingPoseBinding, Stylesheet, TEXT("ISKM Proxy rejected candidate")).Succeeded);
    TestTrue(TEXT("rejected reload retains the accepted tree and revision"),
        InvalidView->GetRegion(TEXT("main")) == InvalidMainBefore
            && InvalidView->GetRevision() == AcceptedRevision);
    const TWeakPtr<FCkUiView> ReleasedInvalidView = InvalidView;
    InvalidInspector.OnDeactivated();
    InvalidMainBefore.Reset();
    InvalidView.Reset();
    TestTrue(TEXT("default-invalid authored view releases on deactivation"),
        InvalidAuthored->Is_Inert() && NOT InvalidAuthored->Is_Mounted()
            && NOT ReleasedInvalidView.IsValid());

    auto EcsWorld = ck::FEcsWorld{};
    auto LifetimeOwner = UCk_Utils_EntityLifetime_UE::Get_TransientEntity(EcsWorld.Get_Registry());
    UWorld* const AuthorityWorld = UWorld::CreateWorld(EWorldType::Game, false);
    ON_SCOPE_EXIT
    {
        if (AuthorityWorld != nullptr)
        { AuthorityWorld->DestroyWorld(false); }
    };
    if (NOT TestTrue(TEXT("fixture has a transient owner and standalone authority world"),
        ck::IsValid(LifetimeOwner) && AuthorityWorld != nullptr
            && AuthorityWorld->GetNetMode() == NM_Standalone))
    { return false; }
    LifetimeOwner.Add<TWeakObjectPtr<UWorld>>(AuthorityWorld);
    UCk_Utils_Net_UE::Add(LifetimeOwner, FCk_Net_ConnectionSettings{
        ECk_Replication::DoesNotReplicate,
        ECk_Net_NetModeType::Host,
        ECk_Net_EntityNetRole::Authority});

    auto ProxyA = CreateCompleteProxy(LifetimeOwner);
    auto ProxyB = CreateCompleteProxy(LifetimeOwner);
    if (NOT TestTrue(TEXT("fixture creates two independent complete ISKM Proxy compositions"),
        ck::IsValid(ProxyA) && ck::IsValid(ProxyB) && ProxyA != ProxyB
            && ProxyA.Has_All<ck::FFragment_IskmProxy_Params, ck::FFragment_IskmProxy_Current,
                ck::FFragment_IskmProxy_AnimState, ck::FFragment_IskmProxy_PoseSource,
                ck::FFragment_IskmProxy_CustomData, ck::FFragment_IskmProxy_MaterialOverrides,
                ck::FFragment_IskmProxy_MorphTargets, ck::FFragment_IskmProxy_Requests>()))
    { return false; }

    auto Inspector = FCkInspector_IskmProxy{};
    const TSharedRef<SWidget> RenderedA = Inspector.Build_Inspector(ProxyA);
    const TSharedRef<SWidget> RenderedB = Inspector.Build_Inspector(ProxyB);
    if (NOT TestEqual(TEXT("proxy A mounts authored composition"), RenderedA->GetTypeAsString(),
            FString{TEXT("SCkInspector_IskmProxyAuthored")})
        || NOT TestEqual(TEXT("proxy B mounts authored composition"), RenderedB->GetTypeAsString(),
            FString{TEXT("SCkInspector_IskmProxyAuthored")}))
    { AddError(Inspector.Get_LastAuthoredLoadError()); return false; }
    const TSharedRef<SCkInspector_IskmProxyAuthored> AuthoredA =
        StaticCastSharedRef<SCkInspector_IskmProxyAuthored>(RenderedA);
    const TSharedRef<SCkInspector_IskmProxyAuthored> AuthoredB =
        StaticCastSharedRef<SCkInspector_IskmProxyAuthored>(RenderedB);
    TestTrue(TEXT("complete proxies own independent views and exact default projection"),
        AuthoredA->Get_View().IsValid() && AuthoredB->Get_View().IsValid()
            && AuthoredA->Get_View() != AuthoredB->Get_View()
            && AuthoredA->Get_IsAvailable() && AuthoredB->Get_IsAvailable()
            && AuthoredA->Get_CanRequest() && AuthoredB->Get_CanRequest()
            && AuthoredA->Get_PoseSourceText() == TEXT("Sequence")
            && AuthoredA->Get_PlayingAnimationText() == TEXT("None")
            && AuthoredA->Get_PlayTimeText() == TEXT("0.00 / 0.00s")
            && FMath::IsNearlyZero(AuthoredA->Get_PlayTimeFraction())
            && AuthoredA->Get_AnimInstanceText() == TEXT("(none — Sequence mode)")
            && AuthoredA->Get_ActiveMontageText() == TEXT("None")
            && AuthoredA->Get_RagdollingText() == TEXT("No")
            && AuthoredA->Get_SubmeshesText() == TEXT("0")
            && AuthoredA->Get_CustomDataSlotZeroText() == TEXT("0.000")
            && NOT AuthoredA->Get_CanEndRagdoll());

    AuthoredB->Commit_PlayRate(2.25f);
    TestTrue(TEXT("per-build request intent remains independent"),
        FMath::IsNearlyEqual(AuthoredA->Get_PlayRateIntent(), 1.0f)
            && FMath::IsNearlyEqual(AuthoredB->Get_PlayRateIntent(), 2.25f)
            && ProxyB.Get<ck::FFragment_IskmProxy_Requests>().Get_Requests().Num() == 1);
    ProxyB.Get<ck::FFragment_IskmProxy_Requests>()._Requests.Reset();

    auto NativeRows = TMap<FString, FString>{};
    {
        const auto Capture = FCkInspector_RowCaptureScope{};
        Inspector.Build_Inspector(ProxyA);
        NativeRows = Capture.Get_Rows();
    }
    const TSet<FString> DiffLabels{TEXT("Pose Source:"), TEXT("Play Rate:")};
    TSharedPtr<SCkInspector_IskmProxyAuthored> DiffAuthored;
    {
        const auto DiffScope = FCkInspector_DiffMarkScope{&DiffLabels};
        DiffAuthored = StaticCastSharedRef<SCkInspector_IskmProxyAuthored>(
            Inspector.Build_Inspector(ProxyA));
    }
    TestTrue(TEXT("native capture and authored diff labels preserve the stable inspector contract"),
        NativeRows.Num() == 15 && NativeRows.FindRef(TEXT("Pose Source:")) == TEXT("Sequence")
            && NativeRows.FindRef(TEXT("Playing Animation:")) == TEXT("None")
            && NativeRows.FindRef(TEXT("Play Time / Length:")) == TEXT("0.00 / 0.00s")
            && NativeRows.FindRef(TEXT("AnimInstance Class:")) == TEXT("(none — Sequence mode)")
            && NativeRows.FindRef(TEXT("Ragdolling:")) == TEXT("No")
            && NativeRows.FindRef(TEXT("Attached Submeshes:")) == TEXT("0")
            && NativeRows.FindRef(TEXT("Custom Data Slot 0:")) == TEXT("0.000")
            && DiffAuthored.IsValid() && DiffAuthored->Is_DiffMarked(TEXT("Pose Source:"))
            && DiffAuthored->Is_DiffMarked(TEXT("Play Rate:"))
            && NOT DiffAuthored->Is_DiffMarked(TEXT("Ragdolling:")));

    const TSharedPtr<SCkDebug_Switch> VisibleSwitch =
        FindSwitch(RenderedA, TEXT("iskm-proxy-visible-switch"));
    const TSharedPtr<SButton> StopButton =
        FindButton(RenderedA, TEXT("iskm-proxy-stop-animation"));
    const TSharedPtr<SButton> EndRagdollButton =
        FindButton(RenderedA, TEXT("iskm-proxy-end-ragdoll"));
    const TSharedPtr<SButton> ClearMorphsButton =
        FindButton(RenderedA, TEXT("iskm-proxy-clear-morphs"));
    const TSharedPtr<SButton> ClearMaterialsButton =
        FindButton(RenderedA, TEXT("iskm-proxy-clear-materials"));
    const TSharedPtr<SButton> DetachSubmeshesButton =
        FindButton(RenderedA, TEXT("iskm-proxy-detach-submeshes"));
    const TSharedPtr<SEditableTextBox> PlayRateEditor =
        FindEditorUnderTag(RenderedA, TEXT("iskm-proxy-play-rate-input"));
    const TSharedPtr<SEditableTextBox> MorphNameEditor =
        FindEditorUnderTag(RenderedA, TEXT("iskm-proxy-morph-input"));
    const TSharedPtr<SEditableTextBox> MorphWeightEditor =
        FindEditorUnderTag(RenderedA, TEXT("iskm-proxy-weight-input"));
    const TSharedPtr<SEditableTextBox> CustomSlotEditor =
        FindEditorUnderTag(RenderedA, TEXT("iskm-proxy-custom-slot-input"));
    const TSharedPtr<SEditableTextBox> CustomValueEditor =
        FindEditorUnderTag(RenderedA, TEXT("iskm-proxy-custom-value-input"));
    bool HasEveryPhysicalControl = true;
    HasEveryPhysicalControl &= TestTrue(TEXT("HTML owns the Visible switch"), VisibleSwitch.IsValid());
    HasEveryPhysicalControl &= TestTrue(TEXT("HTML owns the Stop Animation action"), StopButton.IsValid());
    HasEveryPhysicalControl &= TestTrue(TEXT("HTML owns the End Ragdoll action"), EndRagdollButton.IsValid());
    HasEveryPhysicalControl &= TestTrue(TEXT("HTML owns the Clear Morphs action"), ClearMorphsButton.IsValid());
    HasEveryPhysicalControl &= TestTrue(TEXT("HTML owns the Clear Materials action"), ClearMaterialsButton.IsValid());
    HasEveryPhysicalControl &= TestTrue(TEXT("HTML owns the Detach Submeshes action"), DetachSubmeshesButton.IsValid());
    HasEveryPhysicalControl &= TestTrue(TEXT("HTML owns the Play Rate editor"), PlayRateEditor.IsValid());
    HasEveryPhysicalControl &= TestTrue(TEXT("HTML owns the Morph Target editor"), MorphNameEditor.IsValid());
    HasEveryPhysicalControl &= TestTrue(TEXT("HTML owns the Morph Weight editor"), MorphWeightEditor.IsValid());
    HasEveryPhysicalControl &= TestTrue(TEXT("HTML owns the Custom Data Slot editor"), CustomSlotEditor.IsValid());
    HasEveryPhysicalControl &= TestTrue(TEXT("HTML owns the Custom Data Value editor"), CustomValueEditor.IsValid());
    if (NOT HasEveryPhysicalControl)
    { return false; }

    FSlateApplication& Slate = FSlateApplication::Get();
    FWindowScope WindowScope{Slate};
    WindowScope.Window = SNew(SWindow).AutoCenter(EAutoCenter::None)
        .ClientSize(FVector2D{680.0f, 1100.0f}).CreateTitleBar(false).HasCloseButton(false)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[RenderedA]
            + SVerticalBox::Slot().AutoHeight()[RenderedB]
        ];
    Slate.AddWindow(WindowScope.Window.ToSharedRef(), true);
    Tick(Slate);
    TestTrue(TEXT("standalone authority enables safe controls and disables inactive End Ragdoll"),
        VisibleSwitch->IsEnabled() && StopButton->IsEnabled() && ClearMorphsButton->IsEnabled()
            && ClearMaterialsButton->IsEnabled() && DetachSubmeshesButton->IsEnabled()
            && PlayRateEditor->IsEnabled() && MorphNameEditor->IsEnabled()
            && MorphWeightEditor->IsEnabled() && CustomSlotEditor->IsEnabled()
            && CustomValueEditor->IsEnabled() && NOT EndRagdollButton->IsEnabled()
            && AuthoredA->Get_RequestDisabledReason() == TEXT("End Ragdoll requires an active ragdoll."));

    AuthoredA->Request_EndRagdoll();
    TestTrue(TEXT("inactive End Ragdoll direct action remains a no-op"),
        ProxyA.Get<ck::FFragment_IskmProxy_Requests>().Get_Requests().IsEmpty());
    VisibleSwitch->OnMouseButtonDown(
        FGeometry::MakeRoot(FVector2D{30.0f, 17.0f}, FSlateLayoutTransform{}), MakeClick());
    const bool bEditorsCommitted =
        SetAndCommit(Slate, PlayRateEditor.ToSharedRef(), TEXT("1.75"))
        && SetAndCommit(Slate, MorphNameEditor.ToSharedRef(), TEXT("Smile"))
        && SetAndCommit(Slate, MorphWeightEditor.ToSharedRef(), TEXT("0.5"))
        && SetAndCommit(Slate, CustomSlotEditor.ToSharedRef(), TEXT("2"))
        && SetAndCommit(Slate, CustomValueEditor.ToSharedRef(), TEXT("0.75"));
    StopButton->SimulateClick();
    ClearMorphsButton->SimulateClick();
    ClearMaterialsButton->SimulateClick();
    DetachSubmeshesButton->SimulateClick();
    if (NOT TestTrue(TEXT("physical edits commit and enqueue exactly eight public requests"),
        bEditorsCommitted
            && NOT AuthoredA->Get_VisibilityIntent()
            && FMath::IsNearlyEqual(AuthoredA->Get_PlayRateIntent(), 1.75f)
            && AuthoredA->Get_MorphNameText() == TEXT("Smile")
            && FMath::IsNearlyEqual(AuthoredA->Get_CustomDataSlot(), 2.0f)
            && ProxyA.Get<ck::FFragment_IskmProxy_Requests>().Get_Requests().Num() == 8))
    { return false; }

    const auto& Requests = ProxyA.Get<ck::FFragment_IskmProxy_Requests>().Get_Requests();
    const auto* Visibility = std::get_if<FCk_Request_IskmProxy_SetVisibility>(&Requests[0]);
    const auto* PlayRate = std::get_if<FCk_Request_IskmProxy_SetPlayRate>(&Requests[1]);
    const auto* Morph = std::get_if<FCk_Request_IskmProxy_SetMorphTarget>(&Requests[2]);
    const auto* Custom = std::get_if<FCk_Request_IskmProxy_SetCustomDataFloat>(&Requests[3]);
    TestTrue(TEXT("physical controls preserve exact request order and payloads"),
        Visibility != nullptr && NOT Visibility->Get_IsVisible()
            && PlayRate != nullptr && FMath::IsNearlyEqual(PlayRate->Get_Rate(), 1.75f)
            && Morph != nullptr && Morph->Get_MorphName() == FName{TEXT("Smile")}
            && FMath::IsNearlyEqual(Morph->Get_Value(), 0.5f)
            && Custom != nullptr && Custom->Get_Offset() == 2
            && FMath::IsNearlyEqual(Custom->Get_Value(), 0.75f)
            && std::holds_alternative<FCk_Request_IskmProxy_StopAnimation>(Requests[4])
            && std::holds_alternative<FCk_Request_IskmProxy_ClearMorphTargets>(Requests[5])
            && std::holds_alternative<FCk_Request_IskmProxy_ClearMaterialOverrides>(Requests[6])
            && std::holds_alternative<FCk_Request_IskmProxy_DetachAllSubmeshes>(Requests[7]));

    auto& RequestArray = ProxyA.Get<ck::FFragment_IskmProxy_Requests>()._Requests;
    RequestArray.Reset();
    ProxyA.Try_Remove<ck::FFragment_IskmProxy_MaterialOverrides>();
    Tick(Slate);
    const bool bIntentBeforeCompositionLoss = AuthoredA->Get_VisibilityIntent();
    VisibleSwitch->OnMouseButtonDown(
        FGeometry::MakeRoot(FVector2D{30.0f, 17.0f}, FSlateLayoutTransform{}), MakeClick());
    StopButton->SimulateClick();
    AuthoredA->Commit_PlayRate(3.0f);
    TestTrue(TEXT("composition loss disables retained controls and rejects stale dispatch"),
        NOT Inspector.CanInspect(ProxyA) && NOT AuthoredA->Get_IsAvailable()
            && NOT AuthoredA->Get_CanRequest() && AuthoredA->Get_VisibilityIntent() == bIntentBeforeCompositionLoss
            && FMath::IsNearlyEqual(AuthoredA->Get_PlayRateIntent(), 1.75f)
            && RequestArray.IsEmpty());

    ProxyA.Add<ck::FFragment_IskmProxy_MaterialOverrides>();
    ProxyA.Add<ck::FTag_DestroyEntity_Initiate>();
    Tick(Slate);
    VisibleSwitch->OnMouseButtonDown(
        FGeometry::MakeRoot(FVector2D{30.0f, 17.0f}, FSlateLayoutTransform{}), MakeClick());
    StopButton->SimulateClick();
    AuthoredA->Commit_CustomDataSlot(7.0f);
    AuthoredA->Commit_CustomDataValue(9.0f);
    TestTrue(TEXT("pending destruction rejects held controls without mutating local intent"),
        NOT Inspector.CanInspect(ProxyA) && NOT AuthoredA->Get_IsAvailable()
            && AuthoredA->Get_VisibilityIntent() == bIntentBeforeCompositionLoss
            && FMath::IsNearlyEqual(AuthoredA->Get_CustomDataSlot(), 2.0f)
            && RequestArray.IsEmpty());

    const TWeakPtr<FCkUiView> ReleasedA = AuthoredA->Get_View();
    const TWeakPtr<FCkUiView> ReleasedB = AuthoredB->Get_View();
    const TWeakPtr<FCkUiView> ReleasedDiff = DiffAuthored->Get_View();
    Inspector.OnDeactivated();
    StopButton->SimulateClick();
    AuthoredA->Commit_PlayRate(4.0f);
    TestTrue(TEXT("deactivation releases every retained view and held controls remain inert"),
        AuthoredA->Is_Inert() && AuthoredB->Is_Inert() && DiffAuthored->Is_Inert()
            && NOT AuthoredA->Is_Mounted() && NOT AuthoredB->Is_Mounted()
            && NOT DiffAuthored->Is_Mounted() && NOT ReleasedA.IsValid()
            && NOT ReleasedB.IsValid() && NOT ReleasedDiff.IsValid() && RequestArray.IsEmpty());
    return true;
}

#endif
