#include "CkInspector_ProbeTraces.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkSpatialQuery/Probe/CkProbe_Fragment.h"
#include "CkSpatialQuery/Probe/CkProbeTrace_Utils.h"

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

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_ProbeTraces)

namespace ck_inspector_probetraces
{
    auto IsDestroying(const FCk_Handle& InEntity) -> bool
    {
        return ck::Is_NOT_Valid(InEntity)
            || InEntity.Has_Any<ck::FTag_DestroyEntity_Initiate, ck::FTag_DestroyEntity_EndPlay,
                ck::FTag_DestroyEntity_Teardown, ck::FTag_DestroyEntity_Await,
                ck::FTag_DestroyEntity_Finalize>();
    }

    auto GetStructureMask(const FCk_Handle& InEntity) -> uint8
    {
        if (IsDestroying(InEntity) || NOT UCk_Utils_ProbeTrace_UE::Has(InEntity)
            || NOT InEntity.Has<TSet<FCk_Probe_OverlapInfo>>())
        { return 0; }
        const bool bRay = InEntity.Has<ck::FFragment_ProbeTrace_RayCast>();
        const bool bShape = InEntity.Has<ck::FFragment_ProbeTrace_ShapeCast>();
        if (bRay == bShape)
        { return 0; }
        return bRay ? 1 : 2;
    }

    auto TryGetTrace(const FCk_Handle& InEntity, FCk_Handle_ProbeTrace& OutTrace) -> bool
    {
        OutTrace = {};
        if (GetStructureMask(InEntity) == 0)
        { return false; }
        auto Mutable = InEntity;
        OutTrace = UCk_Utils_ProbeTrace_UE::Cast(Mutable);
        return ck::IsValid(OutTrace);
    }

    auto GetDirection(const FCk_Handle& InEntity) -> FVector
    {
        const uint8 Mask = GetStructureMask(InEntity);
        if (Mask == 1)
        { return InEntity.Get<ck::FFragment_ProbeTrace_RayCast>().Get_DirectionAndLength(); }
        if (Mask == 2)
        { return InEntity.Get<ck::FFragment_ProbeTrace_ShapeCast>().Get_DirectionAndLength(); }
        return FVector::ZeroVector;
    }

    auto GetTypeText(const FCk_Handle& InEntity) -> FString
    {
        const uint8 Mask = GetStructureMask(InEntity);
        return Mask == 1 ? TEXT("RayCast") : Mask == 2 ? TEXT("ShapeCast") : TEXT("--");
    }

    auto GetPolicyText(const FCk_Handle& InEntity) -> FString
    {
        const uint8 Mask = GetStructureMask(InEntity);
        if (Mask == 0)
        { return TEXT("--"); }
        const ECk_ProbeTrace_Policy Policy = Mask == 1
            ? InEntity.Get<ck::FFragment_ProbeTrace_RayCast>().Get_TracePolicy()
            : InEntity.Get<ck::FFragment_ProbeTrace_ShapeCast>().Get_TracePolicy();
        return Policy == ECk_ProbeTrace_Policy::Single ? TEXT("Single") : TEXT("Multi");
    }

    auto GetShapeText(const FCk_Handle& InEntity) -> FString
    {
        if (GetStructureMask(InEntity) != 2)
        { return TEXT("--"); }
        switch (InEntity.Get<ck::FFragment_ProbeTrace_ShapeCast>().Get_Shape().Get_ShapeType())
        {
        case ECk_Shape_Type::Box: return TEXT("Box");
        case ECk_Shape_Type::Sphere: return TEXT("Sphere");
        case ECk_Shape_Type::Capsule: return TEXT("Capsule");
        case ECk_Shape_Type::Cylinder: return TEXT("Cylinder");
        default: return TEXT("Unknown");
        }
    }

    auto GetFilterText(const FCk_Handle& InEntity) -> FString
    {
        const uint8 Mask = GetStructureMask(InEntity);
        if (Mask == 0)
        { return TEXT("--"); }
        const FGameplayTagContainer Filter = Mask == 1
            ? InEntity.Get<ck::FFragment_ProbeTrace_RayCast>().Get_Filter()
            : InEntity.Get<ck::FFragment_ProbeTrace_ShapeCast>().Get_Filter();
        return Filter.IsEmpty() ? TEXT("(Empty)") : Filter.ToString();
    }

