#include "CkInspector_Transform.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkEcsExt/Transform/CkTransform_Utils.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_OrientationCube.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Utils/CkDebug_InspectorEditGuard.h"
#include "CkDebuggerCommon/Utils/CkDebug_RequestGate.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NumericEditor.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Transform)

// --------------------------------------------------------------------------------------------------------------------

namespace ck_inspector_transform
{
    // Live read of the inspected entity's transform. The handle is captured by value in every row
    // attribute (the inspector pattern — rows are released on rebuild / OnDeactivated), so each
    // read re-validates before touching the registry and falls back to identity otherwise.
    static auto Get_CurrentTransform(
        const FCk_Handle& InEntity)
        -> FTransform
    {
        if (ck::Is_NOT_Valid(InEntity) || NOT UCk_Utils_Transform_UE::Has(InEntity))
        { return FTransform::Identity; }

        return UCk_Utils_Transform_TypeUnsafe_UE::Get_EntityCurrentTransform(InEntity);
    }

    // Three fixed-precision components in X/Y/Z order, so AddAlignedNumericRow's index-based axis
    // coloring lines up with the orientation cube's axis edges.
    static auto Make_AxisComponents(
        const FCk_Handle& InEntity,
        TFunction<FVector(const FTransform&)> InProjector)
        -> TArray<TAttribute<FText>>
    {
        auto Components = TArray<TAttribute<FText>>{};
        Components.Reserve(3);

        for (auto Axis = 0; Axis < 3; ++Axis)
        {
            Components.Emplace(TAttribute<FText>::CreateLambda([InEntity, InProjector, Axis]()
            {
                const auto Value = InProjector(Get_CurrentTransform(InEntity));
                return FText::FromString(ck::Format_UE(TEXT("{:.3f}"), Value[Axis]));
            }));
        }

        return Components;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkInspector_Transform::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Transform"));
}

auto FCkInspector_Transform::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity) && UCk_Utils_Transform_UE::Has(Entity);
}

