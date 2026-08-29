#include "Window/SCkDebuggerSuiteWindow.h"

#include "Styles/CkDebuggerLauncherStyle.h"
#include "Window/SCkDebuggerLauncher.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Launcher/CkDebuggerTabUtils.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SCkDebuggerSuiteWindow"

// ====================================================================================================================

namespace ck_debugger_suite_window
{
    // Wide enough that the rail's own 180 px hysteresis threshold is always cleared, so the host
    // never shows a labelless icon strip with an invisible search box.
    constexpr auto RailWidth = 220.0f;
}

// ====================================================================================================================

auto
    SCkDebuggerSuiteWindow::
    Construct(
        const FArguments& InArgs)
    -> void
{
    ChildSlot
    [
        SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
        .AutoWidth()
        [
            SNew(SBox)
            .WidthOverride(ck_debugger_suite_window::RailWidth)
            [
                SAssignNew(_Rail, SCkDebuggerLauncher)
                .OnToolSelected(FCkDebuggerLauncher_OnToolSelected::CreateSP(
                    this, &SCkDebuggerSuiteWindow::Handle_ToolSelected))
                .SelectedToolId(this, &SCkDebuggerSuiteWindow::Get_SelectedToolId)
            ]
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        [
            SNew(SSeparator)
            .Orientation(Orient_Vertical)
            .SeparatorImage(FCkDebuggerLauncherStyle::Get_SeparatorBrush())
            .Thickness(1.0f)
        ]

        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SBorder)
                .BorderImage(FCkDebuggerLauncherStyle::Get_BackgroundBrush())
                .Padding(FMargin{CkStyle::SpaceM, CkStyle::SpaceS})
                [
                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text(this, &SCkDebuggerSuiteWindow::Get_SelectedDisplayName)
                    ]

                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("PopOut", "Pop Out"))
                        .ToolTipText(LOCTEXT("PopOutTooltip",
                            "Close this tool's embedded instance and reopen it as its own dock tab"))
                        .Visibility(this, &SCkDebuggerSuiteWindow::Get_PopOutVisibility)
                        .OnClicked(this, &SCkDebuggerSuiteWindow::Handle_PopOutClicked)
                    ]
                ]
            ]

            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            [
                SAssignNew(_ContentHost, SBox)
                [
                    Build_EmptyCard()
                ]
            ]
        ]
    ];
}

SCkDebuggerSuiteWindow::~SCkDebuggerSuiteWindow()
{
    // Backstop only: the tab's OnTabClosed path already ran the callback-bearing release. By the
    // time a widget destructor runs the owning module may be mid-unload, so detach silently — and
    // deliberately without going through Release_AllEmbeddedTools, which writes to _ContentHost.
    // This widget is already being destroyed; re-slotting one of its children now is not.
    for (auto& Entry : _EmbeddedTools)
    {
        if (NOT Entry.Value.IsValid())
        { continue; }

        Entry.Value->SetOnTabClosed(SDockTab::FOnTabClosedCallback{});
        Entry.Value->SetContent(SNullWidget::NullWidget);
    }

    _EmbeddedTools.Reset();
}

// ====================================================================================================================

auto
    SCkDebuggerSuiteWindow::
    Release_AllEmbeddedTools(
        bool InRunCloseCallbacks)
    -> void
{
    if (_ContentHost.IsValid())
    { _ContentHost->SetContent(SNullWidget::NullWidget); }

    auto TabIds = TArray<FName>{};
    _EmbeddedTools.GetKeys(TabIds);

    for (const auto& TabId : TabIds)
    { Release_EmbeddedTool(TabId, InRunCloseCallbacks); }

    _EmbeddedTools.Reset();
}

// ====================================================================================================================

auto
    SCkDebuggerSuiteWindow::
    Handle_ToolSelected(
        FName InTabId)
    -> void
{
    _SelectedToolId = InTabId;
    Show_Tool(InTabId);
}

auto
    SCkDebuggerSuiteWindow::
    Show_Tool(
        FName InTabId)
    -> void
{
    if (NOT _ContentHost.IsValid())
    { return; }

    auto Tool = FCkDebuggerToolDescriptor{};

    if (InTabId.IsNone() || NOT TryFind_Tool(InTabId, Tool))
    {
        _SelectedDisplayName = FText::GetEmpty();
        _ContentHost->SetContent(Build_EmptyCard());
        return;
    }

    _SelectedDisplayName = Tool.Get_DisplayName();

    // (a) The tool already owns a real dock tab. Focus it and say so — building a second instance
    // here would hand the owning module a duplicate window it did not ask for, and several modules
    // keep exactly one window pointer.
    if (FGlobalTabmanager::Get()->FindExistingLiveTab(FTabId{InTabId}).IsValid())
    {
        // The tool was opened as its own tab while we still held an embedded instance (the
        // standalone rail and ck.* commands can both do this). Drop ours WITHOUT running its close
        // callback: the owning module's window pointers now describe the global tab, and its
        // OnTabClosed would reset those — tearing down the live tab's bookkeeping.
        Release_EmbeddedTool(InTabId, false);

        ck::debugger_tabs::Invoke_DebuggerTab(InTabId);
        _ContentHost->SetContent(Build_ExternalCard(Tool));
        return;
    }

    // (b) Already embedded — swap back to the instance the user left, state intact.
    if (const auto* Cached = _EmbeddedTools.Find(InTabId);
        Cached != nullptr && Cached->IsValid())
    {
        _ContentHost->SetContent((*Cached)->GetContent());
        return;
    }

    if (Tool.Get_TabFactory().IsBound())
    {
        const auto NewTab = Tool.Get_TabFactory().Execute();
        _EmbeddedTools.Add(InTabId, NewTab);
        _ContentHost->SetContent(NewTab->GetContent());
        return;
    }

    // (c) No factory: this tool can only exist as its own tab.
    ck::debugger_tabs::Invoke_DebuggerTab(InTabId);
    _ContentHost->SetContent(Build_ExternalCard(Tool));
}

