#include "CkInspector_OverlapBody.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkOverlapBody/Marker/CkMarker_Fragment.h"
#include "CkOverlapBody/Marker/CkMarker_Utils.h"
#include "CkOverlapBody/Sensor/CkSensor_Fragment.h"
#include "CkOverlapBody/Sensor/CkSensor_Utils.h"

#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Utils/CkDebug_RequestGate.h"
#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "CkSlateLayout/SCkUiSurface.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_OverlapBody)

// =====================================================================================================================

namespace ck_inspector_overlapbody
{
    auto Has_Marker(const FCk_Handle& InEntity) -> bool
    {
        return ck::IsValid(InEntity) && InEntity.Has<ck::FFragment_Marker_Current>();
    }

    auto Has_Sensor(const FCk_Handle& InEntity) -> bool
    {
        return ck::IsValid(InEntity) && InEntity.Has<ck::FFragment_Sensor_Current>();
    }

    auto TryGet_Marker(const FCk_Handle& InEntity, FCk_Handle_Marker& OutMarker) -> bool
    {
        OutMarker = {};
        if (NOT Has_Marker(InEntity) || NOT InEntity.Has<ck::FFragment_Marker_Params>()) { return false; }
        auto MutableEntity = InEntity;
        OutMarker = UCk_Utils_Marker_UE::Cast(MutableEntity);
        return ck::IsValid(OutMarker);
    }

    auto TryGet_Sensor(const FCk_Handle& InEntity, FCk_Handle_Sensor& OutSensor) -> bool
    {
        OutSensor = {};
        if (NOT Has_Sensor(InEntity) || NOT InEntity.Has<ck::FFragment_Sensor_Params>()) { return false; }
        auto MutableEntity = InEntity;
        OutSensor = UCk_Utils_Sensor_UE::Cast(MutableEntity);
        return ck::IsValid(OutSensor);
    }

    static auto Get_EnableDisableTone(ECk_EnableDisable InState) -> ECk_Tone
    {
        return InState == ECk_EnableDisable::Enable ? ECk_Tone::Ok : ECk_Tone::Neutral;
    }

    // A marker/sensor without a backing shape overlaps nothing — a defect, not a configuration.
    static auto Get_ShapeTone(bool InIsValid) -> ECk_Tone
    {
        return InIsValid ? ECk_Tone::Ok : ECk_Tone::Err;
    }

    static auto Make_ShapeText(bool InIsValid) -> FText
    {
        return FText::FromString(InIsValid ? TEXT("Valid") : TEXT("None"));
    }

    auto Get_MarkerState(const FCk_Handle& InEntity) -> ECk_EnableDisable
    {
        return Has_Marker(InEntity)
            ? InEntity.Get<ck::FFragment_Marker_Current>().Get_EnableDisable()
            : ECk_EnableDisable::Disable;
    }

    auto Get_SensorState(const FCk_Handle& InEntity) -> ECk_EnableDisable
    {
        return Has_Sensor(InEntity)
            ? InEntity.Get<ck::FFragment_Sensor_Current>().Get_EnableDisable()
            : ECk_EnableDisable::Disable;
    }

    auto Get_MarkerShapeIsValid(const FCk_Handle& InEntity) -> bool
    {
        return Has_Marker(InEntity)
            && InEntity.Get<ck::FFragment_Marker_Current>().Get_Marker().IsValid();
    }

    auto Get_SensorShapeIsValid(const FCk_Handle& InEntity) -> bool
    {
        return Has_Sensor(InEntity)
            && InEntity.Get<ck::FFragment_Sensor_Current>().Get_Sensor().IsValid();
    }

    auto Get_MarkerOverlapCount(const FCk_Handle& InEntity) -> int32
    {
        return Has_Sensor(InEntity)
            ? InEntity.Get<ck::FFragment_Sensor_Current>().Get_CurrentMarkerOverlaps().Get_Overlaps().Num()
            : 0;
    }

    auto Get_NonMarkerOverlapCount(const FCk_Handle& InEntity) -> int32
    {
        return Has_Sensor(InEntity)
            ? InEntity.Get<ck::FFragment_Sensor_Current>().Get_CurrentNonMarkerOverlaps().Get_Overlaps().Num()
            : 0;
    }

