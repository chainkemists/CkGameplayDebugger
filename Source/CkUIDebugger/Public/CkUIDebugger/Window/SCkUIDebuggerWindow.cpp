#include "CkUIDebugger/Window/SCkUIDebuggerWindow.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/String/CkFuzzyMatch_Utils.h"

#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_PaneHost.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NameLabel.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/CkUiTreeCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"
#include "CkSlateLayout/SCkUiTree.h"

#include "CkUI/Layout/CkUI_Layout_Subsystem.h"
#include "CkUI/Layout/CkUI_PrimaryGameLayout.h"
#include "CkUI/Layout/CkUI_LayerStack.h"

#include "CommonActivatableWidget.h"

#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------
// Local style + helpers (module-unique namespace name — unity builds concatenate TUs).
// --------------------------------------------------------------------------------------------------------------------

namespace ck_ui_debugger
{
    static auto TextField(const FString& InValue) -> FCkUiFieldValue
    { return FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(InValue)}; }
    static auto BoolField(const bool InValue) -> FCkUiFieldValue
    { return FCkUiFieldValue{.Kind = ECkUiFieldKind::Bool, .Bool = InValue}; }
    static auto ColorField(const FLinearColor& InValue) -> FCkUiFieldValue
    { return FCkUiFieldValue{.Kind = ECkUiFieldKind::Color, .Color = InValue}; }
    static auto HistorySchema() -> TArray<FCkUiFieldSchema>
    { return {{TEXT("description"), ECkUiFieldKind::Text}}; }
    static auto HistoryStyleTokens() -> FCkUiView::FTokens
    {
        const auto Color = [](const FLinearColor& InColor) { return TEXT("#") + InColor.ToFColorSRGB().ToHex(); };
        return {
            {TEXT("--ui-history-font-size"), FString::FromInt(ck::debug_axes::Get_ScaledFontSize(CkStyle::FontSizeBody()))},
            {TEXT("--ui-history-text-dim"), Color(CkStyle::TextDim())},
            {TEXT("--ui-history-text-mute"), Color(CkStyle::TextMute())},
            {TEXT("--ui-history-surface"), Color(CkStyle::BgRoot())},
        };
    }
    static auto CommandStyleTokens() -> FCkUiView::FTokens
    {
        const auto Color = [](const FLinearColor& InColor) { return TEXT("#") + InColor.ToFColorSRGB().ToHex(); };
        return {
            {TEXT("--ui-command-font-size"), FString::FromInt(ck::debug_axes::Get_ScaledFontSize(CkStyle::FontSizeBody()))},
            {TEXT("--ui-command-text"), Color(CkStyle::Text())},
            {TEXT("--ui-command-text-dim"), Color(CkStyle::TextDim())},
            {TEXT("--ui-command-text-mute"), Color(CkStyle::TextMute())},
            {TEXT("--ui-command-surface"), Color(CkStyle::BgRoot())},
            {TEXT("--ui-command-outline"), Color(CkStyle::Border())},
            {TEXT("--ui-command-accent"), Color(CkStyle::Accent())},
            {TEXT("--ui-command-button-surface"), Color(CkStyle::Bg1())},
            {TEXT("--ui-command-hover-surface"), Color(CkStyle::Hover())},
            {TEXT("--ui-command-pressed-surface"), Color(CkStyle::Bg3())},
            {TEXT("--ui-command-disabled-surface"), Color(CkStyle::BgRoot())},
            {TEXT("--ui-command-toggle-surface"), Color(CkStyle::AccentDim())},
            {TEXT("--ui-command-toggle-hover-surface"), Color(CkStyle::Selection())},
            {TEXT("--ui-command-toggle-pressed-surface"), Color(CkStyle::Bg3())},
            {TEXT("--ui-command-danger-surface"), Color(CkStyle::ErrDim())},
            {TEXT("--ui-command-danger-outline"), Color(CkStyle::Err())},
            {TEXT("--ui-command-danger-hover-surface"), Color(CkStyle::ErrDim().CopyWithNewOpacity(0.85f))},
            {TEXT("--ui-command-danger-hover-outline"), Color(CkStyle::Err())},
            {TEXT("--ui-command-danger-pressed-surface"), Color(CkStyle::Bg3())},
            {TEXT("--ui-command-danger-text"), Color(CkStyle::Err())},
            {TEXT("--ui-command-group-surface"), Color(CkStyle::Bg2())},
        };
    }
    static auto LayerSchema() -> TArray<FCkUiFieldSchema>
    {
        return {
            {TEXT("node-visible"), ECkUiFieldKind::Bool},
            {TEXT("is-layer"), ECkUiFieldKind::Bool},
            {TEXT("is-widget"), ECkUiFieldKind::Bool},
            {TEXT("status-glyph"), ECkUiFieldKind::Text},
            {TEXT("status-color"), ECkUiFieldKind::Color},
            {TEXT("layer-tag"), ECkUiFieldKind::Text},
            {TEXT("layer-tag-tooltip"), ECkUiFieldKind::Text},
            {TEXT("layer-tag-color"), ECkUiFieldKind::Color},
            {TEXT("priority"), ECkUiFieldKind::Text},
            {TEXT("input-mode"), ECkUiFieldKind::Text},
            {TEXT("widget-count"), ECkUiFieldKind::Text},
            {TEXT("widget-name"), ECkUiFieldKind::Text},
            {TEXT("widget-name-tooltip"), ECkUiFieldKind::Text},
            {TEXT("widget-name-color"), ECkUiFieldKind::Color},
            {TEXT("widget-state"), ECkUiFieldKind::Text},
            {TEXT("widget-state-foreground"), ECkUiFieldKind::Color},
            {TEXT("widget-state-background"), ECkUiFieldKind::Color},
        };
    }
    static auto LayerStyleTokens() -> FCkUiView::FTokens
    {
        const auto Color = [](const FLinearColor& InColor) { return TEXT("#") + InColor.ToFColorSRGB().ToHex(); };
        return {
            {TEXT("--ui-layer-font-size"), FString::FromInt(ck::debug_axes::Get_ScaledFontSize(CkStyle::FontSizeBody()))},
            {TEXT("--ui-layer-text"), Color(CkStyle::Text())},
            {TEXT("--ui-layer-text-dim"), Color(CkStyle::TextDim())},
            {TEXT("--ui-layer-accent"), Color(CkStyle::Accent())},
            {TEXT("--ui-layer-surface"), Color(CkStyle::BgRoot())},
            {TEXT("--ui-layer-surface-raised"), Color(CkStyle::Bg1())},
        };
    }
    static auto SummaryStyleTokens() -> FCkUiView::FTokens
    {
        const auto Color = [](const FLinearColor& InColor) { return TEXT("#") + InColor.ToFColorSRGB().ToHex(); };
        return {
            {TEXT("--ui-summary-label-font-size"), FString::FromInt(ck::debug_axes::Get_ScaledFontSize(CkStyle::FontSizeMicro()))},
            {TEXT("--ui-summary-value-font-size"), FString::FromInt(ck::debug_axes::Get_ScaledFontSize(CkStyle::FontSizeH3()))},
            {TEXT("--ui-summary-text"), Color(CkStyle::Text())},
            {TEXT("--ui-summary-text-mute"), Color(CkStyle::TextMute())},
            {TEXT("--ui-summary-surface"), Color(CkStyle::BgRoot())},
            {TEXT("--ui-summary-outline"), Color(CkStyle::Border())},
            {TEXT("--ui-summary-metric-surface"), Color(CkStyle::Bg1())},
            {TEXT("--ui-summary-metric-outline"), Color(CkStyle::Border())},
            {TEXT("--ui-summary-active-surface"), Color(CkStyle::AccentDim())},
            {TEXT("--ui-summary-active-outline"), Color(CkStyle::Accent())},
            {TEXT("--ui-summary-active-text"), Color(CkStyle::Accent())},
        };
    }
    static auto InputModeToString(ECk_UI_InputMode InMode) -> FString
    {
        switch (InMode)
        {
            case ECk_UI_InputMode::GameOnly:   return TEXT("GameOnly");
            case ECk_UI_InputMode::GameAndUI:  return TEXT("GameAndUI");
            case ECk_UI_InputMode::UIOnly:     return TEXT("UIOnly");
            default:                           return TEXT("Unknown");
        }
    }

    static auto FindLayoutSubsystem() -> UCk_UI_Layout_Subsystem_UE*
    {
        if (NOT GEngine)
        { return nullptr; }

        for (const auto& Context : GEngine->GetWorldContexts())
        {
            auto* World = Context.World();

            if (ck::Is_NOT_Valid(World))
            { continue; }

            if (World->WorldType != EWorldType::Game && World->WorldType != EWorldType::PIE)
            { continue; }

            auto* GameInstance = World->GetGameInstance();

            if (ck::Is_NOT_Valid(GameInstance))
            { continue; }

            const auto& LocalPlayers = GameInstance->GetLocalPlayers();

            if (LocalPlayers.IsEmpty())
            { continue; }

            auto* LocalPlayer = LocalPlayers[0];

            if (ck::Is_NOT_Valid(LocalPlayer))
            { continue; }

            auto* LayoutSubsystem = LocalPlayer->GetSubsystem<UCk_UI_Layout_Subsystem_UE>();

            if (ck::IsValid(LayoutSubsystem) && LayoutSubsystem->Has_Layout())
            { return LayoutSubsystem; }
        }

        return nullptr;
    }
}

