#include "CkInspector_Shapes.h"

#include "CkCore/Validation/CkIsValid.h"

#include "CkShapes/Sphere/CkShapeSphere_Fragment.h"
#include "CkShapes/Box/CkShapeBox_Fragment.h"
#include "CkShapes/Capsule/CkShapeCapsule_Fragment.h"
#include "CkShapes/Cylinder/CkShapeCylinder_Fragment.h"

#include "CkShapes/Sphere/CkShapeSphere_Utils.h"
#include "CkShapes/Box/CkShapeBox_Utils.h"
#include "CkShapes/Capsule/CkShapeCapsule_Utils.h"
#include "CkShapes/Cylinder/CkShapeCylinder_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Utils/CkDebug_InspectorEditGuard.h"
#include "CkDebuggerCommon/Utils/CkDebug_RequestGate.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NumericEditor.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Shapes)

// =====================================================================================================================

namespace ck_inspector_shapes
{
    // A shape's dimensions are a VALUE: every Request_UpdateDimensions replaces the whole struct, so
    // editing one field has to re-read the others and write a full copy back. These helpers keep that
    // read-modify-write in one place per shape, with the typed handle captured BY VALUE and
    // ck::IsValid-checked on every fire (the row outlives the entity).

    // No shape dimension is meaningfully negative — a negative radius/extent is a malformed body, not
    // an experiment worth firing a request for.
    constexpr auto MinDimension = 0.0f;

    static auto Get_BoxHalfExtents(const FCk_Handle_ShapeBox& InBox) -> FVector
    {
        if (ck::Is_NOT_Valid(InBox))
        { return FVector::ZeroVector; }

        return UCk_Utils_ShapeBox_UE::Get_Dimensions(InBox).Get_HalfExtents();
    }

    auto TryGetSphere(const FCk_Handle& InEntity, FCk_Handle_ShapeSphere& OutSphere) -> bool
    {
        OutSphere = {};
        if (ck::Is_NOT_Valid(InEntity) || NOT UCk_Utils_ShapeSphere_UE::Has(InEntity)) { return false; }
        auto MutableEntity = InEntity;
        OutSphere = UCk_Utils_ShapeSphere_UE::Cast(MutableEntity);
        return ck::IsValid(OutSphere)
            && OutSphere.Has<ck::FFragment_ShapeSphere_Params>()
            && OutSphere.Has<ck::FFragment_ShapeSphere_Current>();
    }

    auto TryGetBox(const FCk_Handle& InEntity, FCk_Handle_ShapeBox& OutBox) -> bool
    {
        OutBox = {};
        if (ck::Is_NOT_Valid(InEntity) || NOT UCk_Utils_ShapeBox_UE::Has(InEntity)) { return false; }
        auto MutableEntity = InEntity;
        OutBox = UCk_Utils_ShapeBox_UE::Cast(MutableEntity);
        return ck::IsValid(OutBox)
            && OutBox.Has<ck::FFragment_ShapeBox_Params>()
            && OutBox.Has<ck::FFragment_ShapeBox_Current>();
    }

    auto TryGetCapsule(const FCk_Handle& InEntity, FCk_Handle_ShapeCapsule& OutCapsule) -> bool
    {
        OutCapsule = {};
        if (ck::Is_NOT_Valid(InEntity) || NOT UCk_Utils_ShapeCapsule_UE::Has(InEntity)) { return false; }
        auto MutableEntity = InEntity;
        OutCapsule = UCk_Utils_ShapeCapsule_UE::Cast(MutableEntity);
        return ck::IsValid(OutCapsule)
            && OutCapsule.Has<ck::FFragment_ShapeCapsule_Params>()
            && OutCapsule.Has<ck::FFragment_ShapeCapsule_Current>();
    }

    auto TryGetCylinder(const FCk_Handle& InEntity, FCk_Handle_ShapeCylinder& OutCylinder) -> bool
    {
        OutCylinder = {};
        if (ck::Is_NOT_Valid(InEntity) || NOT UCk_Utils_ShapeCylinder_UE::Has(InEntity)) { return false; }
        auto MutableEntity = InEntity;
        OutCylinder = UCk_Utils_ShapeCylinder_UE::Cast(MutableEntity);
        return ck::IsValid(OutCylinder)
            && OutCylinder.Has<ck::FFragment_ShapeCylinder_Params>()
            && OutCylinder.Has<ck::FFragment_ShapeCylinder_Current>();
    }

    auto GetLocalGate(const FCk_Handle& InEntity, const bool bFeatureAvailable) -> FCk_DebugRequest_GateVerdict
    {
        if (NOT bFeatureAvailable)
        { return FCk_DebugRequest_GateVerdict{false, FText::FromString(TEXT("Shape is unavailable."))}; }
        return ck::DebugRequestGate::Evaluate(InEntity, ECk_DebugRequest_Requirement::LocalOk);
    }

    auto DiffColor(const bool bDiffMarked) -> FLinearColor
    {
        return bDiffMarked ? CkStyle::Accent() : CkStyle::Text();
    }
}

