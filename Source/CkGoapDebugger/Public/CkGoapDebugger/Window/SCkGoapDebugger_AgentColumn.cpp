#include "CkGoapDebugger/Window/SCkGoapDebugger_AgentColumn.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_NameLabel.h"
#include "CkGoapDebugger/ViewModel/CkGoapDebugger_ViewModel.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Styles/CkDebuggerCommonStyle.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Card.h"
#include "CkGoapDebugger/CkGoapDebugger_Axes.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_Chip.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_GlowWrap.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NumericEditor.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SectionHeader.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Switch.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "CkGoap/Planner/CkGoap_Planner_Utils.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================

namespace ck_goap_debugger_agent_column
{
    constexpr float k_FallbackCostFloor = 900.0f;

    // Width of the settings drawer's numeric fields — the same 90px the drawer's own
    // file-local editor used before SCkDebug_NumericEditor was promoted, so the rows keep their
    // established column instead of adopting the widget's narrower shared default.
    constexpr float SettingsNumericWidth = 90.0f;

    auto Tokens() -> FCkUiView::FTokens
    {
        return {{TEXT("--space-s"), FString::SanitizeFloat(CkStyle::SpaceS)},
                {TEXT("--space-m"), FString::SanitizeFloat(CkStyle::SpaceM)},
                {TEXT("--space-l"), FString::SanitizeFloat(CkStyle::SpaceL)},
                {TEXT("--goap-agent-list-text"), TEXT("#") + CkStyle::Text().ToFColorSRGB().ToHex()},
                {TEXT("--goap-agent-list-text-mute"), TEXT("#") + CkStyle::TextMute().ToFColorSRGB().ToHex()}};
    }

    auto PolicyKey(const ECk_Goap_ReplanPolicy InPolicy) -> FString
    {
        switch (InPolicy)
        {
            case ECk_Goap_ReplanPolicy::OnWorldStateDirty: return TEXT("world-state");
            case ECk_Goap_ReplanPolicy::OnCostDirty: return TEXT("cost");
            case ECk_Goap_ReplanPolicy::OnEitherDirty: return TEXT("either");
            default: return TEXT("explicit");
        }
    }

    auto TextField(const FString& InValue) -> FCkUiFieldValue
    {
        return {.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(InValue)};
    }

    auto MakePolicyOptions() -> TSharedPtr<FCkUiCollection>
    {
        TSharedPtr<FCkUiCollection> Options;
        const FCkUiLoadResult SchemaResult = FCkUiCollection::TryCreate(
            {{TEXT("label"), ECkUiFieldKind::Text}}, Options);
        if (!SchemaResult.Succeeded || !Options.IsValid())
        {
            return nullptr;
        }

        const auto Add = [](const TCHAR* InKey, const TCHAR* InLabel)
        {
            FCkUiRecordData Record;
            Record.Key = InKey;
            Record.Fields.Add(TEXT("label"), TextField(InLabel));
            return Record;
        };
        const FCkUiLoadResult RecordsResult = Options->TrySetRecords(
            {Add(TEXT("world-state"), TEXT("On world-state change")),
             Add(TEXT("cost"), TEXT("On cost change")),
             Add(TEXT("either"), TEXT("On either")),
             Add(TEXT("explicit"), TEXT("Only when asked (Explicit)"))});
        return RecordsResult.Succeeded ? Options : nullptr;
    }

    auto InitialsOf(const FString& InName) -> FString
    {
        if (InName.IsEmpty()) { return TEXT("??"); }
        return InName.Left(2).ToUpper();
    }

    auto StatusTone(ECk_GoapPlanStatus InStatus) -> ECk_Tone
    {
        switch (InStatus)
        {
            case ECk_GoapPlanStatus::PlanFound:            return ECk_Tone::Ok;
            case ECk_GoapPlanStatus::Planning:             return ECk_Tone::Accent;
            case ECk_GoapPlanStatus::PlanFailed:           return ECk_Tone::Err;
            case ECk_GoapPlanStatus::CostThresholdReached: return ECk_Tone::Warn;
            default:                                       return ECk_Tone::Neutral;
        }
    }

    auto StatusText(ECk_GoapPlanStatus InStatus) -> FText
    {
        switch (InStatus)
        {
            case ECk_GoapPlanStatus::PlanFound:            return FText::FromString(TEXT("Plan Found"));
            case ECk_GoapPlanStatus::Planning:             return FText::FromString(TEXT("Planning…"));
            case ECk_GoapPlanStatus::PlanFailed:           return FText::FromString(TEXT("Plan Failed"));
            case ECk_GoapPlanStatus::CostThresholdReached: return FText::FromString(TEXT("Cost Threshold"));
            default:                                       return FText::FromString(TEXT("Idle"));
        }
    }

    auto LeafOfTag(const FGameplayTag& InTag) -> FString
    {
        const auto Full = InTag.ToString();
        auto LastDot = int32{INDEX_NONE};
        return Full.FindLastChar(TEXT('.'), LastDot) ? Full.RightChop(LastDot + 1) : Full;
    }

    // The live world-state truth for a key, from the collected entries.
    auto WsValueOf(const FCkGoapDebugger_PlannerInfo& InPlanner, const FGameplayTag& InKey) -> bool
    {
        const auto* Entry = InPlanner.WorldState.FindByPredicate(
            [&InKey](const FCkGoapDebugger_WorldStateEntry& In) { return In.Key == InKey; });
        return Entry != nullptr && Entry->Value;
    }

    // Cross-pane key trace on every chip: highlight when the key is traced,
    // click toggles the trace (same ViewModel channel as the WS rail / matrix).
    auto MakeTraceAttr(
        const TWeakPtr<FCkGoapDebugger_ViewModel>& InWeakVm,
        const FGameplayTag& InKey) -> TAttribute<bool>
    {
        return TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda(
            [InWeakVm, InKey]() -> bool
            {
                const auto Vm = InWeakVm.Pin();
                return Vm.IsValid() && Vm->Get_TracedWsKey() == InKey;
            }));
    }

    auto MakeTraceClick(
        const TWeakPtr<FCkGoapDebugger_ViewModel>& InWeakVm,
        const FGameplayTag& InKey) -> FOnCkDebug_ChipClicked
    {
        return FOnCkDebug_ChipClicked::CreateLambda([InWeakVm, InKey]()
        {
            const auto Vm = InWeakVm.Pin();
            if (NOT Vm.IsValid()) { return; }
            Vm->Set_TracedWsKey(Vm->Get_TracedWsKey() == InKey ? FGameplayTag{} : InKey);
        });
    }

    auto MakeConditionChip(
        const FCkGoapDebugger_PlannerInfo& InPlanner,
        const FGameplayTag& InKey,
        bool InWantedValue,
        const TWeakPtr<FCkGoapDebugger_ViewModel>& InWeakVm) -> TSharedRef<SWidget>
    {
        const auto Satisfied = WsValueOf(InPlanner, InKey) == InWantedValue;
        const auto Label = InWantedValue
            ? LeafOfTag(InKey)
            : FString::Printf(TEXT("%s = false"), *LeafOfTag(InKey));

        auto Chip = SNew(SCkDebug_Chip)
            .Text(FText::FromString(Label))
            .Kind(Satisfied ? ECkDebug_ChipKind::Satisfied : ECkDebug_ChipKind::Unsatisfied)
            .Highlighted(MakeTraceAttr(InWeakVm, InKey))
            .OnClicked(MakeTraceClick(InWeakVm, InKey))
            .CopyText(InKey.ToString());
        Chip->SetToolTipText(FText::FromString(FString::Printf(TEXT("%s — needs %s, currently %s. Click to trace this key."),
            *InKey.ToString(),
            InWantedValue ? TEXT("TRUE") : TEXT("FALSE"),
            WsValueOf(InPlanner, InKey) ? TEXT("TRUE") : TEXT("FALSE"))));
        return Chip;
    }

    auto MakeEffectChip(
        const FGameplayTag& InKey,
        bool InValue,
        const TWeakPtr<FCkGoapDebugger_ViewModel>& InWeakVm) -> TSharedRef<SWidget>
    {
        const auto Label = InValue
            ? LeafOfTag(InKey)
            : FString::Printf(TEXT("%s = false"), *LeafOfTag(InKey));

        auto Chip = SNew(SCkDebug_Chip)
            .Text(FText::FromString(Label))
            .Kind(ECkDebug_ChipKind::Effect)
            .Highlighted(MakeTraceAttr(InWeakVm, InKey))
            .OnClicked(MakeTraceClick(InWeakVm, InKey))
            .CopyText(InKey.ToString());
        Chip->SetToolTipText(FText::FromString(FString::Printf(TEXT("%s — sets %s. Click to trace this key."),
            *InKey.ToString(), InValue ? TEXT("TRUE") : TEXT("FALSE"))));
        return Chip;
    }

    auto MakeLockIcon(const TCHAR* InWhy) -> TSharedRef<SWidget>
    {
        // "RO" text badge — the padlock emoji is outside the editor font's
        // coverage (renders as tofu).
        return SNew(STextBlock)
            .Text(FText::FromString(TEXT("RO")))
            .Font_Lambda([]() -> FSlateFontInfo
            { return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeMicro()); })
            .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
            .ToolTipText(FText::FromString(InWhy));
    }
}

