#include "CkInspector_Probes.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkSpatialQuery/Probe/CkProbe_Fragment.h"
#include "CkSpatialQuery/Probe/CkProbe_Utils.h"

#include "CkDebuggerCommon/Navigation/CkDebug_Navigator.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Utils/CkDebug_RequestGate.h"
#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Probes)

struct FCkInspector_ProbeDrawState
{
    TArray<FCk_Handle> OwnedProbes;
    bool DesiredEnabled = true;
    bool Active = true;
};

namespace ck_inspector_probes
{
    auto IsDestroying(const FCk_Handle& InEntity) -> bool
    {
        return ck::Is_NOT_Valid(InEntity)
            || InEntity.Has_Any<ck::FTag_DestroyEntity_Initiate, ck::FTag_DestroyEntity_EndPlay,
                ck::FTag_DestroyEntity_Teardown, ck::FTag_DestroyEntity_Await,
                ck::FTag_DestroyEntity_Finalize>();
    }

    auto TryGetProbe(const FCk_Handle& InEntity, FCk_Handle_Probe& OutProbe) -> bool
    {
        OutProbe = {};
        if (IsDestroying(InEntity) || NOT UCk_Utils_Probe_UE::Has(InEntity))
        { return false; }
        auto MutableEntity = InEntity;
        OutProbe = UCk_Utils_Probe_UE::Cast(MutableEntity);
        return ck::IsValid(OutProbe);
    }

    auto GetHandleKey(const FCk_Handle& InHandle) -> FString
    {
        return ck::IsValid(InHandle) ? ck::Format_UE(TEXT("{}"), InHandle.Get_Entity()) : FString{};
    }

    auto TextField(const FString& InText) -> FCkUiFieldValue
    {
        return FCkUiFieldValue{.Kind = ECkUiFieldKind::Text, .Text = FText::FromString(InText)};
    }

    auto DiffColor(const bool bInMarked) -> FLinearColor
    {
        return bInMarked ? CkStyle::Accent() : CkStyle::Text();
    }

    auto GetRequestGate(const FCk_Handle& InEntity) -> FCk_DebugRequest_GateVerdict
    {
        auto Probe = FCk_Handle_Probe{};
        if (NOT TryGetProbe(InEntity, Probe))
        { return {false, FText::FromString(TEXT("Probe is unavailable."))}; }
        return ck::DebugRequestGate::Evaluate(InEntity, ECk_DebugRequest_Requirement::CosmeticOnly);
    }

    auto DisableOwnedProbe(FCkInspector_ProbeDrawState& InState, const FCk_Handle& InEntity) -> void
    {
        const int32 Index = InState.OwnedProbes.IndexOfByKey(InEntity);
        if (Index == INDEX_NONE)
        { return; }
        auto Probe = FCk_Handle_Probe{};
        if (TryGetProbe(InEntity, Probe) && Probe.Has<ck::FTag_Probe_DebugDraw>())
        { UCk_Utils_Probe_UE::Request_EnableDisableDebugDraw(Probe, ECk_EnableDisable::Disable, {}); }
        InState.OwnedProbes.RemoveAt(Index);
    }

    auto DisableAllOwned(FCkInspector_ProbeDrawState& InState) -> void
    {
        const auto Owned = InState.OwnedProbes;
        for (const FCk_Handle& Entity : Owned)
        { DisableOwnedProbe(InState, Entity); }
        InState.OwnedProbes.Reset();
    }

    auto ApplyDesired(FCkInspector_ProbeDrawState& InState, const FCk_Handle& InEntity) -> void
    {
        auto Probe = FCk_Handle_Probe{};
        if (NOT InState.Active || NOT TryGetProbe(InEntity, Probe))
        { return; }
        if (NOT InState.DesiredEnabled)
        {
            DisableOwnedProbe(InState, InEntity);
            return;
        }
        const auto PreviouslyOwned = InState.OwnedProbes;
        for (const FCk_Handle& Other : PreviouslyOwned)
        { if (Other != InEntity) { DisableOwnedProbe(InState, Other); } }
        if (Probe.Has<ck::FTag_Probe_DebugDraw>())
        { return; }
        InState.OwnedProbes.RemoveSingle(InEntity);
        UCk_Utils_Probe_UE::Request_EnableDisableDebugDraw(Probe, ECk_EnableDisable::Enable, {});
        if (Probe.Has<ck::FTag_Probe_DebugDraw>())
        { InState.OwnedProbes.AddUnique(InEntity); }
    }
}