    auto Get_MarkerGate(const FCk_Handle& InEntity) -> FCk_DebugRequest_GateVerdict
    {
        auto Marker = FCk_Handle_Marker{};
        if (NOT TryGet_Marker(InEntity, Marker))
        { return {false, FText::FromString(TEXT("Marker is unavailable."))}; }
        return ck::DebugRequestGate::Evaluate(InEntity, ECk_DebugRequest_Requirement::LocalOk);
    }

    auto Get_SensorGate(const FCk_Handle& InEntity) -> FCk_DebugRequest_GateVerdict
    {
        auto Sensor = FCk_Handle_Sensor{};
        if (NOT TryGet_Sensor(InEntity, Sensor))
        { return {false, FText::FromString(TEXT("Sensor is unavailable."))}; }
        return ck::DebugRequestGate::Evaluate(InEntity, ECk_DebugRequest_Requirement::LocalOk);
    }

    auto DiffColor(const bool bDiffMarked) -> FLinearColor
    {
        return bDiffMarked ? CkStyle::Accent() : CkStyle::Text();
    }
}

// =====================================================================================================================

auto SCkInspector_OverlapBodyAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _EnabledDiffMarked = InArgs._EnabledDiffMarked;
    _StateDiffMarked = InArgs._StateDiffMarked;
    _ShapeDiffMarked = InArgs._ShapeDiffMarked;
    _MarkerOverlapsDiffMarked = InArgs._MarkerOverlapsDiffMarked;
    _NonMarkerOverlapsDiffMarked = InArgs._NonMarkerOverlapsDiffMarked;

    const TSharedRef<SBox> RootHost = SNew(SBox);
    ChildSlot[RootHost];
    if (Build_AuthoredView())
    {
        RootHost->SetContent(_View->GetRegion(TEXT("main")));
        return;
    }
    Release();
    RootHost->SetContent(SNullWidget::NullWidget);
}

SCkInspector_OverlapBodyAuthored::~SCkInspector_OverlapBodyAuthored()
{
    Release();
}

auto SCkInspector_OverlapBodyAuthored::Get_HasMarker() const -> bool
{
    return _Active && ck_inspector_overlapbody::Has_Marker(_Entity);
}

auto SCkInspector_OverlapBodyAuthored::Get_HasSensor() const -> bool
{
    return _Active && ck_inspector_overlapbody::Has_Sensor(_Entity);
}

auto SCkInspector_OverlapBodyAuthored::Get_MarkerEnabled() const -> bool
{
    return Get_HasMarker()
        && ck_inspector_overlapbody::Get_MarkerState(_Entity) == ECk_EnableDisable::Enable;
}

auto SCkInspector_OverlapBodyAuthored::Get_SensorEnabled() const -> bool
{
    return Get_HasSensor()
        && ck_inspector_overlapbody::Get_SensorState(_Entity) == ECk_EnableDisable::Enable;
}

auto SCkInspector_OverlapBodyAuthored::Get_MarkerCanToggle() const -> bool
{
    return _Active && ck_inspector_overlapbody::Get_MarkerGate(_Entity).IsEnabled;
}

auto SCkInspector_OverlapBodyAuthored::Get_SensorCanToggle() const -> bool
{
    return _Active && ck_inspector_overlapbody::Get_SensorGate(_Entity).IsEnabled;
}

auto SCkInspector_OverlapBodyAuthored::Get_MarkerDisabledReason() const -> FString
{
    return _Active ? ck_inspector_overlapbody::Get_MarkerGate(_Entity).Reason.ToString() : FString{};
}

auto SCkInspector_OverlapBodyAuthored::Get_SensorDisabledReason() const -> FString
{
    return _Active ? ck_inspector_overlapbody::Get_SensorGate(_Entity).Reason.ToString() : FString{};
}

auto SCkInspector_OverlapBodyAuthored::Get_MarkerStateText() const -> FString
{
    return Get_HasMarker()
        ? ck::Format_UE(TEXT("{}"), ck_inspector_overlapbody::Get_MarkerState(_Entity)) : TEXT("--");
}

