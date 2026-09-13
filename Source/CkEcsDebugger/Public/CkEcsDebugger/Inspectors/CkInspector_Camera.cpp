#include "CkInspector_Camera.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Debug/CkDebugDraw_Utils.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"

#include "CkEcsExt/Transform/CkTransform_Fragment.h"

#include "CkCamera/Camera/CkCamera_Fragment.h"
#include "CkCamera/Camera/CkCamera_Utils.h"
#include "CkCamera/Camera/CameraLayer/CkCameraLayer_Fragment.h"
#include "CkCamera/Camera/CameraLayer/EntityScripts/CkCameraLayer_EntityScript.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Utils/CkDebug_RequestGate.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Camera)

// =====================================================================================================================

static auto DoFmt_ClassName(const UClass* InClass) -> FString
{
    return InClass != nullptr ? InClass->GetName() : FString(TEXT("(None)"));
}

static auto DoFmt_Float(float InValue, const TCHAR* InFormat) -> FText
{
    return FText::FromString(ck::Format_UE(InFormat, InValue));
}

static auto DoFmt_Bool(bool InValue) -> FText
{
    return FText::FromString(InValue ? TEXT("Yes") : TEXT("No"));
}

static auto DoColor_Flag(bool InValue) -> FLinearColor
{
    return InValue ? CkStyle::Ok() : CkStyle::TextMute();
}

// =====================================================================================================================

namespace ck_inspector_camera
{
    static auto IsDestroying(const FCk_Handle& InEntity) -> bool
    {
        return ck::Is_NOT_Valid(InEntity) || InEntity.Has_Any<
            ck::FTag_DestroyEntity_Initiate,
            ck::FTag_DestroyEntity_EndPlay,
            ck::FTag_DestroyEntity_Teardown,
            ck::FTag_DestroyEntity_Await,
            ck::FTag_DestroyEntity_Finalize>();
    }

    static auto TryGetCamera(const FCk_Handle& InEntity, FCk_Handle_Camera& OutCamera, bool bNeedsOrientation) -> bool
    {
        OutCamera = {};
        if (IsDestroying(InEntity)
            || NOT InEntity.Has_All<ck::FFragment_Camera_Params, ck::FFragment_Camera_Current>()
            || (bNeedsOrientation && NOT InEntity.Has<ck::FFragment_Camera_OrientationControl>()))
        { return false; }
        auto Mutable = InEntity;
        OutCamera = UCk_Utils_Camera_UE::Cast(Mutable);
        return ck::IsValid(OutCamera);
    }

    static auto GateReason(const FCk_Handle& InEntity) -> FString
    { return ck::DebugRequestGate::Evaluate(InEntity, ECk_DebugRequest_Requirement::CosmeticOnly).Reason.ToString(); }

    static auto GetLayerLabel(const FCk_Handle_CameraLayer& InLayer) -> FString
    {
        if (NOT InLayer.Has<ck::FFragment_CameraLayer_Params>())
        { return TEXT("(layer)"); }

        const auto& Params = InLayer.Get<ck::FFragment_CameraLayer_Params>();
        if (Params.Get_IsDefault())
        { return TEXT("Base (resting)"); }

        return FString::Printf(TEXT("%s [p%d]"), *DoFmt_ClassName(Params.Get_LayerClass().Get()), Params.Get_Priority());
    }

    // Fixed-precision components in X/Y/Z order so AddAlignedNumericRow's index-based axis coloring
    // lines up with the axis each number belongs to, and every spatial row shares one column grid.
    static auto Make_Components(
        const FCk_Handle& InCamera,
        int32 InComponentCount,
        const TCHAR* InFormat,
        TFunction<double(const ck::FFragment_Camera_Current&, int32)> InProjector)
        -> TArray<TAttribute<FText>>
    {
        auto Components = TArray<TAttribute<FText>>{};
        Components.Reserve(InComponentCount);

        for (auto Index = 0; Index < InComponentCount; ++Index)
        {
            Components.Emplace(TAttribute<FText>::CreateLambda([InCamera, InFormat, InProjector, Index]()
            {
                if (ck::Is_NOT_Valid(InCamera) || NOT InCamera.Has<ck::FFragment_Camera_Current>())
                { return FText::FromString(TEXT("--")); }

                return FText::FromString(ck::Format_UE(InFormat,
                    InProjector(InCamera.Get<ck::FFragment_Camera_Current>(), Index)));
            }));
        }

        return Components;
    }

    static auto Make_VectorComponents(
        const FCk_Handle& InCamera,
        TFunction<FVector(const ck::FFragment_Camera_Current&)> InProjector)
        -> TArray<TAttribute<FText>>
    {
        return Make_Components(InCamera, 3, TEXT("{:.2f}"),
            [InProjector](const ck::FFragment_Camera_Current& InCurrent, int32 InIndex)
            { return InProjector(InCurrent)[InIndex]; });
    }

    // Roll=X, Pitch=Y, Yaw=Z — the axis each angle turns about, so the row's coloring agrees with
    // the vector rows above it. Every rotation label states the order.
    static auto Make_RotatorComponents(
        const FCk_Handle& InCamera,
        TFunction<FRotator(const ck::FFragment_Camera_Current&)> InProjector)
        -> TArray<TAttribute<FText>>
    {
        return Make_Components(InCamera, 3, TEXT("{:.2f}"),
            [InProjector](const ck::FFragment_Camera_Current& InCurrent, int32 InIndex)
            {
                const auto Rotator = InProjector(InCurrent);
                return FVector{Rotator.Roll, Rotator.Pitch, Rotator.Yaw}[InIndex];
            });
    }
}

// =====================================================================================================================

auto FCkInspector_Camera::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Gameplay Camera"));
}

auto FCkInspector_Camera::CanInspect(const FCk_Handle& Entity) const -> bool
{
    if (ck::Is_NOT_Valid(Entity))
    { return false; }

    return Entity.Has_Any<
        ck::FFragment_Camera_Params,
        ck::FFragment_Camera_Current,
        ck::FFragment_CameraLayer_Params,
        ck::FFragment_CameraLayer_Blend>();
}

// =====================================================================================================================

