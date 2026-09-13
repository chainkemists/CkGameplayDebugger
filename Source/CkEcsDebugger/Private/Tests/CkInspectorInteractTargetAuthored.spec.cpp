#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEcsDebugger/Inspectors/CkInspector_InteractTarget.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Switch.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Net/CkNet_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkInteraction/InteractTarget/CkInteractTarget_Fragment.h"
#include "CkInteraction/InteractTarget/CkInteractTarget_Utils.h"
#include "CkInteraction/Interaction/CkInteraction_Fragment.h"
#include "CkInteraction/Interaction/CkInteraction_Utils.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiStyledButton.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SWindow.h"

namespace ck_inspector_interact_target_authored_test
{
    auto FindSwitch(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SCkDebug_Switch>
    {
        if (InRoot->GetTypeAsString() == TEXT("SCkDebug_Switch") && InRoot->GetTag() == InTag)
        { return StaticCastSharedRef<SCkDebug_Switch>(InRoot); }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const auto Found = FindSwitch(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag); Found.IsValid())
            { return Found; }
        }
        return nullptr;
    }

    auto FindSwitch(const TSharedRef<SWidget>& InRoot) -> TSharedPtr<SCkDebug_Switch>
    { return FindSwitch(InRoot, TEXT("interact-target-enabled-switch")); }

    auto FindButton(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SButton>
    {
        const FString Type = InRoot->GetTypeAsString();
        if ((Type == TEXT("SButton") || Type == TEXT("SCkUiStyledButton")) && InRoot->GetTag() == InTag)
        { return StaticCastSharedRef<SButton>(InRoot); }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const auto Found = FindButton(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag); Found.IsValid())
            { return Found; }
        }
        return nullptr;
    }

    auto CollectEntityRefs(const TSharedRef<SWidget>& InRoot, TArray<TSharedRef<SCkDebug_EntityRef>>& OutRefs) -> void
    {
        if (InRoot->GetTypeAsString() == TEXT("SCkDebug_EntityRef"))
        { OutRefs.Add(StaticCastSharedRef<SCkDebug_EntityRef>(InRoot)); }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        { CollectEntityRefs(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), OutRefs); }
    }

    auto TickAuthored(const TSharedRef<SCkInspector_InteractTargetAuthored>& InAuthored) -> void
    { InAuthored->Tick(FGeometry::MakeRoot(FVector2D{1.0f, 1.0f}, FSlateLayoutTransform{}), 0.0, 0.0f); }

    auto TickSlate(FSlateApplication& InSlate) -> void
    { InSlate.PumpMessages(); InSlate.Tick(); InSlate.Tick(); }

    struct FWindowScope final
    {
        explicit FWindowScope(FSlateApplication& InSlate) : Slate(InSlate) {}
        ~FWindowScope()
        {
            if (Window.IsValid()) { Slate.DestroyWindowImmediately(Window.ToSharedRef()); }
        }

        FSlateApplication& Slate;
        TSharedPtr<SWindow> Window;
    };

    auto AddTarget(FCk_Handle& InOwner, const FGameplayTag InChannel,
        const ECk_Interaction_CompletionPolicy InCompletion, const double InSeconds) -> FCk_Handle_InteractTarget
    {
        auto Params = FCk_Fragment_InteractTarget_ParamsData{InChannel};
        Params.Set_CompletionPolicy(InCompletion);
        Params.Set_InteractionDuration(FCk_Time{InSeconds});
        return UCk_Utils_InteractTarget_UE::Add(InOwner, Params, ECk_Replication::DoesNotReplicate);
    }

    auto AddAuthorityAndWorld(FCk_Handle& InEntity) -> bool
    {
        if (GWorld == nullptr) { return false; }
        InEntity.Add<TWeakObjectPtr<UWorld>>(GWorld);
        UCk_Utils_Net_UE::Add(InEntity, FCk_Net_ConnectionSettings{
            ECk_Replication::DoesNotReplicate, ECk_Net_NetModeType::Host, ECk_Net_EntityNetRole::Authority});
        return true;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorInteractTargetAuthored,
    "Ck.UiAuthoring.EcsDebugger.InteractTargetInspector.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkInspectorInteractTargetAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_inspector_interact_target_authored_test;

    const FGameplayTag ChannelA = FGameplayTag::RequestGameplayTag(
        FName{TEXT("InteractionChannel.InteractionGym.Default")}, false);
    const FGameplayTag ChannelB = FGameplayTag::RequestGameplayTag(
        FName{TEXT("InteractionChannel.InteractionGym.Secondary")}, false);
    if (NOT TestTrue(TEXT("fixture has two registered interaction channels"),
        ChannelA.IsValid() && ChannelB.IsValid() && ChannelA != ChannelB)) { return false; }

    auto World = ck::FEcsWorld{};
    auto OwnerA = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(World.Get_Registry());
    auto OwnerB = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(World.Get_Registry());
    if (NOT TestTrue(TEXT("fixture roots carry production-equivalent world and authority state"),
        AddAuthorityAndWorld(OwnerA) && AddAuthorityAndWorld(OwnerB))) { return false; }
    auto TargetA = AddTarget(OwnerA, ChannelA, ECk_Interaction_CompletionPolicy::Timed, 3.5);
    auto TargetB = AddTarget(OwnerB, ChannelB, ECk_Interaction_CompletionPolicy::ManuallyCompleted, 9.0);
    if (NOT TestTrue(TEXT("public APIs create independent owners and complete targets"),
        ck::IsValid(OwnerA) && ck::IsValid(OwnerB) && ck::IsValid(TargetA) && ck::IsValid(TargetB)
            && OwnerA.Has<ck::FFragment_RecordOfInteractTargets>()
            && OwnerB.Has<ck::FFragment_RecordOfInteractTargets>())) { return false; }

    const auto Selection = MakeShared<FCkDebuggerModel_EntitySelection>();
    auto Inspector = FCkInspector_InteractTarget{};
    Inspector.Set_SelectionModel(Selection);
    const auto SectionsA = Inspector.Get_InspectorSections(OwnerA);
    const auto SectionsB = Inspector.Get_InspectorSections(OwnerB);
    if (NOT TestTrue(TEXT("each owner mounts one authored target section"),
        SectionsA.Num() == 1 && SectionsB.Num() == 1
            && SectionsA[0].Widget->GetTypeAsString() == TEXT("SCkInspector_InteractTargetAuthored")
            && SectionsB[0].Widget->GetTypeAsString() == TEXT("SCkInspector_InteractTargetAuthored")))
    { AddError(Inspector.Get_LastAuthoredLoadError()); return false; }
    const auto AuthoredA = StaticCastSharedRef<SCkInspector_InteractTargetAuthored>(SectionsA[0].Widget);
    const auto AuthoredB = StaticCastSharedRef<SCkInspector_InteractTargetAuthored>(SectionsB[0].Widget);
    const auto ViewA = AuthoredA->Get_View();
    const auto ViewB = AuthoredB->Get_View();
    TestTrue(TEXT("authored target views and collections are independently owned"),
        AuthoredA->Is_Mounted() && AuthoredB->Is_Mounted() && ViewA.IsValid() && ViewB.IsValid()
            && ViewA != ViewB && AuthoredA->Get_Interactions().IsValid() && AuthoredB->Get_Interactions().IsValid()
            && AuthoredA->Get_Interactions() != AuthoredB->Get_Interactions());
    TestTrue(TEXT("live target projection preserves timed and non-timed semantics"),
        AuthoredA->Get_ChannelText() == ChannelA.ToString() && AuthoredB->Get_ChannelText() == ChannelB.ToString()
            && AuthoredA->Get_EnabledText() == TEXT("Enable") && AuthoredA->Get_IsEnabled()
            && AuthoredA->Get_CompletionText().Contains(TEXT("Timed"))
            && AuthoredA->Get_DurationText().Contains(TEXT("3.5"))
            && AuthoredB->Get_DurationText() == TEXT("N/A"));

    FSlateApplication& Slate = FSlateApplication::Get();
    FWindowScope WindowScope{Slate};
    WindowScope.Window = SNew(SWindow)
        .AutoCenter(EAutoCenter::None)
        .ClientSize(FVector2D{520.0f, 260.0f})
        .CreateTitleBar(false)
        .HasCloseButton(false)
        [AuthoredA];
    Slate.AddWindow(WindowScope.Window.ToSharedRef(), true);
    TickSlate(Slate);

    auto Rows = TMap<FString, FString>{};
    {
        const FCkInspector_RowCaptureScope Capture;
        Inspector.Get_InspectorSections(OwnerA);
        Rows = Capture.Get_Rows();
    }
    TestTrue(TEXT("native capture remains complete fallback and comparison authority"),
        Rows.Contains(TEXT("Channel:")) && Rows.Contains(TEXT("Enabled:"))
            && Rows.Contains(TEXT("Set Enabled:")) && Rows.Contains(TEXT("Interactions:"))
            && Rows.Contains(TEXT("Completion:")) && Rows.Contains(TEXT("Duration:"))
            && Rows.Contains(TEXT("Concurrent:")));

    ViewA->GetRegion(TEXT("main"))->SlatePrepass();
    const auto HeldSwitch = FindSwitch(ViewA->GetRegion(TEXT("main")));
    const auto HeldCancel = FindButton(ViewA->GetRegion(TEXT("main")), TEXT("interact-target-cancel-all"));
    if (NOT TestTrue(TEXT("authored section mounts physical toggle and cancel action"),
        HeldSwitch.IsValid() && HeldCancel.IsValid() && HeldSwitch->IsEnabled() && HeldCancel->IsEnabled())) { return false; }
    const FGeometry Geometry = FGeometry::MakeRoot(FVector2D{30.0f, 17.0f}, FSlateLayoutTransform{});
    const FPointerEvent Click{0, FVector2D{5.0f, 5.0f}, FVector2D{5.0f, 5.0f},
        TSet<FKey>{EKeys::LeftMouseButton}, EKeys::LeftMouseButton, 0.0f, FModifierKeysState{}};
    HeldSwitch->OnMouseButtonDown(Geometry, Click);
    TestTrue(TEXT("physical toggle immediately mutates only its exact target"),
        UCk_Utils_InteractTarget_UE::Get_Enabled(TargetA) == ECk_EnableDisable::Disable
            && UCk_Utils_InteractTarget_UE::Get_Enabled(TargetB) == ECk_EnableDisable::Enable
            && AuthoredA->Get_EnabledText() == TEXT("Disable") && NOT AuthoredA->Get_IsEnabled());
    HeldSwitch->OnMouseButtonDown(Geometry, Click);
    TestTrue(TEXT("physical toggle restores enabled state synchronously"),
        UCk_Utils_InteractTarget_UE::Get_Enabled(TargetA) == ECk_EnableDisable::Enable);

    auto Source = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(OwnerA);
    auto Interaction = UCk_Utils_Interaction_UE::Add(TargetA, FCk_Fragment_Interaction_ParamsData{
        ChannelA, Source, Source, TargetA, ECk_Interaction_CompletionPolicy::ManuallyCompleted, FCk_Time{0.0}});
    TickSlate(Slate);
    if (NOT TestTrue(TEXT("current interaction is projected as one stable authored entity reference"),
        ck::IsValid(Interaction) && AuthoredA->Get_Interactions()->GetRecords().Num() == 1)) { return false; }
    auto EntityRefs = TArray<TSharedRef<SCkDebug_EntityRef>>{};
    CollectEntityRefs(ViewA->GetRegion(TEXT("main")), EntityRefs);
    if (NOT TestTrue(TEXT("interaction repeat mounts a physical entity reference"), EntityRefs.Num() == 1)) { return false; }
    EntityRefs[0]->OnMouseButtonDown(Geometry, Click);
    TestTrue(TEXT("physical interaction reference routes through the selection model"),
        Selection->Get_SelectedEntities().Num() == 1 && Selection->Get_PrimarySelection() == Interaction);
    HeldCancel->SimulateClick();
    TestTrue(TEXT("physical Cancel All queues exactly one request against the selected target"),
        TargetA.Has<ck::FFragment_InteractTarget_Requests>()
            && TargetA.Get<ck::FFragment_InteractTarget_Requests>().Get_Requests().Num() == 1
            && NOT TargetB.Has<ck::FFragment_InteractTarget_Requests>());

    const TSharedPtr<const FCkUiRecord> StableInteraction = AuthoredA->Get_Interactions()->GetRecords()[0];
    TickAuthored(AuthoredA);
    TestTrue(TEXT("unchanged interaction identity retains its authored repeat record"),
        AuthoredA->Get_Interactions()->GetRecords()[0] == StableInteraction);

    UCk_Utils_Interaction_UE::RecordOfInteractions_Utils::Request_Disconnect(TargetA, Interaction);
    auto ReplacementInteraction = UCk_Utils_Interaction_UE::Add(TargetA, FCk_Fragment_Interaction_ParamsData{
        ChannelA, Source, Source, TargetA, ECk_Interaction_CompletionPolicy::ManuallyCompleted, FCk_Time{0.0}});
    TickSlate(Slate);
    auto ReplacementRefs = TArray<TSharedRef<SCkDebug_EntityRef>>{};
    CollectEntityRefs(ViewA->GetRegion(TEXT("main")), ReplacementRefs);
    Selection->Clear_Selection();
    EntityRefs[0]->OnMouseButtonDown(Geometry, Click);
    TestTrue(TEXT("equal-count interaction replacement retires the old record and held entity reference"),
        ck::IsValid(ReplacementInteraction) && AuthoredA->Get_Interactions()->GetRecords().Num() == 1
            && AuthoredA->Get_Interactions()->GetRecords()[0] != StableInteraction
            && ReplacementRefs.Num() == 1 && ReplacementRefs[0] != EntityRefs[0]
            && Selection->Get_SelectedEntities().IsEmpty());
    ReplacementRefs[0]->OnMouseButtonDown(Geometry, Click);
    TestTrue(TEXT("replacement interaction reference navigates only the new stable handle"),
        Selection->Get_SelectedEntities().Num() == 1
            && Selection->Get_PrimarySelection() == ReplacementInteraction);

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup, Stylesheet;
    const FString Root = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("installed InteractTarget resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(Root, TEXT("EcsInspectorInteractTarget.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(Root, TEXT("EcsInspectorInteractTarget.ui.css"))))) { return false; }
    TestTrue(TEXT("HTML owns every target row, action, toggle, and current-interaction repeat"),
        Markup.Contains(TEXT(">Channel:</text>")) && Markup.Contains(TEXT(">Enabled:</text>"))
            && Markup.Contains(TEXT(">Set Enabled:</text>")) && Markup.Contains(TEXT(">Completion:</text>"))
            && Markup.Contains(TEXT(">Duration:</text>")) && Markup.Contains(TEXT(">Concurrent:</text>"))
            && Markup.Contains(TEXT("<debug-switch")) && Markup.Contains(TEXT("<debug-inspector-action"))
            && Markup.Contains(TEXT("<debug-entity-ref")) && Markup.Contains(TEXT("<repeat")));
    const int64 RevisionA = ViewA->GetRevision();
    TestTrue(TEXT("compatible reload retains physical target action identity"),
        ViewA->TryReload(Markup, Stylesheet, TEXT("InteractTarget compatible candidate")).Succeeded
            && ViewA->GetRevision() > RevisionA
            && FindButton(ViewA->GetRegion(TEXT("main")), TEXT("interact-target-cancel-all")) == HeldCancel
            && FindSwitch(ViewA->GetRegion(TEXT("main"))) == HeldSwitch);
    const int64 RevisionB = ViewB->GetRevision();
    const TSharedRef<SWidget> MainB = ViewB->GetRegion(TEXT("main"));
    TestTrue(TEXT("missing toggle event binding is rejected atomically"),
        NOT ViewB->TryReload(Markup.Replace(TEXT("changed=\"interact-target-enabled-changed\""),
                TEXT("changed=\"interact-target-missing\"")), Stylesheet, TEXT("InteractTarget rejected candidate")).Succeeded
            && ViewB->GetRevision() == RevisionB && &ViewB->GetRegion(TEXT("main")).Get() == &MainB.Get());

    UCk_Utils_InteractTarget_UE::RecordOfInteractTargets_Utils::Request_Disconnect(OwnerA, TargetA);
    auto Replacement = AddTarget(OwnerA, ChannelA, ECk_Interaction_CompletionPolicy::ManuallyCompleted, 0.0);
    Inspector.Tick(OwnerA, 0.0f);
    HeldSwitch->SlatePrepass();
    HeldCancel->SlatePrepass();
    const int32 OldRequests = TargetA.Get<ck::FFragment_InteractTarget_Requests>().Get_Requests().Num();
    HeldCancel->SimulateClick(); HeldSwitch->OnMouseButtonDown(Geometry, Click);
    TestTrue(TEXT("equal-count target replacement requests a keyed rebuild and makes held controls inert"),
        ck::IsValid(Replacement) && Inspector.NeedsRebuild() && NOT AuthoredA->Get_IsAvailable()
            && NOT HeldCancel->IsEnabled() && NOT HeldSwitch->IsEnabled()
            && TargetA.Get<ck::FFragment_InteractTarget_Requests>().Get_Requests().Num() == OldRequests
            && UCk_Utils_InteractTarget_UE::Get_Enabled(Replacement) == ECk_EnableDisable::Enable);

    TSharedPtr<SCkInspector_InteractTargetAuthored> DestructorAuthored;
    {
        auto DestructorInspector = MakeUnique<FCkInspector_InteractTarget>();
        const auto DestructorSections = DestructorInspector->Get_InspectorSections(OwnerB);
        DestructorAuthored = StaticCastSharedRef<SCkInspector_InteractTargetAuthored>(DestructorSections[0].Widget);
    }
    TestTrue(TEXT("inspector destruction releases a retained authored target view"),
        DestructorAuthored.IsValid() && DestructorAuthored->Is_Inert() && NOT DestructorAuthored->Is_Mounted());

    TestTrue(TEXT("target fragment removal preserves the child entity while invalidating complete composition"),
        TargetB.Try_Remove<ck::FFragment_InteractTarget_Current>() && ck::IsValid(TargetB));
    TickAuthored(AuthoredB);
    TestTrue(TEXT("partial target composition fails all authored projection and interaction state closed"),
        NOT AuthoredB->Get_IsAvailable() && NOT AuthoredB->Get_CanRequest()
            && AuthoredB->Get_ChannelText() == TEXT("--") && AuthoredB->Get_EnabledText() == TEXT("--")
            && AuthoredB->Get_CompletionText() == TEXT("--") && AuthoredB->Get_DurationText() == TEXT("--")
            && AuthoredB->Get_ConcurrentText() == TEXT("--") && AuthoredB->Get_Interactions()->GetRecords().IsEmpty());

    Inspector.OnDeactivated();
    TestTrue(TEXT("deactivation releases every authored target view and collection"),
        AuthoredA->Is_Inert() && AuthoredB->Is_Inert() && NOT AuthoredA->Is_Mounted() && NOT AuthoredB->Is_Mounted()
            && NOT AuthoredA->Get_View().IsValid() && NOT AuthoredB->Get_View().IsValid()
            && NOT AuthoredA->Get_Interactions().IsValid() && NOT AuthoredB->Get_Interactions().IsValid());
    HeldCancel->SimulateClick(); HeldSwitch->OnMouseButtonDown(Geometry, Click);
    TestTrue(TEXT("retained physical controls remain inert after deactivation"),
        TargetA.Get<ck::FFragment_InteractTarget_Requests>().Get_Requests().Num() == OldRequests);
    return true;
}

#endif
