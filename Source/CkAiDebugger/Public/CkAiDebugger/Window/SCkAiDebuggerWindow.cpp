#include "CkAiDebugger/Window/SCkAiDebuggerWindow.h"

#include "CkAiDebugger_Module.h"

#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Behavior/SCkDebug_BehaviorOverridePanel.h"
#include "CkDebuggerCommon/Lifecycle/CkDebug_SessionLifecycle.h"
#include "CkDebuggerCommon/Navigation/CkDebug_EntityTarget.h"
#include "CkDebuggerCommon/Navigation/CkDebug_SelectionSync.h"
#include "CkDebuggerCommon/Picker/CkDebug_ViewportPicker.h"
#include "CkDebuggerCommon/Picker/SCkDebug_ViewportPickerControls.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityDebuggerLinks.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EventLog.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EvidenceList.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NameDepthCycler.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NameLabel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Card.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StageStrip.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"
#include "CkDebuggerCommon/Utils/CkDebug_WorldSpeed.h"
#include "CkCrowdDebugger/Data/CkCrowdDebugger_Types.h"
#include "CkCrowdDebugger/ViewModel/CkCrowdDebugger_ViewModel.h"
#include "CkCrowdDebugger/Viewport/SCkCrowdDebugger_3dViewport.h"
#include "CkCrowd/Agent/CkCrowdAgent_Utils.h"
#include "CkEditorTools/Style/CkStyle.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEntityDebugOverlay/Layout/CkDebugOverlay_Layout.h"
#include "CkEntityDebugOverlay/Presentation/CkDebugOverlay_Present.h"
#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Settings/CkDebugOverlay_Settings.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Misc/App.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SCkAiDebuggerWindow"

const FName SCkAiDebuggerWindow::WindowId{TEXT("AiDebugger")};

namespace ck_ai_debugger_window
{
    constexpr auto RefreshInterval = 0.10;

    auto IsAiProvider(const FGameplayTag& InProvider) -> bool;

    auto MakeAiBuildOptions() -> ck_debugoverlay::FCk_DebugOverlay_EntityModelBuildOptions
    {
        auto Options = ck_debugoverlay::FCk_DebugOverlay_EntityModelBuildOptions{};
        Options.bCondensePerSourceSections = false;
        Options.bRetainEmptySourceSections = true;
        return Options;
    }

    auto MakeAiLayout(const TArray<TSharedPtr<ICk_DebugOverlay_Provider>>& InProviders) -> FCk_DebugOverlay_Layout
    {
        const auto* Settings = GetDefault<UCk_DebugOverlay_Settings>();
        for (const auto& Layout : Settings->Layouts)
        {
            if (Layout.LayoutTag == TAG_Ck_OnScreenDebugger_Layout_AI)
            { return Layout; }
        }

        // The project's authored AI layout is authoritative. This fallback exists only for projects
        // that have not authored it yet, and still deliberately collects only AI providers.
        FCk_DebugOverlay_Layout Layout;
        Layout.LayoutTag = TAG_Ck_OnScreenDebugger_Layout_AI;
        for (const auto& Provider : InProviders)
        {
            if (Provider.IsValid() && IsAiProvider(Provider->Get_ProviderTag()))
            { Layout.EnabledProviders.AddTag(Provider->Get_ProviderTag()); }
        }
        return Layout;
    }

    auto IsAiProvider(const FGameplayTag& InProvider) -> bool
    {
        const FString Name = InProvider.ToString();
        return Name.Contains(TEXT(".Goap"))
            || Name.Contains(TEXT(".StateMachine"))
            || Name.Contains(TEXT(".Crowd"))
            || Name.Contains(TEXT(".AStar"))
            || Name.Contains(TEXT(".PathNetwork"));
    }

    auto FindStageSection(
        const FCk_DebugOverlay_EntityModel& InModel,
        const TArray<FString>& InTokens) -> const FCk_DebugOverlay_Section*
    {
        for (const auto& Section : InModel.Sections)
        {
            const auto Provider = Section.ProviderTag.ToString();
            for (const auto& Token : InTokens)
            {
                if (Provider.Contains(Token) && NOT Section.Rows.IsEmpty())
                { return &Section; }
            }
        }
        return nullptr;
    }

    auto GetStageValue(
        const FCk_DebugOverlay_EntityModel& InModel,
        const TArray<FString>& InTokens,
        int32 InRowIndex) -> FText
    {
        const auto* Section = FindStageSection(InModel, InTokens);
        return Section != nullptr && Section->Rows.IsValidIndex(InRowIndex)
            ? Section->Rows[InRowIndex].Value
            : LOCTEXT("StageUnavailable", "—");
    }

    auto GetStageTone(
        const FCk_DebugOverlay_EntityModel& InModel,
        const TArray<FString>& InTokens) -> ECk_Tone
    {
        const auto* Section = FindStageSection(InModel, InTokens);
        const auto Severity = Section == nullptr
            ? ECk_DebugOverlay_Severity::Normal
            : ck_debugoverlay::Get_MaxSeverity(Section->Rows);
        switch (Severity)
        {
            case ECk_DebugOverlay_Severity::Good: return ECk_Tone::Ok;
            case ECk_DebugOverlay_Severity::Warn: return ECk_Tone::Warn;
            case ECk_DebugOverlay_Severity::Bad: return ECk_Tone::Err;
            default: return ECk_Tone::Neutral;
        }
    }

    /**
     * The AI subset of the provider registry.
     *
     * Only Get_ProviderTag() and CanProvide() are called through the result, and both are const on
     * ICk_DebugOverlay_Provider — the mutating Collect() path keeps building a fresh CreateAll().
     * The picker asks this question for every candidate entity on every marker gather, which a
     * per-candidate CreateAll() (~25 shared allocations plus a sort) cannot absorb. The window owns
     * the instances so they never outlive the module that supplied their vtables.
     */
    auto Collect_AiProviders() -> TArray<TSharedPtr<ICk_DebugOverlay_Provider>>
    {
        auto Result = TArray<TSharedPtr<ICk_DebugOverlay_Provider>>{};
        for (const auto& Provider : FCk_DebugOverlay_Registry::Get().CreateAll())
        {
            if (Provider.IsValid() && IsAiProvider(Provider->Get_ProviderTag()))
            { Result.Add(Provider); }
        }
        return Result;
    }

    auto GetCrowdStatus(ECkCrowdDebugger_AgentStatus InStatus) -> FText
    {
        switch (InStatus)
        {
            case ECkCrowdDebugger_AgentStatus::Idle: return LOCTEXT("CrowdIdle", "IDLE");
            case ECkCrowdDebugger_AgentStatus::Walking: return LOCTEXT("CrowdWalking", "MOVING");
            case ECkCrowdDebugger_AgentStatus::Asleep: return LOCTEXT("CrowdAsleep", "ASLEEP");
            case ECkCrowdDebugger_AgentStatus::Replanning: return LOCTEXT("CrowdReplanning", "REPLAN");
            case ECkCrowdDebugger_AgentStatus::Failed: return LOCTEXT("CrowdFailed", "FAILED");
            case ECkCrowdDebugger_AgentStatus::PlayerProxy: return LOCTEXT("CrowdPlayer", "PLAYER");
            default: return LOCTEXT("CrowdUnknown", "UNKNOWN");
        }
    }