auto FCkInspector_Transform::Build_NativeBody(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    const auto CapturedEntity = Entity;

    auto Builder = FCkInspectorWidgetBuilder();
    Builder.SetEditGuard(Get_EditGuard());

    Builder
        .AddAlignedNumericRow(
            FText::FromString(TEXT("Location:")),
            ck_inspector_transform::Make_AxisComponents(CapturedEntity,
                [](const FTransform& InTransform) { return InTransform.GetLocation(); }))
        .AddAlignedNumericRow(
            FText::FromString(TEXT("Rotation (R,P,Y):")),
            ck_inspector_transform::Make_AxisComponents(CapturedEntity,
                [](const FTransform& InTransform)
                {
                    // Euler degrees, same values FRotator printed before — reordered to the axis
                    // each angle turns about (Roll=X, Pitch=Y, Yaw=Z) so the row's X/Y/Z coloring
                    // agrees with the cube's axis edges. The label states the order.
                    const auto Rotator = InTransform.GetRotation().Rotator();
                    return FVector{Rotator.Roll, Rotator.Pitch, Rotator.Yaw};
                }))
        .AddAlignedNumericRow(
            FText::FromString(TEXT("Scale:")),
            ck_inspector_transform::Make_AxisComponents(CapturedEntity,
                [](const FTransform& InTransform) { return InTransform.GetScale3D(); }))
        .AddWidgetRow(
            FText::FromString(TEXT("Orientation:")),
            SNew(SCkDebug_OrientationCube)
                .Rotation_Lambda([CapturedEntity]()
                {
                    return ck_inspector_transform::Get_CurrentTransform(CapturedEntity).GetRotation();
                })
                .Scale_Lambda([CapturedEntity]()
                {
                    return ck_inspector_transform::Get_CurrentTransform(CapturedEntity).GetScale3D();
                }));

    auto MutableEntity = Entity;
    const auto CapturedTransform = UCk_Utils_Transform_UE::Cast(MutableEntity);

    if (ck::Is_NOT_Valid(CapturedTransform))
    { return Builder.Build(Entity); }

    // ---- Edit ----
    //
    // Every row writes through the public Transform Utils with the typed handle captured BY VALUE and
    // re-validated on fire; the read-only rows above stay the display, so nothing here reads a request
    // back. All LocalOk: FProcessor_Transform_HandleRequests runs on every net mode, and a replicated
    // transform simply gets server-stomped on the next update, which is a legitimate local experiment.

    Builder.AddHeader(FText::FromString(TEXT("Edit")));

    // One space for all five verbs below — every Transform request struct carries the same
    // ECk_LocalWorld field, so a single dropdown is honest rather than five identical ones.
    Builder.AddEnumDropdownRow(
        FText::FromString(TEXT("Space:")),
        {
            FText::FromString(TEXT("Local")),
            FText::FromString(TEXT("World"))
        },
        TAttribute<int32>::CreateLambda([Space = _EditSpace]()
        {
            return static_cast<int32>(*Space);
        }),
        [Space = _EditSpace](int32 InIndex)
        {
            *Space = static_cast<ECk_LocalWorld>(InIndex);
        });

    Builder.AddVectorRow(
        FText::FromString(TEXT("Set Location:")),
        TAttribute<FVector>::CreateLambda([CapturedTransform]()
        {
            if (ck::Is_NOT_Valid(CapturedTransform))
            { return FVector::ZeroVector; }

            return UCk_Utils_Transform_UE::Get_EntityCurrentLocation(CapturedTransform);
        }),
        [CapturedTransform, Space = _EditSpace](const FVector& InLocation)
        {
            auto Handle = CapturedTransform;
            if (ck::Is_NOT_Valid(Handle))
            { return; }

            UCk_Utils_Transform_UE::Request_SetLocation(
                Handle,
                FCk_Request_Transform_SetLocation{InLocation}.Set_LocalWorld(*Space),
                {});
        });

    Builder.AddRotatorRow(
        FText::FromString(TEXT("Set Rotation (R,P,Y):")),
        TAttribute<FRotator>::CreateLambda([CapturedTransform]()
        {
            if (ck::Is_NOT_Valid(CapturedTransform))
            { return FRotator::ZeroRotator; }

            return UCk_Utils_Transform_UE::Get_EntityCurrentRotation(CapturedTransform);
        }),
        [CapturedTransform, Space = _EditSpace](const FRotator& InRotation)
        {
            auto Handle = CapturedTransform;
            if (ck::Is_NOT_Valid(Handle))
            { return; }

            UCk_Utils_Transform_UE::Request_SetRotation(
                Handle,
                FCk_Request_Transform_SetRotation{InRotation}.Set_LocalWorld(*Space),
                {});
        });

    // Scale supersedes rather than queues (CkTransform_Utils.h:191) — a second request replaces a
    // still-pending one, which completes Failed. Nothing to code around: the numeric editors commit on
    // enter / lost focus, so one edit produces one request instead of a per-keystroke storm.
    Builder.AddVectorRow(
        FText::FromString(TEXT("Set Scale:")),
        TAttribute<FVector>::CreateLambda([CapturedTransform]()
        {
            if (ck::Is_NOT_Valid(CapturedTransform))
            { return FVector::OneVector; }

            return UCk_Utils_Transform_UE::Get_EntityCurrentScale(CapturedTransform);
        }),
        [CapturedTransform, Space = _EditSpace](const FVector& InScale)
        {
            auto Handle = CapturedTransform;
            if (ck::Is_NOT_Valid(Handle))
            { return; }

            UCk_Utils_Transform_UE::Request_SetScale(
                Handle,
                FCk_Request_Transform_SetScale{InScale}.Set_LocalWorld(*Space),
                {});
        });

    // Offsets are DELTAS, so there is no live value to read back: these two rows hold the delta this
    // inspector will send, and the Apply row is what sends it. Committing a component deliberately does
    // NOT fire — an offset row that fired on every component edit would re-apply the components already
    // typed. The delta is kept after applying, so clicking Apply again steps by the same amount.
    Builder.AddVectorRow(
        FText::FromString(TEXT("Location Offset:")),
        TAttribute<FVector>::CreateLambda([Offset = _LocationOffset]() { return *Offset; }),
        [Offset = _LocationOffset](const FVector& InOffset) { *Offset = InOffset; });

    Builder.AddRotatorRow(
        FText::FromString(TEXT("Rotation Offset (R,P,Y):")),
        TAttribute<FRotator>::CreateLambda([Offset = _RotationOffset]() { return *Offset; }),
        [Offset = _RotationOffset](const FRotator& InOffset) { *Offset = InOffset; });

    Builder.AddActionRow(
        FText::FromString(TEXT("Apply:")),
        {
            FCkInspector_Action
            {
                FText::FromString(TEXT("Add Location")),
                FText::FromString(TEXT("Request_AddLocationOffset with the Location Offset above, in the selected space")),
                [CapturedTransform, Offset = _LocationOffset, Space = _EditSpace]
                {
                    auto Handle = CapturedTransform;
                    if (ck::Is_NOT_Valid(Handle))
                    { return; }

                    UCk_Utils_Transform_UE::Request_AddLocationOffset(
                        Handle,
                        FCk_Request_Transform_AddLocationOffset{*Offset}.Set_LocalWorld(*Space),
                        {});
                }
            },
            FCkInspector_Action
            {
                FText::FromString(TEXT("Add Rotation")),
                FText::FromString(TEXT("Request_AddRotationOffset with the Rotation Offset above, in the selected space")),
                [CapturedTransform, Offset = _RotationOffset, Space = _EditSpace]
                {
                    auto Handle = CapturedTransform;
                    if (ck::Is_NOT_Valid(Handle))
                    { return; }

                    UCk_Utils_Transform_UE::Request_AddRotationOffset(
                        Handle,
                        FCk_Request_Transform_AddRotationOffset{*Offset}.Set_LocalWorld(*Space),
                        {});
                }
            },
            FCkInspector_Action
            {
                FText::FromString(TEXT("Force Refresh")),
                FText::FromString(TEXT("Request_ForceRefresh — re-broadcasts the transform to every listener and sync target")),
                [CapturedTransform]
                {
                    auto Handle = CapturedTransform;
                    if (ck::Is_NOT_Valid(Handle))
                    { return; }

                    UCk_Utils_Transform_UE::Request_ForceRefresh(Handle, {});
                }
            }
        });

    // ---- Interpolation ----
    //
    // Structural: present only while the sibling TransformInterpolation feature is. Tick watches for
    // that flip and asks for a rebuild, which is the only rebuild this inspector requests.
    auto MutableInterpEntity = Entity;
    if (const auto CapturedInterp = UCk_Utils_TransformInterpolation_UE::Cast(MutableInterpEntity);
        ck::IsValid(CapturedInterp))
    {
        Builder.AddHeader(FText::FromString(TEXT("Interpolation")));

        // The goal offsets are write-only in the Utils surface — there is no getter — so these rows
        // display what was last sent from here rather than a live read.
        Builder.AddVectorRow(
            FText::FromString(TEXT("Goal Location Offset:")),
            TAttribute<FVector>::CreateLambda([Goal = _InterpGoalLoc]() { return *Goal; }),
            [CapturedInterp, Goal = _InterpGoalLoc](const FVector& InOffset)
            {
                *Goal = InOffset;

                auto Handle = CapturedInterp;
                if (ck::Is_NOT_Valid(Handle))
                { return; }

                UCk_Utils_TransformInterpolation_UE::Request_SetInterpolationGoal_LocationOffset(Handle, InOffset);
            });

        Builder.AddRotatorRow(
            FText::FromString(TEXT("Goal Rotation Offset (R,P,Y):")),
            TAttribute<FRotator>::CreateLambda([Goal = _InterpGoalRot]() { return *Goal; }),
            [CapturedInterp, Goal = _InterpGoalRot](const FRotator& InOffset)
            {
                *Goal = InOffset;

                auto Handle = CapturedInterp;
                if (ck::Is_NOT_Valid(Handle))
                { return; }

                UCk_Utils_TransformInterpolation_UE::Request_SetInterpolationGoal_RotationOffset(Handle, InOffset);
            });
    }

    return Builder.Build(Entity);
}