auto SCkInspector_ProbesAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _DiffLabels = InArgs._DiffLabels;
    _DrawState = InArgs._DrawState;
    const TSharedRef<SBox> Host = SNew(SBox);
    ChildSlot[Host];
    if (_DrawState.IsValid() && Refresh_Overlaps() && Build_AuthoredView())
    {
        Host->SetContent(_View->GetRegion(TEXT("main")));
        return;
    }
    if (NOT _DrawState.IsValid())
    { _LoadError = TEXT("Probe debug-draw ownership state is unavailable."); }
    Release();
    Host->SetContent(SNullWidget::NullWidget);
}

SCkInspector_ProbesAuthored::~SCkInspector_ProbesAuthored()
{
    Release();
}

auto SCkInspector_ProbesAuthored::Get_IsAvailable() const -> bool
{
    auto Probe = FCk_Handle_Probe{};
    return _Active && ck_inspector_probes::TryGetProbe(_Entity, Probe);
}

auto SCkInspector_ProbesAuthored::Get_NameText() const -> FString
{
    auto Probe = FCk_Handle_Probe{};
    if (NOT _Active || NOT ck_inspector_probes::TryGetProbe(_Entity, Probe))
    { return TEXT("--"); }
    const FGameplayTag Name = UCk_Utils_Probe_UE::Get_Name(Probe);
    return Name.IsValid() ? Name.GetTagName().ToString() : TEXT("Unnamed");
}

auto SCkInspector_ProbesAuthored::Get_StateText() const -> FString
{
    auto Probe = FCk_Handle_Probe{};
    if (NOT _Active || NOT ck_inspector_probes::TryGetProbe(_Entity, Probe))
    { return TEXT("--"); }
    return UCk_Utils_Probe_UE::Get_IsEnabledDisabled(Probe) == ECk_EnableDisable::Enable
        ? TEXT("Enabled") : TEXT("Disabled");
}

auto SCkInspector_ProbesAuthored::Get_StateForeground() const -> FLinearColor
{
    return _Active && Get_StateText() == TEXT("Enabled")
        ? CkStyle::GetToneColor(ECk_Tone::Ok) : CkStyle::GetToneColor(ECk_Tone::Neutral);
}

auto SCkInspector_ProbesAuthored::Get_StateBackground() const -> FLinearColor
{
    return _Active && Get_StateText() == TEXT("Enabled")
        ? CkStyle::GetToneDimColor(ECk_Tone::Ok) : CkStyle::GetToneDimColor(ECk_Tone::Neutral);
}

auto SCkInspector_ProbesAuthored::Get_DebugDrawEnabled() const -> bool
{
    return _Active && _DrawState.IsValid() && _DrawState->Active && _DrawState->DesiredEnabled;
}

auto SCkInspector_ProbesAuthored::Get_CanToggleDebugDraw() const -> bool
{
    return _Active && _DrawState.IsValid() && _DrawState->Active
        && ck_inspector_probes::GetRequestGate(_Entity).IsEnabled;
}

auto SCkInspector_ProbesAuthored::Get_DebugDrawDisabledReason() const -> FString
{
    if (NOT _Active)
    { return {}; }
    const FCk_DebugRequest_GateVerdict Gate = ck_inspector_probes::GetRequestGate(_Entity);
    return Gate.IsEnabled
        ? TEXT("Enable or disable debug rendering for the inspected probe.")
        : Gate.Reason.ToString();
}

auto SCkInspector_ProbesAuthored::Get_ResponseText() const -> FString
{
    auto Probe = FCk_Handle_Probe{};
    if (NOT _Active || NOT ck_inspector_probes::TryGetProbe(_Entity, Probe))
    { return TEXT("--"); }
    return UCk_Utils_Probe_UE::Get_ResponsePolicy(Probe) == ECk_ProbeResponse_Policy::Notify
        ? TEXT("Notify") : TEXT("Silent");
}

