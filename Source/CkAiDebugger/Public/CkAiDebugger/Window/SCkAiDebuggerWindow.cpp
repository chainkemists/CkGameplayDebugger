#include "CkAiDebugger/Window/SCkAiDebuggerWindow.h"

#include "CkAiDebugger_Module.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Behavior/SCkDebug_BehaviorOverridePanel.h"
#include "CkDebuggerCommon/Lifecycle/CkDebug_SessionLifecycle.h"
#include "CkDebuggerCommon/Navigation/CkDebug_EntityTarget.h"
#include "CkDebuggerCommon/Navigation/CkDebug_SelectionSync.h"
#include "CkDebuggerCommon/Picker/CkDebug_ViewportPicker.h"
#include "CkDebuggerCommon/Picker/SCkDebug_ViewportPickerControls.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityDebuggerLinks.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EventLog.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityHealthList.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EvidenceList.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Card.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StageStrip.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"
#include "CkDebuggerCommon/Utils/CkDebug_WorldSpeed.h"
#include "CkCrowdDebugger/Data/CkCrowdDebugger_Types.h"
#include "CkCrowdDebugger/ViewModel/CkCrowdDebugger_ViewModel.h"
#include "CkCrowdDebugger/Viewport/SCkCrowdDebugger_3dViewport.h"
#include "CkEditorTools/Style/CkStyle.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEntityDebugOverlay/Layout/CkDebugOverlay_Layout.h"
#include "CkEntityDebugOverlay/Presentation/CkDebugOverlay_Present.h"
#include "CkEntityDebugOverlay/Provider/CkDebugOverlay_Registry.h"
#include "CkEntityDebugOverlay/Settings/CkDebugOverlay_Settings.h"
#include "CkEntityDebugOverlay/Tags/CkDebugOverlay_Tags.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"

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
}

auto SCkAiDebuggerWindow::Construct(const FArguments&) -> void
{
    Register_WithGate();

    _ViewportPicker = MakeShared<FCkDebug_ViewportPicker>();
    auto PickerParams = FCkDebug_ViewportPicker::FParams{};
    PickerParams.Get_TargetWorld = [WeakThis = TWeakPtr<SCkAiDebuggerWindow>(SharedThis(this))]() -> UWorld*
    { const auto Pinned = WeakThis.Pin(); return Pinned.IsValid() ? Pinned->Get_TargetWorld() : nullptr; };
    PickerParams.TargetFilter = [](const FCk_Handle& InEntity) { return Is_AiEntity(InEntity); };
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
            { Pinned->Select_Entity(Agents[InAgentIndex].OwnerHandle, true); }
        });

    _SessionInvalidatedHandle = ck::DebugSessionLifecycle::Get_OnSessionInvalidated().AddSP(
        this, &SCkAiDebuggerWindow::HandleSessionInvalidated);

    ChildSlot
    [
        SNew(SCkDebug_WindowChrome)
            .WindowId(Get_WindowId())
            .ToolTabId(FCkAiDebuggerModule::Get_TabName())
            .StatusText_Lambda([this]() { return Get_StatusText(); })
            .ShowRefreshControls(true)
            .CommonActionsContent()
            [
                SNew(SCkDebug_ViewportPickerControls)
                    .Picker(_ViewportPicker)
                    .PickTooltip(LOCTEXT("PickAiTooltip", "Pick an entity that has GOAP, State Machine, Crowd, or navigation evidence."))
            ]
            .Content()[Build_Body()]
    ];
}

SCkAiDebuggerWindow::~SCkAiDebuggerWindow()
{
    if (_SessionInvalidatedHandle.IsValid())
    { ck::DebugSessionLifecycle::Get_OnSessionInvalidated().Remove(_SessionInvalidatedHandle); }
    if (_ViewportPicker.IsValid()) { _ViewportPicker->Deactivate(); }
    Clear_Diagnostics();
    // FCk_Handle is deliberately released while ECS registries still exist.
    _SelectedEntity = FCk_Handle{};
    _TrackedRoster.Reset();
    _Model = FCk_DebugOverlay_EntityModel{};
    if (_CrowdViewModel.IsValid()) { _CrowdViewModel->Reset_ForWorldChange(); }
}