// ====================================================================================================================

auto
    SCkGoapDebugger_AgentColumn::
    Construct(const FArguments& InArgs)
    -> void
{
    _ViewModel = InArgs._ViewModel;

    _NativeContent = SNew(SBorder)
        .BorderImage(CkStyle::GetFilledBrush())
        .BorderBackgroundColor(FSlateColor(CkStyle::Bg1()))
        .Padding(FMargin(CkStyle::SpaceM))
        [
            SNew(SScrollBox)
                .Orientation(Orient_Vertical)
                + SScrollBox::Slot()
                [
                    SAssignNew(_Body, SVerticalBox)
                ]
        ];

    ChildSlot
    [
        SAssignNew(_ContentHost, SBox)
        [
            _NativeContent.ToSharedRef()
        ]
    ];

    _PolicyOptions = ck_goap_debugger_agent_column::MakePolicyOptions();
    TryActivateAuthoredView();

    RefreshFromViewModel();
}

auto
    SCkGoapDebugger_AgentColumn::
    Reset_ForWorldChange()
    -> void
{
    _LastHash = 0;
    ++_AuthoredGeneration;
    ClearAuthoredNativePorts();
    _AuthoredView.Reset();
    ActivateNativeFallback();
    if (_Body.IsValid())
    {
        _Body->ClearChildren();
    }
}

auto
    SCkGoapDebugger_AgentColumn::
    RefreshFromViewModel()
    -> void
{
    if (NOT _ViewModel.IsValid() || NOT _Body.IsValid())
    {
        return;
    }

    if (_AuthoredView.IsValid())
    {
        _AuthoredView->PollFiles(ck_goap_debugger_agent_column::Tokens());
        if (!_AuthoredView->GetLastResult().Succeeded)
        {
            // The currently accepted tree stays mounted after a rejected live reload.
            _AuthoredLoadFailure = FString::Join(_AuthoredView->GetLastResult().Errors, TEXT("\n"));
        }
        else
        {
            _AuthoredLoadFailure.Reset();
        }
    }
    else
    {
        TryActivateAuthoredView();
    }

    const auto* Planner = _ViewModel->GetSelectedPlannerInfo();
    const auto* Snapshot = _ViewModel->GetCurrentEntitySnapshot();

    if (Planner == nullptr)
    {
        if (_LastHash != 1)
        {
            _LastHash = 1;
            ClearAuthoredNativePorts();
            if (!_AuthoredView.IsValid())
            {
                _Body->ClearChildren();
                _Body->AddSlot()
                    .AutoHeight()
                    .Padding(CkStyle::SpaceL)
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(TEXT("Select an agent in the Squad tab (or a planner in the tree) to inspect it.")))
                            .Font_Lambda([]() -> FSlateFontInfo
                            { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeBody()); })
                            .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                            .AutoWrapText(true)
                    ];
            }
        }
        return;
    }

    // Structural hash: selection + plan content + chain + WS values + settings.
    auto Hash = GetTypeHash(Planner->PlannerHandle);
    Hash = HashCombine(Hash, GetTypeHash(Planner->PlanAttemptCount));
    Hash = HashCombine(Hash, static_cast<uint32>(Planner->PlanStatus));
    Hash = HashCombine(Hash, GetTypeHash(Planner->PlanClassNames.Num()));
    for (const auto& Name : Planner->PlanClassNames) { Hash = HashCombine(Hash, GetTypeHash(Name)); }
    for (const auto& Entry : Planner->WorldState)
    { Hash = HashCombine(Hash, HashCombine(GetTypeHash(Entry.Key), Entry.Value ? 7u : 3u)); }
    Hash = HashCombine(Hash, Planner->EnableToggle == ECk_EnableDisable::Enable ? 11u : 13u);
    Hash = HashCombine(Hash, static_cast<uint32>(Planner->ReplanPolicy));
    Hash = HashCombine(Hash, GetTypeHash(Planner->MinReplanIntervalSeconds));
    Hash = HashCombine(Hash, GetTypeHash(Planner->SearchBudgetMicroseconds));
    Hash = HashCombine(Hash, GetTypeHash(Planner->CostThreshold));
    // Chain-crumb + plan-step names run through the shared name-depth tuner.
    Hash = HashCombine(Hash, GetTypeHash(_ViewModel->Get_NameDepth()));

    if (Hash == _LastHash)
    {
        return;
    }
    _LastHash = Hash;

    DoRebuild(*Planner, Snapshot != nullptr ? Snapshot->DebugName : FString{});
}

// ====================================================================================================================

auto
    SCkGoapDebugger_AgentColumn::
    DoRebuild(const FCkGoapDebugger_PlannerInfo& InPlanner, const FString& InAgentName)
    -> void
{
    if (_AuthoredView.IsValid())
    {
        if (_BreadcrumbPort.IsValid())
        {
            _BreadcrumbPort->SetContent(DoBuildChainCrumb(InPlanner));
        }
        if (_GoalPort.IsValid())
        {
            _GoalPort->SetContent(DoBuildGoalPanel(InPlanner));
        }
        if (_PlanPort.IsValid())
        {
            _PlanPort->SetContent(DoBuildPlanPanel(InPlanner));
        }
        if (_SearchBudgetPort.IsValid())
        {
            _SearchBudgetPort->SetContent(DoBuildSearchBudgetEditor(InPlanner));
        }
        return;
    }

    _Body->ClearChildren();

    const auto AddSection = [this](TSharedRef<SWidget> InWidget)
    {
        _Body->AddSlot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, CkStyle::SpaceM)
            [
                InWidget
            ];
    };

    AddSection(DoBuildAgentCard(InPlanner, InAgentName));
    AddSection(DoBuildChainCrumb(InPlanner));
    AddSection(DoBuildGoalPanel(InPlanner));
    AddSection(DoBuildPlanPanel(InPlanner));
    AddSection(DoBuildSettingsDrawer(InPlanner));
}

// ====================================================================================================================

auto SCkGoapDebugger_AgentColumn::ClearAuthoredNativePorts() -> void
{
    const auto Clear = [](const TSharedPtr<SBox>& InPort)
    {
        if (InPort.IsValid())
        {
            InPort->SetContent(SNullWidget::NullWidget);
        }
    };

    Clear(_BreadcrumbPort);
    Clear(_GoalPort);
    Clear(_PlanPort);
    Clear(_SearchBudgetPort);
}

auto SCkGoapDebugger_AgentColumn::ActivateNativeFallback() -> void
{
    if (_ContentHost.IsValid() && _NativeContent.IsValid())
    {
        _ContentHost->SetContent(_NativeContent.ToSharedRef());
    }
}