// --------------------------------------------------------------------------------------------------------------------
// Lifecycle
// --------------------------------------------------------------------------------------------------------------------

SCkUIDebuggerWindow::~SCkUIDebuggerWindow()
{
    DoUnbindLayoutEvents();
    _CommandView.Reset();
    _SummaryView.Reset();
    _LayerView.Reset();
    _HistoryView.Reset();
}

// --------------------------------------------------------------------------------------------------------------------
// Construct
// --------------------------------------------------------------------------------------------------------------------

const FName SCkUIDebuggerWindow::WindowId = FName(TEXT("UIDebugger"));

auto
    SCkUIDebuggerWindow::
    Construct(
        const FArguments& InArgs)
    -> void
{
    Register_WithGate();

    _LayerHost = SNew(SBox);
    _HistoryHost = SNew(SBox);
    _SummaryHost = SNew(SBox);
    _CommandHost = SNew(SBox);
    const FCkUiLoadResult HistoryCollectionResult = FCkUiCollection::TryCreate(
        ck_ui_debugger::HistorySchema(), _HistoryCollection);
    if (NOT HistoryCollectionResult.Succeeded)
    {
        _HistoryHost->SetContent(SNew(STextBlock).Text(
            FText::FromString(FString::Join(HistoryCollectionResult.Errors, TEXT("\n")))));
    }
    const FCkUiLoadResult LayerCollectionResult = FCkUiTreeCollection::TryCreate(
        ck_ui_debugger::LayerSchema(), _LayerCollection);
    if (NOT LayerCollectionResult.Succeeded)
    {
        _LayerHost->SetContent(SNew(STextBlock).Text(
            FText::FromString(FString::Join(LayerCollectionResult.Errors, TEXT("\n")))));
    }

    // ---- History area ----

    // ---- Root layout ----

    ChildSlot
    [
        SNew(SCkDebug_WindowChrome)
        .WindowId(WindowId)
        .ToolTabId(TEXT("CkUIDebugger"))
        .ShowRefreshControls(true)
        .Content()
        [
            SNew(SCkDebug_PaneHost)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceM, CkStyle::SpaceS, CkStyle::SpaceM, 0.0f)
                [ _CommandHost.ToSharedRef() ]

            + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceM, CkStyle::SpaceS)
                [ _SummaryHost.ToSharedRef() ]

            + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceM, 0.0f)
                [ ck::debug_axes::Make_AxisSeparator() ]

            + SVerticalBox::Slot().FillHeight(1.0f).Padding(CkStyle::SpaceS)
                [ _LayerHost.ToSharedRef() ]

            + SVerticalBox::Slot().AutoHeight()
                [ _HistoryHost.ToSharedRef() ]
        ]
        ]
    ];
    DoBuildCommandView();
    DoBuildLayerView();
    DoBuildSummaryView();
    DoBuildHistoryView();
}

