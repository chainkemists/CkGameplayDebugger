#include "CkAggroDebugger/Window/SCkAggroDebuggerWindow.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkDebuggerCommon/Lifecycle/CkDebug_SessionLifecycle.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Window/CkDebuggerRefreshGate.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_PaneHost.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

const FName SCkAggroDebuggerWindow::WindowId = FName(TEXT("AggroDebugger"));

namespace ck_aggro_debugger_window
{
    auto TextField(const FString& InValue) -> FCkUiFieldValue
    { return FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(InValue)}; }

    auto BoolField(const bool InValue) -> FCkUiFieldValue
    { return FCkUiFieldValue{.Kind = ECkUiFieldKind::Bool, .Bool = InValue}; }

    auto NumberField(const float InValue) -> FCkUiFieldValue
    { return FCkUiFieldValue{.Kind = ECkUiFieldKind::Number, .Number = InValue}; }

    auto ColorField(const FLinearColor& InValue) -> FCkUiFieldValue
    { return FCkUiFieldValue{.Kind = ECkUiFieldKind::Color, .Color = InValue}; }

    auto Schema() -> TArray<FCkUiFieldSchema>
    {
        return {
            {TEXT("aggro-is-owner"), ECkUiFieldKind::Bool},
            {TEXT("aggro-is-idle"), ECkUiFieldKind::Bool},
            {TEXT("aggro-is-target"), ECkUiFieldKind::Bool},
            {TEXT("aggro-owner-name"), ECkUiFieldKind::Text},
            {TEXT("aggro-owner-active"), ECkUiFieldKind::Text},
            {TEXT("aggro-owner-active-color"), ECkUiFieldKind::Color},
            {TEXT("aggro-owner-meta"), ECkUiFieldKind::Text},
            {TEXT("aggro-target-name"), ECkUiFieldKind::Text},
            {TEXT("aggro-target-name-color"), ECkUiFieldKind::Color},
            {TEXT("aggro-threat-fraction"), ECkUiFieldKind::Number},
            {TEXT("aggro-threat-color"), ECkUiFieldKind::Color},
            {TEXT("aggro-threat-text"), ECkUiFieldKind::Text},
            {TEXT("aggro-threat-text-color"), ECkUiFieldKind::Color},
            {TEXT("aggro-threat-tooltip"), ECkUiFieldKind::Text},
            {TEXT("aggro-score-fraction"), ECkUiFieldKind::Number},
            {TEXT("aggro-score-color"), ECkUiFieldKind::Color},
            {TEXT("aggro-score-text"), ECkUiFieldKind::Text},
            {TEXT("aggro-score-text-color"), ECkUiFieldKind::Color},
            {TEXT("aggro-score-tooltip"), ECkUiFieldKind::Text},
            {TEXT("aggro-state"), ECkUiFieldKind::Text},
            {TEXT("aggro-state-color"), ECkUiFieldKind::Color},
            {TEXT("aggro-detail"), ECkUiFieldKind::Text},
        };
    }

    auto HandleKey(const FCk_Handle& InHandle) -> FString
    {
        const FCk_Entity& Entity = InHandle.Get_Entity();
        return FString::Printf(TEXT("%d:%d"), static_cast<int32>(Entity.Get_VersionNumber()), static_cast<int32>(Entity.Get_EntityNumber()));
    }

    auto OwnerKey(const int64 InGeneration, const FCkAggroDebugger_OwnerInfo& InOwner) -> FString
    { return FString::Printf(TEXT("owner:%lld:%s"), InGeneration, *HandleKey(InOwner.OwnerEntity)); }

    auto TargetKey(const FString& InOwnerKey, const FCkAggroDebugger_TargetInfo& InTarget) -> FString
    { return FString::Printf(TEXT("target:%s:%s"), *InOwnerKey, *HandleKey(InTarget.TargetEntity)); }

    auto Fraction(const float InValue, const float InMax) -> float
    { return InMax <= KINDA_SMALL_NUMBER ? 0.0f : FMath::Clamp(InValue / InMax, 0.0f, 1.0f); }

    auto StyleTokens() -> FCkUiView::FTokens
    {
        const auto Padding = [](const FMargin& InMargin) -> FString
        {
            return FString::Printf(TEXT("%gpx %gpx %gpx %gpx"),
                InMargin.Top, InMargin.Right, InMargin.Bottom, InMargin.Left);
        };
        const auto Color = [](const FLinearColor& InColor) { return TEXT("#") + InColor.ToFColorSRGB().ToHex(); };
        return {
            {TEXT("--aggro-row-font-size"), FString::FromInt(ck::debug_axes::Get_ScaledFontSize(CkStyle::FontSizeSmall()))},
            {TEXT("--aggro-meta-font-size"), FString::FromInt(ck::debug_axes::Get_ScaledFontSize(CkStyle::FontSizeMicro()))},
            {TEXT("--aggro-owner-meta-padding"), Padding(ck::debug_axes::Apply_RowDensity(
                FMargin{CkStyle::SpaceS, 0.0f, 0.0f, CkStyle::SpaceXS}))},
            {TEXT("--aggro-idle-padding"), Padding(ck::debug_axes::Apply_RowDensity(FMargin{CkStyle::SpaceXL, CkStyle::SpaceXS}))},
            {TEXT("--aggro-target-padding"), Padding(ck::debug_axes::Apply_RowDensity(FMargin{CkStyle::SpaceXL, 1.0f}))},
            {TEXT("--aggro-empty-padding"), Padding(ck::debug_axes::Apply_RowDensity(FMargin{CkStyle::SpaceM, CkStyle::SpaceS}))},
            {TEXT("--aggro-text"), Color(CkStyle::Text())},
            {TEXT("--aggro-text-dim"), Color(CkStyle::TextDim())},
            {TEXT("--aggro-text-mute"), Color(CkStyle::TextMute())},
            {TEXT("--aggro-text-strong"), Color(CkStyle::TextStrong())},
        };
    }

    auto OwnerMeta(const FCkAggroDebugger_OwnerInfo& InOwner) -> FString
    {
        auto Result = FString::Printf(
            TEXT("switch bar %.2f (bias %.2fx · thresh %.2fx)  ·  min score %.2f  ·  held %.1fs / min %.1fs  ·  since switch %.1fs / cd %.1fs  ·  evals %lld"),
            InOwner.Get_SwitchBarScore(), InOwner.CurrentTargetBias, InOwner.SwitchThreshold, InOwner.MinimumTargetScore,
            InOwner.SecondsActiveTargetHeld, InOwner.MinimumAggroDuration, InOwner.SecondsSinceSwitch,
            InOwner.SwitchCooldown, InOwner.EvaluationCount);
        if (InOwner.MaxTrackedTargets > 0) { Result += FString::Printf(TEXT("  ·  cap %d/%d"), InOwner.Targets.Num(), InOwner.MaxTrackedTargets); }
        if (InOwner.IsDisabled) { Result += TEXT("  ·  DISABLED"); }
        if (InOwner.IsSelectionPending) { Result += TEXT("  ·  selection pending"); }
        return Result;
    }

    auto MakeRecord(const FString& InKey) -> FCkUiRecordData
    {
        auto Record = FCkUiRecordData{};
        Record.Key = InKey;
        Record.Fields.Add(TEXT("aggro-is-owner"), BoolField(false));
        Record.Fields.Add(TEXT("aggro-is-idle"), BoolField(false));
        Record.Fields.Add(TEXT("aggro-is-target"), BoolField(false));
        Record.Fields.Add(TEXT("aggro-owner-name"), TextField(FString{}));
        Record.Fields.Add(TEXT("aggro-owner-active"), TextField(FString{}));
        Record.Fields.Add(TEXT("aggro-owner-active-color"), ColorField(CkStyle::TextMute()));
        Record.Fields.Add(TEXT("aggro-owner-meta"), TextField(FString{}));
        Record.Fields.Add(TEXT("aggro-target-name"), TextField(FString{}));
        Record.Fields.Add(TEXT("aggro-target-name-color"), ColorField(CkStyle::Text()));
        Record.Fields.Add(TEXT("aggro-threat-fraction"), NumberField(0.0f));
        Record.Fields.Add(TEXT("aggro-threat-color"), ColorField(CkStyle::TextDim()));
        Record.Fields.Add(TEXT("aggro-threat-text"), TextField(FString{}));
        Record.Fields.Add(TEXT("aggro-threat-text-color"), ColorField(CkStyle::Text()));
        Record.Fields.Add(TEXT("aggro-threat-tooltip"), TextField(FString{}));
        Record.Fields.Add(TEXT("aggro-score-fraction"), NumberField(0.0f));
        Record.Fields.Add(TEXT("aggro-score-color"), ColorField(CkStyle::Accent()));
        Record.Fields.Add(TEXT("aggro-score-text"), TextField(FString{}));
        Record.Fields.Add(TEXT("aggro-score-text-color"), ColorField(CkStyle::TextDim()));
        Record.Fields.Add(TEXT("aggro-score-tooltip"), TextField(FString{}));
        Record.Fields.Add(TEXT("aggro-state"), TextField(FString{}));
        Record.Fields.Add(TEXT("aggro-state-color"), ColorField(CkStyle::TextMute()));
        Record.Fields.Add(TEXT("aggro-detail"), TextField(FString{}));
        return Record;
    }
}