// =====================================================================================================================

auto SCkInspector_ShapesAuthored::Get_HasSphere() const -> bool
{
    auto Sphere = FCk_Handle_ShapeSphere{};
    return _Active && ck_inspector_shapes::TryGetSphere(_Entity, Sphere);
}

auto SCkInspector_ShapesAuthored::Get_HasBox() const -> bool
{
    auto Box = FCk_Handle_ShapeBox{};
    return _Active && ck_inspector_shapes::TryGetBox(_Entity, Box);
}

auto SCkInspector_ShapesAuthored::Get_HasCapsule() const -> bool
{
    auto Capsule = FCk_Handle_ShapeCapsule{};
    return _Active && ck_inspector_shapes::TryGetCapsule(_Entity, Capsule);
}

auto SCkInspector_ShapesAuthored::Get_HasCylinder() const -> bool
{
    auto Cylinder = FCk_Handle_ShapeCylinder{};
    return _Active && ck_inspector_shapes::TryGetCylinder(_Entity, Cylinder);
}

auto SCkInspector_ShapesAuthored::Get_IsAvailable() const -> bool
{
    return Get_HasSphere() || Get_HasBox() || Get_HasCapsule() || Get_HasCylinder();
}

auto SCkInspector_ShapesAuthored::Get_CanRequest() const -> bool
{
    return _Active && ck_inspector_shapes::GetLocalGate(_Entity, Get_IsAvailable()).IsEnabled;
}

auto SCkInspector_ShapesAuthored::Get_RequestDisabledReason() const -> FString
{
    return _Active ? ck_inspector_shapes::GetLocalGate(_Entity, Get_IsAvailable()).Reason.ToString() : FString{};
}

auto SCkInspector_ShapesAuthored::Get_SphereRadius() const -> float
{
    auto Sphere = FCk_Handle_ShapeSphere{};
    return Get_HasSphere() && ck_inspector_shapes::TryGetSphere(_Entity, Sphere)
        ? UCk_Utils_ShapeSphere_UE::Get_Dimensions(Sphere).Get_Radius() : 0.0f;
}

auto SCkInspector_ShapesAuthored::Get_BoxHalfExtents() const -> FVector
{
    auto Box = FCk_Handle_ShapeBox{};
    return Get_HasBox() && ck_inspector_shapes::TryGetBox(_Entity, Box)
        ? ck_inspector_shapes::Get_BoxHalfExtents(Box) : FVector::ZeroVector;
}

auto SCkInspector_ShapesAuthored::Get_BoxConvexRadius() const -> float
{
    auto Box = FCk_Handle_ShapeBox{};
    return Get_HasBox() && ck_inspector_shapes::TryGetBox(_Entity, Box)
        ? UCk_Utils_ShapeBox_UE::Get_Dimensions(Box).Get_ConvexRadius() : 0.0f;
}

auto SCkInspector_ShapesAuthored::Get_CapsuleHalfHeight() const -> float
{
    auto Capsule = FCk_Handle_ShapeCapsule{};
    return Get_HasCapsule() && ck_inspector_shapes::TryGetCapsule(_Entity, Capsule)
        ? UCk_Utils_ShapeCapsule_UE::Get_Dimensions(Capsule).Get_HalfHeight() : 0.0f;
}

auto SCkInspector_ShapesAuthored::Get_CapsuleRadius() const -> float
{
    auto Capsule = FCk_Handle_ShapeCapsule{};
    return Get_HasCapsule() && ck_inspector_shapes::TryGetCapsule(_Entity, Capsule)
        ? UCk_Utils_ShapeCapsule_UE::Get_Dimensions(Capsule).Get_Radius() : 0.0f;
}

auto SCkInspector_ShapesAuthored::Get_CylinderHalfHeight() const -> float
{
    auto Cylinder = FCk_Handle_ShapeCylinder{};
    return Get_HasCylinder() && ck_inspector_shapes::TryGetCylinder(_Entity, Cylinder)
        ? UCk_Utils_ShapeCylinder_UE::Get_Dimensions(Cylinder).Get_HalfHeight() : 0.0f;
}

auto SCkInspector_ShapesAuthored::Get_CylinderRadius() const -> float
{
    auto Cylinder = FCk_Handle_ShapeCylinder{};
    return Get_HasCylinder() && ck_inspector_shapes::TryGetCylinder(_Entity, Cylinder)
        ? UCk_Utils_ShapeCylinder_UE::Get_Dimensions(Cylinder).Get_Radius() : 0.0f;
}

auto SCkInspector_ShapesAuthored::Get_CylinderConvexRadius() const -> float
{
    auto Cylinder = FCk_Handle_ShapeCylinder{};
    return Get_HasCylinder() && ck_inspector_shapes::TryGetCylinder(_Entity, Cylinder)
        ? UCk_Utils_ShapeCylinder_UE::Get_Dimensions(Cylinder).Get_ConvexRadius() : 0.0f;
}