// --------------------------------------------------------------------------------------------------------------------
// Tick
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkUIDebuggerWindow::
    Tick(
        const FGeometry& InAllottedGeometry,
        double InCurrentTime,
        float InDeltaTime)
    -> void
{
    SCkDebugger_WindowBase::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    DoPollCommandFiles(InCurrentTime);
    DoPollLayerFiles(InCurrentTime);
    DoPollSummaryFiles(InCurrentTime);
    DoPollHistoryFiles(InCurrentTime);

    if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(WindowId))
    { return; }

    // ---- Resolve layout subsystem (PIE may start/stop) ----

    auto* LayoutSubsystem = ck_ui_debugger::FindLayoutSubsystem();
    auto* Layout = ck::IsValid(LayoutSubsystem)
        ? LayoutSubsystem->Get_Layout()
        : static_cast<UCk_UI_PrimaryGameLayout_UE*>(nullptr);

    if (Layout != _BoundLayout.Get())
    {
        DoUnbindLayoutEvents();

        if (ck::IsValid(Layout))
        {
            DoBindLayoutEvents(Layout);
        }

        _StructureDirty = true;
    }

    if (_IsPostLayerTransitionRefreshPending && _BoundLayout.IsValid())
    {
        if (NOT _BoundLayout->IsAnyLayerTransitioning())
        {
            _IsPostLayerTransitionRefreshPending = false;
            _IsDirty = true;
        }
    }
    else if (_IsPostLayerTransitionRefreshPending)
    {
        _IsPostLayerTransitionRefreshPending = false;
    }

    // ---- Rebuild structure if layout changed ----

    if (_StructureDirty)
    {
        _StructureDirty = false;
        _IsDirty = false;
        DoBuildLayerView();
        DoUpdateAllSlots();
        DoPublishHistoryRecords();
        return;
    }

    // ---- Update in-place if content changed ----

    if (_IsDirty)
    {
        _IsDirty = false;
        DoUpdateAllSlots();
        DoPublishHistoryRecords();
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkUIDebuggerWindow::
    OnStyleRevisionChanged()
    -> void
{
    if (_CommandView.IsValid()) { _CommandView->PollFiles(ck_ui_debugger::CommandStyleTokens()); }
    if (_LayerView.IsValid()) { _LayerView->PollFiles(ck_ui_debugger::LayerStyleTokens()); }
    if (_SummaryView.IsValid()) { _SummaryView->PollFiles(ck_ui_debugger::SummaryStyleTokens()); }
    _StructureDirty = true;
}

// --------------------------------------------------------------------------------------------------------------------
// Event Binding
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkUIDebuggerWindow::
    DoBindLayoutEvents(
        UCk_UI_PrimaryGameLayout_UE* InLayout)
    -> void
{
    if (ck::Is_NOT_Valid(InLayout))
    { return; }

    _BoundLayout = InLayout;

    InLayout->OnWidgetPushed.AddRaw(this, &SCkUIDebuggerWindow::HandleWidgetPushed);
    InLayout->OnWidgetPopped.AddRaw(this, &SCkUIDebuggerWindow::HandleWidgetPopped);
    InLayout->OnLayerCleared.AddRaw(this, &SCkUIDebuggerWindow::HandleLayerCleared);
    InLayout->OnActiveLayerChanged.AddRaw(this, &SCkUIDebuggerWindow::HandleActiveLayerChanged);
    InLayout->OnInputModeChanged.AddRaw(this, &SCkUIDebuggerWindow::HandleInputModeChanged);
}

auto
    SCkUIDebuggerWindow::
    DoUnbindLayoutEvents()
    -> void
{
    _IsPostLayerTransitionRefreshPending = false;

    if (NOT _BoundLayout.IsValid())
    { return; }

    auto* Layout = _BoundLayout.Get();
    Layout->OnWidgetPushed.RemoveAll(this);
    Layout->OnWidgetPopped.RemoveAll(this);
    Layout->OnLayerCleared.RemoveAll(this);
    Layout->OnActiveLayerChanged.RemoveAll(this);
    Layout->OnInputModeChanged.RemoveAll(this);

    _BoundLayout.Reset();
}

// --------------------------------------------------------------------------------------------------------------------
// Event Handlers
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkUIDebuggerWindow::
    HandleWidgetPushed(
        FGameplayTag InLayerTag,
        UCommonActivatableWidget* InWidget)
    -> void
{
    const auto ClassName = ck::IsValid(InWidget, ck::IsValid_Policy_NullptrOnly{})
        ? DoShortName(InWidget->GetClass()->GetName())
        : FString(TEXT("Unknown"));

    DoAppendHistoryEvent(FString::Printf(TEXT("[Push] %s -> %s"), *ClassName, *DoShortName(InLayerTag.ToString())));
    _IsPostLayerTransitionRefreshPending = true;
}

auto
    SCkUIDebuggerWindow::
    HandleWidgetPopped(
        FGameplayTag InLayerTag,
        UCommonActivatableWidget* InWidget)
    -> void
{
    const auto ClassName = ck::IsValid(InWidget, ck::IsValid_Policy_NullptrOnly{})
        ? DoShortName(InWidget->GetClass()->GetName())
        : FString(TEXT("Unknown"));

    DoAppendHistoryEvent(FString::Printf(TEXT("[Pop] %s <- %s"), *ClassName, *DoShortName(InLayerTag.ToString())));
    _IsPostLayerTransitionRefreshPending = true;
}

auto
    SCkUIDebuggerWindow::
    HandleLayerCleared(
        FGameplayTag InLayerTag)
    -> void
{
    DoAppendHistoryEvent(FString::Printf(TEXT("[Cleared] %s"), *DoShortName(InLayerTag.ToString())));
    _IsPostLayerTransitionRefreshPending = true;
}

auto
    SCkUIDebuggerWindow::
    HandleActiveLayerChanged(
        FGameplayTag InNewActiveTag)
    -> void
{
    DoAppendHistoryEvent(FString::Printf(TEXT("Active Layer -> %s"), *DoShortName(InNewActiveTag.ToString())));
}

auto
    SCkUIDebuggerWindow::
    HandleInputModeChanged(
        ECk_UI_InputMode InNewMode)
    -> void
{
    DoAppendHistoryEvent(FString::Printf(TEXT("Input Mode -> %s"), *ck_ui_debugger::InputModeToString(InNewMode)));
}

// --------------------------------------------------------------------------------------------------------------------
// Authored commands
// --------------------------------------------------------------------------------------------------------------------

auto SCkUIDebuggerWindow::DoBuildCommandView() -> void
{
    if (!_CommandHost.IsValid() || _CommandView.IsValid()) { return; }

    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    if (NOT RegistryResult.Succeeded)
    {
        _CommandHost->SetContent(SNew(STextBlock).Text(
            FText::FromString(FString::Join(RegistryResult.Errors, TEXT("\n")))));
        return;
    }

    const TWeakPtr<SCkUIDebuggerWindow> WeakWindow{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakWindow]() { return WeakWindow.IsValid(); });
    Data.Text.Add(TEXT("ui-layer-filter"), TAttribute<FText>::CreateLambda([WeakWindow]()
    {
        const TSharedPtr<SCkUIDebuggerWindow> Window = WeakWindow.Pin();
        return Window.IsValid() ? FText::FromString(Window->_SearchFilter) : FText::GetEmpty();
    }));
    Data.TextChanged.Add(TEXT("ui-layer-filter"), FOnTextChanged::CreateLambda([WeakWindow](const FText& InText)
    {
        const TSharedPtr<SCkUIDebuggerWindow> Window = WeakWindow.Pin();
        if (!Window.IsValid()) { return; }
        Window->_SearchFilter = InText.ToString();
        Window->DoUpdateAllSlots();
    }));
    Data.Text.Add(TEXT("ui-active-layer-only-label"), TAttribute<FText>::CreateLambda([WeakWindow]()
    {
        const TSharedPtr<SCkUIDebuggerWindow> Window = WeakWindow.Pin();
        return FText::FromString(Window.IsValid() && Window->_ShowActiveLayerOnly
            ? TEXT("Active only: ON") : TEXT("Active only: OFF"));
    }));
    Data.Color.Add(TEXT("ui-active-layer-only-color"), TAttribute<FLinearColor>::CreateLambda([WeakWindow]()
    {
        const TSharedPtr<SCkUIDebuggerWindow> Window = WeakWindow.Pin();
        return Window.IsValid() && Window->_ShowActiveLayerOnly ? CkStyle::Accent() : CkStyle::TextDim();
    }));
    Data.Text.Add(TEXT("ui-name-depth-value"), TAttribute<FText>::CreateLambda([WeakWindow]()
    {
        const TSharedPtr<SCkUIDebuggerWindow> Window = WeakWindow.Pin();
        if (!Window.IsValid() || Window->_NameDepth == 0) { return FText::FromString(TEXT("Full")); }
        return FText::AsNumber(Window->_NameDepth);
    }));
    Data.Visibility.Add(TEXT("ui-layer-filter-has-text"), TAttribute<bool>::CreateLambda([WeakWindow]()
    {
        const TSharedPtr<SCkUIDebuggerWindow> Window = WeakWindow.Pin();
        return Window.IsValid() && !Window->_SearchFilter.IsEmpty();
    }));

    auto Actions = FCkUiView::FActions{};
    Actions.Add(TEXT("ui-clear-layer-filter"), FSimpleDelegate::CreateLambda([WeakWindow]()
    {
        const TSharedPtr<SCkUIDebuggerWindow> Window = WeakWindow.Pin();
        if (!Window.IsValid()) { return; }
        Window->_SearchFilter.Empty();
        Window->DoUpdateAllSlots();
    }));
    Actions.Add(TEXT("ui-toggle-active-layer-only"), FSimpleDelegate::CreateLambda([WeakWindow]()
    {
        const TSharedPtr<SCkUIDebuggerWindow> Window = WeakWindow.Pin();
        if (!Window.IsValid()) { return; }
        Window->_ShowActiveLayerOnly = !Window->_ShowActiveLayerOnly;
        Window->_IsDirty = true;
    }));
    Actions.Add(TEXT("ui-force-refresh"), FSimpleDelegate::CreateLambda([WeakWindow]()
    {
        if (const TSharedPtr<SCkUIDebuggerWindow> Window = WeakWindow.Pin(); Window.IsValid())
        {
            ++Window->_ForcedLayerRefreshGeneration;
            Window->_StructureDirty = true;
        }
    }));
    Actions.Add(TEXT("ui-expand-all"), FSimpleDelegate::CreateLambda([WeakWindow]()
    {
        if (const TSharedPtr<SCkUIDebuggerWindow> Window = WeakWindow.Pin(); Window.IsValid())
        { Window->DoExpandAll(); }
    }));
    Actions.Add(TEXT("ui-collapse-all"), FSimpleDelegate::CreateLambda([WeakWindow]()
    {
        if (const TSharedPtr<SCkUIDebuggerWindow> Window = WeakWindow.Pin(); Window.IsValid())
        { Window->DoCollapseAll(); }
    }));
    Actions.Add(TEXT("ui-clear-history"), FSimpleDelegate::CreateLambda([WeakWindow]()
    {
        const TSharedPtr<SCkUIDebuggerWindow> Window = WeakWindow.Pin();
        if (!Window.IsValid()) { return; }
        Window->_HistoryEvents.Empty();
        Window->DoPublishHistoryRecords();
    }));
    Actions.Add(TEXT("ui-name-depth-previous"), FSimpleDelegate::CreateLambda([WeakWindow]()
    {
        if (const TSharedPtr<SCkUIDebuggerWindow> Window = WeakWindow.Pin(); Window.IsValid())
        { Window->DoCycleNameDepth(-1); }
    }));
    Actions.Add(TEXT("ui-name-depth-next"), FSimpleDelegate::CreateLambda([WeakWindow]()
    {
        if (const TSharedPtr<SCkUIDebuggerWindow> Window = WeakWindow.Pin(); Window.IsValid())
        { Window->DoCycleNameDepth(1); }
    }));

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (!Plugin.IsValid())
    {
        _CommandHost->SetContent(SNew(STextBlock).Text(
            FText::FromString(TEXT("CkDebugger resources are unavailable."))));
        return;
    }

    const TSharedRef<FCkUiView> View = FCkUiView::Create({}, MoveTemp(Actions),
        ck_ui_debugger::CommandStyleTokens(), CkStyle::RegularFont(CkStyle::FontSizeBody()), MoveTemp(Data), Registry);
    const TSharedRef<SWidget> Commands = View->GetRegion(TEXT("commands"));
    const FString Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    View->SetFiles(FPaths::Combine(Directory, TEXT("UiDebuggerCommands.ui.html")),
        FPaths::Combine(Directory, TEXT("UiDebuggerCommands.ui.css")));
    View->PollFiles();
    if (NOT View->GetLastResult().Succeeded)
    {
        _CommandHost->SetContent(SNew(STextBlock).Text(
            FText::FromString(FString::Join(View->GetLastResult().Errors, TEXT("\n")))));
        return;
    }

    _CommandView = View;
    _CommandHost->SetContent(Commands);
}

