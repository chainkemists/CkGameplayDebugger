#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CkEcsDebugger/Inspectors/CkInspector_AnimPlans.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"

#include "CkAnimation/AnimPlan/CkAnimPlan_Fragment_Data.h"
#include "CkAnimation/AnimPlan/CkAnimPlan_Processor.h"
#include "CkAnimation/AnimPlan/CkAnimPlan_Utils.h"
#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Utils/CkDebug_InspectorEditGuard.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/World/CkEcsWorld.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiRepeat.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SWindow.h"

namespace ck_inspector_anim_plans_authored_test
{
    auto AddPlan(FCk_Handle& InOwner, const FGameplayTag InGoal, const FGameplayTag InCluster,
        const FGameplayTag InState) -> FCk_Handle_AnimPlan
    {
        auto Params = FCk_Fragment_AnimPlan_ParamsData{InGoal};
        Params.Set_StartingAnimCluster(InCluster);
        Params.Set_StartingAnimState(InState);
        return UCk_Utils_AnimPlan_UE::Add(InOwner, Params, ECk_Replication::DoesNotReplicate);
    }

    auto FindEditor(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SEditableTextBox>
    {
        if (InRoot->GetTag() == InTag && InRoot->GetTypeAsString() == TEXT("SCkUiTextInputBox"))
        { return StaticCastSharedRef<SEditableTextBox>(InRoot); }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SEditableTextBox> Found = FindEditor(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag); Found.IsValid()) { return Found; }
        }
        return {};
    }

    auto FindButton(const TSharedRef<SWidget>& InRoot, const FName InTag) -> TSharedPtr<SButton>
    {
        if (InRoot->GetTag() == InTag && (InRoot->GetTypeAsString() == TEXT("SButton")
                || InRoot->GetTypeAsString() == TEXT("SCkUiStyledButton")))
        { return StaticCastSharedRef<SButton>(InRoot); }
        FChildren* Children = InRoot->GetChildren();
        for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        {
            if (const TSharedPtr<SButton> Found = FindButton(
                ConstCastSharedRef<SWidget>(Children->GetChildAt(Index)), InTag); Found.IsValid()) { return Found; }
        }
        return {};
    }

    auto TickSlate(FSlateApplication& InSlate) -> void
    {
        InSlate.PumpMessages();
        InSlate.Tick();
        InSlate.Tick();
    }

    auto ReplaceAndCommit(FSlateApplication& InSlate, const TSharedRef<SEditableTextBox>& InInput,
        const FString& InText) -> bool
    {
        InSlate.SetUserFocus(0, InInput, EFocusCause::SetDirectly);
        TickSlate(InSlate);
        const FModifierKeysState Control{false, false, true, false, false, false, false, false, false};
        if (NOT InSlate.ProcessKeyDownEvent(FKeyEvent{EKeys::A, Control, 0, false, 0, 0})) { return false; }
        for (const TCHAR Character : InText)
        {
            if (NOT InSlate.ProcessKeyCharEvent(FCharacterEvent{Character, FModifierKeysState{}, 0, false})) { return false; }
        }
        if (NOT InSlate.ProcessKeyDownEvent(FKeyEvent{EKeys::Enter, FModifierKeysState{}, 0, false, 0, 0})) { return false; }
        TickSlate(InSlate);
        return true;
    }

    auto PumpRequests(ck::FEcsWorld& InWorld, FCk_Handle_AnimPlan& InPlan) -> bool
    {
        if (NOT InPlan.Has<ck::FFragment_AnimPlan_Params>()
            || NOT InPlan.Has<ck::FFragment_AnimPlan_Current>()
            || NOT InPlan.Has<ck::FFragment_AnimPlan_Requests>()) { return false; }
        const ck::FProcessor_AnimPlan_HandleRequests Processor{InWorld.Get_Registry()};
        Processor.ForEachEntity(FCk_Time{0.0}, InPlan, InPlan.Get<ck::FFragment_AnimPlan_Params>(),
            InPlan.Get<ck::FFragment_AnimPlan_Current>(), InPlan.Get<ck::FFragment_AnimPlan_Requests>());
        return true;
    }

    struct FWindowScope final
    {
        explicit FWindowScope(FSlateApplication& InSlate) : Slate(InSlate) {}
        ~FWindowScope()
        {
            Slate.ClearUserFocus(0, EFocusCause::SetDirectly);
            if (Window.IsValid()) { Slate.DestroyWindowImmediately(Window.ToSharedRef()); }
        }
        FSlateApplication& Slate;
        TSharedPtr<SWindow> Window;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkInspectorAnimPlansAuthored,
    "Ck.UiAuthoring.EcsDebugger.AnimPlansInspector.AuthoredComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkInspectorAnimPlansAuthored::RunTest(const FString&) -> bool
{
    using namespace ck_inspector_anim_plans_authored_test;
    if (NOT FSlateApplication::IsInitialized()) { AddError(TEXT("Anim Plans authored test requires Slate.")); return false; }

    UCkDebuggerStyleSettings* Style = GetMutableDefault<UCkDebuggerStyleSettings>();
    if (NOT TestNotNull(TEXT("debugger style settings are available"), Style)) { return false; }
    const ECkDebugAxis_EditControlStyle PreviousStyle = Style->Selection.EditControlStyle;
    ON_SCOPE_EXIT { Style->Selection.EditControlStyle = PreviousStyle; };
    Style->Selection.EditControlStyle = ECkDebugAxis_EditControlStyle::Inline;

    auto World = ck::FEcsWorld{};
    auto OwnerA = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(World.Get_Registry());
    auto OwnerB = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(World.Get_Registry());
    auto PlanA = AddPlan(OwnerA, TAG_Label_AnimPlan_Goal.GetTag(), Tag_Label_AnimPlan_Cluster.GetTag(),
        TAG_Label_AnimPlan_State.GetTag());
    auto PlanB = AddPlan(OwnerB, TAG_Label_AnimPlan_Goal.GetTag(), TAG_Label_AnimPlan_State.GetTag(),
        Tag_Label_AnimPlan_Cluster.GetTag());
    if (NOT TestTrue(TEXT("fixture creates independent owners and plans"), ck::IsValid(OwnerA) && ck::IsValid(OwnerB)
        && ck::IsValid(PlanA) && ck::IsValid(PlanB) && PlanA != PlanB)) { return false; }

    const TSharedRef<FCkInspectorEditGuard> EditGuard = MakeShared<FCkInspectorEditGuard>();
    const TSharedRef<FCkDebuggerModel_EntitySelection> Selection = MakeShared<FCkDebuggerModel_EntitySelection>();
    auto Inspector = FCkInspector_AnimPlans{};
    Inspector.Set_EditGuard(EditGuard);
    Inspector.Set_SelectionModel(Selection);
    const TSharedRef<SWidget> RenderedA = Inspector.Build_Inspector(OwnerA);
    const TSharedRef<SWidget> RenderedB = Inspector.Build_Inspector(OwnerB);
    if (NOT TestEqual(TEXT("first build mounts authored Anim Plans"), RenderedA->GetTypeAsString(), FString{TEXT("SCkInspector_AnimPlansAuthored")})
        || NOT TestEqual(TEXT("second build mounts an independent authored Anim Plans"), RenderedB->GetTypeAsString(), FString{TEXT("SCkInspector_AnimPlansAuthored")}))
    { AddError(Inspector.Get_LastAuthoredLoadError()); return false; }
    const TSharedRef<SCkInspector_AnimPlansAuthored> AuthoredA = StaticCastSharedRef<SCkInspector_AnimPlansAuthored>(RenderedA);
    const TSharedRef<SCkInspector_AnimPlansAuthored> AuthoredB = StaticCastSharedRef<SCkInspector_AnimPlansAuthored>(RenderedB);
    TSharedPtr<FCkUiView> ViewA = AuthoredA->Get_View();
    TSharedPtr<FCkUiView> ViewB = AuthoredB->Get_View();
    const TSharedPtr<FCkUiCollection> PlansA = AuthoredA->Get_Plans();
    if (NOT TestTrue(TEXT("each build owns an independent accepted view and collection"), ViewA.IsValid() && ViewB.IsValid()
        && ViewA != ViewB && PlansA.IsValid() && PlansA != AuthoredB->Get_Plans()
        && PlansA->GetRecords().Num() == 1 && AuthoredB->Get_Plans()->GetRecords().Num() == 1)) { return false; }

    auto NativeRowsA = TMap<FString, FString>{};
    auto NativeRowsB = TMap<FString, FString>{};
    { const FCkInspector_RowCaptureScope Capture; Inspector.Build_Inspector(OwnerA); NativeRowsA = Capture.Get_Rows(); }
    { const FCkInspector_RowCaptureScope Capture; Inspector.Build_Inspector(OwnerB); NativeRowsB = Capture.Get_Rows(); }
    const TSet<FString> Differing = FCkInspectorWidgetBuilder::Compute_DifferingLabels({NativeRowsA, NativeRowsB});
    TestTrue(TEXT("native fallback remains full dynamic-row and diff authority"),
        NativeRowsA.Contains(TAG_Label_AnimPlan_Goal.GetTag().ToString()) && NativeRowsA.Contains(TEXT("  Cluster:"))
        && NativeRowsA.Contains(TEXT("  State <- cluster:")) && NativeRowsA.Contains(TEXT("  State <- state:"))
        && Differing.Contains(TEXT("  Cluster:")));

    const FString StableKey = ck::Format_UE(TEXT("{}"), PlanA.Get_Entity());
    const TSharedPtr<SCkUiRepeat> Repeat = ViewA->GetRepeat(TEXT("anim-plans-records"));
    TSharedPtr<SWidget> Item = Repeat.IsValid() ? Repeat->GetItemWidget(StableKey) : nullptr;
    if (NOT TestTrue(TEXT("authored repeat mounts the stable plan record"), Item.IsValid())) { return false; }
    TSharedPtr<SEditableTextBox> ClusterInput = FindEditor(Item.ToSharedRef(), TEXT("anim-plan-cluster-input"));
    TSharedPtr<SEditableTextBox> PendingClusterInput = FindEditor(Item.ToSharedRef(), TEXT("anim-plan-pending-cluster-input"));
    TSharedPtr<SEditableTextBox> PendingStateInput = FindEditor(Item.ToSharedRef(), TEXT("anim-plan-pending-state-input"));
    TSharedPtr<SButton> GoalButton = FindButton(Item.ToSharedRef(), TEXT("anim-plan-goal"));
    TSharedPtr<SButton> ApplyButton = FindButton(Item.ToSharedRef(), TEXT("anim-plan-apply"));
    const auto RefreshControls = [&]() -> bool
    {
        Item = Repeat->GetItemWidget(StableKey);
        if (NOT Item.IsValid()) { return false; }
        ClusterInput = FindEditor(Item.ToSharedRef(), TEXT("anim-plan-cluster-input"));
        PendingClusterInput = FindEditor(Item.ToSharedRef(), TEXT("anim-plan-pending-cluster-input"));
        PendingStateInput = FindEditor(Item.ToSharedRef(), TEXT("anim-plan-pending-state-input"));
        GoalButton = FindButton(Item.ToSharedRef(), TEXT("anim-plan-goal"));
        ApplyButton = FindButton(Item.ToSharedRef(), TEXT("anim-plan-apply"));
        return ClusterInput.IsValid() && PendingClusterInput.IsValid() && PendingStateInput.IsValid()
            && GoalButton.IsValid() && ApplyButton.IsValid();
    };
    const bool bHasEveryPhysicalControl =
        TestTrue(TEXT("HTML owns the direct cluster text input"), ClusterInput.IsValid())
        && TestTrue(TEXT("HTML owns the staged cluster text input"), PendingClusterInput.IsValid())
        && TestTrue(TEXT("HTML owns the staged state text input"), PendingStateInput.IsValid())
        && TestTrue(TEXT("HTML owns the goal navigation button"), GoalButton.IsValid())
        && TestTrue(TEXT("HTML owns the Apply State button"), ApplyButton.IsValid());
    if (NOT bHasEveryPhysicalControl) { return false; }

    FSlateApplication& Slate = FSlateApplication::Get();
    FWindowScope WindowScope{Slate};
    WindowScope.Window = SNew(SWindow).AutoCenter(EAutoCenter::None).ClientSize(FVector2D{640.0f, 320.0f})
        .CreateTitleBar(false).HasCloseButton(false)[RenderedA];
    Slate.AddWindow(WindowScope.Window.ToSharedRef(), true);
    TickSlate(Slate);
    GoalButton->SimulateClick();
    TestTrue(TEXT("goal action navigates to the exact plan child"), Selection->Get_PrimarySelection() == FCk_Handle{PlanA});

    if (NOT TestTrue(TEXT("direct cluster keyboard commit routes through the stable row"),
        ReplaceAndCommit(Slate, ClusterInput.ToSharedRef(), TAG_Label_AnimPlan_State.GetTag().ToString()))) { return false; }
    TestFalse(TEXT("keyboard commit releases the panel edit guard"), EditGuard->Get_HasActiveEdit());
    TestTrue(TEXT("cluster commit queues exactly on the selected plan"), PlanA.Has<ck::FFragment_AnimPlan_Requests>()
        && NOT PlanB.Has<ck::FFragment_AnimPlan_Requests>());
    if (NOT TestTrue(TEXT("cluster request fixture pumps"), PumpRequests(World, PlanA))) { return false; }
    TestTrue(TEXT("cluster request updates cluster and clears state"),
        UCk_Utils_AnimPlan_UE::Get_AnimCluster(PlanA).Get_AnimCluster() == TAG_Label_AnimPlan_State.GetTag()
        && NOT UCk_Utils_AnimPlan_UE::Get_AnimState(PlanA).Get_AnimState().IsValid());
    TickSlate(Slate);
    if (NOT TestTrue(TEXT("live cluster refresh remounts current keyed controls"), RefreshControls())) { return false; }

    if (NOT TestTrue(TEXT("staged cluster keyboard commit routes independently"),
        ReplaceAndCommit(Slate, PendingClusterInput.ToSharedRef(), Tag_Label_AnimPlan_Cluster.GetTag().ToString()))) { return false; }
    if (NOT TestTrue(TEXT("staged cluster refresh remounts current keyed controls"), RefreshControls())) { return false; }
    if (NOT TestTrue(TEXT("staged state keyboard commit routes independently"),
        ReplaceAndCommit(Slate, PendingStateInput.ToSharedRef(), Tag_Label_AnimPlan_Cluster.GetTag().ToString()))) { return false; }
    if (NOT TestTrue(TEXT("staged state refresh remounts current keyed controls"), RefreshControls())) { return false; }
    TestFalse(TEXT("staging alone queues no request"), PlanA.Has<ck::FFragment_AnimPlan_Requests>());
    ApplyButton->SimulateClick();
    TestTrue(TEXT("Apply State atomically queues the staged pair"), PlanA.Has<ck::FFragment_AnimPlan_Requests>());
    if (NOT TestTrue(TEXT("state request fixture pumps"), PumpRequests(World, PlanA))) { return false; }
    TestTrue(TEXT("state request applies both staged tags"),
        UCk_Utils_AnimPlan_UE::Get_AnimCluster(PlanA).Get_AnimCluster() == Tag_Label_AnimPlan_Cluster.GetTag()
        && UCk_Utils_AnimPlan_UE::Get_AnimState(PlanA).Get_AnimState() == Tag_Label_AnimPlan_Cluster.GetTag());
    TickSlate(Slate);
    if (NOT TestTrue(TEXT("live state refresh remounts current keyed controls"), RefreshControls())) { return false; }

    if (NOT TestTrue(TEXT("unknown tag keyboard commit reaches validation"),
        ReplaceAndCommit(Slate, ClusterInput.ToSharedRef(), TEXT("AutoTest.AnimPlans.Unknown")))) { return false; }
    TestFalse(TEXT("unknown tags fail closed without queueing"), PlanA.Has<ck::FFragment_AnimPlan_Requests>());

    const TSharedRef<SWidget> Filtered = Inspector.Build_Inspector(OwnerA, TAG_Label_AnimPlan_Goal.GetTag().ToString());
    const TSharedRef<SCkInspector_AnimPlansAuthored> FilteredAuthored = StaticCastSharedRef<SCkInspector_AnimPlansAuthored>(Filtered);
    TestTrue(TEXT("authored filtering keeps the matching dynamic goal and hides unrelated rows"),
        FilteredAuthored->Get_Plans().IsValid() && FilteredAuthored->Get_Plans()->GetRecords().Num() == 1);

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    FString Markup, Stylesheet;
    const FString ResourceRoot = Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI")) : FString{};
    if (NOT TestTrue(TEXT("installed Anim Plans resources are readable"), Plugin.IsValid()
        && FFileHelper::LoadFileToString(Markup, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorAnimPlans.ui.html")))
        && FFileHelper::LoadFileToString(Stylesheet, *FPaths::Combine(ResourceRoot, TEXT("EcsInspectorAnimPlans.ui.css"))))) { return false; }
    TestTrue(TEXT("resource owns the complete port-free interaction layout"), Markup.Contains(TEXT("bind=\"anim-plans\""))
        && Markup.Contains(TEXT("item-action=\"anim-plan-navigate\""))
        && Markup.Contains(TEXT("item-committed=\"anim-plan-cluster-committed\""))
        && Markup.Contains(TEXT("item-action=\"anim-plan-apply-state\"")) && NOT Markup.Contains(TEXT("<native")));
    const int64 Revision = ViewA->GetRevision();
    const TSharedPtr<SWidget> MainBeforeReject = ViewA->GetRegion(TEXT("main"));
    TestFalse(TEXT("missing keyed cluster handler rejects atomically"), ViewA->TryReload(
        Markup.Replace(TEXT(" item-committed=\"anim-plan-cluster-committed\""), TEXT("")), Stylesheet,
        TEXT("Anim Plans missing cluster handler")).Succeeded);
    TestTrue(TEXT("rejected authored candidate retains the accepted tree and revision"),
        ViewA->GetRevision() == Revision && ViewA->GetRegion(TEXT("main")) == MainBeforeReject);

    PlanA.Remove<ck::FFragment_AnimPlan_Current>();
    ApplyButton->SimulateClick();
    TestFalse(TEXT("held action rejects malformed plan composition"), PlanA.Has<ck::FFragment_AnimPlan_Requests>());

    const TWeakPtr<FCkUiView> ReleasedA = ViewA;
    const TWeakPtr<FCkUiView> ReleasedB = ViewB;
    Inspector.OnDeactivated();
    TestTrue(TEXT("deactivation makes every authored instance inert and releases the edit guard"),
        AuthoredA->Is_Inert() && AuthoredB->Is_Inert() && FilteredAuthored->Is_Inert()
        && NOT AuthoredA->Is_Mounted() && NOT AuthoredB->Is_Mounted() && NOT EditGuard->Get_HasActiveEdit());
    ViewA.Reset(); ViewB.Reset();
    TestFalse(TEXT("deactivation releases every per-build authored view"), ReleasedA.IsValid() || ReleasedB.IsValid());
    ReplaceAndCommit(Slate, PendingStateInput.ToSharedRef(), TAG_Label_AnimPlan_State.GetTag().ToString());
    TestFalse(TEXT("held stale editor cannot queue work or reactivate the guard"),
        PlanA.Has<ck::FFragment_AnimPlan_Requests>() || EditGuard->Get_HasActiveEdit());
    return true;
}

#endif
