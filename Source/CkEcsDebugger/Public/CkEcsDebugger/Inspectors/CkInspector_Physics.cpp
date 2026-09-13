#include "CkInspector_Physics.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkPhysics/Acceleration/CkAcceleration_Fragment.h"
#include "CkPhysics/Acceleration/CkAcceleration_Utils.h"
#include "CkPhysics/EulerIntegrator/CkEulerIntegrator_Fragment.h"
#include "CkPhysics/EulerIntegrator/CkEulerIntegrator_Utils.h"
#include "CkPhysics/PredictedVelocity/CkPredictedVelocity_Fragment.h"
#include "CkPhysics/Velocity/CkVelocity_Fragment.h"
#include "CkPhysics/Velocity/CkVelocity_Utils.h"

#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Utils/CkDebug_InspectorEditGuard.h"
#include "CkDebuggerCommon/Utils/CkDebug_RequestGate.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NumericEditor.h"
#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkSlateLayout/CkUiFloatSeries.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Physics)

namespace ck_inspector_physics
{
    enum : uint8
    {
        VelocityBit = 1 << 0,
        AccelerationBit = 1 << 1,
        PredictedVelocityBit = 1 << 2,
        EulerIntegratorBit = 1 << 3,
    };

    auto Get_StructureMask(const FCk_Handle& InEntity) -> uint8
    {
        if (ck::Is_NOT_Valid(InEntity)) { return 0; }
        uint8 Result = 0;
        if (InEntity.Has<ck::FFragment_Velocity_Current>()) { Result |= VelocityBit; }
        if (InEntity.Has<ck::FFragment_Acceleration_Current>()) { Result |= AccelerationBit; }
        if (InEntity.Has<ck::FFragment_PredictedVelocity_Current>()) { Result |= PredictedVelocityBit; }
        if (InEntity.Has<ck::FFragment_EulerIntegrator_Current>()) { Result |= EulerIntegratorBit; }
        return Result;
    }

    auto Get_Vector(const FCk_Handle& InEntity, const FString& InKind) -> FVector
    {
        if (ck::Is_NOT_Valid(InEntity)) { return FVector::ZeroVector; }
        if (InKind == TEXT("velocity") && InEntity.Has<ck::FFragment_Velocity_Current>())
        { return InEntity.Get<ck::FFragment_Velocity_Current>().Get_CurrentVelocity(); }
        if (InKind == TEXT("acceleration") && InEntity.Has<ck::FFragment_Acceleration_Current>())
        { return InEntity.Get<ck::FFragment_Acceleration_Current>().Get_CurrentAcceleration(); }
        if (InKind == TEXT("predicted-velocity") && InEntity.Has<ck::FFragment_PredictedVelocity_Current>())
        { return InEntity.Get<ck::FFragment_PredictedVelocity_Current>().Get_CurrentVelocity(); }
        if (InKind == TEXT("predicted-location") && InEntity.Has<ck::FFragment_PredictedVelocity_Current>())
        { return InEntity.Get<ck::FFragment_PredictedVelocity_Current>().Get_PreviousLocation(); }
        if (InKind == TEXT("distance-offset") && InEntity.Has<ck::FFragment_EulerIntegrator_Current>())
        { return InEntity.Get<ck::FFragment_EulerIntegrator_Current>().Get_DistanceOffset(); }
        return FVector::ZeroVector;
    }

    auto Make_AxisComponents(const FCk_Handle& InEntity, const FString InKind) -> TArray<TAttribute<FText>>
    {
        auto Components = TArray<TAttribute<FText>>{};
        Components.Reserve(3);
        for (int32 Axis = 0; Axis < 3; ++Axis)
        {
            Components.Emplace(TAttribute<FText>::CreateLambda([InEntity, InKind, Axis]()
            { return FText::FromString(ck::Format_UE(TEXT("{:.3f}"), Get_Vector(InEntity, InKind)[Axis])); }));
        }
        return Components;
    }

    auto Make_SpeedText(const FCk_Handle& InEntity, const FString InKind) -> TAttribute<FText>
    {
        return TAttribute<FText>::CreateLambda([InEntity, InKind]()
        { return FText::FromString(ck::Format_UE(TEXT("{:.2f}"), Get_Vector(InEntity, InKind).Size())); });
    }

    auto Make_SpeedSample(const FCk_Handle& InEntity, const FString InKind) -> TAttribute<float>
    {
        return TAttribute<float>::CreateLambda([InEntity, InKind]()
        { return static_cast<float>(Get_Vector(InEntity, InKind).Size()); });
    }