    auto TextField(const FText& InValue) -> FCkUiFieldValue
    { return FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = InValue}; }

    auto ColorField(const FLinearColor& InValue) -> FCkUiFieldValue
    { return FCkUiFieldValue{.Kind = ECkUiFieldKind::Color, .Color = InValue}; }

    auto AiRosterSchema() -> TArray<FCkUiFieldSchema>
    {
        return {
            {TEXT("ai-roster-name"), ECkUiFieldKind::Text},
            {TEXT("ai-roster-status"), ECkUiFieldKind::Text},
            {TEXT("ai-roster-status-color"), ECkUiFieldKind::Color},
            {TEXT("ai-roster-status-background"), ECkUiFieldKind::Color},
            {TEXT("ai-roster-summary"), ECkUiFieldKind::Text},
            {TEXT("ai-roster-context"), ECkUiFieldKind::Text},
        };
    }

    auto AiRosterKey(const int64 InGeneration, const FCk_Handle& InPhysicalHandle) -> FString
    {
        const FCk_Entity& Entity = InPhysicalHandle.Get_Entity();
        return FString::Printf(TEXT("ai-roster:%lld:%d:%d"), InGeneration,
            static_cast<int32>(Entity.Get_EntityNumber()), static_cast<int32>(Entity.Get_VersionNumber()));
    }

    auto AiRosterStyleTokens() -> FCkUiView::FTokens
    {
        const auto Color = [](const FLinearColor& InColor) { return TEXT("#") + InColor.ToFColorSRGB().ToHex(); };
        return {
            {TEXT("--ai-roster-row-font-size"), FString::FromInt(ck::debug_axes::Get_ScaledFontSize(CkStyle::FontSizeSmall()))},
            {TEXT("--ai-roster-detail-font-size"), FString::FromInt(ck::debug_axes::Get_ScaledFontSize(CkStyle::FontSizeMicro()))},
            {TEXT("--ai-roster-text"), Color(CkStyle::Text())},
            {TEXT("--ai-roster-text-dim"), Color(CkStyle::TextDim())},
            {TEXT("--ai-roster-text-mute"), Color(CkStyle::TextMute())},
            {TEXT("--ai-roster-text-strong"), Color(CkStyle::TextStrong())},
        };
    }

    auto ShellTokens() -> FCkUiView::FTokens
    {
        return {{TEXT("--ai-shell-surface"), TEXT("#") + CkStyle::Bg2().ToFColorSRGB().ToHex()}};
    }
}

auto SCkAiDebuggerWindow::Construct(const FArguments&) -> void
{
    Register_WithGate();

    const FCkUiLoadResult CollectionResult = FCkUiCollection::TryCreate(
        ck_ai_debugger_window::AiRosterSchema(), _AiRosterCollection);

    _AiProviders = ck_ai_debugger_window::Collect_AiProviders();

    _ViewportPicker = MakeShared<FCkDebug_ViewportPicker>();
    auto PickerParams = FCkDebug_ViewportPicker::FParams{};
    PickerParams.Get_TargetWorld = [WeakThis = TWeakPtr<SCkAiDebuggerWindow>(SharedThis(this))]() -> UWorld*
    { const auto Pinned = WeakThis.Pin(); return Pinned.IsValid() ? Pinned->Get_TargetWorld() : nullptr; };
    PickerParams.TargetFilter = [WeakThis = TWeakPtr<SCkAiDebuggerWindow>(SharedThis(this))](const FCk_Handle& InEntity)
    { const auto Pinned = WeakThis.Pin(); return Pinned.IsValid() && Pinned->Is_AiPickCandidate(InEntity); };
    PickerParams.OnEntityPicked = [WeakThis = TWeakPtr<SCkAiDebuggerWindow>(SharedThis(this))](const FCk_Handle& InEntity)
    { if (const auto Pinned = WeakThis.Pin()) { Pinned->Select_Entity(InEntity, true); } };
    _ViewportPicker->Construct(MoveTemp(PickerParams));

    _CrowdViewModel = MakeShared<FCkCrowdDebugger_ViewModel>();
    const auto WeakWindow = TWeakPtr<SCkAiDebuggerWindow>{SharedThis(this)};
    _SpatialViewport = SNew(SCkCrowdDebugger_3dViewport)
        .OnAgentPicked_Lambda([WeakWindow](int32 InAgentIndex)
        {
            const auto Pinned = WeakWindow.Pin();
            if (NOT Pinned.IsValid() || NOT Pinned->_CrowdViewModel.IsValid())
            { return; }
            const auto& Agents = Pinned->_CrowdViewModel->Get_AllAgents();
            if (Agents.IsValidIndex(InAgentIndex))
            {
                Pinned->Select_Entity(Agents[InAgentIndex].Handle, true);
            }
        });

    _SessionInvalidatedHandle = ck::DebugSessionLifecycle::Get_OnSessionInvalidated().AddSP(
        this, &SCkAiDebuggerWindow::HandleSessionInvalidated);
    _WorldInvalidatedHandle = ck::DebugSessionLifecycle::Get_OnWorldInvalidated().AddSP(
        this, &SCkAiDebuggerWindow::HandleWorldInvalidated);

    ChildSlot
    [
        SNew(SCkDebug_WindowChrome)
            .WindowId(Get_WindowId())
            .ToolTabId(FCkAiDebuggerModule::Get_TabName())
            .StatusText_Lambda([this]() { return Get_StatusText(); })
            .ShowRefreshControls(true)
            .CommonActionsContent()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(SCkDebug_ViewportPickerControls)
                        .Picker(_ViewportPicker)
                        .PickTooltip(LOCTEXT("PickAiTooltip", "Pick an entity that has GOAP, State Machine, Crowd, or navigation evidence."))
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(CkStyle::SpaceS, 0.0f, 0.0f, 0.0f)
                [
                    SNew(SCkDebug_NameDepthCycler)
                        .Depth_Lambda([this]() { return _NameDepth; })
                        .MaxDepth_Lambda([this]() { return Get_MaxNameDepth(); })
                        .OnDepthChanged(FOnCkDebug_NameDepthChanged::CreateLambda([this](int32 InDepth)
                        { _NameDepth = InDepth; }))
                ]
            ]
            .Content()
            [
                SAssignNew(_AuthoredShellHost, SBox)
            ]
    ];

    Build_AuthoredShell();

    if (CollectionResult.Succeeded) { Build_AiRosterView(); }
    else if (_AiRosterHost.IsValid())
    {
        _AiRosterLoadError = FString::Join(CollectionResult.Errors, TEXT("\n"));
        _AiRosterHost->SetContent(SNew(STextBlock).Text(FText::FromString(
            _AiRosterLoadError)));
    }
}

