#include "CkNavDebugger/Window/SCkNavDebuggerWindow.h"

#include "CkNavDebugger/Settings/CkNavDebugger_UserSettings.h"
#include "CkNavDebugger/ViewModel/CkNavDebugger_ViewModel.h"
#include "CkNavDebugger/Window/SCkNavDebugger_AgentListPanel.h"
#include "CkNavDebugger/Window/SCkNavDebugger_FailureLogPanel.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_InspectorPanel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_KeyValueRow.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkDebuggerCommon/Window/SCkDebugger_RefreshControls.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

const FName SCkNavDebuggerWindow::WindowId = FName(TEXT("NavDebugger"));

// ====================================================================================================================
// Helpers — TAttribute lambdas read from the live view-model snapshot
// ====================================================================================================================

namespace
{
    auto MakeNavmeshTextAttr(
        TWeakPtr<FCkNavDebugger_ViewModel> InVMWeak,
        TFunction<FText(const FCkNavDebugger_NavmeshInfo&)> InProjector) -> TAttribute<FText>
    {
        return TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateLambda(
            [InVMWeak, InProjector = MoveTemp(InProjector)]()
            {
                auto VM = InVMWeak.Pin();
                if (NOT VM.IsValid()) { return FText::GetEmpty(); }
                return InProjector(VM->Get_NavmeshInfo());
            }));
    }

    auto MakeNavmeshColorAttr(
        TWeakPtr<FCkNavDebugger_ViewModel> InVMWeak,
        TFunction<FLinearColor(const FCkNavDebugger_NavmeshInfo&)> InProjector) -> TAttribute<FLinearColor>
    {
        return TAttribute<FLinearColor>::Create(TAttribute<FLinearColor>::FGetter::CreateLambda(
            [InVMWeak, InProjector = MoveTemp(InProjector)]()
            {
                auto VM = InVMWeak.Pin();
                if (NOT VM.IsValid()) { return CkDebugStyle::TextDim(); }
                return InProjector(VM->Get_NavmeshInfo());
            }));
    }

    auto MakeAgentTextAttr(
        TWeakPtr<FCkNavDebugger_ViewModel> InVMWeak,
        TFunction<FText(const FCkNavDebugger_AgentInfo&)> InProjector,
        const FText& InEmpty = FText::FromString(TEXT("(none)"))) -> TAttribute<FText>
    {
        return TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateLambda(
            [InVMWeak, InProjector = MoveTemp(InProjector), InEmpty]()
            {
                auto VM = InVMWeak.Pin();
                if (NOT VM.IsValid()) { return InEmpty; }
                const auto* Info = VM->Get_SelectedAgentInfo();
                if (Info == nullptr) { return InEmpty; }
                return InProjector(*Info);
            }));
    }

    auto MakeAgentColorAttr(
        TWeakPtr<FCkNavDebugger_ViewModel> InVMWeak,
        TFunction<FLinearColor(const FCkNavDebugger_AgentInfo&)> InProjector) -> TAttribute<FLinearColor>
    {
        return TAttribute<FLinearColor>::Create(TAttribute<FLinearColor>::FGetter::CreateLambda(
            [InVMWeak, InProjector = MoveTemp(InProjector)]()
            {
                auto VM = InVMWeak.Pin();
                if (NOT VM.IsValid()) { return CkDebugStyle::TextDim(); }
                const auto* Info = VM->Get_SelectedAgentInfo();
                if (Info == nullptr) { return CkDebugStyle::TextDim(); }
                return InProjector(*Info);
            }));
    }

    auto OkColor(bool InOk) -> FLinearColor
    {
        return InOk ? CkDebugStyle::Ok() : CkDebugStyle::Err();
    }

    auto BoolText(bool InValue) -> FText
    {
        return FText::FromString(InValue ? TEXT("Yes") : TEXT("No"));
    }

    auto AgentStatusToTone(ECk_Nav_PathStatus InStatus) -> ECkDebug_Tone
    {
        switch (InStatus)
        {
            case ECk_Nav_PathStatus::Ready:   return ECkDebug_Tone::Ok;
            case ECk_Nav_PathStatus::Partial: return ECkDebug_Tone::Warn;
            case ECk_Nav_PathStatus::Failed:  return ECkDebug_Tone::Err;
            case ECk_Nav_PathStatus::Pending: return ECkDebug_Tone::Info;
            default:                          return ECkDebug_Tone::Neutral;
        }
    }

    // ----------------------------------------------------------------------------------------------------------------
    // Compact toolbar widgets for the overlay settings — combo + checkbox.
    // All edits route through UCk_Utils_NavDebugger_Settings_UE so the CVar +
    // persisted config stay in sync regardless of where the change came from.
    // ----------------------------------------------------------------------------------------------------------------

    // Static lifetime — SComboBox needs the source items to outlive it.
    static const TArray<TSharedPtr<ECk_NavDebugger_DrawMode>>& Get_DrawModeItems()
    {
        static const TArray<TSharedPtr<ECk_NavDebugger_DrawMode>> Items =
        {
            MakeShared<ECk_NavDebugger_DrawMode>(ECk_NavDebugger_DrawMode::None),
            MakeShared<ECk_NavDebugger_DrawMode>(ECk_NavDebugger_DrawMode::Selected),
            MakeShared<ECk_NavDebugger_DrawMode>(ECk_NavDebugger_DrawMode::All),
        };
        return Items;
    }

    auto Get_DrawModeLabel(ECk_NavDebugger_DrawMode InMode) -> FText
    {
        switch (InMode)
        {
            case ECk_NavDebugger_DrawMode::None:     return FText::FromString(TEXT("None"));
            case ECk_NavDebugger_DrawMode::Selected: return FText::FromString(TEXT("Selected"));
            case ECk_NavDebugger_DrawMode::All:      return FText::FromString(TEXT("All"));
            default:                                  return FText::FromString(TEXT("?"));
        }
    }

    auto Make_DrawModeCombo(
        TFunction<ECk_NavDebugger_DrawMode()> InGetter,
        TFunction<void(ECk_NavDebugger_DrawMode)> InSetter)
        -> TSharedRef<SWidget>
    {
        const auto& Items = Get_DrawModeItems();
        TSharedPtr<ECk_NavDebugger_DrawMode> Initial = Items[0];
        const auto Current = InGetter();
        for (const auto& Item : Items)
        {
            if (*Item == Current) { Initial = Item; break; }
        }

        return SNew(SComboBox<TSharedPtr<ECk_NavDebugger_DrawMode>>)
            .OptionsSource(&Items)
            .InitiallySelectedItem(Initial)
            .OnGenerateWidget_Lambda([](TSharedPtr<ECk_NavDebugger_DrawMode> InItem)
            {
                return SNew(STextBlock).Text(Get_DrawModeLabel(*InItem));
            })
            .OnSelectionChanged_Lambda([InSetter = MoveTemp(InSetter)]
                (TSharedPtr<ECk_NavDebugger_DrawMode> InItem, ESelectInfo::Type)
            {
                if (InItem.IsValid()) { InSetter(*InItem); }
            })
            [
                SNew(STextBlock)
                    .Text_Lambda([InGetter = MoveTemp(InGetter)]() { return Get_DrawModeLabel(InGetter()); })
            ];
    }

    auto Make_ToolbarCheckbox(
        const FText& InLabel,
        TFunction<bool()> InGetter,
        TFunction<void(bool)> InSetter)
        -> TSharedRef<SWidget>
    {
        return SNew(SCheckBox)
            .IsChecked_Lambda([InGetter = MoveTemp(InGetter)]()
            {
                return InGetter() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
            })
            .OnCheckStateChanged_Lambda([InSetter = MoveTemp(InSetter)](ECheckBoxState InState)
            {
                InSetter(InState == ECheckBoxState::Checked);
            })
            [
                SNew(STextBlock)
                    .Text(InLabel)
                    .ColorAndOpacity(CkDebugStyle::Text())
                    .Margin(FMargin(CkDebugStyle::SpaceS, 0.0f, 0.0f, 0.0f))
            ];
    }
}

