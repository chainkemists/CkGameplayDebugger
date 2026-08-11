#if WITH_EDITOR
#include "CkSmDebugger/Preview/SCkSmDebugger_PreviewPane.h"

#include "CkSmDebugger/Graph/CkSmDebugGraph.h"
#include "CkSmDebugger/Graph/CkSmDebugGraphSchema.h"
#include "CkSmDebugger/Data/CkSmDebugger_Types.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Object/CkObject_Utils.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"

#include "CkStateMachine/State/EntityScripts/CkSmState_EntityScript.h"
#include "CkStateMachine/StateMachine/CkStateMachine_Utils.h"
#include "CkStateMachine/StateMachine/CkStateMachine_Fragment_Data.h"

#if CK_BUILD_SM_GRAPH_WALK
#include "CkStateMachine/Debug/CkStateMachine_Debug_GraphWalk_Fragment.h"
#endif

#include "Engine/World.h"
#include "GraphEditor.h"
#include "PropertyCustomizationHelpers.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

SCkSmDebugger_PreviewPane::~SCkSmDebugger_PreviewPane()
{
    DestroyPreviewEntity();

    if (_PreviewGraph && UObjectInitialized())
    {
        _PreviewGraph->RemoveFromRoot();
        _PreviewGraph = nullptr;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebugger_PreviewPane::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _PreviewGraph = NewObject<UCkSmDebugGraph>(GetTransientPackage());
    _PreviewGraph->AddToRoot();
    _PreviewGraph->Schema = UCkSmDebugGraphSchema::StaticClass();

    _StatusMessage = TEXT("Pick a state class to preview its state machine.");

    _PreviewGraphEditor = SNew(SGraphEditor)
        .GraphToEdit(_PreviewGraph)
        .IsEditable(true);

    // NOTE: The picker row is built by the host window and placed inline with the
    // main toolbar so both graphs start at the same vertical position.
    ChildSlot
    [
        SNew(SVerticalBox)

        + SVerticalBox::Slot()
            .FillHeight(1.0f)
            [
                _PreviewGraphEditor.ToSharedRef()
            ]

        + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(TAttribute<FMargin>::CreateLambda([]() -> FMargin
            {
                return ck::debug_axes::Get_RowPadding(UCkDebuggerStyleSettings::Get_Selection());
            }))
            [
                SNew(STextBlock)
                    .Text_Lambda([this]() { return GetStatusText(); })
                    .Font_Lambda([]() -> FSlateFontInfo
                    { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeSmall()); })
                    .ColorAndOpacity(CkStyle::TextDim())
            ]
    ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebugger_PreviewPane::
    Tick(
        const FGeometry& AllottedGeometry,
        const double InCurrentTime,
        const float InDeltaTime)
    -> void
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

    if (_WalkInProgress)
    { PollWalkAndFinalize(); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebugger_PreviewPane::
    BuildPickerRow()
    -> TSharedRef<SWidget>
{
    return BuildHeader();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebugger_PreviewPane::
    Set_WorldContext(
        const TWeakObjectPtr<UWorld>& InWorld)
    -> void
{
    const auto WorldChanged = (_World != InWorld);
    _World = InWorld;

    if (NOT WorldChanged)
    { return; }

    // World gone (PIE stopped) — drop the preview entity; the picker selection is kept
    // so a subsequent PIE session auto-kicks a new walk.
    if (ck::Is_NOT_Valid(_World.Get()))
    {
        DestroyPreviewEntity();
        _StatusMessage = TEXT("Start PIE to walk the selected state machine.");
        return;
    }

    // Fresh world arrived and we have a class queued — kick the walk.
    if (NOT _SelectedInitialStateClass.IsNull() && NOT _WalkInProgress && NOT ck::IsValid(_PreviewSmHandle))
    { StartWalk(); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebugger_PreviewPane::
    BuildHeader()
    -> TSharedRef<SWidget>
{
    auto* MetaClass = FindObject<UClass>(nullptr, TEXT("/Script/CkStateMachine.Ck_SmState_EntityScript"));
    if (ck::Is_NOT_Valid(MetaClass))
    { MetaClass = UObject::StaticClass(); }

    return SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(0.0f, 0.0f, 6.0f, 0.0f)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Initial State:")))
                    .Font_Lambda([]() -> FSlateFontInfo
                    { return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeBody()); })
                    .ColorAndOpacity(CkStyle::TextDim())
            ]

        + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(SClassPropertyEntryBox)
                    .MetaClass(MetaClass)
                    .AllowAbstract(false)
                    .AllowNone(true)
                    .SelectedClass_Lambda([this]() -> const UClass*
                    {
                        return _SelectedInitialStateClass.LoadSynchronous();
                    })
                    .OnSetClass_Lambda([this](const UClass* InClass)
                    {
                        _SelectedInitialStateClass = InClass;
                        RebuildPreviewGraph();
                    })
            ];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebugger_PreviewPane::
    GetSelectedClassPath() const
    -> FString
{
    return _SelectedInitialStateClass.ToString();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebugger_PreviewPane::
    RebuildPreviewGraph()
    -> void
{
    if (ck::Is_NOT_Valid(_PreviewGraph))
    { return; }

    // Drop any in-flight walk — class changed.
    DestroyPreviewEntity();

    _PreviewGraph->SetSuppressNotifications(true);
    _PreviewGraph->Nodes.Empty();
    _PreviewGraph->SetSuppressNotifications(false);
    _PreviewGraph->NotifyGraphChanged();

    auto* SelectedClass = _SelectedInitialStateClass.LoadSynchronous();
    if (ck::Is_NOT_Valid(SelectedClass))
    {
        _StatusMessage = TEXT("Pick a state class to preview its state machine.");
        return;
    }

#if !CK_BUILD_SM_GRAPH_WALK
    _StatusMessage = TEXT("SM graph walker disabled in this build configuration.");
#else
    if (ck::Is_NOT_Valid(_World.Get()))
    {
        _StatusMessage = TEXT("Start PIE to walk the selected state machine.");
        return;
    }

    StartWalk();
#endif
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebugger_PreviewPane::
    StartWalk()
    -> void
{
#if CK_BUILD_SM_GRAPH_WALK
    auto* World = _World.Get();
    if (ck::Is_NOT_Valid(World) || NOT World->HasBegunPlay())
    {
        _StatusMessage = TEXT("Start PIE to walk the selected state machine.");
        return;
    }

    auto* InitialStateClass = _SelectedInitialStateClass.LoadSynchronous();
    if (ck::Is_NOT_Valid(InitialStateClass))
    { return; }

    auto TransientOwner = UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(World);
    if (ck::Is_NOT_Valid(TransientOwner))
    {
        _StatusMessage = TEXT("ECS world subsystem not ready yet.");
        return;
    }

    // Create a throwaway SM entity. AutoStart=Disabled so the walker runs without
    // actually entering the initial state — we only want structural discovery.
    auto PreviewSmParams = FCk_Fragment_StateMachine_ParamsData{TSubclassOf<UCk_SmState_EntityScript>(InitialStateClass)};
    PreviewSmParams.Set_AutoStart(ECk_SmAutoStart::Disabled);
    _PreviewSmHandle = UCk_Utils_StateMachine_UE::Add(TransientOwner, PreviewSmParams);

    if (ck::Is_NOT_Valid(_PreviewSmHandle))
    {
        _StatusMessage = TEXT("Failed to create preview SM entity.");
        return;
    }

    _WalkInProgress = true;
    _StatusMessage = ck::Format_UE(TEXT("Walking {}..."),
        UCk_Utils_Object_UE::Get_CleanClassName(InitialStateClass));
#endif
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebugger_PreviewPane::
    PollWalkAndFinalize()
    -> void
{
#if CK_BUILD_SM_GRAPH_WALK
    if (ck::Is_NOT_Valid(_PreviewSmHandle))
    {
        _WalkInProgress = false;
        return;
    }

    auto SmHandle = static_cast<FCk_Handle>(_PreviewSmHandle);
    if (NOT SmHandle.Has<ck::FFragment_Sm_Debug_GraphDefinition>())
    { return; }

    const auto& GraphDef = SmHandle.Get<ck::FFragment_Sm_Debug_GraphDefinition>();
    if (NOT GraphDef.Get_IsComplete())
    { return; }

    PublishStructuralSmInfo();
    _WalkInProgress = false;

    // Keep the SM entity alive for now — destroying it before the user changes class
    // keeps the graph data available if we later want to re-sync without re-walking.
#endif
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebugger_PreviewPane::
    PublishStructuralSmInfo()
    -> void
{
#if CK_BUILD_SM_GRAPH_WALK
    if (ck::Is_NOT_Valid(_PreviewGraph) || ck::Is_NOT_Valid(_PreviewSmHandle))
    { return; }

    auto SmHandle = static_cast<FCk_Handle>(_PreviewSmHandle);
    const auto& GraphDef = SmHandle.Get<ck::FFragment_Sm_Debug_GraphDefinition>();

    auto* InitialClass = _SelectedInitialStateClass.LoadSynchronous();

    auto SmInfo = FCkSmDebugger_SmInfo{};
    SmInfo.Handle = _PreviewSmHandle;
    SmInfo.InitialStateClass = TSubclassOf<UCk_SmState_EntityScript>(InitialClass);
    SmInfo.RunStatus = ECk_SmRunStatus::Stopped;
    SmInfo.DebugName = ck::IsValid(InitialClass)
        ? UCk_Utils_Object_UE::Get_CleanClassName(InitialClass)
        : FString(TEXT("(preview)"));

    auto StateClassToIndex = TMap<TSubclassOf<UCk_SmState_EntityScript>, int32>{};

    for (const auto& [StateClass, StateDef] : GraphDef.Get_StateDefinitions())
    {
        auto StateInfo = FCkSmDebugger_StateInfo{};
        StateInfo.StateClass = StateClass;
        StateInfo.ScriptClass = StateDef.ScriptClass;
        StateInfo.RequestedScriptClass = StateDef.RequestedScriptClass;
        StateInfo.StateName  = StateDef.StateName;

        for (const auto& TaskDef : StateDef.Tasks)
        {
            auto TaskInfo = FCkSmDebugger_TaskInfo{};
            TaskInfo.ClassName             = TaskDef.ClassName;
            TaskInfo.ScriptClass           = TaskDef.ScriptClass;
            TaskInfo.Mode                  = TaskDef.Mode;
            TaskInfo.HasSubStateMachine    = TaskDef.HasSubStateMachine;
            TaskInfo.SubSmInitialStateClass = TaskDef.SubSmInitialStateClass;
            StateInfo.Tasks.Add(MoveTemp(TaskInfo));

            if (TaskDef.HasSubStateMachine)
            { StateInfo.HasSubStateMachine = true; }
        }

        const auto Index = SmInfo.States.Num();
        StateClassToIndex.Add(StateClass, Index);
        SmInfo.States.Add(MoveTemp(StateInfo));
    }

    // Inline each sub-SM's states so the preview renders nested SMs too (mirrors the
    // live view which uses compound nodes to group sub-SM children).
    for (const auto& [ParentClass, SubSmDef] : GraphDef.Get_SubSmDefinitions())
    {
        const auto ParentIndexPtr = StateClassToIndex.Find(ParentClass);
        const auto ParentIndex    = ParentIndexPtr ? *ParentIndexPtr : -1;

        for (const auto& [StateClass, StateDef] : SubSmDef.StateDefinitions)
        {
            if (StateClassToIndex.Contains(StateClass))
            { continue; }

            auto StateInfo = FCkSmDebugger_StateInfo{};
            StateInfo.StateClass           = StateClass;
            StateInfo.ScriptClass          = StateDef.ScriptClass;
            StateInfo.RequestedScriptClass = StateDef.RequestedScriptClass;
            StateInfo.StateName            = StateDef.StateName;
            StateInfo.IsSubSmNode          = true;
            StateInfo.SubSmParentStateIndex = ParentIndex;
            if (ParentIndex >= 0)
            { StateInfo.SubSmParentStateName = SmInfo.States[ParentIndex].StateName; }

            for (const auto& TaskDef : StateDef.Tasks)
            {
                auto TaskInfo = FCkSmDebugger_TaskInfo{};
                TaskInfo.ClassName             = TaskDef.ClassName;
                TaskInfo.ScriptClass           = TaskDef.ScriptClass;
                TaskInfo.Mode                  = TaskDef.Mode;
                TaskInfo.HasSubStateMachine    = TaskDef.HasSubStateMachine;
                TaskInfo.SubSmInitialStateClass = TaskDef.SubSmInitialStateClass;
                StateInfo.Tasks.Add(MoveTemp(TaskInfo));
            }

            const auto Index = SmInfo.States.Num();
            StateClassToIndex.Add(StateClass, Index);
            SmInfo.States.Add(MoveTemp(StateInfo));
        }
    }

    // Top-level transitions
    for (const auto& [StateClass, StateDef] : GraphDef.Get_StateDefinitions())
    {
        const auto* SourceIndex = StateClassToIndex.Find(StateClass);
        if (NOT SourceIndex)
        { continue; }

        auto Order = 0;
        for (const auto& TransDef : StateDef.Transitions)
        {
            auto TransInfo = FCkSmDebugger_TransitionInfo{};
            TransInfo.SourceStateIndex = *SourceIndex;
            TransInfo.SourceStateClass = StateClass;
            TransInfo.SourceStateName  = SmInfo.States[*SourceIndex].StateName;
            TransInfo.TargetStateClass = TransDef.TargetStateClass;
            TransInfo.Order            = Order++;

            if (ck::IsValid(TransDef.TargetStateClass))
            { TransInfo.TargetStateName = UCk_Utils_Object_UE::Get_CleanClassName(TransDef.TargetStateClass); }

            const auto* TargetIndex = StateClassToIndex.Find(TransDef.TargetStateClass);
            TransInfo.TargetStateIndex = TargetIndex ? *TargetIndex : -1;

            for (const auto& CondDef : TransDef.Conditions)
            {
                auto CondInfo = FCkSmDebugger_ConditionInfo{};
                CondInfo.ClassName = CondDef.ClassName;
                CondInfo.ScriptClass = CondDef.ScriptClass;
                CondInfo.Mode = CondDef.Mode;
                CondInfo.Result = ECk_SmConditionResult::Undetermined;
                TransInfo.Conditions.Add(MoveTemp(CondInfo));
            }
            TransInfo.TotalCount = TransInfo.Conditions.Num();

            SmInfo.Transitions.Add(MoveTemp(TransInfo));
        }
    }

    // Sub-SM transitions (marked so the graph renderer nests them under the right parent)
    for (const auto& [ParentClass, SubSmDef] : GraphDef.Get_SubSmDefinitions())
    {
        for (const auto& [StateClass, StateDef] : SubSmDef.StateDefinitions)
        {
            const auto* SourceIndex = StateClassToIndex.Find(StateClass);
            if (NOT SourceIndex)
            { continue; }

            auto Order = 0;
            for (const auto& TransDef : StateDef.Transitions)
            {
                auto TransInfo = FCkSmDebugger_TransitionInfo{};
                TransInfo.SourceStateIndex = *SourceIndex;
                TransInfo.SourceStateClass = StateClass;
                TransInfo.SourceStateName  = SmInfo.States[*SourceIndex].StateName;
                TransInfo.TargetStateClass = TransDef.TargetStateClass;
                TransInfo.Order            = Order++;
                TransInfo.IsSubSmTransition = true;

                if (ck::IsValid(TransDef.TargetStateClass))
                { TransInfo.TargetStateName = UCk_Utils_Object_UE::Get_CleanClassName(TransDef.TargetStateClass); }

                const auto* TargetIndex = StateClassToIndex.Find(TransDef.TargetStateClass);
                TransInfo.TargetStateIndex = TargetIndex ? *TargetIndex : -1;

                for (const auto& CondDef : TransDef.Conditions)
                {
                    auto CondInfo = FCkSmDebugger_ConditionInfo{};
                    CondInfo.ClassName = CondDef.ClassName;
                    CondInfo.ScriptClass = CondDef.ScriptClass;
                    CondInfo.Mode = CondDef.Mode;
                    CondInfo.Result = ECk_SmConditionResult::Undetermined;
                    TransInfo.Conditions.Add(MoveTemp(CondInfo));
                }
                TransInfo.TotalCount = TransInfo.Conditions.Num();

                SmInfo.Transitions.Add(MoveTemp(TransInfo));
            }
        }
    }

    _PreviewGraph->SetSuppressNotifications(true);
    _PreviewGraph->RebuildFromSmInfo(SmInfo);
    _PreviewGraph->SetSuppressNotifications(false);
    _PreviewGraph->NotifyGraphChanged();

    _StatusMessage = ck::Format_UE(TEXT("Walked {} states / {} transitions."),
        SmInfo.States.Num(), SmInfo.Transitions.Num());
#endif
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebugger_PreviewPane::
    DestroyPreviewEntity()
    -> void
{
    if (ck::IsValid(_PreviewSmHandle))
    {
        auto Handle = static_cast<FCk_Handle>(_PreviewSmHandle);
        UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(Handle);
    }

    _PreviewSmHandle = FCk_Handle_StateMachine{};
    _WalkInProgress  = false;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebugger_PreviewPane::
    GetStatusText() const
    -> FText
{
    return FText::FromString(_StatusMessage);
}
#endif