auto SCkInspector_OverlapBodyAuthored::Get_SensorStateText() const -> FString
{
    return Get_HasSensor()
        ? ck::Format_UE(TEXT("{}"), ck_inspector_overlapbody::Get_SensorState(_Entity)) : TEXT("--");
}

auto SCkInspector_OverlapBodyAuthored::Get_MarkerShapeText() const -> FString
{
    return Get_HasMarker()
        ? ck_inspector_overlapbody::Make_ShapeText(ck_inspector_overlapbody::Get_MarkerShapeIsValid(_Entity)).ToString()
        : TEXT("--");
}

auto SCkInspector_OverlapBodyAuthored::Get_SensorShapeText() const -> FString
{
    return Get_HasSensor()
        ? ck_inspector_overlapbody::Make_ShapeText(ck_inspector_overlapbody::Get_SensorShapeIsValid(_Entity)).ToString()
        : TEXT("--");
}

auto SCkInspector_OverlapBodyAuthored::Get_MarkerStateForeground() const -> FLinearColor
{
    return Get_HasMarker()
        ? CkStyle::GetToneColor(ck_inspector_overlapbody::Get_EnableDisableTone(
            ck_inspector_overlapbody::Get_MarkerState(_Entity))) : FLinearColor::Transparent;
}

auto SCkInspector_OverlapBodyAuthored::Get_MarkerStateBackground() const -> FLinearColor
{
    return Get_HasMarker()
        ? CkStyle::GetToneDimColor(ck_inspector_overlapbody::Get_EnableDisableTone(
            ck_inspector_overlapbody::Get_MarkerState(_Entity))) : FLinearColor::Transparent;
}

auto SCkInspector_OverlapBodyAuthored::Get_SensorStateForeground() const -> FLinearColor
{
    return Get_HasSensor()
        ? CkStyle::GetToneColor(ck_inspector_overlapbody::Get_EnableDisableTone(
            ck_inspector_overlapbody::Get_SensorState(_Entity))) : FLinearColor::Transparent;
}

auto SCkInspector_OverlapBodyAuthored::Get_SensorStateBackground() const -> FLinearColor
{
    return Get_HasSensor()
        ? CkStyle::GetToneDimColor(ck_inspector_overlapbody::Get_EnableDisableTone(
            ck_inspector_overlapbody::Get_SensorState(_Entity))) : FLinearColor::Transparent;
}

auto SCkInspector_OverlapBodyAuthored::Get_MarkerShapeForeground() const -> FLinearColor
{
    return Get_HasMarker()
        ? CkStyle::GetToneColor(ck_inspector_overlapbody::Get_ShapeTone(
            ck_inspector_overlapbody::Get_MarkerShapeIsValid(_Entity))) : FLinearColor::Transparent;
}

auto SCkInspector_OverlapBodyAuthored::Get_MarkerShapeBackground() const -> FLinearColor
{
    return Get_HasMarker()
        ? CkStyle::GetToneDimColor(ck_inspector_overlapbody::Get_ShapeTone(
            ck_inspector_overlapbody::Get_MarkerShapeIsValid(_Entity))) : FLinearColor::Transparent;
}

auto SCkInspector_OverlapBodyAuthored::Get_SensorShapeForeground() const -> FLinearColor
{
    return Get_HasSensor()
        ? CkStyle::GetToneColor(ck_inspector_overlapbody::Get_ShapeTone(
            ck_inspector_overlapbody::Get_SensorShapeIsValid(_Entity))) : FLinearColor::Transparent;
}

auto SCkInspector_OverlapBodyAuthored::Get_SensorShapeBackground() const -> FLinearColor
{
    return Get_HasSensor()
        ? CkStyle::GetToneDimColor(ck_inspector_overlapbody::Get_ShapeTone(
            ck_inspector_overlapbody::Get_SensorShapeIsValid(_Entity))) : FLinearColor::Transparent;
}

auto SCkInspector_OverlapBodyAuthored::Get_MarkerOverlapCountText() const -> FString
{
    return Get_HasSensor()
        ? ck::Format_UE(TEXT("{}"), ck_inspector_overlapbody::Get_MarkerOverlapCount(_Entity)) : TEXT("0");
}