    auto AxisColor(const int32 InAxis) -> FLinearColor
    {
        switch (InAxis)
        {
            case 0: return CkStyle::AxisX();
            case 1: return CkStyle::AxisY();
            default: return CkStyle::AxisZ();
        }
    }

    auto DiffColor(const bool bMarked) -> FLinearColor
    { return bMarked ? CkStyle::Accent() : CkStyle::Text(); }
}

auto FCkInspector_Physics::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Physics"));
}

auto FCkInspector_Physics::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck_inspector_physics::Get_StructureMask(Entity) != 0;
}

auto FCkInspector_Physics::Build_NativeBody(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    namespace inspector = ck_inspector_physics;
    auto Builder = FCkInspectorWidgetBuilder();
    Builder.SetEditGuard(Get_EditGuard());
    const auto CapturedEntity = Entity;

    if (Entity.Has<ck::FFragment_Velocity_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Velocity")));
        Builder.AddAlignedNumericRow(FText::FromString(TEXT("Current:")),
            inspector::Make_AxisComponents(CapturedEntity, TEXT("velocity")));
        Builder.AddSparklineRow(FText::FromString(TEXT("Speed:")),
            inspector::Make_SpeedSample(CapturedEntity, TEXT("velocity")), ECk_Tone::Accent,
            inspector::Make_SpeedText(CapturedEntity, TEXT("velocity")));
        auto MutableEntity = Entity;
        const auto CapturedVelocity = UCk_Utils_Velocity_UE::Cast(MutableEntity);
        if (ck::IsValid(CapturedVelocity))
        {
            Builder.AddVectorRow(FText::FromString(TEXT("Override:")),
                TAttribute<FVector>::CreateLambda([CapturedVelocity]()
                {
                    return ck::IsValid(CapturedVelocity)
                        ? UCk_Utils_Velocity_UE::Get_CurrentVelocity(CapturedVelocity) : FVector::ZeroVector;
                }),
                [CapturedVelocity](const FVector& InVelocity)
                {
                    auto MutableVelocity = CapturedVelocity;
                    if (ck::IsValid(MutableVelocity))
                    { UCk_Utils_Velocity_UE::Request_OverrideVelocity(MutableVelocity, InVelocity, {}); }
                }, ECk_DebugRequest_Requirement::AuthorityOnly);
        }
    }

    if (Entity.Has<ck::FFragment_Acceleration_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Acceleration")));
        Builder.AddAlignedNumericRow(FText::FromString(TEXT("Current:")),
            inspector::Make_AxisComponents(CapturedEntity, TEXT("acceleration")));
        auto MutableEntity = Entity;
        const auto CapturedAcceleration = UCk_Utils_Acceleration_UE::Cast(MutableEntity);
        if (ck::IsValid(CapturedAcceleration))
        {
            Builder.AddVectorRow(FText::FromString(TEXT("Override:")),
                TAttribute<FVector>::CreateLambda([CapturedAcceleration]()
                {
                    return ck::IsValid(CapturedAcceleration)
                        ? UCk_Utils_Acceleration_UE::Get_CurrentAcceleration(CapturedAcceleration) : FVector::ZeroVector;
                }),
                [CapturedAcceleration](const FVector& InAcceleration)
                {
                    auto MutableAcceleration = CapturedAcceleration;
                    if (ck::IsValid(MutableAcceleration))
                    { UCk_Utils_Acceleration_UE::Request_OverrideAcceleration(MutableAcceleration, InAcceleration, {}); }
                }, ECk_DebugRequest_Requirement::AuthorityOnly);
        }
    }

    if (Entity.Has<ck::FFragment_PredictedVelocity_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Predicted Velocity")));
        Builder.AddAlignedNumericRow(FText::FromString(TEXT("Velocity:")),
            inspector::Make_AxisComponents(CapturedEntity, TEXT("predicted-velocity")));
        Builder.AddSparklineRow(FText::FromString(TEXT("Speed:")),
            inspector::Make_SpeedSample(CapturedEntity, TEXT("predicted-velocity")), ECk_Tone::Info,
            inspector::Make_SpeedText(CapturedEntity, TEXT("predicted-velocity")));
        Builder.AddAlignedNumericRow(FText::FromString(TEXT("Prev Location:")),
            inspector::Make_AxisComponents(CapturedEntity, TEXT("predicted-location")));
        Builder.AddRow(FText::FromString(TEXT("Prev DeltaTime:")), [CapturedEntity](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedEntity)
                || NOT CapturedEntity.Has<ck::FFragment_PredictedVelocity_Current>())
            { return FText::FromString(TEXT("--")); }
            return FText::FromString(FString::Printf(TEXT("%.3f s"),
                CapturedEntity.Get<ck::FFragment_PredictedVelocity_Current>().Get_PreviousDeltaTime().Get_Seconds()));
        }, CkStyle::Value_Numeric());
    }

    if (Entity.Has<ck::FFragment_EulerIntegrator_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Euler Integrator")));
        Builder.AddAlignedNumericRow(FText::FromString(TEXT("Distance Offset:")),
            inspector::Make_AxisComponents(CapturedEntity, TEXT("distance-offset")));
        Builder.AddActionRow(FText::FromString(TEXT("Integrator:")),
        {
            FCkInspector_Action{
                FText::FromString(TEXT("Start")),
                FText::FromString(TEXT("UCk_Utils_EulerIntegrator_UE::Request_Start")),
                [CapturedEntity]()
                {
                    auto MutableEntity = CapturedEntity;
                    if (ck::IsValid(MutableEntity)) { UCk_Utils_EulerIntegrator_UE::Request_Start(MutableEntity, {}); }
                }, ECk_DebugRequest_Requirement::LocalOk},
            FCkInspector_Action{
                FText::FromString(TEXT("Stop")),
                FText::FromString(TEXT("UCk_Utils_EulerIntegrator_UE::Request_Stop")),
                [CapturedEntity]()
                {
                    auto MutableEntity = CapturedEntity;
                    if (ck::IsValid(MutableEntity)) { UCk_Utils_EulerIntegrator_UE::Request_Stop(MutableEntity, {}); }
                }, ECk_DebugRequest_Requirement::LocalOk},
        });
    }

    return Builder.Build(Entity);
}