auto FCkInspector_Transform::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    static_cast<void>(InDeltaTime);
    if (ck::IsValid(Entity) && UCk_Utils_Transform_UE::Has(Entity))
    {
        // The only structural change this inspector has: the Interpolation section exists exactly while the
        // sibling feature does. Everything else in the panel is attribute-driven and never needs a rebuild.
        if (const auto HasInterpolation = UCk_Utils_TransformInterpolation_UE::Has(Entity);
            HasInterpolation != _HadInterpolation)
        {
            _HadInterpolation = HasInterpolation;
            RequestRebuild();
        }
    }

    _LastAuthoredLoadError.Reset();
    for (const TWeakPtr<SCkInspector_TransformAuthored>& WeakInstance : _AuthoredInstances)
    {
        if (const auto Instance = WeakInstance.Pin(); Instance.IsValid() && NOT Instance->Get_LoadError().IsEmpty())
        {
            _LastAuthoredLoadError = Instance->Get_LoadError();
            break;
        }
    }
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_TransformAuthored>& InInstance)
    { return NOT InInstance.IsValid() || InInstance.Pin()->Is_Inert(); });
}

// --------------------------------------------------------------------------------------------------------------------
// Authored Transform shell. HTML/CSS owns every row, label, section and action placement; the native ports below are
// deliberately limited to canonical interactive leaves. Build_NativeBody remains the independent capture/fallback path.

namespace ck_inspector_transform
{
    auto DiffColor(const bool bDiffMarked) -> FLinearColor
    { return bDiffMarked ? CkStyle::Accent() : CkStyle::Text(); }