auto SCkInspector_ProbesAuthored::Get_MotionText() const -> FString
{
    auto Probe = FCk_Handle_Probe{};
    if (NOT _Active || NOT ck_inspector_probes::TryGetProbe(_Entity, Probe))
    { return TEXT("--"); }
    switch (UCk_Utils_Probe_UE::Get_MotionType(Probe))
    {
    case ECk_MotionType::Static: return TEXT("Static");
    case ECk_MotionType::Kinematic: return TEXT("Kinematic");
    case ECk_MotionType::Dynamic: return TEXT("Dynamic");
    default: return TEXT("Unknown");
    }
}

auto SCkInspector_ProbesAuthored::Get_QualityText() const -> FString
{
    auto Probe = FCk_Handle_Probe{};
    if (NOT _Active || NOT ck_inspector_probes::TryGetProbe(_Entity, Probe))
    { return TEXT("--"); }
    return UCk_Utils_Probe_UE::Get_MotionQuality(Probe) == ECk_MotionQuality::Discrete
        ? TEXT("Discrete") : TEXT("LinearCast (CCD)");
}

auto SCkInspector_ProbesAuthored::Get_FilterText() const -> FString
{
    auto Probe = FCk_Handle_Probe{};
    if (NOT _Active || NOT ck_inspector_probes::TryGetProbe(_Entity, Probe))
    { return TEXT("--"); }
    const FGameplayTagContainer Filter = UCk_Utils_Probe_UE::Get_Filter(Probe);
    return Filter.IsEmpty() ? TEXT("(Empty)") : Filter.ToString();
}

auto SCkInspector_ProbesAuthored::Refresh_Overlaps() -> bool
{
    if (NOT _Overlaps.IsValid())
    {
        const FCkUiLoadResult Result = FCkUiCollection::TryCreate({
            {TEXT("overlap-id"), ECkUiFieldKind::Text},
            {TEXT("overlap-name"), ECkUiFieldKind::Text}}, _Overlaps);
        if (NOT Result.Succeeded || NOT _Overlaps.IsValid())
        {
            _LoadError = FString::Join(Result.Errors, TEXT("\n"));
            return false;
        }
    }
    auto Records = TArray<FCkUiRecordData>{};
    auto ByKey = TMap<FString, FCk_Handle>{};
    auto Snapshot = TArray<FString>{};
    auto Probe = FCk_Handle_Probe{};
    if (_Active && ck_inspector_probes::TryGetProbe(_Entity, Probe))
    {
        for (const FCk_Probe_OverlapInfo& Info : UCk_Utils_Probe_UE::Get_CurrentOverlaps(Probe))
        {
            const FCk_Handle Other = Info.Get_OtherEntity();
            const FString Key = ck_inspector_probes::GetHandleKey(Other);
            if (Key.IsEmpty() || ByKey.Contains(Key))
            { continue; }
            const FString Name = UCk_Utils_Handle_UE::Get_DebugName(Other).ToString();
            auto Record = FCkUiRecordData{};
            Record.Key = Key;
            Record.Fields.Add(TEXT("overlap-id"), ck_inspector_probes::TextField(
                ck::Format_UE(TEXT("{}"), Other.Get_Entity())));
            Record.Fields.Add(TEXT("overlap-name"), ck_inspector_probes::TextField(Name));
            Records.Add(MoveTemp(Record));
            ByKey.Add(Key, Other);
            Snapshot.Add(Key + TEXT("|") + Name);
        }
    }
    Snapshot.Sort();
    if (Snapshot == _OverlapSnapshot)
    {
        _OverlapsByKey = MoveTemp(ByKey);
        return true;
    }
    Records.Sort([](const FCkUiRecordData& A, const FCkUiRecordData& B) { return A.Key < B.Key; });
    const FCkUiLoadResult Result = _Overlaps->TrySetRecords(MoveTemp(Records));
    if (NOT Result.Succeeded)
    {
        _LoadError = FString::Join(Result.Errors, TEXT("\n"));
        return false;
    }
    _OverlapsByKey = MoveTemp(ByKey);
    _OverlapSnapshot = MoveTemp(Snapshot);
    return true;
}

auto SCkInspector_ProbesAuthored::Set_DebugDrawEnabled(const bool bInEnabled) -> void
{
    if (NOT Get_CanToggleDebugDraw() || NOT _DrawState.IsValid())
    { return; }
    _DrawState->DesiredEnabled = bInEnabled;
    ck_inspector_probes::ApplyDesired(*_DrawState, _Entity);
}