auto SCkInspector_PhysicsAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _EditGuard = InArgs._EditGuard;
    _DiffLabels = InArgs._DiffLabels;
    const TSharedRef<SBox> Host = SNew(SBox);
    ChildSlot[Host];
    if (Build_AuthoredView())
    {
        Host->SetContent(_View->GetRegion(TEXT("main")));
        Push_SpeedSamples();
        return;
    }
    Release();
    Host->SetContent(SNullWidget::NullWidget);
}

SCkInspector_PhysicsAuthored::~SCkInspector_PhysicsAuthored()
{
    Release();
}

auto SCkInspector_PhysicsAuthored::Get_IsAvailable() const -> bool
{
    return _Active && ck_inspector_physics::Get_StructureMask(_Entity) != 0;
}

auto SCkInspector_PhysicsAuthored::Get_HasVelocity() const -> bool
{
    return _Active && ck::IsValid(_Entity) && _Entity.Has<ck::FFragment_Velocity_Current>();
}

auto SCkInspector_PhysicsAuthored::Get_HasAcceleration() const -> bool
{
    return _Active && ck::IsValid(_Entity) && _Entity.Has<ck::FFragment_Acceleration_Current>();
}

auto SCkInspector_PhysicsAuthored::Get_HasPredictedVelocity() const -> bool
{
    return _Active && ck::IsValid(_Entity) && _Entity.Has<ck::FFragment_PredictedVelocity_Current>();
}

auto SCkInspector_PhysicsAuthored::Get_HasEulerIntegrator() const -> bool
{
    return _Active && ck::IsValid(_Entity) && _Entity.Has<ck::FFragment_EulerIntegrator_Current>();
}

auto SCkInspector_PhysicsAuthored::Get_CanOverrideVelocity() const -> bool
{
    return Get_HasVelocity() && _Entity.Has<ck::FFragment_Velocity_Params>()
        && ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::AuthorityOnly).IsEnabled;
}

auto SCkInspector_PhysicsAuthored::Get_CanOverrideAcceleration() const -> bool
{
    return Get_HasAcceleration() && _Entity.Has<ck::FFragment_Acceleration_Params>()
        && ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::AuthorityOnly).IsEnabled;
}

auto SCkInspector_PhysicsAuthored::Get_CanStartIntegrator() const -> bool
{
    return _Active && ck::IsValid(_Entity) && NOT Get_HasEulerIntegrator()
        && ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::LocalOk).IsEnabled;
}

auto SCkInspector_PhysicsAuthored::Get_CanStopIntegrator() const -> bool
{
    return Get_HasEulerIntegrator()
        && ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::LocalOk).IsEnabled;
}

