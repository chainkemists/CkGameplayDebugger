#include "CkAStarDebugger/Window/SCkAStarDebuggerWindow.h"
#include "CkAStarDebugger/ViewModel/CkAStarDebugger_ViewModel.h"

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkAStarDebugger/GridView/SCkAStarDebugger_GridView.h"
#include "CkAStarDebugger/Window/SCkAStarDebugger_StatsPanel.h"
#include "CkAStarDebugger/Window/SCkAStarDebugger_SearchHistory.h"
#include "CkAStarDebugger/CkAStarDebuggerStyle.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_IconToggle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_WorldSelector.h"
#include "CkDebuggerCommon/Lifecycle/CkDebug_SessionLifecycle.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkDebuggerCommon/Navigation/CkDebug_SelectionSync.h"
#include "CkDebuggerCommon/Window/SCkDebugger_RefreshControls.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"

#include "Engine/Engine.h"
#include "Engine/World.h"

// ====================================================================================================================
// Construction
// ====================================================================================================================

const FName SCkAStarDebuggerWindow::WindowId = FName(TEXT("AStarDebugger"));

auto
    SCkAStarDebuggerWindow::
    Construct(
        const FArguments& InArgs)
    -> void
{
    Register_WithGate();

    _ViewModel = MakeShared<FCkAStarDebugger_ViewModel>();
    _WorldModel = MakeShared<FCkDebuggerModel_WorldSelector>();
    _WorldChangedHandle = _WorldModel->OnWorldChanged.AddSP(
        this, &SCkAStarDebuggerWindow::HandleWorldChanged);
    _SessionInvalidatedHandle = ck::DebugSessionLifecycle::Get_OnSessionInvalidated().AddSP(
        this, &SCkAStarDebuggerWindow::HandleSessionInvalidated);

    // Layout:
    //   Toolbar (auto-height)
    //   ├─ GridView (left, ~70%)
    //   └─ Right panel (~30%)
    //      ├─ StatsPanel (top, ~60%)
    //      └─ SearchHistory (bottom, ~40%)

    ChildSlot
    [
        SNew(SCkDebug_WindowChrome)
            .WindowId(WindowId)
            .ToolTabId(TEXT("CkAStarDebugger"))
            .DisplayName(FText::FromString(TEXT("CK A* Debugger")))
            .MenuActionsContent()
            [
                SNew(SCkDebug_IconToggle)
                .IconId(TEXT("Hourglass"))
                .Label(FText::FromString(TEXT("Pause capture")))
                .ToolTip(FText::FromString(TEXT("Freeze A* debugger capture; gameplay continues running.")))
                .IsOn_Lambda([this]() { return _ViewModel.IsValid() && _ViewModel->Get_Paused(); })
                .OnStateChanged_Lambda([this](const bool InPaused)
                {
                    if (_ViewModel.IsValid()) { _ViewModel->Set_Paused(InPaused); }
                })
            ]
            .Content()
            [
                SNew(SVerticalBox)

            // Toolbar
            + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(4.0f)
                [
                    BuildToolbar()
                ]

            // Main content
            + SVerticalBox::Slot()
                .FillHeight(1.0f)
                [
                    SNew(SSplitter)
                        .Orientation(Orient_Horizontal)

                        // Grid view (left, ~70%)
                        + SSplitter::Slot()
                            .Value(0.7f)
                            [
                                SAssignNew(_GridView, SCkAStarDebugger_GridView, _ViewModel)
                            ]

                        // Right panel (~30%)
                        + SSplitter::Slot()
                            .Value(0.3f)
                            [
                                SNew(SSplitter)
                                    .Orientation(Orient_Vertical)

                                    // Stats panel (top, ~60%)
                                    + SSplitter::Slot()
                                        .Value(0.6f)
                                        [
                                            SAssignNew(_StatsPanel, SCkAStarDebugger_StatsPanel, _ViewModel)
                                        ]

                                    // Search history (bottom, ~40%)
                                    + SSplitter::Slot()
                                        .Value(0.4f)
                                        [
                                            SAssignNew(_SearchHistory, SCkAStarDebugger_SearchHistory, _ViewModel)
                                        ]
                            ]
                ]
            ]
    ];
}