auto SCkUIDebuggerWindow::DoPollCommandFiles(const double InCurrentTime) -> void
{
    constexpr double PollIntervalSeconds = 0.5;
    if (InCurrentTime < _NextCommandPollSeconds) { return; }
    _NextCommandPollSeconds = InCurrentTime + PollIntervalSeconds;
    if (_CommandView.IsValid()) { _CommandView->PollFiles(ck_ui_debugger::CommandStyleTokens()); }
    else { DoBuildCommandView(); }
}

// --------------------------------------------------------------------------------------------------------------------
// Structure Building (one-time when layout binds)
// --------------------------------------------------------------------------------------------------------------------

auto SCkUIDebuggerWindow::DoBuildLayerView() -> void
{
    if (!_LayerHost.IsValid() || !_LayerCollection.IsValid() || _LayerView.IsValid()) { return; }

    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    if (NOT RegistryResult.Succeeded)
    {
        _LayerHost->SetContent(SNew(STextBlock).Text(FText::FromString(FString::Join(RegistryResult.Errors, TEXT("\n")))));
        return;
    }

    const TWeakPtr<SCkUIDebuggerWindow> WeakWindow{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakWindow]() { return WeakWindow.IsValid(); });
    Data.Trees.Add(TEXT("ui-layer-nodes"), _LayerCollection);

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (!Plugin.IsValid())
    {
        _LayerHost->SetContent(SNew(STextBlock).Text(FText::FromString(TEXT("CkDebugger resources are unavailable."))));
        return;
    }

    const TSharedRef<FCkUiView> View = FCkUiView::Create({}, {}, ck_ui_debugger::LayerStyleTokens(),
        CkStyle::RegularFont(CkStyle::FontSizeBody()), MoveTemp(Data), Registry);
    const TSharedRef<SWidget> Layers = View->GetRegion(TEXT("layers"));
    const FString Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    View->SetFiles(FPaths::Combine(Directory, TEXT("UiDebuggerLayers.ui.html")),
        FPaths::Combine(Directory, TEXT("UiDebuggerLayers.ui.css")));
    View->PollFiles();
    if (NOT View->GetLastResult().Succeeded)
    {
        _LayerHost->SetContent(SNew(STextBlock).Text(FText::FromString(FString::Join(View->GetLastResult().Errors, TEXT("\n")))));
        return;
    }

    _LayerView = View;
    _LayerHost->SetContent(Layers);
}