auto SCkInspector_PhysicsAuthored::Get_DisabledReason(
    const ECk_DebugRequest_Requirement InRequirement) const -> FString
{
    if (NOT _Active || ck::Is_NOT_Valid(_Entity)) { return TEXT("Physics entity is unavailable."); }
    return ck::DebugRequestGate::Evaluate(_Entity, InRequirement).Reason.ToString();
}

auto SCkInspector_PhysicsAuthored::Get_StartIntegratorDisabledReason() const -> FString
{
    if (NOT _Active || ck::Is_NOT_Valid(_Entity)) { return TEXT("Physics entity is unavailable."); }
    if (Get_HasEulerIntegrator()) { return TEXT("Euler integrator is already running."); }
    return Get_DisabledReason(ECk_DebugRequest_Requirement::LocalOk);
}

auto SCkInspector_PhysicsAuthored::Get_StopIntegratorDisabledReason() const -> FString
{
    if (NOT _Active || ck::Is_NOT_Valid(_Entity)) { return TEXT("Physics entity is unavailable."); }
    if (NOT Get_HasEulerIntegrator()) { return TEXT("Euler integrator is not running."); }
    return Get_DisabledReason(ECk_DebugRequest_Requirement::LocalOk);
}

auto SCkInspector_PhysicsAuthored::Get_Vector(const FString& InKind) const -> FVector
{
    if (NOT _Active) { return FVector::ZeroVector; }
    return ck_inspector_physics::Get_Vector(_Entity, InKind);
}

auto SCkInspector_PhysicsAuthored::Get_AxisText(const FString& InKind, const int32 InAxis) const -> FString
{
    const bool bPresent =
        (InKind == TEXT("velocity") && Get_HasVelocity())
        || (InKind == TEXT("acceleration") && Get_HasAcceleration())
        || ((InKind == TEXT("predicted-velocity") || InKind == TEXT("predicted-location"))
            && Get_HasPredictedVelocity())
        || (InKind == TEXT("distance-offset") && Get_HasEulerIntegrator());
    return bPresent && InAxis >= 0 && InAxis < 3
        ? ck::Format_UE(TEXT("{:.3f}"), Get_Vector(InKind)[InAxis]) : TEXT("--");
}

auto SCkInspector_PhysicsAuthored::Get_SpeedText(const FString& InKind) const -> FString
{
    const bool bPresent = InKind == TEXT("velocity") ? Get_HasVelocity() : Get_HasPredictedVelocity();
    return bPresent ? ck::Format_UE(TEXT("{:.2f}"), Get_Vector(InKind).Size()) : TEXT("--");
}

auto SCkInspector_PhysicsAuthored::Get_PreviousDeltaTimeText() const -> FString
{
    if (NOT Get_HasPredictedVelocity()) { return TEXT("--"); }
    return FString::Printf(TEXT("%.3f s"),
        _Entity.Get<ck::FFragment_PredictedVelocity_Current>().Get_PreviousDeltaTime().Get_Seconds());
}

auto SCkInspector_PhysicsAuthored::Commit_Velocity(const FVector& InValue) -> void
{
    if (NOT Get_CanOverrideVelocity()) { return; }
    auto Entity = _Entity;
    auto Velocity = UCk_Utils_Velocity_UE::Cast(Entity);
    if (ck::IsValid(Velocity)) { UCk_Utils_Velocity_UE::Request_OverrideVelocity(Velocity, InValue, {}); }
}

auto SCkInspector_PhysicsAuthored::Commit_Acceleration(const FVector& InValue) -> void
{
    if (NOT Get_CanOverrideAcceleration()) { return; }
    auto Entity = _Entity;
    auto Acceleration = UCk_Utils_Acceleration_UE::Cast(Entity);
    if (ck::IsValid(Acceleration))
    { UCk_Utils_Acceleration_UE::Request_OverrideAcceleration(Acceleration, InValue, {}); }
}

auto SCkInspector_PhysicsAuthored::Request_StartIntegrator() -> void
{
    if (NOT Get_CanStartIntegrator()) { return; }
    auto Entity = _Entity;
    UCk_Utils_EulerIntegrator_UE::Request_Start(Entity, {});
}

auto SCkInspector_PhysicsAuthored::Request_StopIntegrator() -> void
{
    if (NOT Get_CanStopIntegrator()) { return; }
    auto Entity = _Entity;
    UCk_Utils_EulerIntegrator_UE::Request_Stop(Entity, {});
}