auto SCkGoapDebugger_AgentColumn::TryActivateAuthoredView() -> void
{
    using namespace ck_goap_debugger_agent_column;

    if (_AuthoredView.IsValid() || !_ContentHost.IsValid() || !_PolicyOptions.IsValid())
    {
        return;
    }

    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    if (!RegistryResult.Succeeded)
    {
        _AuthoredLoadFailure = FString::Join(RegistryResult.Errors, TEXT("\n"));
        ActivateNativeFallback();
        return;
    }

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (!Plugin.IsValid())
    {
        _AuthoredLoadFailure = TEXT("CkDebugger plugin is unavailable.");
        ActivateNativeFallback();
        return;
    }

    _BreadcrumbPort = SNew(SBox);
    _GoalPort = SNew(SBox);
    _PlanPort = SNew(SBox);
    _SearchBudgetPort = SNew(SBox);

    FCkUiView::FNativeBindings NativeBindings;
    NativeBindings.Add(TEXT("goap-agent-breadcrumb-body"), _BreadcrumbPort);
    NativeBindings.Add(TEXT("goap-agent-goal-body"), _GoalPort);
    NativeBindings.Add(TEXT("goap-agent-plan-body"), _PlanPort);
    NativeBindings.Add(TEXT("goap-agent-budget-body"), _SearchBudgetPort);

    const TWeakPtr<SCkGoapDebugger_AgentColumn> WeakPanel{SharedThis(this)};
    const uint64 Generation = ++_AuthoredGeneration;
    const auto GetPlanner = [WeakPanel, Generation]() -> const FCkGoapDebugger_PlannerInfo*
    {
        const TSharedPtr<SCkGoapDebugger_AgentColumn> Panel = WeakPanel.Pin();
        return Panel.IsValid() && Panel->_AuthoredGeneration == Generation && Panel->_ViewModel.IsValid()
            ? Panel->_ViewModel->GetSelectedPlannerInfo()
            : nullptr;
    };

    FCkUiView::FDataBindings Data;
    Data.SlateUserIndex = 0;
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakPanel, Generation]()
    {
        const TSharedPtr<SCkGoapDebugger_AgentColumn> Panel = WeakPanel.Pin();
        return Panel.IsValid() && Panel->_AuthoredGeneration == Generation && Panel->_AuthoredView.IsValid();
    });
    Data.Visibility.Add(TEXT("goap-agent-empty-visible"), TAttribute<bool>::CreateLambda([GetPlanner]()
    {
        return GetPlanner() == nullptr;
    }));
    Data.Visibility.Add(TEXT("goap-agent-card-visible"), TAttribute<bool>::CreateLambda([GetPlanner]()
    {
        return GetPlanner() != nullptr;
    }));
    Data.Visibility.Add(TEXT("goap-agent-enabled"), TAttribute<bool>::CreateLambda([GetPlanner]()
    {
        const FCkGoapDebugger_PlannerInfo* Planner = GetPlanner();
        return Planner != nullptr && Planner->EnableToggle == ECk_EnableDisable::Enable;
    }));
    // The underlying request is admitted but intentionally ignored by the GOAP processor. Keep the
    // authored select live as a construction-time fact, while preventing misleading runtime input.
    Data.Visibility.Add(TEXT("goap-agent-policy-read-only"), TAttribute<bool>::CreateLambda([]()
    {
        return true;
    }));
    Data.Text.Add(TEXT("goap-agent-empty"), TAttribute<FText>::CreateLambda([]()
    {
        return FText::FromString(TEXT("Select an agent in the Squad tab (or a planner in the tree) to inspect it."));
    }));
    Data.Text.Add(TEXT("goap-agent-name"), TAttribute<FText>::CreateLambda([WeakPanel, GetPlanner]()
    {
        const TSharedPtr<SCkGoapDebugger_AgentColumn> Panel = WeakPanel.Pin();
        const FCkGoapDebugger_EntitySnapshot* Snapshot = Panel.IsValid() && Panel->_ViewModel.IsValid()
            ? Panel->_ViewModel->GetCurrentEntitySnapshot()
            : nullptr;
        const FCkGoapDebugger_PlannerInfo* Planner = GetPlanner();
        return FText::FromString(Snapshot != nullptr && !Snapshot->DebugName.IsEmpty()
            ? Snapshot->DebugName
            : (Planner != nullptr ? Planner->DisplayName : FString{}));
    }));
    Data.Text.Add(TEXT("goap-agent-status"), TAttribute<FText>::CreateLambda([GetPlanner]()
    {
        const FCkGoapDebugger_PlannerInfo* Planner = GetPlanner();
        return Planner != nullptr ? StatusText(Planner->PlanStatus) : FText::GetEmpty();
    }));
    Data.Color.Add(TEXT("goap-agent-status-foreground"), TAttribute<FLinearColor>::CreateLambda([GetPlanner]()
    {
        const FCkGoapDebugger_PlannerInfo* Planner = GetPlanner();
        if (Planner == nullptr)
        {
            return CkStyle::TextMute();
        }
        switch (StatusTone(Planner->PlanStatus))
        {
            case ECk_Tone::Ok: return CkStyle::Ok();
            case ECk_Tone::Err: return CkStyle::Err();
            case ECk_Tone::Warn: return CkStyle::Warn();
            case ECk_Tone::Accent: return CkStyle::Accent();
            default: return CkStyle::TextMute();
        }
    }));
    Data.Color.Add(TEXT("goap-agent-status-background"), TAttribute<FLinearColor>::CreateLambda([GetPlanner]()
    {
        const FCkGoapDebugger_PlannerInfo* Planner = GetPlanner();
        return Planner != nullptr ? CkStyle::OverlayOf(Planner->PlanStatus == ECk_GoapPlanStatus::PlanFailed
            ? CkStyle::Err()
            : CkStyle::Accent(), 0.12f) : FLinearColor::Transparent;
    }));

    const auto AddPlannerText = [&Data, GetPlanner](const TCHAR* InName, TFunction<FString(const FCkGoapDebugger_PlannerInfo&)> InText)
    {
        Data.Text.Add(InName, TAttribute<FText>::CreateLambda([GetPlanner, Text = MoveTemp(InText)]()
        {
            const FCkGoapDebugger_PlannerInfo* Planner = GetPlanner();
            return Planner != nullptr ? FText::FromString(Text(*Planner)) : FText::GetEmpty();
        }));
    };
    AddPlannerText(TEXT("goap-agent-planner-tag"), [](const FCkGoapDebugger_PlannerInfo& Planner) { return Planner.PlannerTag.ToString(); });
    AddPlannerText(TEXT("goap-agent-world-state"), [](const FCkGoapDebugger_PlannerInfo& Planner)
    {
        return Planner.WorldStateSourceLabel.IsEmpty() ? TEXT("(inherited)") : Planner.WorldStateSourceLabel;
    });
    AddPlannerText(TEXT("goap-agent-plan-on-start"), [](const FCkGoapDebugger_PlannerInfo& Planner)
    {
        return Planner.PlanOnStart ? TEXT("true") : TEXT("false");
    });
    AddPlannerText(TEXT("goap-agent-defaults"), [](const FCkGoapDebugger_PlannerInfo& Planner)
    {
        return Planner.AllowPlanFailed ? TEXT("plan failure allowed")
            : (Planner.HasUnconditionalFallback ? TEXT("fallback present") : TEXT("fallback missing"));
    });
    AddPlannerText(TEXT("goap-agent-last-result"), [](const FCkGoapDebugger_PlannerInfo& Planner)
    {
        return StatusText(Planner.PlanStatus).ToString();
    });

    Data.String.Add(TEXT("goap-agent-policy"), TAttribute<FString>::CreateLambda([GetPlanner]()
    {
        const FCkGoapDebugger_PlannerInfo* Planner = GetPlanner();
        return Planner != nullptr ? PolicyKey(Planner->ReplanPolicy) : FString{};
    }));
    Data.Collections.Add(TEXT("goap-agent-policy-options"), _PolicyOptions);
    // FCkUiSelect requires a changed callback even when read-only. This deliberate no-op is not a
    // request path: the policy is construction-time until the GOAP processor supports runtime tuning.
    Data.StringChanged.Add(TEXT("goap-agent-policy-changed"), FCkUiOnStringChanged::CreateLambda([](const FString&)
    {
    }));
    Data.BoolChanged.Add(TEXT("goap-agent-enabled-changed"), FCkUiOnBoolChanged::CreateLambda([GetPlanner](const bool InEnabled)
    {
        const FCkGoapDebugger_PlannerInfo* Planner = GetPlanner();
        if (Planner == nullptr)
        {
            return;
        }
        auto Mutable = Planner->PlannerHandle;
        if (ck::Is_NOT_Valid(Mutable))
        {
            return;
        }
        UCk_Utils_Goap_Planner_UE::Request_SetEnableToggle(Mutable,
            InEnabled ? ECk_EnableDisable::Enable : ECk_EnableDisable::Disable, {});
    }));
    Data.Number.Add(TEXT("goap-agent-min-interval"), TAttribute<float>::CreateLambda([GetPlanner]()
    {
        const FCkGoapDebugger_PlannerInfo* Planner = GetPlanner();
        return Planner != nullptr ? Planner->MinReplanIntervalSeconds : 0.0f;
    }));
    Data.NumberCommitted.Add(TEXT("goap-agent-min-interval-committed"), FCkUiOnNumberCommitted::CreateLambda([GetPlanner](const float InValue, ETextCommit::Type)
    {
        const FCkGoapDebugger_PlannerInfo* Planner = GetPlanner();
        if (Planner == nullptr)
        {
            return;
        }
        auto Mutable = Planner->PlannerHandle;
        if (ck::Is_NOT_Valid(Mutable))
        {
            return;
        }
        UCk_Utils_Goap_Planner_UE::Request_SetReplanInterval(Mutable, FMath::Max(0.0f, InValue), {});
    }));
    Data.Number.Add(TEXT("goap-agent-threshold"), TAttribute<float>::CreateLambda([GetPlanner]()
    {
        const FCkGoapDebugger_PlannerInfo* Planner = GetPlanner();
        return Planner != nullptr ? Planner->CostThreshold : 0.0f;
    }));
    Data.NumberCommitted.Add(TEXT("goap-agent-threshold-committed"), FCkUiOnNumberCommitted::CreateLambda([GetPlanner](const float InValue, ETextCommit::Type)
    {
        const FCkGoapDebugger_PlannerInfo* Planner = GetPlanner();
        if (Planner == nullptr)
        {
            return;
        }
        auto Mutable = Planner->PlannerHandle;
        if (ck::Is_NOT_Valid(Mutable))
        {
            return;
        }
        UCk_Utils_Goap_Planner_UE::Request_SetCostThreshold(Mutable, FMath::Max(0.0f, InValue), {});
    }));
    FCkUiView::FActions Actions;
    const auto AddRequestAction = [&Actions, GetPlanner](const TCHAR* InName, TFunction<void(FCk_Handle_Goap_Planner&)> InRequest)
    {
        Actions.Add(InName, FSimpleDelegate::CreateLambda([GetPlanner, Request = MoveTemp(InRequest)]()
        {
            const FCkGoapDebugger_PlannerInfo* Planner = GetPlanner();
            if (Planner == nullptr)
            {
                return;
            }
            auto Mutable = Planner->PlannerHandle;
            if (ck::Is_NOT_Valid(Mutable))
            {
                return;
            }
            Request(Mutable);
        }));
    };
    AddRequestAction(TEXT("goap-agent-replan"), [](FCk_Handle_Goap_Planner& Mutable) { UCk_Utils_Goap_Planner_UE::Request_Plan(Mutable, {}); });
    AddRequestAction(TEXT("goap-agent-cancel"), [](FCk_Handle_Goap_Planner& Mutable) { UCk_Utils_Goap_Planner_UE::Request_CancelPlan(Mutable, {}); });
    AddRequestAction(TEXT("goap-agent-reset"), [](FCk_Handle_Goap_Planner& Mutable) { UCk_Utils_Goap_Planner_UE::Request_ResetActiveChain(Mutable, {}); });

    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(MoveTemp(NativeBindings), MoveTemp(Actions), Tokens(),
        CkStyle::RegularFont(CkStyle::FontSizeBody()), MoveTemp(Data), Registry);
    const TSharedRef<SWidget> Main = Candidate->GetRegion(TEXT("main"));
    const FString Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(FPaths::Combine(Directory, TEXT("GoapDebuggerAgentColumn.ui.html")),
        FPaths::Combine(Directory, TEXT("GoapDebuggerAgentColumn.ui.css")));
    Candidate->PollFiles();
    if (!Candidate->GetLastResult().Succeeded)
    {
        _AuthoredLoadFailure = FString::Join(Candidate->GetLastResult().Errors, TEXT("\n"));
        ActivateNativeFallback();
        return;
    }

    _AuthoredLoadFailure.Reset();
    _AuthoredView = Candidate;
    _ContentHost->SetContent(Main);
}