auto SCkInspector_OverlapBodyAuthored::Get_NonMarkerOverlapCountText() const -> FString
{
    return Get_HasSensor()
        ? ck::Format_UE(TEXT("{}"), ck_inspector_overlapbody::Get_NonMarkerOverlapCount(_Entity)) : TEXT("0");
}

auto SCkInspector_OverlapBodyAuthored::Set_MarkerEnabled(const bool InIsEnabled) -> void
{
    auto Marker = FCk_Handle_Marker{};
    if (NOT _Active || NOT ck_inspector_overlapbody::TryGet_Marker(_Entity, Marker)
        || NOT ck_inspector_overlapbody::Get_MarkerGate(_Entity).IsEnabled)
    { return; }
    UCk_Utils_Marker_UE::Request_EnableDisable(Marker,
        FCk_Request_Marker_EnableDisable{InIsEnabled ? ECk_EnableDisable::Enable : ECk_EnableDisable::Disable}, {});
}

auto SCkInspector_OverlapBodyAuthored::Set_SensorEnabled(const bool InIsEnabled) -> void
{
    auto Sensor = FCk_Handle_Sensor{};
    if (NOT _Active || NOT ck_inspector_overlapbody::TryGet_Sensor(_Entity, Sensor)
        || NOT ck_inspector_overlapbody::Get_SensorGate(_Entity).IsEnabled)
    { return; }
    UCk_Utils_Sensor_UE::Request_EnableDisable(Sensor,
        FCk_Request_Sensor_EnableDisable{InIsEnabled ? ECk_EnableDisable::Enable : ECk_EnableDisable::Disable}, {});
}