auto SCkInspector_PhysicsAuthored::Present_EditControl(
    const TSharedRef<SWidget> InInput,
    const TAttribute<FText> InReadOnlyText) -> TSharedRef<SWidget>
{
    const FCkDebuggerStyleSelection& Selection = UCkDebuggerStyleSettings::Get_Selection();
    if (NOT ck::debug_axes::EditControls_AreVisible(Selection))
    { return SNew(STextBlock).Text(InReadOnlyText); }
    if (NOT ck::debug_axes::EditControls_RevealOnHover(Selection)) { return InInput; }

    const TSharedRef<SBox> HoverHost = SNew(SBox);
    const TWeakPtr<SBox> WeakHoverHost{HoverHost};
    const auto IsHovered = [WeakHoverHost]()
    { const auto Host = WeakHoverHost.Pin(); return Host.IsValid() && Host->IsHovered(); };
    const TSharedRef<STextBlock> ReadOnly = SNew(STextBlock).Text(InReadOnlyText);
    ReadOnly->SetVisibility(TAttribute<EVisibility>::CreateLambda([IsHovered]()
    { return IsHovered() ? EVisibility::Hidden : EVisibility::Visible; }));
    InInput->SetVisibility(TAttribute<EVisibility>::CreateLambda([IsHovered]()
    { return IsHovered() ? EVisibility::Visible : EVisibility::Hidden; }));
    HoverHost->SetContent(SNew(SOverlay) + SOverlay::Slot()[ReadOnly] + SOverlay::Slot()[InInput]);
    return HoverHost;
}

auto SCkInspector_PhysicsAuthored::Build_VectorValue(
    const FName InTagPrefix,
    TFunction<FVector()> InGet,
    TFunction<void(const FVector&)> InSet,
    TFunction<bool()> InIsEnabled,
    const ECk_DebugRequest_Requirement InRequirement) -> TSharedRef<SWidget>
{
    const TWeakPtr<SCkInspector_PhysicsAuthored> WeakWidget{SharedThis(this)};
    const FString Prefix = InTagPrefix.ToString();
    const TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        const TSharedPtr<FCkInspectorEditScope> Scope = MakeShared<FCkInspectorEditScope>(_EditGuard);
        _EditScopes.Add(Scope);
        const FName InputTag{FString::Printf(TEXT("%s-%c-input"), *Prefix, TEXT("xyz")[Axis])};
        const TSharedRef<SCkDebug_NumericEditor> Editor = SNew(SCkDebug_NumericEditor)
            .Tag(InputTag)
            .Value_Lambda([InGet, Axis]() { return static_cast<double>(InGet()[Axis]); })
            .Kind(ECkDebug_NumericKind::Float)
            .Width(72.0f)
            .ForegroundColor(ck_inspector_physics::AxisColor(Axis))
            .OnValueCommitted_Lambda([InGet, InSet, Axis](const double InValue)
            { auto Value = InGet(); Value[Axis] = InValue; InSet(Value); })
            .OnEditStateChanged_Lambda([Scope](const bool bEditing)
            { if (Scope.IsValid()) { Scope->Set_Active(bEditing); } });
        Editor->SetEnabled(TAttribute<bool>::CreateLambda([WeakWidget, InIsEnabled]()
        { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && NOT Widget->Is_Inert() && InIsEnabled(); }));
        Editor->SetToolTipText(TAttribute<FText>::CreateLambda([WeakWidget, InRequirement]()
        {
            const auto Widget = WeakWidget.Pin();
            return Widget.IsValid() ? FText::FromString(Widget->Get_DisabledReason(InRequirement)) : FText::GetEmpty();
        }));
        Row->AddSlot().AutoWidth().Padding(0.0f, 0.0f, CkStyle::SpaceXS, 0.0f)[Editor];
    }
    return Present_EditControl(Row, TAttribute<FText>::CreateLambda([InGet]()
    {
        const FVector Value = InGet();
        return FText::FromString(ck::Format_UE(TEXT("{:.3f}, {:.3f}, {:.3f}"), Value.X, Value.Y, Value.Z));
    }));
}

auto SCkInspector_PhysicsAuthored::Make_NativePort(TSharedRef<SWidget> InLeaf) -> TSharedRef<SBox>
{
    const TSharedRef<SBox> Port = SNew(SBox);
    Port->SetContent(MoveTemp(InLeaf));
    _NativePorts.Add(Port);
    return Port;
}

auto SCkInspector_PhysicsAuthored::Detach_NativePorts() -> void
{
    for (const TSharedPtr<SBox>& Port : _NativePorts)
    { if (Port.IsValid()) { Port->SetContent(SNullWidget::NullWidget); } }
    _NativePorts.Reset();
}

