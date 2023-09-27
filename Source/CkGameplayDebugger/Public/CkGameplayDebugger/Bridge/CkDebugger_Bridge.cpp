#include "CkDebugger_Bridge.h"

#include "CkCore/Algorithms/CkAlgorithms.h"
#include "CkCore/Ensure/CkEnsure.h"

#include "CkGameplayDebugger/Action/CkDebugger_Action.h"
#include "CkGameplayDebugger/Filter/CkDebugger_Filter.h"
#include "CkGameplayDebugger/Profile/CkDebugger_Profile.h"
#include "CkGameplayDebugger/Submenu/CkDebugger_Submenu.h"
#include "CkGameplayDebugger/Engine/CkDebugger_GameplayDebuggerCategoryReplicator.h"

#include <GameFramework/PlayerController.h>

// --------------------------------------------------------------------------------------------------------------------

ACk_GameplayDebugger_DebugBridge_UE::
    ACk_GameplayDebugger_DebugBridge_UE()
{
#if WITH_GAMEPLAY_DEBUGGER
    const auto& gameplayDebugger = FCk_GameplayDebugger_Category::Get_Instance();

    if (ck::Is_NOT_Valid(gameplayDebugger))
    { return; }

    gameplayDebugger->_OnActivatedDelegate.BindDynamic(this, &ThisType::OnGameplayDebugger_Activated);
    gameplayDebugger->_OnDeactivatedDelegate.BindDynamic(this, &ThisType::OnGameplayDebugger_Deactivated);
    gameplayDebugger->_OnCollectDataDelegate.BindDynamic(this, &ThisType::OnGameplayDebugger_CollectData);
    gameplayDebugger->_OnDrawDataDelegate.BindDynamic(this, &ThisType::OnGameplayDebugger_DrawData);
#endif
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    LoadNewDebugProfile(
        UCk_GameplayDebugger_DebugProfile_PDA* InDebugProfile)
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
    CK_ENSURE_IF_NOT(ck::IsValid(InDebugProfile), TEXT("Invalid GameplayDebugger Debug Profile to load!"))
    { return; }

    if (ck::IsValid(_CurrentlyLoadedDebugProfile))
    {
        UnloadCurrentDebugProfile();
    }

    _CurrentlyLoadedDebugProfile = InDebugProfile;
    // TODO: Do Stuff when profile is loaded, instance it ?
#endif
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    UnloadCurrentDebugProfile()
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
    if (ck::Is_NOT_Valid(_CurrentlyLoadedDebugProfile))
    { return; }

    _CurrentlyLoadedDebugProfile = nullptr;
    // TODO: Do stuff ?
#endif
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    OnGameplayDebugger_Activated()
    -> void
{
    if (ck::Is_NOT_Valid(_CurrentlyLoadedDebugProfile))
    { return; }

    // Activate all debug submenus
    ck::algo::ForEachIsValid(_CurrentlyLoadedDebugProfile->Get_Submenus(), [](TObjectPtr<UCk_GameplayDebugger_DebugSubmenu_UE> InSubmenu) -> void
    {
        InSubmenu->ActivateSubmenu();
    });

    // Activate all global debug actions
    ck::algo::ForEachIsValid(_CurrentlyLoadedDebugProfile->Get_GlobalActions(), [](TObjectPtr<UCk_GameplayDebugger_DebugAction_UE> InAction) -> void
    {
        InAction->ActivateAction();
    });

    // Activate the currently selected debug filter
    DoActivateCurrentlySelectedFilter();
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    OnGameplayDebugger_Deactivated()
    -> void
{
    if (ck::Is_NOT_Valid(_CurrentlyLoadedDebugProfile))
    { return; }

    // Deactivate all debug submenus
    ck::algo::ForEachIsValid(_CurrentlyLoadedDebugProfile->Get_Submenus(), [](TObjectPtr<UCk_GameplayDebugger_DebugSubmenu_UE> InSubmenu) -> void
    {
        InSubmenu->DeactivateSubmenu();
    });

    // Deactivate the currently selected debug filter
    DoDeactivateCurrentlySelectedFilter();

    // Deactivate all global debug actions
    ck::algo::ForEachIsValid(_CurrentlyLoadedDebugProfile->Get_GlobalActions(), [](TObjectPtr<UCk_GameplayDebugger_DebugAction_UE> InGlobalAction) -> void
    {
        InGlobalAction->DeactivateAction();
    });
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    OnGameplayDebugger_CollectData(
        const FCk_Payload_GameplayDebugger_OnCollectData& InPayload)
    -> void
{
    // Empty on purpose.
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    OnGameplayDebugger_DrawData(
        const FCk_Payload_GameplayDebugger_OnDrawData& InPayload)
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
    if (ck::Is_NOT_Valid(_CurrentlyLoadedDebugProfile))
    { return; }

    const auto& canvasContext = InPayload.Get_CanvasContext();
    if (ck::Is_NOT_Valid(canvasContext, ck::IsValid_Policy_NullptrOnly{}))
    { return; }

    const auto& ownerPC = InPayload.Get_OwnerPC();
    if (ck::Is_NOT_Valid(ownerPC))
    { return; }

    FString mainMenu;

    ck::algo::ForEachIsValid(_CurrentlyLoadedDebugProfile->Get_Submenus(), [&](const UCk_GameplayDebugger_DebugSubmenu_UE* InSubmenu) -> void
    {
        const auto& keyToShowMenu = InSubmenu->Get_KeyToShowMenu();
        if (ck::Is_NOT_Valid(keyToShowMenu))
        { return; }

        const auto& isShowing = InSubmenu->Get_ShowState() == ECk_GameplayDebugger_DebugSubmenu_ShowState::Visible;

        mainMenu += ck::Format_UE
        (
            TEXT("{{white}}{} {}{}]"),
            InSubmenu->Get_MenuName(),
            isShowing ? TEXT("{{green}}[") : TEXT("{{red}}["),
            InSubmenu->Get_KeyToShowMenu()
        );
    });

    canvasContext->Print(mainMenu);

    DoHandleDebugActorSelectionCycling(InPayload);

    ck::algo::ForEachIsValid(_CurrentlyLoadedDebugProfile->Get_Submenus(), [&](UCk_GameplayDebugger_DebugSubmenu_UE* InSubmenu) -> void
    {
        // NOTE: We handle input in "DrawData" since "CollectData" does not run while the game is paused
        const auto& keyToShowMenu = InSubmenu->Get_KeyToShowMenu();
        if (ck::Is_NOT_Valid(keyToShowMenu))
        { return; }

        if (ownerPC->WasInputKeyJustPressed(keyToShowMenu))
        {
            InSubmenu->ToggleShowState();
        }

        if (InSubmenu->Get_ShowState() == ECk_GameplayDebugger_DebugSubmenu_ShowState::Hidden)
        { return; }

        InSubmenu->DrawData
        (
            FCk_GameplayDebugger_DrawSubmenuData_Params
            {
                InPayload.Get_Replicator()->GetDebugActor(),
                InPayload
            }
        );
    });
#endif
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    DoChangeFilter(
        int32 InPreviousFilterIndex,
        int32 InNewFilterIndex)
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
    if (InPreviousFilterIndex == InNewFilterIndex)
    { return; }

    const auto& debugFilters = _CurrentlyLoadedDebugProfile->Get_Filters();

    const auto& previousFilter = debugFilters[InPreviousFilterIndex];
    previousFilter->DeactivateFilter();

    _CurrentlySelectedFilterIndex = InNewFilterIndex;
    _CurrentlySelectedFilterIndex = _CurrentlySelectedFilterIndex % debugFilters.Num();

    if (_CurrentlySelectedFilterIndex < 0)
    {
        _CurrentlySelectedFilterIndex += debugFilters.Num();
        _CurrentlySelectedActorIndex = 0;
    }

    const auto& newFilter = debugFilters[_CurrentlySelectedFilterIndex];
    newFilter->ActivateFilter();
#endif
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    DoHandleDebugActorSelectionCycling(
        const FCk_Payload_GameplayDebugger_OnDrawData& InDrawData)
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
    const auto& ownerPC = InDrawData.Get_OwnerPC().Get();
    if (ck::Is_NOT_Valid(ownerPC))
    { return; }

    const auto& canvasContext = InDrawData.Get_CanvasContext();
    if (ck::Is_NOT_Valid(canvasContext, ck::IsValid_Policy_NullptrOnly{}))
    { return; }

    const auto& DoPrintMessageToCanvas = [&](const FString& InMessage)
    {
        canvasContext->Print(ck::Format_UE(TEXT("{}"), InMessage));
    };

    if (ck::Is_NOT_Valid(_CurrentlyLoadedDebugProfile))
    { return; }

    const auto& debugFilters = _CurrentlyLoadedDebugProfile->Get_Filters();

    if (debugFilters.IsEmpty())
    {
        _CurrentlySelectedFilterIndex = 0;
        return;
    }

    const auto& debugNavControls = _CurrentlyLoadedDebugProfile->Get_DebugNavControls();

    DoHandleFilterChanges(ownerPC, debugNavControls);

    const auto& currentlySelectedFilter = debugFilters[_CurrentlySelectedFilterIndex];

    DoHandleFilterActionActivationToggling(ownerPC, currentlySelectedFilter);

    if (ck::Is_NOT_Valid(currentlySelectedFilter))
    { return; }

    const auto& sortedFilteredActorList = currentlySelectedFilter->Get_SortedFilteredActors
    (
        FCk_GameplayDebugger_GetSortedFilteredActors_Params{InDrawData}
    );

    const auto& sortedFilteredActors = sortedFilteredActorList.Get_DebugActors();

    const auto& canvasMessage = DoGet_FormattedCanvasMessage(currentlySelectedFilter, sortedFilteredActorList);

    if (sortedFilteredActors.IsEmpty())
    {
        DoPrintMessageToCanvas(canvasMessage);

        _CurrentlySelectedActorIndex = 0;

        return;
    }

    DoHandleSelectedActorChange(ownerPC, debugNavControls, sortedFilteredActors);

    // Update new currently selected debug actor
    const auto& replicator = InDrawData.Get_Replicator().Get();
    if (ck::IsValid(replicator))
    {
        static constexpr auto selectActorInEditor = true;
        replicator->SetDebugActor(sortedFilteredActors[_CurrentlySelectedActorIndex], selectActorInEditor);
    }

    DoPerformActionsOnFilteredActors(currentlySelectedFilter, sortedFilteredActorList, canvasContext);

    DoPrintMessageToCanvas(canvasMessage);
#endif
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    DoActivateCurrentlySelectedFilter() const
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
    const auto& debugFilters = _CurrentlyLoadedDebugProfile->Get_Filters();

    if (debugFilters.IsValidIndex(_CurrentlySelectedFilterIndex))
    {
        const auto& currentlySelectedFilter = debugFilters[_CurrentlySelectedFilterIndex];

        if (ck::IsValid(currentlySelectedFilter))
        {
            currentlySelectedFilter->ActivateFilter();
        }
    }
#endif
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    DoDeactivateCurrentlySelectedFilter() const
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
    const auto& debugFilters = _CurrentlyLoadedDebugProfile->Get_Filters();

    if (debugFilters.IsValidIndex(_CurrentlySelectedFilterIndex))
    {
        const auto& currentlySelectedFilter = debugFilters[_CurrentlySelectedFilterIndex];

        if (ck::IsValid(currentlySelectedFilter))
        {
            currentlySelectedFilter->DeactivateFilter();
        }
    }
#endif
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    DoGet_FormattedCanvasMessage(
        const UCk_GameplayDebugger_DebugFilter_UE* InSelectedFilter,
        const FCk_GameplayDebugger_DebugActorList& InFilteredActors) const
    -> FString
{
    if (ck::Is_NOT_Valid(InSelectedFilter))
    { return {}; }

    FString message;

#if WITH_GAMEPLAY_DEBUGGER
    // Construct Filter message
    {
        message += ck::Format_UE(TEXT("{{white}}CURRENT FILTER: {{cyan}}{}"), InSelectedFilter->Get_FilterName());

        const auto& filteredDebugActors = InFilteredActors.Get_DebugActors();

        if (filteredDebugActors.IsEmpty())
        {
            message += ck::Format_UE(TEXT(" {{red}}[NO ACTORS RETURNED]"));
        }
        else
        {
            message += ck::Format_UE
            (
                TEXT(" {{white}}[{{yellow}}{}{{white}}/{}]"),
                _CurrentlySelectedActorIndex + 1,
                filteredDebugActors.Num()
            );
        }
    }

    const auto& DoGet_ActionMessage = [](TObjectPtr<UCk_GameplayDebugger_DebugAction_UE> InAction) -> FString
    {
        const auto& isActionActive = InAction->Get_IsActive();

        const FString isActionActiveStr = ck::Format_UE
        (
            TEXT("{}{}]"),
            (isActionActive ? TEXT(" {{green}}[") : TEXT(" {{red}}[")),
            InAction->Get_ToggleActivateKey()
        );

        return ck::Format_UE
        (
            TEXT("\n\t{{white}}{} {}"),
            InAction->Get_ActionName(),
            isActionActiveStr
        );
    };

    // Construct Filter Actions message
    {
        message += ck::Format_UE(TEXT("\n{{white}}FILTER ACTIONS:"));
        const auto& filterDebugActions = InSelectedFilter->Get_FilterDebugActions();

        if (filterDebugActions.IsEmpty())
        {
            message += ck::Format_UE(TEXT(" {{red}}[NO ACTIONS]"));
        }
        else
        {
            ck::algo::ForEachIsValid(filterDebugActions, [&](TObjectPtr<UCk_GameplayDebugger_DebugAction_UE> InAction) -> void
            {
                message += DoGet_ActionMessage(InAction);
            });
        }
    }

    // Construct Global Actions Message
    {
        message += ck::Format_UE(TEXT("\n{{white}}GLOBAL ACTIONS:"));

        if (ck::IsValid(_CurrentlyLoadedDebugProfile))
        {
            const auto& globalDebugActions = _CurrentlyLoadedDebugProfile->Get_GlobalActions();

            if (globalDebugActions.IsEmpty())
            {
                message += ck::Format_UE(TEXT(" {{red}}[NO ACTIONS]"));
            }
            else
            {
                ck::algo::ForEachIsValid(globalDebugActions, [&](TObjectPtr<UCk_GameplayDebugger_DebugAction_UE> InAction) -> void
            {
                message += DoGet_ActionMessage(InAction);
            });
            }
        }
    }
#endif

    return message;
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    DoPerformActionsOnFilteredActors(
        const UCk_GameplayDebugger_DebugFilter_UE* InSelectedFilter,
        const FCk_GameplayDebugger_DebugActorList& InFilteredActors,
        FGameplayDebuggerCanvasContext* InCanvasContext) const
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
    const auto& performDebugActionParams = FCk_GameplayDebugger_PerformDebugAction_Params
    {
        InFilteredActors,
        _CurrentlySelectedActorIndex,
        InCanvasContext
    };

    if (ck::IsValid(InSelectedFilter))
    {
        const auto& filterDebugActions = InSelectedFilter->Get_FilterDebugActions();

        ck::algo::ForEachIsValid(filterDebugActions, [&](TObjectPtr<UCk_GameplayDebugger_DebugAction_UE> InAction) -> void
        {
            InAction->PerformDebugAction(performDebugActionParams);
        });
    }

    if (ck::IsValid(_CurrentlyLoadedDebugProfile))
    {
        const auto& globalDebugActions = _CurrentlyLoadedDebugProfile->Get_GlobalActions();

        ck::algo::ForEachIsValid(globalDebugActions, [&](TObjectPtr<UCk_GameplayDebugger_DebugAction_UE> InAction) -> void
        {
            InAction->PerformDebugAction(performDebugActionParams);
        });
    }
#endif
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    DoHandleFilterChanges(
        APlayerController* const& InOwnerPC,
        const FCk_GameplayDebugger_DebugNavControls& InDebugNavControls)
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
    if (ck::Is_NOT_Valid(InOwnerPC))
    { return; }

    const auto currentSelectedFilterIndexCopy = _CurrentlySelectedFilterIndex;

    if (InOwnerPC->WasInputKeyJustPressed(InDebugNavControls.Get_NextFilterKey()))
    {
        DoChangeFilter(_CurrentlySelectedFilterIndex, _CurrentlySelectedFilterIndex + 1);

        if (currentSelectedFilterIndexCopy != _CurrentlySelectedFilterIndex)
        {
            _CurrentlySelectedActorIndex = 0;
        }
    }
    else if (InOwnerPC->WasInputKeyJustPressed(InDebugNavControls.Get_PreviousFilterKey()))
    {
        DoChangeFilter(_CurrentlySelectedFilterIndex, _CurrentlySelectedFilterIndex - 1);
        if (currentSelectedFilterIndexCopy != _CurrentlySelectedFilterIndex)
        {
            _CurrentlySelectedActorIndex = 0;
        }
    }
#endif
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    DoHandleFilterActionActivationToggling(
        APlayerController* const& InOwnerPC,
        const UCk_GameplayDebugger_DebugFilter_UE* InSelectedFilter) const
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
    if (ck::Is_NOT_Valid(InOwnerPC))
    { return; }

    const auto& DoTryToggleAllValidActions = [&](TArray<TObjectPtr<UCk_GameplayDebugger_DebugAction_UE>> InActions) -> void
    {
        ck::algo::ForEachIsValid(InActions, [&](TObjectPtr<UCk_GameplayDebugger_DebugAction_UE> InValidAction) -> void
        {
            if (NOT InOwnerPC->WasInputKeyJustPressed(InValidAction->Get_ToggleActivateKey()))
            { return; }

            InValidAction->ToggleAction();
        });

    };

    if (ck::IsValid(InSelectedFilter))
    {
        const auto& filterDebugActions = InSelectedFilter->Get_FilterDebugActions();

        DoTryToggleAllValidActions(filterDebugActions);
    }

    if (ck::IsValid(_CurrentlyLoadedDebugProfile))
    {
        const auto& globalDebugActions = _CurrentlyLoadedDebugProfile->Get_GlobalActions();

        DoTryToggleAllValidActions(globalDebugActions);
    }
#endif
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    DoHandleSelectedActorChange(
        APlayerController* const& InOwnerPC,
        const FCk_GameplayDebugger_DebugNavControls& InDebugNavControls,
        const TArray<TObjectPtr<AActor>>& InSortedFilteredActors)
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
    if (ck::Is_NOT_Valid(InOwnerPC))
    { return; }

    if (InOwnerPC->WasInputKeyJustPressed(InDebugNavControls.Get_NextActorKey()))
    {
        ++_CurrentlySelectedActorIndex;
    }
    else if (InOwnerPC->WasInputKeyJustPressed(InDebugNavControls.Get_PreviousActorKey()))
    {
        --_CurrentlySelectedActorIndex;
    }
    if (InOwnerPC->WasInputKeyJustPressed(InDebugNavControls.Get_FirstActorKey()))
    {
        _CurrentlySelectedActorIndex = 0;
    }
    else if (InOwnerPC->WasInputKeyJustPressed(InDebugNavControls.Get_LastActorKey()))
    {
        _CurrentlySelectedActorIndex = InSortedFilteredActors.Num() - 1;
    }

    _CurrentlySelectedActorIndex = _CurrentlySelectedActorIndex % InSortedFilteredActors.Num();

    if (_CurrentlySelectedActorIndex < 0)
    {
        _CurrentlySelectedActorIndex += InSortedFilteredActors.Num();
    }
#endif
}

// --------------------------------------------------------------------------------------------------------------------