auto FCkInspector_Camera::Build_NativeBody(const FCk_Handle& Entity) const -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();
    Builder.SetEditGuard(Get_EditGuard());

    if (ck::Is_NOT_Valid(Entity))
    { return Builder.Build(Entity); }

    const auto Cam = Entity;

    // ---- Director ----
    if (Entity.Has<ck::FFragment_Camera_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Gameplay Camera")));

        Builder.AddRow(
            FText::FromString(TEXT("Orientation Intention:")),
            [Cam](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(Cam) || NOT Cam.Has<ck::FFragment_Camera_Current>())
                { return FText::FromString(TEXT("--")); }
                return FText::FromString(ck::Format_UE(TEXT("{}"),
                    Cam.Get<ck::FFragment_Camera_Current>().Get_OrientationIntention()));
            },
            CkStyle::Value_Math());

        Builder.AddRow(
            FText::FromString(TEXT("Dominant Modifier:")),
            [Cam](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(Cam) || NOT Cam.Has<ck::FFragment_Camera_Current>())
                { return FText::FromString(TEXT("--")); }
                return FText::FromString(DoFmt_ClassName(
                    Cam.Get<ck::FFragment_Camera_Current>().Get_DominantLayerClass().Get()));
            },
            CkStyle::Value_Object());

        Builder.AddRow(
            FText::FromString(TEXT("Dominant Look-At:")),
            [Cam](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(Cam) || NOT Cam.Has<ck::FFragment_Camera_Current>())
                { return FText::FromString(TEXT("--")); }
                const auto& LookAt = Cam.Get<ck::FFragment_Camera_Current>().Get_DominantLookAt();
                return FText::FromString(LookAt.IsSet()
                    ? ck::Format_UE(TEXT("{}"), LookAt.GetValue())
                    : FString(TEXT("(none)")));
            },
            CkStyle::Value_Math());

        // ---- Composed profile summary ----
        Builder.AddHeader(FText::FromString(TEXT("Composed Profile")));

        Builder.AddRow(
            FText::FromString(TEXT("FOV:")),
            [Cam](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(Cam) || NOT Cam.Has<ck::FFragment_Camera_Current>())
                { return FText::FromString(TEXT("--")); }
                return DoFmt_Float(Cam.Get<ck::FFragment_Camera_Current>()
                    .Get_ComposedProfile().Get_Sensor().Get_FOV(), TEXT("{:.1f}"));
            },
            CkStyle::Value_Numeric());

        Builder.AddRow(
            FText::FromString(TEXT("Boom Length:")),
            [Cam](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(Cam) || NOT Cam.Has<ck::FFragment_Camera_Current>())
                { return FText::FromString(TEXT("--")); }
                return DoFmt_Float(Cam.Get<ck::FFragment_Camera_Current>()
                    .Get_ComposedProfile().Get_Rig().Get_BoomArmLength(), TEXT("{:.0f}"));
            },
            CkStyle::Value_Numeric());

        Builder.AddAlignedNumericRow(
            FText::FromString(TEXT("Framing Offset:")),
            ck_inspector_camera::Make_VectorComponents(Cam,
                [](const ck::FFragment_Camera_Current& InCurrent)
                { return InCurrent.Get_ComposedProfile().Get_Rig().Get_FramingOffset(); }));

        Builder.AddAlignedNumericRow(
            FText::FromString(TEXT("Framing Pitch/Yaw:")),
            ck_inspector_camera::Make_Components(Cam, 2, TEXT("{:.1f}"),
                [](const ck::FFragment_Camera_Current& InCurrent, int32 InIndex)
                {
                    const auto& Rig = InCurrent.Get_ComposedProfile().Get_Rig();
                    return InIndex == 0 ? Rig.Get_FramingPitch() : Rig.Get_FramingYaw();
                }));

        Builder.AddConditionalRow(
            FText::FromString(TEXT("Orientation Control:")),
            [Cam](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(Cam) || NOT Cam.Has<ck::FFragment_Camera_Current>())
                { return FText::FromString(TEXT("--")); }
                return DoFmt_Bool(Cam.Get<ck::FFragment_Camera_Current>().Get_ComposedProfile().Get_HasOrientationControl());
            },
            [Cam](const FCk_Handle&) -> FLinearColor
            {
                if (ck::Is_NOT_Valid(Cam) || NOT Cam.Has<ck::FFragment_Camera_Current>())
                { return CkStyle::None(); }
                return DoColor_Flag(Cam.Get<ck::FFragment_Camera_Current>().Get_ComposedProfile().Get_HasOrientationControl());
            });

        Builder.AddConditionalRow(
            FText::FromString(TEXT("Auto-Reorient:")),
            [Cam](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(Cam) || NOT Cam.Has<ck::FFragment_Camera_Current>())
                { return FText::FromString(TEXT("--")); }
                return DoFmt_Bool(Cam.Get<ck::FFragment_Camera_Current>().Get_ComposedProfile().Get_HasAutoReorient());
            },
            [Cam](const FCk_Handle&) -> FLinearColor
            {
                if (ck::Is_NOT_Valid(Cam) || NOT Cam.Has<ck::FFragment_Camera_Current>())
                { return CkStyle::None(); }
                return DoColor_Flag(Cam.Get<ck::FFragment_Camera_Current>().Get_ComposedProfile().Get_HasAutoReorient());
            });

        Builder.AddConditionalRow(
            FText::FromString(TEXT("Collision:")),
            [Cam](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(Cam) || NOT Cam.Has<ck::FFragment_Camera_Current>())
                { return FText::FromString(TEXT("--")); }
                return DoFmt_Bool(Cam.Get<ck::FFragment_Camera_Current>().Get_ComposedProfile().Get_HasCollision());
            },
            [Cam](const FCk_Handle&) -> FLinearColor
            {
                if (ck::Is_NOT_Valid(Cam) || NOT Cam.Has<ck::FFragment_Camera_Current>())
                { return CkStyle::None(); }
                return DoColor_Flag(Cam.Get<ck::FFragment_Camera_Current>().Get_ComposedProfile().Get_HasCollision());
            });

        // ---- Live controls ----
        //
        // The seven non-blending bool leaves, the boom snap, the yaw clamp window, and the orbit
        // intention — every one a public UCk_Utils_Camera_UE Request_*, fired against a typed
        // FCk_Handle_Camera captured BY VALUE and ck::IsValid-checked on each commit.
        //
        // All CosmeticOnly: a dedicated server resolves no local view, so the gate greys these out
        // there with the reason in the tooltip rather than firing a request nobody can see.
        //
        // Reads come from the SAME composed-profile getters the display rows above use, so a toggle
        // reflects what the camera actually resolved this frame — including a layer overriding the
        // leaf — rather than the last value this panel wrote.
        //
        // The CameraLayer Acquire -> Override modifier protocol is deliberately NOT exposed: acquiring
        // a tuner modifier is a multi-step layer-scoped lifetime, not a single request.
        {
            auto       MutableCameraEntity = Entity;
            const auto CapturedCamera      = UCk_Utils_Camera_UE::Cast(MutableCameraEntity);

            Builder.AddHeader(FText::FromString(TEXT("Camera Controls")));

            const auto AddFlagToggle = [&Builder, Cam, CapturedCamera](
                const TCHAR*                              InLabel,
                TFunction<bool(const FCk_CameraProfile&)> InGet,
                TFunction<void(FCk_Handle_Camera&, bool)> InSet)
            {
                Builder.AddToggleRow(
                    FText::FromString(InLabel),
                    TAttribute<bool>::CreateLambda([Cam, InGet]()
                    {
                        if (ck::Is_NOT_Valid(Cam) || NOT Cam.Has<ck::FFragment_Camera_Current>())
                        { return false; }

                        return InGet(Cam.Get<ck::FFragment_Camera_Current>().Get_ComposedProfile());
                    }),
                    [CapturedCamera, InSet](bool InIsEnabled)
                    {
                        auto MutableCamera = FCk_Handle_Camera{};
                        if (NOT ck_inspector_camera::TryGetCamera(CapturedCamera, MutableCamera, false))
                        { return; }

                        InSet(MutableCamera, InIsEnabled);
                    },
                    ECk_DebugRequest_Requirement::CosmeticOnly);
            };

            AddFlagToggle(TEXT("Use Fixed Boom Rotation:"),
                [](const FCk_CameraProfile& InProfile) { return InProfile.Get_Rig().Get_UseFixedBoomRotation(); },
                [](FCk_Handle_Camera& InCamera, bool InIsEnabled) { UCk_Utils_Camera_UE::Request_Set_UseFixedBoomRotation(InCamera, InIsEnabled, {}); });

            AddFlagToggle(TEXT("Constrain Aspect Ratio:"),
                [](const FCk_CameraProfile& InProfile) { return InProfile.Get_Sensor().Get_ConstrainAspectRatio(); },
                [](FCk_Handle_Camera& InCamera, bool InIsEnabled) { UCk_Utils_Camera_UE::Request_Set_ConstrainAspectRatio(InCamera, InIsEnabled, {}); });

            AddFlagToggle(TEXT("Has Orientation Control:"),
                [](const FCk_CameraProfile& InProfile) { return InProfile.Get_HasOrientationControl(); },
                [](FCk_Handle_Camera& InCamera, bool InIsEnabled) { UCk_Utils_Camera_UE::Request_Set_HasOrientationControl(InCamera, InIsEnabled, {}); });

            AddFlagToggle(TEXT("Has Auto-Reorient:"),
                [](const FCk_CameraProfile& InProfile) { return InProfile.Get_HasAutoReorient(); },
                [](FCk_Handle_Camera& InCamera, bool InIsEnabled) { UCk_Utils_Camera_UE::Request_Set_HasAutoReorient(InCamera, InIsEnabled, {}); });

            AddFlagToggle(TEXT("Has Collision:"),
                [](const FCk_CameraProfile& InProfile) { return InProfile.Get_HasCollision(); },
                [](FCk_Handle_Camera& InCamera, bool InIsEnabled) { UCk_Utils_Camera_UE::Request_Set_HasCollision(InCamera, InIsEnabled, {}); });

            AddFlagToggle(TEXT("Use Async Trace:"),
                [](const FCk_CameraProfile& InProfile) { return InProfile.Get_Collision().Get_UseAsyncTrace(); },
                [](FCk_Handle_Camera& InCamera, bool InIsEnabled) { UCk_Utils_Camera_UE::Request_Set_UseAsyncTrace(InCamera, InIsEnabled, {}); });

            AddFlagToggle(TEXT("Use Post Process:"),
                [](const FCk_CameraProfile& InProfile) { return InProfile.Get_UsePostProcess(); },
                [](FCk_Handle_Camera& InCamera, bool InIsEnabled) { UCk_Utils_Camera_UE::Request_Set_UsePostProcess(InCamera, InIsEnabled, {}); });

            // Absolute seed of the persistent boom rotation — the orbit equivalent of a teleport. Reads
            // the live POV boom rotation, so the row shows where the boom IS before you retarget it.
            Builder.AddRotatorRow(
                FText::FromString(TEXT("Snap Boom Rotation (R,P,Y):")),
                TAttribute<FRotator>::CreateLambda([Cam]()
                {
                    if (ck::Is_NOT_Valid(Cam) || NOT Cam.Has<ck::FFragment_Camera_Current>())
                    { return FRotator::ZeroRotator; }

                    return Cam.Get<ck::FFragment_Camera_Current>().Get_PovState()._BoomArmRotation;
                }),
                [CapturedCamera](const FRotator& InWorldRotation)
                {
                    auto MutableCamera = FCk_Handle_Camera{};
                    if (NOT ck_inspector_camera::TryGetCamera(CapturedCamera, MutableCamera, false))
                    { return; }

                    UCk_Utils_Camera_UE::Request_SnapBoomRotation(MutableCamera, InWorldRotation, {});
                },
                ECk_DebugRequest_Requirement::CosmeticOnly);

            // Yaw clamp WINDOW (absolute world yaws), not a +/- half-angle. The request takes both edges
            // at once, so each row re-reads the other edge live at commit time instead of caching it.
            const auto Read_YawLimits = [Cam]() -> FCk_FloatRange
            {
                if (ck::Is_NOT_Valid(Cam)
                    || NOT Cam.Has_All<ck::FFragment_Camera_Current, ck::FFragment_Camera_OrientationControl>())
                { return FCk_FloatRange{}; }

                return Cam.Get<ck::FFragment_Camera_Current>()
                    .Get_ComposedProfile().Get_OrientationControl().Get_Yaw().Get_Limits();
            };

            Builder.AddNumericRow(
                FText::FromString(TEXT("Orientation Yaw Min:")),
                TAttribute<float>::CreateLambda([Read_YawLimits]() { return static_cast<float>(Read_YawLimits().Get_Min()); }),
                [CapturedCamera, Read_YawLimits](float InMinYaw)
                {
                    auto MutableCamera = FCk_Handle_Camera{};
                    if (NOT ck_inspector_camera::TryGetCamera(CapturedCamera, MutableCamera, true))
                    { return; }

                    UCk_Utils_Camera_UE::Request_Set_OrientationYawLimits(
                        MutableCamera, InMinYaw, static_cast<float>(Read_YawLimits().Get_Max()), {});
                },
                TOptional<float>{},
                TOptional<float>{},
                ECk_DebugRequest_Requirement::CosmeticOnly);

            Builder.AddNumericRow(
                FText::FromString(TEXT("Orientation Yaw Max:")),
                TAttribute<float>::CreateLambda([Read_YawLimits]() { return static_cast<float>(Read_YawLimits().Get_Max()); }),
                [CapturedCamera, Read_YawLimits](float InMaxYaw)
                {
                    auto MutableCamera = FCk_Handle_Camera{};
                    if (NOT ck_inspector_camera::TryGetCamera(CapturedCamera, MutableCamera, true))
                    { return; }

                    UCk_Utils_Camera_UE::Request_Set_OrientationYawLimits(
                        MutableCamera, static_cast<float>(Read_YawLimits().Get_Min()), InMaxYaw, {});
                },
                TOptional<float>{},
                TOptional<float>{},
                ECk_DebugRequest_Requirement::CosmeticOnly);

            // A per-frame DELTA that UpdatePOV CONSUMES (resets to zero) after applying — so this row
            // reads back 0,0,0 on the very next frame. Committing it is a one-frame orbit nudge, not a
            // setting; the read-only "Orientation Intention" row above shows the same live value.
            Builder.AddVectorRow(
                FText::FromString(TEXT("Push Orientation Intention:")),
                TAttribute<FVector>::CreateLambda([Cam]()
                {
                    if (ck::Is_NOT_Valid(Cam) || NOT Cam.Has<ck::FFragment_Camera_Current>())
                    { return FVector::ZeroVector; }

                    return Cam.Get<ck::FFragment_Camera_Current>().Get_OrientationIntention();
                }),
                [CapturedCamera](const FVector& InIntention)
                {
                    auto MutableCamera = FCk_Handle_Camera{};
                    if (NOT ck_inspector_camera::TryGetCamera(CapturedCamera, MutableCamera, false))
                    { return; }

                    UCk_Utils_Camera_UE::Request_SetOrientationIntention(MutableCamera, InIntention, {});
                },
                ECk_DebugRequest_Requirement::CosmeticOnly);
        }

        // ---- Resolved view info ----
        Builder.AddHeader(FText::FromString(TEXT("View Info (resolved)")));

        Builder.AddAlignedNumericRow(
            FText::FromString(TEXT("Location:")),
            ck_inspector_camera::Make_VectorComponents(Cam,
                [](const ck::FFragment_Camera_Current& InCurrent)
                { return InCurrent.Get_ViewInfo().Location; }));

        Builder.AddAlignedNumericRow(
            FText::FromString(TEXT("Rotation (R,P,Y):")),
            ck_inspector_camera::Make_RotatorComponents(Cam,
                [](const ck::FFragment_Camera_Current& InCurrent)
                { return InCurrent.Get_ViewInfo().Rotation; }));

        Builder.AddRow(
            FText::FromString(TEXT("FOV:")),
            [Cam](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(Cam) || NOT Cam.Has<ck::FFragment_Camera_Current>())
                { return FText::FromString(TEXT("--")); }
                return DoFmt_Float(Cam.Get<ck::FFragment_Camera_Current>().Get_ViewInfo().FOV, TEXT("{:.1f}"));
            },
            CkStyle::Value_Numeric());
    }

    // ---- Layer stack (one live row per record entry, with a blend-weight bar; the persistent base layer is marked) ----
    if (Entity.Has<ck::FFragment_Camera_Current>())
    {
        auto MutableEntity = Entity;
        auto Layers        = TArray<FCk_Handle_CameraLayer>{};
        ck::FUtils_RecordOfCameraLayers::ForEach_ValidEntry(MutableEntity,
        [&Layers](FCk_Handle_CameraLayer InLayer)
        {
            Layers.Add(InLayer);
        });

        Builder.AddHeader(FText::FromString(ck::Format_UE(TEXT("Layer Stack ({})"), Layers.Num())));

        if (Layers.IsEmpty())
        {
            Builder.AddRow(
                FText::FromString(TEXT("(empty)")),
                [](const FCk_Handle&) { return FText::FromString(TEXT("--")); },
                CkStyle::TextMute());
        }
        else
        {
            for (const auto& Layer : Layers)
            {
                const auto Lyr       = Layer;
                auto       IsDefault = false;
                const auto LabelStr  = ck_inspector_camera::GetLayerLabel(Lyr);

                if (Lyr.Has<ck::FFragment_CameraLayer_Params>())
                {
                    const auto& Params = Lyr.Get<ck::FFragment_CameraLayer_Params>();
                    IsDefault = Params.Get_IsDefault();
                }

                // Tone the weight meter + readout by state: base = info, exiting = warn,
                // active = ok, pending = neutral. Live, so a layer blending out re-tones itself
                // without the inspector rebuilding.
                const auto ToneOf = TAttribute<ECk_Tone>::CreateLambda([Lyr, IsDefault]() -> ECk_Tone
                {
                    if (ck::Is_NOT_Valid(Lyr))
                    { return ECk_Tone::Neutral; }
                    if (IsDefault)
                    { return ECk_Tone::Info; }
                    if (Lyr.Has<ck::FFragment_CameraLayer_Blend>() && Lyr.Get<ck::FFragment_CameraLayer_Blend>().Get_TargetAlpha() <= 0.0f)
                    { return ECk_Tone::Warn; }
                    if (Lyr.Has<ck::FTag_CameraLayer_Active>())
                    { return ECk_Tone::Ok; }
                    return ECk_Tone::Neutral;
                });

                Builder.AddMeterRow(
                    FText::FromString(LabelStr),
                    TAttribute<float>::CreateLambda([Lyr]()
                    {
                        if (ck::Is_NOT_Valid(Lyr) || NOT Lyr.Has<ck::FFragment_CameraLayer_Blend>())
                        { return 0.0f; }
                        return Lyr.Get<ck::FFragment_CameraLayer_Blend>().Get_Alpha();
                    }),
                    ToneOf,
                    TAttribute<FText>::CreateLambda([Lyr]() -> FText
                    {
                        if (ck::Is_NOT_Valid(Lyr) || NOT Lyr.Has<ck::FFragment_CameraLayer_Blend>())
                        { return FText::FromString(TEXT("--")); }
                        const auto& Blend    = Lyr.Get<ck::FFragment_CameraLayer_Blend>();
                        const auto  Alpha    = Blend.Get_Alpha();
                        const auto  bExiting = Blend.Get_TargetAlpha() <= 0.0f;
                        const auto* State    = bExiting                               ? TEXT(" exit")
                                             : Lyr.Has<ck::FTag_CameraLayer_Active>() ? TEXT("")
                                             :                                          TEXT(" pend");
                        return FText::FromString(FString::Printf(TEXT("%.2f%s"), Alpha, State));
                    }));
            }
        }
    }

    // ---- POV pipeline intermediates ----
    if (Entity.Has<ck::FFragment_Camera_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("POV Pipeline")));

        const auto AddPovVectorRow = [&Builder, Cam](const FString& InLabel, TFunction<FVector(const ck::camera::FPov_State&)> InGet)
        {
            Builder.AddAlignedNumericRow(
                FText::FromString(InLabel),
                ck_inspector_camera::Make_VectorComponents(Cam,
                    [InGet](const ck::FFragment_Camera_Current& InCurrent)
                    { return InGet(InCurrent.Get_PovState()); }));
        };

        const auto AddPovRotatorRow = [&Builder, Cam](const FString& InLabel, TFunction<FRotator(const ck::camera::FPov_State&)> InGet)
        {
            Builder.AddAlignedNumericRow(
                FText::FromString(InLabel),
                ck_inspector_camera::Make_RotatorComponents(Cam,
                    [InGet](const ck::FFragment_Camera_Current& InCurrent)
                    { return InGet(InCurrent.Get_PovState()); }));
        };

        AddPovRotatorRow(TEXT("Boom Rotation (R,P,Y):"), [](const auto& P) { return P._BoomArmRotation; });
        AddPovVectorRow (TEXT("Group-Base Loc:"),        [](const auto& P) { return P._GroupBaseLocation; });
        AddPovVectorRow (TEXT("Look-At Loc:"),           [](const auto& P) { return P._LookAtLocation; });
        AddPovVectorRow (TEXT("Boom-End Loc:"),          [](const auto& P) { return P._BoomArmEndTransform.GetLocation(); });
        AddPovVectorRow (TEXT("Framing Loc:"),           [](const auto& P) { return P._FramingTransform.GetLocation(); });
        AddPovVectorRow (TEXT("Camera Loc:"),            [](const auto& P) { return P._CameraTransform.GetLocation(); });
        AddPovRotatorRow(TEXT("Noise (R,P,Y):"),         [](const auto& P) { return P._NoiseRotator; });

        Builder.AddRow(
            FText::FromString(TEXT("Collision Dist:")),
            [Cam](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(Cam) || NOT Cam.Has<ck::FFragment_Camera_Current>())
                { return FText::FromString(TEXT("--")); }
                const auto& Distance = Cam.Get<ck::FFragment_Camera_Current>().Get_PovState()._CollisionDistance;
                return Distance.IsSet()
                    ? FText::FromString(ck::Format_UE(TEXT("{:.1f}"), Distance.GetValue()))
                    : FText::FromString(TEXT("-"));
            },
            CkStyle::Value_Numeric());
    }

    // ---- Modifier detail (when a modifier child entity is selected) ----
    if (Entity.Has_Any<ck::FFragment_CameraLayer_Params, ck::FFragment_CameraLayer_Blend>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Camera Modifier")));

        if (Entity.Has<ck::FFragment_CameraLayer_Params>())
        {
            const auto& Params       = Entity.Get<ck::FFragment_CameraLayer_Params>();
            const auto  ClassName    = DoFmt_ClassName(Params.Get_LayerClass().Get());
            const auto  PriorityStr  = FString::Printf(TEXT("%d"), Params.Get_Priority());
            const auto& CameraTarget = Params.Get_CameraTarget();
            const auto  TargetHandle = CameraTarget.Get_Target();
            const FString ModeStr    = CameraTarget.Get_Mode() == ECk_Camera_TargetMode::ViewTarget ? TEXT("ViewTarget") : TEXT("LookAt");
            const auto  TargetStr    = ck::IsValid(TargetHandle) ? ck::Format_UE(TEXT("{} ({})"), TargetHandle, ModeStr) : FString(TEXT("(none)"));

            Builder.AddRow(FText::FromString(TEXT("Class:")),
                [ClassName](const FCk_Handle&) { return FText::FromString(ClassName); }, CkStyle::Value_Object());
            Builder.AddRow(FText::FromString(TEXT("Priority:")),
                [PriorityStr](const FCk_Handle&) { return FText::FromString(PriorityStr); }, CkStyle::Value_Numeric());
            Builder.AddRow(FText::FromString(TEXT("Camera Target:")),
                [TargetStr](const FCk_Handle&) { return FText::FromString(TargetStr); }, CkStyle::Value_Handle());
        }

        if (Entity.Has<ck::FFragment_CameraLayer_Blend>())
        {
            Builder.AddMeterRow(
                FText::FromString(TEXT("Alpha (cur → target):")),
                TAttribute<float>::CreateLambda([Cam]()
                {
                    if (ck::Is_NOT_Valid(Cam) || NOT Cam.Has<ck::FFragment_CameraLayer_Blend>())
                    { return 0.0f; }
                    return Cam.Get<ck::FFragment_CameraLayer_Blend>().Get_Alpha();
                }),
                ECk_Tone::Accent,
                TAttribute<FText>::CreateLambda([Cam]()
                {
                    if (ck::Is_NOT_Valid(Cam) || NOT Cam.Has<ck::FFragment_CameraLayer_Blend>())
                    { return FText::FromString(TEXT("--")); }
                    const auto& Blend = Cam.Get<ck::FFragment_CameraLayer_Blend>();
                    return FText::FromString(ck::Format_UE(TEXT("{:.2f} → {:.2f}"),
                        Blend.Get_Alpha(), Blend.Get_TargetAlpha()));
                }));

            Builder.AddRow(FText::FromString(TEXT("Blend Rate:")),
                [Cam](const FCk_Handle&)
                {
                    if (ck::Is_NOT_Valid(Cam) || NOT Cam.Has<ck::FFragment_CameraLayer_Blend>())
                    { return FText::FromString(TEXT("--")); }
                    return DoFmt_Float(Cam.Get<ck::FFragment_CameraLayer_Blend>().Get_BlendRate(), TEXT("{:.1f}"));
                }, CkStyle::Value_Numeric());
        }

        Builder.AddStatusPillRow(
            FText::FromString(TEXT("State:")),
            TAttribute<FText>::CreateLambda([Cam]()
            {
                if (ck::Is_NOT_Valid(Cam))
                { return FText::FromString(TEXT("--")); }
                const auto IsExiting = Cam.Has<ck::FFragment_CameraLayer_Blend>() && Cam.Get<ck::FFragment_CameraLayer_Blend>().Get_TargetAlpha() <= 0.0f;
                if (IsExiting)                              { return FText::FromString(TEXT("Exiting")); }
                if (Cam.Has<ck::FTag_CameraLayer_Active>()) { return FText::FromString(TEXT("Active")); }
                return FText::FromString(TEXT("Pending"));
            }),
            TAttribute<ECk_Tone>::CreateLambda([Cam]()
            {
                if (ck::Is_NOT_Valid(Cam))                  { return ECk_Tone::Neutral; }
                const auto IsExiting = Cam.Has<ck::FFragment_CameraLayer_Blend>() && Cam.Get<ck::FFragment_CameraLayer_Blend>().Get_TargetAlpha() <= 0.0f;
                if (IsExiting)                              { return ECk_Tone::Warn; }
                if (Cam.Has<ck::FTag_CameraLayer_Active>()) { return ECk_Tone::Ok; }
                return ECk_Tone::Neutral;
            }));
    }

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto FCkInspector_Camera::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_CameraAuthored>& InInstance)
    { return NOT InInstance.IsValid() || InInstance.Pin()->Is_Inert(); });

    if (ck::Is_NOT_Valid(Entity) || NOT Entity.Has<ck::FFragment_Camera_Current>())
    { return; }

    // Rebuild the stack section when the live modifier count changes (modifiers are added / pruned over time).
    {
        auto MutableEntity = Entity;
        auto Count = int32{0};
        ck::FUtils_RecordOfCameraLayers::ForEach_ValidEntry(MutableEntity,
        [&Count](FCk_Handle_CameraLayer) { ++Count; });

        if (_AuthoredInstances.IsEmpty() && Count != _LastModifierCount)
        {
            _LastModifierCount = Count;
            RequestRebuild();
        }
    }

    // In-world visualization of the resolved rig — only for the director (an inspected modifier has no transform/POV).
    if (NOT Entity.Has<ck::FFragment_Transform>())
    { return; }

    const auto EntityWorld = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(Entity);
    if (ck::Is_NOT_Valid(EntityWorld))
    { return; }

    const auto& Current = Entity.Get<ck::FFragment_Camera_Current>();
    const auto& Pov     = Current.Get_PovState();

    const auto AnchorLocation = Pov._GroupBaseLocation;
    const auto CameraLocation = Pov._CameraTransform.GetLocation();
    const auto CameraRotation = Pov._CameraTransform.Rotator();

    constexpr auto NoDuration = 0.0f;

    // Boom arm — red while collision is actively pushing the camera in, info-blue otherwise.
    const auto BoomColor = Pov._CollisionDistance.IsSet() ? CkStyle::Err() : CkStyle::Info();
    UCk_Utils_DebugDraw_UE::DrawDebugLine(EntityWorld, AnchorLocation, CameraLocation, BoomColor, NoDuration, 1.5f);

    // Camera pose: axes + a forward arrow showing view direction.
    UCk_Utils_DebugDraw_UE::DrawDebugCoordinateSystem(EntityWorld, CameraLocation, CameraRotation, 0.5f, NoDuration, 1.5f);
    UCk_Utils_DebugDraw_UE::DrawDebugArrow(
        EntityWorld, CameraLocation, CameraLocation + (CameraRotation.Vector() * 150.0f), 25.0f, CkStyle::Accent(), NoDuration, 1.5f);

    // Look-at target (auto-reorient / lock-on).
    if (Current.Get_DominantLookAt().IsSet())
    {
        const auto Target = Current.Get_DominantLookAt().GetValue();
        UCk_Utils_DebugDraw_UE::DrawDebugLine(EntityWorld, CameraLocation, Target, CkStyle::Warn(), NoDuration, 1.0f);
        UCk_Utils_DebugDraw_UE::DrawDebugSphere(EntityWorld, Target, 24.0f, 12, CkStyle::Warn(), NoDuration, 1.0f);
    }

    UCk_Utils_DebugDraw_UE::DrawDebugString(
        EntityWorld, CameraLocation + FVector(0.0f, 0.0f, 20.0f), Entity.ToString(), CkStyle::Text(), NoDuration);
}