auto SCkUIDebuggerWindow::DoPublishLayerNodes() -> void
{
    if (!_LayerCollection.IsValid()) { return; }
    auto Nodes = TArray<FCkUiTreeNodeData>{};
    if (NOT _BoundLayout.IsValid())
    {
        _LayerCollection->TrySetNodes(MoveTemp(Nodes));
        return;
    }

    struct FLayerEntry { UCk_UI_LayerStack_UE* Stack = nullptr; bool IsTransitioning = false; };
    auto Entries = TArray<FLayerEntry>{};
    _BoundLayout->ForEachLayer([&Entries](UCk_UI_LayerStack_UE* InStack, const bool InIsTransitioning)
    {
        if (ck::IsValid(InStack)) { Entries.Add({InStack, InIsTransitioning}); }
    });
    Entries.Sort([](const FLayerEntry& A, const FLayerEntry& B)
    {
        if (A.Stack->Get_Priority() != B.Stack->Get_Priority()) { return A.Stack->Get_Priority() > B.Stack->Get_Priority(); }
        return A.Stack->Get_LayerTag().ToString() < B.Stack->Get_LayerTag().ToString();
    });

    const FGameplayTag ActiveTag = _BoundLayout->Get_ActiveLayerTag();
    Nodes.Reserve(Entries.Num() * (MaxWidgetsPerLayer + 1));
    for (const FLayerEntry& Entry : Entries)
    {
        UCk_UI_LayerStack_UE* Stack = Entry.Stack;
        const FString FullTag = Stack->Get_LayerTag().ToString();
        const bool IsActive = Stack->Get_LayerTag() == ActiveTag;
        const bool IsVisible = DoMatchesFilter(FullTag) && (!_ShowActiveLayerOnly || IsActive);
        const FLinearColor StatusColor = Entry.IsTransitioning ? CkStyle::Warn()
            : IsActive ? CkStyle::Ok() : Stack->HasWidgets() ? CkStyle::TextDim() : CkStyle::TextMute();
        const FString LayerKey = FullTag;
        const auto EmptyText = ck_ui_debugger::TextField(TEXT(""));
        auto Layer = FCkUiTreeNodeData{};
        Layer.Key = LayerKey;
        Layer.Fields = {
            {TEXT("node-visible"), ck_ui_debugger::BoolField(IsVisible)},
            {TEXT("is-layer"), ck_ui_debugger::BoolField(true)},
            {TEXT("is-widget"), ck_ui_debugger::BoolField(false)},
            {TEXT("status-glyph"), ck_ui_debugger::TextField(TEXT("●"))},
            {TEXT("status-color"), ck_ui_debugger::ColorField(StatusColor)},
            {TEXT("layer-tag"), ck_ui_debugger::TextField(DoShortName(FullTag))},
            {TEXT("layer-tag-tooltip"), ck_ui_debugger::TextField(FullTag)},
            {TEXT("layer-tag-color"), ck_ui_debugger::ColorField(IsActive ? CkStyle::TextStrong() : CkStyle::Text())},
            {TEXT("priority"), ck_ui_debugger::TextField(FString::Printf(TEXT("[%d]"), Stack->Get_Priority()))},
            {TEXT("input-mode"), ck_ui_debugger::TextField(ck_ui_debugger::InputModeToString(Stack->Get_DefaultInputMode()))},
            {TEXT("widget-count"), ck_ui_debugger::TextField(FString::Printf(TEXT("(%d)"), Stack->GetWidgetList().Num()))},
            {TEXT("widget-name"), EmptyText}, {TEXT("widget-name-tooltip"), EmptyText},
            {TEXT("widget-name-color"), ck_ui_debugger::ColorField(CkStyle::TextDim())},
            {TEXT("widget-state"), EmptyText},
            {TEXT("widget-state-foreground"), ck_ui_debugger::ColorField(CkStyle::TextDim())},
            {TEXT("widget-state-background"), ck_ui_debugger::ColorField(CkStyle::BgRoot())},
        };
        Nodes.Add(MoveTemp(Layer));

        const auto& Widgets = Stack->GetWidgetList();
        UCommonActivatableWidget* ActiveWidget = Stack->GetActiveWidget();
        for (int32 WidgetIndex = 0; WidgetIndex < Widgets.Num() && WidgetIndex < MaxWidgetsPerLayer; ++WidgetIndex)
        {
            UCommonActivatableWidget* Widget = Widgets[WidgetIndex];
            if (!ck::IsValid(Widget)) { continue; }
            const bool IsActiveWidget = Widget == ActiveWidget;
            const FString FullName = Widget->GetClass()->GetName();
            auto Child = FCkUiTreeNodeData{};
            Child.Key = FString::Printf(TEXT("%s|%s"), *LayerKey, *Widget->GetPathName());
            Child.ParentKey = LayerKey;
            Child.Fields = {
                {TEXT("node-visible"), ck_ui_debugger::BoolField(IsVisible)},
                {TEXT("is-layer"), ck_ui_debugger::BoolField(false)},
                {TEXT("is-widget"), ck_ui_debugger::BoolField(true)},
                {TEXT("status-glyph"), ck_ui_debugger::TextField(TEXT("●"))},
                {TEXT("status-color"), ck_ui_debugger::ColorField(IsActiveWidget ? CkStyle::Ok() : CkStyle::TextMute())},
                {TEXT("layer-tag"), EmptyText}, {TEXT("layer-tag-tooltip"), EmptyText},
                {TEXT("layer-tag-color"), ck_ui_debugger::ColorField(CkStyle::TextDim())},
                {TEXT("priority"), EmptyText}, {TEXT("input-mode"), EmptyText}, {TEXT("widget-count"), EmptyText},
                {TEXT("widget-name"), ck_ui_debugger::TextField(DoShortName(FullName))},
                {TEXT("widget-name-tooltip"), ck_ui_debugger::TextField(FullName)},
                {TEXT("widget-name-color"), ck_ui_debugger::ColorField(IsActiveWidget ? CkStyle::Text() : CkStyle::TextDim())},
                {TEXT("widget-state"), ck_ui_debugger::TextField(IsActiveWidget ? TEXT("Active") : TEXT("Inactive"))},
                {TEXT("widget-state-foreground"), ck_ui_debugger::ColorField(IsActiveWidget ? CkStyle::Ok() : CkStyle::TextDim())},
                {TEXT("widget-state-background"), ck_ui_debugger::ColorField(IsActiveWidget ? CkStyle::GetToneDimColor(ECk_Tone::Ok) : CkStyle::GetToneDimColor(ECk_Tone::Neutral))},
            };
            Nodes.Add(MoveTemp(Child));
        }
    }
    const FCkUiLoadResult PublicationResult = _LayerCollection->TrySetNodes(MoveTemp(Nodes));
    TSharedPtr<SCkUiTree> Tree;
    if (_LayerView.IsValid()) { Tree = _LayerView->GetTree(TEXT("ui-layer-tree")); }
    if (PublicationResult.Succeeded && Tree.IsValid())
    {
        for (const TSharedPtr<const FCkUiTreeNode>& Root : _LayerCollection->GetRoots())
        {
            if (Root.IsValid() && !_InitializedLayerRootKeys.Contains(Root->GetKey())
                && Tree->TrySetExpanded(Root->GetKey(), true))
            { _InitializedLayerRootKeys.Add(Root->GetKey()); }
        }
    }
}