auto SCkInspector_ShapesAuthored::Commit_SphereRadius(const float InValue) -> void
{
    auto Sphere = FCk_Handle_ShapeSphere{};
    if (NOT _Active || NOT ck_inspector_shapes::TryGetSphere(_Entity, Sphere)
        || NOT ck_inspector_shapes::GetLocalGate(_Entity, true).IsEnabled) { return; }
    UCk_Utils_ShapeSphere_UE::Request_UpdateDimensions(Sphere,
        FCk_Request_ShapeSphere_UpdateDimensions{FCk_ShapeSphere_Dimensions{InValue}}, {});
}

auto SCkInspector_ShapesAuthored::Commit_BoxHalfExtents(const FVector& InValue) -> void
{
    auto Box = FCk_Handle_ShapeBox{};
    if (NOT _Active || NOT ck_inspector_shapes::TryGetBox(_Entity, Box)
        || NOT ck_inspector_shapes::GetLocalGate(_Entity, true).IsEnabled) { return; }
    auto Dimensions = FCk_ShapeBox_Dimensions{InValue};
    Dimensions.Set_ConvexRadius(UCk_Utils_ShapeBox_UE::Get_Dimensions(Box).Get_ConvexRadius());
    UCk_Utils_ShapeBox_UE::Request_UpdateDimensions(Box, FCk_Request_ShapeBox_UpdateDimensions{Dimensions}, {});
}

auto SCkInspector_ShapesAuthored::Commit_BoxConvexRadius(const float InValue) -> void
{
    auto Box = FCk_Handle_ShapeBox{};
    if (NOT _Active || NOT ck_inspector_shapes::TryGetBox(_Entity, Box)
        || NOT ck_inspector_shapes::GetLocalGate(_Entity, true).IsEnabled) { return; }
    auto Dimensions = UCk_Utils_ShapeBox_UE::Get_Dimensions(Box);
    Dimensions.Set_ConvexRadius(InValue);
    UCk_Utils_ShapeBox_UE::Request_UpdateDimensions(Box, FCk_Request_ShapeBox_UpdateDimensions{Dimensions}, {});
}

auto SCkInspector_ShapesAuthored::Commit_CapsuleHalfHeight(const float InValue) -> void
{
    auto Capsule = FCk_Handle_ShapeCapsule{};
    if (NOT _Active || NOT ck_inspector_shapes::TryGetCapsule(_Entity, Capsule)
        || NOT ck_inspector_shapes::GetLocalGate(_Entity, true).IsEnabled) { return; }
    const float Radius = UCk_Utils_ShapeCapsule_UE::Get_Dimensions(Capsule).Get_Radius();
    UCk_Utils_ShapeCapsule_UE::Request_UpdateDimensions(Capsule,
        FCk_Request_ShapeCapsule_UpdateDimensions{FCk_ShapeCapsule_Dimensions{InValue, Radius}}, {});
}

auto SCkInspector_ShapesAuthored::Commit_CapsuleRadius(const float InValue) -> void
{
    auto Capsule = FCk_Handle_ShapeCapsule{};
    if (NOT _Active || NOT ck_inspector_shapes::TryGetCapsule(_Entity, Capsule)
        || NOT ck_inspector_shapes::GetLocalGate(_Entity, true).IsEnabled) { return; }
    const float HalfHeight = UCk_Utils_ShapeCapsule_UE::Get_Dimensions(Capsule).Get_HalfHeight();
    UCk_Utils_ShapeCapsule_UE::Request_UpdateDimensions(Capsule,
        FCk_Request_ShapeCapsule_UpdateDimensions{FCk_ShapeCapsule_Dimensions{HalfHeight, InValue}}, {});
}

auto SCkInspector_ShapesAuthored::Commit_CylinderHalfHeight(const float InValue) -> void
{
    auto Cylinder = FCk_Handle_ShapeCylinder{};
    if (NOT _Active || NOT ck_inspector_shapes::TryGetCylinder(_Entity, Cylinder)
        || NOT ck_inspector_shapes::GetLocalGate(_Entity, true).IsEnabled) { return; }
    const auto Current = UCk_Utils_ShapeCylinder_UE::Get_Dimensions(Cylinder);
    auto Dimensions = FCk_ShapeCylinder_Dimensions{InValue, Current.Get_Radius()};
    Dimensions.Set_ConvexRadius(Current.Get_ConvexRadius());
    UCk_Utils_ShapeCylinder_UE::Request_UpdateDimensions(Cylinder,
        FCk_Request_ShapeCylinder_UpdateDimensions{Dimensions}, {});
}