// ====================================================================================================================
// Construction
// ====================================================================================================================

auto
    SCkNavDebuggerWindow::
    Construct(
        const FArguments& InArgs)
    -> void
{
    Register_WithGate();

    _ViewModel = MakeShared<FCkNavDebugger_ViewModel>();

    SAssignNew(_HealthCheckBody, SVerticalBox);

    ChildSlot
    [
        SNew(SVerticalBox)

            + SVerticalBox::Slot().AutoHeight().Padding(CkDebugStyle::SpaceM)
                [
                    BuildToolbar()
                ]

            + SVerticalBox::Slot().FillHeight(1.0f)
                [
                    SNew(SSplitter).Orientation(Orient_Vertical)

                        // Top half: agent list (left) | navmesh + health (right)
                        + SSplitter::Slot().Value(0.5f)
                            [
                                SNew(SSplitter).Orientation(Orient_Horizontal)
                                    + SSplitter::Slot().Value(0.45f)
                                        [
                                            SNew(SCkDebug_InspectorPanel)
                                                .Title(FText::FromString(TEXT("Nav Agents")))
                                                .Body()
                                                [
                                                    SAssignNew(_AgentListPanel, SCkNavDebugger_AgentListPanel,
                                                        _ViewModel.ToSharedRef())
                                                ]
                                        ]
                                    + SSplitter::Slot().Value(0.55f)
                                        [
                                            SNew(SSplitter).Orientation(Orient_Vertical)
                                                + SSplitter::Slot().Value(0.55f)
                                                    [
                                                        SNew(SCkDebug_InspectorPanel)
                                                            .Title(FText::FromString(TEXT("Navmesh Status")))
                                                            .Body()
                                                            [
                                                                BuildNavmeshStatusBody()
                                                            ]
                                                    ]
                                                + SSplitter::Slot().Value(0.45f)
                                                    [
                                                        SNew(SCkDebug_InspectorPanel)
                                                            .Title(FText::FromString(TEXT("Health Check")))
                                                            .Body()
                                                            [
                                                                BuildHealthCheckBody()
                                                            ]
                                                    ]
                                        ]
                            ]

                        // Selected agent inspector (full width)
                        + SSplitter::Slot().Value(0.25f)
                            [
                                SNew(SCkDebug_InspectorPanel)
                                    .Title(FText::FromString(TEXT("Selected Agent")))
                                    .Body()
                                    [
                                        BuildAgentInspectorBody()
                                    ]
                            ]

                        // Failure log
                        + SSplitter::Slot().Value(0.25f)
                            [
                                SNew(SCkDebug_InspectorPanel)
                                    .Title(FText::FromString(TEXT("Failure Log")))
                                    .Body()
                                    [
                                        SAssignNew(_FailureLogPanel, SCkNavDebugger_FailureLogPanel,
                                            _ViewModel.ToSharedRef())
                                    ]
                            ]
                ]
    ];
}