auto SCkInspector_ProbesAuthored::Navigate_Overlap(const FString& InStableKey) -> void
{
    if (NOT Get_IsAvailable())
    { return; }
    const FCk_Handle* Expected = _OverlapsByKey.Find(InStableKey);
    if (Expected == nullptr || ck::Is_NOT_Valid(*Expected))
    { return; }
    auto Probe = FCk_Handle_Probe{};
    if (NOT ck_inspector_probes::TryGetProbe(_Entity, Probe))
    { return; }
    for (const FCk_Probe_OverlapInfo& Info : UCk_Utils_Probe_UE::Get_CurrentOverlaps(Probe))
    {
        const FCk_Handle Current = Info.Get_OtherEntity();
        if (Current == *Expected && ck_inspector_probes::GetHandleKey(Current) == InStableKey)
        {
            ck::DebugNav::Goto_Entity(Current);
            return;
        }
    }
}

auto SCkInspector_ProbesAuthored::Build_AuthoredView() -> bool
{
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT RegistryResult.Succeeded || NOT Registry.IsValid() || NOT Plugin.IsValid())
    {
        auto Errors = RegistryResult.Errors;
        if (NOT Registry.IsValid()) { Errors.Add(TEXT("Debugger UI registry is unavailable.")); }
        if (NOT Plugin.IsValid()) { Errors.Add(TEXT("CkDebugger plugin is unavailable.")); }
        _LoadError = FString::Join(Errors, TEXT("\n"));
        return false;
    }
    const TWeakPtr<SCkInspector_ProbesAuthored> Weak{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    const auto BindText = [&Data, Weak](const FString& InName,
        FString (SCkInspector_ProbesAuthored::* InGetter)() const)
    {
        Data.Text.Add(InName, TAttribute<FText>::CreateLambda([Weak, InGetter]()
        {
            const auto Widget = Weak.Pin();
            return FText::FromString(Widget.IsValid() ? (Widget.Get()->*InGetter)() : FString{});
        }));
    };
    BindText(TEXT("probe-name"), &SCkInspector_ProbesAuthored::Get_NameText);
    BindText(TEXT("probe-state"), &SCkInspector_ProbesAuthored::Get_StateText);
    BindText(TEXT("probe-debug-draw-tooltip"), &SCkInspector_ProbesAuthored::Get_DebugDrawDisabledReason);
    BindText(TEXT("probe-response"), &SCkInspector_ProbesAuthored::Get_ResponseText);
    BindText(TEXT("probe-motion"), &SCkInspector_ProbesAuthored::Get_MotionText);
    BindText(TEXT("probe-quality"), &SCkInspector_ProbesAuthored::Get_QualityText);
    BindText(TEXT("probe-filter"), &SCkInspector_ProbesAuthored::Get_FilterText);
    Data.Visibility.Add(TEXT("probe-available"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_IsAvailable(); }));
    Data.Visibility.Add(TEXT("probe-unavailable"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return NOT Widget.IsValid() || NOT Widget->Get_IsAvailable(); }));
    Data.Visibility.Add(TEXT("probe-debug-draw"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_DebugDrawEnabled(); }));
    Data.Visibility.Add(TEXT("probe-can-toggle-debug-draw"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_CanToggleDebugDraw(); }));
    Data.Visibility.Add(TEXT("probe-has-overlaps"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->_OverlapSnapshot.Num() > 0; }));
    Data.Visibility.Add(TEXT("probe-no-overlaps"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return NOT Widget.IsValid() || Widget->_OverlapSnapshot.IsEmpty(); }));
    Data.Color.Add(TEXT("probe-state-foreground"), TAttribute<FLinearColor>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() ? Widget->Get_StateForeground() : FLinearColor::Transparent; }));
    Data.Color.Add(TEXT("probe-state-background"), TAttribute<FLinearColor>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() ? Widget->Get_StateBackground() : FLinearColor::Transparent; }));
    for (const TPair<FString, FString>& Pair : TArray<TPair<FString, FString>>{
        {TEXT("name"), TEXT("Name:")}, {TEXT("state"), TEXT("State:")},
        {TEXT("debug-draw"), TEXT("Debug Draw:")}, {TEXT("overlaps"), TEXT("Overlaps:")},
        {TEXT("response"), TEXT("Response:")}, {TEXT("motion"), TEXT("Motion:")},
        {TEXT("quality"), TEXT("Quality:")}, {TEXT("filter"), TEXT("Filter:")}})
    {
        Data.Color.Add(TEXT("probe-") + Pair.Key + TEXT("-diff-color"),
            TAttribute<FLinearColor>::CreateLambda([Weak, Label = Pair.Value]()
            {
                const auto Widget = Weak.Pin();
                return Widget.IsValid()
                    ? ck_inspector_probes::DiffColor(Widget->Is_DiffMarked(Label)) : FLinearColor::Transparent;
            }));
    }
    Data.BoolChanged.Add(TEXT("probe-debug-draw-changed"), FCkUiOnBoolChanged::CreateLambda(
        [Weak](const bool bEnabled)
        { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Set_DebugDrawEnabled(bEnabled); } }));
    Data.Collections.Add(TEXT("probe-overlaps"), _Overlaps);
    Data.ItemActions.Add(TEXT("probe-navigate-overlap"), FCkUiOnItemAction::CreateLambda(
        [Weak](const FString& Key)
        { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Navigate_Overlap(Key); } }));
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_IsAvailable(); });
    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        {}, {}, {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString Root = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(FPaths::Combine(Root, TEXT("EcsInspectorProbes.ui.html")),
        FPaths::Combine(Root, TEXT("EcsInspectorProbes.ui.css")));
    Candidate->PollFiles();
    if (NOT Candidate->GetLastResult().Succeeded)
    {
        _LoadError = FString::Join(Candidate->GetLastResult().Errors, TEXT("\n"));
        return false;
    }
    _View = Candidate;
    _Mounted = true;
    _LoadError.Reset();
    return true;
}

