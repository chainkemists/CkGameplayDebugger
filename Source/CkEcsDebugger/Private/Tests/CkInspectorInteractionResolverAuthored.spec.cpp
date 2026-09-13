#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEcsDebugger/Inspectors/CkInspector_InteractionResolver.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Net/CkNet_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkInteraction/InteractTarget/CkInteractTarget_Utils.h"
#include "CkInteraction/InteractionResolver/CkInteractionResolver_Fragment.h"
#include "CkInteraction/InteractionResolver/CkInteractionResolver_Processor.h"
#include "CkInteraction/InteractionResolver/CkInteractionResolver_Utils.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiStyledButton.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"

namespace ck_inspector_interaction_resolver_authored_test
{
    auto FindButtons(const TSharedRef<SWidget>& InRoot, const FName InTag, TArray<TSharedPtr<SButton>>& OutButtons) -> void
    {
        const FString Type = InRoot->GetTypeAsString();
        if ((Type == TEXT("SButton") || Type == TEXT("SCkUiStyledButton")) && InRoot->GetTag() == InTag)
        { OutButtons.Add(StaticCastSharedRef<SButton>(InRoot)); }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        { FindButtons(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag, OutButtons); }
    }

    auto CollectEntityRefs(const TSharedRef<SWidget>& InRoot,
        TArray<TSharedRef<SCkDebug_EntityRef>>& OutRefs) -> void
    {
        if (InRoot->GetTypeAsString() == TEXT("SCkDebug_EntityRef"))
        { OutRefs.Add(StaticCastSharedRef<SCkDebug_EntityRef>(InRoot)); }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        { CollectEntityRefs(ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), OutRefs); }
    }

    auto TickAuthored(const TSharedRef<SCkInspector_InteractionResolverAuthored>& InAuthored) -> void
    { InAuthored->Tick(FGeometry::MakeRoot(FVector2D{1.0f, 1.0f}, FSlateLayoutTransform{}), 0.0, 0.0f); }

    auto TickSlate(FSlateApplication& InSlate) -> void
    { InSlate.PumpMessages(); InSlate.Tick(); InSlate.Tick(); }

    struct FWindowScope final
    {
        explicit FWindowScope(FSlateApplication& InSlate) : Slate(InSlate) {}
        ~FWindowScope() { if (Window.IsValid()) { Slate.DestroyWindowImmediately(Window.ToSharedRef()); } }
        FSlateApplication& Slate;
        TSharedPtr<SWindow> Window;
    };

    auto AddAuthorityAndWorld(FCk_Handle& InEntity) -> bool
    {
        if (GWorld == nullptr) { return false; }
        InEntity.Add<TWeakObjectPtr<UWorld>>(GWorld);
        UCk_Utils_Net_UE::Add(InEntity, FCk_Net_ConnectionSettings{
            ECk_Replication::DoesNotReplicate, ECk_Net_NetModeType::Host, ECk_Net_EntityNetRole::Authority});
        return true;
    }

    auto MakeParams(const FGameplayTag InIntent, const TArray<FGameplayTag>& InChannels, const int32 InMax)
        -> FCk_InteractionResolver_ParamsData
    {
        auto Mapping = FCk_InteractionResolver_IntentChannelMapping{InIntent, InChannels};
        Mapping.Set_DistanceSorting(ECk_InteractionResolver_DistanceSorting::Disabled);
        Mapping.Set_MaxConcurrentInteractions(InMax);
        return FCk_InteractionResolver_ParamsData{{Mapping}};
    }

    auto MakeTwoIntentParams(const FGameplayTag InIntentA, const FGameplayTag InIntentB,
        const FGameplayTag InChannel) -> FCk_InteractionResolver_ParamsData
    {
        auto MappingA = FCk_InteractionResolver_IntentChannelMapping{InIntentA, {InChannel}};
        auto MappingB = FCk_InteractionResolver_IntentChannelMapping{InIntentB, {InChannel}};
        MappingA.Set_DistanceSorting(ECk_InteractionResolver_DistanceSorting::Disabled);
        MappingB.Set_DistanceSorting(ECk_InteractionResolver_DistanceSorting::Disabled);
        return FCk_InteractionResolver_ParamsData{{MappingA, MappingB}};
    }

    auto AddTarget(FCk_Handle& InOwner, const FGameplayTag InChannel) -> FCk_Handle_InteractTarget
    {
        return UCk_Utils_InteractTarget_UE::Add(InOwner, FCk_Fragment_InteractTarget_ParamsData{InChannel},
            ECk_Replication::DoesNotReplicate);
    }

    auto PumpResolverRequests(ck::FEcsWorld& InWorld) -> void
    { ck::FProcessor_InteractionResolver_HandleRequests{InWorld.Get_Registry()}.DoTick(FCk_Time{0.0}); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorInteractionResolverAuthored,
    "Ck.UiAuthoring.EcsDebugger.InteractionResolverInspector.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkInspectorInteractionResolverAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_inspector_interaction_resolver_authored_test;

    const FGameplayTag Intent = FGameplayTag::RequestGameplayTag(
        FName{TEXT("InteractionIntent.InteractionGym.Use")}, false);
    const FGameplayTag IntentParent = FGameplayTag::RequestGameplayTag(
        FName{TEXT("InteractionIntent")}, false);
    const FGameplayTag ChannelA = FGameplayTag::RequestGameplayTag(
        FName{TEXT("InteractionChannel.InteractionGym.Default")}, false);
    const FGameplayTag ChannelB = FGameplayTag::RequestGameplayTag(
        FName{TEXT("InteractionChannel.InteractionGym.Secondary")}, false);
    if (NOT TestTrue(TEXT("fixture has registered intent and channel topology"),
        Intent.IsValid() && IntentParent.IsValid() && Intent != IntentParent
            && ChannelA.IsValid() && ChannelB.IsValid() && ChannelA != ChannelB))
    { return false; }

    auto World = ck::FEcsWorld{};
    auto OwnerA = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(World.Get_Registry());
    auto OwnerB = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(World.Get_Registry());
    auto OwnerC = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(World.Get_Registry());
    auto ResolverA = UCk_Utils_InteractionResolver_UE::Create(
        OwnerA, MakeParams(Intent, {ChannelA, ChannelB}, 2), ECk_Replication::DoesNotReplicate);
    auto ResolverB = UCk_Utils_InteractionResolver_UE::Create(
        OwnerB, MakeParams(Intent, {ChannelB, FGameplayTag{}}, 1), ECk_Replication::DoesNotReplicate);
    auto ResolverC = UCk_Utils_InteractionResolver_UE::Create(
        OwnerC, MakeTwoIntentParams(Intent, IntentParent, ChannelA), ECk_Replication::DoesNotReplicate);
    auto GenericA = FCk_Handle{ResolverA};
    auto GenericB = FCk_Handle{ResolverB};
    auto GenericC = FCk_Handle{ResolverC};
    if (NOT TestTrue(TEXT("public APIs create complete independent resolvers with request metadata"),
        ck::IsValid(ResolverA) && ck::IsValid(ResolverB) && AddAuthorityAndWorld(GenericA)
            && ck::IsValid(ResolverC) && AddAuthorityAndWorld(GenericB) && AddAuthorityAndWorld(GenericC)))
    { return false; }

    const auto InvalidAuthored = SNew(SCkInspector_InteractionResolverAuthored).Entity(FCk_Handle{});
    const auto OrderedAuthored = SNew(SCkInspector_InteractionResolverAuthored).Entity(GenericC);
    TestTrue(TEXT("default-invalid input mounts an inert-safe authored unavailable shell"),
        InvalidAuthored->Is_Mounted() && NOT InvalidAuthored->Get_IsAvailable()
            && InvalidAuthored->Get_Mappings().IsValid()
            && InvalidAuthored->Get_Mappings()->GetRecords().IsEmpty());
    TestTrue(TEXT("two configured intents mount a dedicated authored ordering fixture"),
        OrderedAuthored->Is_Mounted() && OrderedAuthored->Get_Mappings()->GetRecords().Num() == 2);

    const auto Selection = MakeShared<FCkDebuggerModel_EntitySelection>();
    auto Inspector = FCkInspector_InteractionResolver{};
    Inspector.Set_SelectionModel(Selection);
    const auto WidgetA = Inspector.Build_Inspector(GenericA);
    const auto WidgetB = Inspector.Build_Inspector(GenericB);
    if (NOT TestTrue(TEXT("complete resolvers mount authored inspectors independently"),
        WidgetA->GetTypeAsString() == TEXT("SCkInspector_InteractionResolverAuthored")
            && WidgetB->GetTypeAsString() == TEXT("SCkInspector_InteractionResolverAuthored")))
    { AddError(Inspector.Get_LastAuthoredLoadError()); return false; }
    const auto AuthoredA = StaticCastSharedRef<SCkInspector_InteractionResolverAuthored>(WidgetA);
    const auto AuthoredB = StaticCastSharedRef<SCkInspector_InteractionResolverAuthored>(WidgetB);
    const auto ViewA = AuthoredA->Get_View();
    const auto ViewB = AuthoredB->Get_View();
    TestTrue(TEXT("each build owns its view and both collection graphs"),
        ViewA.IsValid() && ViewB.IsValid() && ViewA != ViewB
            && AuthoredA->Get_Mappings() != AuthoredB->Get_Mappings()
            && AuthoredA->Get_AvailableTargets() != AuthoredB->Get_AvailableTargets());
    if (NOT TestTrue(TEXT("mapping records preserve params and nested channel topology"),
        AuthoredA->Get_Mappings()->GetRecords().Num() == 1
            && AuthoredB->Get_Mappings()->GetRecords().Num() == 1)) { return false; }
    const auto MappingA = AuthoredA->Get_Mappings()->GetRecords()[0];
    const auto MappingB = AuthoredB->Get_Mappings()->GetRecords()[0];
    const auto ChannelsA = MappingA->FindChildCollection(TEXT("channels"));
    const auto ChannelsB = MappingB->FindChildCollection(TEXT("channels"));
    TestTrue(TEXT("the first authored mapping exposes two configured channels"),
        ChannelsA.IsValid() && ChannelsA->GetRecords().Num() == 2
            && MappingA->FindField(TEXT("intent"))->Text.ToString() == Intent.ToString()
            && MappingA->FindField(TEXT("distance-sort"))->Text.ToString().Contains(TEXT("Disabled"))
            && MappingA->FindField(TEXT("max-concurrent"))->Text.ToString() == TEXT("2"));
    TestTrue(TEXT("invalid channel configuration remains visible but cannot publish a clear action"),
        ChannelsB.IsValid() && ChannelsB->GetRecords().Num() == 2
            && MappingB->FindField(TEXT("has-clear-actions"))->Bool
            && ChannelsB->GetRecords()[0]->FindField(TEXT("clear-visible"))->Bool
            && NOT ChannelsB->GetRecords()[1]->FindField(TEXT("clear-visible"))->Bool);
    TestTrue(TEXT("runtime projection begins empty and request admission is enabled"),
        AuthoredA->Get_ActiveIntentsText() == TEXT("None") && NOT AuthoredA->Get_HasAvailableTargets()
            && AuthoredA->Get_CanRequest());

    auto Rows = TMap<FString, FString>{};
    {
        const FCkInspector_RowCaptureScope Capture;
        Inspector.Build_Inspector(GenericA);
        Rows = Capture.Get_Rows();
    }
    TestTrue(TEXT("native capture remains the complete fallback and comparison authority"),
        Rows.Contains(TEXT("Intent:")) && Rows.Contains(TEXT("Channels:"))
            && Rows.Contains(TEXT("Clear Channel:")) && Rows.Contains(TEXT("Distance Sort:"))
            && Rows.Contains(TEXT("Max Concurrent:")) && Rows.Contains(TEXT("Best Targets:"))
            && Rows.Contains(TEXT("Active Intents:")) && Rows.Contains(TEXT("Available Targets:")));

    FSlateApplication& Slate = FSlateApplication::Get();
    FWindowScope WindowScope{Slate};
    WindowScope.Window = SNew(SWindow)
        .AutoCenter(EAutoCenter::None).ClientSize(FVector2D{620.0f, 460.0f})
        .CreateTitleBar(false).HasCloseButton(false)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[AuthoredA]
            + SVerticalBox::Slot().AutoHeight()[AuthoredB]
        ];
    Slate.AddWindow(WindowScope.Window.ToSharedRef(), true);
    TickSlate(Slate);
    ViewA->GetRegion(TEXT("main"))->SlatePrepass();
    ViewB->GetRegion(TEXT("main"))->SlatePrepass();

    auto StartButtons = TArray<TSharedPtr<SButton>>{};
    auto StopButtons = TArray<TSharedPtr<SButton>>{};
    auto ClearButtons = TArray<TSharedPtr<SButton>>{};
    FindButtons(ViewA->GetRegion(TEXT("main")), TEXT("interaction-resolver-start"), StartButtons);
    FindButtons(ViewA->GetRegion(TEXT("main")), TEXT("interaction-resolver-stop"), StopButtons);
    FindButtons(ViewA->GetRegion(TEXT("main")), TEXT("interaction-resolver-clear"), ClearButtons);
    if (NOT TestTrue(TEXT("authored nested repeats mount physical intent and per-channel actions"),
        StartButtons.Num() == 1 && StopButtons.Num() == 1 && ClearButtons.Num() == 2
            && StartButtons[0]->IsEnabled() && StopButtons[0]->IsEnabled()
            && ClearButtons[0]->IsEnabled() && ClearButtons[1]->IsEnabled()))
    { return false; }
    auto InvalidChannelClearButtons = TArray<TSharedPtr<SButton>>{};
    FindButtons(ViewB->GetRegion(TEXT("main")), TEXT("interaction-resolver-clear"), InvalidChannelClearButtons);
    int32 VisibleInvalidChannelClearButtons = 0;
    for (const TSharedPtr<SButton>& Button : InvalidChannelClearButtons)
    {
        auto VisiblePath = FWidgetPath{};
        if (Button.IsValid()
            && Slate.GeneratePathToWidgetUnchecked(Button.ToSharedRef(), VisiblePath, EVisibility::Visible))
        { ++VisibleInvalidChannelClearButtons; }
    }
    TestTrue(TEXT("mounted malformed-channel mapping exposes only its one valid clear action"),
        InvalidChannelClearButtons.Num() == 2 && VisibleInvalidChannelClearButtons == 1);
    StartButtons[0]->SimulateClick();
    StopButtons[0]->SimulateClick();
    ClearButtons[1]->SimulateClick();
    if (NOT TestTrue(TEXT("physical actions queue exactly their configured intent and channel requests"),
        ResolverA.Has<ck::FFragment_InteractionResolver_Requests>())) { return false; }
    const auto& ActionRequests = ResolverA.Get<ck::FFragment_InteractionResolver_Requests>().Get_Requests();
    TestTrue(TEXT("the action queue retains exact request variants and ordering"),
        ActionRequests.Num() == 3
            && std::holds_alternative<FCk_Request_InteractionResolver_StartIntent>(ActionRequests[0])
            && std::get<FCk_Request_InteractionResolver_StartIntent>(ActionRequests[0]).Get_Intent() == Intent
            && std::holds_alternative<FCk_Request_InteractionResolver_StopIntent>(ActionRequests[1])
            && std::get<FCk_Request_InteractionResolver_StopIntent>(ActionRequests[1]).Get_Intent() == Intent
            && std::holds_alternative<FCk_Request_InteractionResolver_RemoveAllTargetsByChannel>(ActionRequests[2])
            && std::get<FCk_Request_InteractionResolver_RemoveAllTargetsByChannel>(ActionRequests[2]).Get_Channel() == ChannelB);
    PumpResolverRequests(World);

    UCk_Utils_InteractionResolver_UE::Request_StartIntent(
        ResolverC, FCk_Request_InteractionResolver_StartIntent{IntentParent}, {});
    UCk_Utils_InteractionResolver_UE::Request_StartIntent(
        ResolverC, FCk_Request_InteractionResolver_StartIntent{Intent}, {});
    PumpResolverRequests(World);
    auto ExpectedActiveIntents = FString{};
    for (const FGameplayTag& ActiveIntent :
        ResolverC.Get<ck::FFragment_InteractionResolver_Current>().Get_ActiveIntents())
    {
        if (NOT ExpectedActiveIntents.IsEmpty()) { ExpectedActiveIntents += TEXT(", "); }
        ExpectedActiveIntents += ActiveIntent.GetTagName().ToString();
    }
    TestEqual(TEXT("active intent text preserves the resolver's stored iteration order"),
        OrderedAuthored->Get_ActiveIntentsText(), ExpectedActiveIntents);

    auto TargetOwnerA = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(World.Get_Registry());
    auto TargetOwnerB = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(World.Get_Registry());
    auto TargetA = AddTarget(TargetOwnerA, ChannelA);
    auto TargetB = AddTarget(TargetOwnerB, ChannelA);
    UCk_Utils_InteractionResolver_UE::Request_AddInteractTarget(
        ResolverA, FCk_Request_InteractionResolver_AddInteractTarget{TargetA}, {});
    PumpResolverRequests(World);
    TickAuthored(AuthoredA);
    TickSlate(Slate);
    if (NOT TestTrue(TEXT("the live available-target collection publishes one stable identity"),
        AuthoredA->Get_AvailableTargets()->GetRecords().Num() == 1 && AuthoredA->Get_HasAvailableTargets()))
    { return false; }
    const auto StableAvailable = AuthoredA->Get_AvailableTargets()->GetRecords()[0];
    auto AvailableRefs = TArray<TSharedRef<SCkDebug_EntityRef>>{};
    CollectEntityRefs(ViewA->GetRegion(TEXT("main")), AvailableRefs);
    if (NOT TestTrue(TEXT("available target renders as a physical authored entity reference"), AvailableRefs.Num() == 1))
    { return false; }
    const FGeometry Geometry = FGeometry::MakeRoot(FVector2D{30.0f, 17.0f}, FSlateLayoutTransform{});
    const FPointerEvent Click{0, FVector2D{5.0f, 5.0f}, FVector2D{5.0f, 5.0f},
        TSet<FKey>{EKeys::LeftMouseButton}, EKeys::LeftMouseButton, 0.0f, FModifierKeysState{}};
    AvailableRefs[0]->OnMouseButtonDown(Geometry, Click);
    TestTrue(TEXT("available target navigation routes through the inspector selection model"),
        Selection->Get_PrimarySelection() == TargetA);

    UCk_Utils_InteractionResolver_UE::Request_AddInteractTarget(
        ResolverA, FCk_Request_InteractionResolver_AddInteractTarget{TargetB}, {});
    PumpResolverRequests(World);
    TickAuthored(AuthoredA);
    TickSlate(Slate);
    const auto& OrderedTargets =
        ResolverA.Get<ck::FFragment_InteractionResolver_Current>().Get_AvailableTargets();
    const auto& OrderedRecords = AuthoredA->Get_AvailableTargets()->GetRecords();
    bool PreservesResolverOrder = OrderedRecords.Num() == OrderedTargets.Num();
    int32 OrderedIndex = 0;
    for (const FCk_Handle_InteractTarget& OrderedTarget : OrderedTargets)
    {
        const FCkUiFieldValue* TargetId = PreservesResolverOrder
            ? OrderedRecords[OrderedIndex++]->FindField(TEXT("target-id")) : nullptr;
        PreservesResolverOrder = TargetId != nullptr
            && TargetId->Text.ToString() == ck::Format_UE(TEXT("{}"), OrderedTarget.Get_Entity());
    }
    TestTrue(TEXT("available target records preserve the resolver's stored iteration order"), PreservesResolverOrder);
    UCk_Utils_InteractionResolver_UE::Request_RemoveInteractTarget(
        ResolverA, FCk_Request_InteractionResolver_RemoveInteractTarget{TargetB}, {});
    PumpResolverRequests(World);
    TickAuthored(AuthoredA);
    TickSlate(Slate);
    auto RestoredSingleRefs = TArray<TSharedRef<SCkDebug_EntityRef>>{};
    CollectEntityRefs(ViewA->GetRegion(TEXT("main")), RestoredSingleRefs);
    TestTrue(TEXT("removing the sibling retains the surviving target's physical identity"),
        RestoredSingleRefs.Num() == 1 && RestoredSingleRefs[0] == AvailableRefs[0]);

    UCk_Utils_InteractionResolver_UE::Request_RemoveInteractTarget(
        ResolverA, FCk_Request_InteractionResolver_RemoveInteractTarget{TargetA}, {});
    UCk_Utils_InteractionResolver_UE::Request_AddInteractTarget(
        ResolverA, FCk_Request_InteractionResolver_AddInteractTarget{TargetB}, {});
    PumpResolverRequests(World);
    TickAuthored(AuthoredA);
    TickSlate(Slate);
    auto ReplacementRefs = TArray<TSharedRef<SCkDebug_EntityRef>>{};
    CollectEntityRefs(ViewA->GetRegion(TEXT("main")), ReplacementRefs);
    Selection->Clear_Selection();
    AvailableRefs[0]->OnMouseButtonDown(Geometry, Click);
    if (NOT TestTrue(TEXT("equal-count replacement retires the old record and held reference"),
        AuthoredA->Get_AvailableTargets()->GetRecords().Num() == 1
            && AuthoredA->Get_AvailableTargets()->GetRecords()[0] != StableAvailable
            && ReplacementRefs.Num() == 1 && ReplacementRefs[0] != AvailableRefs[0]
            && Selection->Get_SelectedEntities().IsEmpty()))
    { return false; }
    ReplacementRefs[0]->OnMouseButtonDown(Geometry, Click);
    TestTrue(TEXT("the replacement reference navigates only the new target identity"),
        Selection->Get_PrimarySelection() == TargetB);

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup, Stylesheet;
    const FString Root = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("installed InteractionResolver resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(Root, TEXT("EcsInspectorInteractionResolver.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(Root, TEXT("EcsInspectorInteractionResolver.ui.css")))))
    { return false; }
    TestTrue(TEXT("HTML owns the complete mapping, action, nested collection, and runtime layout"),
        Markup.Contains(TEXT("child-bind=\"channels\""))
            && Markup.Contains(TEXT("child-bind=\"best-targets\""))
            && Markup.Contains(TEXT("interaction-resolver-start-intent"))
            && Markup.Contains(TEXT("interaction-resolver-stop-intent"))
            && Markup.Contains(TEXT("interaction-resolver-clear-channel"))
            && Markup.Contains(TEXT("visible-field=\"has-clear-actions\""))
            && Markup.Contains(TEXT("visible-field=\"clear-visible\""))
            && Markup.Contains(TEXT("interaction-resolver-available-targets"))
            && NOT Markup.Contains(TEXT("<native")));
    const int64 RevisionA = ViewA->GetRevision();
    TestTrue(TEXT("compatible reload retains physical action identity"),
        ViewA->TryReload(Markup, Stylesheet, TEXT("InteractionResolver compatible candidate")).Succeeded
            && ViewA->GetRevision() > RevisionA
            && [&]() { auto Found = TArray<TSharedPtr<SButton>>{};
                FindButtons(ViewA->GetRegion(TEXT("main")), TEXT("interaction-resolver-start"), Found);
                return Found.Num() == 1 && Found[0] == StartButtons[0]; }());
    const int64 RevisionB = ViewB->GetRevision();
    const TSharedRef<SWidget> MainB = ViewB->GetRegion(TEXT("main"));
    TestTrue(TEXT("missing nested item action is rejected atomically"),
        NOT ViewB->TryReload(Markup.Replace(TEXT("item-action=\"interaction-resolver-clear-channel\""),
                TEXT("item-action=\"interaction-resolver-missing\"")), Stylesheet,
                TEXT("InteractionResolver rejected candidate")).Succeeded
            && ViewB->GetRevision() == RevisionB && &ViewB->GetRegion(TEXT("main")).Get() == &MainB.Get());

    auto StaleStartButtons = TArray<TSharedPtr<SButton>>{};
    FindButtons(ViewB->GetRegion(TEXT("main")), TEXT("interaction-resolver-start"), StaleStartButtons);
    const int32 RequestsBeforeTopologyChange = ResolverB.Has<ck::FFragment_InteractionResolver_Requests>()
        ? ResolverB.Get<ck::FFragment_InteractionResolver_Requests>().Get_Requests().Num() : 0;
    if (NOT TestTrue(TEXT("fixture can replace resolver params beneath a retained mapping action"),
        StaleStartButtons.Num() == 1
            && GenericB.Try_Remove<ck::FFragment_InteractionResolver_Params>()))
    { return false; }
    GenericB.Add<ck::FFragment_InteractionResolver_Params>(
        MakeParams(ChannelA, {ChannelB}, 1));
    StaleStartButtons[0]->SimulateClick();
    const int32 RequestsAfterTopologyChange = ResolverB.Has<ck::FFragment_InteractionResolver_Requests>()
        ? ResolverB.Get<ck::FFragment_InteractionResolver_Requests>().Get_Requests().Num() : 0;
    TestEqual(TEXT("retained action rejects a stale mapping key before final request admission"),
        RequestsAfterTopologyChange, RequestsBeforeTopologyChange);

    TSharedPtr<SCkInspector_InteractionResolverAuthored> DestructorAuthored;
    {
        auto DestructorInspector = MakeUnique<FCkInspector_InteractionResolver>();
        DestructorAuthored = StaticCastSharedRef<SCkInspector_InteractionResolverAuthored>(
            DestructorInspector->Build_Inspector(GenericB));
    }
    TestTrue(TEXT("inspector destruction releases its retained authored resolver view"),
        DestructorAuthored.IsValid() && DestructorAuthored->Is_Inert() && NOT DestructorAuthored->Is_Mounted());

    TestTrue(TEXT("removing Current leaves a live entity with partial resolver composition"),
        GenericB.Try_Remove<ck::FFragment_InteractionResolver_Current>() && ck::IsValid(GenericB));
    TickAuthored(AuthoredB);
    TestTrue(TEXT("partial composition fails projection, collections, and request admission closed"),
        NOT AuthoredB->Get_IsAvailable() && NOT AuthoredB->Get_CanRequest()
            && AuthoredB->Get_ActiveIntentsText() == TEXT("--")
            && AuthoredB->Get_Mappings()->GetRecords().IsEmpty()
            && AuthoredB->Get_AvailableTargets()->GetRecords().IsEmpty());

    const int32 RequestsBeforeRelease = ResolverA.Has<ck::FFragment_InteractionResolver_Requests>()
        ? ResolverA.Get<ck::FFragment_InteractionResolver_Requests>().Get_Requests().Num() : 0;
    Inspector.OnDeactivated();
    TestTrue(TEXT("deactivation releases every retained resolver view and collection graph"),
        AuthoredA->Is_Inert() && AuthoredB->Is_Inert()
            && NOT AuthoredA->Is_Mounted() && NOT AuthoredB->Is_Mounted()
            && NOT AuthoredA->Get_View().IsValid() && NOT AuthoredB->Get_View().IsValid()
            && NOT AuthoredA->Get_Mappings().IsValid() && NOT AuthoredB->Get_Mappings().IsValid()
            && NOT AuthoredA->Get_AvailableTargets().IsValid() && NOT AuthoredB->Get_AvailableTargets().IsValid());
    StartButtons[0]->SimulateClick();
    ClearButtons[0]->SimulateClick();
    const int32 RequestsAfterRelease = ResolverA.Has<ck::FFragment_InteractionResolver_Requests>()
        ? ResolverA.Get<ck::FFragment_InteractionResolver_Requests>().Get_Requests().Num() : 0;
    TestEqual(TEXT("retained physical actions remain inert after owner release"),
        RequestsAfterRelease, RequestsBeforeRelease);
    InvalidAuthored->Release();
    OrderedAuthored->Release();
    return true;
}

#endif