auto SCkAggroDebuggerWindow::Construct(const FArguments&) -> void
{
    const FCkUiLoadResult CollectionResult = FCkUiCollection::TryCreate(ck_aggro_debugger_window::Schema(), _AggroCollection);
    ChildSlot
    [
        SNew(SCkDebug_WindowChrome)
        .WindowId(WindowId)
        .ToolTabId(TEXT("CkAggroDebugger"))
        .CommandGroups({
            FCkDebug_CommandGroup::Primary(TEXT("AggroView"), FText::FromString(TEXT("Aggro view controls")), SAssignNew(_ControlsHost, SBox)),
            FCkDebug_CommandGroup::Context(TEXT("AggroOverview"), FText::FromString(TEXT("Aggro overview")), SAssignNew(_OverviewHost, SBox)),
            FCkDebug_CommandGroup::Context(TEXT("AggroSearch"), FText::FromString(TEXT("Aggro search and status")), SAssignNew(_SearchHost, SBox))
        })
        .Content()[SNew(SCkDebug_PaneHost)[SAssignNew(_AggroHost, SBox)]]
    ];

    if (CollectionResult.Succeeded) { DoBuild_AggroView(); }
    else { _AggroHost->SetContent(SNew(STextBlock).Text(FText::FromString(FString::Join(CollectionResult.Errors, TEXT("\n"))))); }

    _SessionInvalidatedHandle = ck::DebugSessionLifecycle::Get_OnSessionInvalidated().AddSP(this, &SCkAggroDebuggerWindow::HandleSessionInvalidated);
    _WorldInvalidatedHandle = ck::DebugSessionLifecycle::Get_OnWorldInvalidated().AddSP(this, &SCkAggroDebuggerWindow::HandleWorldInvalidated);
    Register_WithGate();
}