auto SCkInspector_ProbesAuthored::Tick(
    const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    if (NOT _Active || NOT _View.IsValid())
    { return; }
    if (NOT Refresh_Overlaps())
    { return; }
    _View->PollFiles();
    if (NOT _View->GetLastResult().Succeeded)
    { _LoadError = FString::Join(_View->GetLastResult().Errors, TEXT("\n")); }
    else
    { _LoadError.Reset(); }
}

auto SCkInspector_ProbesAuthored::Release() -> void
{
    if (NOT _Active)
    { return; }
    _Active = false;
    _Entity = {};
    _DiffLabels.Reset();
    _OverlapsByKey.Reset();
    _OverlapSnapshot.Reset();
    _Overlaps.Reset();
    _View.Reset();
    _DrawState.Reset();
    _Mounted = false;
}

FCkInspector_Probes::FCkInspector_Probes()
{
    Ensure_DrawState();
}

FCkInspector_Probes::~FCkInspector_Probes()
{
    OnDeactivated();
}

auto FCkInspector_Probes::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Probe"));
}

auto FCkInspector_Probes::CanInspect(const FCk_Handle& Entity) const -> bool
{
    auto Probe = FCk_Handle_Probe{};
    return ck_inspector_probes::TryGetProbe(Entity, Probe);
}

auto FCkInspector_Probes::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    Ensure_DrawState();
    const TSharedRef<SWidget> NativeBody = Build_NativeBody(Entity);
    if (FCkInspector_RowCaptureScope::Is_Active())
    { return NativeBody; }
    auto DiffLabels = TSet<FString>{};
    for (const FString& Label : TArray<FString>{
        TEXT("Name:"), TEXT("State:"), TEXT("Debug Draw:"), TEXT("Overlaps:"),
        TEXT("Response:"), TEXT("Motion:"), TEXT("Quality:"), TEXT("Filter:")})
    {
        if (FCkInspector_DiffMarkScope::Is_LabelMarked(Label))
        { DiffLabels.Add(Label); }
    }
    const TSharedRef<SCkInspector_ProbesAuthored> Authored = SNew(SCkInspector_ProbesAuthored)
        .Entity(Entity).DiffLabels(MoveTemp(DiffLabels)).DrawState(_DrawState);
    if (NOT Authored->Is_Mounted())
    {
        _LastAuthoredLoadError = Authored->Get_LoadError();
        return NativeBody;
    }
    _LastAuthoredLoadError.Reset();
    _AuthoredInstances.Add(Authored);
    return Authored;
}

