#include "CkInspector_AnimPlans.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkAnimation/AnimPlan/CkAnimPlan_Utils.h"
#include "CkLabel/CkLabel_Utils.h"

#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Utils/CkDebug_InspectorEditGuard.h"
#include "CkDebuggerCommon/Utils/CkDebug_RequestGate.h"
#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_AnimPlans)

namespace ck_inspector_anim_plans
{
    auto IsPlanAvailable(const FCk_Handle_AnimPlan& InPlan) -> bool
    {
        return ck::IsValid(InPlan)
            && InPlan.Has<ck::FFragment_AnimPlan_Params>()
            && InPlan.Has<ck::FFragment_AnimPlan_Current>();
    }

    auto Key(const FCk_Handle_AnimPlan& InPlan) -> FString
    {
        return IsPlanAvailable(InPlan) ? ck::Format_UE(TEXT("{}"), InPlan.Get_Entity()) : FString{};
    }

    auto ParseTag(const FText& InText) -> FGameplayTag
    {
        const FString Trimmed = InText.ToString().TrimStartAndEnd();
        if (Trimmed.IsEmpty()) { return {}; }
        constexpr bool ErrorIfNotFound = false;
        return FGameplayTag::RequestGameplayTag(FName{*Trimmed}, ErrorIfNotFound);
    }

    auto TagText(const FGameplayTag InTag) -> FString
    {
        return InTag.IsValid() ? InTag.ToString() : TEXT("(none)");
    }

    auto TextField(const FString& InText) -> FCkUiFieldValue
    {
        return FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(InText)};
    }

    auto BoolField(const bool bInValue) -> FCkUiFieldValue
    {
        return FCkUiFieldValue{.Kind = ECkUiFieldKind::Bool, .Bool = bInValue};
    }

    auto ColorField(const bool bInMarked) -> FCkUiFieldValue
    {
        return FCkUiFieldValue{.Kind = ECkUiFieldKind::Color,
            .Color = bInMarked ? CkStyle::Accent() : CkStyle::Text()};
    }

    auto Gate(const FCk_Handle& InOwner) -> FCk_DebugRequest_GateVerdict
    {
        return ck::DebugRequestGate::Evaluate(InOwner, ECk_DebugRequest_Requirement::LocalOk);
    }
}

auto SCkInspector_AnimPlansAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _Filter = InArgs._Filter;
    _DiffLabels = InArgs._DiffLabels;
    _SelectionModel = InArgs._SelectionModel;
    _EditScope = MakeShared<FCkInspectorEditScope>(InArgs._EditGuard);
    const TSharedRef<SBox> Host = SNew(SBox);
    ChildSlot[Host];
    if (Refresh_Records() && Build_AuthoredView())
    {
        Host->SetContent(_View->GetRegion(TEXT("main")));
        return;
    }
    Release();
    Host->SetContent(SNullWidget::NullWidget);
}

SCkInspector_AnimPlansAuthored::~SCkInspector_AnimPlansAuthored()
{
    Release();
}

auto SCkInspector_AnimPlansAuthored::Get_IsAvailable() const -> bool
{
    return _Active && ck::IsValid(_Entity) && UCk_Utils_AnimPlan_UE::Has_Any(_Entity);
}

auto SCkInspector_AnimPlansAuthored::Get_EditEnabled() const -> bool
{
    return Get_IsAvailable() && ck_inspector_anim_plans::Gate(_Entity).IsEnabled;
}

auto SCkInspector_AnimPlansAuthored::Get_DisabledReason() const -> FString
{
    if (NOT _Active) { return {}; }
    return ck_inspector_anim_plans::Gate(_Entity).Reason.ToString();
}

auto SCkInspector_AnimPlansAuthored::ResolvePlan(const FString& InStableKey, FCk_Handle_AnimPlan& OutPlan) const -> bool
{
    OutPlan = {};
    if (NOT Get_IsAvailable()) { return false; }
    const FCk_Handle_AnimPlan* Candidate = _PlansByKey.Find(InStableKey);
    if (Candidate == nullptr || NOT ck_inspector_anim_plans::IsPlanAvailable(*Candidate)) { return false; }
    const FGameplayTag Goal = UCk_Utils_AnimPlan_UE::Get_AnimGoal(*Candidate).Get_AnimGoal();
    if (NOT Goal.IsValid()) { return false; }
    const FCk_Handle_AnimPlan Current = UCk_Utils_AnimPlan_UE::TryGet_AnimPlan(_Entity, Goal);
    if (NOT ck_inspector_anim_plans::IsPlanAvailable(Current) || Current != *Candidate) { return false; }
    OutPlan = Current;
    return true;
}