// ====================================================================================================================

auto
    SCkGoapDebugger_AgentColumn::
    DoBuildAgentCard(const FCkGoapDebugger_PlannerInfo& InPlanner, const FString& InAgentName)
    -> TSharedRef<SWidget>
{
    using namespace ck_goap_debugger_agent_column;

    const auto Status = InPlanner.PlanStatus;

    return SNew(SCkDebug_Card)
        [
            SNew(SHorizontalBox)

            // Avatar — two-letter initials on an accent-dim rounded box.
            + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(0.0f, 0.0f, CkStyle::SpaceL, 0.0f)
                [
                    SNew(SBox)
                        .WidthOverride(38.0f)
                        .HeightOverride(38.0f)
                        [
                            SNew(SBorder)
                                .BorderImage(CkStyle::GetRoundedBrush_Large())
                                .BorderBackgroundColor(FSlateColor(CkStyle::AccentDim()))
                                .HAlign(HAlign_Center)
                                .VAlign(VAlign_Center)
                                [
                                    SNew(STextBlock)
                                        .Text(FText::FromString(InitialsOf(InAgentName)))
                                        .Font_Lambda([]() -> FSlateFontInfo
                                        { return ck::debug_axes::ScaledFont("Mono", CkStyle::FontSizeH2()); })
                                        .ColorAndOpacity(FSlateColor(CkStyle::Accent()))
                                ]
                        ]
                ]

            + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(SVerticalBox)

                    + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(SCkDebug_SelectableLabel)
                                .Text(FText::FromString(InAgentName.IsEmpty() ? TEXT("(no agent)") : *InAgentName))
                                .Font_Lambda([]() -> FSlateFontInfo
                                { return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeH2()); })
                                .ColorAndOpacity(FSlateColor(CkStyle::Text()))
                        ]

                    + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, 3.0f, 0.0f, 0.0f)
                        [
                            SNew(SHorizontalBox)

                            + SHorizontalBox::Slot()
                                .AutoWidth()
                                .VAlign(VAlign_Center)
                                .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
                                [
                                    SNew(SCkDebug_SelectableLabel)
                                        .Text(FText::FromString(InPlanner.DisplayName))
                                        .Font_Lambda([]() -> FSlateFontInfo
                                        { return ck::debug_axes::ScaledFont("Mono", CkStyle::FontSizeSmall()); })
                                        .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
                                ]

                            + SHorizontalBox::Slot()
                                .AutoWidth()
                                .VAlign(VAlign_Center)
                                .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
                                [
                                    SNew(SCkDebug_StatusPill)
                                        .Text(StatusText(Status))
                                        .Tone(StatusTone(Status))
                                ]

                            + SHorizontalBox::Slot()
                                .AutoWidth()
                                .VAlign(VAlign_Center)
                                [
                                    SNew(STextBlock)
                                        .Text(FText::FromString(FString::Printf(TEXT("attempt #%d"), InPlanner.PlanAttemptCount)))
                                        .Font_Lambda([]() -> FSlateFontInfo
                                        { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeSmall()); })
                                        .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
                                ]
                        ]
                ]
        ];
}

// ====================================================================================================================