SCkAiDebuggerWindow::~SCkAiDebuggerWindow()
{
    if (_SessionInvalidatedHandle.IsValid())
    { ck::DebugSessionLifecycle::Get_OnSessionInvalidated().Remove(_SessionInvalidatedHandle); }
    if (_WorldInvalidatedHandle.IsValid())
    { ck::DebugSessionLifecycle::Get_OnWorldInvalidated().Remove(_WorldInvalidatedHandle); }
    if (_ViewportPicker.IsValid()) { _ViewportPicker->Deactivate(); }
    _AuthoredShellView.Reset();
    _AiProviders.Reset();
    Clear_Diagnostics();
    // FCk_Handle is deliberately released while ECS registries still exist.
    _SelectedEntity = FCk_Handle{};
    _TrackedRoster.Reset();
    _Model = FCk_DebugOverlay_EntityModel{};
    Invalidate_AiRosterView();
    if (_CrowdViewModel.IsValid()) { _CrowdViewModel->Reset_ForWorldChange(); }
}

auto SCkAiDebuggerWindow::Tick(const FGeometry& InGeometry, double InNow, float InDeltaSeconds) -> void
{
    TRACE_CPUPROFILER_EVENT_SCOPE(CkAiDbg_WindowTick);
    SCkDebugger_WindowBase::Tick(InGeometry, InNow, InDeltaSeconds);
    Poll_AuthoredShell();
    if (_ViewportPicker.IsValid()) { _ViewportPicker->Tick(InDeltaSeconds); }

    auto* World = Get_TargetWorld();
    if (World != _AiRosterWorld.Get())
    {
        Invalidate_AiRosterView();
        _AiRosterWorld = World;
        Build_AiRosterView();
    }
    if (ck::IsValid(World))
    { _ActiveWorld = World; }
    if (_CrowdViewModel.IsValid())
    {
        {
            TRACE_CPUPROFILER_EVENT_SCOPE(CkAiDbg_CrowdViewModelTick);
            _CrowdViewModel->Tick(World, InDeltaSeconds);
        }
        if (_SpatialViewport.IsValid())
        {
            _SpatialViewport->Set_NavmeshTriangles(
                _CrowdViewModel->Get_NavTriVerts(), _CrowdViewModel->Get_NavGeometryRevision());
            _SpatialViewport->Set_AgentSnapshots(
                _CrowdViewModel->Get_AllAgents(), _CrowdViewModel->Get_SelectedHandle());
            _SpatialViewport->Set_PathNetworkRibbons(_CrowdViewModel->Get_PathNetworkRibbons());
            _SpatialViewport->Set_QueueSnapshots(_CrowdViewModel->Get_Queues());
        }
        {
            TRACE_CPUPROFILER_EVENT_SCOPE(CkAiDbg_RefreshRoster);
            Refresh_Roster();
        }
    }
    Poll_AiRosterFiles(InNow);
    if (InNow - _LastRefreshTime < ck_ai_debugger_window::RefreshInterval) { return; }
    _LastRefreshTime = InNow;

    if (ck::Is_NOT_Valid(_SelectedEntity))
    {
        Clear_Diagnostics();
        _SelectedEntity = {};
        _Model = {};
        if (_CrowdViewModel.IsValid())
        { _CrowdViewModel->Set_SelectedHandle({}); }
        return;
    }
    const auto Updated = Build_Model(_SelectedEntity, InNow);
    if (NOT Is_AiModel(Updated) && NOT Has_ActiveRosterSelection())
    {
        Clear_Diagnostics();
        _SelectedEntity = FCk_Handle{};
        _Model = {};
        Refresh_Roster();
        return;
    }
    _Model = Updated;
    Refresh_Diagnostics(InNow);
}

auto SCkAiDebuggerWindow::Is_AiEntity(const FCk_Handle& InEntity) -> bool
{
    if (ck::Is_NOT_Valid(InEntity)) { return false; }
    const auto Providers = FCk_DebugOverlay_Registry::Get().CreateAll();
    return Is_AiModel(ck_debugoverlay::Build_EntityModel(InEntity, Providers,
        ck_ai_debugger_window::MakeAiLayout(Providers), nullptr, FPlatformTime::Seconds(),
        ck_ai_debugger_window::MakeAiBuildOptions()));
}

auto SCkAiDebuggerWindow::Is_AiPickCandidate(const FCk_Handle& InEntity) const -> bool
{
    if (ck::Is_NOT_Valid(InEntity)) { return false; }

    for (const auto& Provider : _AiProviders)
    {
        if (Provider.IsValid() && Provider->CanProvide(InEntity))
        { return true; }
    }
    return false;
}

auto SCkAiDebuggerWindow::OpenForEntity(const FCk_Handle& InEntity) -> void
{
    if (NOT Is_AiEntity(InEntity)) { return; }
    auto& Module = FCkAiDebuggerModule::Get();
    Module.OpenDebugger();
    if (const auto Window = Module.Get_DebuggerWindow())
    { Window->Select_Entity(InEntity, false); }
}

auto SCkAiDebuggerWindow::Select_Entity(const FCk_Handle& InEntity, bool InBroadcast) -> void
{ Select_EntityImpl(InEntity, InBroadcast, false); }

auto SCkAiDebuggerWindow::Select_EntityImpl(
    const FCk_Handle& InEntity,
    const bool InBroadcast,
    const bool InAllowDirectCrowdAgent) -> void
{
    if (ck::Is_NOT_Valid(InEntity)) { return; }
    const auto Target = ck::DebugSelectionSync::Resolve_ConceptualTarget(InEntity);
    const bool bDirectCrowdAgent = InAllowDirectCrowdAgent && UCk_Utils_CrowdAgent_UE::Has(InEntity);
    if (ck::Is_NOT_Valid(Target) || (NOT bDirectCrowdAgent && NOT Is_AiEntity(Target))) { return; }

    auto* TargetWorld = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(Target);
    const auto TargetWorldIsValid = ck::IsValid(TargetWorld);
    CK_ENSURE_IF_NOT(TargetWorldIsValid,
        TEXT("AI Overview cannot select entity [{}] without a valid owning world"),
        Target)
    { return; }

    const auto EntityChanged = _SelectedEntity != Target;
    if (EntityChanged) { Clear_Diagnostics(); }
    _ActiveWorld = TargetWorld;
    _SelectedEntity = Target;
    const auto Now = FPlatformTime::Seconds();
    _Model = Build_Model(Target, Now);
    if (NOT _TrackedRoster.ContainsByPredicate([&Target](const FCk_Handle& Existing) { return Existing == Target; }))
    { _TrackedRoster.Insert(Target, 0); }
    if (_TrackedRoster.Num() > 24) { _TrackedRoster.SetNum(24); }
    if (_CrowdViewModel.IsValid())
    {
        auto PreferredAgent = FCk_Handle{};
        const auto ExistingAgent = _CrowdViewModel->Get_SelectedHandle();
        for (const auto& Agent : _CrowdViewModel->Get_AllAgents())
        {
            if (Agent.Handle == InEntity)
            {
                PreferredAgent = Agent.Handle;
                break;
            }
            if (Agent.Handle == ExistingAgent
                && ck::DebugSelectionSync::Resolve_ConceptualTarget(Agent.Handle) == Target)
            { PreferredAgent = Agent.Handle; }
            else if (ck::Is_NOT_Valid(PreferredAgent)
                && ck::DebugSelectionSync::Resolve_ConceptualTarget(Agent.Handle) == Target)
            { PreferredAgent = Agent.Handle; }
        }
        _CrowdViewModel->Set_SelectedHandle(PreferredAgent);
    }
    Refresh_Roster();
    Refresh_Diagnostics(Now);
    if (InBroadcast) { ck::DebugSelectionSync::Broadcast(Target, FCkAiDebuggerModule::Get_TabName()); }
}