SCkAggroDebuggerWindow::~SCkAggroDebuggerWindow()
{
    if (_SessionInvalidatedHandle.IsValid()) { ck::DebugSessionLifecycle::Get_OnSessionInvalidated().Remove(_SessionInvalidatedHandle); }
    if (_WorldInvalidatedHandle.IsValid()) { ck::DebugSessionLifecycle::Get_OnWorldInvalidated().Remove(_WorldInvalidatedHandle); }
    DoInvalidate_AggroView();
}

auto SCkAggroDebuggerWindow::Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime) -> void
{
    SCkDebugger_WindowBase::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    UWorld* const World = DoGet_PieWorld();
    if (World != _ObservedWorld.Get())
    {
        DoInvalidate_AggroView();
        _ObservedWorld = World;
        DoBuild_AggroView();
    }
    DoPoll_AggroFiles(InCurrentTime);
    if (NOT FCkDebuggerRefreshGate::Should_RefreshNow(WindowId)) { return; }
    _Collector.Collect(World);
    DoProject_Aggro();
}

auto SCkAggroDebuggerWindow::OnStyleRevisionChanged() -> void
{
    if (_AggroView.IsValid()) { _AggroView->PollFiles(ck_aggro_debugger_window::StyleTokens()); }
}

auto SCkAggroDebuggerWindow::DoGet_PieWorld() const -> UWorld*
{
    if (ck::Is_NOT_Valid(GEngine)) { return nullptr; }
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        UWorld* const World = Context.World();
        if (ck::IsValid(World) && (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game) && World->HasBegunPlay()) { return World; }
    }
    return nullptr;
}