    auto AxisColor(const int32 InAxis) -> FLinearColor
    {
        switch (InAxis)
        {
            case 0: return CkStyle::AxisX();
            case 1: return CkStyle::AxisY();
            default: return CkStyle::AxisZ();
        }
    }

    auto TryGetTransform(const FCk_Handle& InEntity, FCk_Handle_Transform& OutTransform) -> bool
    {
        OutTransform = {};
        if (ck::Is_NOT_Valid(InEntity) || NOT UCk_Utils_Transform_UE::Has(InEntity)) { return false; }
        auto MutableEntity = InEntity;
        OutTransform = UCk_Utils_Transform_UE::Cast(MutableEntity);
        return ck::IsValid(OutTransform);
    }
}

auto SCkInspector_TransformAuthored::Construct(const FArguments& InArgs) -> void
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

SCkInspector_TransformAuthored::~SCkInspector_TransformAuthored()
{
    Release();
}

auto SCkInspector_TransformAuthored::Get_IsAvailable() const -> bool
{
    auto Transform = FCk_Handle_Transform{};
    return _Active && ck_inspector_transform::TryGetTransform(_Entity, Transform);
}

auto SCkInspector_TransformAuthored::Get_HasInterpolation() const -> bool
{
    if (NOT Get_IsAvailable() || NOT UCk_Utils_TransformInterpolation_UE::Has(_Entity)) { return false; }
    auto MutableEntity = _Entity;
    return ck::IsValid(UCk_Utils_TransformInterpolation_UE::Cast(MutableEntity));
}

auto SCkInspector_TransformAuthored::Get_CanRequest() const -> bool
{
    return Get_IsAvailable()
        && ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::LocalOk).IsEnabled;
}

auto SCkInspector_TransformAuthored::Get_RequestDisabledReason() const -> FString
{
    if (NOT Get_IsAvailable()) { return TEXT("Transform is unavailable."); }
    return ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::LocalOk).Reason.ToString();
}

auto SCkInspector_TransformAuthored::Get_Location() const -> FVector
{
    auto Transform = FCk_Handle_Transform{};
    return _Active && ck_inspector_transform::TryGetTransform(_Entity, Transform)
        ? UCk_Utils_Transform_UE::Get_EntityCurrentLocation(Transform) : FVector::ZeroVector;
}

auto SCkInspector_TransformAuthored::Get_Rotation() const -> FRotator
{
    auto Transform = FCk_Handle_Transform{};
    return _Active && ck_inspector_transform::TryGetTransform(_Entity, Transform)
        ? UCk_Utils_Transform_UE::Get_EntityCurrentRotation(Transform) : FRotator::ZeroRotator;
}

auto SCkInspector_TransformAuthored::Get_Scale() const -> FVector
{
    auto Transform = FCk_Handle_Transform{};
    return _Active && ck_inspector_transform::TryGetTransform(_Entity, Transform)
        ? UCk_Utils_Transform_UE::Get_EntityCurrentScale(Transform) : FVector::OneVector;
}

auto SCkInspector_TransformAuthored::Get_AxisText(const FString& InKind, const int32 InAxis) const -> FString
{
    if (NOT Get_IsAvailable()) { return TEXT("--"); }
    if (InKind == TEXT("rotation"))
    {
        const FRotator Value = Get_Rotation();
        const double Components[] = {Value.Roll, Value.Pitch, Value.Yaw};
        return ck::Format_UE(TEXT("{:.3f}"), Components[FMath::Clamp(InAxis, 0, 2)]);
    }
    const FVector Value = InKind == TEXT("scale") ? Get_Scale() : Get_Location();
    return ck::Format_UE(TEXT("{:.3f}"), Value[FMath::Clamp(InAxis, 0, 2)]);
}

auto SCkInspector_TransformAuthored::Get_EditSpaceText() const -> FString
{ return _EditSpace == ECk_LocalWorld::Local ? TEXT("Local") : TEXT("World"); }

auto SCkInspector_TransformAuthored::Commit_EditSpace(const int32 InIndex) -> void
{ if (_Active && Get_CanRequest()) { _EditSpace = InIndex == 0 ? ECk_LocalWorld::Local : ECk_LocalWorld::World; } }

auto SCkInspector_TransformAuthored::Commit_SetLocation(const FVector& InValue) -> void
{
    if (NOT Get_CanRequest()) { return; }
    auto Transform = FCk_Handle_Transform{};
    if (ck_inspector_transform::TryGetTransform(_Entity, Transform))
    { UCk_Utils_Transform_UE::Request_SetLocation(
        Transform, FCk_Request_Transform_SetLocation{InValue}.Set_LocalWorld(_EditSpace), {}); }
}