    auto GetRequestGate(const FCk_Handle& InEntity) -> FCk_DebugRequest_GateVerdict
    {
        auto Trace = FCk_Handle_ProbeTrace{};
        if (NOT TryGetTrace(InEntity, Trace))
        { return {false, FText::FromString(TEXT("Probe Trace is unavailable."))}; }
        return ck::DebugRequestGate::Evaluate(InEntity, ECk_DebugRequest_Requirement::LocalOk);
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

    auto GetOverlapSnapshot(const FCk_Handle& InEntity, TArray<FCk_Handle>* OutHandles = nullptr) -> TArray<FString>
    {
        auto Snapshot = TArray<FString>{};
        auto Trace = FCk_Handle_ProbeTrace{};
        if (NOT TryGetTrace(InEntity, Trace))
        { return Snapshot; }
        for (const FCk_Probe_OverlapInfo& Info : UCk_Utils_ProbeTrace_UE::Get_CurrentOverlaps(Trace))
        {
            const FCk_Handle Other = Info.Get_OtherEntity();
            const FString Key = GetHandleKey(Other);
            if (Key.IsEmpty())
            { continue; }
            const FString Name = UCk_Utils_Handle_UE::Get_DebugName(Other).ToString();
            Snapshot.Add(Key + TEXT("|") + Name);
            if (OutHandles != nullptr)
            { OutHandles->Add(Other); }
        }
        Snapshot.Sort();
        return Snapshot;
    }
}

auto SCkInspector_ProbeTracesAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _DiffLabels = InArgs._DiffLabels;
    const TSharedRef<SBox> Host = SNew(SBox);
    ChildSlot[Host];
    if (Refresh_Overlaps() && Build_AuthoredView())
    {
        Host->SetContent(_View->GetRegion(TEXT("main")));
        return;
    }
    Release();
    Host->SetContent(SNullWidget::NullWidget);
}

SCkInspector_ProbeTracesAuthored::~SCkInspector_ProbeTracesAuthored()
{
    Release();
}

auto SCkInspector_ProbeTracesAuthored::Get_IsAvailable() const -> bool
{
    auto Trace = FCk_Handle_ProbeTrace{};
    return _Active && ck_inspector_probetraces::TryGetTrace(_Entity, Trace);
}

auto SCkInspector_ProbeTracesAuthored::Get_IsEnabled() const -> bool
{
    auto Trace = FCk_Handle_ProbeTrace{};
    return _Active && ck_inspector_probetraces::TryGetTrace(_Entity, Trace)
        && UCk_Utils_ProbeTrace_UE::Get_IsEnabledDisabled(Trace) == ECk_EnableDisable::Enable;
}

auto SCkInspector_ProbeTracesAuthored::Get_CanToggleEnabled() const -> bool
{
    return _Active && ck_inspector_probetraces::GetRequestGate(_Entity).IsEnabled;
}

auto SCkInspector_ProbeTracesAuthored::Get_EnabledTooltip() const -> FString
{
    if (NOT _Active)
    { return {}; }
    const FCk_DebugRequest_GateVerdict Gate = ck_inspector_probetraces::GetRequestGate(_Entity);
    return Gate.IsEnabled ? TEXT("Enable or disable the inspected persistent probe trace.") : Gate.Reason.ToString();
}

auto SCkInspector_ProbeTracesAuthored::Get_TypeText() const -> FString
{
    return _Active ? ck_inspector_probetraces::GetTypeText(_Entity) : TEXT("--");
}

auto SCkInspector_ProbeTracesAuthored::Get_DirectionXText() const -> FString
{
    return ck::Format_UE(TEXT("{:.3f}"), _Active ? ck_inspector_probetraces::GetDirection(_Entity).X : 0.0);
}

auto SCkInspector_ProbeTracesAuthored::Get_DirectionYText() const -> FString
{
    return ck::Format_UE(TEXT("{:.3f}"), _Active ? ck_inspector_probetraces::GetDirection(_Entity).Y : 0.0);
}