auto SCkAiDebuggerWindow::Tick(const FGeometry& InGeometry, double InNow, float InDeltaSeconds) -> void
{
    SCkDebugger_WindowBase::Tick(InGeometry, InNow, InDeltaSeconds);
    if (_ViewportPicker.IsValid()) { _ViewportPicker->Tick(InDeltaSeconds); }

    auto* World = Get_TargetWorld();
    if (_CrowdViewModel.IsValid())
    {
        _CrowdViewModel->Tick(World, InDeltaSeconds);
        if (_SpatialViewport.IsValid())
        {
            _SpatialViewport->Set_NavmeshTriangles(
                _CrowdViewModel->Get_NavTriVerts(), _CrowdViewModel->Get_NavGeometryRevision());
            _SpatialViewport->Set_AgentSnapshots(
                _CrowdViewModel->Get_AllAgents(), _CrowdViewModel->Get_SelectedHandle());
            _SpatialViewport->Set_PathNetworkRibbons(_CrowdViewModel->Get_PathNetworkRibbons());
            _SpatialViewport->Set_QueueSnapshots(_CrowdViewModel->Get_Queues());
        }
        Refresh_Roster();
    }
    if (InNow - _LastRefreshTime < ck_ai_debugger_window::RefreshInterval) { return; }
    _LastRefreshTime = InNow;

    if (ck::Is_NOT_Valid(_SelectedEntity)) { return; }
    const auto Updated = Build_Model(_SelectedEntity, InNow);
    if (NOT Is_AiModel(Updated))
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

auto SCkAiDebuggerWindow::OpenForEntity(const FCk_Handle& InEntity) -> void
{
    if (NOT Is_AiEntity(InEntity)) { return; }
    auto& Module = FCkAiDebuggerModule::Get();
    Module.OpenDebugger();
    if (const auto Window = Module.Get_DebuggerWindow())
    { Window->Select_Entity(InEntity, false); }
}

auto SCkAiDebuggerWindow::Select_Entity(const FCk_Handle& InEntity, bool InBroadcast) -> void
{
    if (NOT Is_AiEntity(InEntity)) { return; }
    const auto EntityChanged = _SelectedEntity != InEntity;
    if (EntityChanged) { Clear_Diagnostics(); }
    _SelectedEntity = InEntity;
    const auto Now = FPlatformTime::Seconds();
    _Model = Build_Model(InEntity, Now);
    if (NOT _TrackedRoster.ContainsByPredicate([&InEntity](const FCk_Handle& Existing) { return Existing == InEntity; }))
    { _TrackedRoster.Insert(InEntity, 0); }
    if (_TrackedRoster.Num() > 24) { _TrackedRoster.SetNum(24); }
    if (_CrowdViewModel.IsValid())
    {
        for (const auto& Agent : _CrowdViewModel->Get_AllAgents())
        {
            if (Agent.Handle == InEntity || Agent.OwnerHandle == InEntity)
            { _CrowdViewModel->Set_SelectedHandle(Agent.Handle); break; }
        }
    }
    Refresh_Roster();
    Refresh_Diagnostics(Now);
    if (InBroadcast) { ck::DebugSelectionSync::Broadcast(InEntity, FCkAiDebuggerModule::Get_TabName()); }
}

auto SCkAiDebuggerWindow::Is_AiModel(const FCk_DebugOverlay_EntityModel& InModel) -> bool
{
    return InModel.Sections.ContainsByPredicate([](const FCk_DebugOverlay_Section& Section)
    { return ck_ai_debugger_window::IsAiProvider(Section.ProviderTag); });
}

auto SCkAiDebuggerWindow::Build_Model(const FCk_Handle& InEntity, double InNow) -> FCk_DebugOverlay_EntityModel
{
    const auto Providers = FCk_DebugOverlay_Registry::Get().CreateAll();
    return ck_debugoverlay::Build_EntityModel(InEntity, Providers,
        ck_ai_debugger_window::MakeAiLayout(Providers), &_History, InNow,
        ck_ai_debugger_window::MakeAiBuildOptions());
}

auto SCkAiDebuggerWindow::Build_Body() -> TSharedRef<SWidget>
{
    return SNew(SSplitter)
        .Orientation(Orient_Horizontal)
        .PhysicalSplitterHandleSize(5.0f)
        + SSplitter::Slot().Value(0.20f)
        [
            SNew(SCkDebug_Card).BodyPadding(FMargin{CkStyle::SpaceM})
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[SNew(SCkDebug_SectionHeader).Label(LOCTEXT("Roster", "NPC health")).Underline(true)]
                + SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, CkStyle::SpaceS)
                [
                    SAssignNew(_HealthList, SCkDebug_EntityHealthList)
                    .SelectedEntity_Lambda([this] { return _SelectedEntity; })
                    .OnSelected_Lambda([this](const FCk_Handle& InEntity) { Select_Entity(InEntity, true); })
                ]
            ]
        ]
        + SSplitter::Slot().Value(0.80f)
        [
            SNew(SSplitter)
            .Orientation(Orient_Vertical)
            .PhysicalSplitterHandleSize(5.0f)
            + SSplitter::Slot().Value(0.52f)
            [
                Build_OverviewPane()
            ]
            + SSplitter::Slot().Value(0.48f)
            [
                SNew(SSplitter)
                .Orientation(Orient_Horizontal)
                .PhysicalSplitterHandleSize(5.0f)
                + SSplitter::Slot().Value(0.42f)
                [
                    SNew(SSplitter)
                    .Orientation(Orient_Vertical)
                    .PhysicalSplitterHandleSize(5.0f)
                    + SSplitter::Slot().Value(0.58f)
                    [
                        SNew(SCkDebug_Card).BodyPadding(FMargin{CkStyle::SpaceM})
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
                        ]
                    ]
                    + SSplitter::Slot().Value(0.42f)
                    [
                        SNew(SCkDebug_Card).BodyPadding(FMargin{CkStyle::SpaceM})
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
                        ]
                    ]
                ]
                + SSplitter::Slot().Value(0.58f)
                [
                    SNew(SCkDebug_Card).BodyPadding(FMargin{CkStyle::SpaceS})
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()
                        [SNew(SCkDebug_SectionHeader).Label(LOCTEXT("Spatial", "Spatial evidence")).Underline(true)]
                        + SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, CkStyle::SpaceS)
                        [_SpatialViewport.ToSharedRef()]
                    ]
                ]
            ]
        ];
}