// ====================================================================================================================
// Tick — find PIE world, drive ViewModel; structural panels self-update via TAttribute
// ====================================================================================================================

auto
    SCkNavDebuggerWindow::
    Tick(
        const FGeometry& InAllottedGeometry,
        double InCurrentTime,
        float InDeltaTime)
    -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(WindowId))
    { return; }

    {
        auto FoundWorld = static_cast<UWorld*>(nullptr);
        for (auto It = GEngine->GetWorldContexts().CreateConstIterator(); It; ++It)
        {
            if (It->WorldType == EWorldType::PIE && IsValid(It->World()))
            { FoundWorld = It->World(); break; }
        }
        _CachedWorld = FoundWorld;
    }

    if (NOT _CachedWorld) { return; }

    _ViewModel->Tick(_CachedWorld, InDeltaTime);

    if (_AgentListPanel.IsValid())  { _AgentListPanel->Refresh(); }
    if (_FailureLogPanel.IsValid()) { _FailureLogPanel->Refresh(); }
    // Navmesh / Inspector / HealthCheck sections update via TAttribute bindings — no rebuild here.
}

// ====================================================================================================================
// Toolbar
// ====================================================================================================================

auto
    SCkNavDebuggerWindow::
    BuildToolbar()
    -> TSharedRef<SWidget>
{
    return SNew(SHorizontalBox)

        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkDebugStyle::SpaceS, 0.0f)
            [
                SNew(SButton)
                    .Text(FText::FromString(TEXT("Run Health Check")))
                    .ToolTipText(FText::FromString(TEXT("Run the system-level diagnostic battery (synthetic FindPath, NavData/filter checks, ...).")))
                    .OnClicked_Lambda([this]() -> FReply
                    {
                        if (_ViewModel.IsValid())
                        {
                            _ViewModel->Run_HealthCheck();
                            Rebuild_HealthCheck();
                        }
                        return FReply::Handled();
                    })
            ]

        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkDebugStyle::SpaceS, 0.0f)
            [
                SNew(SButton)
                    .Text_Lambda([this]() -> FText
                    {
                        const auto IsPaused = _ViewModel.IsValid() && _ViewModel->Get_Paused();
                        return FText::FromString(IsPaused ? TEXT("Resume") : TEXT("Pause"));
                    })
                    .OnClicked_Lambda([this]() -> FReply
                    {
                        if (_ViewModel.IsValid())
                        {
                            _ViewModel->Set_Paused(NOT _ViewModel->Get_Paused());
                        }
                        return FReply::Handled();
                    })
            ]

        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkDebugStyle::SpaceS, 0.0f)
            [
                SNew(SButton)
                    .Text(FText::FromString(TEXT("Clear Failure Log")))
                    .OnClicked_Lambda([this]() -> FReply
                    {
                        if (_ViewModel.IsValid())
                        {
                            _ViewModel->Clear_FailureLog();
                        }
                        return FReply::Handled();
                    })
            ]

        // ---- Compact overlay controls ----
        // Sit inline in the toolbar so they're one click away without taking
        // a panel slot. Same UCk_Utils_NavDebugger_Settings_UE setters as
        // before — CVar + persisted config stay in sync regardless of route.

        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(CkDebugStyle::SpaceL, 0.0f, CkDebugStyle::SpaceXS, 0.0f)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Capsules:")))
                    .ColorAndOpacity(CkDebugStyle::TextDim())
            ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkDebugStyle::SpaceM, 0.0f)
            [
                SNew(SBox).MinDesiredWidth(96.0f)
                [
                    Make_DrawModeCombo(
                        []() { return UCk_Utils_NavDebugger_Settings_UE::Get_AgentCapsuleMode(); },
                        [](ECk_NavDebugger_DrawMode InMode) { UCk_Utils_NavDebugger_Settings_UE::Set_AgentCapsuleMode(InMode); })
                ]
            ]

        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkDebugStyle::SpaceXS, 0.0f)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Paths:")))
                    .ColorAndOpacity(CkDebugStyle::TextDim())
            ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkDebugStyle::SpaceM, 0.0f)
            [
                SNew(SBox).MinDesiredWidth(96.0f)
                [
                    Make_DrawModeCombo(
                        []() { return UCk_Utils_NavDebugger_Settings_UE::Get_AgentPathMode(); },
                        [](ECk_NavDebugger_DrawMode InMode) { UCk_Utils_NavDebugger_Settings_UE::Set_AgentPathMode(InMode); })
                ]
            ]

        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkDebugStyle::SpaceM, 0.0f)
            [
                Make_ToolbarCheckbox(
                    FText::FromString(TEXT("Projections")),
                    []() { return UCk_Utils_NavDebugger_Settings_UE::Get_DrawProjections(); },
                    [](bool InValue) { UCk_Utils_NavDebugger_Settings_UE::Set_DrawProjections(InValue); })
            ]

        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, CkDebugStyle::SpaceM, 0.0f)
            [
                Make_ToolbarCheckbox(
                    FText::FromString(TEXT("Bounds")),
                    []() { return UCk_Utils_NavDebugger_Settings_UE::Get_DrawNavmeshBounds(); },
                    [](bool InValue) { UCk_Utils_NavDebugger_Settings_UE::Set_DrawNavmeshBounds(InValue); })
            ]

        + SHorizontalBox::Slot().FillWidth(1.0f)

        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(CkDebugStyle::SpaceL, 0.0f, 0.0f, 0.0f)
            [
                SNew(SCkDebugger_RefreshControls).WindowId(SCkNavDebuggerWindow::WindowId)
            ];
}