// =====================================================================================================================

auto SCkInspector_CameraAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _DiffLabels = InArgs._DiffLabels;
    const TSharedRef<SBox> Root = SNew(SBox);
    ChildSlot[Root];
    if (Refresh_Layers() && Build_AuthoredView())
    { Root->SetContent(_View->GetRegion(TEXT("main"))); return; }
    Release();
    Root->SetContent(SNullWidget::NullWidget);
}

SCkInspector_CameraAuthored::~SCkInspector_CameraAuthored()
{ Release(); }

auto SCkInspector_CameraAuthored::Get_IsAvailable() const -> bool
{ return _Active && NOT ck_inspector_camera::IsDestroying(_Entity) && _Entity.Has<ck::FFragment_Camera_Current>(); }

auto SCkInspector_CameraAuthored::Get_IsLayerAvailable() const -> bool
{
    return _Active && NOT ck_inspector_camera::IsDestroying(_Entity)
        && _Entity.Has_Any<ck::FFragment_CameraLayer_Params, ck::FFragment_CameraLayer_Blend>();
}

auto SCkInspector_CameraAuthored::Get_CanEdit() const -> bool
{
    auto Camera = FCk_Handle_Camera{};
    return _Active && ck_inspector_camera::TryGetCamera(_Entity, Camera, false)
        && ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::CosmeticOnly).IsEnabled;
}