SCkAStarDebuggerWindow::~SCkAStarDebuggerWindow()
{
    if (_WorldModel.IsValid() && _WorldChangedHandle.IsValid())
    { _WorldModel->OnWorldChanged.Remove(_WorldChangedHandle); }

    if (_SessionInvalidatedHandle.IsValid())
    { ck::DebugSessionLifecycle::Get_OnSessionInvalidated().Remove(_SessionInvalidatedHandle); }
}

auto SCkAStarDebuggerWindow::HandleWorldChanged(UWorld*) -> void
{
    _PendingTarget.Reset();
    _CachedWorld = nullptr;
    _EntitySelectorItems.Reset();
    _EntitySelectorHandles.Reset();

    if (_EntitySelectorLabel.IsValid())
    { _EntitySelectorLabel->SetText(FText::FromString(TEXT("(no entities)"))); }
    if (_StatusBadgeText.IsValid())
    {
        _StatusBadgeText->SetText(FText::FromString(TEXT("Idle")));
        _StatusBadgeText->SetColorAndOpacity(FCkAStarDebuggerStyle::Color_Status_Idle);
    }
    if (_GridView.IsValid())
    { _GridView->Reset_ForWorldChange(); }
    if (_ViewModel.IsValid())
    { _ViewModel->Reset_ForWorldChange(); }
}

auto SCkAStarDebuggerWindow::HandleSessionInvalidated() -> void
{
    if (_WorldModel.IsValid() && _WorldModel->Get_SelectedWorld() != nullptr)
    {
        _WorldModel->Set_SelectedWorld(nullptr);
        return;
    }

    HandleWorldChanged(nullptr);
}

// ====================================================================================================================
// Tick — find PIE world, collect data, feed grid view
// ====================================================================================================================

auto
    SCkAStarDebuggerWindow::
    Tick(
        const FGeometry& InAllottedGeometry,
        double InCurrentTime,
        float InDeltaTime)
    -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(WindowId))
    { return; }

    _WorldModel->Ensure_AutoSelect();
    _CachedWorld = _WorldModel->Get_SelectedWorld();

    if (NOT IsValid(_CachedWorld) || NOT _CachedWorld->HasBegunPlay())
    { return; }

    _ViewModel->Tick(_CachedWorld, InDeltaTime);
    RefreshEntitySelector();

    if (_PendingTarget.IsSet())
    {
        const auto Target = _PendingTarget.GetValue();
        _PendingTarget.Reset();
        for (const auto& Candidate : _EntitySelectorHandles)
        {
            if (Candidate.Get_Entity() != Target) { continue; }
            _ViewModel->Set_SelectedEntityHandle(Candidate);
            const auto SelectedIndex = _EntitySelectorHandles.IndexOfByKey(Candidate);
            if (_EntitySelectorLabel.IsValid() && _EntitySelectorItems.IsValidIndex(SelectedIndex))
            { _EntitySelectorLabel->SetText(FText::FromString(*_EntitySelectorItems[SelectedIndex])); }
            break;
        }
    }

    auto* Info = _ViewModel->Get_CurrentSearchInfo();

    if (Info)
    {
        _GridView->SetSearchInfo(*Info);

        if (_StatusBadgeText.IsValid())
        {
            auto StatusStr = CkAStarDebugger::GetStatusString(Info->SearchStatus);
            auto StatusColor = CkAStarDebugger::GetStatusColor(Info->SearchStatus);
            _StatusBadgeText->SetText(FText::FromString(StatusStr));
            _StatusBadgeText->SetColorAndOpacity(StatusColor);
        }
    }
}

auto SCkAStarDebuggerWindow::TargetEntity(const FCk_Handle& InEntity) -> void
{
    if (ck::IsValid(InEntity)) { _PendingTarget = InEntity.Get_Entity(); }
}

// ====================================================================================================================
// Toolbar
// ====================================================================================================================