auto SCkUIDebuggerWindow::DoPollLayerFiles(const double InCurrentTime) -> void
{
    constexpr double PollIntervalSeconds = 0.5;
    if (InCurrentTime < _NextLayerPollSeconds) { return; }
    _NextLayerPollSeconds = InCurrentTime + PollIntervalSeconds;
    if (_LayerView.IsValid()) { _LayerView->PollFiles(ck_ui_debugger::LayerStyleTokens()); }
    else { DoBuildLayerView(); }
}

auto SCkUIDebuggerWindow::DoBuildSummaryView() -> void
{
    if (!_SummaryHost.IsValid() || _SummaryView.IsValid()) { return; }
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    if (NOT RegistryResult.Succeeded)
    {
        _SummaryHost->SetContent(SNew(STextBlock).Text(
            FText::FromString(FString::Join(RegistryResult.Errors, TEXT("\n")))));
        return;
    }

    const TWeakPtr<SCkUIDebuggerWindow> WeakWindow{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakWindow]() { return WeakWindow.IsValid(); });
    Data.Text.Add(TEXT("summary-active-tag"), TAttribute<FText>::CreateLambda([WeakWindow]()
    {
        const TSharedPtr<SCkUIDebuggerWindow> Window = WeakWindow.Pin();
        return Window.IsValid() ? Window->_SummaryActiveTag : FText::GetEmpty();
    }));
    Data.Text.Add(TEXT("summary-input-mode"), TAttribute<FText>::CreateLambda([WeakWindow]()
    {
        const TSharedPtr<SCkUIDebuggerWindow> Window = WeakWindow.Pin();
        return Window.IsValid() ? Window->_SummaryInputMode : FText::GetEmpty();
    }));
    Data.Text.Add(TEXT("summary-layer-count"), TAttribute<FText>::CreateLambda([WeakWindow]()
    {
        const TSharedPtr<SCkUIDebuggerWindow> Window = WeakWindow.Pin();
        return Window.IsValid() ? Window->_SummaryLayerCount : FText::GetEmpty();
    }));
    Data.Text.Add(TEXT("summary-no-active-layout"),
        FText::FromString(TEXT("No active layout. Start PIE to see layer data.")));
    Data.Visibility.Add(TEXT("summary-has-active-layout"), TAttribute<bool>::CreateLambda([WeakWindow]()
    {
        const TSharedPtr<SCkUIDebuggerWindow> Window = WeakWindow.Pin();
        return Window.IsValid() && Window->_HasActiveLayout;
    }));
    Data.Visibility.Add(TEXT("summary-no-active-layout-visible"), TAttribute<bool>::CreateLambda([WeakWindow]()
    {
        const TSharedPtr<SCkUIDebuggerWindow> Window = WeakWindow.Pin();
        return Window.IsValid() && !Window->_HasActiveLayout;
    }));

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (!Plugin.IsValid())
    {
        _SummaryHost->SetContent(SNew(STextBlock).Text(
            FText::FromString(TEXT("CkDebugger resources are unavailable."))));
        return;
    }
    const TSharedRef<FCkUiView> View = FCkUiView::Create({}, {}, ck_ui_debugger::SummaryStyleTokens(),
        CkStyle::RegularFont(CkStyle::FontSizeBody()), MoveTemp(Data), Registry);
    const TSharedRef<SWidget> Summary = View->GetRegion(TEXT("summary"));
    const FString Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    View->SetFiles(FPaths::Combine(Directory, TEXT("UiDebuggerSummary.ui.html")),
        FPaths::Combine(Directory, TEXT("UiDebuggerSummary.ui.css")));
    View->PollFiles();
    if (NOT View->GetLastResult().Succeeded)
    {
        _SummaryHost->SetContent(SNew(STextBlock).Text(
            FText::FromString(FString::Join(View->GetLastResult().Errors, TEXT("\n")))));
        return;
    }
    _SummaryView = View;
    _SummaryHost->SetContent(Summary);
}