auto SCkInspector_OverlapBodyAuthored::Build_AuthoredView() -> bool
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

    const TWeakPtr<SCkInspector_OverlapBodyAuthored> WeakWidget{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    const auto BindText = [&Data, WeakWidget](const FString& InName,
        FString (SCkInspector_OverlapBodyAuthored::* InGetter)() const)
    {
        Data.Text.Add(InName, TAttribute<FText>::CreateLambda([WeakWidget, InGetter]()
        {
            const TSharedPtr<SCkInspector_OverlapBodyAuthored> Widget = WeakWidget.Pin();
            return Widget.IsValid() && NOT Widget->Is_Inert()
                ? FText::FromString((Widget.Get()->*InGetter)()) : FText::GetEmpty();
        }));
    };
    BindText(TEXT("overlap-marker-state"), &SCkInspector_OverlapBodyAuthored::Get_MarkerStateText);
    BindText(TEXT("overlap-marker-shape"), &SCkInspector_OverlapBodyAuthored::Get_MarkerShapeText);
    BindText(TEXT("overlap-sensor-state"), &SCkInspector_OverlapBodyAuthored::Get_SensorStateText);
    BindText(TEXT("overlap-sensor-shape"), &SCkInspector_OverlapBodyAuthored::Get_SensorShapeText);
    BindText(TEXT("overlap-marker-count"), &SCkInspector_OverlapBodyAuthored::Get_MarkerOverlapCountText);
    BindText(TEXT("overlap-non-marker-count"), &SCkInspector_OverlapBodyAuthored::Get_NonMarkerOverlapCountText);
    BindText(TEXT("overlap-marker-disabled-reason"), &SCkInspector_OverlapBodyAuthored::Get_MarkerDisabledReason);
    BindText(TEXT("overlap-sensor-disabled-reason"), &SCkInspector_OverlapBodyAuthored::Get_SensorDisabledReason);

    Data.Visibility.Add(TEXT("overlap-has-marker"), TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_HasMarker(); }));
    Data.Visibility.Add(TEXT("overlap-has-sensor"), TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_HasSensor(); }));
    Data.Visibility.Add(TEXT("overlap-marker-enabled"), TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_MarkerEnabled(); }));
    Data.Visibility.Add(TEXT("overlap-sensor-enabled"), TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_SensorEnabled(); }));
    Data.Visibility.Add(TEXT("overlap-marker-can-toggle"), TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_MarkerCanToggle(); }));
    Data.Visibility.Add(TEXT("overlap-sensor-can-toggle"), TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_SensorCanToggle(); }));
    Data.BoolChanged.Add(TEXT("overlap-marker-enabled-changed"), FCkUiOnBoolChanged::CreateLambda(
        [WeakWidget](const bool InValue)
        { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Set_MarkerEnabled(InValue); } }));
    Data.BoolChanged.Add(TEXT("overlap-sensor-enabled-changed"), FCkUiOnBoolChanged::CreateLambda(
        [WeakWidget](const bool InValue)
        { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Set_SensorEnabled(InValue); } }));
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && (Widget->Get_HasMarker() || Widget->Get_HasSensor()); });

    const auto BindColor = [&Data, WeakWidget](const FString& InName,
        FLinearColor (SCkInspector_OverlapBodyAuthored::* InGetter)() const)
    {
        Data.Color.Add(InName, TAttribute<FLinearColor>::CreateLambda([WeakWidget, InGetter]()
        {
            const auto Widget = WeakWidget.Pin();
            return Widget.IsValid() ? (Widget.Get()->*InGetter)() : FLinearColor::Transparent;
        }));
    };
    BindColor(TEXT("overlap-marker-state-foreground"), &SCkInspector_OverlapBodyAuthored::Get_MarkerStateForeground);
    BindColor(TEXT("overlap-marker-state-background"), &SCkInspector_OverlapBodyAuthored::Get_MarkerStateBackground);
    BindColor(TEXT("overlap-marker-shape-foreground"), &SCkInspector_OverlapBodyAuthored::Get_MarkerShapeForeground);
    BindColor(TEXT("overlap-marker-shape-background"), &SCkInspector_OverlapBodyAuthored::Get_MarkerShapeBackground);
    BindColor(TEXT("overlap-sensor-state-foreground"), &SCkInspector_OverlapBodyAuthored::Get_SensorStateForeground);
    BindColor(TEXT("overlap-sensor-state-background"), &SCkInspector_OverlapBodyAuthored::Get_SensorStateBackground);
    BindColor(TEXT("overlap-sensor-shape-foreground"), &SCkInspector_OverlapBodyAuthored::Get_SensorShapeForeground);
    BindColor(TEXT("overlap-sensor-shape-background"), &SCkInspector_OverlapBodyAuthored::Get_SensorShapeBackground);
    Data.Color.Add(TEXT("overlap-count-foreground"), CkStyle::GetToneColor(ECk_Tone::Info));
    Data.Color.Add(TEXT("overlap-count-background"), CkStyle::GetToneDimColor(ECk_Tone::Info));

    const auto BindDiff = [&Data, WeakWidget](const FString& InName,
        const bool SCkInspector_OverlapBodyAuthored::* InMember)
    {
        Data.Color.Add(InName, TAttribute<FLinearColor>::CreateLambda([WeakWidget, InMember]()
        {
            const auto Widget = WeakWidget.Pin();
            return Widget.IsValid() && NOT Widget->Is_Inert()
                ? ck_inspector_overlapbody::DiffColor(Widget.Get()->*InMember) : FLinearColor::Transparent;
        }));
    };
    BindDiff(TEXT("overlap-enabled-diff-color"), &SCkInspector_OverlapBodyAuthored::_EnabledDiffMarked);
    BindDiff(TEXT("overlap-state-diff-color"), &SCkInspector_OverlapBodyAuthored::_StateDiffMarked);
    BindDiff(TEXT("overlap-shape-diff-color"), &SCkInspector_OverlapBodyAuthored::_ShapeDiffMarked);
    BindDiff(TEXT("overlap-marker-count-diff-color"), &SCkInspector_OverlapBodyAuthored::_MarkerOverlapsDiffMarked);
    BindDiff(TEXT("overlap-non-marker-count-diff-color"), &SCkInspector_OverlapBodyAuthored::_NonMarkerOverlapsDiffMarked);

    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        {}, {}, {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString ResourceRoot = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(
        FPaths::Combine(ResourceRoot, TEXT("EcsInspectorOverlapBody.ui.html")),
        FPaths::Combine(ResourceRoot, TEXT("EcsInspectorOverlapBody.ui.css")));
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

auto SCkInspector_OverlapBodyAuthored::Tick(
    const FGeometry& InAllottedGeometry,
    const double InCurrentTime,
    const float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    if (NOT _Active || NOT _View.IsValid()) { return; }
    _View->PollFiles();
    if (NOT _View->GetLastResult().Succeeded)
    { _LoadError = FString::Join(_View->GetLastResult().Errors, TEXT("\n")); }
    else { _LoadError.Reset(); }
}

auto SCkInspector_OverlapBodyAuthored::Release() -> void
{
    if (NOT _Active) { return; }
    _Active = false;
    _Entity = {};
    _View.Reset();
    _Mounted = false;
}

// =====================================================================================================================

auto FCkInspector_OverlapBody::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Overlap Body"));
}

