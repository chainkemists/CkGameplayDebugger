#include "CkDialogDebugger/Window/SCkDialogDebuggerWindow.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Lifecycle/CkDebug_SessionLifecycle.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_PaneHost.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

// --------------------------------------------------------------------------------------------------------------------

const FName SCkDialogDebuggerWindow::WindowId = FName(TEXT("DialogDebugger"));

// --------------------------------------------------------------------------------------------------------------------

namespace ck_dialog_debugger_window
{
    auto StyleTokens() -> FCkUiView::FTokens
    {
        const auto Padding = [](const FMargin& InMargin) -> FString
        {
            return FString::Printf(TEXT("%gpx %gpx %gpx %gpx"),
                InMargin.Top, InMargin.Right, InMargin.Bottom, InMargin.Left);
        };
        const auto Color = [](const FLinearColor& InColor) -> FString
        {
            return TEXT("#") + InColor.ToFColorSRGB().ToHex();
        };
        return {
            {TEXT("--dialog-row-font-size"), FString::FromInt(ck::debug_axes::Get_ScaledFontSize(CkStyle::FontSizeSmall()))},
            {TEXT("--dialog-heading-padding"), Padding(ck::debug_axes::Apply_RowDensity(
                FMargin{CkStyle::SpaceM, CkStyle::SpaceM, 0.0f, CkStyle::SpaceXS}))},
            {TEXT("--dialog-row-padding"), Padding(ck::debug_axes::Apply_RowDensity(FMargin{CkStyle::SpaceXL, CkStyle::SpaceXS}))},
            {TEXT("--dialog-empty-padding"), Padding(ck::debug_axes::Apply_RowDensity(FMargin{CkStyle::SpaceM, CkStyle::SpaceS}))},
            {TEXT("--dialog-text"), Color(CkStyle::Text())},
            {TEXT("--dialog-text-strong"), Color(CkStyle::TextStrong())},
            {TEXT("--dialog-text-dim"), Color(CkStyle::TextDim())},
            {TEXT("--dialog-text-mute"), Color(CkStyle::TextMute())},
        };
    }