auto SCkInspector_PhysicsAuthored::Build_AuthoredView() -> bool
{
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    const FCkUiLoadResult VelocitySeriesResult = FCkUiFloatSeries::TryCreate({}, _VelocitySeries);
    const FCkUiLoadResult PredictedSeriesResult = FCkUiFloatSeries::TryCreate({}, _PredictedVelocitySeries);
    if (NOT RegistryResult.Succeeded || NOT Registry.IsValid() || NOT Plugin.IsValid()
        || NOT VelocitySeriesResult.Succeeded || NOT PredictedSeriesResult.Succeeded)
    {
        auto Errors = RegistryResult.Errors;
        Errors.Append(VelocitySeriesResult.Errors);
        Errors.Append(PredictedSeriesResult.Errors);
        if (NOT Registry.IsValid()) { Errors.Add(TEXT("Debugger UI registry is unavailable.")); }
        if (NOT Plugin.IsValid()) { Errors.Add(TEXT("CkDebugger plugin is unavailable.")); }
        _LoadError = FString::Join(Errors, TEXT("\n"));
        return false;
    }

    const TWeakPtr<SCkInspector_PhysicsAuthored> WeakWidget{SharedThis(this)};
    auto Ports = FCkUiView::FNativeBindings{};
    Ports.Add(TEXT("physics-velocity-override-port"), Make_NativePort(Build_VectorValue(
        TEXT("physics-velocity-override"),
        [WeakWidget]() { const auto Widget = WeakWidget.Pin(); return Widget.IsValid()
            ? Widget->Get_Vector(TEXT("velocity")) : FVector::ZeroVector; },
        [WeakWidget](const FVector& Value) { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid())
            { Widget->Commit_Velocity(Value); } },
        [WeakWidget]() { const auto Widget = WeakWidget.Pin(); return Widget.IsValid()
            && Widget->Get_CanOverrideVelocity(); }, ECk_DebugRequest_Requirement::AuthorityOnly)));
    Ports.Add(TEXT("physics-acceleration-override-port"), Make_NativePort(Build_VectorValue(
        TEXT("physics-acceleration-override"),
        [WeakWidget]() { const auto Widget = WeakWidget.Pin(); return Widget.IsValid()
            ? Widget->Get_Vector(TEXT("acceleration")) : FVector::ZeroVector; },
        [WeakWidget](const FVector& Value) { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid())
            { Widget->Commit_Acceleration(Value); } },
        [WeakWidget]() { const auto Widget = WeakWidget.Pin(); return Widget.IsValid()
            && Widget->Get_CanOverrideAcceleration(); }, ECk_DebugRequest_Requirement::AuthorityOnly)));

    auto Data = FCkUiView::FDataBindings{};
    const TArray<FString> Kinds = {
        TEXT("velocity"), TEXT("acceleration"), TEXT("predicted-velocity"),
        TEXT("predicted-location"), TEXT("distance-offset")};
    for (const FString& Kind : Kinds)
    {
        for (int32 Axis = 0; Axis < 3; ++Axis)
        {
            const FString Key = FString::Printf(TEXT("physics-%s-%c"), *Kind, TEXT("xyz")[Axis]);
            Data.Text.Add(Key, TAttribute<FText>::CreateLambda([WeakWidget, Kind, Axis]()
            {
                const auto Widget = WeakWidget.Pin();
                return Widget.IsValid() ? FText::FromString(Widget->Get_AxisText(Kind, Axis)) : FText::GetEmpty();
            }));
        }
    }
    Data.Text.Add(TEXT("physics-velocity-speed"), TAttribute<FText>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid()
        ? FText::FromString(Widget->Get_SpeedText(TEXT("velocity"))) : FText::GetEmpty(); }));
    Data.Text.Add(TEXT("physics-predicted-speed"), TAttribute<FText>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid()
        ? FText::FromString(Widget->Get_SpeedText(TEXT("predicted-velocity"))) : FText::GetEmpty(); }));
    Data.Text.Add(TEXT("physics-predicted-delta-time"), TAttribute<FText>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid()
        ? FText::FromString(Widget->Get_PreviousDeltaTimeText()) : FText::GetEmpty(); }));
    Data.Text.Add(TEXT("physics-integrator-start-label"), FText::FromString(TEXT("Start")));
    Data.Text.Add(TEXT("physics-integrator-stop-label"), FText::FromString(TEXT("Stop")));
    Data.Text.Add(TEXT("physics-integrator-start-tooltip"),
        FText::FromString(TEXT("UCk_Utils_EulerIntegrator_UE::Request_Start")));
    Data.Text.Add(TEXT("physics-integrator-stop-tooltip"),
        FText::FromString(TEXT("UCk_Utils_EulerIntegrator_UE::Request_Stop")));
    Data.Text.Add(TEXT("physics-integrator-start-disabled-reason"), TAttribute<FText>::CreateLambda([WeakWidget]()
    {
        const auto Widget = WeakWidget.Pin();
        return Widget.IsValid() ? FText::FromString(Widget->Get_StartIntegratorDisabledReason()) : FText::GetEmpty();
    }));
    Data.Text.Add(TEXT("physics-integrator-stop-disabled-reason"), TAttribute<FText>::CreateLambda([WeakWidget]()
    {
        const auto Widget = WeakWidget.Pin();
        return Widget.IsValid() ? FText::FromString(Widget->Get_StopIntegratorDisabledReason()) : FText::GetEmpty();
    }));
    Data.FloatSeries.Add(TEXT("physics-velocity-series"), _VelocitySeries);
    Data.FloatSeries.Add(TEXT("physics-predicted-series"), _PredictedVelocitySeries);
    Data.Visibility.Add(TEXT("physics-has-velocity"), TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_HasVelocity(); }));
    Data.Visibility.Add(TEXT("physics-has-acceleration"), TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_HasAcceleration(); }));
    Data.Visibility.Add(TEXT("physics-has-predicted"), TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_HasPredictedVelocity(); }));
    Data.Visibility.Add(TEXT("physics-has-integrator"), TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_HasEulerIntegrator(); }));
    Data.Visibility.Add(TEXT("physics-can-override-velocity"), TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_CanOverrideVelocity(); }));
    Data.Visibility.Add(TEXT("physics-can-override-acceleration"), TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_CanOverrideAcceleration(); }));
    Data.Visibility.Add(TEXT("physics-can-start-integrator"), TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_CanStartIntegrator(); }));
    Data.Visibility.Add(TEXT("physics-can-stop-integrator"), TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_CanStopIntegrator(); }));
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_IsAvailable(); });

    const auto BindDiff = [&Data, WeakWidget](const FString& InKey, const FString& InLabel)
    {
        Data.Color.Add(InKey, TAttribute<FLinearColor>::CreateLambda([WeakWidget, InLabel]()
        {
            const auto Widget = WeakWidget.Pin();
            return Widget.IsValid() && NOT Widget->Is_Inert()
                ? ck_inspector_physics::DiffColor(Widget->Is_DiffMarked(InLabel)) : FLinearColor::Transparent;
        }));
    };
    for (const TPair<FString, FString>& Binding : TArray<TPair<FString, FString>>{
        {TEXT("velocity-current"), TEXT("Current:")}, {TEXT("velocity-speed"), TEXT("Speed:")},
        {TEXT("velocity-override"), TEXT("Override:")}, {TEXT("acceleration-current"), TEXT("Current:")},
        {TEXT("acceleration-override"), TEXT("Override:")}, {TEXT("predicted-velocity"), TEXT("Velocity:")},
        {TEXT("predicted-speed"), TEXT("Speed:")}, {TEXT("predicted-location"), TEXT("Prev Location:")},
        {TEXT("predicted-delta-time"), TEXT("Prev DeltaTime:")},
        {TEXT("distance-offset"), TEXT("Distance Offset:")}, {TEXT("integrator"), TEXT("Integrator:")}})
    { BindDiff(TEXT("physics-") + Binding.Key + TEXT("-diff-color"), Binding.Value); }

    auto Actions = FCkUiView::FActions{};
    Actions.Add(TEXT("physics-integrator-start"), FSimpleDelegate::CreateLambda([WeakWidget]()
    { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Request_StartIntegrator(); } }));
    Actions.Add(TEXT("physics-integrator-stop"), FSimpleDelegate::CreateLambda([WeakWidget]()
    { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Request_StopIntegrator(); } }));

    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        MoveTemp(Ports), MoveTemp(Actions), {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString Root = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(FPaths::Combine(Root, TEXT("EcsInspectorPhysics.ui.html")),
        FPaths::Combine(Root, TEXT("EcsInspectorPhysics.ui.css")));
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

auto SCkInspector_PhysicsAuthored::Push_SpeedSamples() -> void
{
    constexpr int32 MaxSamples = 60;
    const auto Push = [](TArray<float>& InOutSamples, const float InValue)
    {
        InOutSamples.Add(InValue);
        if (InOutSamples.Num() > MaxSamples) { InOutSamples.RemoveAt(0, InOutSamples.Num() - MaxSamples); }
    };
    if (Get_HasVelocity()) { Push(_VelocitySamples, Get_Vector(TEXT("velocity")).Size()); }
    else { _VelocitySamples.Reset(); }
    if (Get_HasPredictedVelocity()) { Push(_PredictedVelocitySamples, Get_Vector(TEXT("predicted-velocity")).Size()); }
    else { _PredictedVelocitySamples.Reset(); }
    if (_VelocitySeries.IsValid()) { _VelocitySeries->TrySetSamples(_VelocitySamples); }
    if (_PredictedVelocitySeries.IsValid()) { _PredictedVelocitySeries->TrySetSamples(_PredictedVelocitySamples); }
}

auto SCkInspector_PhysicsAuthored::Tick(
    const FGeometry& InAllottedGeometry,
    const double InCurrentTime,
    const float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    if (NOT _Active || NOT _View.IsValid()) { return; }
    Push_SpeedSamples();
    _View->PollFiles();
    if (NOT _View->GetLastResult().Succeeded)
    { _LoadError = FString::Join(_View->GetLastResult().Errors, TEXT("\n")); }
    else { _LoadError.Reset(); }
}

auto SCkInspector_PhysicsAuthored::Release() -> void
{
    if (NOT _Active) { return; }
    _Active = false;
    for (const TSharedPtr<FCkInspectorEditScope>& Scope : _EditScopes)
    { if (Scope.IsValid()) { Scope->Set_Active(false); } }
    _EditScopes.Reset();
    Detach_NativePorts();
    _VelocitySeries.Reset();
    _PredictedVelocitySeries.Reset();
    _VelocitySamples.Reset();
    _PredictedVelocitySamples.Reset();
    _Entity = {};
    _EditGuard.Reset();
    _View.Reset();
    _Mounted = false;
}

auto FCkInspector_Physics::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> NativeBody = Build_NativeBody(Entity);
    if (FCkInspector_RowCaptureScope::Is_Active()) { return NativeBody; }
    _StructureMask = ck_inspector_physics::Get_StructureMask(Entity);

    auto DiffLabels = TSet<FString>{};
    for (const FString& Label : {TEXT("Current:"), TEXT("Speed:"), TEXT("Override:"), TEXT("Velocity:"),
        TEXT("Prev Location:"), TEXT("Prev DeltaTime:"), TEXT("Distance Offset:"), TEXT("Integrator:")})
    { if (FCkInspector_DiffMarkScope::Is_LabelMarked(Label)) { DiffLabels.Add(Label); } }

    const TSharedRef<SCkInspector_PhysicsAuthored> Authored = SNew(SCkInspector_PhysicsAuthored)
        .Entity(Entity)
        .EditGuard(Get_EditGuard())
        .DiffLabels(MoveTemp(DiffLabels));
    if (NOT Authored->Is_Mounted())
    {
        _LastAuthoredLoadError = Authored->Get_LoadError();
        return NativeBody;
    }
    _LastAuthoredLoadError.Reset();
    _AuthoredInstances.Add(Authored);
    return Authored;
}