auto SCkAiDebuggerWindow::Build_OverviewPane() -> TSharedRef<SWidget>
{
    return SNew(SSplitter)
        .Orientation(Orient_Vertical)
        .PhysicalSplitterHandleSize(5.0f)
        + SSplitter::Slot().Value(0.46f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().FillHeight(1.0f)
            [
                SNew(SSplitter)
                .Orientation(Orient_Horizontal)
                .PhysicalSplitterHandleSize(5.0f)
                + SSplitter::Slot().Value(0.38f)
                [Build_IdentityPanel()]
                + SSplitter::Slot().Value(0.34f)
                [
                    SNew(SCkDebug_Card).BodyPadding(FMargin{CkStyle::SpaceM})
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()
                        [SNew(SCkDebug_SectionHeader).Label(LOCTEXT("BehaviorOverrides", "Behavior controls")).Underline(true)]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS)
                        [SNew(SCkDebug_BehaviorOverridePanel)]
                    ]
                ]
                + SSplitter::Slot().Value(0.28f)
                [
                    SNew(SCkDebug_Card).BodyPadding(FMargin{CkStyle::SpaceM})
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()
                        [SNew(SCkDebug_SectionHeader).Label(LOCTEXT("DrillInto", "Drill into")).Underline(true)]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS)
                        [SNew(SCkDebug_EntityDebuggerLinks).Entity_Lambda([this]() { return _SelectedEntity; }).ExcludeTabId(FCkAiDebuggerModule::Get_TabName())]
                    ]
                ]
            ]
            + SVerticalBox::Slot().AutoHeight()[Build_StagePanel()]
        ]
        + SSplitter::Slot().Value(0.54f)
        [
            SNew(SSplitter)
            .Orientation(Orient_Horizontal)
            .PhysicalSplitterHandleSize(5.0f)
            + SSplitter::Slot().Value(0.50f)
            [
                SNew(SCkDebug_Card).BodyPadding(FMargin{CkStyle::SpaceM})
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [SNew(SCkDebug_SectionHeader).Label(LOCTEXT("GoapTopology", "GOAP hierarchy")).Underline(true)]
                    + SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, CkStyle::SpaceS)
                    [
                        SAssignNew(_GoapTopologyList, SCkDebug_EvidenceList)
                        .MaxItems(100)
                        .EmptyText(LOCTEXT("NoGoapTopology", "No GOAP instances reported."))
                    ]
                ]
            ]
            + SSplitter::Slot().Value(0.50f)
            [
                SNew(SCkDebug_Card).BodyPadding(FMargin{CkStyle::SpaceM})
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [SNew(SCkDebug_SectionHeader).Label(LOCTEXT("StateMachineTopology", "State Machine hierarchy")).Underline(true)]
                    + SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, CkStyle::SpaceS)
                    [
                        SAssignNew(_StateMachineTopologyList, SCkDebug_EvidenceList)
                        .MaxItems(100)
                        .EmptyText(LOCTEXT("NoStateMachineTopology", "No State Machine instances reported."))
                    ]
                ]
            ]
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

auto SCkAiDebuggerWindow::Get_StatusText() const -> FText
{
    if (NOT Get_TargetWorld()) { return LOCTEXT("NoWorld", "No PIE or game world. Start a session to inspect AI."); }
    return ck::IsValid(_SelectedEntity)
        ? FText::Format(LOCTEXT("SelectedStatus", "{0} tracked · overlay providers are the single source of AI facts"), FText::AsNumber(_TrackedRoster.Num()))
        : LOCTEXT("NoSelected", "Pick an AI entity or select one in a linked debugger.");
}