auto SCkAiDebuggerWindow::Has_ActiveRosterSelection() const -> bool
{
    if (NOT _CrowdViewModel.IsValid()) { return false; }
    const FCk_Handle PhysicalAgent = _CrowdViewModel->Get_SelectedHandle();
    return ck::IsValid(PhysicalAgent) && UCk_Utils_CrowdAgent_UE::Has(PhysicalAgent)
        && ck::DebugSelectionSync::Resolve_ConceptualTarget(PhysicalAgent) == _SelectedEntity;
}

auto SCkAiDebuggerWindow::Is_AiModel(const FCk_DebugOverlay_EntityModel& InModel) -> bool
{
    return InModel.Sections.ContainsByPredicate([](const FCk_DebugOverlay_Section& Section)
    { return ck_ai_debugger_window::IsAiProvider(Section.ProviderTag); });
}

auto SCkAiDebuggerWindow::Build_Model(const FCk_Handle& InEntity, double InNow) -> FCk_DebugOverlay_EntityModel
{
    TRACE_CPUPROFILER_EVENT_SCOPE(CkAiDbg_BuildModel);
    const auto EntityIsValid = ck::IsValid(InEntity);
    CK_ENSURE_IF_NOT(EntityIsValid, TEXT("AI Overview cannot build a model for an invalid entity"))
    { return {}; }

    const auto Providers = FCk_DebugOverlay_Registry::Get().CreateAll();
    return ck_debugoverlay::Build_EntityModel(InEntity, Providers,
        ck_ai_debugger_window::MakeAiLayout(Providers), &_History, InNow,
        ck_ai_debugger_window::MakeAiBuildOptions());
}

auto SCkAiDebuggerWindow::Build_AuthoredShell() -> void
{
    const TArray<TPair<FName, TSharedRef<SWidget>>> Panes{
        {TEXT("Roster"), Build_RosterPanel()},
        {TEXT("Identity"), Build_IdentityPanel()},
        {TEXT("Behavior"), Build_BehaviorPanel()},
        {TEXT("Drill"), Build_DrillPanel()},
        {TEXT("Stage"), Build_StagePanel()},
        {TEXT("Goap"), Build_TopologyPanel(true)},
        {TEXT("StateMachine"), Build_TopologyPanel(false)},
        {TEXT("Evidence"), Build_CurrentEvidencePanel()},
        {TEXT("Events"), Build_EventLogPanel()},
        {TEXT("Spatial"), Build_SpatialPanel()},
    };

    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT RegistryResult.Succeeded || NOT Registry.IsValid() || NOT Plugin.IsValid())
    {
        _AuthoredShellLoadError = RegistryResult.Succeeded
            ? TEXT("CkDebugger plugin is unavailable.")
            : FString::Join(RegistryResult.Errors, TEXT("\n"));
        _AuthoredShellHost->SetContent(Build_NativeShellFallback(Panes));
        return;
    }

    FCkUiView::FNativeBindings NativeBindings;
    NativeBindings.Add(TEXT("ai-roster-pane"), Panes[0].Value);
    NativeBindings.Add(TEXT("ai-identity-pane"), Panes[1].Value);
    NativeBindings.Add(TEXT("ai-behavior-pane"), Panes[2].Value);
    NativeBindings.Add(TEXT("ai-drill-pane"), Panes[3].Value);
    NativeBindings.Add(TEXT("ai-stage-pane"), Panes[4].Value);
    NativeBindings.Add(TEXT("ai-goap-pane"), Panes[5].Value);
    NativeBindings.Add(TEXT("ai-state-machine-pane"), Panes[6].Value);
    NativeBindings.Add(TEXT("ai-evidence-pane"), Panes[7].Value);
    NativeBindings.Add(TEXT("ai-events-pane"), Panes[8].Value);
    NativeBindings.Add(TEXT("ai-spatial-pane"), Panes[9].Value);

    auto Data = FCkUiView::FDataBindings{};
    const TWeakPtr<SCkAiDebuggerWindow> WeakWindow{SharedThis(this)};
    Data.SlateUserIndex = 0;
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakWindow]() { return WeakWindow.IsValid(); });

    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        MoveTemp(NativeBindings), {}, ck_ai_debugger_window::ShellTokens(),
        CkStyle::RegularFont(CkStyle::FontSizeBody()), MoveTemp(Data), Registry);
    const TSharedRef<SWidget> Main = Candidate->GetRegion(TEXT("main"));
    const FString ResourceRoot = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(
        FPaths::Combine(ResourceRoot, TEXT("AiDebuggerShell.ui.html")),
        FPaths::Combine(ResourceRoot, TEXT("AiDebuggerShell.ui.css")));
    Candidate->PollFiles();
    if (NOT Candidate->GetLastResult().Succeeded)
    {
        _AuthoredShellLoadError = FString::Join(Candidate->GetLastResult().Errors, TEXT("\n"));
        _AuthoredShellHost->SetContent(Build_NativeShellFallback(Panes));
        return;
    }

    _AuthoredShellView = Candidate;
    _AuthoredShellMounted = true;
    _AuthoredShellLoadError.Reset();
    _AuthoredShellHost->SetContent(Main);
}

auto SCkAiDebuggerWindow::Build_NativeShellFallback(
    const TArray<TPair<FName, TSharedRef<SWidget>>>& InPanes) -> TSharedRef<SWidget>
{
    return SNew(SSplitter)
        .Orientation(Orient_Horizontal).PhysicalSplitterHandleSize(5.0f)
        + SSplitter::Slot().Value(0.20f)[InPanes[0].Value]
        + SSplitter::Slot().Value(0.80f)
        [
            SNew(SSplitter)
            .Orientation(Orient_Vertical).PhysicalSplitterHandleSize(5.0f)
            + SSplitter::Slot().Value(0.52f)
            [
                SNew(SSplitter)
                .Orientation(Orient_Vertical).PhysicalSplitterHandleSize(5.0f)
                + SSplitter::Slot().Value(0.46f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().FillHeight(1.0f)
                    [
                        SNew(SSplitter)
                        .Orientation(Orient_Horizontal).PhysicalSplitterHandleSize(5.0f)
                        + SSplitter::Slot().Value(0.38f)[InPanes[1].Value]
                        + SSplitter::Slot().Value(0.34f)[InPanes[2].Value]
                        + SSplitter::Slot().Value(0.28f)[InPanes[3].Value]
                    ]
                    + SVerticalBox::Slot().AutoHeight()[InPanes[4].Value]
                ]
                + SSplitter::Slot().Value(0.54f)
                [
                    SNew(SSplitter)
                    .Orientation(Orient_Horizontal).PhysicalSplitterHandleSize(5.0f)
                    + SSplitter::Slot().Value(0.50f)[InPanes[5].Value]
                    + SSplitter::Slot().Value(0.50f)[InPanes[6].Value]
                ]
            ]
            + SSplitter::Slot().Value(0.48f)
            [
                SNew(SSplitter)
                .Orientation(Orient_Horizontal).PhysicalSplitterHandleSize(5.0f)
                + SSplitter::Slot().Value(0.42f)
                [
                    SNew(SSplitter)
                    .Orientation(Orient_Vertical).PhysicalSplitterHandleSize(5.0f)
                    + SSplitter::Slot().Value(0.58f)[InPanes[7].Value]
                    + SSplitter::Slot().Value(0.42f)[InPanes[8].Value]
                ]
                + SSplitter::Slot().Value(0.58f)[InPanes[9].Value]
            ]
        ];
}