auto SCkInspector_ShapesAuthored::Commit_CylinderRadius(const float InValue) -> void
{
    auto Cylinder = FCk_Handle_ShapeCylinder{};
    if (NOT _Active || NOT ck_inspector_shapes::TryGetCylinder(_Entity, Cylinder)
        || NOT ck_inspector_shapes::GetLocalGate(_Entity, true).IsEnabled) { return; }
    const auto Current = UCk_Utils_ShapeCylinder_UE::Get_Dimensions(Cylinder);
    auto Dimensions = FCk_ShapeCylinder_Dimensions{Current.Get_HalfHeight(), InValue};
    Dimensions.Set_ConvexRadius(Current.Get_ConvexRadius());
    UCk_Utils_ShapeCylinder_UE::Request_UpdateDimensions(Cylinder,
        FCk_Request_ShapeCylinder_UpdateDimensions{Dimensions}, {});
}

auto SCkInspector_ShapesAuthored::Commit_CylinderConvexRadius(const float InValue) -> void
{
    auto Cylinder = FCk_Handle_ShapeCylinder{};
    if (NOT _Active || NOT ck_inspector_shapes::TryGetCylinder(_Entity, Cylinder)
        || NOT ck_inspector_shapes::GetLocalGate(_Entity, true).IsEnabled) { return; }
    auto Dimensions = UCk_Utils_ShapeCylinder_UE::Get_Dimensions(Cylinder);
    Dimensions.Set_ConvexRadius(InValue);
    UCk_Utils_ShapeCylinder_UE::Request_UpdateDimensions(Cylinder,
        FCk_Request_ShapeCylinder_UpdateDimensions{Dimensions}, {});
}

auto SCkInspector_ShapesAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _EditGuard = InArgs._EditGuard;
    _DiffLabels = InArgs._DiffLabels;

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

SCkInspector_ShapesAuthored::~SCkInspector_ShapesAuthored()
{
    Release();
}

auto SCkInspector_ShapesAuthored::Build_Number(
    const FName InTag,
    TFunction<float()> InGetter,
    TFunction<void(float)> InCommit,
    TFunction<bool()> InIsEnabled,
    const bool bClampNonNegative) -> TSharedRef<SWidget>
{
    const TWeakPtr<SCkInspector_ShapesAuthored> WeakWidget{SharedThis(this)};
    const TSharedPtr<FCkInspectorEditScope> EditScope = MakeShared<FCkInspectorEditScope>(_EditGuard);
    _EditScopes.Add(EditScope);

    const TSharedRef<SCkDebug_NumericEditor> Editor = SNew(SCkDebug_NumericEditor)
        .Tag(InTag)
        .Value_Lambda([InGetter = MoveTemp(InGetter)]() { return static_cast<double>(InGetter()); })
        .Kind(ECkDebug_NumericKind::Float)
        .MinValue(bClampNonNegative
            ? TOptional<double>{ck_inspector_shapes::MinDimension}
            : TOptional<double>{})
        .Width(72.0f)
        .ForegroundColor(CkStyle::Value_Numeric())
        .OnValueCommitted_Lambda([InCommit = MoveTemp(InCommit)](const double InValue)
        { InCommit(static_cast<float>(InValue)); })
        .OnEditStateChanged_Lambda([EditScope](const bool bIsEditing)
        { if (EditScope.IsValid()) { EditScope->Set_Active(bIsEditing); } });
    Editor->SetEnabled(TAttribute<bool>::CreateLambda([WeakWidget, InIsEnabled = MoveTemp(InIsEnabled)]()
    {
        const TSharedPtr<SCkInspector_ShapesAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && NOT Widget->Is_Inert() && InIsEnabled();
    }));
    Editor->SetToolTipText(TAttribute<FText>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_ShapesAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() ? FText::FromString(Widget->Get_RequestDisabledReason()) : FText::GetEmpty();
    }));
    return Editor;
}

auto SCkInspector_ShapesAuthored::Build_BoxHalfExtentsValue() -> TSharedRef<SWidget>
{
    const TWeakPtr<SCkInspector_ShapesAuthored> WeakWidget{SharedThis(this)};
    const auto BuildComponent = [this, WeakWidget](const FName InTag, const int32 InIndex) -> TSharedRef<SWidget>
    {
        return Build_Number(
            InTag,
            [WeakWidget, InIndex]()
            {
                const TSharedPtr<SCkInspector_ShapesAuthored> Widget = WeakWidget.Pin();
                return Widget.IsValid() ? Widget->Get_BoxHalfExtents()[InIndex] : 0.0f;
            },
            [WeakWidget, InIndex](const float InValue)
            {
                if (const TSharedPtr<SCkInspector_ShapesAuthored> Widget = WeakWidget.Pin(); Widget.IsValid())
                {
                    auto HalfExtents = Widget->Get_BoxHalfExtents();
                    HalfExtents[InIndex] = InValue;
                    Widget->Commit_BoxHalfExtents(HalfExtents);
                }
            },
            [WeakWidget]()
            {
                const TSharedPtr<SCkInspector_ShapesAuthored> Widget = WeakWidget.Pin();
                return Widget.IsValid() && Widget->Get_HasBox() && Widget->Get_CanRequest();
            },
            false);
    };

    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth()[BuildComponent(TEXT("shapes-box-half-extents-x-input"), 0)]
        + SHorizontalBox::Slot().AutoWidth()[BuildComponent(TEXT("shapes-box-half-extents-y-input"), 1)]
        + SHorizontalBox::Slot().AutoWidth()[BuildComponent(TEXT("shapes-box-half-extents-z-input"), 2)];
}