auto SCkAggroDebuggerWindow::DoPassesFilter(const FString& InText) const -> bool
{ return _FilterString.IsEmpty() || InText.Contains(_FilterString, ESearchCase::IgnoreCase); }

auto SCkAggroDebuggerWindow::DoPassesFilter(const FCkAggroDebugger_OwnerInfo& InOwner) const -> bool
{
    if (_FilterString.IsEmpty() || DoPassesFilter(InOwner.OwnerName)) { return true; }
    return InOwner.Targets.ContainsByPredicate([this](const FCkAggroDebugger_TargetInfo& Target) { return DoPassesFilter(Target.TrackedName); });
}

auto SCkAggroDebuggerWindow::DoPassesEngagedFilter(const FCkAggroDebugger_OwnerInfo& InOwner) const -> bool
{ return NOT _ShowEngagedOwnersOnly || ck::IsValid(InOwner.ActiveTrackedEntity); }

auto SCkAggroDebuggerWindow::DoGet_ThreatColor(const FCkAggroDebugger_TargetInfo& InTarget) -> FLinearColor
{
    if (InTarget.IsPendingForget) { return CkStyle::TextMute(); }
    if (InTarget.IsActive) { return CkStyle::Err(); }
    if (InTarget.IsPerceived) { return CkStyle::Warn(); }
    return CkStyle::TextDim();
}

auto SCkAggroDebuggerWindow::DoBuild_StateText(const FCkAggroDebugger_TargetInfo& InTarget) -> FString
{
    auto State = FString{};
    if (InTarget.IsActive) { State += TEXT("ACTIVE "); }
    if (InTarget.IsPerceived) { State += TEXT("SEEN "); }
    if (InTarget.IsWithinRetention) { State += TEXT("RANGE "); }
    if (InTarget.IsPendingForget) { State += TEXT("FORGET "); }
    if (InTarget.CannotBecomeActive) { State += TEXT("NOACTIVE "); }
    if (InTarget.CannotBeForgotten) { State += TEXT("PINNED "); }
    return State.IsEmpty() ? FString(TEXT("--")) : State.TrimEnd();
}

auto SCkAggroDebuggerWindow::DoBuild_DetailText(const FCkAggroDebugger_TargetInfo& InTarget) -> FString
{
    const float Rate = InTarget.Get_EffectiveDecayRate();
    FString Forget = TEXT("stable");
    if (Rate > KINDA_SMALL_NUMBER)
    {
        const float Seconds = InTarget.Get_SecondsToForget();
        Forget = Seconds <= 0.0f ? TEXT("forgetting") : FString::Printf(TEXT("~%.1fs left"), Seconds);
    }
    return FString::Printf(TEXT("d=%.0f/%.0f  -%.2f/s  %s  seen %.1fs ago"), InTarget.Distance, InTarget.RetentionDistance,
        Rate, *Forget, InTarget.SecondsSincePerceived);
}