auto SCkUIDebuggerWindow::DoPollSummaryFiles(const double InCurrentTime) -> void
{
    constexpr double PollIntervalSeconds = 0.5;
    if (InCurrentTime < _NextSummaryPollSeconds) { return; }
    _NextSummaryPollSeconds = InCurrentTime + PollIntervalSeconds;
    if (_SummaryView.IsValid()) { _SummaryView->PollFiles(ck_ui_debugger::SummaryStyleTokens()); }
    else { DoBuildSummaryView(); }
}

auto SCkUIDebuggerWindow::DoUpdateAllSlots() -> void
{
    if (NOT _BoundLayout.IsValid())
    {
        _HasActiveLayout = false;
        _SummaryActiveTag = FText::GetEmpty();
        _SummaryInputMode = FText::GetEmpty();
        _SummaryLayerCount = FText::GetEmpty();
        DoPublishLayerNodes();
        return;
    }

    UCk_UI_PrimaryGameLayout_UE* Layout = _BoundLayout.Get();
    int32 LayerCount = 0;
    Layout->ForEachLayer([&LayerCount](UCk_UI_LayerStack_UE*, bool) { ++LayerCount; });
    _HasActiveLayout = true;
    _SummaryActiveTag = FText::FromString(Layout->Get_ActiveLayerTag().ToString());
    _SummaryInputMode = FText::FromString(ck_ui_debugger::InputModeToString(Layout->Get_EffectiveInputMode()));
    _SummaryLayerCount = FText::AsNumber(LayerCount);
    DoPublishLayerNodes();
}

// --------------------------------------------------------------------------------------------------------------------
// History List
// --------------------------------------------------------------------------------------------------------------------

