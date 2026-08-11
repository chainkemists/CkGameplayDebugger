#include "CkSmDebugger/Preview/SCkSmDebugger_PreviewPane.h"

#include "CkSmDebugger/Graph/SCkSmRuntimeGraph.h"
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
#include "UObject/UObjectIterator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Views/SListView.h"

// --------------------------------------------------------------------------------------------------------------------

SCkSmDebugger_PreviewPane::~SCkSmDebugger_PreviewPane()
{
    DestroyPreviewEntity();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkSmDebugger_PreviewPane::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _StatusMessage = TEXT("Pick a state class to preview its state machine.");
    RefreshClassOptions();

    // NOTE: The picker row is built by the host window and placed inline with the
    // main toolbar so both graphs start at the same vertical position.
    ChildSlot
    [
        SNew(SVerticalBox)

        + SVerticalBox::Slot()
            .FillHeight(1.0f)
            [
                SAssignNew(_PreviewRuntimeGraph, SCkSmRuntimeGraph)
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

    if (_PendingFrameAll && _PreviewRuntimeGraph.IsValid())
    {
        _PreviewRuntimeGraph->FrameAll();
        _PendingFrameAll = false;
    }
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
                SAssignNew(_ClassPicker, SComboButton)
                    .OnGetMenuContent(this, &SCkSmDebugger_PreviewPane::BuildClassPickerMenu)
                    .ToolTipText_Lambda([this]()
                    { return FText::FromString(GetSelectedClassPath()); })
                    .ButtonContent()
                    [
                        SNew(STextBlock)
                            .Text_Lambda([this]()
                            { return FText::FromString(GetSelectedClassLabel()); })
                    ]
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
    // Drop any in-flight walk — class changed.
    DestroyPreviewEntity();
    _PreviewSmInfo = FCkSmDebugger_SmInfo{};
    _PendingFrameAll = false;
    if (_PreviewRuntimeGraph.IsValid()) { _PreviewRuntimeGraph->Clear(); }

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
    if (NOT _PreviewRuntimeGraph.IsValid() || ck::Is_NOT_Valid(_PreviewSmHandle))
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

    _PreviewSmInfo = MoveTemp(SmInfo);
    _PreviewRuntimeGraph->SetSmInfo(&_PreviewSmInfo);
    _PendingFrameAll = true;

    _StatusMessage = ck::Format_UE(TEXT("Walked {} states / {} transitions."),
        _PreviewSmInfo.States.Num(), _PreviewSmInfo.Transitions.Num());
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

auto SCkSmDebugger_PreviewPane::RefreshClassOptions() -> void
{
    _ClassOptions.Reset();
    auto NoneOption = MakeShared<FClassOption>();
    NoneOption->Label = TEXT("None");
    _ClassOptions.Add(MoveTemp(NoneOption));

    for (TObjectIterator<UClass> It; It; ++It)
    {
        auto* Class = *It;
        if (NOT IsValid(Class) || Class == UCk_SmState_EntityScript::StaticClass() ||
            NOT Class->IsChildOf(UCk_SmState_EntityScript::StaticClass()) ||
            Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
        { continue; }

        auto Option = MakeShared<FClassOption>();
        Option->Class = Class;
        Option->Label = UCk_Utils_Object_UE::Get_CleanClassName(Class);
        Option->Path = Class->GetPathName();
        _ClassOptions.Add(MoveTemp(Option));
    }
    _ClassOptions.Sort([](const TSharedPtr<FClassOption>& A, const TSharedPtr<FClassOption>& B)
    {
        if (NOT A.IsValid() || NOT B.IsValid())
        { return A.IsValid(); }
        if (A->Class == nullptr || B->Class == nullptr)
        { return A->Class == nullptr && B->Class != nullptr; }
        const auto LabelOrder = A->Label.Compare(B->Label, ESearchCase::IgnoreCase);
        return LabelOrder == 0 ? A->Path < B->Path : LabelOrder < 0;
    });
    RefilterClassOptions({});
}

auto SCkSmDebugger_PreviewPane::RefilterClassOptions(const FString& InFilter) -> void
{
    _FilteredClassOptions.Reset();
    for (const auto& Option : _ClassOptions)
    {
        if (NOT Option.IsValid())
        { continue; }
        if (InFilter.IsEmpty()
            || Option->Label.Contains(InFilter, ESearchCase::IgnoreCase)
            || Option->Path.Contains(InFilter, ESearchCase::IgnoreCase))
        { _FilteredClassOptions.Add(Option); }
    }
    if (_ClassListView.IsValid())
    { _ClassListView->RequestListRefresh(); }
}

auto SCkSmDebugger_PreviewPane::BuildClassPickerMenu() -> TSharedRef<SWidget>
{
    RefreshClassOptions();

    return SNew(SBox)
        .WidthOverride(520.0f)
        .HeightOverride(420.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(4.0f)
                [
                    SNew(SSearchBox)
                        .HintText(FText::FromString(TEXT("Search state classes...")))
                        .OnTextChanged_Lambda([this](const FText& InText)
                        { RefilterClassOptions(InText.ToString()); })
                ]
            + SVerticalBox::Slot()
                .FillHeight(1.0f)
                .Padding(4.0f, 0.0f, 4.0f, 4.0f)
                [
                    SAssignNew(_ClassListView, SListView<TSharedPtr<FClassOption>>)
                        .ListItemsSource(&_FilteredClassOptions)
                        .SelectionMode(ESelectionMode::Single)
                        .OnGenerateRow_Lambda([](TSharedPtr<FClassOption> InOption,
                                                const TSharedRef<STableViewBase>& InOwner)
                        {
                            const auto Label = InOption.IsValid() ? InOption->Label : FString{};
                            const auto Path = InOption.IsValid() ? InOption->Path : FString{};
                            return SNew(STableRow<TSharedPtr<FClassOption>>, InOwner)
                                .ToolTipText(FText::FromString(Path))
                                [SNew(STextBlock).Text(FText::FromString(Label))];
                        })
                        .OnSelectionChanged_Lambda([this](TSharedPtr<FClassOption> InOption,
                                                         ESelectInfo::Type InSelectInfo)
                        {
                            if (InSelectInfo != ESelectInfo::Direct)
                            { HandleClassPicked(MoveTemp(InOption)); }
                        })
                ]
        ];
}

auto SCkSmDebugger_PreviewPane::HandleClassPicked(TSharedPtr<FClassOption> InOption) -> void
{
    _SelectedInitialStateClass = InOption.IsValid() ? InOption->Class : nullptr;
    if (_ClassPicker.IsValid())
    { _ClassPicker->SetIsOpen(false); }
    RebuildPreviewGraph();
}

auto SCkSmDebugger_PreviewPane::GetSelectedClassLabel() const -> FString
{
    auto* SelectedClass = _SelectedInitialStateClass.Get();
    return ck::IsValid(SelectedClass)
        ? UCk_Utils_Object_UE::Get_CleanClassName(SelectedClass)
        : FString(TEXT("None"));
}