auto SCkAggroDebuggerWindow::DoProject_Aggro() -> void
{
    if (!_AggroCollection.IsValid()) { return; }
    const FCkAggroDebugger_Snapshot& Snapshot = _Collector.Get_Snapshot();
    auto Records = TArray<FCkUiRecordData>{};
    for (const FCkAggroDebugger_OwnerInfo& Owner : Snapshot.Owners)
    {
        if (!DoPassesFilter(Owner) || !DoPassesEngagedFilter(Owner)) { continue; }
        const FString OwnerId = ck_aggro_debugger_window::OwnerKey(_AggroGeneration, Owner);
        auto OwnerRecord = ck_aggro_debugger_window::MakeRecord(OwnerId);
        OwnerRecord.Fields[TEXT("aggro-is-owner")].Bool = true;
        OwnerRecord.Fields[TEXT("aggro-owner-name")].Text = FText::FromString(Owner.OwnerName);
        OwnerRecord.Fields[TEXT("aggro-owner-active")].Text = FText::FromString(ck::IsValid(Owner.ActiveTrackedEntity)
            ? FString::Printf(TEXT("▶ %s  (%.1fs)"), *Owner.ActiveTrackedName, Owner.SecondsActiveTargetHeld) : TEXT("idle"));
        OwnerRecord.Fields[TEXT("aggro-owner-active-color")].Color = ck::IsValid(Owner.ActiveTrackedEntity) ? CkStyle::Err() : CkStyle::TextMute();
        OwnerRecord.Fields[TEXT("aggro-owner-meta")].Text = FText::FromString(ck_aggro_debugger_window::OwnerMeta(Owner));
        Records.Add(MoveTemp(OwnerRecord));
        if (Owner.Targets.IsEmpty())
        {
            auto IdleRecord = ck_aggro_debugger_window::MakeRecord(OwnerId + TEXT(":idle"));
            IdleRecord.Fields[TEXT("aggro-is-idle")].Bool = true;
            IdleRecord.Fields[TEXT("aggro-detail")].Text = FText::FromString(TEXT("(no tracked targets — idle)"));
            Records.Add(MoveTemp(IdleRecord));
            continue;
        }
        const float MaxThreat = Owner.Get_MaxThreat();
        const float MaxScore = Owner.Get_MaxScore();
        for (const FCkAggroDebugger_TargetInfo& Target : Owner.Targets)
        {
            auto TargetRecord = ck_aggro_debugger_window::MakeRecord(ck_aggro_debugger_window::TargetKey(OwnerId, Target));
            const FLinearColor ThreatColor = DoGet_ThreatColor(Target);
            TargetRecord.Fields[TEXT("aggro-is-target")].Bool = true;
            TargetRecord.Fields[TEXT("aggro-target-name")].Text = FText::FromString(Target.TrackedName);
            TargetRecord.Fields[TEXT("aggro-target-name-color")].Color = Target.IsActive ? CkStyle::TextStrong() : CkStyle::Text();
            TargetRecord.Fields[TEXT("aggro-threat-fraction")].Number = ck_aggro_debugger_window::Fraction(Target.Threat, MaxThreat);
            TargetRecord.Fields[TEXT("aggro-threat-color")].Color = ThreatColor;
            TargetRecord.Fields[TEXT("aggro-threat-text")].Text = FText::FromString(FString::Printf(TEXT("%.1f / %.1f"), Target.Threat, Target.MinimumTrackedThreat));
            TargetRecord.Fields[TEXT("aggro-threat-tooltip")].Text = FText::FromString(TEXT("Threat, relative to this owner's strongest target"));
            TargetRecord.Fields[TEXT("aggro-score-fraction")].Number = ck_aggro_debugger_window::Fraction(Target.Score, MaxScore);
            TargetRecord.Fields[TEXT("aggro-score-color")].Color = CkStyle::Accent();
            TargetRecord.Fields[TEXT("aggro-score-text")].Text = FText::FromString(FString::Printf(TEXT("s %.2f"), Target.Score));
            TargetRecord.Fields[TEXT("aggro-score-tooltip")].Text = FText::FromString(TEXT("Selection score — the quantity the argmax actually compares"));
            TargetRecord.Fields[TEXT("aggro-state")].Text = FText::FromString(DoBuild_StateText(Target));
            TargetRecord.Fields[TEXT("aggro-state-color")].Color = ThreatColor;
            TargetRecord.Fields[TEXT("aggro-detail")].Text = FText::FromString(DoBuild_DetailText(Target));
            Records.Add(MoveTemp(TargetRecord));
        }
    }
    _AggroCollection->TrySetRecords(MoveTemp(Records));
}

auto SCkAggroDebuggerWindow::CanUse_AggroView(const int64 InGeneration) const -> bool
{ return InGeneration == _AggroGeneration; }

auto SCkAggroDebuggerWindow::DoInvalidate_AggroView() -> void
{
    ++_AggroGeneration;
    _Collector.Reset();
    if (_AggroCollection.IsValid()) { _AggroCollection->TrySetRecords({}); }
    _AggroView.Reset();
    for (const TSharedPtr<SBox>& Host : {_ControlsHost, _OverviewHost, _SearchHost, _AggroHost})
    { if (Host.IsValid()) { Host->SetContent(SNullWidget::NullWidget); } }
}