auto SCkInspector_AnimPlansAuthored::Refresh_Records() -> bool
{
    if (NOT _Plans.IsValid())
    {
        const FCkUiLoadResult CreateResult = FCkUiCollection::TryCreate({
            {TEXT("goal"), ECkUiFieldKind::Text}, {TEXT("summary"), ECkUiFieldKind::Text},
            {TEXT("cluster"), ECkUiFieldKind::Text}, {TEXT("pending-cluster"), ECkUiFieldKind::Text},
            {TEXT("pending-state"), ECkUiFieldKind::Text}, {TEXT("goal-visible"), ECkUiFieldKind::Bool},
            {TEXT("cluster-visible"), ECkUiFieldKind::Bool}, {TEXT("pending-cluster-visible"), ECkUiFieldKind::Bool},
            {TEXT("pending-state-visible"), ECkUiFieldKind::Bool}, {TEXT("apply-visible"), ECkUiFieldKind::Bool},
            {TEXT("edit-visible"), ECkUiFieldKind::Bool}, {TEXT("read-visible"), ECkUiFieldKind::Bool},
            {TEXT("goal-color"), ECkUiFieldKind::Color}, {TEXT("cluster-color"), ECkUiFieldKind::Color},
            {TEXT("pending-cluster-color"), ECkUiFieldKind::Color}, {TEXT("pending-state-color"), ECkUiFieldKind::Color},
            {TEXT("apply-label"), ECkUiFieldKind::Text},
            {TEXT("apply-tooltip"), ECkUiFieldKind::Text},
        }, _Plans);
        if (NOT CreateResult.Succeeded || NOT _Plans.IsValid())
        {
            _LoadError = CreateResult.Succeeded ? TEXT("Anim Plans collection is unavailable.") : FString::Join(CreateResult.Errors, TEXT("\n"));
            return false;
        }
    }

    auto Records = TArray<FCkUiRecordData>{};
    auto LivePlans = TMap<FString, FCk_Handle_AnimPlan>{};
    auto LiveKeys = TSet<FString>{};
    if (Get_IsAvailable())
    {
        auto MutableOwner = _Entity;
        UCk_Utils_AnimPlan_UE::ForEach_AnimPlan(MutableOwner, [this, &Records, &LivePlans, &LiveKeys](FCk_Handle_AnimPlan& InPlan)
        {
            if (NOT ck_inspector_anim_plans::IsPlanAvailable(InPlan)) { return; }
            const FString StableKey = ck_inspector_anim_plans::Key(InPlan);
            const FGameplayTag GoalTag = UCk_Utils_AnimPlan_UE::Get_AnimGoal(InPlan).Get_AnimGoal();
            if (StableKey.IsEmpty() || NOT GoalTag.IsValid() || LiveKeys.Contains(StableKey)) { return; }
            LiveKeys.Add(StableKey);
            LivePlans.Add(StableKey, InPlan);

            const FGameplayTag ClusterTag = UCk_Utils_AnimPlan_UE::Get_AnimCluster(InPlan).Get_AnimCluster();
            const FGameplayTag StateTag = UCk_Utils_AnimPlan_UE::Get_AnimState(InPlan).Get_AnimState();
            if (NOT _PendingClusters.Contains(StableKey)) { _PendingClusters.Add(StableKey, ClusterTag); }
            if (NOT _PendingStates.Contains(StableKey)) { _PendingStates.Add(StableKey, StateTag); }
            const FString Goal = GoalTag.ToString();
            const FString Cluster = ck_inspector_anim_plans::TagText(ClusterTag);
            const FString State = ck_inspector_anim_plans::TagText(StateTag);
            const FString PendingCluster = ck_inspector_anim_plans::TagText(_PendingClusters.FindRef(StableKey));
            const FString PendingState = ck_inspector_anim_plans::TagText(_PendingStates.FindRef(StableKey));
            const FString Summary = ck::Format_UE(TEXT("{} | {}"), ClusterTag.IsValid() ? ClusterTag.ToString() : TEXT("-"), StateTag.IsValid() ? StateTag.ToString() : TEXT("-"));
            const bool bGoalVisible = FCkInspectorWidgetBuilder::Matches_Filter(_Filter, Goal, Summary);
            const bool bClusterVisible = FCkInspectorWidgetBuilder::Matches_Filter(_Filter, TEXT("  Cluster:"), Cluster);
            const bool bPendingClusterVisible = FCkInspectorWidgetBuilder::Matches_Filter(_Filter, TEXT("  State <- cluster:"), PendingCluster);
            const bool bPendingStateVisible = FCkInspectorWidgetBuilder::Matches_Filter(_Filter, TEXT("  State <- state:"), PendingState);
            const bool bApplyVisible = FCkInspectorWidgetBuilder::Matches_Filter(_Filter, TEXT("  "), TEXT("Apply State"));
            if (NOT bGoalVisible && NOT bClusterVisible && NOT bPendingClusterVisible && NOT bPendingStateVisible && NOT bApplyVisible) { return; }

            const bool bEditVisible = ck::debug_axes::EditControls_AreVisible(UCkDebuggerStyleSettings::Get_Selection());
            auto Record = FCkUiRecordData{};
            Record.Key = StableKey;
            Record.Fields.Add(TEXT("goal"), ck_inspector_anim_plans::TextField(Goal));
            Record.Fields.Add(TEXT("summary"), ck_inspector_anim_plans::TextField(Summary));
            Record.Fields.Add(TEXT("cluster"), ck_inspector_anim_plans::TextField(Cluster));
            Record.Fields.Add(TEXT("pending-cluster"), ck_inspector_anim_plans::TextField(PendingCluster));
            Record.Fields.Add(TEXT("pending-state"), ck_inspector_anim_plans::TextField(PendingState));
            Record.Fields.Add(TEXT("goal-visible"), ck_inspector_anim_plans::BoolField(bGoalVisible));
            Record.Fields.Add(TEXT("cluster-visible"), ck_inspector_anim_plans::BoolField(bClusterVisible));
            Record.Fields.Add(TEXT("pending-cluster-visible"), ck_inspector_anim_plans::BoolField(bPendingClusterVisible));
            Record.Fields.Add(TEXT("pending-state-visible"), ck_inspector_anim_plans::BoolField(bPendingStateVisible));
            Record.Fields.Add(TEXT("apply-visible"), ck_inspector_anim_plans::BoolField(bEditVisible && bApplyVisible));
            Record.Fields.Add(TEXT("edit-visible"), ck_inspector_anim_plans::BoolField(bEditVisible));
            Record.Fields.Add(TEXT("read-visible"), ck_inspector_anim_plans::BoolField(NOT bEditVisible));
            Record.Fields.Add(TEXT("goal-color"), ck_inspector_anim_plans::ColorField(_DiffLabels.Contains(Goal)));
            Record.Fields.Add(TEXT("cluster-color"), ck_inspector_anim_plans::ColorField(_DiffLabels.Contains(TEXT("  Cluster:"))));
            Record.Fields.Add(TEXT("pending-cluster-color"), ck_inspector_anim_plans::ColorField(_DiffLabels.Contains(TEXT("  State <- cluster:"))));
            Record.Fields.Add(TEXT("pending-state-color"), ck_inspector_anim_plans::ColorField(_DiffLabels.Contains(TEXT("  State <- state:"))));
            Record.Fields.Add(TEXT("apply-label"), ck_inspector_anim_plans::TextField(TEXT("Apply State")));
            Record.Fields.Add(TEXT("apply-tooltip"), ck_inspector_anim_plans::TextField(TEXT("Request_UpdateAnimState with the two staged tags above. Both must resolve to real gameplay tags.")));
            Records.Add(MoveTemp(Record));
        });
    }
    _PlansByKey = MoveTemp(LivePlans);
    for (auto It = _PendingClusters.CreateIterator(); It; ++It) { if (NOT LiveKeys.Contains(It.Key())) { It.RemoveCurrent(); } }
    for (auto It = _PendingStates.CreateIterator(); It; ++It) { if (NOT LiveKeys.Contains(It.Key())) { It.RemoveCurrent(); } }
    const FCkUiLoadResult PublishResult = _Plans->TrySetRecords(MoveTemp(Records));
    if (NOT PublishResult.Succeeded) { _LoadError = FString::Join(PublishResult.Errors, TEXT("\n")); return false; }
    return true;
}