auto
    SCkGoapDebugger_AgentColumn::
    DoBuildChainCrumb(const FCkGoapDebugger_PlannerInfo& InPlanner)
    -> TSharedRef<SWidget>
{
    using namespace ck_goap_debugger_agent_column;

    auto Wrap = SNew(SWrapBox)
        .UseAllottedSize(true)
        .InnerSlotPadding(FVector2D(4.0f, 4.0f));

    struct FCrumb
    {
        FString Glyph;
        FString Name;
        FString FullName;
        bool    IsFallback = false;
        bool    IsLeaf = false;
    };

    const auto NameDepth = _ViewModel.IsValid() ? _ViewModel->Get_NameDepth() : 1;

    auto Crumbs = TArray<FCrumb>{};
    Crumbs.Add(FCrumb{TEXT("◆"), InPlanner.DisplayName, InPlanner.DisplayName, false, false});

    // Walk Plan[0] display names down the sub-planner chain.
    const auto* Cursor = &InPlanner;
    while (Cursor != nullptr && NOT Cursor->PlanClassNames.IsEmpty())
    {
        const auto& StepName = Cursor->PlanClassNames[0];
        const auto* StepHandle = Cursor->PlanHandles.IsValidIndex(0) ? &Cursor->PlanHandles[0] : nullptr;

        const FCkGoapDebugger_PlannerInfo* NextPlanner = nullptr;
        if (StepHandle != nullptr)
        {
            NextPlanner = Cursor->ChildPlanners.FindByPredicate(
                [StepHandle](const FCkGoapDebugger_PlannerInfo& In)
                {
                    return static_cast<FCk_Handle>(In.PlannerHandle) == static_cast<FCk_Handle>(*StepHandle);
                });
        }

        const auto IsFallbackStep = [&]() -> bool
        {
            const auto* StepInfo = Cursor->ChildActions.FindByPredicate(
                [StepHandle](const FCkGoapDebugger_ActionInfo& In)
                { return StepHandle != nullptr && In.Handle == *StepHandle; });
            return StepInfo != nullptr && StepInfo->Cost >= k_FallbackCostFloor;
        }();

        auto Crumb = FCrumb{};
        Crumb.Glyph = NextPlanner != nullptr ? TEXT("◆●") : TEXT("●");
        Crumb.Name = SCkDebug_NameLabel::Get_ShortName(StepName, NameDepth);
        Crumb.FullName = StepName;
        Crumb.IsFallback = IsFallbackStep;
        Crumb.IsLeaf = NextPlanner == nullptr;
        Crumbs.Add(MoveTemp(Crumb));

        Cursor = NextPlanner;
    }

    for (auto Index = 0; Index < Crumbs.Num(); ++Index)
    {
        const auto& Crumb = Crumbs[Index];

        if (Index > 0)
        {
            Wrap->AddSlot()
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("›")))
                        .Font_Lambda([]() -> FSlateFontInfo
                        { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeSmall()); })
                        .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
                ];
        }

        const auto BorderColor = Crumb.IsFallback
            ? CkStyle::Warn()
            : CkStyle::OverlayOf(CkStyle::Accent(), 0.55f);
        const auto FillColor = Crumb.IsFallback ? CkStyle::WarnDim() : CkStyle::AccentDim();
        const auto TextColor = Crumb.IsFallback ? CkStyle::Warn() : CkStyle::Accent();

        Wrap->AddSlot()
            .VAlign(VAlign_Center)
            [
                SNew(SCkDebug_GlowWrap)
                    .Tight(true)
                    .Extent(3.0f)
                    .GlowColor(Crumb.IsLeaf && NOT Crumb.IsFallback ? CkStyle::Accent() : FLinearColor::Transparent)
                    [
                        SNew(SBorder)
                            .BorderImage(CkStyle::GetRoundedBrush())
                            .BorderBackgroundColor(FSlateColor(BorderColor))
                            .Padding(FMargin(1.0f))
                            [
                                SNew(SBorder)
                                    .BorderImage(CkStyle::GetRoundedBrush())
                                    .BorderBackgroundColor(FSlateColor(FillColor))
                                    .Padding(FMargin(8.0f, 3.0f))
                                    [
                                        SNew(STextBlock)
                                            .Text(FText::FromString(FString::Printf(TEXT("%s %s"), *Crumb.Glyph, *Crumb.Name)))
                                            .ToolTipText(FText::FromString(Crumb.FullName))
                                            .Font_Lambda([]() -> FSlateFontInfo
                                            { return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeSmall()); })
                                            .ColorAndOpacity(FSlateColor(TextColor))
                                    ]
                            ]
                    ]
            ];
    }

    return SNew(SCkDebug_Card)
        .BodyPadding(FMargin(10.0f, 8.0f))
        [
            Wrap
        ];
}

// ====================================================================================================================

auto
    SCkGoapDebugger_AgentColumn::
    DoBuildGoalPanel(const FCkGoapDebugger_PlannerInfo& InPlanner)
    -> TSharedRef<SWidget>
{
    using namespace ck_goap_debugger_agent_column;

    auto Chips = SNew(SWrapBox)
        .UseAllottedSize(true)
        .InnerSlotPadding(FVector2D(4.0f, 4.0f));

    if (InPlanner.GoalAuthored.IsEmpty())
    {
        Chips->AddSlot()
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Empty goal — plans complete immediately.")))
                    .Font_Lambda([]() -> FSlateFontInfo
                    { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeSmall()); })
                    .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
            ];
    }

    for (const auto& Goal : InPlanner.GoalAuthored)
    {
        Chips->AddSlot()
            [
                MakeConditionChip(InPlanner, Goal.Get_Key(), Goal.Get_Value(), TWeakPtr<FCkGoapDebugger_ViewModel>(_ViewModel))
            ];
    }

    auto Body = SNew(SVerticalBox)

        + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SCkDebug_SectionHeader)
                    .Label(FText::FromString(TEXT("Goal")))
                    .SubText(FText::FromString(TEXT("this planner plans toward")))
            ]

        + SVerticalBox::Slot()
            .AutoHeight()
            [
                Chips
            ];

    // Invalid-goal diagnostics — goal keys no registered action provides.
    if (NOT InPlanner.InvalidGoalAuthored.IsEmpty())
    {
        auto InvalidChips = SNew(SWrapBox)
            .UseAllottedSize(true)
            .InnerSlotPadding(FVector2D(4.0f, 4.0f));

        for (const auto& Invalid : InPlanner.InvalidGoalAuthored)
        {
            InvalidChips->AddSlot()
                [
                    MakeConditionChip(InPlanner, Invalid.Get_Key(), Invalid.Get_Value(), TWeakPtr<FCkGoapDebugger_ViewModel>(_ViewModel))
                ];
        }

        Body->AddSlot()
            .AutoHeight()
            .Padding(0.0f, CkStyle::SpaceM, 0.0f, 0.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                    [
                        SNew(STextBlock)
                            .Text(FText::FromString(TEXT("! Unregistered goal keys:")))
                            .Font_Lambda([]() -> FSlateFontInfo
                            { return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeSmall()); })
                            .ColorAndOpacity(FSlateColor(CkStyle::Err()))
                    ]
                + SHorizontalBox::Slot().FillWidth(1.0f) [ InvalidChips ]
            ];
    }

    return SNew(SCkDebug_Card) [ Body ];
}

// ====================================================================================================================