auto SCkAggroDebuggerWindow::DoPoll_AggroFiles(const double InCurrentTime) -> void
{
    constexpr double PollIntervalSeconds = 0.5;
    if (InCurrentTime < _NextAggroPollSeconds) { return; }
    _NextAggroPollSeconds = InCurrentTime + PollIntervalSeconds;
    if (_AggroView.IsValid()) { _AggroView->PollFiles(ck_aggro_debugger_window::StyleTokens()); }
    else { DoBuild_AggroView(); }
}

auto SCkAggroDebuggerWindow::DoBuild_AggroView() -> void
{
    if (!_ControlsHost.IsValid() || !_OverviewHost.IsValid() || !_SearchHost.IsValid() || !_AggroHost.IsValid()
        || !_AggroCollection.IsValid() || _AggroView.IsValid()) { return; }
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    if (!RegistryResult.Succeeded)
    {
        _AggroHost->SetContent(SNew(STextBlock).Text(FText::FromString(FString::Join(RegistryResult.Errors, TEXT("\n")))));
        return;
    }
    const TWeakPtr<SCkAggroDebuggerWindow> WeakWindow{SharedThis(this)};
    const int64 Generation = _AggroGeneration;
    auto Data = FCkUiView::FDataBindings{};
    Data.SlateUserIndex = 0;
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakWindow, Generation]()
    { const auto Window = WeakWindow.Pin(); return Window.IsValid() && Window->CanUse_AggroView(Generation); });
    Data.Visibility.Add(TEXT("aggro-engaged-only"), TAttribute<bool>::CreateLambda([WeakWindow, Generation]()
    { const auto Window = WeakWindow.Pin(); return Window.IsValid() && Window->CanUse_AggroView(Generation) && Window->_ShowEngagedOwnersOnly; }));
    Data.Text.Add(TEXT("aggro-engaged-only-label"), FText::FromString(TEXT("Engaged owners only")));
    Data.Text.Add(TEXT("aggro-filter"), TAttribute<FText>::CreateLambda([WeakWindow, Generation]()
    { const auto Window = WeakWindow.Pin(); return Window.IsValid() && Window->CanUse_AggroView(Generation) ? FText::FromString(Window->_FilterString) : FText::GetEmpty(); }));
    Data.Text.Add(TEXT("aggro-highlight"), TAttribute<FText>::CreateLambda([WeakWindow, Generation]()
    { const auto Window = WeakWindow.Pin(); return Window.IsValid() && Window->CanUse_AggroView(Generation) ? FText::FromString(Window->_HighlightString) : FText::GetEmpty(); }));
    Data.Text.Add(TEXT("aggro-status"), TAttribute<FText>::CreateLambda([WeakWindow, Generation]()
    {
        const auto Window = WeakWindow.Pin();
        if (!Window.IsValid() || !Window->CanUse_AggroView(Generation)) { return FText::GetEmpty(); }
        const auto& Snapshot = Window->_Collector.Get_Snapshot();
        if (!Snapshot.HasWorld) { return FText::FromString(TEXT("(no active PIE session — start Play In Editor to inspect the threat model)")); }
        if (Snapshot.NumOwners == 0) { return FText::FromString(TEXT("(no Aggro owners — note Aggro is authority-only, so a client PIE window shows none)")); }
        return FText::FromString(FString::Printf(TEXT("%d owner(s) · %d engaged · %d tracked target(s)"), Snapshot.NumOwners, Snapshot.NumEngaged, Snapshot.NumTargets));
    }));
    Data.Text.Add(TEXT("aggro-owners-count"), TAttribute<FText>::CreateLambda([WeakWindow, Generation]()
    { const auto Window = WeakWindow.Pin(); return FText::AsNumber(Window.IsValid() && Window->CanUse_AggroView(Generation) ? Window->_Collector.Get_Snapshot().NumOwners : 0); }));
    Data.Text.Add(TEXT("aggro-engaged-count"), TAttribute<FText>::CreateLambda([WeakWindow, Generation]()
    { const auto Window = WeakWindow.Pin(); return FText::AsNumber(Window.IsValid() && Window->CanUse_AggroView(Generation) ? Window->_Collector.Get_Snapshot().NumEngaged : 0); }));
    Data.Text.Add(TEXT("aggro-targets-count"), TAttribute<FText>::CreateLambda([WeakWindow, Generation]()
    { const auto Window = WeakWindow.Pin(); return FText::AsNumber(Window.IsValid() && Window->CanUse_AggroView(Generation) ? Window->_Collector.Get_Snapshot().NumTargets : 0); }));
    Data.Text.Add(TEXT("aggro-empty"), TAttribute<FText>::CreateLambda([WeakWindow, Generation]()
    { const auto Window = WeakWindow.Pin(); return Window.IsValid() && Window->CanUse_AggroView(Generation) && Window->_AggroCollection.IsValid() && Window->_AggroCollection->GetRecords().IsEmpty() ? FText::FromString(TEXT("(no Aggro owners in this world)")) : FText::GetEmpty(); }));
    Data.TextChanged.Add(TEXT("aggro-filter"), FOnTextChanged::CreateLambda([WeakWindow, Generation](const FText& Value)
    { if (const auto Window = WeakWindow.Pin(); Window.IsValid() && Window->CanUse_AggroView(Generation)) { Window->_FilterString = Value.ToString(); Window->DoProject_Aggro(); } }));
    Data.TextChanged.Add(TEXT("aggro-highlight"), FOnTextChanged::CreateLambda([WeakWindow, Generation](const FText& Value)
    { if (const auto Window = WeakWindow.Pin(); Window.IsValid() && Window->CanUse_AggroView(Generation)) { Window->_HighlightString = Value.ToString(); } }));
    Data.BoolChanged.Add(TEXT("aggro-engaged-only"), FCkUiOnBoolChanged::CreateLambda([WeakWindow, Generation](const bool Value)
    { if (const auto Window = WeakWindow.Pin(); Window.IsValid() && Window->CanUse_AggroView(Generation) && Window->_ShowEngagedOwnersOnly != Value) { Window->_ShowEngagedOwnersOnly = Value; Window->DoProject_Aggro(); } }));
    Data.Collections.Add(TEXT("aggro-records"), _AggroCollection);

    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (!Plugin.IsValid()) { _AggroHost->SetContent(SNew(STextBlock).Text(FText::FromString(TEXT("CkDebugger resources are unavailable.")))); return; }
    const TSharedRef<FCkUiView> View = FCkUiView::Create({}, {}, ck_aggro_debugger_window::StyleTokens(), CkStyle::RegularFont(CkStyle::FontSizeBody()), MoveTemp(Data), Registry);
    const TSharedRef<SWidget> Controls = View->GetRegion(TEXT("controls"));
    const TSharedRef<SWidget> Overview = View->GetRegion(TEXT("overview"));
    const TSharedRef<SWidget> Search = View->GetRegion(TEXT("search"));
    const TSharedRef<SWidget> Main = View->GetRegion(TEXT("main"));
    const FString Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    View->SetFiles(FPaths::Combine(Directory, TEXT("AggroDebugger.ui.html")), FPaths::Combine(Directory, TEXT("AggroDebugger.ui.css")));
    View->PollFiles();
    if (!View->GetLastResult().Succeeded)
    {
        _AggroHost->SetContent(SNew(STextBlock).Text(FText::FromString(FString::Join(View->GetLastResult().Errors, TEXT("\n")))));
        return;
    }
    _AggroView = View;
    _ControlsHost->SetContent(Controls);
    _OverviewHost->SetContent(Overview);
    _SearchHost->SetContent(Search);
    _AggroHost->SetContent(Main);
}

auto SCkAggroDebuggerWindow::HandleSessionInvalidated() -> void
{
    DoInvalidate_AggroView();
    _ObservedWorld = nullptr;
}

auto SCkAggroDebuggerWindow::HandleWorldInvalidated(UWorld* InWorld) -> void
{
    if (ck::IsValid(InWorld) && InWorld != _ObservedWorld.Get()) { return; }
    HandleSessionInvalidated();
}