// ====================================================================================================================
// Navmesh status — built once, values pull via TAttribute
// ====================================================================================================================

auto
    SCkNavDebuggerWindow::
    BuildNavmeshStatusBody()
    -> TSharedRef<SWidget>
{
    auto VMWeak = TWeakPtr<FCkNavDebugger_ViewModel>(_ViewModel);

    auto Body = SNew(SVerticalBox);

    const auto AddRow = [&](const FText& InKey, TAttribute<FText> InValue, TAttribute<FLinearColor> InColor)
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, CkDebugStyle::SpaceXS)
        [
            SNew(SCkDebug_KeyValueRow)
                .KeyText(InKey)
                .ValueText(InValue)
                .Tone(ECkDebug_KeyValueTone::Custom)
                .CustomValueColor(InColor)
        ];
    };

    AddRow(FText::FromString(TEXT("World")),
        MakeNavmeshTextAttr(VMWeak, [](const auto& I) { return BoolText(I.HasWorld); }),
        MakeNavmeshColorAttr(VMWeak, [](const auto& I) { return OkColor(I.HasWorld); }));

    AddRow(FText::FromString(TEXT("Nav System")),
        MakeNavmeshTextAttr(VMWeak, [](const auto& I) { return BoolText(I.HasNavSystem); }),
        MakeNavmeshColorAttr(VMWeak, [](const auto& I) { return OkColor(I.HasNavSystem); }));

    AddRow(FText::FromString(TEXT("Default NavData")),
        MakeNavmeshTextAttr(VMWeak, [](const auto& I)
        {
            return I.HasNavData
                ? FText::FromString(FString::Printf(TEXT("Yes — %s"), *I.NavDataName))
                : FText::FromString(TEXT("No"));
        }),
        MakeNavmeshColorAttr(VMWeak, [](const auto& I) { return OkColor(I.HasNavData); }));

    AddRow(FText::FromString(TEXT("Default Query Filter")),
        MakeNavmeshTextAttr(VMWeak, [](const auto& I) { return BoolText(I.DefaultFilterValid); }),
        MakeNavmeshColorAttr(VMWeak, [](const auto& I) { return OkColor(I.DefaultFilterValid); }));

    AddRow(FText::FromString(TEXT("Bounds")),
        MakeNavmeshTextAttr(VMWeak, [](const auto& I)
        {
            if (NOT I.BoundsValid)
            { return FText::FromString(TEXT("Invalid (zero)")); }
            return FText::FromString(FString::Printf(TEXT("Min %s, Max %s"),
                *I.NavMeshBounds.Min.ToString(), *I.NavMeshBounds.Max.ToString()));
        }),
        MakeNavmeshColorAttr(VMWeak, [](const auto& I) { return OkColor(I.BoundsHasFootprint); }));

    AddRow(FText::FromString(TEXT("NavDataSet count")),
        MakeNavmeshTextAttr(VMWeak, [](const auto& I)
        { return FText::FromString(FString::FromInt(I.NavDataSetCount)); }),
        MakeNavmeshColorAttr(VMWeak, [](const auto& I)
        { return I.NavDataSetCount > 1 ? CkDebugStyle::Warn() : CkDebugStyle::Text(); }));

    AddRow(FText::FromString(TEXT("Agent (config)")),
        MakeNavmeshTextAttr(VMWeak, [](const auto& I)
        {
            if (NOT I.HasNavData) { return FText::FromString(TEXT("(no nav data)")); }
            return FText::FromString(FString::Printf(TEXT("%s — r=%.1f h=%.1f step=%.1f"),
                *I.AgentName.ToString(), I.AgentRadius, I.AgentHeight, I.AgentStepHeight));
        }),
        MakeNavmeshColorAttr(VMWeak, [](const auto&) { return CkDebugStyle::Text(); }));

    AddRow(FText::FromString(TEXT("MaxPathQueriesPerFrame")),
        MakeNavmeshTextAttr(VMWeak, [](const auto& I)
        { return FText::FromString(FString::FromInt(I.MaxPathQueriesPerFrame)); }),
        MakeNavmeshColorAttr(VMWeak, [](const auto& I) { return OkColor(I.MaxPathQueriesPerFrame > 0); }));

    AddRow(FText::FromString(TEXT("bAutoCreateNavigationData")),
        MakeNavmeshTextAttr(VMWeak, [](const auto& I) { return BoolText(I.AutoCreateNavigationData); }),
        MakeNavmeshColorAttr(VMWeak, [](const auto& I)
        { return I.AutoCreateNavigationData ? CkDebugStyle::Text() : CkDebugStyle::Warn(); }));

    AddRow(FText::FromString(TEXT("SupportedAgents")),
        MakeNavmeshTextAttr(VMWeak, [](const auto& I)
        { return FText::FromString(FString::FromInt(I.RegisteredAgentCount)); }),
        MakeNavmeshColorAttr(VMWeak, [](const auto& I) { return OkColor(I.RegisteredAgentCount > 0); }));

    return SNew(SBox).Padding(CkDebugStyle::SpaceM, CkDebugStyle::SpaceS)
    [
        Body
    ];
}