auto
    SCkDebuggerSuiteWindow::
    Release_EmbeddedTool(
        FName InTabId,
        bool InRunCloseCallbacks)
    -> void
{
    auto Tab = TSharedPtr<SDockTab>{};

    if (NOT _EmbeddedTools.RemoveAndCopyValue(InTabId, Tab))
    { return; }

    if (NOT Tab.IsValid())
    { return; }

    if (InRunCloseCallbacks && FSlateApplication::IsInitialized() && NOT IsEngineExitRequested())
    {
        // RequestCloseTab on an unparented tab still runs OnPersistVisualState, OnCanCloseTab and
        // OnTabClosed, and its RemoveTabFromParent_Internal is a no-op with no tab well. That is
        // the whole reason the SDockTab was kept: the owning module resets its own window pointers
        // here, exactly as if the user had closed a docked tab.
        Tab->RequestCloseTab();
    }
    else
    {
        // Engine exit / destructor: the module that bound OnTabClosed may already be unloading.
        Tab->SetOnTabClosed(SDockTab::FOnTabClosedCallback{});
    }

    // Drop the widget tree (and any ECS handles it owns) with the last reference, rather than
    // leaving it reachable from a tab nothing displays.
    Tab->SetContent(SNullWidget::NullWidget);
}

// ====================================================================================================================

auto
    SCkDebuggerSuiteWindow::
    Handle_PopOutClicked()
    -> FReply
{
    const auto TabId = _SelectedToolId;

    if (TabId.IsNone())
    { return FReply::Handled(); }

    // Release BEFORE invoking: the module's spawner overwrites the same window pointers the
    // embedded instance is using, so closing after the pop-out would tear down the new tab's
    // bookkeeping instead of the old one's.
    Release_EmbeddedTool(TabId, true);

    if (_ContentHost.IsValid())
    { _ContentHost->SetContent(SNullWidget::NullWidget); }

    ck::debugger_tabs::Invoke_DebuggerTab(TabId);

    auto Tool = FCkDebuggerToolDescriptor{};

    if (_ContentHost.IsValid() && TryFind_Tool(TabId, Tool))
    { _ContentHost->SetContent(Build_ExternalCard(Tool)); }

    return FReply::Handled();
}

auto
    SCkDebuggerSuiteWindow::
    Handle_FocusExternalClicked()
    -> FReply
{
    if (_SelectedToolId.IsNone())
    { return FReply::Handled(); }

    ck::debugger_tabs::Invoke_DebuggerTab(_SelectedToolId);
    return FReply::Handled();
}

// ====================================================================================================================

auto
    SCkDebuggerSuiteWindow::
    Get_SelectedToolId() const
    -> FName
{
    return _SelectedToolId;
}

auto
    SCkDebuggerSuiteWindow::
    Get_SelectedDisplayName() const
    -> FText
{
    // Read per frame; the cache is refreshed by Show_Tool so this never walks the registry.
    if (_SelectedDisplayName.IsEmpty())
    { return LOCTEXT("NoSelection", "CK Debugger Suite"); }

    return _SelectedDisplayName;
}

auto
    SCkDebuggerSuiteWindow::
    Get_PopOutVisibility() const
    -> EVisibility
{
    return _EmbeddedTools.Contains(_SelectedToolId) ? EVisibility::Visible : EVisibility::Collapsed;
}

// ====================================================================================================================

auto
    SCkDebuggerSuiteWindow::
    Build_ExternalCard(
        const FCkDebuggerToolDescriptor& InTool)
    -> TSharedRef<SWidget>
{
    return SNew(SBox)
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        .Padding(FMargin{CkStyle::SpaceL})
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::Format(
                    LOCTEXT("ExternalCardTitle", "{0} is open in its own tab"),
                    InTool.Get_DisplayName()))
                .ColorAndOpacity(FSlateColor::UseSubduedForeground())
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Center)
            .Padding(FMargin{0.0f, CkStyle::SpaceM, 0.0f, 0.0f})
            [
                SNew(SButton)
                .Text(LOCTEXT("FocusExternal", "Focus its tab"))
                .ToolTipText(LOCTEXT("FocusExternalTooltip",
                    "Bring the tool's own dock tab to the front"))
                .OnClicked(this, &SCkDebuggerSuiteWindow::Handle_FocusExternalClicked)
            ]
        ];
}

auto
    SCkDebuggerSuiteWindow::
    Build_EmptyCard()
    -> TSharedRef<SWidget>
{
    return SNew(SBox)
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        .Padding(FMargin{CkStyle::SpaceL})
        [
            SNew(STextBlock)
            .Text(LOCTEXT("EmptySelection", "Pick a debugger from the rail"))
            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
        ];
}

auto
    SCkDebuggerSuiteWindow::
    TryFind_Tool(
        FName InTabId,
        FCkDebuggerToolDescriptor& OutTool)
    -> bool
{
    for (const auto& Tool : FCkDebuggerToolRegistry::Get().Get_Tools())
    {
        if (Tool.Get_TabId() != InTabId)
        { continue; }

        OutTool = Tool;
        return true;
    }

    return false;
}

#undef LOCTEXT_NAMESPACE

// ====================================================================================================================