auto
    SCkGoapDebugger_AgentColumn::
    DoBuildPlanPanel(const FCkGoapDebugger_PlannerInfo& InPlanner)
    -> TSharedRef<SWidget>
{
    using namespace ck_goap_debugger_agent_column;

    auto Steps = SNew(SVerticalBox);

    Steps->AddSlot()
        .AutoHeight()
        [
            SNew(SCkDebug_SectionHeader)
                .Label(FText::FromString(TEXT("Plan")))
                .SubText(FText::FromString(FString::Printf(TEXT("status · attempt #%d"), InPlanner.PlanAttemptCount)))
        ];

    if (InPlanner.PlanClassNames.IsEmpty())
    {
        Steps->AddSlot()
            .AutoHeight()
            [
                SNew(STextBlock)
                    .Text(FText::FromString(InPlanner.PlanStatus == ECk_GoapPlanStatus::PlanFound
                        ? TEXT("Goal already satisfied — empty plan (Plan Found immediately).")
                        : TEXT("No plan yet.")))
                    .Font_Lambda([]() -> FSlateFontInfo
                    { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeSmall()); })
                    .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
                    .AutoWrapText(true)
            ];
    }

    for (auto StepIndex = 0; StepIndex < InPlanner.PlanClassNames.Num(); ++StepIndex)
    {
        const auto& StepName = InPlanner.PlanClassNames[StepIndex];
        const auto* StepHandle = InPlanner.PlanHandles.IsValidIndex(StepIndex)
            ? &InPlanner.PlanHandles[StepIndex] : nullptr;

        const auto* StepInfo = InPlanner.ChildActions.FindByPredicate(
            [StepHandle](const FCkGoapDebugger_ActionInfo& In)
            { return StepHandle != nullptr && In.Handle == *StepHandle; });

        const auto IsNow = StepIndex == 0;
        const auto IsFallback = StepInfo != nullptr && StepInfo->Cost >= k_FallbackCostFloor;

        auto Conds = SNew(SWrapBox)
            .UseAllottedSize(true)
            .InnerSlotPadding(FVector2D(4.0f, 4.0f));

        if (StepInfo != nullptr)
        {
            for (const auto& Pre : StepInfo->Preconditions)
            { Conds->AddSlot() [ MakeConditionChip(InPlanner, Pre.Key, Pre.Value, TWeakPtr<FCkGoapDebugger_ViewModel>(_ViewModel)) ]; }
            for (const auto& Eff : StepInfo->Effects)
            { Conds->AddSlot() [ MakeEffectChip(Eff.Key, Eff.Value, TWeakPtr<FCkGoapDebugger_ViewModel>(_ViewModel)) ]; }
        }

        auto StepBody = SNew(SVerticalBox)

            + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
                        [
                            SNew(SBorder)
                                .BorderImage(CkStyle::GetRoundedBrush_Small())
                                .BorderBackgroundColor(FSlateColor(IsNow ? CkStyle::AccentDim() : CkStyle::NeutralDim()))
                                .Padding(FMargin(6.0f, 1.0f))
                                [
                                    SNew(STextBlock)
                                        .Text(FText::AsNumber(StepIndex + 1))
                                        .Font_Lambda([]() -> FSlateFontInfo
                                        { return ck::debug_axes::ScaledFont("Mono", CkStyle::FontSizeMicro()); })
                                        .ColorAndOpacity(FSlateColor(IsNow ? CkStyle::Accent() : CkStyle::TextMute()))
                                ]
                        ]

                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
                        [
                            SNew(SCkDebug_NameLabel)
                                .FullName(StepName)
                                .NameDepth_Lambda([WeakVm = TWeakPtr<FCkGoapDebugger_ViewModel>(_ViewModel)]() -> int32
                                {
                                    const auto Vm = WeakVm.Pin();
                                    return Vm.IsValid() ? Vm->Get_NameDepth() : 1;
                                })
                                .Font_Lambda([]() -> FSlateFontInfo
                                { return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeBody()); })
                                .ColorAndOpacity(FSlateColor(CkStyle::Text()))
                        ]

                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(StepInfo != nullptr
                                    ? FString::Printf(TEXT("%.1f"), StepInfo->Cost) : FString{}))
                                .Font_Lambda([]() -> FSlateFontInfo
                                { return ck::debug_axes::ScaledFont("Mono", CkStyle::FontSizeSmall()); })
                                .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                        ]

                    + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("◀ NOW")))
                                .Font_Lambda([]() -> FSlateFontInfo
                                { return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeMicro()); })
                                .ColorAndOpacity(FSlateColor(CkStyle::Accent()))
                                .Visibility(IsNow ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed)
                        ]
                ]

            + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0.0f, 5.0f, 0.0f, 0.0f)
                [
                    Conds
                ];

        if (IsFallback)
        {
            StepBody->AddSlot()
                .AutoHeight()
                .Padding(0.0f, 4.0f, 0.0f, 0.0f)
                [
                    SNew(STextBlock)
                        .Text(FText::FromString(TEXT("! Fallback — selected because nothing cheaper reaches the goal")))
                        .Font_Lambda([]() -> FSlateFontInfo
                        { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeSmall()); })
                        .ColorAndOpacity(FSlateColor(CkStyle::Warn()))
                        .AutoWrapText(true)
                ];
        }

        Steps->AddSlot()
            .AutoHeight()
            .Padding(ck_goap_debugger_axes::Live_RowDensity(
                FMargin{0.0f, 0.0f, 0.0f, CkStyle::SpaceS}))
            [
                SNew(SCkDebug_Card)
                    .StripeColor(IsNow ? CkStyle::Accent() : (IsFallback ? CkStyle::Warn() : FLinearColor::Transparent))
                    .BodyPadding(FMargin(9.0f, 7.0f))
                    [
                        StepBody
                    ]
            ];

        // Nested sub-plan expansion — the composite step's own Plan[0].
        const FCkGoapDebugger_PlannerInfo* SubPlanner = InPlanner.ChildPlanners.FindByPredicate(
            [StepHandle](const FCkGoapDebugger_PlannerInfo& In)
            {
                return StepHandle != nullptr &&
                    static_cast<FCk_Handle>(In.PlannerHandle) == static_cast<FCk_Handle>(*StepHandle);
            });

        if (SubPlanner != nullptr && NOT SubPlanner->PlanClassNames.IsEmpty())
        {
            const auto& SubStepName = SubPlanner->PlanClassNames[0];
            const auto* SubStepHandle = SubPlanner->PlanHandles.IsValidIndex(0) ? &SubPlanner->PlanHandles[0] : nullptr;
            const auto* SubStepInfo = SubPlanner->ChildActions.FindByPredicate(
                [SubStepHandle](const FCkGoapDebugger_ActionInfo& In)
                { return SubStepHandle != nullptr && In.Handle == *SubStepHandle; });

            auto SubConds = SNew(SWrapBox)
                .UseAllottedSize(true)
                .InnerSlotPadding(FVector2D(4.0f, 4.0f));

            if (SubStepInfo != nullptr)
            {
                for (const auto& Pre : SubStepInfo->Preconditions)
                { SubConds->AddSlot() [ MakeConditionChip(*SubPlanner, Pre.Key, Pre.Value, TWeakPtr<FCkGoapDebugger_ViewModel>(_ViewModel)) ]; }
                for (const auto& Eff : SubStepInfo->Effects)
                { SubConds->AddSlot() [ MakeEffectChip(Eff.Key, Eff.Value, TWeakPtr<FCkGoapDebugger_ViewModel>(_ViewModel)) ]; }
            }

            const auto SubIsFallback = SubStepInfo != nullptr && SubStepInfo->Cost >= k_FallbackCostFloor;

            Steps->AddSlot()
                .AutoHeight()
                .Padding(ck_goap_debugger_axes::Live_RowDensity(
                    FMargin{26.0f, 0.0f, 0.0f, CkStyle::SpaceS}))
                [
                    SNew(SCkDebug_Card)
                        .StripeColor(SubIsFallback ? CkStyle::Warn() : CkStyle::OverlayOf(CkStyle::Accent(), 0.45f))
                        .BodyPadding(FMargin(9.0f, 7.0f))
                        [
                            SNew(SVerticalBox)

                            + SVerticalBox::Slot()
                                .AutoHeight()
                                [
                                    SNew(SHorizontalBox)
                                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
                                        [
                                            SNew(STextBlock)
                                                .Text(FText::FromString(TEXT("»")))
                                                .Font_Lambda([]() -> FSlateFontInfo
                                                { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeBody()); })
                                                .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
                                        ]
                                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
                                        [
                                            SNew(SCkDebug_NameLabel)
                                                .FullName(SubStepName)
                                                .NameDepth_Lambda([WeakVm = TWeakPtr<FCkGoapDebugger_ViewModel>(_ViewModel)]() -> int32
                                                {
                                                    const auto Vm = WeakVm.Pin();
                                                    return Vm.IsValid() ? Vm->Get_NameDepth() : 1;
                                                })
                                                .Font_Lambda([]() -> FSlateFontInfo
                                                { return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeBody()); })
                                                .ColorAndOpacity(FSlateColor(CkStyle::Text()))
                                        ]
                                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceM, 0.0f)
                                        [
                                            SNew(STextBlock)
                                                .Text(FText::FromString(SubStepInfo != nullptr
                                                    ? FString::Printf(TEXT("%.1f"), SubStepInfo->Cost) : FString{}))
                                                .Font_Lambda([]() -> FSlateFontInfo
                                                { return ck::debug_axes::ScaledFont("Mono", CkStyle::FontSizeSmall()); })
                                                .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                                        ]
                                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                                        [
                                            SNew(STextBlock)
                                                .Text(FText::FromString(TEXT("sub-planner expansion")))
                                                .Font_Lambda([]() -> FSlateFontInfo
                                                { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeMicro()); })
                                                .ColorAndOpacity(FSlateColor(CkStyle::TextMute()))
                                        ]
                                ]

                            + SVerticalBox::Slot()
                                .AutoHeight()
                                .Padding(0.0f, 5.0f, 0.0f, 0.0f)
                                [
                                    SubConds
                                ]
                        ]
                ];
        }
    }

    // Footer stats.
    const auto SubCost = [&InPlanner]() -> float
    {
        if (InPlanner.PlanHandles.IsEmpty()) { return 0.0f; }
        const auto& First = InPlanner.PlanHandles[0];
        const auto* Sub = InPlanner.ChildPlanners.FindByPredicate(
            [&First](const FCkGoapDebugger_PlannerInfo& In)
            { return static_cast<FCk_Handle>(In.PlannerHandle) == static_cast<FCk_Handle>(First); });
        return Sub != nullptr ? Sub->PlanCost : 0.0f;
    }();

    auto Footer = SNew(SHorizontalBox)

        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, CkStyle::SpaceL, 0.0f)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(FString::Printf(TEXT("top-tier cost %.1f"), InPlanner.PlanCost)))
                    .Font_Lambda([]() -> FSlateFontInfo
                    { return ck::debug_axes::ScaledFont("Mono", CkStyle::FontSizeSmall()); })
                    .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
            ];

    if (SubCost > 0.0f)
    {
        Footer->AddSlot().AutoWidth().Padding(0.0f, 0.0f, CkStyle::SpaceL, 0.0f)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(FString::Printf(TEXT("sub expansion %.1f"), SubCost)))
                    .Font_Lambda([]() -> FSlateFontInfo
                    { return ck::debug_axes::ScaledFont("Mono", CkStyle::FontSizeSmall()); })
                    .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
            ];
    }

    Steps->AddSlot()
        .AutoHeight()
        .Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
        [
            Footer
        ];

    return SNew(SCkDebug_Card) [ Steps ];
}