auto SCkInspector_AnimPlansAuthored::Build_AuthoredView() -> bool
{
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT RegistryResult.Succeeded || NOT Registry.IsValid() || NOT Plugin.IsValid())
    {
        auto Errors = RegistryResult.Errors;
        if (NOT Registry.IsValid()) { Errors.Add(TEXT("Debugger UI registry is unavailable.")); }
        if (NOT Plugin.IsValid()) { Errors.Add(TEXT("CkDebugger plugin is unavailable.")); }
        _LoadError = FString::Join(Errors, TEXT("\n"));
        return false;
    }

    const TWeakPtr<SCkInspector_AnimPlansAuthored> WeakWidget{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    Data.Collections.Add(TEXT("anim-plans"), _Plans);
    Data.Visibility.Add(TEXT("anim-plans-visible"), TAttribute<bool>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_AnimPlansAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && Widget->Get_IsAvailable();
    }));
    Data.Visibility.Add(TEXT("anim-plans-edit-enabled"), TAttribute<bool>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_AnimPlansAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && Widget->Get_EditEnabled();
    }));
    Data.Text.Add(TEXT("anim-plans-disabled-reason"), TAttribute<FText>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_AnimPlansAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() ? FText::FromString(Widget->Get_DisabledReason()) : FText::GetEmpty();
    }));
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_AnimPlansAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && Widget->Get_IsAvailable();
    });
    Data.ItemActions.Add(TEXT("anim-plan-navigate"), FCkUiOnItemAction::CreateLambda([WeakWidget](const FString& Key)
    { if (const TSharedPtr<SCkInspector_AnimPlansAuthored> Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Navigate(Key); } }));
    Data.ItemActions.Add(TEXT("anim-plan-apply-state"), FCkUiOnItemAction::CreateLambda([WeakWidget](const FString& Key)
    { if (const TSharedPtr<SCkInspector_AnimPlansAuthored> Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Apply_State(Key); } }));
    const auto BindChanged = [&Data, WeakWidget](const FString& Name)
    {
        Data.ItemTextChanged.Add(Name, FCkUiOnItemTextChanged::CreateLambda([WeakWidget](const FString& Key, const FText Text)
        { if (const TSharedPtr<SCkInspector_AnimPlansAuthored> Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Handle_TextChanged(Key, Text); } }));
    };
    BindChanged(TEXT("anim-plan-cluster-changed"));
    BindChanged(TEXT("anim-plan-pending-cluster-changed"));
    BindChanged(TEXT("anim-plan-pending-state-changed"));
    Data.ItemTextCommitted.Add(TEXT("anim-plan-cluster-committed"), FCkUiOnItemTextCommitted::CreateLambda(
        [WeakWidget](const FString& Key, const FText Text, ETextCommit::Type)
        { if (const TSharedPtr<SCkInspector_AnimPlansAuthored> Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Commit_Cluster(Key, Text); } }));
    Data.ItemTextCommitted.Add(TEXT("anim-plan-pending-cluster-committed"), FCkUiOnItemTextCommitted::CreateLambda(
        [WeakWidget](const FString& Key, const FText Text, ETextCommit::Type)
        { if (const TSharedPtr<SCkInspector_AnimPlansAuthored> Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Stage_Cluster(Key, Text); } }));
    Data.ItemTextCommitted.Add(TEXT("anim-plan-pending-state-committed"), FCkUiOnItemTextCommitted::CreateLambda(
        [WeakWidget](const FString& Key, const FText Text, ETextCommit::Type)
        { if (const TSharedPtr<SCkInspector_AnimPlansAuthored> Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Stage_State(Key, Text); } }));

    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create({}, {}, {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString ResourceRoot = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(FPaths::Combine(ResourceRoot, TEXT("EcsInspectorAnimPlans.ui.html")),
        FPaths::Combine(ResourceRoot, TEXT("EcsInspectorAnimPlans.ui.css")));
    Candidate->PollFiles();
    if (NOT Candidate->GetLastResult().Succeeded)
    {
        _LoadError = FString::Join(Candidate->GetLastResult().Errors, TEXT("\n"));
        return false;
    }
    _View = Candidate;
    _Mounted = true;
    _LoadError.Reset();
    return true;
}

auto SCkInspector_AnimPlansAuthored::Handle_TextChanged(const FString& InStableKey, const FText&) -> void
{
    FCk_Handle_AnimPlan Plan;
    if (_EditScope.IsValid() && ResolvePlan(InStableKey, Plan)) { _EditScope->Set_Active(true); }
}

auto SCkInspector_AnimPlansAuthored::Commit_Cluster(const FString& InStableKey, const FText& InText) -> void
{
    if (_EditScope.IsValid()) { _EditScope->Set_Active(false); }
    const FGameplayTag ParsedTag = ck_inspector_anim_plans::ParseTag(InText);
    FCk_Handle_AnimPlan Plan;
    if (NOT ParsedTag.IsValid() || NOT ResolvePlan(InStableKey, Plan) || NOT Get_EditEnabled()) { return; }
    UCk_Utils_AnimPlan_UE::Request_UpdateAnimCluster(Plan, FCk_Request_AnimPlan_UpdateAnimCluster{ParsedTag}, {});
}

auto SCkInspector_AnimPlansAuthored::Stage_Cluster(const FString& InStableKey, const FText& InText) -> void
{
    if (_EditScope.IsValid()) { _EditScope->Set_Active(false); }
    const FGameplayTag ParsedTag = ck_inspector_anim_plans::ParseTag(InText);
    FCk_Handle_AnimPlan Plan;
    if (NOT ParsedTag.IsValid() || NOT ResolvePlan(InStableKey, Plan) || NOT Get_EditEnabled()) { return; }
    _PendingClusters.FindOrAdd(InStableKey) = ParsedTag;
}

auto SCkInspector_AnimPlansAuthored::Stage_State(const FString& InStableKey, const FText& InText) -> void
{
    if (_EditScope.IsValid()) { _EditScope->Set_Active(false); }
    const FGameplayTag ParsedTag = ck_inspector_anim_plans::ParseTag(InText);
    FCk_Handle_AnimPlan Plan;
    if (NOT ParsedTag.IsValid() || NOT ResolvePlan(InStableKey, Plan) || NOT Get_EditEnabled()) { return; }
    _PendingStates.FindOrAdd(InStableKey) = ParsedTag;
}

auto SCkInspector_AnimPlansAuthored::Navigate(const FString& InStableKey) -> void
{
    FCk_Handle_AnimPlan Plan;
    if (_SelectionModel.IsValid() && ResolvePlan(InStableKey, Plan))
    { _SelectionModel->Set_SelectedEntities({FCk_Handle{Plan}}); }
}

auto SCkInspector_AnimPlansAuthored::Apply_State(const FString& InStableKey) -> void
{
    FCk_Handle_AnimPlan Plan;
    const FGameplayTag* Cluster = _PendingClusters.Find(InStableKey);
    const FGameplayTag* State = _PendingStates.Find(InStableKey);
    if (NOT ResolvePlan(InStableKey, Plan) || NOT Get_EditEnabled() || Cluster == nullptr || State == nullptr
        || NOT Cluster->IsValid() || NOT State->IsValid()) { return; }
    UCk_Utils_AnimPlan_UE::Request_UpdateAnimState(Plan, FCk_Request_AnimPlan_UpdateAnimState{*Cluster, *State}, {});
}

auto SCkInspector_AnimPlansAuthored::Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    if (NOT _Active || NOT _View.IsValid()) { return; }
    if (NOT _EditScope.IsValid() || NOT _EditScope->Get_IsActive()) { Refresh_Records(); }
    _View->PollFiles();
    if (NOT _View->GetLastResult().Succeeded) { _LoadError = FString::Join(_View->GetLastResult().Errors, TEXT("\n")); }
    else if (_Plans.IsValid()) { _LoadError.Reset(); }
}

