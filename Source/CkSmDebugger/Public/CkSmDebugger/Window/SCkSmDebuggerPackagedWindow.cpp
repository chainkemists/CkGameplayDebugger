#include "CkSmDebugger/Window/SCkSmDebuggerPackagedWindow.h"

#include "CkSmDebugger/ViewModel/CkSmDebugger_ViewModel.h"
#include "CkSmDebugger/Window/SCkSmDebugger_HistoryList.h"
#include "CkSmDebugger/Graph/SCkSmRuntimeGraph.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Lifecycle/CkDebug_SessionLifecycle.h"
#include "CkDebuggerCommon/Models/CkDebuggerModel_WorldSelector.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_WorldSelector.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"

#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

const FName SCkSmDebuggerPackagedWindow::WindowId = FName(TEXT("SmDebugger"));

namespace
{
    auto GetRunStatusText(const ECk_SmRunStatus InStatus) -> FText
    {
        switch (InStatus)
        {
        case ECk_SmRunStatus::Running: return FText::FromString(TEXT("Running"));
        case ECk_SmRunStatus::Paused: return FText::FromString(TEXT("Paused"));
        default: return FText::FromString(TEXT("Stopped"));
        }
    }

    auto GetConditionText(const FCkSmDebugger_TransitionInfo& InTransition) -> FString
    {
        auto Parts = TArray<FString>{};
        for (const auto& Condition : InTransition.Conditions)
        {
            Parts.Add(FString::Printf(TEXT("%s %s"), CkSmDebugger::GetConditionResultLabel(Condition.Result), *Condition.ClassName));
        }
        return Parts.IsEmpty() ? TEXT("Unconditional") : FString::Join(Parts, TEXT(" | "));
    }
}

SCkSmDebuggerPackagedWindow::~SCkSmDebuggerPackagedWindow()
{
    if (_WorldModel.IsValid() && _WorldChangedHandle.IsValid())
    { _WorldModel->OnWorldChanged.Remove(_WorldChangedHandle); }
    if (_SessionInvalidatedHandle.IsValid())
    { ck::DebugSessionLifecycle::Get_OnSessionInvalidated().Remove(_SessionInvalidatedHandle); }
}

auto SCkSmDebuggerPackagedWindow::Construct(const FArguments& InArgs) -> void
{
    Register_WithGate();
    _ViewModel = MakeShared<FCkSmDebugger_ViewModel>();
    _WorldModel = MakeShared<FCkDebuggerModel_WorldSelector>();
    _WorldChangedHandle = _WorldModel->OnWorldChanged.AddSP(this, &SCkSmDebuggerPackagedWindow::HandleWorldChanged);
    _SessionInvalidatedHandle = ck::DebugSessionLifecycle::Get_OnSessionInvalidated().AddSP(this, &SCkSmDebuggerPackagedWindow::HandleSessionInvalidated);

    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(6.0f)
        [ BuildToolbar() ]
        + SVerticalBox::Slot().FillHeight(1.0f).Padding(6.0f, 0.0f)
        [
            SNew(SSplitter)
            + SSplitter::Slot().Value(0.68f)
            [
                SAssignNew(_RuntimeGraph, SCkSmRuntimeGraph)
                    .OnSelectionChanged(FOnCkSmRuntimeGraphSelection::CreateLambda([this](const int32 InStateIndex, const int32 /*InTransitionIndex*/)
                    {
                        if (_ViewModel.IsValid()) { _ViewModel->Set_SelectedNodeIndex(InStateIndex); }
                    }))
            ]
            + SSplitter::Slot().Value(0.32f)
            [
                SNew(SBorder).Padding(4.0f)
                [ SAssignNew(_HistoryList, SCkSmDebugger_HistoryList, _ViewModel) ]
            ]
        ]
    ];
}