auto SCkInspector_TransformAuthored::Commit_SetRotation(const FRotator& InValue) -> void
{
    if (NOT Get_CanRequest()) { return; }
    auto Transform = FCk_Handle_Transform{};
    if (ck_inspector_transform::TryGetTransform(_Entity, Transform))
    { UCk_Utils_Transform_UE::Request_SetRotation(
        Transform, FCk_Request_Transform_SetRotation{InValue}.Set_LocalWorld(_EditSpace), {}); }
}

auto SCkInspector_TransformAuthored::Commit_SetScale(const FVector& InValue) -> void
{
    if (NOT Get_CanRequest()) { return; }
    auto Transform = FCk_Handle_Transform{};
    if (ck_inspector_transform::TryGetTransform(_Entity, Transform))
    { UCk_Utils_Transform_UE::Request_SetScale(
        Transform, FCk_Request_Transform_SetScale{InValue}.Set_LocalWorld(_EditSpace), {}); }
}

auto SCkInspector_TransformAuthored::Commit_LocationOffset(const FVector& InValue) -> void
{ if (_Active) { _LocationOffset = InValue; } }

auto SCkInspector_TransformAuthored::Commit_RotationOffset(const FRotator& InValue) -> void
{ if (_Active) { _RotationOffset = InValue; } }

auto SCkInspector_TransformAuthored::Commit_InterpolationLocation(const FVector& InValue) -> void
{
    if (NOT Get_CanRequest() || NOT Get_HasInterpolation()) { return; }
    _InterpGoalLoc = InValue;
    auto MutableEntity = _Entity;
    auto Handle = UCk_Utils_TransformInterpolation_UE::Cast(MutableEntity);
    if (ck::IsValid(Handle))
    { UCk_Utils_TransformInterpolation_UE::Request_SetInterpolationGoal_LocationOffset(Handle, InValue); }
}

auto SCkInspector_TransformAuthored::Commit_InterpolationRotation(const FRotator& InValue) -> void
{
    if (NOT Get_CanRequest() || NOT Get_HasInterpolation()) { return; }
    _InterpGoalRot = InValue;
    auto MutableEntity = _Entity;
    auto Handle = UCk_Utils_TransformInterpolation_UE::Cast(MutableEntity);
    if (ck::IsValid(Handle))
    { UCk_Utils_TransformInterpolation_UE::Request_SetInterpolationGoal_RotationOffset(Handle, InValue); }
}

auto SCkInspector_TransformAuthored::Request_AddLocation() -> void
{
    if (NOT Get_CanRequest()) { return; }
    auto Transform = FCk_Handle_Transform{};
    if (ck_inspector_transform::TryGetTransform(_Entity, Transform))
    { UCk_Utils_Transform_UE::Request_AddLocationOffset(
        Transform, FCk_Request_Transform_AddLocationOffset{_LocationOffset}.Set_LocalWorld(_EditSpace), {}); }
}

auto SCkInspector_TransformAuthored::Request_AddRotation() -> void
{
    if (NOT Get_CanRequest()) { return; }
    auto Transform = FCk_Handle_Transform{};
    if (ck_inspector_transform::TryGetTransform(_Entity, Transform))
    { UCk_Utils_Transform_UE::Request_AddRotationOffset(
        Transform, FCk_Request_Transform_AddRotationOffset{_RotationOffset}.Set_LocalWorld(_EditSpace), {}); }
}

auto SCkInspector_TransformAuthored::Request_ForceRefresh() -> void
{
    if (NOT Get_CanRequest()) { return; }
    auto Transform = FCk_Handle_Transform{};
    if (ck_inspector_transform::TryGetTransform(_Entity, Transform))
    { UCk_Utils_Transform_UE::Request_ForceRefresh(Transform, {}); }
}

auto SCkInspector_TransformAuthored::Present_EditControl(
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

auto SCkInspector_TransformAuthored::Build_VectorValue(
    const FName InTagPrefix,
    TFunction<FVector()> InGet,
    TFunction<void(const FVector&)> InSet,
    TFunction<bool()> InIsEnabled) -> TSharedRef<SWidget>
{
    const TWeakPtr<SCkInspector_TransformAuthored> WeakWidget{SharedThis(this)};
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
            .ForegroundColor(ck_inspector_transform::AxisColor(Axis))
            .OnValueCommitted_Lambda([InGet, InSet, Axis](const double InValue)
            { auto Value = InGet(); Value[Axis] = InValue; InSet(Value); })
            .OnEditStateChanged_Lambda([Scope](const bool bEditing)
            { if (Scope.IsValid()) { Scope->Set_Active(bEditing); } });
        Editor->SetEnabled(TAttribute<bool>::CreateLambda([WeakWidget, InIsEnabled]()
        { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && NOT Widget->Is_Inert() && InIsEnabled(); }));
        Editor->SetToolTipText(TAttribute<FText>::CreateLambda([WeakWidget]()
        { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() ? FText::FromString(Widget->Get_RequestDisabledReason()) : FText::GetEmpty(); }));
        Row->AddSlot().AutoWidth().Padding(0.0f, 0.0f, CkStyle::SpaceXS, 0.0f)[Editor];
    }
    return Present_EditControl(Row, TAttribute<FText>::CreateLambda([InGet]()
    { const FVector Value = InGet(); return FText::FromString(ck::Format_UE(TEXT("{:.3f}, {:.3f}, {:.3f}"), Value.X, Value.Y, Value.Z)); }));
}