auto SCkInspector_CameraAuthored::Get_EditDisabledReason() const -> FString
{
    auto Camera = FCk_Handle_Camera{};
    return ck_inspector_camera::TryGetCamera(_Entity, Camera, false)
        ? ck_inspector_camera::GateReason(_Entity) : TEXT("Camera Current is unavailable or being destroyed.");
}

auto SCkInspector_CameraAuthored::Get_Text(const FString& InKey) const -> FString
{
    if (Get_IsLayerAvailable())
    {
        if (InKey == TEXT("layer-class"))
        {
            return _Entity.Has<ck::FFragment_CameraLayer_Params>()
                ? DoFmt_ClassName(_Entity.Get<ck::FFragment_CameraLayer_Params>().Get_LayerClass().Get()) : TEXT("--");
        }
        if (InKey == TEXT("layer-priority"))
        {
            return _Entity.Has<ck::FFragment_CameraLayer_Params>()
                ? FString::FromInt(_Entity.Get<ck::FFragment_CameraLayer_Params>().Get_Priority()) : TEXT("--");
        }
        if (InKey == TEXT("layer-target"))
        {
            if (NOT _Entity.Has<ck::FFragment_CameraLayer_Params>()) { return TEXT("--"); }
            const auto& Target = _Entity.Get<ck::FFragment_CameraLayer_Params>().Get_CameraTarget();
            const auto Handle = Target.Get_Target();
            return ck::IsValid(Handle)
                ? ck::Format_UE(TEXT("{} ({})"), Handle, Target.Get_Mode() == ECk_Camera_TargetMode::ViewTarget ? TEXT("ViewTarget") : TEXT("LookAt"))
                : TEXT("(none)");
        }
        if (InKey == TEXT("layer-alpha"))
        {
            if (NOT _Entity.Has<ck::FFragment_CameraLayer_Blend>()) { return TEXT("--"); }
            const auto& Blend = _Entity.Get<ck::FFragment_CameraLayer_Blend>();
            return ck::Format_UE(TEXT("{:.2f} → {:.2f}"), Blend.Get_Alpha(), Blend.Get_TargetAlpha());
        }
        if (InKey == TEXT("layer-rate")) return _Entity.Has<ck::FFragment_CameraLayer_Blend>() ? ck::Format_UE(TEXT("{:.1f}"), _Entity.Get<ck::FFragment_CameraLayer_Blend>().Get_BlendRate()) : TEXT("--");
        if (InKey == TEXT("layer-state"))
        {
            if (_Entity.Has<ck::FFragment_CameraLayer_Blend>() && _Entity.Get<ck::FFragment_CameraLayer_Blend>().Get_TargetAlpha() <= 0.0f) return TEXT("Exiting");
            return _Entity.Has<ck::FTag_CameraLayer_Active>() ? TEXT("Active") : TEXT("Pending");
        }
    }
    if (NOT Get_IsAvailable()) { return TEXT("--"); }
    const auto& Current = _Entity.Get<ck::FFragment_Camera_Current>();
    const auto& Profile = Current.Get_ComposedProfile();
    const auto& Pov = Current.Get_PovState();
    const auto FormatVector = [](const FVector& V) { return FString::Printf(TEXT("%.2f  %.2f  %.2f"), V.X, V.Y, V.Z); };
    const auto FormatRotator = [](const FRotator& R) { return FString::Printf(TEXT("%.2f  %.2f  %.2f"), R.Roll, R.Pitch, R.Yaw); };
    if (InKey == TEXT("orientation")) return ck::Format_UE(TEXT("{}"), Current.Get_OrientationIntention());
    if (InKey == TEXT("dominant")) return DoFmt_ClassName(Current.Get_DominantLayerClass().Get());
    if (InKey == TEXT("look-at")) return Current.Get_DominantLookAt().IsSet() ? ck::Format_UE(TEXT("{}"), Current.Get_DominantLookAt().GetValue()) : TEXT("(none)");
    if (InKey == TEXT("fov")) return ck::Format_UE(TEXT("{:.1f}"), Profile.Get_Sensor().Get_FOV());
    if (InKey == TEXT("boom-length")) return ck::Format_UE(TEXT("{:.0f}"), Profile.Get_Rig().Get_BoomArmLength());
    if (InKey == TEXT("framing-offset")) return FormatVector(Profile.Get_Rig().Get_FramingOffset());
    if (InKey == TEXT("framing-pitch-yaw")) return ck::Format_UE(TEXT("{:.1f}, {:.1f}"), Profile.Get_Rig().Get_FramingPitch(), Profile.Get_Rig().Get_FramingYaw());
    if (InKey == TEXT("orientation-control")) return Current.Get_ComposedProfile().Get_HasOrientationControl() ? TEXT("Yes") : TEXT("No");
    if (InKey == TEXT("auto-reorient")) return Profile.Get_HasAutoReorient() ? TEXT("Yes") : TEXT("No");
    if (InKey == TEXT("collision")) return Profile.Get_HasCollision() ? TEXT("Yes") : TEXT("No");
    if (InKey == TEXT("view-location")) return FormatVector(Current.Get_ViewInfo().Location);
    if (InKey == TEXT("view-rotation")) return FormatRotator(Current.Get_ViewInfo().Rotation);
    if (InKey == TEXT("view-fov")) return ck::Format_UE(TEXT("{:.1f}"), Current.Get_ViewInfo().FOV);
    if (InKey == TEXT("pov-boom")) return FormatRotator(Pov._BoomArmRotation);
    if (InKey == TEXT("pov-group")) return FormatVector(Pov._GroupBaseLocation);
    if (InKey == TEXT("pov-look-at")) return FormatVector(Pov._LookAtLocation);
    if (InKey == TEXT("pov-boom-end")) return FormatVector(Pov._BoomArmEndTransform.GetLocation());
    if (InKey == TEXT("pov-framing")) return FormatVector(Pov._FramingTransform.GetLocation());
    if (InKey == TEXT("pov-camera")) return FormatVector(Pov._CameraTransform.GetLocation());
    if (InKey == TEXT("pov-noise")) return FormatRotator(Pov._NoiseRotator);
    if (InKey == TEXT("pov-collision")) return Pov._CollisionDistance.IsSet() ? ck::Format_UE(TEXT("{:.1f}"), Pov._CollisionDistance.GetValue()) : TEXT("-");
    return TEXT("--");
}