// ====================================================================================================================
// Agent inspector — built once, values pull via TAttribute
// ====================================================================================================================

auto
    SCkNavDebuggerWindow::
    BuildAgentInspectorBody()
    -> TSharedRef<SWidget>
{
    auto VMWeak = TWeakPtr<FCkNavDebugger_ViewModel>(_ViewModel);

    auto Body = SNew(SVerticalBox);

    const auto AddRow = [&](const FText& InKey, TAttribute<FText> InValue, TAttribute<FLinearColor> InColor)
    {
        Body->AddSlot().AutoHeight().Padding(0.0f, CkDebugStyle::SpaceXS)
        [
            SNew(SCkDebug_KeyValueRow)
                .KeyText(InKey)
                .ValueText(InValue)
                .Tone(ECkDebug_KeyValueTone::Custom)
                .CustomValueColor(InColor)
        ];
    };

    const auto NoSelection = FText::FromString(TEXT("(no agent selected)"));

    AddRow(FText::FromString(TEXT("Name")),
        MakeAgentTextAttr(VMWeak, [](const auto& I) { return FText::FromString(I.DebugName); }, NoSelection),
        MakeAgentColorAttr(VMWeak, [](const auto&) { return CkDebugStyle::Text(); }));

    AddRow(FText::FromString(TEXT("Status")),
        MakeAgentTextAttr(VMWeak, [](const auto& I)
        { return FText::FromString(CkNavDebugger::GetStatusString(I.PathStatus)); }, NoSelection),
        MakeAgentColorAttr(VMWeak, [](const auto& I)
        {
            return CkDebugStyle::GetToneColor(AgentStatusToTone(I.PathStatus));
        }));

    AddRow(FText::FromString(TEXT("Fail Reason")),
        MakeAgentTextAttr(VMWeak, [](const auto& I)
        {
            const auto Reason = I.Diagnostics.Get_LastFailReason();
            if (Reason == ECk_Nav_PathFailReason::None)
            { return FText::FromString(TEXT("—")); }
            return FText::FromString(CkNavDebugger::GetFailReasonString(Reason));
        }),
        MakeAgentColorAttr(VMWeak, [](const auto& I)
        {
            return I.Diagnostics.Get_LastFailReason() == ECk_Nav_PathFailReason::None
                ? CkDebugStyle::TextDim() : CkDebugStyle::Err();
        }));

    AddRow(FText::FromString(TEXT("Hint")),
        MakeAgentTextAttr(VMWeak, [](const auto& I)
        {
            const auto Hint = CkNavDebugger::GetFailReasonHint(I.Diagnostics.Get_LastFailReason());
            return Hint.IsEmpty() ? FText::FromString(TEXT("—")) : FText::FromString(Hint);
        }),
        MakeAgentColorAttr(VMWeak, [](const auto&) { return CkDebugStyle::TextDim(); }));

    AddRow(FText::FromString(TEXT("Agent Location")),
        MakeAgentTextAttr(VMWeak, [](const auto& I) { return FText::FromString(I.AgentLocation.ToString()); }),
        MakeAgentColorAttr(VMWeak, [](const auto&) { return CkDebugStyle::Text(); }));

    AddRow(FText::FromString(TEXT("Last Target")),
        MakeAgentTextAttr(VMWeak, [](const auto& I)
        { return FText::FromString(I.Diagnostics.Get_LastTargetLocation().ToString()); }),
        MakeAgentColorAttr(VMWeak, [](const auto&) { return CkDebugStyle::Text(); }));

    AddRow(FText::FromString(TEXT("Start Projected")),
        MakeAgentTextAttr(VMWeak, [](const auto& I)
        {
            return FText::FromString(FString::Printf(TEXT("%s @ %s"),
                I.Diagnostics.Get_StartProjected() ? TEXT("Yes") : TEXT("No"),
                *I.Diagnostics.Get_LastProjectedStart().ToString()));
        }),
        MakeAgentColorAttr(VMWeak, [](const auto& I)
        { return OkColor(I.Diagnostics.Get_StartProjected()); }));

    AddRow(FText::FromString(TEXT("End Projected")),
        MakeAgentTextAttr(VMWeak, [](const auto& I)
        {
            return FText::FromString(FString::Printf(TEXT("%s @ %s"),
                I.Diagnostics.Get_EndProjected() ? TEXT("Yes") : TEXT("No"),
                *I.Diagnostics.Get_LastProjectedEnd().ToString()));
        }),
        MakeAgentColorAttr(VMWeak, [](const auto& I)
        { return OkColor(I.Diagnostics.Get_EndProjected()); }));

    AddRow(FText::FromString(TEXT("Waypoints")),
        MakeAgentTextAttr(VMWeak, [](const auto& I)
        {
            return FText::FromString(FString::Printf(TEXT("%d  (raw %d)"),
                I.Diagnostics.Get_ExtractedWaypointCount(), I.Diagnostics.Get_RawPathPointCount()));
        }),
        MakeAgentColorAttr(VMWeak, [](const auto&) { return CkDebugStyle::Text(); }));

    AddRow(FText::FromString(TEXT("Last Query")),
        MakeAgentTextAttr(VMWeak, [](const auto& I)
        {
            return FText::FromString(FString::Printf(TEXT("%.2f ms"),
                I.Diagnostics.Get_LastQueryDurationMs()));
        }),
        MakeAgentColorAttr(VMWeak, [](const auto&) { return CkDebugStyle::Value_Numeric(); }));

    AddRow(FText::FromString(TEXT("Queued Requests")),
        MakeAgentTextAttr(VMWeak, [](const auto& I)
        { return FText::FromString(FString::FromInt(I.QueuedRequestCount)); }),
        MakeAgentColorAttr(VMWeak, [](const auto&) { return CkDebugStyle::Value_Numeric(); }));

    AddRow(FText::FromString(TEXT("Tags")),
        MakeAgentTextAttr(VMWeak, [](const auto& I)
        {
            return FText::FromString(FString::Printf(TEXT("Pending=%s  Ready=%s  Failed=%s  Crowd=%s"),
                I.HasPathPendingTag    ? TEXT("y") : TEXT("n"),
                I.HasPathReadyTag      ? TEXT("y") : TEXT("n"),
                I.HasPathFailedTag     ? TEXT("y") : TEXT("n"),
                I.HasCrowdRegisteredTag ? TEXT("y") : TEXT("n")));
        }),
        MakeAgentColorAttr(VMWeak, [](const auto&) { return CkDebugStyle::TextDim(); }));

    AddRow(FText::FromString(TEXT("Agent Params")),
        MakeAgentTextAttr(VMWeak, [](const auto& I)
        {
            return FText::FromString(FString::Printf(TEXT("r=%.1f h=%.1f speed=%.0f accel=%.0f sep=%.2f"),
                I.AgentParams.Get_Radius(), I.AgentParams.Get_Height(),
                I.AgentParams.Get_MaxSpeed(), I.AgentParams.Get_MaxAcceleration(),
                I.AgentParams.Get_SeparationWeight()));
        }),
        MakeAgentColorAttr(VMWeak, [](const auto&) { return CkDebugStyle::TextDim(); }));

    return SNew(SBox).Padding(CkDebugStyle::SpaceM, CkDebugStyle::SpaceS)
    [
        SNew(SScrollBox)
            + SScrollBox::Slot()
                [
                    Body
                ]
    ];
}