auto SCkAiDebuggerWindow::Poll_AuthoredShell() -> void
{
    if (NOT _AuthoredShellView.IsValid()) { return; }
    _AuthoredShellView->PollFiles(ck_ai_debugger_window::ShellTokens());
    if (NOT _AuthoredShellView->GetLastResult().Succeeded)
    {
        _AuthoredShellLoadError = FString::Join(_AuthoredShellView->GetLastResult().Errors, TEXT("\n"));
        return;
    }
    _AuthoredShellLoadError.Reset();
}

auto SCkAiDebuggerWindow::Build_RosterPanel() -> TSharedRef<SWidget>
{
    return SNew(SCkDebug_Card).BodyPadding(FMargin{CkStyle::SpaceM})
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [SNew(SCkDebug_SectionHeader).Label(LOCTEXT("Roster", "NPC health")).Underline(true)]
            + SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, CkStyle::SpaceS)
            [SAssignNew(_AiRosterHost, SBox)]
        ];
}

auto SCkAiDebuggerWindow::Build_IdentityPanel() -> TSharedRef<SWidget>
{
    return SNew(SCkDebug_Card).BodyPadding(FMargin{CkStyle::SpaceM})
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()[SNew(SCkDebug_SectionHeader).Label(LOCTEXT("Selected", "Selected entity")).Underline(true)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS)
        [SNew(SCkDebug_EntityRef).Entity_Lambda([this]() { return _SelectedEntity; }).ShowName(true)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS)
        [SNew(SCkDebug_SelectableLabel).Text_Lambda([this]() { return _Model.Header; }).ColorAndOpacity(CkStyle::TextDim())]
    ];
}

auto SCkAiDebuggerWindow::Build_BehaviorPanel() -> TSharedRef<SWidget>
{
    return SNew(SCkDebug_Card).BodyPadding(FMargin{CkStyle::SpaceM})
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [SNew(SCkDebug_SectionHeader).Label(LOCTEXT("BehaviorOverrides", "Behavior controls")).Underline(true)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS)
        [SNew(SCkDebug_BehaviorOverridePanel)]
    ];
}

auto SCkAiDebuggerWindow::Build_DrillPanel() -> TSharedRef<SWidget>
{
    return SNew(SCkDebug_Card).BodyPadding(FMargin{CkStyle::SpaceM})
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [SNew(SCkDebug_SectionHeader).Label(LOCTEXT("DrillInto", "Drill into")).Underline(true)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS)
        [SNew(SCkDebug_EntityDebuggerLinks).Entity_Lambda([this]() { return _SelectedEntity; }).ExcludeTabId(FCkAiDebuggerModule::Get_TabName())]
    ];
}

auto SCkAiDebuggerWindow::Build_StagePanel() -> TSharedRef<SWidget>
{
    auto Stages = TArray<FCkDebug_StageDescriptor>{};
    const auto AddStage = [this, &Stages](
        FText InLabel,
        ECk_Icon InIcon,
        std::initializer_list<const TCHAR*> InTokens)
    {
        auto Tokens = TArray<FString>{};
        for (const auto* Token : InTokens) { Tokens.Add(Token); }
        Stages.Add(FCkDebug_StageDescriptor{
            MoveTemp(InLabel),
            InIcon,
            TAttribute<FText>::CreateLambda([this, Tokens]
            {
                return ck_ai_debugger_window::GetStageValue(_Model, Tokens, 0);
            }),
            TAttribute<FText>::CreateLambda([this, Tokens]
            {
                return ck_ai_debugger_window::GetStageValue(_Model, Tokens, 1);
            }),
            TAttribute<ECk_Tone>::CreateLambda([this, Tokens]
            {
                return ck_ai_debugger_window::GetStageTone(_Model, Tokens);
            })});
    };

    AddStage(LOCTEXT("IntentStage", "Intent"), ECk_Icon::Objective, {TEXT(".Objective"), TEXT(".InteractTarget")});
    AddStage(LOCTEXT("GoapStage", "GOAP"), ECk_Icon::Goap, {TEXT(".Goap")});
    AddStage(LOCTEXT("StateStage", "State"), ECk_Icon::StateMachine, {TEXT(".StateMachine")});
    AddStage(LOCTEXT("NavigationStage", "Navigation"), ECk_Icon::PathNetwork, {TEXT(".PathNetwork"), TEXT(".AStar")});
    AddStage(LOCTEXT("CrowdStage", "Crowd"), ECk_Icon::Crowd, {TEXT(".Crowd")});

    return SNew(SCkDebug_Card).BodyPadding(FMargin{CkStyle::SpaceM})
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()[SNew(SCkDebug_SectionHeader).Label(LOCTEXT("DecisionMotion", "Decision → motion")).SubText(LOCTEXT("ModelSource", "overlay provider model")).Underline(true)]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS)
        [SNew(SCkDebug_StageStrip).Stages(MoveTemp(Stages))]
    ];
}

auto SCkAiDebuggerWindow::Build_TopologyPanel(const bool InGoap) -> TSharedRef<SWidget>
{
    const FText Heading = InGoap ? LOCTEXT("GoapTopology", "GOAP hierarchy")
                                 : LOCTEXT("StateMachineTopology", "State Machine hierarchy");
    const FText Empty = InGoap ? LOCTEXT("NoGoapTopology", "No GOAP instances reported.")
                               : LOCTEXT("NoStateMachineTopology", "No State Machine instances reported.");
    TSharedPtr<SCkDebug_EvidenceList> List;
    const TSharedRef<SWidget> Panel = SNew(SCkDebug_Card).BodyPadding(FMargin{CkStyle::SpaceM})
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [SNew(SCkDebug_SectionHeader).Label(Heading).Underline(true)]
        + SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, CkStyle::SpaceS)
        [SAssignNew(List, SCkDebug_EvidenceList).MaxItems(100).EmptyText(Empty)]
    ];
    if (InGoap) { _GoapTopologyList = List; }
    else { _StateMachineTopologyList = List; }
    return Panel;
}