auto FCkInspector_Probes::Build_NativeBody(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder{};
    Builder.SetEditGuard(Get_EditGuard());
    auto Probe = FCk_Handle_Probe{};
    if (NOT ck_inspector_probes::TryGetProbe(Entity, Probe))
    { return Builder.Build(Entity, FString{}); }
    const auto CapturedProbe = Probe;
    const FGameplayTag ProbeName = UCk_Utils_Probe_UE::Get_Name(Probe);
    const FString ProbeNameText = ProbeName.IsValid() ? ProbeName.GetTagName().ToString() : TEXT("Unnamed");
    Builder.AddRow(FText::FromString(TEXT("Name:")),
        [ProbeNameText](const FCk_Handle&) { return FText::FromString(ProbeNameText); }, CkStyle::Value_Tag());
    Builder.AddStatusPillRow(FText::FromString(TEXT("State:")),
        TAttribute<FText>::CreateLambda([CapturedProbe]()
        {
            return FText::FromString(ck::IsValid(CapturedProbe)
                && UCk_Utils_Probe_UE::Get_IsEnabledDisabled(CapturedProbe) == ECk_EnableDisable::Enable
                ? TEXT("Enabled") : TEXT("Disabled"));
        }), TAttribute<ECk_Tone>::CreateLambda([CapturedProbe]()
        {
            return ck::IsValid(CapturedProbe)
                && UCk_Utils_Probe_UE::Get_IsEnabledDisabled(CapturedProbe) == ECk_EnableDisable::Enable
                ? ECk_Tone::Ok : ECk_Tone::Neutral;
        }));
    const TSharedPtr<FCkInspector_ProbeDrawState> State = _DrawState;
    Builder.AddToggleRow(FText::FromString(TEXT("Debug Draw:")),
        TAttribute<bool>::CreateLambda([State]()
        { return State.IsValid() && State->Active && State->DesiredEnabled; }),
        [State, Entity](const bool bEnabled)
        {
            if (NOT State.IsValid() || NOT State->Active
                || NOT ck_inspector_probes::GetRequestGate(Entity).IsEnabled)
            { return; }
            State->DesiredEnabled = bEnabled;
            ck_inspector_probes::ApplyDesired(*State, Entity);
        }, ECk_DebugRequest_Requirement::CosmeticOnly);
    auto OverlapHandles = TArray<FCk_Handle>{};
    for (const FCk_Probe_OverlapInfo& Info : UCk_Utils_Probe_UE::Get_CurrentOverlaps(Probe))
    { if (ck::IsValid(Info.Get_OtherEntity())) { OverlapHandles.Add(Info.Get_OtherEntity()); } }
    _NativeLastOverlapCount = OverlapHandles.Num();
    _NativeOverlapsBox = FCkInspectorWidgetBuilder::MakeBadgeBox(OverlapHandles);
    Builder.AddWidgetRow(FText::FromString(TEXT("Overlaps:")), _NativeOverlapsBox.ToSharedRef());
    Builder.AddRow(FText::FromString(TEXT("Response:")), [](const FCk_Handle& E)
    {
        auto Mutable = E; const auto Current = UCk_Utils_Probe_UE::Cast(Mutable);
        if (ck::Is_NOT_Valid(Current)) { return FText::FromString(TEXT("--")); }
        return FText::FromString(UCk_Utils_Probe_UE::Get_ResponsePolicy(Current)
            == ECk_ProbeResponse_Policy::Notify ? TEXT("Notify") : TEXT("Silent"));
    }, CkStyle::State_Config());
    Builder.AddRow(FText::FromString(TEXT("Motion:")), [](const FCk_Handle& E)
    {
        auto Mutable = E; const auto Current = UCk_Utils_Probe_UE::Cast(Mutable);
        if (ck::Is_NOT_Valid(Current)) { return FText::FromString(TEXT("--")); }
        switch (UCk_Utils_Probe_UE::Get_MotionType(Current))
        {
        case ECk_MotionType::Static: return FText::FromString(TEXT("Static"));
        case ECk_MotionType::Kinematic: return FText::FromString(TEXT("Kinematic"));
        case ECk_MotionType::Dynamic: return FText::FromString(TEXT("Dynamic"));
        default: return FText::FromString(TEXT("Unknown"));
        }
    }, CkStyle::State_Config());
    Builder.AddRow(FText::FromString(TEXT("Quality:")), [](const FCk_Handle& E)
    {
        auto Mutable = E; const auto Current = UCk_Utils_Probe_UE::Cast(Mutable);
        if (ck::Is_NOT_Valid(Current)) { return FText::FromString(TEXT("--")); }
        return FText::FromString(UCk_Utils_Probe_UE::Get_MotionQuality(Current)
            == ECk_MotionQuality::Discrete ? TEXT("Discrete") : TEXT("LinearCast (CCD)"));
    }, CkStyle::State_Config());
    Builder.AddRow(FText::FromString(TEXT("Filter:")), [](const FCk_Handle& E)
    {
        auto Mutable = E; const auto Current = UCk_Utils_Probe_UE::Cast(Mutable);
        if (ck::Is_NOT_Valid(Current)) { return FText::FromString(TEXT("--")); }
        const FGameplayTagContainer Filter = UCk_Utils_Probe_UE::Get_Filter(Current);
        return FText::FromString(Filter.IsEmpty() ? TEXT("(Empty)") : Filter.ToString());
    }, CkStyle::TextDim());
    return Builder.Build(Entity, FString{});
}