auto
    SCkAStarDebuggerWindow::
    BuildToolbar()
    -> TSharedRef<SWidget>
{
    return SNew(SHorizontalBox)

        // World selector (shared across all CK debuggers)
        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(4.0f, 0.0f)
            [
                SNew(SCkDebug_WorldSelector, _WorldModel)
                    .ShowHeaderLabel(false)
            ]

        // "Entity:" label
        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(4.0f, 0.0f)
            [
                SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Entity:")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                    .ColorAndOpacity(FCkAStarDebuggerStyle::Color_Text_Secondary)
            ]

        // Entity selector combo box
        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(4.0f, 0.0f)
            [
                SNew(SComboBox<TSharedPtr<FString>>)
                    .OptionsSource(&_EntitySelectorItems)
                    .OnGenerateWidget_Lambda([](TSharedPtr<FString> InItem) -> TSharedRef<SWidget>
                    {
                        return SNew(STextBlock)
                            .Text(FText::FromString(*InItem))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9));
                    })
                    .OnSelectionChanged_Lambda([this](TSharedPtr<FString> InItem, ESelectInfo::Type)
                    {
                        if (NOT InItem.IsValid()) { return; }

                        auto Idx = _EntitySelectorItems.IndexOfByKey(InItem);

                        if (Idx != INDEX_NONE && Idx < _EntitySelectorHandles.Num())
                        {
                            _ViewModel->Set_SelectedEntityHandle(_EntitySelectorHandles[Idx]);
                        }
                    })
                    [
                        SAssignNew(_EntitySelectorLabel, STextBlock)
                            .Text(FText::FromString(TEXT("(no entities)")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
                    ]
            ]

        // Click-to-open in CK ECS Debugger. The combo immediately to the
        // left already shows the selected entity's DebugName, so the pill
        // renders just the canonical ID (ShowName=false) to avoid duplicating
        // the name twice in adjacent widgets. Same pattern in SM and GOAP.
        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(4.0f, 0.0f)
            [
                SNew(SCkDebug_EntityRef)
                    .Entity_Lambda([this]() -> FCk_Handle
                    {
                        if (NOT _ViewModel.IsValid())
                        { return FCk_Handle{}; }
                        return _ViewModel->Get_SelectedEntityHandle();
                    })
            ]

        // Status badge
        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(8.0f, 0.0f)
            [
                SAssignNew(_StatusBadgeText, STextBlock)
                    .Text(FText::FromString(TEXT("Idle")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                    .ColorAndOpacity(FCkAStarDebuggerStyle::Color_Status_Idle)
            ]

        // Spacer
        + SHorizontalBox::Slot()
            .FillWidth(1.0f)

        // Refresh-mode + rate-cap controls
        + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(12.0f, 0.0f, 0.0f, 0.0f)
            [
                SNew(SCkDebugger_RefreshControls)
                    .WindowId(SCkAStarDebuggerWindow::WindowId)
            ];
}

// ====================================================================================================================
// Entity selector refresh
// ====================================================================================================================

auto
    SCkAStarDebuggerWindow::
    RefreshEntitySelector()
    -> void
{
    if (NOT _ViewModel.IsValid())
    { return; }

    const auto& Entities = _ViewModel->Get_AllSearchEntities();

    if (Entities.Num() == _EntitySelectorItems.Num())
    { return; }

    _EntitySelectorItems.Reset();
    _EntitySelectorHandles.Reset();

    for (const auto& Info : Entities)
    {
        auto Label = FString::Printf(TEXT("%s"), *Info.DebugName);

        if (Info.GridWidth > 0 && Info.GridHeight > 0)
        {
            Label += FString::Printf(TEXT(" \u2014 %dx%d Grid"), Info.GridWidth, Info.GridHeight);
        }

        _EntitySelectorItems.Add(MakeShared<FString>(Label));
        _EntitySelectorHandles.Add(Info.EntityHandle);
    }

    if (_EntitySelectorLabel.IsValid() && _EntitySelectorItems.Num() > 0)
    {
        auto SelectedHandle = _ViewModel->Get_SelectedEntityHandle();
        auto SelectedIdx = _EntitySelectorHandles.IndexOfByKey(SelectedHandle);

        if (SelectedIdx != INDEX_NONE)
        {
            _EntitySelectorLabel->SetText(FText::FromString(*_EntitySelectorItems[SelectedIdx]));
        }
        else
        {
            _EntitySelectorLabel->SetText(FText::FromString(*_EntitySelectorItems[0]));
        }
    }
    else if (_EntitySelectorLabel.IsValid())
    {
        _EntitySelectorLabel->SetText(FText::FromString(TEXT("(no entities)")));
    }
}

// ====================================================================================================================