auto SCkSmDebuggerPackagedWindow::BuildToolbar() -> TSharedRef<SWidget>
{
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
        [ SNew(SCkDebug_WorldSelector, _WorldModel).ShowHeaderLabel(false) ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
        [
            SAssignNew(_SmSelector, SComboBox<TSharedPtr<FString>>)
                .OptionsSource(&_SmSelectorItems)
                .OnGenerateWidget(this, &SCkSmDebuggerPackagedWindow::GenerateSmOption)
                .OnSelectionChanged(this, &SCkSmDebuggerPackagedWindow::OnSmSelectionChanged)
                [ SAssignNew(_SmSelectorLabel, STextBlock).Text(FText::FromString(TEXT("Select state machine"))) ]
        ]
        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
        [
            SNew(STextBlock).Text_Lambda([this]()
            {
                const auto* Info = _ViewModel.IsValid() ? _ViewModel->Get_CurrentSmInfo() : nullptr;
                if (Info == nullptr) { return FText::GetEmpty(); }
                const auto StateName = Info->States.IsValidIndex(Info->CurrentStateIndex)
                    ? Info->States[Info->CurrentStateIndex].StateName : FString(TEXT("No active state"));
                return FText::FromString(FString::Printf(TEXT("%s  |  %s"), *GetRunStatusText(Info->RunStatus).ToString(), *StateName));
            })
        ];
}

auto SCkSmDebuggerPackagedWindow::Tick(const FGeometry& InAllottedGeometry, double InCurrentTime, float InDeltaTime) -> void
{
    SCkDebugger_WindowBase::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(WindowId)) { return; }

    _WorldModel->Ensure_AutoSelect();
    _CachedWorld = _WorldModel->Get_SelectedWorld();
    auto* World = _CachedWorld.Get();
    if (ck::Is_NOT_Valid(World) || NOT World->HasBegunPlay()) { return; }

    _ViewModel->Tick(World, InDeltaTime);
    RefreshSmSelector();

    if (_PendingTarget.IsSet())
    {
        const auto Target = _PendingTarget.GetValue();
        _PendingTarget.Reset();
        for (auto Index = 0; Index < _SmSelectorHandles.Num(); ++Index)
        {
            if (_SmSelectorHandles[Index].Get_Entity() != Target) { continue; }
            _ViewModel->Set_SelectedSmHandle(_SmSelectorHandles[Index]);
            if (_SmSelector.IsValid()) { _SmSelector->SetSelectedItem(_SmSelectorItems[Index]); }
            break;
        }
    }

    RefreshDetailLists();
    if (_RuntimeGraph.IsValid()) { _RuntimeGraph->SetSmInfo(_ViewModel->Get_CurrentSmInfo()); }
}

auto SCkSmDebuggerPackagedWindow::TargetEntity(const FCk_Handle& InEntity) -> void
{
    if (ck::IsValid(InEntity)) { _PendingTarget = InEntity.Get_Entity(); }
}

auto SCkSmDebuggerPackagedWindow::OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) -> FReply
{
    if (InKeyEvent.GetKey() == EKeys::F && _RuntimeGraph.IsValid())
    {
        _RuntimeGraph->FrameAll();
        return FReply::Handled();
    }
    return SCkDebugger_WindowBase::OnKeyDown(InGeometry, InKeyEvent);
}

auto SCkSmDebuggerPackagedWindow::HandleWorldChanged(UWorld* InWorld) -> void
{
    _CachedWorld = InWorld;
    HandleSessionInvalidated();
}

auto SCkSmDebuggerPackagedWindow::HandleSessionInvalidated() -> void
{
    if (_ViewModel.IsValid()) { _ViewModel->Reset_ForWorldChange(); }
    _PendingTarget.Reset();
    _SmSelectorItems.Empty();
    _SmSelectorHandles.Empty();
    _StateItems.Empty();
    _TransitionItems.Empty();
    _LastDetailHandle = FCk_Handle_StateMachine{};
    _LastStateCount = INDEX_NONE;
    _LastTransitionCount = INDEX_NONE;
    if (_SmSelector.IsValid()) { _SmSelector->RefreshOptions(); }
    if (_StateList.IsValid()) { _StateList->RequestListRefresh(); }
    if (_TransitionList.IsValid()) { _TransitionList->RequestListRefresh(); }
    if (_RuntimeGraph.IsValid()) { _RuntimeGraph->Clear(); }
}

auto SCkSmDebuggerPackagedWindow::RefreshSmSelector() -> void
{
    const auto& AllSms = _ViewModel->Get_AllStateMachines();
    auto Changed = AllSms.Num() != _SmSelectorHandles.Num();
    if (NOT Changed)
    {
        for (auto Index = 0; Index < AllSms.Num(); ++Index)
        {
            if (_SmSelectorHandles[Index] != AllSms[Index].Handle) { Changed = true; break; }
        }
    }
    if (NOT Changed) { return; }

    _SmSelectorItems.Empty(AllSms.Num());
    _SmSelectorHandles.Empty(AllSms.Num());
    for (const auto& Info : AllSms)
    {
        _SmSelectorHandles.Add(Info.Handle);
        _SmSelectorItems.Add(MakeShared<FString>(Info.DebugName));
    }
    if (_SmSelector.IsValid()) { _SmSelector->RefreshOptions(); }
    if (NOT _ViewModel->Has_SelectedSm() && _SmSelectorHandles.Num() > 0)
    {
        _ViewModel->Set_SelectedSmHandle(_SmSelectorHandles[0]);
        if (_SmSelector.IsValid()) { _SmSelector->SetSelectedItem(_SmSelectorItems[0]); }
    }
}