// ====================================================================================================================
// Health-check (rebuild on demand only)
// ====================================================================================================================

auto
    SCkNavDebuggerWindow::
    BuildHealthCheckBody()
    -> TSharedRef<SWidget>
{
    return SNew(SBox).Padding(CkDebugStyle::SpaceM, CkDebugStyle::SpaceS)
    [
        SNew(SScrollBox)
            + SScrollBox::Slot()
                [
                    _HealthCheckBody.ToSharedRef()
                ]
    ];
}

auto
    SCkNavDebuggerWindow::
    Rebuild_HealthCheck()
    -> void
{
    if (NOT _ViewModel.IsValid() || NOT _HealthCheckBody.IsValid()) { return; }
    _HealthCheckBody->ClearChildren();

    if (NOT _ViewModel->Has_HealthCheckReport())
    {
        _HealthCheckBody->AddSlot().AutoHeight()
        [
            SNew(SCkDebug_SelectableLabel)
                .Text(FText::FromString(TEXT("Click \"Run Health Check\" to diagnose nav-stack setup.")))
                .ColorAndOpacity(CkDebugStyle::TextDim())
        ];
        return;
    }

    const auto& Report = _ViewModel->Get_HealthCheckReport();

    for (const auto& Item : Report.Items)
    {
        const auto Tone = Item.Status == ECkNavDebugger_HealthStatus::Pass ? ECkDebug_Tone::Ok
                        : Item.Status == ECkNavDebugger_HealthStatus::Warn ? ECkDebug_Tone::Warn
                        :                                                    ECkDebug_Tone::Err;
        const auto PillText = Item.Status == ECkNavDebugger_HealthStatus::Pass ? TEXT("OK")
                            : Item.Status == ECkNavDebugger_HealthStatus::Warn ? TEXT("WARN")
                            :                                                    TEXT("FAIL");

        _HealthCheckBody->AddSlot().AutoHeight().Padding(0.0f, CkDebugStyle::SpaceXS)
        [
            SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
                    [
                        SNew(SBox).MinDesiredWidth(64.0f)
                        [
                            SNew(SCkDebug_StatusPill)
                                .Text(FText::FromString(PillText))
                                .Tone(Tone)
                        ]
                    ]
                + SHorizontalBox::Slot().FillWidth(1.0f).Padding(CkDebugStyle::SpaceS, 0.0f, 0.0f, 0.0f)
                    [
                        SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight()
                                [
                                    SNew(SCkDebug_SelectableLabel)
                                        .Text(FText::FromString(Item.Name))
                                        .ColorAndOpacity(CkDebugStyle::Text())
                                ]
                            + SVerticalBox::Slot().AutoHeight()
                                [
                                    SNew(STextBlock)
                                        .Text(FText::FromString(Item.Detail))
                                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", CkDebugStyle::FontSizeSmall()))
                                        .ColorAndOpacity(CkDebugStyle::TextDim())
                                        .AutoWrapText(true)
                                ]
                    ]
        ];
    }
}

// --------------------------------------------------------------------------------------------------------------------