auto SCkAiDebuggerWindow::Refresh_Roster() -> void
{
    if (NOT _HealthList.IsValid() || NOT _CrowdViewModel.IsValid()) { return; }

    auto SignatureParts = TArray<FString>{};
    auto Items = TArray<FCkDebug_EntityHealthItem>{};
    for (const auto& Agent : _CrowdViewModel->Get_AllAgents())
    {
        const auto Entity = ck::IsValid(Agent.OwnerHandle) ? Agent.OwnerHandle : Agent.Handle;
        if (ck::Is_NOT_Valid(Entity)) { continue; }

        SignatureParts.Add(FString::Printf(TEXT("%u:%d:%d:%d:%s:%s:%d:%.1f"),
            Entity.Get_Entity().Get_ID(),
            static_cast<int32>(Agent.Status),
            Agent.HasPathTroubleEvent ? 1 : 0,
            Agent.NeighborCount,
            *Agent.PrimaryTag,
            *Agent.QueueDebugName,
            Agent.QueueRank,
            Agent.Velocity.Size()));

        const auto HasFailure = Agent.Status == ECkCrowdDebugger_AgentStatus::Failed;
        const auto NeedsAttention = HasFailure || Agent.HasPathTroubleEvent
            || Agent.Status == ECkCrowdDebugger_AgentStatus::Replanning;
        const auto Tone = HasFailure ? ECk_Tone::Err : NeedsAttention ? ECk_Tone::Warn : ECk_Tone::Ok;
        const auto Summary = Agent.PathTroubleSummary.IsEmpty()
            ? FText::Format(LOCTEXT("RosterSummary", "{0} cm/s of {1} · {2} neighbors · {3} path points"),
                FText::AsNumber(FMath::RoundToInt(Agent.Velocity.Size())),
                FText::AsNumber(FMath::RoundToInt(Agent.MaxSpeed)),
                FText::AsNumber(Agent.NeighborCount),
                FText::AsNumber(Agent.PlannedPath.Num()))
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
        const auto DisplayName = Agent.OwnerName.IsEmpty()
            ? FText::FromName(Entity.Get_DebugName())
            : FText::FromString(Agent.OwnerName);
        Items.Add(FCkDebug_EntityHealthItem{
            Entity,
            DisplayName,
            Summary,
            Context,
            ck_ai_debugger_window::GetCrowdStatus(Agent.Status),
            Tone});
    }

    const auto Signature = FString::Join(SignatureParts, TEXT("|"));
    if (Signature == _LastRosterSignature) { return; }
    _LastRosterSignature = Signature;
    _HealthList->Set_Items(MoveTemp(Items));
}

auto SCkAiDebuggerWindow::HandleSessionInvalidated() -> void
{
    if (_ViewportPicker.IsValid()) { _ViewportPicker->Deactivate(); }
    _SelectedEntity = {};
    _TrackedRoster.Reset();
    _Model = {};
    _History = {};
    Clear_Diagnostics();
    _LastRosterSignature.Reset();
    if (_CrowdViewModel.IsValid()) { _CrowdViewModel->Reset_ForWorldChange(); }
    if (_SpatialViewport.IsValid()) { _SpatialViewport->Clear_VoxelNavSnapshot(); }
    if (_HealthList.IsValid()) { _HealthList->Clear_Items(); }
}

auto SCkAiDebuggerWindow::Refresh_Diagnostics(double InNow) -> void
{
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
    const auto ReconcileTopology = [](const TArray<FCkAiDebugger_TopologyNode>& InNodes,
        const TCHAR* InCategory, const TSharedPtr<SCkDebug_EvidenceList>& InList)
    {
        if (NOT InList.IsValid()) { return; }
        auto Items = TArray<FCkDebug_EvidenceItem>{};
        Items.Reserve(InNodes.Num());
        for (const auto& Node : InNodes)
        {
            auto Relation = Node.ParentSourceEntityId == 0
                ? FString{TEXT("Root instance")}
                : FString::Printf(TEXT("Child of entity #%u"), Node.ParentSourceEntityId);
            if (NOT Node.Detail.IsEmpty()) { Relation += TEXT(" · ") + Node.Detail; }
            const auto CopyText = FString::Printf(TEXT("[%s] %s (#%u)\n%s\nsource order: %d · depth: %d"),
                InCategory, *Node.Name, Node.SourceEntityId, *Relation, Node.SourceOrder, Node.Depth);
            Items.Add(FCkDebug_EvidenceItem{
                Node.StableKey,
                Node.Tone,
                FText::FromString(InCategory),
                FText::FromString(Node.Name),
                FText::FromString(Relation),
                Node.Depth,
                FText::FromString(Node.State),
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