auto SCkAiDebuggerWindow::Build_CurrentEvidencePanel() -> TSharedRef<SWidget>
{
    return SNew(SCkDebug_Card).BodyPadding(FMargin{CkStyle::SpaceM})
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [SNew(SCkDebug_SectionHeader).Label(LOCTEXT("CurrentEvidence", "Current evidence")).Underline(true)]
        + SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, CkStyle::SpaceS)
        [
            SAssignNew(_CurrentEvidenceList, SCkDebug_EvidenceList)
            .MaxItems(200)
            .EmptyText(LOCTEXT("NoCurrentEvidence", "Select or pick an AI entity to inspect its current evidence."))
        ]
    ];
}

auto SCkAiDebuggerWindow::Build_EventLogPanel() -> TSharedRef<SWidget>
{
    return SNew(SCkDebug_Card).BodyPadding(FMargin{CkStyle::SpaceM})
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [SNew(SCkDebug_SectionHeader).Label(LOCTEXT("CrossSystemEvents", "Recent cross-system events")).Underline(true)]
        + SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, CkStyle::SpaceS)
        [
            SAssignNew(_EventLog, SCkDebug_EventLog)
            .MaxEntries(200)
            .EmptyText(LOCTEXT("NoCrossSystemEvents", "No changes since this entity was selected."))
        ]
    ];
}

auto SCkAiDebuggerWindow::Build_SpatialPanel() -> TSharedRef<SWidget>
{
    return SNew(SCkDebug_Card).BodyPadding(FMargin{CkStyle::SpaceS})
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [SNew(SCkDebug_SectionHeader).Label(LOCTEXT("Spatial", "Spatial evidence")).Underline(true)]
        + SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, CkStyle::SpaceS)
        [_SpatialViewport.ToSharedRef()]
    ];
}

auto SCkAiDebuggerWindow::Get_StatusText() const -> FText
{
    if (NOT Get_TargetWorld()) { return LOCTEXT("NoWorld", "No PIE or game world. Start a session to inspect AI."); }
    return ck::IsValid(_SelectedEntity)
        ? FText::Format(LOCTEXT("SelectedStatus", "{0} tracked · overlay providers are the single source of AI facts"), FText::AsNumber(_TrackedRoster.Num()))
        : LOCTEXT("NoSelected", "Pick an AI entity or select one in a linked debugger.");
}

auto SCkAiDebuggerWindow::Get_MaxNameDepth() const -> int32
{
    return FMath::Max(1, _MaxNameDepth);
}

auto SCkAiDebuggerWindow::Get_ShortName(const FString& InFullName) const -> FString
{
    // Get_ShortName is the suite's one shortener; depth 0 means "leave it alone", which is exactly
    // what the cycler's Full setting should do.
    return _NameDepth <= 0 ? InFullName : SCkDebug_NameLabel::Get_ShortName(InFullName, _NameDepth);
}

auto SCkAiDebuggerWindow::Refresh_Roster() -> void
{
    if (NOT _AiRosterCollection.IsValid() || NOT _CrowdViewModel.IsValid()) { return; }

    auto Records = TArray<FCkUiRecordData>{};
    auto NextHandles = TMap<FString, FCk_Handle>{};
    for (const auto& Agent : _CrowdViewModel->Get_AllAgents())
    {
        const auto AgentIsValid = ck::IsValid(Agent.Handle);
        CK_ENSURE_IF_NOT(AgentIsValid, TEXT("AI Overview roster received an invalid Crowd agent"))
        { continue; }

        const auto Entity = ck::DebugSelectionSync::Resolve_ConceptualTarget(Agent.Handle);
        const auto TargetIsValid = ck::IsValid(Entity);
        CK_ENSURE_IF_NOT(TargetIsValid,
            TEXT("AI Overview could not resolve a conceptual target for Crowd agent [{}]"),
            Agent.Handle)
        { continue; }

        const auto HasFailure = Agent.Status == ECkCrowdDebugger_AgentStatus::Failed;
        const auto NeedsAttention = HasFailure || Agent.HasPathTroubleEvent
            || Agent.Status == ECkCrowdDebugger_AgentStatus::Replanning;
        const auto Tone = HasFailure ? ECk_Tone::Err : NeedsAttention ? ECk_Tone::Warn : ECk_Tone::Ok;
        const auto Summary = Agent.PathTroubleSummary.IsEmpty()
            ? FText::Format(LOCTEXT("RosterSummary", "{0} cm/s of {1} · {2} neighbors · {3} path points"),
                FText::AsNumber(FMath::RoundToInt(Agent.Velocity.Size())),
                FText::AsNumber(FMath::RoundToInt(Agent.MaxSpeed)),
                FText::AsNumber(Agent.NeighborCount),
                FText::AsNumber(Agent.PlannedPathPointCount))
            : FText::FromString(Agent.PathTroubleSummary);
        auto ContextParts = TArray<FString>{
            FString::Printf(TEXT("entity #%u"), Entity.Get_Entity().Get_ID())};
        if (NOT Agent.PrimaryTag.IsEmpty() && Agent.PrimaryTag != TEXT("—"))
        { ContextParts.Add(Agent.PrimaryTag); }
        if (NOT Agent.QueueDebugName.IsEmpty())
        {
            ContextParts.Add(Agent.QueueRank == INDEX_NONE
                ? FString::Printf(TEXT("queue %s"), *Agent.QueueDebugName)
                : FString::Printf(TEXT("queue %s · rank %d"), *Agent.QueueDebugName, Agent.QueueRank));
        }
        const auto Context = FText::FromString(FString::Join(ContextParts, TEXT(" · ")));
        const auto DisplayName = FText::FromName(Agent.Handle.Get_DebugName());
        const FString Key = ck_ai_debugger_window::AiRosterKey(_AiRosterGeneration, Agent.Handle);
        auto Record = FCkUiRecordData{};
        Record.Key = Key;
        Record.Fields.Add(TEXT("ai-roster-name"), ck_ai_debugger_window::TextField(DisplayName));
        Record.Fields.Add(TEXT("ai-roster-status"), ck_ai_debugger_window::TextField(
            ck_ai_debugger_window::GetCrowdStatus(Agent.Status)));
        Record.Fields.Add(TEXT("ai-roster-status-color"), ck_ai_debugger_window::ColorField(
            CkStyle::GetToneColor(Tone)));
        Record.Fields.Add(TEXT("ai-roster-status-background"), ck_ai_debugger_window::ColorField(
            CkStyle::GetToneDimColor(Tone)));
        Record.Fields.Add(TEXT("ai-roster-summary"), ck_ai_debugger_window::TextField(Summary));
        Record.Fields.Add(TEXT("ai-roster-context"), ck_ai_debugger_window::TextField(Context));
        Records.Add(MoveTemp(Record));
        NextHandles.Add(Key, Agent.Handle);
    }

    // Collection admission validates every typed record before publishing. The corresponding physical-handle map
    // changes only with the accepted record set, so a rejected projection cannot leave selection pointing at new data.
    if (_AiRosterCollection->TrySetRecords(MoveTemp(Records)).Succeeded)
    {
        _AiRosterHandles = MoveTemp(NextHandles);
        if (_AiRosterView.IsValid())
        {
            const FCk_Handle SelectedPhysical = _CrowdViewModel->Get_SelectedHandle();
            const TOptional<FString> SelectedKey = ck::IsValid(SelectedPhysical)
                ? TOptional<FString>{ck_ai_debugger_window::AiRosterKey(_AiRosterGeneration, SelectedPhysical)}
                : TOptional<FString>{};
            if (const TSharedPtr<SCkUiTable> Table = _AiRosterView->GetTable(TEXT("ai-roster")))
            { Table->TrySelectKey(SelectedKey); }
        }
    }
}