auto SCkInspector_TransformAuthored::Build_RotatorValue(
    const FName InTagPrefix,
    TFunction<FRotator()> InGet,
    TFunction<void(const FRotator&)> InSet,
    TFunction<bool()> InIsEnabled) -> TSharedRef<SWidget>
{
    const auto GetVector = [InGet]()
    { const FRotator Value = InGet(); return FVector{Value.Roll, Value.Pitch, Value.Yaw}; };
    const auto SetVector = [InSet](const FVector& Value)
    { InSet(FRotator{Value.Y, Value.Z, Value.X}); };
    return Build_VectorValue(InTagPrefix, GetVector, SetVector, MoveTemp(InIsEnabled));
}

auto SCkInspector_TransformAuthored::Build_OrientationValue() -> TSharedRef<SWidget>
{
    const TWeakPtr<SCkInspector_TransformAuthored> WeakWidget{SharedThis(this)};
    return SNew(SCkDebug_OrientationCube)
        .Rotation_Lambda([WeakWidget]()
        { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() ? Widget->Get_Rotation().Quaternion() : FQuat::Identity; })
        .Scale_Lambda([WeakWidget]()
        { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() ? Widget->Get_Scale() : FVector::OneVector; });
}

auto SCkInspector_TransformAuthored::Build_SpaceValue() -> TSharedRef<SWidget>
{
    _SpaceOptions = {MakeShared<FString>(TEXT("Local")), MakeShared<FString>(TEXT("World"))};
    const TWeakPtr<SCkInspector_TransformAuthored> WeakWidget{SharedThis(this)};
    const TSharedRef<SComboBox<TSharedPtr<FString>>> Combo = SNew(SComboBox<TSharedPtr<FString>>)
        .OptionsSource(&_SpaceOptions)
        .OnGenerateWidget_Lambda([](const TSharedPtr<FString> Item)
        { return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : FString{})); })
        .OnSelectionChanged_Lambda([WeakWidget](const TSharedPtr<FString> Item, const ESelectInfo::Type SelectInfo)
        {
            if (SelectInfo != ESelectInfo::Direct && Item.IsValid())
            { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Commit_EditSpace(*Item == TEXT("Local") ? 0 : 1); } }
        })
        [SNew(STextBlock).Text_Lambda([WeakWidget]()
        { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() ? FText::FromString(Widget->Get_EditSpaceText()) : FText::GetEmpty(); })];
    const TSharedRef<SBox> Input = SNew(SBox).Tag(TEXT("transform-space-input")).IsEnabled_Lambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_CanRequest(); })[Combo];
    return Present_EditControl(Input, TAttribute<FText>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() ? FText::FromString(Widget->Get_EditSpaceText()) : FText::GetEmpty(); }));
}

auto SCkInspector_TransformAuthored::Make_NativePort(TSharedRef<SWidget> InLeaf) -> TSharedRef<SBox>
{
    const TSharedRef<SBox> Port = SNew(SBox);
    Port->SetContent(MoveTemp(InLeaf));
    _NativePorts.Add(Port);
    return Port;
}

auto SCkInspector_TransformAuthored::Detach_NativePorts() -> void
{
    for (const TSharedPtr<SBox>& Port : _NativePorts)
    { if (Port.IsValid()) { Port->SetContent(SNullWidget::NullWidget); } }
    _NativePorts.Reset();
}