// ====================================================================================================================

auto SCkGoapDebugger_AgentColumn::DoBuildSearchBudgetEditor(const FCkGoapDebugger_PlannerInfo& InPlanner)
    -> TSharedRef<SWidget>
{
    const FCk_Handle_Goap_Planner PlannerHandle = InPlanner.PlannerHandle;
    return SNew(SCkDebug_NumericEditor)
        .Value_Lambda([PlannerHandle, Snapshot = InPlanner.SearchBudgetMicroseconds]() -> double
        {
            return ck::IsValid(PlannerHandle)
                ? static_cast<double>(UCk_Utils_Goap_Planner_UE::Get_SearchBudgetMicroseconds(PlannerHandle))
                : static_cast<double>(Snapshot);
        })
        .Kind(ECkDebug_NumericKind::Integer)
        .MinValue(0.0)
        .Width(ck_goap_debugger_agent_column::SettingsNumericWidth)
        .OnValueCommitted_Lambda([PlannerHandle](const double InValue)
        {
            auto Mutable = PlannerHandle;
            if (ck::Is_NOT_Valid(Mutable)) { return; }

            UCk_Utils_Goap_Planner_UE::Request_SetSearchBudget(Mutable,
                static_cast<int64>(FMath::Max(0.0, InValue)), {});
        });
}

// ====================================================================================================================