auto SCkSmDebuggerPackagedWindow::RefreshDetailLists() -> void
{
    const auto* Info = _ViewModel->Get_CurrentSmInfo();
    if (Info == nullptr) { return; }
    if (_LastDetailHandle == Info->Handle && _LastStateCount == Info->States.Num() && _LastTransitionCount == Info->Transitions.Num()) { return; }

    _LastDetailHandle = Info->Handle;
    _LastStateCount = Info->States.Num();
    _LastTransitionCount = Info->Transitions.Num();
    _StateItems.Empty(Info->States.Num());
    _TransitionItems.Empty(Info->Transitions.Num());
    for (auto Index = 0; Index < Info->States.Num(); ++Index) { _StateItems.Add(MakeShared<FIndexedItem>(FIndexedItem{Index})); }
    for (auto Index = 0; Index < Info->Transitions.Num(); ++Index) { _TransitionItems.Add(MakeShared<FIndexedItem>(FIndexedItem{Index})); }
    if (_StateList.IsValid()) { _StateList->RequestListRefresh(); }
    if (_TransitionList.IsValid()) { _TransitionList->RequestListRefresh(); }
}

auto SCkSmDebuggerPackagedWindow::GenerateSmOption(TSharedPtr<FString> InItem) -> TSharedRef<SWidget>
{
    return SNew(STextBlock).Text(FText::FromString(InItem.IsValid() ? *InItem : FString{}));
}

auto SCkSmDebuggerPackagedWindow::GenerateStateRow(FIndexedItemPtr InItem, const TSharedRef<STableViewBase>& InOwnerTable) -> TSharedRef<ITableRow>
{
    const auto Index = InItem.IsValid() ? InItem->Index : INDEX_NONE;
    return SNew(STableRow<FIndexedItemPtr>, InOwnerTable)
        [ SNew(STextBlock).Text_Lambda([this, Index]()
        {
            const auto* Info = _ViewModel->Get_CurrentSmInfo();
            if (Info == nullptr || NOT Info->States.IsValidIndex(Index)) { return FText::GetEmpty(); }
            const auto& State = Info->States[Index];
            return FText::FromString(FString::Printf(TEXT("%s%s  (%0.2fs)"), State.IsCurrentState ? TEXT("> ") : TEXT("  "), *State.StateName, State.DwellTimeSeconds));
        }) ];
}

auto SCkSmDebuggerPackagedWindow::GenerateTransitionRow(FIndexedItemPtr InItem, const TSharedRef<STableViewBase>& InOwnerTable) -> TSharedRef<ITableRow>
{
    const auto Index = InItem.IsValid() ? InItem->Index : INDEX_NONE;
    return SNew(STableRow<FIndexedItemPtr>, InOwnerTable)
        [ SNew(STextBlock).AutoWrapText(true).Text_Lambda([this, Index]()
        {
            const auto* Info = _ViewModel->Get_CurrentSmInfo();
            if (Info == nullptr || NOT Info->Transitions.IsValidIndex(Index)) { return FText::GetEmpty(); }
            const auto& Transition = Info->Transitions[Index];
            return FText::FromString(FString::Printf(TEXT("%s -> %s  [%d/%d]\n%s"), *Transition.SourceStateName, *Transition.TargetStateName, Transition.SatisfiedCount, Transition.TotalCount, *GetConditionText(Transition)));
        }) ];
}

auto SCkSmDebuggerPackagedWindow::OnSmSelectionChanged(TSharedPtr<FString> InItem, ESelectInfo::Type InSelectInfo) -> void
{
    const auto Index = _SmSelectorItems.IndexOfByKey(InItem);
    if (_SmSelectorHandles.IsValidIndex(Index))
    {
        _ViewModel->Set_SelectedSmHandle(_SmSelectorHandles[Index]);
        if (_SmSelectorLabel.IsValid()) { _SmSelectorLabel->SetText(FText::FromString(*InItem)); }
    }
}

auto SCkSmDebuggerPackagedWindow::OnStateSelectionChanged(FIndexedItemPtr InItem, ESelectInfo::Type InSelectInfo) -> void
{
    if (InItem.IsValid()) { _ViewModel->Set_SelectedNodeIndex(InItem->Index); }
}