auto FCkInspector_Probes::Ensure_DrawState() -> void
{
    if (_DrawState.IsValid() && _DrawState->Active)
    { return; }
    _DrawState = MakeShared<FCkInspector_ProbeDrawState>();
    _DrawState->DesiredEnabled = _DebugDrawPreference;
}

auto FCkInspector_Probes::Disable_AllOwnedDebugDraw() -> void
{
    if (NOT _DrawState.IsValid())
    { return; }
    ck_inspector_probes::DisableAllOwned(*_DrawState);
}

auto FCkInspector_Probes::Tick(const FCk_Handle& Entity, float) -> void
{
    Ensure_DrawState();
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_ProbesAuthored>& Weak)
    { const auto Instance = Weak.Pin(); return NOT Instance.IsValid() || Instance->Is_Inert(); });
    if (_LastInspectedEntity != Entity)
    {
        if (ck::IsValid(_LastInspectedEntity))
        { ck_inspector_probes::DisableOwnedProbe(*_DrawState, _LastInspectedEntity); }
        _LastInspectedEntity = Entity;
    }
    if (ck_inspector_probes::GetRequestGate(Entity).IsEnabled)
    { ck_inspector_probes::ApplyDesired(*_DrawState, Entity); }
    auto Probe = FCk_Handle_Probe{};
    if (_NativeOverlapsBox.IsValid() && ck_inspector_probes::TryGetProbe(Entity, Probe))
    {
        const auto Current = UCk_Utils_Probe_UE::Get_CurrentOverlaps(Probe);
        if (Current.Num() != _NativeLastOverlapCount)
        {
            auto Handles = TArray<FCk_Handle>{};
            for (const FCk_Probe_OverlapInfo& Info : Current)
            { if (ck::IsValid(Info.Get_OtherEntity())) { Handles.Add(Info.Get_OtherEntity()); } }
            FCkInspectorWidgetBuilder::PopulateBadgeBox(*_NativeOverlapsBox, Handles);
            _NativeLastOverlapCount = Handles.Num();
        }
    }
    _LastAuthoredLoadError.Reset();
    for (const auto& Weak : _AuthoredInstances)
    {
        if (const auto Instance = Weak.Pin(); Instance.IsValid() && NOT Instance->Get_LoadError().IsEmpty())
        { _LastAuthoredLoadError = Instance->Get_LoadError(); break; }
    }
}

auto FCkInspector_Probes::OnDeactivated() -> void
{
    if (_DrawState.IsValid())
    {
        _DebugDrawPreference = _DrawState->DesiredEnabled;
        Disable_AllOwnedDebugDraw();
        _DrawState->Active = false;
    }
    for (const auto& Weak : _AuthoredInstances)
    { if (const auto Instance = Weak.Pin(); Instance.IsValid()) { Instance->Release(); } }
    _AuthoredInstances.Reset();
    _LastInspectedEntity = {};
    _NativeOverlapsBox.Reset();
    _NativeLastOverlapCount = -1;
    _LastAuthoredLoadError.Reset();
}