auto
    SCkGoapDebugger_AgentColumn::
    DoBuildSettingsDrawer(const FCkGoapDebugger_PlannerInfo& InPlanner)
    -> TSharedRef<SWidget>
{
    using namespace ck_goap_debugger_agent_column;

    const auto PlannerHandle = InPlanner.PlannerHandle;

    const auto MakeRow = [](const TCHAR* InLabel, const TCHAR* InTooltip, TSharedRef<SWidget> InValue) -> TSharedRef<SWidget>
    {
        return SNew(SHorizontalBox)
            .ToolTipText(FText::FromString(InTooltip))

            + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SBox)
                        .WidthOverride(128.0f)
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(InLabel))
                                .Font_Lambda([]() -> FSlateFontInfo
                                { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeSmall()); })
                                .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                        ]
                ]

            + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    InValue
                ];
    };

    const auto MakeMonoValue = [](const FString& InText) -> TSharedRef<SWidget>
    {
        return SNew(SCkDebug_SelectableLabel)
            .Text(FText::FromString(InText))
            .Font_Lambda([]() -> FSlateFontInfo
            { return ck::debug_axes::ScaledFont("Mono", CkStyle::FontSizeSmall()); })
            .ColorAndOpacity(FSlateColor(CkStyle::Text()));
    };

    // The file-local MakeNumericEditor lambda that used to live here is gone: its
    // commit-on-enter-or-focus-loss behaviour was promoted verbatim into CkDebuggerCommon's
    // SCkDebug_NumericEditor, which the three settings numerics below now use directly. The widget adds
    // what the lambda could not: a LIVE value attribute (so a gameplay-side change to an interval shows
    // without a drawer rebuild), typed float/integer formatting, and an edit-state bracket.
    static const TArray<TSharedPtr<FString>> PolicyOptions = {
        MakeShared<FString>(TEXT("On world-state change")),
        MakeShared<FString>(TEXT("On cost change")),
        MakeShared<FString>(TEXT("On either")),
        MakeShared<FString>(TEXT("Only when asked (Explicit)")),
    };

    const auto PolicyToIndex = [](ECk_Goap_ReplanPolicy InPolicy) -> int32
    {
        switch (InPolicy)
        {
            case ECk_Goap_ReplanPolicy::OnWorldStateDirty: return 0;
            case ECk_Goap_ReplanPolicy::OnCostDirty:       return 1;
            case ECk_Goap_ReplanPolicy::OnEitherDirty:     return 2;
            default:                                       return 3;
        }
    };
    auto Rows = SNew(SVerticalBox);

    const auto AddRow = [&Rows](TSharedRef<SWidget> InRow)
    {
        Rows->AddSlot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 6.0f)
            [
                InRow
            ];
    };

    // Depth-tuned tag/name values run through the shared name label so the
    // chrome Name cycler applies here too; the »/« button reveals the full tag.
    const auto MakeNameValue = [this](const FString& InFullName) -> TSharedRef<SWidget>
    {
        return SNew(SCkDebug_NameLabel)
            .FullName(InFullName)
            .NameDepth_Lambda([WeakVm = TWeakPtr<FCkGoapDebugger_ViewModel>(_ViewModel)]() -> int32
            {
                const auto Vm = WeakVm.Pin();
                return Vm.IsValid() ? Vm->Get_NameDepth() : 1;
            })
            .Font_Lambda([]() -> FSlateFontInfo
            { return ck::debug_axes::ScaledFont("Mono", CkStyle::FontSizeSmall()); })
            .ColorAndOpacity(FSlateColor(CkStyle::Text()));
    };

    AddRow(MakeRow(TEXT("Planner tag"),
        TEXT("Identity of this planner on its owner. Set at Add/Create; read-only at runtime."),
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [ MakeNameValue(InPlanner.PlannerTag.ToString()) ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [ MakeLockIcon(TEXT("Construction-time only")) ]));

    AddRow(MakeRow(TEXT("World state"),
        TEXT("Which shared World State entity this planner reads. Sub-planners inherit their parent's unless overridden."),
        InPlanner.WorldStateSourceLabel.IsEmpty()
            ? MakeMonoValue(TEXT("(inherited)"))
            : MakeNameValue(InPlanner.WorldStateSourceLabel)));

    AddRow(MakeRow(TEXT("Enabled"),
        TEXT("Disabled planners skip planning and activation. Request_SetEnableToggle."),
        // HAlign_Left keeps the 30×17 switch its natural size — the fill-width
        // value slot would otherwise stretch it into an ellipse.
        SNew(SBox)
            .HAlign(HAlign_Left)
            [
                SNew(SCkDebug_Switch)
                    .IsOn(InPlanner.EnableToggle == ECk_EnableDisable::Enable)
                    .OnStateChanged_Lambda([PlannerHandle](bool InNew)
                    {
                        auto Mutable = PlannerHandle;
                        if (ck::Is_NOT_Valid(Mutable)) { return; }
                        UCk_Utils_Goap_Planner_UE::Request_SetEnableToggle(Mutable,
                            InNew ? ECk_EnableDisable::Enable : ECk_EnableDisable::Disable, {});
                    })
            ]));

    AddRow(MakeRow(TEXT("Replan policy"),
        TEXT("Construction-time policy. Runtime tuning is not supported by the GOAP processor."),
        SNew(SComboBox<TSharedPtr<FString>>)
            .OptionsSource(&PolicyOptions)
            .InitiallySelectedItem(PolicyOptions[PolicyToIndex(InPlanner.ReplanPolicy)])
            .IsEnabled(false)
            .OnGenerateWidget_Lambda([](TSharedPtr<FString> InOption)
            {
                return SNew(STextBlock)
                    .Text(FText::FromString(InOption.IsValid() ? *InOption : FString{}))
                    .Font_Lambda([]() -> FSlateFontInfo
                    { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeSmall()); });
            })
            [
                SNew(STextBlock)
                    .Text(FText::FromString(*PolicyOptions[PolicyToIndex(InPlanner.ReplanPolicy)]))
                    .Font_Lambda([]() -> FSlateFontInfo
                    { return ck::debug_axes::ScaledFont("Regular", CkStyle::FontSizeSmall()); })
            ]));

    AddRow(MakeRow(TEXT("Min replan interval"),
        TEXT("Seconds. Dirty events inside this window coalesce into ONE replan at window end — the anti-thrash dial. Request_SetReplanInterval."),
        SNew(SCkDebug_NumericEditor)
            .Value_Lambda([PlannerHandle, Snapshot = InPlanner.MinReplanIntervalSeconds]() -> double
            {
                return ck::IsValid(PlannerHandle)
                    ? static_cast<double>(UCk_Utils_Goap_Planner_UE::Get_MinReplanInterval(PlannerHandle))
                    : static_cast<double>(Snapshot);
            })
            .Kind(ECkDebug_NumericKind::Float)
            .MinValue(0.0)
            .Width(SettingsNumericWidth)
            .FractionalDigits(2)
            .OnValueCommitted_Lambda([PlannerHandle](double InValue)
            {
                auto Mutable = PlannerHandle;
                if (ck::Is_NOT_Valid(Mutable)) { return; }

                UCk_Utils_Goap_Planner_UE::Request_SetReplanInterval(Mutable,
                    FMath::Max(0.0f, static_cast<float>(InValue)), {});
            })));

    AddRow(MakeRow(TEXT("Search budget"),
        TEXT("Per-tick A* time slice in microseconds. 0 = finish in one tick. Request_SetSearchBudget."),
        SNew(SCkDebug_NumericEditor)
            .Value_Lambda([PlannerHandle, Snapshot = InPlanner.SearchBudgetMicroseconds]() -> double
            {
                return ck::IsValid(PlannerHandle)
                    ? static_cast<double>(UCk_Utils_Goap_Planner_UE::Get_SearchBudgetMicroseconds(PlannerHandle))
                    : static_cast<double>(Snapshot);
            })
            .Kind(ECkDebug_NumericKind::Integer)
            .MinValue(0.0)
            .Width(SettingsNumericWidth)
            .OnValueCommitted_Lambda([PlannerHandle](double InValue)
            {
                auto Mutable = PlannerHandle;
                if (ck::Is_NOT_Valid(Mutable)) { return; }

                UCk_Utils_Goap_Planner_UE::Request_SetSearchBudget(Mutable,
                    static_cast<int64>(FMath::Max(0.0, InValue)), {});
            })));

    AddRow(MakeRow(TEXT("Cost threshold"),
        TEXT("If > 0, the planner reports 'Cost Threshold Reached' instead of committing to a plan more expensive than this. 0 = off. Request_SetCostThreshold."),
        SNew(SCkDebug_NumericEditor)
            .Value_Lambda([PlannerHandle, Snapshot = InPlanner.CostThreshold]() -> double
            {
                return ck::IsValid(PlannerHandle)
                    ? static_cast<double>(UCk_Utils_Goap_Planner_UE::Get_CostThreshold(PlannerHandle))
                    : static_cast<double>(Snapshot);
            })
            .Kind(ECkDebug_NumericKind::Float)
            .MinValue(0.0)
            .Width(SettingsNumericWidth)
            .FractionalDigits(1)
            .OnValueCommitted_Lambda([PlannerHandle](double InValue)
            {
                auto Mutable = PlannerHandle;
                if (ck::Is_NOT_Valid(Mutable)) { return; }

                UCk_Utils_Goap_Planner_UE::Request_SetCostThreshold(Mutable,
                    FMath::Max(0.0f, static_cast<float>(InValue)), {});
            })));

    AddRow(MakeRow(TEXT("Plan on start"),
        TEXT("Fire one plan automatically when the planner activates. Construction-time."),
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [ MakeMonoValue(InPlanner.PlanOnStart ? TEXT("true") : TEXT("false")) ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [ MakeLockIcon(TEXT("Construction-time only")) ]));

    AddRow(MakeRow(TEXT("Allow plan failed"),
        TEXT("PlanFailed is treated as a misconfiguration: the framework insists on a no-precondition fallback action covering the goal, and ensures if a plan ever fails. Only tests set this true."),
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [ MakeMonoValue(InPlanner.AllowPlanFailed ? TEXT("true") : TEXT("false")) ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [ MakeLockIcon(TEXT("Construction-time only. Game content must never set this true.")) ]));

    AddRow(MakeRow(TEXT("Fallback guarantee"),
        TEXT("Setup-time check: some registered action has no preconditions and covers every goal condition (the always-valid-plan tenet)."),
        SNew(SBox)
            .HAlign(HAlign_Left)
            [
                SNew(SCkDebug_StatusPill)
                    .Text(FText::FromString(InPlanner.HasUnconditionalFallback ? TEXT("present") : TEXT("MISSING")))
                    .Tone(InPlanner.HasUnconditionalFallback ? ECk_Tone::Ok : ECk_Tone::Err)
            ]));

    // Runtime verbs.
    auto Buttons = SNew(SHorizontalBox);

    const auto AddButton = [&Buttons](const TCHAR* InLabel, const TCHAR* InTooltip, bool InAccent, TFunction<void(FCk_Handle_Goap_Planner&)> InAction, FCk_Handle_Goap_Planner InHandle)
    {
        Buttons->AddSlot()
            .AutoWidth()
            .Padding(0.0f, 0.0f, 6.0f, 0.0f)
            [
                SNew(SBorder)
                    .BorderImage(CkStyle::GetRoundedBrush())
                    .BorderBackgroundColor(FSlateColor(InAccent
                        ? CkStyle::OverlayOf(CkStyle::Accent(), 0.6f)
                        : CkStyle::Border()))
                    .Padding(FMargin(1.0f))
                    [
                        SNew(SButton)
                            .ButtonStyle(&FCkDebuggerCommonStyle::Get_FlatButtonStyle())
                            .ContentPadding(FMargin(10.0f, 4.0f))
                            .ToolTipText(FText::FromString(InTooltip))
                            .OnClicked_Lambda([InAction, InHandle]() -> FReply
                            {
                                auto Mutable = InHandle;
                                if (ck::IsValid(Mutable)) { InAction(Mutable); }
                                return FReply::Handled();
                            })
                            [
                                SNew(STextBlock)
                                    .Text(FText::FromString(InLabel))
                                    .Font_Lambda([]() -> FSlateFontInfo
                                    { return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeSmall()); })
                                    .ColorAndOpacity(FSlateColor(InAccent ? CkStyle::Accent() : CkStyle::Text()))
                            ]
                    ]
            ];
    };

    AddButton(TEXT("Replan now"), TEXT("Request_Plan — force an immediate replan"), true,
        [](FCk_Handle_Goap_Planner& InMutable) { UCk_Utils_Goap_Planner_UE::Request_Plan(InMutable, {}); }, PlannerHandle);
    AddButton(TEXT("Cancel"), TEXT("Request_CancelPlan — abort the in-flight search"), false,
        [](FCk_Handle_Goap_Planner& InMutable) { UCk_Utils_Goap_Planner_UE::Request_CancelPlan(InMutable, {}); }, PlannerHandle);
    AddButton(TEXT("Reset chain"), TEXT("Request_ResetActiveChain — collapse the chain; it re-extends next frame unless disabled"), false,
        [](FCk_Handle_Goap_Planner& InMutable) { UCk_Utils_Goap_Planner_UE::Request_ResetActiveChain(InMutable, {}); }, PlannerHandle);

    Rows->AddSlot()
        .AutoHeight()
        .Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
        [
            Buttons
        ];

    return SNew(SCkDebug_Card)
        .BodyPadding(FMargin(0.0f))
        [
            SNew(SExpandableArea)
                .InitiallyCollapsed(false)
                .BorderImage(CkStyle::GetFilledBrush())
                .BorderBackgroundColor(FSlateColor(FLinearColor::Transparent))
                .HeaderContent()
                [
                    SNew(SBox)
                        .Padding(FMargin(4.0f, 6.0f))
                        [
                            SNew(STextBlock)
                                .Text(FText::FromString(TEXT("PLANNER SETTINGS")))
                                .Font_Lambda([]() -> FSlateFontInfo
                                { return ck::debug_axes::ScaledFont("Bold", CkStyle::FontSizeH4()); })
                                .ColorAndOpacity(FSlateColor(CkStyle::TextDim()))
                        ]
                ]
                .BodyContent()
                [
                    SNew(SBox)
                        .Padding(FMargin(11.0f, 6.0f, 11.0f, 10.0f))
                        [
                            Rows
                        ]
                ]
        ];
}

// ====================================================================================================================