auto SCkAiDebuggerWindow::Get_AiRosterSelectedPhysicalHandle() const -> FCk_Handle
{ return _CrowdViewModel.IsValid() ? _CrowdViewModel->Get_SelectedHandle() : FCk_Handle{}; }

auto SCkAiDebuggerWindow::CanUse_AiRosterView(const int64 InGeneration) const -> bool
{ return InGeneration == _AiRosterGeneration; }

auto SCkAiDebuggerWindow::Invalidate_AiRosterView() -> void
{
    ++_AiRosterGeneration;
    // Generation and handle-bearing state must be gone before the retained table releases its callbacks.
    _AiRosterHandles.Reset();
    if (_AiRosterCollection.IsValid()) { _AiRosterCollection->TrySetRecords({}); }
    _AiRosterView.Reset();
    if (_AiRosterHost.IsValid()) { _AiRosterHost->SetContent(SNullWidget::NullWidget); }
}

auto SCkAiDebuggerWindow::Poll_AiRosterFiles(const double InNow) -> void
{
    constexpr double PollIntervalSeconds = 0.5;
    if (InNow < _NextAiRosterPollSeconds) { return; }
    _NextAiRosterPollSeconds = InNow + PollIntervalSeconds;
    if (_AiRosterView.IsValid()) { _AiRosterView->PollFiles(ck_ai_debugger_window::AiRosterStyleTokens()); }
    else { Build_AiRosterView(); }
}

auto SCkAiDebuggerWindow::Build_AiRosterView() -> void
{
    if (NOT _AiRosterHost.IsValid() || NOT _AiRosterCollection.IsValid() || _AiRosterView.IsValid()) { return; }

    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    if (NOT RegistryResult.Succeeded)
    {
        _AiRosterLoadError = FString::Join(RegistryResult.Errors, TEXT("\n"));
        _AiRosterHost->SetContent(SNew(STextBlock).Text(FText::FromString(
            _AiRosterLoadError)));
        return;
    }

    const TWeakPtr<SCkAiDebuggerWindow> WeakPanel{SharedThis(this)};
    const int64 Generation = _AiRosterGeneration;
    auto Data = FCkUiView::FDataBindings{};
    Data.SlateUserIndex = 0;
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakPanel, Generation]()
    {
        const TSharedPtr<SCkAiDebuggerWindow> Panel = WeakPanel.Pin();
        return Panel.IsValid() && Panel->CanUse_AiRosterView(Generation);
    });
    Data.Text.Add(TEXT("ai-roster-empty"), TAttribute<FText>::CreateLambda([WeakPanel, Generation]()
    {
        const TSharedPtr<SCkAiDebuggerWindow> Panel = WeakPanel.Pin();
        if (NOT Panel.IsValid() || NOT Panel->CanUse_AiRosterView(Generation)) { return FText::GetEmpty(); }
        if (NOT ck::IsValid(Panel->Get_TargetWorld()))
        { return FText::FromString(TEXT("No PIE or game world. Start a session to inspect AI.")); }
        return Panel->_AiRosterCollection.IsValid() && Panel->_AiRosterCollection->GetRecords().IsEmpty()
            ? FText::FromString(TEXT("No Crowd agents in this world."))
            : FText::GetEmpty();
    }));
    Data.Visibility.Add(TEXT("ai-roster-empty-visible"), TAttribute<bool>::CreateLambda([WeakPanel, Generation]()
    {
        const TSharedPtr<SCkAiDebuggerWindow> Panel = WeakPanel.Pin();
        return Panel.IsValid() && Panel->CanUse_AiRosterView(Generation)
            && (NOT ck::IsValid(Panel->Get_TargetWorld())
                || (Panel->_AiRosterCollection.IsValid() && Panel->_AiRosterCollection->GetRecords().IsEmpty()));
    }));
    Data.Collections.Add(TEXT("ai-roster-records"), _AiRosterCollection);
    Data.TableSelectionChanged.Add(TEXT("select-ai-roster"), FOnCkUiTableSelectionChanged::CreateLambda(
        [WeakPanel, Generation](TOptional<FString> InKey, ESelectInfo::Type InInfo)
        {
            if (InInfo == ESelectInfo::Direct || NOT InKey.IsSet()) { return; }
            const TSharedPtr<SCkAiDebuggerWindow> Panel = WeakPanel.Pin();
            if (NOT Panel.IsValid() || NOT Panel->CanUse_AiRosterView(Generation)) { return; }
            const FCk_Handle* PhysicalHandle = Panel->_AiRosterHandles.Find(InKey.GetValue());
            if (PhysicalHandle != nullptr && ck::IsValid(*PhysicalHandle))
            { Panel->Select_EntityImpl(*PhysicalHandle, true, true); }
        }));

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT Plugin.IsValid())
    {
        _AiRosterLoadError = TEXT("CkDebugger resources are unavailable.");
        _AiRosterHost->SetContent(SNew(STextBlock).Text(FText::FromString(_AiRosterLoadError)));
        return;
    }

    const TSharedRef<FCkUiView> View = FCkUiView::Create({}, {}, ck_ai_debugger_window::AiRosterStyleTokens(),
        CkStyle::RegularFont(CkStyle::FontSizeBody()), MoveTemp(Data), Registry);
    const TSharedRef<SWidget> Main = View->GetRegion(TEXT("main"));
    const FString Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    View->SetFiles(FPaths::Combine(Directory, TEXT("AiDebuggerRoster.ui.html")),
        FPaths::Combine(Directory, TEXT("AiDebuggerRoster.ui.css")));
    View->PollFiles();
    if (NOT View->GetLastResult().Succeeded)
    {
        _AiRosterLoadError = FString::Join(View->GetLastResult().Errors, TEXT("\n"));
        _AiRosterHost->SetContent(SNew(STextBlock).Text(FText::FromString(
            _AiRosterLoadError)));
        return;
    }

    _AiRosterLoadError.Reset();
    _AiRosterView = View;
    _AiRosterHost->SetContent(Main);
}

auto SCkAiDebuggerWindow::OnStyleRevisionChanged() -> void
{
    if (_AuthoredShellView.IsValid()) { _AuthoredShellView->PollFiles(ck_ai_debugger_window::ShellTokens()); }
    if (_AiRosterView.IsValid()) { _AiRosterView->PollFiles(ck_ai_debugger_window::AiRosterStyleTokens()); }
}

auto SCkAiDebuggerWindow::HandleSessionInvalidated() -> void
{
    if (_ViewportPicker.IsValid()) { _ViewportPicker->Deactivate(); }
    _SelectedEntity = {};
    _TrackedRoster.Reset();
    _Model = {};
    _History = {};
    _ActiveWorld.Reset();
    _AiRosterWorld.Reset();
    Invalidate_AiRosterView();
    Clear_Diagnostics();
    if (_CrowdViewModel.IsValid()) { _CrowdViewModel->Reset_ForWorldChange(); }
    if (_SpatialViewport.IsValid()) { _SpatialViewport->Notify_WorldChanged(); }
}