auto SCkInspector_ProbeTracesAuthored::Get_DirectionZText() const -> FString
{
    return ck::Format_UE(TEXT("{:.3f}"), _Active ? ck_inspector_probetraces::GetDirection(_Entity).Z : 0.0);
}

auto SCkInspector_ProbeTracesAuthored::Get_PolicyText() const -> FString
{
    return _Active ? ck_inspector_probetraces::GetPolicyText(_Entity) : TEXT("--");
}

auto SCkInspector_ProbeTracesAuthored::Get_IsShapeVisible() const -> bool
{
    return _Active && ck_inspector_probetraces::GetStructureMask(_Entity) == 2;
}

auto SCkInspector_ProbeTracesAuthored::Get_ShapeText() const -> FString
{
    return _Active ? ck_inspector_probetraces::GetShapeText(_Entity) : TEXT("--");
}

auto SCkInspector_ProbeTracesAuthored::Get_FilterText() const -> FString
{
    return _Active ? ck_inspector_probetraces::GetFilterText(_Entity) : TEXT("--");
}

auto SCkInspector_ProbeTracesAuthored::Refresh_Overlaps() -> bool
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
    auto Trace = FCk_Handle_ProbeTrace{};
    if (_Active && ck_inspector_probetraces::TryGetTrace(_Entity, Trace))
    {
        for (const FCk_Probe_OverlapInfo& Info : UCk_Utils_ProbeTrace_UE::Get_CurrentOverlaps(Trace))
        {
            const FCk_Handle Other = Info.Get_OtherEntity();
            const FString Key = ck_inspector_probetraces::GetHandleKey(Other);
            if (Key.IsEmpty() || ByKey.Contains(Key))
            { continue; }
            const FString Name = UCk_Utils_Handle_UE::Get_DebugName(Other).ToString();
            auto Record = FCkUiRecordData{};
            Record.Key = Key;
            Record.Fields.Add(TEXT("overlap-id"), ck_inspector_probetraces::TextField(
                ck::Format_UE(TEXT("{}"), Other.Get_Entity())));
            Record.Fields.Add(TEXT("overlap-name"), ck_inspector_probetraces::TextField(Name));
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

auto SCkInspector_ProbeTracesAuthored::Set_Enabled(const bool bInEnabled) -> void
{
    if (NOT Get_CanToggleEnabled())
    { return; }
    auto Trace = FCk_Handle_ProbeTrace{};
    if (NOT ck_inspector_probetraces::TryGetTrace(_Entity, Trace))
    { return; }
    UCk_Utils_ProbeTrace_UE::Request_EnableDisable(Trace,
        FCk_Request_Probe_EnableDisable{bInEnabled ? ECk_EnableDisable::Enable : ECk_EnableDisable::Disable}, {});
    Refresh_Overlaps();
}

auto SCkInspector_ProbeTracesAuthored::Navigate_Overlap(const FString& InStableKey) -> void
{
    if (NOT Get_IsAvailable())
    { return; }
    const FCk_Handle* Expected = _OverlapsByKey.Find(InStableKey);
    if (Expected == nullptr || ck::Is_NOT_Valid(*Expected))
    { return; }
    auto Trace = FCk_Handle_ProbeTrace{};
    if (NOT ck_inspector_probetraces::TryGetTrace(_Entity, Trace))
    { return; }
    for (const FCk_Probe_OverlapInfo& Info : UCk_Utils_ProbeTrace_UE::Get_CurrentOverlaps(Trace))
    {
        const FCk_Handle Current = Info.Get_OtherEntity();
        if (Current == *Expected && ck_inspector_probetraces::GetHandleKey(Current) == InStableKey)
        {
            ck::DebugNav::Goto_Entity(Current);
            return;
        }
    }
}

auto SCkInspector_ProbeTracesAuthored::Build_AuthoredView() -> bool
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
    const TWeakPtr<SCkInspector_ProbeTracesAuthored> Weak{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    const auto BindText = [&Data, Weak](const FString& InName,
        FString (SCkInspector_ProbeTracesAuthored::* InGetter)() const)
    {
        Data.Text.Add(InName, TAttribute<FText>::CreateLambda([Weak, InGetter]()
        {
            const auto Widget = Weak.Pin();
            return FText::FromString(Widget.IsValid() ? (Widget.Get()->*InGetter)() : FString{});
        }));
    };
    BindText(TEXT("probe-trace-enabled-tooltip"), &SCkInspector_ProbeTracesAuthored::Get_EnabledTooltip);
    BindText(TEXT("probe-trace-type"), &SCkInspector_ProbeTracesAuthored::Get_TypeText);
    BindText(TEXT("probe-trace-direction-x"), &SCkInspector_ProbeTracesAuthored::Get_DirectionXText);
    BindText(TEXT("probe-trace-direction-y"), &SCkInspector_ProbeTracesAuthored::Get_DirectionYText);
    BindText(TEXT("probe-trace-direction-z"), &SCkInspector_ProbeTracesAuthored::Get_DirectionZText);
    BindText(TEXT("probe-trace-policy"), &SCkInspector_ProbeTracesAuthored::Get_PolicyText);
    BindText(TEXT("probe-trace-shape"), &SCkInspector_ProbeTracesAuthored::Get_ShapeText);
    BindText(TEXT("probe-trace-filter"), &SCkInspector_ProbeTracesAuthored::Get_FilterText);
    Data.Visibility.Add(TEXT("probe-trace-available"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_IsAvailable(); }));
    Data.Visibility.Add(TEXT("probe-trace-unavailable"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return NOT Widget.IsValid() || NOT Widget->Get_IsAvailable(); }));
    Data.Visibility.Add(TEXT("probe-trace-enabled"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_IsEnabled(); }));
    Data.Visibility.Add(TEXT("probe-trace-can-toggle-enabled"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_CanToggleEnabled(); }));
    Data.Visibility.Add(TEXT("probe-trace-shape-visible"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_IsShapeVisible(); }));
    Data.Visibility.Add(TEXT("probe-trace-overlaps-present"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_HasOverlaps(); }));
    Data.Visibility.Add(TEXT("probe-trace-overlaps-empty"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return NOT Widget.IsValid() || NOT Widget->Get_HasOverlaps(); }));
    for (const TPair<FString, FString>& Pair : TArray<TPair<FString, FString>>{
        {TEXT("enabled"), TEXT("Enabled:")}, {TEXT("type"), TEXT("Type:")},
        {TEXT("direction"), TEXT("Direction:")}, {TEXT("policy"), TEXT("Policy:")},
        {TEXT("shape"), TEXT("Shape:")}, {TEXT("filter"), TEXT("Filter:")},
        {TEXT("overlaps"), TEXT("Overlaps:")}})
    {
        Data.Color.Add(TEXT("probe-trace-") + Pair.Key + TEXT("-diff-color"),
            TAttribute<FLinearColor>::CreateLambda([Weak, Label = Pair.Value]()
            {
                const auto Widget = Weak.Pin();
                return Widget.IsValid()
                    ? ck_inspector_probetraces::DiffColor(Widget->Is_DiffMarked(Label))
                    : FLinearColor::Transparent;
            }));
    }
    Data.BoolChanged.Add(TEXT("probe-trace-enabled-changed"), FCkUiOnBoolChanged::CreateLambda(
        [Weak](const bool bEnabled)
        { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Set_Enabled(bEnabled); } }));
    Data.Collections.Add(TEXT("probe-trace-overlaps"), _Overlaps);
    Data.ItemActions.Add(TEXT("probe-trace-navigate-overlap"), FCkUiOnItemAction::CreateLambda(
        [Weak](const FString& Key)
        { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Navigate_Overlap(Key); } }));
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_IsAvailable(); });
    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        {}, {}, {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString Root = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(FPaths::Combine(Root, TEXT("EcsInspectorProbeTraces.ui.html")),
        FPaths::Combine(Root, TEXT("EcsInspectorProbeTraces.ui.css")));
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

auto SCkInspector_ProbeTracesAuthored::Tick(
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

auto SCkInspector_ProbeTracesAuthored::Release() -> void
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
    _Mounted = false;
}

FCkInspector_ProbeTraces::~FCkInspector_ProbeTraces()
{
    OnDeactivated();
}

auto FCkInspector_ProbeTraces::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Probe Trace"));
}