auto SCkUIDebuggerWindow::DoBuildHistoryView() -> void
{
    if (!_HistoryHost.IsValid() || !_HistoryCollection.IsValid()) { return; }
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    if (NOT RegistryResult.Succeeded)
    {
        _HistoryHost->SetContent(SNew(STextBlock).Text(
            FText::FromString(FString::Join(RegistryResult.Errors, TEXT("\n")))));
        return;
    }
    auto Data = FCkUiView::FDataBindings{};
    const TWeakPtr<SCkUIDebuggerWindow> WeakWindow{SharedThis(this)};
    Data.Text.Add(TEXT("history-title"), FText::FromString(TEXT("Event History")));
    Data.Text.Add(TEXT("history-empty"), TAttribute<FText>::CreateLambda([WeakWindow]()
    {
        const auto Window = WeakWindow.Pin();
        return Window.IsValid() && Window->_HistoryCollection.IsValid() && Window->_HistoryCollection->GetRecords().IsEmpty()
            ? FText::FromString(TEXT("No events yet.")) : FText::GetEmpty();
    }));
    Data.Collections.Add(TEXT("history-records"), _HistoryCollection);
    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (!Plugin.IsValid())
    {
        _HistoryHost->SetContent(SNew(STextBlock).Text(
            FText::FromString(TEXT("CkDebugger resources are unavailable."))));
        return;
    }
    const TSharedRef<FCkUiView> View = FCkUiView::Create({}, {}, ck_ui_debugger::HistoryStyleTokens(),
        CkStyle::RegularFont(CkStyle::FontSizeBody()), MoveTemp(Data), Registry);
    const TSharedRef<SWidget> Main = View->GetRegion(TEXT("main"));
    const FString Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    View->SetFiles(FPaths::Combine(Directory, TEXT("UiDebuggerHistory.ui.html")),
        FPaths::Combine(Directory, TEXT("UiDebuggerHistory.ui.css")));
    View->PollFiles();
    if (NOT View->GetLastResult().Succeeded)
    {
        _HistoryHost->SetContent(SNew(STextBlock).Text(
            FText::FromString(FString::Join(View->GetLastResult().Errors, TEXT("\n")))));
        return;
    }
    _HistoryView = View;
    _HistoryHost->SetContent(Main);
}

auto SCkUIDebuggerWindow::DoPublishHistoryRecords() -> void
{
    if (!_HistoryCollection.IsValid()) { return; }
    auto Records = TArray<FCkUiRecordData>{};
    for (int32 Index = 0; Index < _HistoryEvents.Num(); ++Index)
    {
        auto Record = FCkUiRecordData{};
        Record.Key = FString::Printf(TEXT("history:%llu"),
            static_cast<unsigned long long>(_HistoryEvents[Index].Key));
        Record.Fields.Add(TEXT("description"), ck_ui_debugger::TextField(_HistoryEvents[Index].Description));
        Records.Add(MoveTemp(Record));
    }
    _HistoryCollection->TrySetRecords(MoveTemp(Records));
}

auto SCkUIDebuggerWindow::DoPollHistoryFiles(const double InCurrentTime) -> void
{
    constexpr double PollIntervalSeconds = 0.5;
    if (InCurrentTime < _NextHistoryPollSeconds) { return; }
    _NextHistoryPollSeconds = InCurrentTime + PollIntervalSeconds;
    if (_HistoryView.IsValid()) { _HistoryView->PollFiles(ck_ui_debugger::HistoryStyleTokens()); }
    else { DoBuildHistoryView(); }
}

auto SCkUIDebuggerWindow::DoAppendHistoryEvent(FString InDescription) -> void
{
    _HistoryEvents.Insert(FCkUIDebugger_HistoryEvent{
        _NextHistoryKey++, FPlatformTime::Seconds(), MoveTemp(InDescription)}, 0);
    if (_HistoryEvents.Num() > MaxHistoryEvents) { _HistoryEvents.SetNum(MaxHistoryEvents); }
    _IsDirty = true;
}

// --------------------------------------------------------------------------------------------------------------------
// Toolbar Actions
// --------------------------------------------------------------------------------------------------------------------

auto SCkUIDebuggerWindow::DoExpandAll() -> void
{
    TSharedPtr<SCkUiTree> Tree;
    if (_LayerView.IsValid()) { Tree = _LayerView->GetTree(TEXT("ui-layer-tree")); }
    if (!Tree.IsValid() || !_LayerCollection.IsValid()) { return; }
    for (const TSharedPtr<const FCkUiTreeNode>& Root : _LayerCollection->GetRoots())
    {
        if (Root.IsValid()) { Tree->TrySetExpanded(Root->GetKey(), true); }
    }
}

auto SCkUIDebuggerWindow::DoCollapseAll() -> void
{
    TSharedPtr<SCkUiTree> Tree;
    if (_LayerView.IsValid()) { Tree = _LayerView->GetTree(TEXT("ui-layer-tree")); }
    if (!Tree.IsValid() || !_LayerCollection.IsValid()) { return; }
    for (const TSharedPtr<const FCkUiTreeNode>& Root : _LayerCollection->GetRoots())
    {
        if (Root.IsValid()) { Tree->TrySetExpanded(Root->GetKey(), false); }
    }
}

auto SCkUIDebuggerWindow::DoCycleNameDepth(const int32 InDirection) -> void
{
    const int32 MaxDepth = FMath::Max(1, _MaxNameSegments);
    if (InDirection < 0)
    {
        _NameDepth = _NameDepth == 0 ? MaxDepth : _NameDepth - 1;
    }
    else if (InDirection > 0)
    {
        _NameDepth = _NameDepth >= MaxDepth ? 0 : _NameDepth + 1;
    }
    _IsDirty = true;
}

// --------------------------------------------------------------------------------------------------------------------
// Search
// --------------------------------------------------------------------------------------------------------------------

auto
    SCkUIDebuggerWindow::
    DoMatchesFilter(
        const FString& InLayerTag) const
    -> bool
{
    if (_SearchFilter.IsEmpty())
    { return true; }

    return ck::fuzzy::Match(_SearchFilter, InLayerTag, {}).Get_IsMatch();
}

auto
    SCkUIDebuggerWindow::
    DoShortName(
        const FString& InFullName)
    -> FString
{
    _MaxNameSegments = FMath::Max(_MaxNameSegments, SCkDebug_NameLabel::Get_SegmentCount(InFullName));
    return SCkDebug_NameLabel::Get_ShortName(InFullName, _NameDepth);
}

// --------------------------------------------------------------------------------------------------------------------