auto FCkInspector_OverlapBody::CanInspect(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity))
    { return false; }

    return Entity.Has_Any<
        ck::FFragment_Marker_Current,
        ck::FFragment_Sensor_Current>();
}

// =====================================================================================================================

auto FCkInspector_OverlapBody::Build_NativeBody(const FCk_Handle& Entity) const -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();

    auto MutableEntity = Entity;

    // ---- Marker ----
    if (Entity.Has<ck::FFragment_Marker_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Marker")));

        const auto CapturedEntity = Entity;
        const auto CapturedMarker = UCk_Utils_Marker_UE::Cast(MutableEntity);

        // Enable/disable is the one arg-free write a marker has that isn't a nested shape payload
        // (Request_Resize takes the shape variant, which is SKIP tier). The switch reads the live
        // Current fragment, so a gameplay-side flip is reflected without a rebuild.
        Builder.AddToggleRow(
            FText::FromString(TEXT("Enabled:")),
            TAttribute<bool>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Marker_Current>())
                { return false; }
                return CapturedEntity.Get<ck::FFragment_Marker_Current>().Get_EnableDisable() == ECk_EnableDisable::Enable;
            }),
            [CapturedMarker](bool InIsEnabled)
            {
                auto Mutable = CapturedMarker;
                if (ck::Is_NOT_Valid(Mutable)) { return; }

                UCk_Utils_Marker_UE::Request_EnableDisable(Mutable,
                    FCk_Request_Marker_EnableDisable{InIsEnabled ? ECk_EnableDisable::Enable : ECk_EnableDisable::Disable},
                    {});
            });

        Builder.AddStatusPillRow(
            FText::FromString(TEXT("State:")),
            TAttribute<FText>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Marker_Current>())
                { return FText::FromString(TEXT("--")); }
                const auto State = CapturedEntity.Get<ck::FFragment_Marker_Current>().Get_EnableDisable();
                return FText::FromString(ck::Format_UE(TEXT("{}"), State));
            }),
            TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Marker_Current>())
                { return ECk_Tone::Neutral; }
                return ck_inspector_overlapbody::Get_EnableDisableTone(
                    CapturedEntity.Get<ck::FFragment_Marker_Current>().Get_EnableDisable());
            }));

        Builder.AddStatusPillRow(
            FText::FromString(TEXT("Shape:")),
            TAttribute<FText>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Marker_Current>())
                { return FText::FromString(TEXT("--")); }
                return ck_inspector_overlapbody::Make_ShapeText(
                    CapturedEntity.Get<ck::FFragment_Marker_Current>().Get_Marker().IsValid());
            }),
            TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Marker_Current>())
                { return ECk_Tone::Neutral; }
                return ck_inspector_overlapbody::Get_ShapeTone(
                    CapturedEntity.Get<ck::FFragment_Marker_Current>().Get_Marker().IsValid());
            }));
    }

    // ---- Sensor ----
    if (Entity.Has<ck::FFragment_Sensor_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Sensor")));

        const auto CapturedEntity = Entity;
        const auto CapturedSensor = UCk_Utils_Sensor_UE::Cast(MutableEntity);

        Builder.AddToggleRow(
            FText::FromString(TEXT("Enabled:")),
            TAttribute<bool>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Sensor_Current>())
                { return false; }
                return CapturedEntity.Get<ck::FFragment_Sensor_Current>().Get_EnableDisable() == ECk_EnableDisable::Enable;
            }),
            [CapturedSensor](bool InIsEnabled)
            {
                auto Mutable = CapturedSensor;
                if (ck::Is_NOT_Valid(Mutable)) { return; }

                UCk_Utils_Sensor_UE::Request_EnableDisable(Mutable,
                    FCk_Request_Sensor_EnableDisable{InIsEnabled ? ECk_EnableDisable::Enable : ECk_EnableDisable::Disable},
                    {});
            });

        Builder.AddStatusPillRow(
            FText::FromString(TEXT("State:")),
            TAttribute<FText>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Sensor_Current>())
                { return FText::FromString(TEXT("--")); }
                const auto State = CapturedEntity.Get<ck::FFragment_Sensor_Current>().Get_EnableDisable();
                return FText::FromString(ck::Format_UE(TEXT("{}"), State));
            }),
            TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Sensor_Current>())
                { return ECk_Tone::Neutral; }
                return ck_inspector_overlapbody::Get_EnableDisableTone(
                    CapturedEntity.Get<ck::FFragment_Sensor_Current>().Get_EnableDisable());
            }));

        Builder.AddStatusPillRow(
            FText::FromString(TEXT("Shape:")),
            TAttribute<FText>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Sensor_Current>())
                { return FText::FromString(TEXT("--")); }
                return ck_inspector_overlapbody::Make_ShapeText(
                    CapturedEntity.Get<ck::FFragment_Sensor_Current>().Get_Sensor().IsValid());
            }),
            TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Sensor_Current>())
                { return ECk_Tone::Neutral; }
                return ck_inspector_overlapbody::Get_ShapeTone(
                    CapturedEntity.Get<ck::FFragment_Sensor_Current>().Get_Sensor().IsValid());
            }));

        Builder.AddCountBadgeRow(
            FText::FromString(TEXT("Marker Overlaps:")),
            TAttribute<int32>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Sensor_Current>())
                { return 0; }
                return CapturedEntity.Get<ck::FFragment_Sensor_Current>()
                    .Get_CurrentMarkerOverlaps().Get_Overlaps().Num();
            }),
            ECk_Tone::Info);

        Builder.AddCountBadgeRow(
            FText::FromString(TEXT("Non-Marker Overlaps:")),
            TAttribute<int32>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Sensor_Current>())
                { return 0; }
                return CapturedEntity.Get<ck::FFragment_Sensor_Current>()
                    .Get_CurrentNonMarkerOverlaps().Get_Overlaps().Num();
            }),
            ECk_Tone::Info);
    }

    return Builder.Build(Entity);
}

