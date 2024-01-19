#include "CkDebugger_Bridge.h"

#include "GameplayDebuggerConfig.h"

#include "CkCore/Algorithms/CkAlgorithms.h"
#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/Math/Arithmetic/CkArithmetic_Utils.h"
#include "CkCore/Object/CkObject_Utils.h"

#include "CkInput/CkInput_Utils.h"

#include "CkGameplayDebugger/Action/CkDebugger_Action.h"
#include "CkGameplayDebugger/Filter/CkDebugger_Filter.h"
#include "CkGameplayDebugger/Profile/CkDebugger_Profile.h"
#include "CkGameplayDebugger/Submenu/CkDebugger_Submenu.h"
#include "CkGameplayDebugger/Engine/CkDebugger_GameplayDebuggerCategoryReplicator.h"
#include "CkGameplayDebugger/Settings/CkDebugger_Settings.h"
#include "CkGameplayDebugger/Widget/CkDebugger_Widget.h"

#include <GameFramework/PlayerController.h>

// --------------------------------------------------------------------------------------------------------------------

ACk_GameplayDebugger_DebugBridge_UE::
    ACk_GameplayDebugger_DebugBridge_UE()
{
#if WITH_GAMEPLAY_DEBUGGER
    const auto& GameplayDebugger = FCk_GameplayDebugger_Category::Get_Instance();

    if (ck::Is_NOT_Valid(GameplayDebugger))
    { return; }

    GameplayDebugger->_OnActivatedDelegate.BindDynamic(this, &ThisType::OnGameplayDebugger_Activated);
    GameplayDebugger->_OnDeactivatedDelegate.BindDynamic(this, &ThisType::OnGameplayDebugger_Deactivated);
    GameplayDebugger->_OnCollectDataDelegate.BindDynamic(this, &ThisType::OnGameplayDebugger_CollectData);
    GameplayDebugger->_OnDrawDataDelegate.BindDynamic(this, &ThisType::OnGameplayDebugger_DrawData);
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

    _CurrentlyLoadedDebugProfile = UCk_Utils_Object_UE::Request_CloneObject(InDebugProfile, InDebugProfile);
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

    if (ck::IsValid(_DebugWidget))
    {
        _DebugWidget->RemoveFromParent();
        _DebugWidget = nullptr;
    }

    _CurrentlyLoadedDebugProfile = nullptr;
#endif
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    OnGameplayDebugger_Activated()
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
    if (ck::Is_NOT_Valid(_CurrentlyLoadedDebugProfile))
    { return; }

    if (const auto& DebugWidgetToCreate = _CurrentlyLoadedDebugProfile->Get_HUD_DebugWidget(); ck::IsValid(DebugWidgetToCreate))
    {
        _DebugWidget = UCk_GameplayDebugger_DebugWidget_UE::Create
        (
            GetWorld(),
            DebugWidgetToCreate,
            FCk_GameplayDebugger_DebugWidget_Params{_CurrentlyLoadedDebugProfile->Get_DebugNavControls()}
        );

        if (ck::IsValid(_DebugWidget))
        {
            static constexpr auto zOrder = 9999;
            _DebugWidget->AddToViewport(zOrder);
        }
    }

    // Activate all debug submenus
    ck::algo::ForEachIsValid(_CurrentlyLoadedDebugProfile->Get_Submenus(),
    [](TObjectPtr<UCk_GameplayDebugger_DebugSubmenu_UE> InSubmenu) -> void
    {
        InSubmenu->ActivateSubmenu();
    });

    // Activate all global debug actions
    ck::algo::ForEachIsValid(_CurrentlyLoadedDebugProfile->Get_GlobalActions(),
    [](TObjectPtr<UCk_GameplayDebugger_DebugAction_UE> InAction) -> void
    {
        InAction->ActivateAction();
    });

    // Activate the currently selected debug filter
    DoActivateCurrentlySelectedFilter();
#endif
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    OnGameplayDebugger_Deactivated()
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
    if (ck::Is_NOT_Valid(_CurrentlyLoadedDebugProfile))
    { return; }

    if (ck::IsValid(_DebugWidget))
    {
        _DebugWidget->RemoveFromParent();
        _DebugWidget= nullptr;
    }

    // Deactivate all debug submenus
    ck::algo::ForEachIsValid(_CurrentlyLoadedDebugProfile->Get_Submenus(),
    [](TObjectPtr<UCk_GameplayDebugger_DebugSubmenu_UE> InSubmenu) -> void
    {
        InSubmenu->DeactivateSubmenu();
    });

    // Deactivate the currently selected debug filter
    DoDeactivateCurrentlySelectedFilter();

    // Deactivate all global debug actions
    ck::algo::ForEachIsValid(_CurrentlyLoadedDebugProfile->Get_GlobalActions(),
    [](TObjectPtr<UCk_GameplayDebugger_DebugAction_UE> InGlobalAction) -> void
    {
        InGlobalAction->DeactivateAction();
    });
#endif
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

    const auto& CanvasContext = InPayload.Get_CanvasContext();
    if (ck::Is_NOT_Valid(CanvasContext, ck::IsValid_Policy_NullptrOnly{}))
    { return; }

    const auto& OwnerPC = InPayload.Get_OwnerPC();
    if (ck::Is_NOT_Valid(OwnerPC))
    { return; }

    if (UCk_Utils_GameplayDebugger_UserSettings_UE::Get_DisplayTranslucentBackground())
    {
        FVector2D OutViewportSize;
        GEngine->GameViewport->GetViewportSize(OutViewportSize);

        constexpr auto BackgroundPadding = 5.0f;

        const auto& BackgroundWidth = [&]()
        {
            switch (const auto& SelectedWidth = UCk_Utils_GameplayDebugger_UserSettings_UE::Get_BackgroundWidth())
            {
                case ECk_GameplayDebugger_BackgroundWidth::OneThird:
                {
                    return OutViewportSize.X / 3.0f;
                }
                case ECk_GameplayDebugger_BackgroundWidth::Half:
                {
                    return OutViewportSize.X / 2.0f;
                }
                case ECk_GameplayDebugger_BackgroundWidth::ThreeFourth:
                {
                    return OutViewportSize.X * 0.75f;
                }
                case ECk_GameplayDebugger_BackgroundWidth::Full:
                {
                    return OutViewportSize.X;
                }
                default:
                {
                    CK_INVALID_ENUM(SelectedWidth);
                    return OutViewportSize.X;
                }
            }
        }();

        const auto BackgroundSize = FVector2D(BackgroundWidth - (2.0f * BackgroundPadding), OutViewportSize.Y);
        const auto BackgroundColor = UCk_Utils_GameplayDebugger_UserSettings_UE::Get_BackgroundColor();

        FCanvasTileItem Background(FVector2D::ZeroVector, GWhiteTexture, BackgroundSize, BackgroundColor);
        Background.BlendMode = SE_BLEND_Translucent;

        CanvasContext->DrawItem(Background, CanvasContext->DefaultX - BackgroundPadding, CanvasContext->DefaultY - BackgroundPadding);
    }

    FString MainMenu;

    ck::algo::ForEachIsValid(_CurrentlyLoadedDebugProfile->Get_Submenus(),
    [&](const UCk_GameplayDebugger_DebugSubmenu_UE* InSubmenu) -> void
    {
        if (const auto& KeyToShowMenu = InSubmenu->Get_KeyToShowMenu(); ck::Is_NOT_Valid(KeyToShowMenu))
        { return; }

        const auto& IsSubmenuVisible = InSubmenu->Get_ShowState() == ECk_GameplayDebugger_DebugSubmenu_ShowState::Visible;

        MainMenu += ck::Format_UE
        (
            TEXT("{{white}}{} {}{}] "),
            InSubmenu->Get_MenuName(),
            IsSubmenuVisible ? TEXT("{{green}}[") : TEXT("{{red}}["),
            InSubmenu->Get_KeyToShowMenu()
        );
    });

    CanvasContext->Print(MainMenu);

    DoHandleDebugActorSelectionCycling(InPayload);

    ck::algo::ForEachIsValid(_CurrentlyLoadedDebugProfile->Get_Submenus(),
    [&](UCk_GameplayDebugger_DebugSubmenu_UE* InSubmenu) -> void
    {
        // NOTE: We handle input in "DrawData" since "CollectData" does not run while the game is paused
        const auto& KeyToShowMenu = InSubmenu->Get_KeyToShowMenu();
        if (ck::Is_NOT_Valid(KeyToShowMenu))
        { return; }

        const auto& StickyModifierKey = _CurrentlyLoadedDebugProfile->Get_DebugNavControls().Get_StickyModiferKey();
        if (UCk_Utils_Input_UE::WasInputKeyJustPressed_WithCustomModifier(OwnerPC.Get(), KeyToShowMenu, StickyModifierKey))
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

    const auto& DebugFilters = _CurrentlyLoadedDebugProfile->Get_Filters();

    const auto& PreviousFilter = DebugFilters[InPreviousFilterIndex];
    PreviousFilter->DeactivateFilter();

    _CurrentlySelectedFilterIndex = InNewFilterIndex;
    _CurrentlySelectedFilterIndex = _CurrentlySelectedFilterIndex % DebugFilters.Num();

    if (_CurrentlySelectedFilterIndex < 0)
    {
        _CurrentlySelectedFilterIndex += DebugFilters.Num();
        _CurrentlySelectedActorIndex = 0;
    }

    const auto& NewFilter = DebugFilters[_CurrentlySelectedFilterIndex];
    NewFilter->ActivateFilter();
#endif
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    DoHandleDebugActorSelectionCycling(
        const FCk_Payload_GameplayDebugger_OnDrawData& InDrawData)
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
    const auto& OwnerPC = InDrawData.Get_OwnerPC().Get();
    if (ck::Is_NOT_Valid(OwnerPC))
    { return; }

    const auto& CanvasContext = InDrawData.Get_CanvasContext();
    if (ck::Is_NOT_Valid(CanvasContext, ck::IsValid_Policy_NullptrOnly{}))
    { return; }

    const auto& DoPrintMessageToCanvas = [&](const FString& InMessage)
    {
        CanvasContext->Print(ck::Format_UE(TEXT("{}"), InMessage));
    };

    const auto& UpdateSelectedDebugActor = [&](AActor* InSelectedDebugActor)
    {
        const auto& Replicator = InDrawData.Get_Replicator().Get();
        if (ck::Is_NOT_Valid(Replicator))
        { return; }

        static constexpr auto SelectActorInEditor = true;
        _PreviouslySelectedActor = InSelectedDebugActor;
        Replicator->SetDebugActor(InSelectedDebugActor, SelectActorInEditor);
    };

    if (ck::Is_NOT_Valid(_CurrentlyLoadedDebugProfile))
    { return; }

    const auto& DebugFilters = _CurrentlyLoadedDebugProfile->Get_Filters();

    if (DebugFilters.IsEmpty())
    {
        _CurrentlySelectedFilterIndex = 0;
        return;
    }

    const auto& DebugNavControls = _CurrentlyLoadedDebugProfile->Get_DebugNavControls();

    DoHandleFilterChanges(OwnerPC, DebugNavControls);
    DoHandleWorldChange(OwnerPC, DebugNavControls, InDrawData.Get_AvailableWorlds());

    const auto& CurrentlySelectedFilter = DebugFilters[_CurrentlySelectedFilterIndex];

    DoHandleFilterActionActivationToggling(OwnerPC, DebugNavControls, CurrentlySelectedFilter);

    if (ck::Is_NOT_Valid(CurrentlySelectedFilter))
    { return; }

    const auto CurrentWorldToUse = InDrawData.Get_AvailableWorlds()[_CurrentWorldToUseIndex];
    {
        // followed GameplayDebuggerLocalController:307
        const auto NetModeText = [&]()
        {
            if (CurrentWorldToUse->IsNetMode(NM_DedicatedServer))
            {
                return TEXT("== Dedicated Server ==");
            }
            if (CurrentWorldToUse->IsNetMode(NM_Client))
            {
                return TEXT("== Client == ");
            }
            if (CurrentWorldToUse->IsNetMode(NM_ListenServer))
            {
                return TEXT("== Listen Server ==");
            }
            if (CurrentWorldToUse->IsNetMode(NM_Standalone))
            {
                return TEXT("== Standalone ==");
            }

            return TEXT("NO NETMODE");
        }();

        float TimestampSizeX = 0.0f, TimestampSizeY = 0.0f;
        InDrawData.Get_CanvasContext()->MeasureString(NetModeText, TimestampSizeX, TimestampSizeY);

        const auto SettingsCDO = UGameplayDebuggerConfig::StaticClass()->GetDefaultObject<UGameplayDebuggerConfig>();
        const auto PaddingLeft = SettingsCDO->DebugCanvasPaddingLeft;
        const auto PaddingRight = SettingsCDO->DebugCanvasPaddingRight;
        const auto PaddingTop = SettingsCDO->DebugCanvasPaddingTop;

        const auto SimulateMode = FGameplayDebuggerAddonBase::IsSimulateInEditor() || InDrawData.Get_Replicator()->IsEditorWorldReplicator();

        const float DPIScale = InDrawData.Get_CanvasContext()->Canvas->GetDPIScale();
        const float CanvasSizeX = (InDrawData.Get_CanvasContext()->Canvas->SizeX / DPIScale) - PaddingLeft - PaddingRight;
        const float UsePaddingTop = PaddingTop + (SimulateMode ? 30.0f : 0) + 30.0f;

        InDrawData.Get_CanvasContext()->PrintAt((CanvasSizeX - TimestampSizeX) * 0.5f, UsePaddingTop, FColor::Cyan, FString{NetModeText}.ToUpper());
    }

    const auto& SortedFilteredActorList = CurrentlySelectedFilter->Get_SortedFilteredActors
    (
        FCk_GameplayDebugger_GetSortedFilteredActors_Params
        {
            FCk_Payload_GameplayDebugger_OnDrawData
            {
                InDrawData.Get_OwnerPC(),
                InDrawData.Get_CanvasContext(),
                InDrawData.Get_Replicator(),
                InDrawData.Get_AvailableWorlds(),
                InDrawData.Get_AvailableWorlds()[_CurrentWorldToUseIndex]
            }
        }
    );

    const auto& SortedFilteredActors = SortedFilteredActorList.Get_DebugActors();

    const auto& CanvasMessage = DoGet_FormattedCanvasMessage(CurrentlySelectedFilter, SortedFilteredActorList);

    if (SortedFilteredActors.IsEmpty())
    {
        UpdateSelectedDebugActor(nullptr);
        DoPrintMessageToCanvas(CanvasMessage);

        _CurrentlySelectedActorIndex = 0;

        return;
    }

    DoHandleSelectedActorChange(OwnerPC, DebugNavControls, SortedFilteredActors);

    UpdateSelectedDebugActor(SortedFilteredActors[_CurrentlySelectedActorIndex]);

    DoPerformActionsOnFilteredActors(CurrentlySelectedFilter, SortedFilteredActorList, CanvasContext);

    DoPrintMessageToCanvas(CanvasMessage);
#endif
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    DoActivateCurrentlySelectedFilter() const
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
    const auto& DebugFilters = _CurrentlyLoadedDebugProfile->Get_Filters();

    if (DebugFilters.IsValidIndex(_CurrentlySelectedFilterIndex))
    {
        const auto& CurrentlySelectedFilter = DebugFilters[_CurrentlySelectedFilterIndex];

        if (ck::IsValid(CurrentlySelectedFilter))
        {
            CurrentlySelectedFilter->ActivateFilter();
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
    const auto& DebugFilters = _CurrentlyLoadedDebugProfile->Get_Filters();

    if (DebugFilters.IsValidIndex(_CurrentlySelectedFilterIndex))
    {
        const auto& CurrentlySelectedFilter = DebugFilters[_CurrentlySelectedFilterIndex];

        if (ck::IsValid(CurrentlySelectedFilter))
        {
            CurrentlySelectedFilter->DeactivateFilter();
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

    FString Message;

#if WITH_GAMEPLAY_DEBUGGER
    // Construct Filter message
    {
        Message += ck::Format_UE(TEXT("{{white}}CURRENT FILTER: {{cyan}}{}"), InSelectedFilter->Get_FilterName());

        const auto& FilteredDebugActors = InFilteredActors.Get_DebugActors();

        if (FilteredDebugActors.IsEmpty())
        {
            Message += ck::Format_UE(TEXT(" {{red}}[NO ACTORS RETURNED]"));
        }
        else
        {
            Message += ck::Format_UE
            (
                TEXT(" {{white}}[{{yellow}}{}{{white}}/{}]"),
                _CurrentlySelectedActorIndex + 1,
                FilteredDebugActors.Num()
            );
        }
    }

    const auto& DoGet_ActionMessage = [](TObjectPtr<UCk_GameplayDebugger_DebugAction_UE> InAction) -> FString
    {
        const auto& IsActionActive = InAction->Get_IsActive();

        const auto IsActionActiveStr = ck::Format_UE
        (
            TEXT("{}{}]"),
            (IsActionActive ? TEXT(" {{green}}[") : TEXT(" {{red}}[")),
            InAction->Get_ToggleActivateKey()
        );

        return ck::Format_UE
        (
            TEXT("\n\t{{white}}{} {}"),
            InAction->Get_ActionName(),
            IsActionActiveStr
        );
    };

    // Construct Filter Actions message
    {
        Message += ck::Format_UE(TEXT("\n{{white}}FILTER ACTIONS:"));
        const auto& FilterDebugActions = InSelectedFilter->Get_FilterDebugActions();

        if (FilterDebugActions.IsEmpty())
        {
            Message += ck::Format_UE(TEXT(" {{red}}[NO ACTIONS]"));
        }
        else
        {
            ck::algo::ForEachIsValid(FilterDebugActions,
            [&](TObjectPtr<UCk_GameplayDebugger_DebugAction_UE> InAction) -> void
            {
                Message += DoGet_ActionMessage(InAction);
            });
        }
    }

    // Construct Global Actions Message
    {
        Message += ck::Format_UE(TEXT("\n{{white}}GLOBAL ACTIONS:"));

        if (ck::IsValid(_CurrentlyLoadedDebugProfile))
        {
            const auto& GlobalDebugActions = _CurrentlyLoadedDebugProfile->Get_GlobalActions();

            if (GlobalDebugActions.IsEmpty())
            {
                Message += ck::Format_UE(TEXT(" {{red}}[NO ACTIONS]"));
            }
            else
            {
                ck::algo::ForEachIsValid(GlobalDebugActions,
                [&](TObjectPtr<UCk_GameplayDebugger_DebugAction_UE> InAction) -> void
            {
                Message += DoGet_ActionMessage(InAction);
            });
            }
        }
    }
#endif

    return Message;
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
    const auto& PerformDebugActionParams = FCk_GameplayDebugger_PerformDebugAction_Params
    {
        InFilteredActors,
        _CurrentlySelectedActorIndex,
        InCanvasContext
    };

    if (ck::IsValid(InSelectedFilter))
    {
        const auto& FilterDebugActions = InSelectedFilter->Get_FilterDebugActions();

        ck::algo::ForEachIsValid(FilterDebugActions,
        [&](TObjectPtr<UCk_GameplayDebugger_DebugAction_UE> InAction) -> void
        {
            InAction->PerformDebugAction(PerformDebugActionParams);
        });
    }

    if (ck::IsValid(_CurrentlyLoadedDebugProfile))
    {
        const auto& GlobalDebugActions = _CurrentlyLoadedDebugProfile->Get_GlobalActions();

        ck::algo::ForEachIsValid(GlobalDebugActions,
        [&](TObjectPtr<UCk_GameplayDebugger_DebugAction_UE> InAction) -> void
        {
            InAction->PerformDebugAction(PerformDebugActionParams);
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

    const auto CurrentSelectedFilterIndexCopy = _CurrentlySelectedFilterIndex;

    if (UCk_Utils_Input_UE::WasInputKeyJustPressed(InOwnerPC, InDebugNavControls.Get_NextFilterKey()))
    {
        DoChangeFilter(_CurrentlySelectedFilterIndex, _CurrentlySelectedFilterIndex + 1);

        if (CurrentSelectedFilterIndexCopy != _CurrentlySelectedFilterIndex)
        {
            _CurrentlySelectedActorIndex = 0;
        }
    }
    else if (UCk_Utils_Input_UE::WasInputKeyJustPressed(InOwnerPC, InDebugNavControls.Get_PreviousFilterKey()))
    {
        DoChangeFilter(_CurrentlySelectedFilterIndex, _CurrentlySelectedFilterIndex - 1);
        if (CurrentSelectedFilterIndexCopy != _CurrentlySelectedFilterIndex)
        {
            _CurrentlySelectedActorIndex = 0;
        }
    }
#endif
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    DoHandleWorldChange(
        APlayerController* const& InOwnerPC,
        const FCk_GameplayDebugger_DebugNavControls& InDebugNavControls,
        const TArray<TWeakObjectPtr<UWorld>>& InAvailableWorlds)
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
    if (ck::Is_NOT_Valid(InOwnerPC))
    { return; }

    const auto CurrentSelectedFilterIndexCopy = _CurrentlySelectedFilterIndex;

    if (UCk_Utils_Input_UE::WasInputKeyJustPressed(InOwnerPC, InDebugNavControls.Get_NextWorldKey()))
    {
        _CurrentWorldToUseIndex = UCk_Utils_Arithmetic_UE::Get_Increment_WithWrap(_CurrentWorldToUseIndex, FCk_IntRange{0, InAvailableWorlds.Num()});
    }
    else if (UCk_Utils_Input_UE::WasInputKeyJustPressed(InOwnerPC, InDebugNavControls.Get_PrevWorldKey()))
    {
        _CurrentWorldToUseIndex = UCk_Utils_Arithmetic_UE::Get_Decrement_WithWrap(_CurrentWorldToUseIndex, FCk_IntRange{0, InAvailableWorlds.Num()});
    }
    else if (UCk_Utils_Input_UE::WasInputKeyJustPressed(InOwnerPC, InDebugNavControls.Get_ServerClientWorldToggleKey()))
    {
        const auto SelectedWorldIndex = InAvailableWorlds.IndexOfByPredicate([&](const TWeakObjectPtr<UWorld>& InWorld)
        {
            const auto CurrentWorldNetMode = InAvailableWorlds[_CurrentWorldToUseIndex]->GetNetMode();

            if (CurrentWorldNetMode == NM_Client)
            {
                if (InWorld->IsNetMode(NM_DedicatedServer) || InWorld->IsNetMode(NM_ListenServer) || InWorld->IsNetMode(NM_Standalone))
                { return true; }
            }
            else
            {
                if (InWorld->IsNetMode(NM_Client))
                { return true; }
            }

            return false;
        });

        if (SelectedWorldIndex != INDEX_NONE)
        { _CurrentWorldToUseIndex = SelectedWorldIndex; }
    }
#endif
}

auto
    ACk_GameplayDebugger_DebugBridge_UE::
    DoHandleFilterActionActivationToggling(
        APlayerController* const& InOwnerPC,
        const FCk_GameplayDebugger_DebugNavControls& InDebugNavControls,
        const UCk_GameplayDebugger_DebugFilter_UE* InSelectedFilter) const
    -> void
{
#if WITH_GAMEPLAY_DEBUGGER
    if (ck::Is_NOT_Valid(InOwnerPC))
    { return; }

    const auto& DoTryToggleAllValidActions = [&](TArray<TObjectPtr<UCk_GameplayDebugger_DebugAction_UE>> InActions) -> void
    {
        ck::algo::ForEachIsValid(InActions,
        [&](TObjectPtr<UCk_GameplayDebugger_DebugAction_UE> InValidAction) -> void
        {
            if (NOT UCk_Utils_Input_UE::WasInputKeyJustPressed_WithCustomModifier(InOwnerPC, InValidAction->Get_ToggleActivateKey(), InDebugNavControls.Get_StickyModiferKey()))
            { return; }

            InValidAction->ToggleAction();
        });
    };

    if (ck::IsValid(InSelectedFilter))
    {
        const auto& FilterDebugActions = InSelectedFilter->Get_FilterDebugActions();

        DoTryToggleAllValidActions(FilterDebugActions);
    }

    if (ck::IsValid(_CurrentlyLoadedDebugProfile))
    {
        const auto& GlobalDebugActions = _CurrentlyLoadedDebugProfile->Get_GlobalActions();

        DoTryToggleAllValidActions(GlobalDebugActions);
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
    const auto PreviouslySelectedActorIndex = _CurrentlySelectedActorIndex;

#if WITH_GAMEPLAY_DEBUGGER
    if (ck::Is_NOT_Valid(InOwnerPC))
    { return; }

    const auto& StickyModifierKey = InDebugNavControls.Get_StickyModiferKey();

    if (UCk_Utils_Input_UE::WasInputKeyJustPressed(InOwnerPC, InDebugNavControls.Get_NextActorKey()))
    {
        ++_CurrentlySelectedActorIndex;
    }
    else if (UCk_Utils_Input_UE::WasInputKeyJustPressed(InOwnerPC, InDebugNavControls.Get_PreviousActorKey()))
    {
        --_CurrentlySelectedActorIndex;
    }

    if (UCk_Utils_Input_UE::WasInputKeyJustPressed(InOwnerPC, InDebugNavControls.Get_FirstActorKey()))
    {
        _CurrentlySelectedActorIndex = 0;
    }
    else if (UCk_Utils_Input_UE::WasInputKeyJustPressed(InOwnerPC, InDebugNavControls.Get_LastActorKey()))
    {
        _CurrentlySelectedActorIndex = InSortedFilteredActors.Num() - 1;
    }

    // Enforce Selected Actor stability if user did not request any selection change
    if (PreviouslySelectedActorIndex == _CurrentlySelectedActorIndex && ck::IsValid(_PreviouslySelectedActor))
    {
        const auto& IndexOfPreviouslySelectedActor = InSortedFilteredActors.IndexOfByPredicate([&](const TObjectPtr<AActor> InSortedFilteredActor)
        {
            return InSortedFilteredActor == _PreviouslySelectedActor.Get();
        });

        if (IndexOfPreviouslySelectedActor != INDEX_NONE)
        {
            _CurrentlySelectedActorIndex = IndexOfPreviouslySelectedActor;
        }
    }

    _CurrentlySelectedActorIndex = _CurrentlySelectedActorIndex % InSortedFilteredActors.Num();

    if (_CurrentlySelectedActorIndex < 0)
    {
        _CurrentlySelectedActorIndex += InSortedFilteredActors.Num();
    }
#endif
}

// --------------------------------------------------------------------------------------------------------------------