auto SCkInspector_TransformAuthored::Build_AuthoredView() -> bool
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

    const TWeakPtr<SCkInspector_TransformAuthored> WeakWidget{SharedThis(this)};
    auto Available = [WeakWidget]() { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_CanRequest(); };
    auto InterpolationAvailable = [WeakWidget]() { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_CanRequest() && Widget->Get_HasInterpolation(); };
    auto Ports = FCkUiView::FNativeBindings{};
    Ports.Add(TEXT("transform-orientation-port"), Make_NativePort(Build_OrientationValue()));
    Ports.Add(TEXT("transform-space-port"), Make_NativePort(Build_SpaceValue()));
    Ports.Add(TEXT("transform-set-location-port"), Make_NativePort(Build_VectorValue(TEXT("transform-set-location"),
        [WeakWidget]() { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() ? Widget->Get_Location() : FVector::ZeroVector; },
        [WeakWidget](const FVector& Value) { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Commit_SetLocation(Value); } }, Available)));
    Ports.Add(TEXT("transform-set-rotation-port"), Make_NativePort(Build_RotatorValue(TEXT("transform-set-rotation"),
        [WeakWidget]() { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() ? Widget->Get_Rotation() : FRotator::ZeroRotator; },
        [WeakWidget](const FRotator& Value) { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Commit_SetRotation(Value); } }, Available)));
    Ports.Add(TEXT("transform-set-scale-port"), Make_NativePort(Build_VectorValue(TEXT("transform-set-scale"),
        [WeakWidget]() { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() ? Widget->Get_Scale() : FVector::OneVector; },
        [WeakWidget](const FVector& Value) { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Commit_SetScale(Value); } }, Available)));
    Ports.Add(TEXT("transform-location-offset-port"), Make_NativePort(Build_VectorValue(TEXT("transform-location-offset"),
        [WeakWidget]() { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() ? Widget->_LocationOffset : FVector::ZeroVector; },
        [WeakWidget](const FVector& Value) { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Commit_LocationOffset(Value); } }, Available)));
    Ports.Add(TEXT("transform-rotation-offset-port"), Make_NativePort(Build_RotatorValue(TEXT("transform-rotation-offset"),
        [WeakWidget]() { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() ? Widget->_RotationOffset : FRotator::ZeroRotator; },
        [WeakWidget](const FRotator& Value) { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Commit_RotationOffset(Value); } }, Available)));
    Ports.Add(TEXT("transform-interpolation-location-port"), Make_NativePort(Build_VectorValue(TEXT("transform-interpolation-location"),
        [WeakWidget]() { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() ? Widget->_InterpGoalLoc : FVector::ZeroVector; },
        [WeakWidget](const FVector& Value) { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Commit_InterpolationLocation(Value); } }, InterpolationAvailable)));
    Ports.Add(TEXT("transform-interpolation-rotation-port"), Make_NativePort(Build_RotatorValue(TEXT("transform-interpolation-rotation"),
        [WeakWidget]() { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() ? Widget->_InterpGoalRot : FRotator::ZeroRotator; },
        [WeakWidget](const FRotator& Value) { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Commit_InterpolationRotation(Value); } }, InterpolationAvailable)));

    auto Data = FCkUiView::FDataBindings{};
    for (const FString& Kind : {TEXT("location"), TEXT("rotation"), TEXT("scale")})
    {
        for (int32 Axis = 0; Axis < 3; ++Axis)
        {
            const FString Key = FString::Printf(TEXT("transform-%s-%c"), *Kind, TEXT("xyz")[Axis]);
            Data.Text.Add(Key, TAttribute<FText>::CreateLambda([WeakWidget, Kind, Axis]()
            { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() ? FText::FromString(Widget->Get_AxisText(Kind, Axis)) : FText::GetEmpty(); }));
        }
    }
    Data.Text.Add(TEXT("transform-add-location-label"), FText::FromString(TEXT("Add Location")));
    Data.Text.Add(TEXT("transform-add-rotation-label"), FText::FromString(TEXT("Add Rotation")));
    Data.Text.Add(TEXT("transform-force-refresh-label"), FText::FromString(TEXT("Force Refresh")));
    Data.Text.Add(TEXT("transform-add-location-tooltip"), FText::FromString(TEXT("Request_AddLocationOffset with the Location Offset above, in the selected space")));
    Data.Text.Add(TEXT("transform-add-rotation-tooltip"), FText::FromString(TEXT("Request_AddRotationOffset with the Rotation Offset above, in the selected space")));
    Data.Text.Add(TEXT("transform-force-refresh-tooltip"), FText::FromString(TEXT("Request_ForceRefresh — re-broadcasts the transform to every listener and sync target")));
    Data.Text.Add(TEXT("transform-disabled-reason"), TAttribute<FText>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() ? FText::FromString(Widget->Get_RequestDisabledReason()) : FText::GetEmpty(); }));
    Data.Visibility.Add(TEXT("transform-has-interpolation"), TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_HasInterpolation(); }));
    Data.Visibility.Add(TEXT("transform-can-request"), TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_CanRequest(); }));
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakWidget]()
    { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && Widget->Get_CanRequest(); });
    const auto BindDiff = [&Data, WeakWidget](const FString& InKey, const FString& InLabel)
    {
        Data.Color.Add(InKey, TAttribute<FLinearColor>::CreateLambda([WeakWidget, InLabel]()
        { const auto Widget = WeakWidget.Pin(); return Widget.IsValid() && NOT Widget->Is_Inert()
            ? ck_inspector_transform::DiffColor(Widget->Is_DiffMarked(InLabel)) : FLinearColor::Transparent; }));
    };
    const TArray<TPair<FString, FString>> DiffBindings = {
        {TEXT("location"), TEXT("Location:")}, {TEXT("rotation"), TEXT("Rotation (R,P,Y):")},
        {TEXT("scale"), TEXT("Scale:")}, {TEXT("orientation"), TEXT("Orientation:")},
        {TEXT("space"), TEXT("Space:")}, {TEXT("set-location"), TEXT("Set Location:")},
        {TEXT("set-rotation"), TEXT("Set Rotation (R,P,Y):")}, {TEXT("set-scale"), TEXT("Set Scale:")},
        {TEXT("location-offset"), TEXT("Location Offset:")},
        {TEXT("rotation-offset"), TEXT("Rotation Offset (R,P,Y):")}, {TEXT("apply"), TEXT("Apply:")},
        {TEXT("interpolation-location"), TEXT("Goal Location Offset:")},
        {TEXT("interpolation-rotation"), TEXT("Goal Rotation Offset (R,P,Y):")}};
    for (const auto& Binding : DiffBindings)
    { BindDiff(TEXT("transform-") + Binding.Key + TEXT("-diff-color"), Binding.Value); }

    auto Actions = FCkUiView::FActions{};
    Actions.Add(TEXT("transform-add-location"), FSimpleDelegate::CreateLambda([WeakWidget]()
    { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Request_AddLocation(); } }));
    Actions.Add(TEXT("transform-add-rotation"), FSimpleDelegate::CreateLambda([WeakWidget]()
    { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Request_AddRotation(); } }));
    Actions.Add(TEXT("transform-force-refresh"), FSimpleDelegate::CreateLambda([WeakWidget]()
    { if (const auto Widget = WeakWidget.Pin(); Widget.IsValid()) { Widget->Request_ForceRefresh(); } }));

    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        MoveTemp(Ports), MoveTemp(Actions), {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString Root = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(FPaths::Combine(Root, TEXT("EcsInspectorTransform.ui.html")),
        FPaths::Combine(Root, TEXT("EcsInspectorTransform.ui.css")));
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

auto SCkInspector_TransformAuthored::Tick(
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

auto SCkInspector_TransformAuthored::Release() -> void
{
    if (NOT _Active) { return; }
    _Active = false;
    for (const TSharedPtr<FCkInspectorEditScope>& Scope : _EditScopes)
    { if (Scope.IsValid()) { Scope->Set_Active(false); } }
    _EditScopes.Reset();
    Detach_NativePorts();
    _SpaceOptions.Reset();
    _Entity = {};
    _EditGuard.Reset();
    _View.Reset();
    _Mounted = false;
}

auto FCkInspector_Transform::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> NativeBody = Build_NativeBody(Entity);
    if (FCkInspector_RowCaptureScope::Is_Active()) { return NativeBody; }
    _HadInterpolation = ck::IsValid(Entity) && UCk_Utils_TransformInterpolation_UE::Has(Entity);

    auto DiffLabels = TSet<FString>{};
    for (const FString& Label : {TEXT("Location:"), TEXT("Rotation (R,P,Y):"), TEXT("Scale:"), TEXT("Orientation:"),
        TEXT("Space:"), TEXT("Set Location:"), TEXT("Set Rotation (R,P,Y):"), TEXT("Set Scale:"),
        TEXT("Location Offset:"), TEXT("Rotation Offset (R,P,Y):"), TEXT("Apply:"),
        TEXT("Goal Location Offset:"), TEXT("Goal Rotation Offset (R,P,Y):")})
    { if (FCkInspector_DiffMarkScope::Is_LabelMarked(Label)) { DiffLabels.Add(Label); } }

    const TSharedRef<SCkInspector_TransformAuthored> Authored = SNew(SCkInspector_TransformAuthored)
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

FCkInspector_Transform::~FCkInspector_Transform()
{
    OnDeactivated();
}

auto FCkInspector_Transform::OnDeactivated() -> void
{
    for (const TWeakPtr<SCkInspector_TransformAuthored>& Weak : _AuthoredInstances)
    { if (const auto Instance = Weak.Pin(); Instance.IsValid()) { Instance->Release(); } }
    _AuthoredInstances.Reset();
}