auto FCkInspector_ProbeTraces::CanInspect(const FCk_Handle& Entity) const -> bool
{
    auto Trace = FCk_Handle_ProbeTrace{};
    return ck_inspector_probetraces::TryGetTrace(Entity, Trace);
}

auto FCkInspector_ProbeTraces::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> NativeBody = Build_NativeBody(Entity);
    if (FCkInspector_RowCaptureScope::Is_Active())
    { return NativeBody; }
    auto DiffLabels = TSet<FString>{};
    for (const FString& Label : TArray<FString>{
        TEXT("Enabled:"), TEXT("Type:"), TEXT("Direction:"), TEXT("Policy:"),
        TEXT("Shape:"), TEXT("Filter:"), TEXT("Overlaps:")})
    {
        if (FCkInspector_DiffMarkScope::Is_LabelMarked(Label))
        { DiffLabels.Add(Label); }
    }
    const TSharedRef<SCkInspector_ProbeTracesAuthored> Authored = SNew(SCkInspector_ProbeTracesAuthored)
        .Entity(Entity).DiffLabels(MoveTemp(DiffLabels));
    if (NOT Authored->Is_Mounted())
    {
        _LastAuthoredLoadError = Authored->Get_LoadError();
        return NativeBody;
    }
    _LastAuthoredLoadError.Reset();
    _AuthoredInstances.Add(Authored);
    _StructureMask = ck_inspector_probetraces::GetStructureMask(Entity);
    return Authored;
}