auto SCkInspector_CameraAuthored::Get_Bool(const FString& InKey) const -> bool
{
    if (NOT Get_IsAvailable()) { return false; }
    const auto& Profile = _Entity.Get<ck::FFragment_Camera_Current>().Get_ComposedProfile();
    if (InKey == TEXT("fixed-boom")) return Profile.Get_Rig().Get_UseFixedBoomRotation();
    if (InKey == TEXT("aspect")) return Profile.Get_Sensor().Get_ConstrainAspectRatio();
    if (InKey == TEXT("orientation-control")) return Profile.Get_HasOrientationControl();
    if (InKey == TEXT("auto-reorient")) return Profile.Get_HasAutoReorient();
    if (InKey == TEXT("collision")) return Profile.Get_HasCollision();
    if (InKey == TEXT("async-trace")) return Profile.Get_Collision().Get_UseAsyncTrace();
    return InKey == TEXT("post-process") && Profile.Get_UsePostProcess();
}

auto SCkInspector_CameraAuthored::Get_Number(const FString& InKey) const -> float
{
    if (NOT Get_IsAvailable()) { return 0.0f; }
    const auto& Current = _Entity.Get<ck::FFragment_Camera_Current>();
    const auto& Boom = Current.Get_PovState()._BoomArmRotation;
    if (InKey == TEXT("yaw-min")) return Current.Get_ComposedProfile().Get_OrientationControl().Get_Yaw().Get_Limits().Get_Min();
    if (InKey == TEXT("yaw-max")) return Current.Get_ComposedProfile().Get_OrientationControl().Get_Yaw().Get_Limits().Get_Max();
    if (InKey == TEXT("boom-roll")) return Boom.Roll;
    if (InKey == TEXT("boom-pitch")) return Boom.Pitch;
    if (InKey == TEXT("boom-yaw")) return Boom.Yaw;
    const FVector Intention = Current.Get_OrientationIntention();
    if (InKey == TEXT("intention-x")) return Intention.X;
    if (InKey == TEXT("intention-y")) return Intention.Y;
    return Intention.Z;
}