auto SCkInspector_AnimPlansAuthored::Release() -> void
{
    if (NOT _Active) { return; }
    _Active = false;
    if (_EditScope.IsValid()) { _EditScope->Set_Active(false); }
    _Entity = {};
    _SelectionModel.Reset();
    _EditScope.Reset();
    _PlansByKey.Reset();
    _PendingClusters.Reset();
    _PendingStates.Reset();
    _Plans.Reset();
    _View.Reset();
    _Mounted = false;
}

auto FCkInspector_AnimPlans::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Anim Plans"));
}

auto FCkInspector_AnimPlans::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity) && UCk_Utils_AnimPlan_UE::Has_Any(Entity);
}

auto FCkInspector_AnimPlans::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    return Build_Inspector(Entity, FString());
}

auto FCkInspector_AnimPlans::Build_Inspector(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> NativeBody = Build_NativeBody(Entity, InFilter);
    if (FCkInspector_RowCaptureScope::Is_Active()) { return NativeBody; }
    auto DiffLabels = TSet<FString>{};
    const auto CaptureDiff = [&DiffLabels](const FString& Label)
    { if (FCkInspector_DiffMarkScope::Is_LabelMarked(Label)) { DiffLabels.Add(Label); } };
    CaptureDiff(TEXT("  Cluster:"));
    CaptureDiff(TEXT("  State <- cluster:"));
    CaptureDiff(TEXT("  State <- state:"));
    CaptureDiff(TEXT("  "));
    if (ck::IsValid(Entity))
    {
        auto Mutable = Entity;
        UCk_Utils_AnimPlan_UE::ForEach_AnimPlan(Mutable, [&CaptureDiff](FCk_Handle_AnimPlan& Plan)
        {
            if (ck_inspector_anim_plans::IsPlanAvailable(Plan))
            {
                const FGameplayTag Goal = UCk_Utils_AnimPlan_UE::Get_AnimGoal(Plan).Get_AnimGoal();
                if (Goal.IsValid()) { CaptureDiff(Goal.ToString()); }
            }
        });
    }
    const TSharedRef<SCkInspector_AnimPlansAuthored> Authored = SNew(SCkInspector_AnimPlansAuthored)
        .Entity(Entity).Filter(InFilter).DiffLabels(MoveTemp(DiffLabels))
        .SelectionModel(SelectionModel).EditGuard(Get_EditGuard());
    if (NOT Authored->Is_Mounted())
    {
        _LastAuthoredLoadError = Authored->Get_LoadError();
        return NativeBody;
    }
    _LastAuthoredLoadError.Reset();
    _AuthoredInstances.Add(Authored);
    return Authored;
}