auto FCkInspector_ProbeTraces::Build_NativeBody(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder{};
    Builder.SetEditGuard(Get_EditGuard());
    auto Trace = FCk_Handle_ProbeTrace{};
    if (NOT ck_inspector_probetraces::TryGetTrace(Entity, Trace))
    { return Builder.Build(Entity, FString{}); }
    Builder.AddToggleRow(FText::FromString(TEXT("Enabled:")),
        TAttribute<bool>::CreateLambda([Entity]()
        {
            auto Current = FCk_Handle_ProbeTrace{};
            return ck_inspector_probetraces::TryGetTrace(Entity, Current)
                && UCk_Utils_ProbeTrace_UE::Get_IsEnabledDisabled(Current) == ECk_EnableDisable::Enable;
        }),
        [Entity](const bool bEnabled)
        {
            if (NOT ck_inspector_probetraces::GetRequestGate(Entity).IsEnabled)
            { return; }
            auto Current = FCk_Handle_ProbeTrace{};
            if (NOT ck_inspector_probetraces::TryGetTrace(Entity, Current))
            { return; }
            UCk_Utils_ProbeTrace_UE::Request_EnableDisable(Current,
                FCk_Request_Probe_EnableDisable{
                    bEnabled ? ECk_EnableDisable::Enable : ECk_EnableDisable::Disable}, {});
        }, ECk_DebugRequest_Requirement::LocalOk);
    Builder.AddRow(FText::FromString(TEXT("Type:")), [](const FCk_Handle& Current)
    { return FText::FromString(ck_inspector_probetraces::GetTypeText(Current)); }, CkStyle::Value_Enum());
    auto Components = TArray<TAttribute<FText>>{};
    Components.Reserve(3);
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        Components.Emplace(TAttribute<FText>::CreateLambda([Entity, Axis]()
        {
            return FText::FromString(ck::Format_UE(
                TEXT("{:.3f}"), ck_inspector_probetraces::GetDirection(Entity)[Axis]));
        }));
    }
    Builder.AddAlignedNumericRow(FText::FromString(TEXT("Direction:")), Components);
    Builder.AddRow(FText::FromString(TEXT("Policy:")), [](const FCk_Handle& Current)
    { return FText::FromString(ck_inspector_probetraces::GetPolicyText(Current)); }, CkStyle::State_Config());
    if (ck_inspector_probetraces::GetStructureMask(Entity) == 2)
    {
        Builder.AddRow(FText::FromString(TEXT("Shape:")), [](const FCk_Handle& Current)
        { return FText::FromString(ck_inspector_probetraces::GetShapeText(Current)); }, CkStyle::State_Config());
    }
    Builder.AddRow(FText::FromString(TEXT("Filter:")), [](const FCk_Handle& Current)
    { return FText::FromString(ck_inspector_probetraces::GetFilterText(Current)); }, CkStyle::TextDim());
    auto Handles = TArray<FCk_Handle>{};
    _NativeOverlapSnapshot = ck_inspector_probetraces::GetOverlapSnapshot(Entity, &Handles);
    _NativeOverlapsBox = FCkInspectorWidgetBuilder::MakeBadgeBox(Handles);
    Builder.AddWidgetRow(FText::FromString(TEXT("Overlaps:")), _NativeOverlapsBox.ToSharedRef());
    return Builder.Build(Entity, FString{});
}