auto SCkAiDebuggerWindow::HandleWorldInvalidated(UWorld* InWorld) -> void
{
    const auto WorldIsValid = ck::IsValid(InWorld);
    CK_ENSURE_IF_NOT(WorldIsValid, TEXT("AI Overview received an invalid world-teardown boundary"))
    { return; }

    if (_ActiveWorld.Get() != InWorld && _AiRosterWorld.Get() != InWorld)
    { return; }

    HandleSessionInvalidated();
}

auto SCkAiDebuggerWindow::Refresh_Diagnostics(double InNow) -> void
{
    TRACE_CPUPROFILER_EVENT_SCOPE(CkAiDbg_RefreshDiagnostics);
    const auto Facts = ck::ai_debugger::evidence::Normalize(_Model);
    if (_CurrentEvidenceList.IsValid())
    {
        auto Items = TArray<FCkDebug_EvidenceItem>{};
        Items.Reserve(Facts.Num());
        for (const auto& Fact : Facts)
        {
            Items.Add(FCkDebug_EvidenceItem{
                Fact.StableKey,
                Fact.Tone,
                FText::FromString(Fact.Category),
                FText::FromString(Fact.SourceLabel + TEXT(" · ") + Fact.Headline),
                FText::FromString(Fact.Detail),
                0,
                FText::FromString(Fact.DisplayValue),
                Fact.CopyText,
                INDEX_NONE});
        }
        _CurrentEvidenceList->Set_Items(MoveTemp(Items));
    }

    const auto Topology = ck::ai_debugger::evidence::NormalizeTopology(_Model);

    // The cycler's ceiling follows the content: it is the longest name the two hierarchies
    // currently hold, so cycling past it always wraps to Full rather than to a dead depth.
    _MaxNameDepth = 1;
    const auto WidenMaxDepth = [this](const TArray<FCkAiDebugger_TopologyNode>& InNodes)
    {
        for (const auto& Node : InNodes)
        {
            _MaxNameDepth = FMath::Max(_MaxNameDepth, SCkDebug_NameLabel::Get_SegmentCount(Node.Name));
            _MaxNameDepth = FMath::Max(_MaxNameDepth, SCkDebug_NameLabel::Get_SegmentCount(Node.Headline));
            for (const auto& Step : Node.Chain)
            { _MaxNameDepth = FMath::Max(_MaxNameDepth, SCkDebug_NameLabel::Get_SegmentCount(Step)); }
        }
    };
    WidenMaxDepth(Topology.Goaps);
    WidenMaxDepth(Topology.StateMachines);

    // One scannable line plus, at most, the chain. Entity ids, parent links, depth, and the raw
    // un-shortened names are deliberately NOT on screen — they live in the row's copy text, which
    // is one right-click away and is what a bug report actually needs.
    const auto ReconcileTopology = [this](const TArray<FCkAiDebugger_TopologyNode>& InNodes,
        const TCHAR* InCategory, const TSharedPtr<SCkDebug_EvidenceList>& InList)
    {
        if (NOT InList.IsValid()) { return; }
        auto Items = TArray<FCkDebug_EvidenceItem>{};
        Items.Reserve(InNodes.Num());
        for (const auto& Node : InNodes)
        {
            auto Headline = Get_ShortName(Node.Name);
            if (NOT Node.Headline.IsEmpty())
            { Headline += FString::Printf(TEXT("  ▸  %s"), *Get_ShortName(Node.Headline)); }

            auto ShortChain = TArray<FString>{};
            ShortChain.Reserve(Node.Chain.Num());
            for (const auto& Step : Node.Chain) { ShortChain.Add(Get_ShortName(Step)); }

            const auto Detail = ShortChain.IsEmpty()
                ? FString{}
                : FString::Printf(TEXT("%s: %s"), *Node.ChainLabel, *FString::Join(ShortChain, TEXT(" → ")));

            auto Relation = Node.ParentSourceEntityId == 0
                ? FString{TEXT("Root instance")}
                : FString::Printf(TEXT("Child of entity #%u"), Node.ParentSourceEntityId);
            const auto CopyText = FString::Printf(
                TEXT("[%s] %s (#%u)\n%s\n%s\nsource order: %d · depth: %d · chain level: %d"),
                InCategory, *Node.Name, Node.SourceEntityId, *Relation, *Node.Detail,
                Node.SourceOrder, Node.Depth, Node.ChainLevel);

            Items.Add(FCkDebug_EvidenceItem{
                Node.StableKey,
                Node.Tone,
                FText::FromString(InCategory),
                FText::FromString(Headline),
                FText::FromString(Detail),
                Node.Depth,
                FText::FromString(Node.Status),
                CopyText,
                INDEX_NONE});
        }
        InList->Set_Items(MoveTemp(Items));
    };
    ReconcileTopology(Topology.Goaps, TEXT("GOAP"), _GoapTopologyList);
    ReconcileTopology(Topology.StateMachines, TEXT("STATE"), _StateMachineTopologyList);

    if (_EventLog.IsValid())
    {
        auto Entries = TArray<FCkDebug_EventLogEntry>{};
        for (const auto& Event : _EvidenceDeltaTracker.Observe(Facts, InNow))
        {
            Entries.Add(FCkDebug_EventLogEntry{
                Event.Message, Event.Category, Event.Tone, Event.TimeSeconds, INDEX_NONE});
        }
        if (NOT Entries.IsEmpty()) { _EventLog->Add_Entries(MoveTemp(Entries)); }
    }
}

auto SCkAiDebuggerWindow::Clear_Diagnostics() -> void
{
    _EvidenceDeltaTracker.Reset();
    if (_CurrentEvidenceList.IsValid()) { _CurrentEvidenceList->Clear_Items(); }
    if (_GoapTopologyList.IsValid()) { _GoapTopologyList->Clear_Items(); }
    if (_StateMachineTopologyList.IsValid()) { _StateMachineTopologyList->Clear_Items(); }
    if (_EventLog.IsValid()) { _EventLog->Clear_Entries(); }
}

auto SCkAiDebuggerWindow::Get_TargetWorld() const -> UWorld*
{
    if (ck::IsValid(_SelectedEntity))
    {
        auto* SelectedWorld = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(_SelectedEntity);
        if (ck::IsValid(SelectedWorld)) { return SelectedWorld; }
    }

    const auto AuthorityTarget = ck::DebugWorldSpeed::Resolve_AuthorityWorld();
    if (AuthorityTarget.CanMutate()) { return AuthorityTarget.World.Get(); }

    if (NOT ck::IsValid(GEngine)) { return nullptr; }
    for (const auto& Context : GEngine->GetWorldContexts())
    {
        auto* World = Context.World();
        if (ck::IsValid(World) && World->HasBegunPlay()
            && (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game))
        { return World; }
    }
    return nullptr;
}

#undef LOCTEXT_NAMESPACE