auto SCkInspector_CameraAuthored::Refresh_Layers() -> bool
{
    if (NOT _Layers.IsValid())
    {
        const FCkUiLoadResult Create = FCkUiCollection::TryCreate({
            {TEXT("label"), ECkUiFieldKind::Text}, {TEXT("value"), ECkUiFieldKind::Text},
            {TEXT("fraction"), ECkUiFieldKind::Number}, {TEXT("foreground"), ECkUiFieldKind::Color},
            {TEXT("label-color"), ECkUiFieldKind::Color},
            {TEXT("background"), ECkUiFieldKind::Color}}, _Layers);
        if (NOT Create.Succeeded || NOT _Layers.IsValid()) { _LoadError = FString::Join(Create.Errors, TEXT("\n")); return false; }
    }
    auto Records = TArray<FCkUiRecordData>{};
    if (Get_IsAvailable())
    {
        auto Mutable = _Entity;
        ck::FUtils_RecordOfCameraLayers::ForEach_ValidEntry(Mutable, [this, &Records](FCk_Handle_CameraLayer InLayer)
        {
            auto Record = FCkUiRecordData{};
            Record.Key = InLayer.ToString(); // handle/version is the live stable identity, never the display label.
            const bool bBase = InLayer.Has<ck::FFragment_CameraLayer_Params>() && InLayer.Get<ck::FFragment_CameraLayer_Params>().Get_IsDefault();
            const auto Alpha = InLayer.Has<ck::FFragment_CameraLayer_Blend>() ? InLayer.Get<ck::FFragment_CameraLayer_Blend>().Get_Alpha() : 0.0f;
            const bool bExiting = InLayer.Has<ck::FFragment_CameraLayer_Blend>() && InLayer.Get<ck::FFragment_CameraLayer_Blend>().Get_TargetAlpha() <= 0.0f;
            const ECk_Tone Tone = bBase ? ECk_Tone::Info : bExiting ? ECk_Tone::Warn : InLayer.Has<ck::FTag_CameraLayer_Active>() ? ECk_Tone::Ok : ECk_Tone::Neutral;
            const FString Label = ck_inspector_camera::GetLayerLabel(InLayer);
            auto LabelValue = FCkUiFieldValue{}; LabelValue.Kind = ECkUiFieldKind::Text; LabelValue.Text = FText::FromString(Label);
            auto ReadoutValue = FCkUiFieldValue{}; ReadoutValue.Kind = ECkUiFieldKind::Text; ReadoutValue.Text = FText::FromString(FString::Printf(TEXT("%.2f%s"), Alpha, bExiting ? TEXT(" exit") : InLayer.Has<ck::FTag_CameraLayer_Active>() ? TEXT("") : TEXT(" pend")));
            auto FractionValue = FCkUiFieldValue{}; FractionValue.Kind = ECkUiFieldKind::Number; FractionValue.Number = Alpha;
            auto ForegroundValue = FCkUiFieldValue{}; ForegroundValue.Kind = ECkUiFieldKind::Color; ForegroundValue.Color = CkStyle::GetToneColor(Tone);
            auto LabelColorValue = FCkUiFieldValue{}; LabelColorValue.Kind = ECkUiFieldKind::Color;
            LabelColorValue.Color = _DiffLabels.Contains(Label) ? CkStyle::Accent() : CkStyle::Text();
            auto BackgroundValue = FCkUiFieldValue{}; BackgroundValue.Kind = ECkUiFieldKind::Color; BackgroundValue.Color = CkStyle::GetToneDimColor(Tone);
            Record.Fields.Add(TEXT("label"), MoveTemp(LabelValue));
            Record.Fields.Add(TEXT("value"), MoveTemp(ReadoutValue));
            Record.Fields.Add(TEXT("fraction"), MoveTemp(FractionValue));
            Record.Fields.Add(TEXT("foreground"), MoveTemp(ForegroundValue));
            Record.Fields.Add(TEXT("label-color"), MoveTemp(LabelColorValue));
            Record.Fields.Add(TEXT("background"), MoveTemp(BackgroundValue));
            Records.Add(MoveTemp(Record));
        });
    }
    const FCkUiLoadResult Set = _Layers->TrySetRecords(MoveTemp(Records));
    if (NOT Set.Succeeded) { _LoadError = FString::Join(Set.Errors, TEXT("\n")); return false; }
    return true;
}