// =====================================================================================================================

FCkInspector_OverlapBody::~FCkInspector_OverlapBody()
{
    OnDeactivated();
}

auto FCkInspector_OverlapBody::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> NativeBody = Build_NativeBody(Entity);
    if (FCkInspector_RowCaptureScope::Is_Active()) { return NativeBody; }

    const TSharedRef<SCkInspector_OverlapBodyAuthored> Authored = SNew(SCkInspector_OverlapBodyAuthored)
        .Entity(Entity)
        .EnabledDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Enabled:")))
        .StateDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("State:")))
        .ShapeDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Shape:")))
        .MarkerOverlapsDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Marker Overlaps:")))
        .NonMarkerOverlapsDiffMarked(FCkInspector_DiffMarkScope::Is_LabelMarked(TEXT("Non-Marker Overlaps:")));
    if (NOT Authored->Is_Mounted())
    {
        _LastAuthoredLoadError = Authored->Get_LoadError();
        return NativeBody;
    }
    _LastAuthoredLoadError.Reset();
    _AuthoredInstances.Add(Authored);
    return Authored;
}

auto FCkInspector_OverlapBody::Tick(const FCk_Handle&, const float) -> void
{
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_OverlapBodyAuthored>& InInstance)
    { return NOT InInstance.IsValid() || InInstance.Pin()->Is_Inert(); });
}

auto FCkInspector_OverlapBody::OnDeactivated() -> void
{
    for (const TWeakPtr<SCkInspector_OverlapBodyAuthored>& WeakInstance : _AuthoredInstances)
    {
        if (const TSharedPtr<SCkInspector_OverlapBodyAuthored> Instance = WeakInstance.Pin(); Instance.IsValid())
        { Instance->Release(); }
    }
    _AuthoredInstances.Reset();
}

// =====================================================================================================================