auto FCkInspector_Physics::Tick(const FCk_Handle& Entity, const float InDeltaTime) -> void
{
    static_cast<void>(InDeltaTime);
    const uint8 StructureMask = ck_inspector_physics::Get_StructureMask(Entity);
    if (StructureMask != _StructureMask)
    {
        _StructureMask = StructureMask;
        RequestRebuild();
    }
    _LastAuthoredLoadError.Reset();
    for (const TWeakPtr<SCkInspector_PhysicsAuthored>& Weak : _AuthoredInstances)
    {
        if (const auto Instance = Weak.Pin(); Instance.IsValid() && NOT Instance->Get_LoadError().IsEmpty())
        {
            _LastAuthoredLoadError = Instance->Get_LoadError();
            break;
        }
    }
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_PhysicsAuthored>& InInstance)
    { return NOT InInstance.IsValid() || InInstance.Pin()->Is_Inert(); });
}

FCkInspector_Physics::~FCkInspector_Physics()
{
    OnDeactivated();
}

auto FCkInspector_Physics::OnDeactivated() -> void
{
    for (const TWeakPtr<SCkInspector_PhysicsAuthored>& Weak : _AuthoredInstances)
    { if (const auto Instance = Weak.Pin(); Instance.IsValid()) { Instance->Release(); } }
    _AuthoredInstances.Reset();
}