auto SCkInspector_CameraAuthored::Build_AuthoredView() -> bool
{
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT RegistryResult.Succeeded || NOT Registry.IsValid() || NOT Plugin.IsValid())
    { _LoadError = Plugin.IsValid() ? FString::Join(RegistryResult.Errors, TEXT("\n")) : TEXT("CkDebugger plugin could not be resolved for Camera inspector authored resources."); return false; }

    const TWeakPtr<SCkInspector_CameraAuthored> Weak{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    const TPair<const TCHAR*, const TCHAR*> DiffBindings[] = {
        {TEXT("orientation-diff"), TEXT("Orientation Intention:")}, {TEXT("dominant-diff"), TEXT("Dominant Modifier:")},
        {TEXT("look-at-diff"), TEXT("Dominant Look-At:")}, {TEXT("fov-diff"), TEXT("FOV:")},
        {TEXT("boom-length-diff"), TEXT("Boom Length:")}, {TEXT("framing-offset-diff"), TEXT("Framing Offset:")},
        {TEXT("framing-pitch-yaw-diff"), TEXT("Framing Pitch/Yaw:")}, {TEXT("orientation-control-diff"), TEXT("Orientation Control:")},
        {TEXT("auto-reorient-diff"), TEXT("Auto-Reorient:")}, {TEXT("collision-diff"), TEXT("Collision:")},
        {TEXT("fixed-boom-control-diff"), TEXT("Use Fixed Boom Rotation:")}, {TEXT("aspect-control-diff"), TEXT("Constrain Aspect Ratio:")},
        {TEXT("orientation-control-toggle-diff"), TEXT("Has Orientation Control:")}, {TEXT("auto-reorient-toggle-diff"), TEXT("Has Auto-Reorient:")},
        {TEXT("collision-toggle-diff"), TEXT("Has Collision:")}, {TEXT("async-trace-diff"), TEXT("Use Async Trace:")},
        {TEXT("post-process-diff"), TEXT("Use Post Process:")}, {TEXT("boom-control-diff"), TEXT("Snap Boom Rotation (R,P,Y):")},
        {TEXT("yaw-min-diff"), TEXT("Orientation Yaw Min:")}, {TEXT("yaw-max-diff"), TEXT("Orientation Yaw Max:")},
        {TEXT("intention-control-diff"), TEXT("Push Orientation Intention:")}, {TEXT("view-location-diff"), TEXT("Location:")},
        {TEXT("view-rotation-diff"), TEXT("Rotation (R,P,Y):")}, {TEXT("view-fov-diff"), TEXT("FOV:")},
        {TEXT("pov-boom-diff"), TEXT("Boom Rotation (R,P,Y):")}, {TEXT("pov-group-diff"), TEXT("Group-Base Loc:")},
        {TEXT("pov-look-at-diff"), TEXT("Look-At Loc:")}, {TEXT("pov-boom-end-diff"), TEXT("Boom-End Loc:")},
        {TEXT("pov-framing-diff"), TEXT("Framing Loc:")}, {TEXT("pov-camera-diff"), TEXT("Camera Loc:")},
        {TEXT("pov-noise-diff"), TEXT("Noise (R,P,Y):")}, {TEXT("pov-collision-diff"), TEXT("Collision Dist:")},
        {TEXT("layer-class-diff"), TEXT("Class:")}, {TEXT("layer-priority-diff"), TEXT("Priority:")},
        {TEXT("layer-target-diff"), TEXT("Camera Target:")}, {TEXT("layer-alpha-diff"), TEXT("Alpha (cur → target):")},
        {TEXT("layer-rate-diff"), TEXT("Blend Rate:")}, {TEXT("layer-state-diff"), TEXT("State:")},
    };
    for (const auto& Pair : DiffBindings)
    {
        const FString CapturedLabel{Pair.Value};
        Data.Color.Add(Pair.Key, TAttribute<FLinearColor>::CreateLambda([Weak, CapturedLabel]()
        {
            const auto W = Weak.Pin();
            return W.IsValid() ? W->Get_DiffColor(CapturedLabel) : CkStyle::Text();
        }));
    }
    for (const TCHAR* Key : {TEXT("orientation"), TEXT("dominant"), TEXT("look-at"), TEXT("fov"), TEXT("boom-length"), TEXT("framing-offset"), TEXT("framing-pitch-yaw"), TEXT("orientation-control"), TEXT("auto-reorient"), TEXT("collision"), TEXT("view-location"), TEXT("view-rotation"), TEXT("view-fov"), TEXT("pov-boom"), TEXT("pov-group"), TEXT("pov-look-at"), TEXT("pov-boom-end"), TEXT("pov-framing"), TEXT("pov-camera"), TEXT("pov-noise"), TEXT("pov-collision"), TEXT("layer-class"), TEXT("layer-priority"), TEXT("layer-target"), TEXT("layer-alpha"), TEXT("layer-rate"), TEXT("layer-state")})
    { const FString Name{Key}; Data.Text.Add(Name, TAttribute<FText>::CreateLambda([Weak, Name]() { const auto W = Weak.Pin(); return W.IsValid() && NOT W->Is_Inert() ? FText::FromString(W->Get_Text(Name)) : FText::GetEmpty(); })); }
    for (const TCHAR* Key : {TEXT("fixed-boom"), TEXT("aspect"), TEXT("orientation-control"), TEXT("auto-reorient"), TEXT("collision"), TEXT("async-trace"), TEXT("post-process")})
    { const FString Name{Key}; Data.Visibility.Add(Name + TEXT("-can-edit"), TAttribute<bool>::CreateLambda([Weak]() { const auto W = Weak.Pin(); return W.IsValid() && W->Get_CanEdit(); })); Data.BoolChanged.Add(Name + TEXT("-changed"), FCkUiOnBoolChanged::CreateLambda([Weak, Name](bool Value) { if (const auto W = Weak.Pin(); W.IsValid()) { W->Set_Flag(Name, Value); } })); }
    for (const TCHAR* Key : {TEXT("fixed-boom"), TEXT("aspect"), TEXT("orientation-control"), TEXT("auto-reorient"), TEXT("collision"), TEXT("async-trace"), TEXT("post-process")})
    { const FString Name{Key}; Data.Visibility.Add(Name, TAttribute<bool>::CreateLambda([Weak, Name]() { const auto W = Weak.Pin(); return W.IsValid() && W->Get_Bool(Name); })); }
    Data.Text.Add(TEXT("camera-disabled"), TAttribute<FText>::CreateLambda([Weak]() { const auto W = Weak.Pin(); return W.IsValid() ? FText::FromString(W->Get_EditDisabledReason()) : FText::GetEmpty(); }));
    Data.Visibility.Add(TEXT("camera-available"), TAttribute<bool>::CreateLambda([Weak]() { const auto W = Weak.Pin(); return W.IsValid() && W->Get_IsAvailable(); }));
    Data.Visibility.Add(TEXT("camera-layer-available"), TAttribute<bool>::CreateLambda([Weak]() { const auto W = Weak.Pin(); return W.IsValid() && W->Get_IsLayerAvailable(); }));
    Data.Visibility.Add(TEXT("camera-unavailable"), TAttribute<bool>::CreateLambda([Weak]() { const auto W = Weak.Pin(); return W.IsValid() && NOT W->Get_IsAvailable() && NOT W->Get_IsLayerAvailable(); }));
    Data.Number.Add(TEXT("layer-alpha-fraction"), TAttribute<float>::CreateLambda([Weak]()
    {
        const auto W = Weak.Pin();
        return W.IsValid() && W->Get_IsLayerAvailable() && W->_Entity.Has<ck::FFragment_CameraLayer_Blend>()
            ? W->_Entity.Get<ck::FFragment_CameraLayer_Blend>().Get_Alpha() : 0.0f;
    }));
    const auto GetLayerTone = [Weak]()
    {
        const auto W = Weak.Pin();
        if (NOT W.IsValid() || NOT W->Get_IsLayerAvailable()) return ECk_Tone::Neutral;
        if (W->_Entity.Has<ck::FFragment_CameraLayer_Blend>() && W->_Entity.Get<ck::FFragment_CameraLayer_Blend>().Get_TargetAlpha() <= 0.0f) return ECk_Tone::Warn;
        return W->_Entity.Has<ck::FTag_CameraLayer_Active>() ? ECk_Tone::Ok : ECk_Tone::Neutral;
    };
    Data.Color.Add(TEXT("layer-state-foreground"), TAttribute<FLinearColor>::CreateLambda([GetLayerTone]() { return CkStyle::GetToneColor(GetLayerTone()); }));
    Data.Color.Add(TEXT("layer-state-background"), TAttribute<FLinearColor>::CreateLambda([GetLayerTone]() { return CkStyle::GetToneDimColor(GetLayerTone()); }));
    Data.Visibility.Add(TEXT("camera-can-edit"), TAttribute<bool>::CreateLambda([Weak]() { const auto W = Weak.Pin(); return W.IsValid() && W->Get_CanEdit(); }));
    for (const TCHAR* Key : {TEXT("yaw-min"), TEXT("yaw-max"), TEXT("boom-roll"), TEXT("boom-pitch"), TEXT("boom-yaw"), TEXT("intention-x"), TEXT("intention-y"), TEXT("intention-z")})
    { const FString Name{Key}; Data.Number.Add(Name, TAttribute<float>::CreateLambda([Weak, Name]() { const auto W = Weak.Pin(); return W.IsValid() ? W->Get_Number(Name) : 0.0f; })); }
    Data.NumberCommitted.Add(TEXT("yaw-min-committed"), FCkUiOnNumberCommitted::CreateLambda([Weak](float V, ETextCommit::Type) { if (const auto W = Weak.Pin(); W.IsValid()) W->Commit_YawMin(V); }));
    Data.NumberCommitted.Add(TEXT("yaw-max-committed"), FCkUiOnNumberCommitted::CreateLambda([Weak](float V, ETextCommit::Type) { if (const auto W = Weak.Pin(); W.IsValid()) W->Commit_YawMax(V); }));
    for (const TCHAR* Key : {TEXT("boom-roll"), TEXT("boom-pitch"), TEXT("boom-yaw")}) { const FString Name{Key}; Data.NumberCommitted.Add(Name + TEXT("-committed"), FCkUiOnNumberCommitted::CreateLambda([Weak, Name](float V, ETextCommit::Type) { if (const auto W = Weak.Pin(); W.IsValid()) W->Commit_BoomComponent(Name, V); })); }
    for (const TCHAR* Key : {TEXT("intention-x"), TEXT("intention-y"), TEXT("intention-z")}) { const FString Name{Key}; Data.NumberCommitted.Add(Name + TEXT("-committed"), FCkUiOnNumberCommitted::CreateLambda([Weak, Name](float V, ETextCommit::Type) { if (const auto W = Weak.Pin(); W.IsValid()) W->Commit_OrientationComponent(Name, V); })); }
    Data.Collections.Add(TEXT("camera-layers"), _Layers);
    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create({}, {}, {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString Root = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(FPaths::Combine(Root, TEXT("EcsInspectorCamera.ui.html")), FPaths::Combine(Root, TEXT("EcsInspectorCamera.ui.css")));
    Candidate->PollFiles();
    if (NOT Candidate->GetLastResult().Succeeded) { _LoadError = FString::Join(Candidate->GetLastResult().Errors, TEXT("\n")); return false; }
    _View = Candidate; _Mounted = true; _LoadError.Reset(); return true;
}

auto SCkInspector_CameraAuthored::Set_Flag(const FString& InKey, const bool bInEnabled) -> void
{
    auto Camera = FCk_Handle_Camera{};
    if (NOT _Active || NOT ck_inspector_camera::TryGetCamera(_Entity, Camera, false)
        || NOT ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::CosmeticOnly).IsEnabled) { return; }
    if (InKey == TEXT("fixed-boom")) UCk_Utils_Camera_UE::Request_Set_UseFixedBoomRotation(Camera, bInEnabled, {});
    else if (InKey == TEXT("aspect")) UCk_Utils_Camera_UE::Request_Set_ConstrainAspectRatio(Camera, bInEnabled, {});
    else if (InKey == TEXT("orientation-control")) UCk_Utils_Camera_UE::Request_Set_HasOrientationControl(Camera, bInEnabled, {});
    else if (InKey == TEXT("auto-reorient")) UCk_Utils_Camera_UE::Request_Set_HasAutoReorient(Camera, bInEnabled, {});
    else if (InKey == TEXT("collision")) UCk_Utils_Camera_UE::Request_Set_HasCollision(Camera, bInEnabled, {});
    else if (InKey == TEXT("async-trace")) UCk_Utils_Camera_UE::Request_Set_UseAsyncTrace(Camera, bInEnabled, {});
    else if (InKey == TEXT("post-process")) UCk_Utils_Camera_UE::Request_Set_UsePostProcess(Camera, bInEnabled, {});
}

auto SCkInspector_CameraAuthored::Get_DiffColor(const FString& InLabel) const -> FLinearColor
{ return _DiffLabels.Contains(InLabel) ? CkStyle::Accent() : CkStyle::Text(); }

auto SCkInspector_CameraAuthored::Commit_BoomRotation(const FRotator& InRotation) -> void
{
    auto Camera = FCk_Handle_Camera{};
    if (NOT _Active || NOT ck_inspector_camera::TryGetCamera(_Entity, Camera, false)
        || NOT ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::CosmeticOnly).IsEnabled) { return; }
    UCk_Utils_Camera_UE::Request_SnapBoomRotation(Camera, InRotation.GetNormalized(), {});
}