    auto TextField(const FString& InValue) -> FCkUiFieldValue
    {
        return FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(InValue)};
    }

    auto BoolField(const bool InValue) -> FCkUiFieldValue
    {
        return FCkUiFieldValue{.Kind = ECkUiFieldKind::Bool, .Bool = InValue};
    }

    auto NumberField(const float InValue) -> FCkUiFieldValue
    {
        return FCkUiFieldValue{.Kind = ECkUiFieldKind::Number, .Number = InValue};
    }

    auto ColorField(const FLinearColor& InValue) -> FCkUiFieldValue
    {
        return FCkUiFieldValue{.Kind = ECkUiFieldKind::Color, .Color = InValue};
    }

    auto CooldownSchema() -> TArray<FCkUiFieldSchema>
    {
        return {
            {TEXT("dialog-is-emitter"), ECkUiFieldKind::Bool},
            {TEXT("dialog-is-cooldown"), ECkUiFieldKind::Bool},
            {TEXT("dialog-heading"), ECkUiFieldKind::Text},
            {TEXT("dialog-tags"), ECkUiFieldKind::Text},
            {TEXT("dialog-line-id"), ECkUiFieldKind::Text},
            {TEXT("dialog-fraction"), ECkUiFieldKind::Number},
            {TEXT("dialog-meter-color"), ECkUiFieldKind::Color},
            {TEXT("dialog-remaining"), ECkUiFieldKind::Text},
            {TEXT("dialog-text-color"), ECkUiFieldKind::Color},
        };
    }

    auto MakeRecord(
        const FString& InKey,
        const bool InIsEmitter,
        const bool InIsCooldown,
        const FString& InHeading,
        const FString& InTags,
        const FString& InLineId,
        const float InFraction,
        const FLinearColor& InMeterColor,
        const FString& InRemaining,
        const FLinearColor& InTextColor) -> FCkUiRecordData
    {
        auto Record = FCkUiRecordData{};
        Record.Key = InKey;
        Record.Fields.Add(TEXT("dialog-is-emitter"), BoolField(InIsEmitter));
        Record.Fields.Add(TEXT("dialog-is-cooldown"), BoolField(InIsCooldown));
        Record.Fields.Add(TEXT("dialog-heading"), TextField(InHeading));
        Record.Fields.Add(TEXT("dialog-tags"), TextField(InTags));
        Record.Fields.Add(TEXT("dialog-line-id"), TextField(InLineId));
        Record.Fields.Add(TEXT("dialog-fraction"), NumberField(InFraction));
        Record.Fields.Add(TEXT("dialog-meter-color"), ColorField(InMeterColor));
        Record.Fields.Add(TEXT("dialog-remaining"), TextField(InRemaining));
        Record.Fields.Add(TEXT("dialog-text-color"), ColorField(InTextColor));
        return Record;
    }

    auto EmitterKey(const int64 InGeneration, const FCk_Handle_DialogEmitter& InEmitter) -> FString
    {
        const FCk_Entity& Entity = InEmitter.Get_Entity();
        return FString::Printf(TEXT("emitter:%lld:%d:%d"), InGeneration,
            static_cast<int32>(Entity.Get_VersionNumber()), static_cast<int32>(Entity.Get_EntityNumber()));
    }

    auto CooldownKey(const FString& InEmitterKey, const FName InLineId) -> FString
    {
        return FString::Printf(TEXT("cooldown:%s:%s"), *InEmitterKey, *InLineId.ToString());
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDialogDebuggerWindow::
    Construct(
        const FArguments& InArgs)
    -> void
{
    const FCkUiLoadResult CollectionResult = FCkUiCollection::TryCreate(
        ck_dialog_debugger_window::CooldownSchema(), _CooldownCollection);

    ChildSlot
    [
        SNew(SCkDebug_WindowChrome)
        .WindowId(WindowId)
        .ToolTabId(TEXT("CkDialogDebugger"))
        .CommandGroups({
            FCkDebug_CommandGroup::Primary(TEXT("CooldownView"), FText::FromString(TEXT("Cooldown view controls")),
                SAssignNew(_CooldownControlsHost, SBox)),
            FCkDebug_CommandGroup::Context(TEXT("RuntimeCommands"), FText::FromString(TEXT("Dialog runtime commands")),
                SAssignNew(_RuntimeCommandsHost, SBox)),
            FCkDebug_CommandGroup::Context(TEXT("DialogSearch"), FText::FromString(TEXT("Dialog search")),
                SAssignNew(_SearchHost, SBox))
        })
        .Content()
        [
            SNew(SCkDebug_PaneHost)
            [
                SAssignNew(_DialogHost, SBox)
            ]
        ]
    ];

    if (NOT CollectionResult.Succeeded)
    {
        _DialogHost->SetContent(SNew(STextBlock).Text(FText::FromString(
            FString::Join(CollectionResult.Errors, TEXT("\n")))));
    }
    else
    {
        DoBuild_DialogView();
    }

    _SessionInvalidatedHandle = ck::DebugSessionLifecycle::Get_OnSessionInvalidated().AddSP(
        this, &SCkDialogDebuggerWindow::HandleSessionInvalidated);
    _WorldInvalidatedHandle = ck::DebugSessionLifecycle::Get_OnWorldInvalidated().AddSP(
        this, &SCkDialogDebuggerWindow::HandleWorldInvalidated);

    Register_WithGate();
}

// --------------------------------------------------------------------------------------------------------------------

SCkDialogDebuggerWindow::~SCkDialogDebuggerWindow()
{
    if (_SessionInvalidatedHandle.IsValid())
    { ck::DebugSessionLifecycle::Get_OnSessionInvalidated().Remove(_SessionInvalidatedHandle); }

    if (_WorldInvalidatedHandle.IsValid())
    { ck::DebugSessionLifecycle::Get_OnWorldInvalidated().Remove(_WorldInvalidatedHandle); }

    // A destructor can run after the last tick; invalidate and release all four retained mounts synchronously.
    DoInvalidate_DialogView();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDialogDebuggerWindow::
    Tick(
        const FGeometry& InAllottedGeometry,
        const double InCurrentTime,
        const float InDeltaTime)
    -> void
{
    SCkDebugger_WindowBase::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

    UWorld* const World = DoGet_PieWorld();
    if (World != _ObservedWorld.Get())
    {
        // All prior callbacks become ineligible before we touch old-world projection data or detach its view.
        DoInvalidate_DialogView();
        _ObservedWorld = World;
        DoBuild_DialogView();
    }

    // File admission is bounded and deliberately precedes the refresh gate: source recovery must not depend on
    // collector cadence, but it never performs per-frame disk I/O.
    DoPoll_DialogFiles(InCurrentTime);

    if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(WindowId))
    { return; }

    _Collector.Collect(World);
    DoProject_Cooldowns();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDialogDebuggerWindow::
    OnStyleRevisionChanged()
    -> void
{
    // Apply through the ordinary atomic reload so searches keep their draft/focus and repeat items keep identity.
    // An invalid resource/token pair leaves the last accepted presentation intact.
    if (_DialogView.IsValid())
    { _DialogView->PollFiles(ck_dialog_debugger_window::StyleTokens()); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDialogDebuggerWindow::
    DoGet_PieWorld() const
    -> UWorld*
{
    if (ck::Is_NOT_Valid(GEngine))
    { return nullptr; }

    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        UWorld* const World = Context.World();
        if (ck::Is_NOT_Valid(World))
        { continue; }

        if ((Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
            && World->HasBegunPlay())
        { return World; }
    }

    return nullptr;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDialogDebuggerWindow::
    CanUse_DialogView(
        const int64 InGeneration) const
    -> bool
{
    return InGeneration == _DialogGeneration;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDialogDebuggerWindow::
    CanDispatch_Dialog(
        const int64 InGeneration) const
    -> bool
{
    return CanUse_DialogView(InGeneration)
        && _ObservedWorld.IsValid()
        && DoGet_PieWorld() == _ObservedWorld.Get();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDialogDebuggerWindow::
    DoExecCommand(
        const FString& InCommand)
    -> void
{
    UWorld* const World = DoGet_PieWorld();
    if (World == nullptr)
    { return; }

    if (APlayerController* const PlayerController = World->GetFirstPlayerController(); ck::IsValid(PlayerController))
    { PlayerController->ConsoleCommand(InCommand, true); }
    else if (ck::IsValid(GEngine))
    { GEngine->Exec(World, *InCommand, *GLog); }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDialogDebuggerWindow::
    DoPassesFilter(
        const FString& InText) const
    -> bool
{
    return _FilterString.IsEmpty() || InText.Contains(_FilterString, ESearchCase::IgnoreCase);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDialogDebuggerWindow::
    DoBuild_DiagnosticText() const
    -> FText
{
    if (NOT _ObservedWorld.IsValid())
    { return FText::FromString(TEXT("(no active PIE session — start Play In Editor to inspect the Dialog registry)")); }

    const FCkDialogDebugger_RegistrySnapshot& Snapshot = _Collector.Get_Snapshot();
    auto Out = FString::Printf(TEXT("DIALOG REGISTRY    ready=%s    %d lines    %d banks\n"),
        Snapshot.IsReady ? TEXT("YES") : TEXT("no"), Snapshot.Lines.Num(), Snapshot.NumBanks);
    Out += TEXT("--------------------------------------------------------------------\n\n");
    Out += FString::Printf(TEXT("LINES (%d)\n"), Snapshot.Lines.Num());

    for (const FCkDialogDebugger_LineInfo& Line : Snapshot.Lines)
    {
        if (NOT DoPassesFilter(Line.LineID.ToString()))
        { continue; }

        Out += FString::Printf(TEXT("  %-28s  enter=%s  exit=%s  conds=%d\n"),
            *Line.LineID.ToString(), *Line.EventTag.ToString(),
            Line.LinkedEventTag.IsValid() ? *Line.LinkedEventTag.ToString() : TEXT("(none)"), Line.NumConditions);
    }

    Out += FString::Printf(TEXT("\nEMITTERS (%d)\n"), Snapshot.Emitters.Num());
    for (const FCkDialogDebugger_EmitterInfo& Emitter : Snapshot.Emitters)
    {
        if (NOT DoPassesFilter(Emitter.DebugName) && NOT DoPassesFilter(Emitter.EmitterTags.ToStringSimple()))
        { continue; }

        Out += FString::Printf(TEXT("\n  %s\n    tags: %s\n"), *Emitter.DebugName,
            Emitter.EmitterTags.IsEmpty() ? TEXT("(global — no tags)") : *Emitter.EmitterTags.ToStringSimple());
        if (!Emitter.QueryHistory.IsEmpty())
        {
            const FCkDialogDebugger_QueryHistoryEntry& Last = Emitter.QueryHistory.Last();
            Out += FString::Printf(TEXT("    last query: %s  ->  %d pass / %d fail(line) / %d fail(cooldown)\n"),
                *Last.EventTag.ToString(), Last.NumPassed, Last.NumFailLine, Last.NumFailEmitter);
        }
    }

    return FText::FromString(Out);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDialogDebuggerWindow::
    DoProject_Cooldowns()
    -> void
{
    if (NOT _CooldownCollection.IsValid())
    { return; }

    const FCkDialogDebugger_RegistrySnapshot& Snapshot = _Collector.Get_Snapshot();
    auto NextRecords = TArray<FCkUiRecordData>{};
    int32 NextVisibleCooldownCount = 0;

    for (const FCkDialogDebugger_EmitterInfo& Emitter : Snapshot.Emitters)
    {
        if (!DoPassesFilter(Emitter.DebugName) && !DoPassesFilter(Emitter.EmitterTags.ToStringSimple()))
        { continue; }

        auto VisibleCooldowns = TArray<const FCkDialogDebugger_CooldownInfo*>{};
        for (const FCkDialogDebugger_CooldownInfo& Cooldown : Emitter.Cooldowns)
        {
            if (_ShowActiveCooldownsOnly && !Cooldown.IsForever && Cooldown.RemainingSeconds <= 0.0f)
            { continue; }
            VisibleCooldowns.Add(&Cooldown);
        }
        if (VisibleCooldowns.IsEmpty())
        { continue; }

        const FString EmitterKey = ck_dialog_debugger_window::EmitterKey(_DialogGeneration, Emitter.Handle);
        NextRecords.Add(ck_dialog_debugger_window::MakeRecord(EmitterKey, true, false,
            Emitter.DebugName,
            Emitter.EmitterTags.IsEmpty() ? TEXT("(global — no tags)") : Emitter.EmitterTags.ToStringSimple(),
            TEXT(""), 0.0f, CkStyle::TextMute(), TEXT(""), CkStyle::TextMute()));

        for (const FCkDialogDebugger_CooldownInfo* Cooldown : VisibleCooldowns)
        {
            const float Fraction = (Cooldown->IsForever || Cooldown->TotalSeconds <= 0.0f)
                ? 1.0f
                : FMath::Clamp(Cooldown->RemainingSeconds / Cooldown->TotalSeconds, 0.0f, 1.0f);
            const FLinearColor MeterColor = Cooldown->IsForever
                ? CkStyle::Err()
                : (Fraction > 0.25f ? CkStyle::Warn() : CkStyle::Ok());
            const FString Remaining = Cooldown->IsForever
                ? TEXT("forever")
                : FString::Printf(TEXT("%.2fs / %.2fs"), Cooldown->RemainingSeconds, Cooldown->TotalSeconds);

            NextRecords.Add(ck_dialog_debugger_window::MakeRecord(
                ck_dialog_debugger_window::CooldownKey(EmitterKey, Cooldown->LineID), false, true,
                TEXT(""), TEXT(""), Cooldown->LineID.ToString(), Fraction, MeterColor, Remaining, CkStyle::TextDim()));
            ++NextVisibleCooldownCount;
        }
    }

    // Complete projection admission is all-or-nothing. A malformed source record cannot partially publish rows.
    if (_CooldownCollection->TrySetRecords(MoveTemp(NextRecords)).Succeeded)
    { _VisibleCooldownCount = NextVisibleCooldownCount; }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDialogDebuggerWindow::
    DoInvalidate_DialogView()
    -> void
{
    ++_DialogGeneration;
    _Collector.Reset();
    _VisibleCooldownCount = 0;
    if (_CooldownCollection.IsValid())
    { _CooldownCollection->TrySetRecords({}); }
    _DialogView.Reset();
    for (const TSharedPtr<SBox>& Host : {_CooldownControlsHost, _RuntimeCommandsHost, _SearchHost, _DialogHost})
    {
        if (Host.IsValid())
        { Host->SetContent(SNullWidget::NullWidget); }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDialogDebuggerWindow::
    DoPoll_DialogFiles(
        const double InCurrentTime)
    -> void
{
    constexpr double PollIntervalSeconds = 0.5;
    if (InCurrentTime < _NextDialogPollSeconds)
    { return; }

    _NextDialogPollSeconds = InCurrentTime + PollIntervalSeconds;
    if (_DialogView.IsValid())
    {
        _DialogView->PollFiles(ck_dialog_debugger_window::StyleTokens());
        return;
    }

    DoBuild_DialogView();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDialogDebuggerWindow::
    DoBuild_DialogView()
    -> void
{
    if (NOT _CooldownControlsHost.IsValid() || NOT _RuntimeCommandsHost.IsValid() || NOT _SearchHost.IsValid()
        || NOT _DialogHost.IsValid() || NOT _CooldownCollection.IsValid() || _DialogView.IsValid())
    { return; }

    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    if (NOT RegistryResult.Succeeded)
    {
        _DialogHost->SetContent(SNew(STextBlock).Text(FText::FromString(FString::Join(RegistryResult.Errors, TEXT("\n")))));
        return;
    }

    const TWeakPtr<SCkDialogDebuggerWindow> WeakPanel{SharedThis(this)};
    const int64 Generation = _DialogGeneration;
    auto Data = FCkUiView::FDataBindings{};
    Data.SlateUserIndex = 0;
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakPanel, Generation]()
    {
        const TSharedPtr<SCkDialogDebuggerWindow> Panel = WeakPanel.Pin();
        return Panel.IsValid() && Panel->CanUse_DialogView(Generation);
    });
    Data.Visibility.Add(TEXT("dialog-available"), TAttribute<bool>::CreateLambda([WeakPanel, Generation]()
    {
        const TSharedPtr<SCkDialogDebuggerWindow> Panel = WeakPanel.Pin();
        return Panel.IsValid() && Panel->CanDispatch_Dialog(Generation);
    }));
    Data.Visibility.Add(TEXT("dialog-active-only"), TAttribute<bool>::CreateLambda([WeakPanel, Generation]()
    {
        const TSharedPtr<SCkDialogDebuggerWindow> Panel = WeakPanel.Pin();
        return Panel.IsValid() && Panel->CanUse_DialogView(Generation) && Panel->_ShowActiveCooldownsOnly;
    }));
    Data.Text.Add(TEXT("dialog-active-only-label"), FText::FromString(TEXT("Active cooldowns only")));
    Data.Text.Add(TEXT("dialog-count"), TAttribute<FText>::CreateLambda([WeakPanel, Generation]()
    {
        const TSharedPtr<SCkDialogDebuggerWindow> Panel = WeakPanel.Pin();
        const int32 Count = Panel.IsValid() && Panel->CanUse_DialogView(Generation) ? Panel->_VisibleCooldownCount : 0;
        return FText::FromString(FString::Printf(TEXT("COOLDOWNS (%d)"), Count));
    }));
    Data.Text.Add(TEXT("dialog-empty-state"), TAttribute<FText>::CreateLambda([WeakPanel, Generation]()
    {
        const TSharedPtr<SCkDialogDebuggerWindow> Panel = WeakPanel.Pin();
        return Panel.IsValid() && Panel->CanUse_DialogView(Generation) && Panel->_VisibleCooldownCount == 0
            ? FText::FromString(TEXT("(nothing cooling)"))
            : FText::GetEmpty();
    }));
    Data.Text.Add(TEXT("dialog-diagnostic"), TAttribute<FText>::CreateLambda([WeakPanel, Generation]()
    {
        const TSharedPtr<SCkDialogDebuggerWindow> Panel = WeakPanel.Pin();
        return Panel.IsValid() && Panel->CanUse_DialogView(Generation)
            ? Panel->DoBuild_DiagnosticText()
            : FText::GetEmpty();
    }));
    Data.Text.Add(TEXT("dialog-filter"), TAttribute<FText>::CreateLambda([WeakPanel, Generation]()
    {
        const TSharedPtr<SCkDialogDebuggerWindow> Panel = WeakPanel.Pin();
        return Panel.IsValid() && Panel->CanUse_DialogView(Generation)
            ? FText::FromString(Panel->_FilterString)
            : FText::GetEmpty();
    }));
    Data.Text.Add(TEXT("dialog-highlight"), TAttribute<FText>::CreateLambda([WeakPanel, Generation]()
    {
        const TSharedPtr<SCkDialogDebuggerWindow> Panel = WeakPanel.Pin();
        return Panel.IsValid() && Panel->CanUse_DialogView(Generation)
            ? FText::FromString(Panel->_HighlightString)
            : FText::GetEmpty();
    }));
    Data.TextChanged.Add(TEXT("dialog-filter"), FOnTextChanged::CreateLambda([WeakPanel, Generation](const FText& InValue)
    {
        const TSharedPtr<SCkDialogDebuggerWindow> Panel = WeakPanel.Pin();
        if (!Panel.IsValid() || !Panel->CanUse_DialogView(Generation))
        { return; }
        Panel->_FilterString = InValue.ToString();
        Panel->DoProject_Cooldowns();
    }));
    Data.TextChanged.Add(TEXT("dialog-highlight"), FOnTextChanged::CreateLambda([WeakPanel, Generation](const FText& InValue)
    {
        const TSharedPtr<SCkDialogDebuggerWindow> Panel = WeakPanel.Pin();
        if (Panel.IsValid() && Panel->CanUse_DialogView(Generation))
        { Panel->_HighlightString = InValue.ToString(); }
    }));
    Data.BoolChanged.Add(TEXT("dialog-active-only"), FCkUiOnBoolChanged::CreateLambda([WeakPanel, Generation](const bool InValue)
    {
        const TSharedPtr<SCkDialogDebuggerWindow> Panel = WeakPanel.Pin();
        if (!Panel.IsValid() || !Panel->CanUse_DialogView(Generation) || Panel->_ShowActiveCooldownsOnly == InValue)
        { return; }
        Panel->_ShowActiveCooldownsOnly = InValue;
        Panel->DoProject_Cooldowns();
    }));
    Data.Collections.Add(TEXT("dialog-cooldown-records"), _CooldownCollection);

    auto Actions = FCkUiView::FActions{};
    Actions.Add(TEXT("dialog-save"), FSimpleDelegate::CreateLambda([WeakPanel, Generation]()
    {
        const TSharedPtr<SCkDialogDebuggerWindow> Panel = WeakPanel.Pin();
        if (Panel.IsValid() && Panel->CanDispatch_Dialog(Generation))
        { Panel->DoExecCommand(TEXT("Ck_Save")); }
    }));
    Actions.Add(TEXT("dialog-load"), FSimpleDelegate::CreateLambda([WeakPanel, Generation]()
    {
        const TSharedPtr<SCkDialogDebuggerWindow> Panel = WeakPanel.Pin();
        if (Panel.IsValid() && Panel->CanDispatch_Dialog(Generation))
        { Panel->DoExecCommand(TEXT("Ck_Load")); }
    }));

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT Plugin.IsValid())
    {
        _DialogHost->SetContent(SNew(STextBlock).Text(FText::FromString(
            TEXT("CkDebugger resources are unavailable."))));
        return;
    }

    const TSharedRef<FCkUiView> View = FCkUiView::Create({}, MoveTemp(Actions), ck_dialog_debugger_window::StyleTokens(),
        CkStyle::RegularFont(CkStyle::FontSizeBody()), MoveTemp(Data), Registry);
    // All four persistent Chrome mounts must exist before loading so the document's complete region inventory is
    // admitted atomically. The SBoxes survive compatible reloads and source recovery.
    const TSharedRef<SWidget> CooldownControls = View->GetRegion(TEXT("cooldown-controls"));
    const TSharedRef<SWidget> RuntimeCommands = View->GetRegion(TEXT("runtime-commands"));
    const TSharedRef<SWidget> Search = View->GetRegion(TEXT("search"));
    const TSharedRef<SWidget> Main = View->GetRegion(TEXT("main"));
    const FString Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    View->SetFiles(FPaths::Combine(Directory, TEXT("DialogDebugger.ui.html")),
        FPaths::Combine(Directory, TEXT("DialogDebugger.ui.css")));
    View->PollFiles();
    if (NOT View->GetLastResult().Succeeded)
    {
        _DialogHost->SetContent(SNew(STextBlock).Text(FText::FromString(
            FString::Join(View->GetLastResult().Errors, TEXT("\n")))));
        return;
    }

    _DialogView = View;
    _CooldownControlsHost->SetContent(CooldownControls);
    _RuntimeCommandsHost->SetContent(RuntimeCommands);
    _SearchHost->SetContent(Search);
    _DialogHost->SetContent(Main);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDialogDebuggerWindow::
    HandleSessionInvalidated()
    -> void
{
    // The shared boundary fires before the registry disappears. Tick is too late to release collector handles.
    DoInvalidate_DialogView();
    _ObservedWorld = nullptr;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkDialogDebuggerWindow::
    HandleWorldInvalidated(
        UWorld* InWorld)
    -> void
{
    // A second live game world must not invalidate the observed world's registry-backed handles.
    if (ck::IsValid(InWorld) && InWorld != _ObservedWorld.Get())
    { return; }

    HandleSessionInvalidated();
}

// --------------------------------------------------------------------------------------------------------------------