auto FCkInspector_ProbeTraces::Disable_OwnedDebugDraw(const FCk_Handle& Entity) -> void
{
    const int32 Index = _OwnedDebugDrawTraces.IndexOfByKey(Entity);
    if (Index == INDEX_NONE)
    { return; }
    auto Mutable = Entity;
    if (ck::IsValid(Mutable) && Mutable.Has<ck::FTag_ProbeTrace_DebugDraw>())
    { Mutable.Try_Remove<ck::FTag_ProbeTrace_DebugDraw>(); }
    _OwnedDebugDrawTraces.RemoveAt(Index);
}

auto FCkInspector_ProbeTraces::Disable_AllOwnedDebugDraw() -> void
{
    const auto Owned = _OwnedDebugDrawTraces;
    for (const FCk_Handle& Entity : Owned)
    { Disable_OwnedDebugDraw(Entity); }
    _OwnedDebugDrawTraces.Reset();
}

auto FCkInspector_ProbeTraces::Tick(const FCk_Handle& Entity, float) -> void
{
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_ProbeTracesAuthored>& Weak)
    { const auto Instance = Weak.Pin(); return NOT Instance.IsValid() || Instance->Is_Inert(); });
    if (_LastInspectedEntity != Entity)
    {
        Disable_OwnedDebugDraw(_LastInspectedEntity);
        _LastInspectedEntity = Entity;
    }
    auto Trace = FCk_Handle_ProbeTrace{};
    if (ck_inspector_probetraces::TryGetTrace(Entity, Trace)
        && ck::DebugRequestGate::Evaluate(Entity, ECk_DebugRequest_Requirement::CosmeticOnly).IsEnabled)
    {
        if (NOT Entity.Has<ck::FTag_ProbeTrace_DebugDraw>())
        {
            auto Mutable = Entity;
            Mutable.AddOrGet<ck::FTag_ProbeTrace_DebugDraw>();
            if (Mutable.Has<ck::FTag_ProbeTrace_DebugDraw>())
            { _OwnedDebugDrawTraces.AddUnique(Entity); }
        }
    }
    else
    {
        Disable_OwnedDebugDraw(Entity);
    }
    if (_NativeOverlapsBox.IsValid())
    {
        auto Handles = TArray<FCk_Handle>{};
        const TArray<FString> Snapshot = ck_inspector_probetraces::GetOverlapSnapshot(Entity, &Handles);
        if (Snapshot != _NativeOverlapSnapshot)
        {
            FCkInspectorWidgetBuilder::PopulateBadgeBox(*_NativeOverlapsBox, Handles);
            _NativeOverlapSnapshot = Snapshot;
        }
    }
    const uint8 StructureMask = ck_inspector_probetraces::GetStructureMask(Entity);
    if (StructureMask != _StructureMask)
    {
        _StructureMask = StructureMask;
        RequestRebuild();
    }
    _LastAuthoredLoadError.Reset();
    for (const auto& Weak : _AuthoredInstances)
    {
        if (const auto Instance = Weak.Pin(); Instance.IsValid() && NOT Instance->Get_LoadError().IsEmpty())
        { _LastAuthoredLoadError = Instance->Get_LoadError(); break; }
    }
}

auto FCkInspector_ProbeTraces::OnDeactivated() -> void
{
    Disable_AllOwnedDebugDraw();
    for (const auto& Weak : _AuthoredInstances)
    { if (const auto Instance = Weak.Pin(); Instance.IsValid()) { Instance->Release(); } }
    _AuthoredInstances.Reset();
    _LastInspectedEntity = {};
    _NativeOverlapsBox.Reset();
    _NativeOverlapSnapshot.Reset();
    _LastAuthoredLoadError.Reset();
    _StructureMask = 0;
}