auto FCkInspector_AnimPlans::Build_NativeBody(const FCk_Handle& Entity, const FString& InFilter) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();
    Builder.SetEditGuard(Get_EditGuard());

    auto WeakSelectionModel = SelectionModel;

    auto MutableEntity = Entity;
    UCk_Utils_AnimPlan_UE::ForEach_AnimPlan(MutableEntity, [&Builder, WeakSelectionModel](FCk_Handle_AnimPlan& InAnimPlan)
    {
        const auto Goal = UCk_Utils_AnimPlan_UE::Get_AnimGoal(InAnimPlan);
        const auto Cluster = UCk_Utils_AnimPlan_UE::Get_AnimCluster(InAnimPlan);
        const auto State = UCk_Utils_AnimPlan_UE::Get_AnimState(InAnimPlan);

        const auto GoalTag = Goal.Get_AnimGoal();
        const auto GoalName = GoalTag.IsValid() ? GoalTag.ToString() : TEXT("Unknown");

        const auto AnimPlanHandle = FCk_Handle(InAnimPlan);

        Builder.AddClickableRow(
            FText::FromString(GoalName),
            [GoalTag](const FCk_Handle& E)
            {
                auto MutableE = E;
                const auto Plan = UCk_Utils_AnimPlan_UE::TryGet_AnimPlan(MutableE, GoalTag);
                if (ck::Is_NOT_Valid(Plan)) { return FText::GetEmpty(); }

                const auto PlanCluster = UCk_Utils_AnimPlan_UE::Get_AnimCluster(Plan);
                const auto PlanState = UCk_Utils_AnimPlan_UE::Get_AnimState(Plan);

                const auto ClusterTag = PlanCluster.Get_AnimCluster();
                const auto StateTag = PlanState.Get_AnimState();

                auto ClusterStr = ClusterTag.IsValid() ? ClusterTag.ToString() : TEXT("-");
                auto StateStr = StateTag.IsValid() ? StateTag.ToString() : TEXT("-");

                return FText::FromString(ck::Format_UE(TEXT("{} | {}"), ClusterStr, StateStr));
            },
            CkStyle::Value_Tag(),
            [WeakSelectionModel, AnimPlanHandle]()
            {
                if (WeakSelectionModel.IsValid() && ck::IsValid(AnimPlanHandle))
                {
                    WeakSelectionModel->Set_SelectedEntities({ AnimPlanHandle });
                }
            });

        // ---- Cluster / state entries ----
        // Both requests are LocalOk: an anim plan drives local presentation and re-derives from the
        // owning gameplay state, so a client-side nudge is a legitimate experiment.
        const auto CapturedPlan = InAnimPlan;

        // Cluster is a ONE-tag request, so committing the entry IS the write — no button needed.
        Builder.AddTagEntryRow(
            FText::FromString(TEXT("  Cluster:")),
            TAttribute<FText>::CreateLambda([CapturedPlan]()
            {
                if (ck::Is_NOT_Valid(CapturedPlan)) { return FText::FromString(TEXT("--")); }
                const auto Tag = UCk_Utils_AnimPlan_UE::Get_AnimCluster(CapturedPlan).Get_AnimCluster();
                return Tag.IsValid() ? FText::FromName(Tag.GetTagName()) : FText::FromString(TEXT("(none)"));
            }),
            [CapturedPlan](FGameplayTag InTag)
            {
                auto Mutable = CapturedPlan;
                if (ck::Is_NOT_Valid(Mutable)) { return; }

                UCk_Utils_AnimPlan_UE::Request_UpdateAnimCluster(Mutable,
                    FCk_Request_AnimPlan_UpdateAnimCluster{InTag}, {});
            });

        // State is a TWO-tag request (cluster + state), and a half-filled pair is not a request worth
        // firing — so the two entries stage into row-owned pendings and an explicit button commits the
        // pair. The pendings die with the row; a rebuild reseeds them from the live plan.
        const auto PendingCluster = MakeShared<FGameplayTag>(
            ck::IsValid(CapturedPlan) ? UCk_Utils_AnimPlan_UE::Get_AnimCluster(CapturedPlan).Get_AnimCluster() : FGameplayTag{});
        const auto PendingState = MakeShared<FGameplayTag>(
            ck::IsValid(CapturedPlan) ? UCk_Utils_AnimPlan_UE::Get_AnimState(CapturedPlan).Get_AnimState() : FGameplayTag{});

        Builder.AddTagEntryRow(
            FText::FromString(TEXT("  State <- cluster:")),
            TAttribute<FText>::CreateLambda([PendingCluster]()
            {
                return PendingCluster->IsValid() ? FText::FromName(PendingCluster->GetTagName()) : FText::FromString(TEXT("(none)"));
            }),
            [PendingCluster](FGameplayTag InTag) { *PendingCluster = InTag; });

        Builder.AddTagEntryRow(
            FText::FromString(TEXT("  State <- state:")),
            TAttribute<FText>::CreateLambda([PendingState]()
            {
                return PendingState->IsValid() ? FText::FromName(PendingState->GetTagName()) : FText::FromString(TEXT("(none)"));
            }),
            [PendingState](FGameplayTag InTag) { *PendingState = InTag; });

        Builder.AddActionRow(
            FText::FromString(TEXT("  ")),
            {
                FCkInspector_Action
                {
                    FText::FromString(TEXT("Apply State")),
                    FText::FromString(TEXT("Request_UpdateAnimState with the two staged tags above. Both must resolve to real gameplay tags.")),
                    [CapturedPlan, PendingCluster, PendingState]()
                    {
                        auto Mutable = CapturedPlan;
                        if (ck::Is_NOT_Valid(Mutable)) { return; }
                        if (NOT PendingCluster->IsValid() || NOT PendingState->IsValid()) { return; }

                        UCk_Utils_AnimPlan_UE::Request_UpdateAnimState(Mutable,
                            FCk_Request_AnimPlan_UpdateAnimState{*PendingCluster, *PendingState}, {});
                    }
                },
            });
    });

    return Builder.Build(Entity, InFilter);
}

auto FCkInspector_AnimPlans::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}

FCkInspector_AnimPlans::~FCkInspector_AnimPlans()
{
    OnDeactivated();
}

auto FCkInspector_AnimPlans::OnDeactivated() -> void
{
    for (const TWeakPtr<SCkInspector_AnimPlansAuthored>& WeakInstance : _AuthoredInstances)
    {
        if (const TSharedPtr<SCkInspector_AnimPlansAuthored> Instance = WeakInstance.Pin()) { Instance->Release(); }
    }
    _AuthoredInstances.Reset();
}