auto SCkInspector_CameraAuthored::Commit_BoomComponent(const FString& InKey, const float InValue) -> void
{
    if (NOT Get_IsAvailable()) { return; }
    auto Rotation = _Entity.Get<ck::FFragment_Camera_Current>().Get_PovState()._BoomArmRotation;
    if (InKey == TEXT("boom-roll")) Rotation.Roll = InValue;
    else if (InKey == TEXT("boom-pitch")) Rotation.Pitch = InValue;
    else Rotation.Yaw = InValue;
    Commit_BoomRotation(Rotation);
}

auto SCkInspector_CameraAuthored::Commit_YawMin(const float InValue) -> void
{
    auto Camera = FCk_Handle_Camera{};
    if (NOT _Active || NOT ck_inspector_camera::TryGetCamera(_Entity, Camera, true)
        || NOT ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::CosmeticOnly).IsEnabled) { return; }
    const auto Limits = _Entity.Get<ck::FFragment_Camera_Current>().Get_ComposedProfile().Get_OrientationControl().Get_Yaw().Get_Limits();
    UCk_Utils_Camera_UE::Request_Set_OrientationYawLimits(Camera, InValue, Limits.Get_Max(), {});
}

auto SCkInspector_CameraAuthored::Commit_YawMax(const float InValue) -> void
{
    auto Camera = FCk_Handle_Camera{};
    if (NOT _Active || NOT ck_inspector_camera::TryGetCamera(_Entity, Camera, true)
        || NOT ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::CosmeticOnly).IsEnabled) { return; }
    const auto Limits = _Entity.Get<ck::FFragment_Camera_Current>().Get_ComposedProfile().Get_OrientationControl().Get_Yaw().Get_Limits();
    UCk_Utils_Camera_UE::Request_Set_OrientationYawLimits(Camera, Limits.Get_Min(), InValue, {});
}

auto SCkInspector_CameraAuthored::Commit_OrientationIntention(const FVector& InValue) -> void
{
    auto Camera = FCk_Handle_Camera{};
    if (NOT _Active || NOT ck_inspector_camera::TryGetCamera(_Entity, Camera, false)
        || NOT ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::CosmeticOnly).IsEnabled) { return; }
    UCk_Utils_Camera_UE::Request_SetOrientationIntention(Camera, InValue, {});
}

auto SCkInspector_CameraAuthored::Commit_OrientationComponent(const FString& InKey, const float InValue) -> void
{
    if (NOT Get_IsAvailable()) { return; }
    auto Intention = _Entity.Get<ck::FFragment_Camera_Current>().Get_OrientationIntention();
    if (InKey == TEXT("intention-x")) Intention.X = InValue;
    else if (InKey == TEXT("intention-y")) Intention.Y = InValue;
    else Intention.Z = InValue;
    Commit_OrientationIntention(Intention);
}

auto SCkInspector_CameraAuthored::Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime) -> void
{
    SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);
    if (NOT _Active || NOT _View.IsValid() || NOT Refresh_Layers()) { return; }
    _View->PollFiles();
    if (NOT _View->GetLastResult().Succeeded) _LoadError = FString::Join(_View->GetLastResult().Errors, TEXT("\n"));
    else _LoadError.Reset();
}

auto SCkInspector_CameraAuthored::Release() -> void
{
    if (NOT _Active) { return; }
    _Active = false; _Entity = {}; _DiffLabels.Reset(); _Layers.Reset(); _View.Reset(); _Mounted = false;
}

FCkInspector_Camera::~FCkInspector_Camera()
{ OnDeactivated(); }

auto FCkInspector_Camera::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> Native = Build_NativeBody(Entity);
    if (FCkInspector_RowCaptureScope::Is_Active()) { return Native; }
    auto DiffLabels = TSet<FString>{};
    for (const TCHAR* Label : {
        TEXT("Orientation Intention:"), TEXT("Dominant Modifier:"), TEXT("Dominant Look-At:"), TEXT("FOV:"),
        TEXT("Boom Length:"), TEXT("Framing Offset:"), TEXT("Framing Pitch/Yaw:"), TEXT("Orientation Control:"),
        TEXT("Auto-Reorient:"), TEXT("Collision:"), TEXT("Use Fixed Boom Rotation:"), TEXT("Constrain Aspect Ratio:"),
        TEXT("Has Orientation Control:"), TEXT("Has Auto-Reorient:"), TEXT("Has Collision:"), TEXT("Use Async Trace:"),
        TEXT("Use Post Process:"), TEXT("Snap Boom Rotation (R,P,Y):"), TEXT("Orientation Yaw Min:"),
        TEXT("Orientation Yaw Max:"), TEXT("Push Orientation Intention:"), TEXT("Location:"), TEXT("Rotation (R,P,Y):"),
        TEXT("Boom Rotation (R,P,Y):"), TEXT("Group-Base Loc:"), TEXT("Look-At Loc:"), TEXT("Boom-End Loc:"),
        TEXT("Framing Loc:"), TEXT("Camera Loc:"), TEXT("Noise (R,P,Y):"), TEXT("Collision Dist:"), TEXT("Class:"),
        TEXT("Priority:"), TEXT("Camera Target:"), TEXT("Alpha (cur → target):"), TEXT("Blend Rate:"), TEXT("State:")})
    {
        if (FCkInspector_DiffMarkScope::Is_LabelMarked(Label))
        { DiffLabels.Add(Label); }
    }
    if (Entity.Has<ck::FFragment_Camera_Current>())
    {
        auto Mutable = Entity;
        ck::FUtils_RecordOfCameraLayers::ForEach_ValidEntry(Mutable, [&DiffLabels](const FCk_Handle_CameraLayer InLayer)
        {
            const FString Label = ck_inspector_camera::GetLayerLabel(InLayer);
            if (FCkInspector_DiffMarkScope::Is_LabelMarked(Label))
            { DiffLabels.Add(Label); }
        });
    }
    const TSharedRef<SCkInspector_CameraAuthored> Authored = SNew(SCkInspector_CameraAuthored)
        .Entity(Entity)
        .DiffLabels(MoveTemp(DiffLabels));
    if (NOT Authored->Is_Mounted()) { _LastAuthoredLoadError = Authored->Get_LoadError(); return Native; }
    _LastAuthoredLoadError.Reset(); _AuthoredInstances.Add(Authored); return Authored;
}

auto FCkInspector_Camera::OnDeactivated() -> void
{
    for (const TWeakPtr<SCkInspector_CameraAuthored>& Weak : _AuthoredInstances)
    { if (const auto Instance = Weak.Pin(); Instance.IsValid()) { Instance->Release(); } }
    _AuthoredInstances.Reset();
}