auto SCkInspector_ShapesAuthored::Make_NativePort(TSharedRef<SWidget> InLeaf) -> TSharedRef<SBox>
{
    const TSharedRef<SBox> Port = SNew(SBox);
    Port->SetContent(MoveTemp(InLeaf));
    _NativePorts.Add(Port);
    return Port;
}

auto SCkInspector_ShapesAuthored::Detach_NativePorts() -> void
{
    for (const TSharedPtr<SBox>& Port : _NativePorts)
    { if (Port.IsValid()) { Port->SetContent(SNullWidget::NullWidget); } }
    _NativePorts.Reset();
}

auto SCkInspector_ShapesAuthored::Build_AuthoredView() -> bool
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

    const TWeakPtr<SCkInspector_ShapesAuthored> WeakWidget{SharedThis(this)};
    FCkUiView::FNativeBindings NativeBindings;
    NativeBindings.Add(TEXT("shapes-sphere-radius-port"), Make_NativePort(Build_Number(
        TEXT("shapes-sphere-radius-input"),
        [WeakWidget]() { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() ? Widget->Get_SphereRadius() : 0.0f; },
        [WeakWidget](const float InValue) { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Commit_SphereRadius(InValue); } },
        [WeakWidget]() { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_HasSphere() && Widget->Get_CanRequest(); })));
    NativeBindings.Add(TEXT("shapes-box-half-extents-port"), Make_NativePort(Build_BoxHalfExtentsValue()));
    NativeBindings.Add(TEXT("shapes-box-convex-radius-port"), Make_NativePort(Build_Number(
        TEXT("shapes-box-convex-radius-input"),
        [WeakWidget]() { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() ? Widget->Get_BoxConvexRadius() : 0.0f; },
        [WeakWidget](const float InValue) { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Commit_BoxConvexRadius(InValue); } },
        [WeakWidget]() { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_HasBox() && Widget->Get_CanRequest(); })));
    NativeBindings.Add(TEXT("shapes-capsule-half-height-port"), Make_NativePort(Build_Number(
        TEXT("shapes-capsule-half-height-input"),
        [WeakWidget]() { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() ? Widget->Get_CapsuleHalfHeight() : 0.0f; },
        [WeakWidget](const float InValue) { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Commit_CapsuleHalfHeight(InValue); } },
        [WeakWidget]() { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_HasCapsule() && Widget->Get_CanRequest(); })));
    NativeBindings.Add(TEXT("shapes-capsule-radius-port"), Make_NativePort(Build_Number(
        TEXT("shapes-capsule-radius-input"),
        [WeakWidget]() { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() ? Widget->Get_CapsuleRadius() : 0.0f; },
        [WeakWidget](const float InValue) { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Commit_CapsuleRadius(InValue); } },
        [WeakWidget]() { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_HasCapsule() && Widget->Get_CanRequest(); })));
    NativeBindings.Add(TEXT("shapes-cylinder-half-height-port"), Make_NativePort(Build_Number(
        TEXT("shapes-cylinder-half-height-input"),
        [WeakWidget]() { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() ? Widget->Get_CylinderHalfHeight() : 0.0f; },
        [WeakWidget](const float InValue) { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Commit_CylinderHalfHeight(InValue); } },
        [WeakWidget]() { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_HasCylinder() && Widget->Get_CanRequest(); })));
    NativeBindings.Add(TEXT("shapes-cylinder-radius-port"), Make_NativePort(Build_Number(
        TEXT("shapes-cylinder-radius-input"),
        [WeakWidget]() { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() ? Widget->Get_CylinderRadius() : 0.0f; },
        [WeakWidget](const float InValue) { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Commit_CylinderRadius(InValue); } },
        [WeakWidget]() { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_HasCylinder() && Widget->Get_CanRequest(); })));
    NativeBindings.Add(TEXT("shapes-cylinder-convex-radius-port"), Make_NativePort(Build_Number(
        TEXT("shapes-cylinder-convex-radius-input"),
        [WeakWidget]() { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() ? Widget->Get_CylinderConvexRadius() : 0.0f; },
        [WeakWidget](const float InValue) { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Commit_CylinderConvexRadius(InValue); } },
        [WeakWidget]() { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_HasCylinder() && Widget->Get_CanRequest(); })));

    auto Data = FCkUiView::FDataBindings{};
    const auto BindVisibility = [&Data, WeakWidget](const FString& InName, bool (SCkInspector_ShapesAuthored::* InGetter)() const)
    {
        Data.Visibility.Add(InName, TAttribute<bool>::CreateLambda([WeakWidget, InGetter]()
        {
            const TSharedPtr<SCkInspector_ShapesAuthored> Widget = WeakWidget.Pin();
            return Widget.IsValid() && (Widget.Get()->*InGetter)();
        }));
    };
    BindVisibility(TEXT("shapes-sphere-visible"), &SCkInspector_ShapesAuthored::Get_HasSphere);
    BindVisibility(TEXT("shapes-box-visible"), &SCkInspector_ShapesAuthored::Get_HasBox);
    BindVisibility(TEXT("shapes-capsule-visible"), &SCkInspector_ShapesAuthored::Get_HasCapsule);
    BindVisibility(TEXT("shapes-cylinder-visible"), &SCkInspector_ShapesAuthored::Get_HasCylinder);
    const auto BindDiffColor = [&Data, WeakWidget](const FString& InName, const FString& InLabel)
    {
        Data.Color.Add(InName, TAttribute<FLinearColor>::CreateLambda([WeakWidget, InLabel]()
        {
            const TSharedPtr<SCkInspector_ShapesAuthored> Widget = WeakWidget.Pin();
            return Widget.IsValid() && NOT Widget->Is_Inert()
                ? ck_inspector_shapes::DiffColor(Widget->Is_DiffMarked(InLabel)) : FLinearColor::Transparent;
        }));
    };
    BindDiffColor(TEXT("shapes-sphere-radius-diff-color"), TEXT("Radius:"));
    BindDiffColor(TEXT("shapes-box-half-extents-diff-color"), TEXT("Half Extents:"));
    BindDiffColor(TEXT("shapes-box-convex-radius-diff-color"), TEXT("Convex Radius:"));
    BindDiffColor(TEXT("shapes-capsule-half-height-diff-color"), TEXT("Half Height:"));
    BindDiffColor(TEXT("shapes-capsule-radius-diff-color"), TEXT("Radius:"));
    BindDiffColor(TEXT("shapes-cylinder-half-height-diff-color"), TEXT("Half Height:"));
    BindDiffColor(TEXT("shapes-cylinder-radius-diff-color"), TEXT("Radius:"));
    BindDiffColor(TEXT("shapes-cylinder-convex-radius-diff-color"), TEXT("Convex Radius:"));
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakWidget]()
    {
        const TSharedPtr<SCkInspector_ShapesAuthored> Widget = WeakWidget.Pin();
        return Widget.IsValid() && Widget->Get_CanRequest();
    });

    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        MoveTemp(NativeBindings), {}, {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString ResourceRoot = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(
        FPaths::Combine(ResourceRoot, TEXT("EcsInspectorShapes.ui.html")),
        FPaths::Combine(ResourceRoot, TEXT("EcsInspectorShapes.ui.css")));
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

auto SCkInspector_ShapesAuthored::Tick(
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

auto SCkInspector_ShapesAuthored::Release() -> void
{
    if (NOT _Active) { return; }
    _Active = false;
    for (const TSharedPtr<FCkInspectorEditScope>& EditScope : _EditScopes)
    { if (EditScope.IsValid()) { EditScope->Set_Active(false); } }
    _EditScopes.Reset();
    Detach_NativePorts();
    _Entity = {};
    _EditGuard.Reset();
    _View.Reset();
    _Mounted = false;
}

auto FCkInspector_Shapes::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Shapes"));
}

auto FCkInspector_Shapes::CanInspect(const FCk_Handle& Entity) const -> bool
{
    auto Sphere = FCk_Handle_ShapeSphere{};
    auto Box = FCk_Handle_ShapeBox{};
    auto Capsule = FCk_Handle_ShapeCapsule{};
    auto Cylinder = FCk_Handle_ShapeCylinder{};
    return ck_inspector_shapes::TryGetSphere(Entity, Sphere)
        || ck_inspector_shapes::TryGetBox(Entity, Box)
        || ck_inspector_shapes::TryGetCapsule(Entity, Capsule)
        || ck_inspector_shapes::TryGetCylinder(Entity, Cylinder);
}

// =====================================================================================================================

auto FCkInspector_Shapes::Build_NativeBody(const FCk_Handle& Entity) const -> TSharedRef<SWidget>
{
    namespace shapes = ck_inspector_shapes;

    auto Builder = FCkInspectorWidgetBuilder();
    Builder.SetEditGuard(Get_EditGuard());

    auto MutableEntity = Entity;

    // ---- Sphere ----
    if (Entity.Has<ck::FFragment_ShapeSphere_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Sphere")));

        const auto Sphere = UCk_Utils_ShapeSphere_UE::Cast(MutableEntity);

        Builder.AddNumericRow(
            FText::FromString(TEXT("Radius:")),
            TAttribute<float>::CreateLambda([Sphere]()
            {
                if (ck::Is_NOT_Valid(Sphere)) { return 0.0f; }
                return UCk_Utils_ShapeSphere_UE::Get_Dimensions(Sphere).Get_Radius();
            }),
            [Sphere](float InValue)
            {
                auto Mutable = Sphere;
                if (ck::Is_NOT_Valid(Mutable)) { return; }

                UCk_Utils_ShapeSphere_UE::Request_UpdateDimensions(Mutable,
                    FCk_Request_ShapeSphere_UpdateDimensions{FCk_ShapeSphere_Dimensions{InValue}}, {});
            },
            shapes::MinDimension);
    }

    // ---- Box ----
    if (Entity.Has<ck::FFragment_ShapeBox_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Box")));

        const auto Box = UCk_Utils_ShapeBox_UE::Cast(MutableEntity);

        // Convex radius is a separate field on the SAME dimensions struct, so each editor re-reads the
        // other before writing — otherwise committing one silently resets the other to its default.
        Builder.AddVectorRow(
            FText::FromString(TEXT("Half Extents:")),
            TAttribute<FVector>::CreateLambda([Box]() { return shapes::Get_BoxHalfExtents(Box); }),
            [Box](const FVector& InValue)
            {
                auto Mutable = Box;
                if (ck::Is_NOT_Valid(Mutable)) { return; }

                auto NewDims = FCk_ShapeBox_Dimensions{InValue};
                NewDims.Set_ConvexRadius(UCk_Utils_ShapeBox_UE::Get_Dimensions(Mutable).Get_ConvexRadius());

                UCk_Utils_ShapeBox_UE::Request_UpdateDimensions(Mutable,
                    FCk_Request_ShapeBox_UpdateDimensions{NewDims}, {});
            });

        Builder.AddNumericRow(
            FText::FromString(TEXT("Convex Radius:")),
            TAttribute<float>::CreateLambda([Box]()
            {
                if (ck::Is_NOT_Valid(Box)) { return 0.0f; }
                return UCk_Utils_ShapeBox_UE::Get_Dimensions(Box).Get_ConvexRadius();
            }),
            [Box](float InValue)
            {
                auto Mutable = Box;
                if (ck::Is_NOT_Valid(Mutable)) { return; }

                auto NewDims = FCk_ShapeBox_Dimensions{UCk_Utils_ShapeBox_UE::Get_Dimensions(Mutable).Get_HalfExtents()};
                NewDims.Set_ConvexRadius(InValue);

                UCk_Utils_ShapeBox_UE::Request_UpdateDimensions(Mutable,
                    FCk_Request_ShapeBox_UpdateDimensions{NewDims}, {});
            },
            shapes::MinDimension);
    }

    // ---- Capsule ----
    if (Entity.Has<ck::FFragment_ShapeCapsule_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Capsule")));

        const auto Capsule = UCk_Utils_ShapeCapsule_UE::Cast(MutableEntity);

        Builder.AddNumericRow(
            FText::FromString(TEXT("Half Height:")),
            TAttribute<float>::CreateLambda([Capsule]()
            {
                if (ck::Is_NOT_Valid(Capsule)) { return 0.0f; }
                return UCk_Utils_ShapeCapsule_UE::Get_Dimensions(Capsule).Get_HalfHeight();
            }),
            [Capsule](float InValue)
            {
                auto Mutable = Capsule;
                if (ck::Is_NOT_Valid(Mutable)) { return; }

                const auto Radius = UCk_Utils_ShapeCapsule_UE::Get_Dimensions(Mutable).Get_Radius();

                UCk_Utils_ShapeCapsule_UE::Request_UpdateDimensions(Mutable,
                    FCk_Request_ShapeCapsule_UpdateDimensions{FCk_ShapeCapsule_Dimensions{InValue, Radius}}, {});
            },
            shapes::MinDimension);

        Builder.AddNumericRow(
            FText::FromString(TEXT("Radius:")),
            TAttribute<float>::CreateLambda([Capsule]()
            {
                if (ck::Is_NOT_Valid(Capsule)) { return 0.0f; }
                return UCk_Utils_ShapeCapsule_UE::Get_Dimensions(Capsule).Get_Radius();
            }),
            [Capsule](float InValue)
            {
                auto Mutable = Capsule;
                if (ck::Is_NOT_Valid(Mutable)) { return; }

                const auto HalfHeight = UCk_Utils_ShapeCapsule_UE::Get_Dimensions(Mutable).Get_HalfHeight();

                UCk_Utils_ShapeCapsule_UE::Request_UpdateDimensions(Mutable,
                    FCk_Request_ShapeCapsule_UpdateDimensions{FCk_ShapeCapsule_Dimensions{HalfHeight, InValue}}, {});
            },
            shapes::MinDimension);
    }

    // ---- Cylinder ----
    if (Entity.Has<ck::FFragment_ShapeCylinder_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Cylinder")));

        const auto Cylinder = UCk_Utils_ShapeCylinder_UE::Cast(MutableEntity);

        // Cylinder dimensions carry a third field (convex radius) that the two-arg constructor does not
        // take, so every write restores it explicitly.
        const auto MakeCylinderDims = [](float InHalfHeight, float InRadius, float InConvexRadius)
        {
            auto NewDims = FCk_ShapeCylinder_Dimensions{InHalfHeight, InRadius};
            NewDims.Set_ConvexRadius(InConvexRadius);
            return NewDims;
        };

        Builder.AddNumericRow(
            FText::FromString(TEXT("Half Height:")),
            TAttribute<float>::CreateLambda([Cylinder]()
            {
                if (ck::Is_NOT_Valid(Cylinder)) { return 0.0f; }
                return UCk_Utils_ShapeCylinder_UE::Get_Dimensions(Cylinder).Get_HalfHeight();
            }),
            [Cylinder, MakeCylinderDims](float InValue)
            {
                auto Mutable = Cylinder;
                if (ck::Is_NOT_Valid(Mutable)) { return; }

                const auto Dims = UCk_Utils_ShapeCylinder_UE::Get_Dimensions(Mutable);

                UCk_Utils_ShapeCylinder_UE::Request_UpdateDimensions(Mutable,
                    FCk_Request_ShapeCylinder_UpdateDimensions{
                        MakeCylinderDims(InValue, Dims.Get_Radius(), Dims.Get_ConvexRadius())}, {});
            },
            shapes::MinDimension);

        Builder.AddNumericRow(
            FText::FromString(TEXT("Radius:")),
            TAttribute<float>::CreateLambda([Cylinder]()
            {
                if (ck::Is_NOT_Valid(Cylinder)) { return 0.0f; }
                return UCk_Utils_ShapeCylinder_UE::Get_Dimensions(Cylinder).Get_Radius();
            }),
            [Cylinder, MakeCylinderDims](float InValue)
            {
                auto Mutable = Cylinder;
                if (ck::Is_NOT_Valid(Mutable)) { return; }

                const auto Dims = UCk_Utils_ShapeCylinder_UE::Get_Dimensions(Mutable);

                UCk_Utils_ShapeCylinder_UE::Request_UpdateDimensions(Mutable,
                    FCk_Request_ShapeCylinder_UpdateDimensions{
                        MakeCylinderDims(Dims.Get_HalfHeight(), InValue, Dims.Get_ConvexRadius())}, {});
            },
            shapes::MinDimension);

        Builder.AddNumericRow(
            FText::FromString(TEXT("Convex Radius:")),
            TAttribute<float>::CreateLambda([Cylinder]()
            {
                if (ck::Is_NOT_Valid(Cylinder)) { return 0.0f; }
                return UCk_Utils_ShapeCylinder_UE::Get_Dimensions(Cylinder).Get_ConvexRadius();
            }),
            [Cylinder, MakeCylinderDims](float InValue)
            {
                auto Mutable = Cylinder;
                if (ck::Is_NOT_Valid(Mutable)) { return; }

                const auto Dims = UCk_Utils_ShapeCylinder_UE::Get_Dimensions(Mutable);

                UCk_Utils_ShapeCylinder_UE::Request_UpdateDimensions(Mutable,
                    FCk_Request_ShapeCylinder_UpdateDimensions{
                        MakeCylinderDims(Dims.Get_HalfHeight(), Dims.Get_Radius(), InValue)}, {});
            },
            shapes::MinDimension);
    }

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto FCkInspector_Shapes::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> NativeBody = Build_NativeBody(Entity);
    if (FCkInspector_RowCaptureScope::Is_Active()) { return NativeBody; }

    auto DiffLabels = TSet<FString>{};
    for (const TCHAR* Label : {TEXT("Radius:"), TEXT("Half Extents:"), TEXT("Convex Radius:"), TEXT("Half Height:")})
    {
        if (FCkInspector_DiffMarkScope::Is_LabelMarked(Label)) { DiffLabels.Add(Label); }
    }

    const TSharedRef<SCkInspector_ShapesAuthored> Authored = SNew(SCkInspector_ShapesAuthored)
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

auto FCkInspector_Shapes::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    _LastAuthoredLoadError.Reset();
    for (const TWeakPtr<SCkInspector_ShapesAuthored>& WeakInstance : _AuthoredInstances)
    {
        if (const TSharedPtr<SCkInspector_ShapesAuthored> Instance = WeakInstance.Pin(); Instance.IsValid()
            && NOT Instance->Get_LoadError().IsEmpty())
        {
            _LastAuthoredLoadError = Instance->Get_LoadError();
            break;
        }
    }
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_ShapesAuthored>& InInstance)
    { return NOT InInstance.IsValid() || InInstance.Pin()->Is_Inert(); });
}

// =====================================================================================================================

FCkInspector_Shapes::~FCkInspector_Shapes()
{
    OnDeactivated();
}

auto FCkInspector_Shapes::OnDeactivated() -> void
{
    for (const TWeakPtr<SCkInspector_ShapesAuthored>& WeakInstance : _AuthoredInstances)
    {
        if (const TSharedPtr<SCkInspector_ShapesAuthored> Instance = WeakInstance.Pin(); Instance.IsValid())
        { Instance->Release(); }
    }
    _AuthoredInstances.Reset();
}

// =====================================================================================================================
