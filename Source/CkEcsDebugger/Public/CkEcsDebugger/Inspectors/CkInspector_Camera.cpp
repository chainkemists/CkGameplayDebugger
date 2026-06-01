#include "CkInspector_Camera.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Debug/CkDebugDraw_Utils.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"

#include "CkEcsExt/Transform/CkTransform_Fragment.h"

#include "CkCamera/Camera/CkCamera_Fragment.h"
#include "CkCamera/Camera/CameraLayer/CkCameraLayer_Fragment.h"
#include "CkCamera/Camera/CameraLayer/EntityScripts/CkCameraLayer_EntityScript.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkDebuggerCommon/Style/CkDebugStyle.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Notifications/SProgressBar.h"

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
    return InValue ? CkDebugStyle::Ok() : CkDebugStyle::TextMute();
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

auto FCkInspector_Camera::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();

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
            CkDebugStyle::Value_Math());

        Builder.AddRow(
            FText::FromString(TEXT("Dominant Modifier:")),
            [Cam](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(Cam) || NOT Cam.Has<ck::FFragment_Camera_Current>())
                { return FText::FromString(TEXT("--")); }
                return FText::FromString(DoFmt_ClassName(
                    Cam.Get<ck::FFragment_Camera_Current>().Get_DominantLayerClass().Get()));
            },
            CkDebugStyle::Value_Object());

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
            CkDebugStyle::Value_Math());

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
            CkDebugStyle::Value_Numeric());

        Builder.AddRow(
            FText::FromString(TEXT("Boom Length:")),
            [Cam](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(Cam) || NOT Cam.Has<ck::FFragment_Camera_Current>())
                { return FText::FromString(TEXT("--")); }
                return DoFmt_Float(Cam.Get<ck::FFragment_Camera_Current>()
                    .Get_ComposedProfile().Get_Rig().Get_BoomArmLength(), TEXT("{:.0f}"));
            },
            CkDebugStyle::Value_Numeric());

        Builder.AddRow(
            FText::FromString(TEXT("Framing Offset:")),
            [Cam](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(Cam) || NOT Cam.Has<ck::FFragment_Camera_Current>())
                { return FText::FromString(TEXT("--")); }
                return FText::FromString(ck::Format_UE(TEXT("{}"), Cam.Get<ck::FFragment_Camera_Current>()
                    .Get_ComposedProfile().Get_Rig().Get_FramingOffset()));
            },
            CkDebugStyle::Value_Math());

        Builder.AddRow(
            FText::FromString(TEXT("Framing Pitch/Yaw:")),
            [Cam](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(Cam) || NOT Cam.Has<ck::FFragment_Camera_Current>())
                { return FText::FromString(TEXT("--")); }
                const auto& Rig = Cam.Get<ck::FFragment_Camera_Current>().Get_ComposedProfile().Get_Rig();
                return FText::FromString(FString::Printf(TEXT("%.1f / %.1f"), Rig.Get_FramingPitch(), Rig.Get_FramingYaw()));
            },
            CkDebugStyle::Value_Numeric());

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
                { return CkDebugStyle::None(); }
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
                { return CkDebugStyle::None(); }
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
                { return CkDebugStyle::None(); }
                return DoColor_Flag(Cam.Get<ck::FFragment_Camera_Current>().Get_ComposedProfile().Get_HasCollision());
            });

        // ---- Resolved view info ----
        Builder.AddHeader(FText::FromString(TEXT("View Info (resolved)")));

        Builder.AddRow(
            FText::FromString(TEXT("Location:")),
            [Cam](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(Cam) || NOT Cam.Has<ck::FFragment_Camera_Current>())
                { return FText::FromString(TEXT("--")); }
                return FText::FromString(ck::Format_UE(TEXT("{}"),
                    Cam.Get<ck::FFragment_Camera_Current>().Get_ViewInfo().Location));
            },
            CkDebugStyle::Transform());

        Builder.AddRow(
            FText::FromString(TEXT("Rotation:")),
            [Cam](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(Cam) || NOT Cam.Has<ck::FFragment_Camera_Current>())
                { return FText::FromString(TEXT("--")); }
                return FText::FromString(ck::Format_UE(TEXT("{}"),
                    Cam.Get<ck::FFragment_Camera_Current>().Get_ViewInfo().Rotation));
            },
            CkDebugStyle::Transform());

        Builder.AddRow(
            FText::FromString(TEXT("FOV:")),
            [Cam](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(Cam) || NOT Cam.Has<ck::FFragment_Camera_Current>())
                { return FText::FromString(TEXT("--")); }
                return DoFmt_Float(Cam.Get<ck::FFragment_Camera_Current>().Get_ViewInfo().FOV, TEXT("{:.1f}"));
            },
            CkDebugStyle::Value_Numeric());
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
                CkDebugStyle::TextMute());
        }
        else
        {
            for (const auto& Layer : Layers)
            {
                const auto Lyr       = Layer;
                auto       IsDefault = false;
                auto       LabelStr  = FString(TEXT("(layer)"));

                if (Lyr.Has<ck::FFragment_CameraLayer_Params>())
                {
                    const auto& Params = Lyr.Get<ck::FFragment_CameraLayer_Params>();
                    IsDefault = Params.Get_IsDefault();
                    LabelStr  = IsDefault ? FString(TEXT("Base (resting)")) : DoFmt_ClassName(Params.Get_LayerClass().Get());
                    if (NOT IsDefault)
                    { LabelStr += FString::Printf(TEXT(" [p%d]"), Params.Get_Priority()); }
                }

                // Color the weight bar + readout by state: base = info-blue, exiting = warn, active = green, pending = muted.
                const auto ColorOf = [Lyr, IsDefault]() -> FLinearColor
                {
                    if (ck::Is_NOT_Valid(Lyr))
                    { return CkDebugStyle::None(); }
                    if (IsDefault)
                    { return CkDebugStyle::Info(); }
                    if (Lyr.Has<ck::FFragment_CameraLayer_Blend>() && Lyr.Get<ck::FFragment_CameraLayer_Blend>().Get_TargetAlpha() <= 0.0f)
                    { return CkDebugStyle::Warn(); }
                    if (Lyr.Has<ck::FTag_CameraLayer_Active>())
                    { return CkDebugStyle::Status_Active(); }
                    return CkDebugStyle::TextMute();
                };

                auto Row = SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                    [
                        SNew(SProgressBar)
                        .Percent_Lambda([Lyr]() -> TOptional<float>
                        {
                            if (ck::Is_NOT_Valid(Lyr) || NOT Lyr.Has<ck::FFragment_CameraLayer_Blend>())
                            { return 0.0f; }
                            return FMath::Clamp(Lyr.Get<ck::FFragment_CameraLayer_Blend>().Get_Alpha(), 0.0f, 1.0f);
                        })
                        .FillColorAndOpacity_Lambda([ColorOf]() -> FSlateColor { return ColorOf(); })
                    ]
                    + SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f, 0.0f, 0.0f).VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .ColorAndOpacity_Lambda([ColorOf]() -> FSlateColor { return ColorOf(); })
                        .Text_Lambda([Lyr]() -> FText
                        {
                            if (ck::Is_NOT_Valid(Lyr) || NOT Lyr.Has<ck::FFragment_CameraLayer_Blend>())
                            { return FText::FromString(TEXT("--")); }
                            const auto& Blend   = Lyr.Get<ck::FFragment_CameraLayer_Blend>();
                            const auto  Alpha   = Blend.Get_Alpha();
                            const auto  bExiting = Blend.Get_TargetAlpha() <= 0.0f;
                            const auto* State   = bExiting                               ? TEXT(" exit")
                                                : Lyr.Has<ck::FTag_CameraLayer_Active>() ? TEXT("")
                                                :                                          TEXT(" pend");
                            return FText::FromString(FString::Printf(TEXT("%.2f%s"), Alpha, State));
                        })
                    ];

                Builder.AddWidgetRow(FText::FromString(LabelStr), Row);
            }
        }
    }

    // ---- POV pipeline intermediates ----
    if (Entity.Has<ck::FFragment_Camera_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("POV Pipeline")));

        const auto AddPovRow = [&Builder, Cam](const FString& InLabel, TFunction<FText(const ck::camera::FPov_State&)> InGet)
        {
            Builder.AddRow(
                FText::FromString(InLabel),
                [Cam, InGet](const FCk_Handle&)
                {
                    if (ck::Is_NOT_Valid(Cam) || NOT Cam.Has<ck::FFragment_Camera_Current>())
                    { return FText::FromString(TEXT("--")); }
                    return InGet(Cam.Get<ck::FFragment_Camera_Current>().Get_PovState());
                },
                CkDebugStyle::Transform());
        };

        AddPovRow(TEXT("Boom Rotation:"),  [](const auto& P) { return FText::FromString(ck::Format_UE(TEXT("{}"), P._BoomArmRotation)); });
        AddPovRow(TEXT("Group-Base Loc:"), [](const auto& P) { return FText::FromString(ck::Format_UE(TEXT("{}"), P._GroupBaseLocation)); });
        AddPovRow(TEXT("Look-At Loc:"),    [](const auto& P) { return FText::FromString(ck::Format_UE(TEXT("{}"), P._LookAtLocation)); });
        AddPovRow(TEXT("Boom-End Loc:"),   [](const auto& P) { return FText::FromString(ck::Format_UE(TEXT("{}"), P._BoomArmEndTransform.GetLocation())); });
        AddPovRow(TEXT("Framing Loc:"),    [](const auto& P) { return FText::FromString(ck::Format_UE(TEXT("{}"), P._FramingTransform.GetLocation())); });
        AddPovRow(TEXT("Camera Loc:"),     [](const auto& P) { return FText::FromString(ck::Format_UE(TEXT("{}"), P._CameraTransform.GetLocation())); });
        AddPovRow(TEXT("Collision Dist:"), [](const auto& P) { return P._CollisionDistance.IsSet()
            ? FText::FromString(FString::Printf(TEXT("%.1f"), P._CollisionDistance.GetValue()))
            : FText::FromString(TEXT("-")); });
        AddPovRow(TEXT("Noise:"),          [](const auto& P) { return FText::FromString(ck::Format_UE(TEXT("{}"), P._NoiseRotator)); });
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
            const auto  LookAtTarget = Params.Get_LookAtTarget();
            const auto  LookAtStr    = ck::IsValid(LookAtTarget) ? ck::Format_UE(TEXT("{}"), LookAtTarget) : FString(TEXT("(none)"));

            Builder.AddRow(FText::FromString(TEXT("Class:")),
                [ClassName](const FCk_Handle&) { return FText::FromString(ClassName); }, CkDebugStyle::Value_Object());
            Builder.AddRow(FText::FromString(TEXT("Priority:")),
                [PriorityStr](const FCk_Handle&) { return FText::FromString(PriorityStr); }, CkDebugStyle::Value_Numeric());
            Builder.AddRow(FText::FromString(TEXT("Look-At Target:")),
                [LookAtStr](const FCk_Handle&) { return FText::FromString(LookAtStr); }, CkDebugStyle::Value_Handle());
        }

        if (Entity.Has<ck::FFragment_CameraLayer_Blend>())
        {
            Builder.AddRow(FText::FromString(TEXT("Alpha:")),
                [Cam](const FCk_Handle&)
                {
                    if (ck::Is_NOT_Valid(Cam) || NOT Cam.Has<ck::FFragment_CameraLayer_Blend>())
                    { return FText::FromString(TEXT("--")); }
                    return DoFmt_Float(Cam.Get<ck::FFragment_CameraLayer_Blend>().Get_Alpha(), TEXT("{:.2f}"));
                }, CkDebugStyle::Value_Numeric());

            Builder.AddRow(FText::FromString(TEXT("Target Alpha:")),
                [Cam](const FCk_Handle&)
                {
                    if (ck::Is_NOT_Valid(Cam) || NOT Cam.Has<ck::FFragment_CameraLayer_Blend>())
                    { return FText::FromString(TEXT("--")); }
                    return DoFmt_Float(Cam.Get<ck::FFragment_CameraLayer_Blend>().Get_TargetAlpha(), TEXT("{:.2f}"));
                }, CkDebugStyle::Value_Numeric());

            Builder.AddRow(FText::FromString(TEXT("Blend Rate:")),
                [Cam](const FCk_Handle&)
                {
                    if (ck::Is_NOT_Valid(Cam) || NOT Cam.Has<ck::FFragment_CameraLayer_Blend>())
                    { return FText::FromString(TEXT("--")); }
                    return DoFmt_Float(Cam.Get<ck::FFragment_CameraLayer_Blend>().Get_BlendRate(), TEXT("{:.1f}"));
                }, CkDebugStyle::Value_Numeric());
        }

        Builder.AddConditionalRow(
            FText::FromString(TEXT("State:")),
            [Cam](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(Cam))
                { return FText::FromString(TEXT("--")); }
                const auto bExiting = Cam.Has<ck::FFragment_CameraLayer_Blend>() && Cam.Get<ck::FFragment_CameraLayer_Blend>().Get_TargetAlpha() <= 0.0f;
                if (bExiting)                               { return FText::FromString(TEXT("Exiting")); }
                if (Cam.Has<ck::FTag_CameraLayer_Active>()) { return FText::FromString(TEXT("Active")); }
                return FText::FromString(TEXT("Pending"));
            },
            [Cam](const FCk_Handle&) -> FLinearColor
            {
                if (ck::Is_NOT_Valid(Cam))                  { return CkDebugStyle::None(); }
                const auto bExiting = Cam.Has<ck::FFragment_CameraLayer_Blend>() && Cam.Get<ck::FFragment_CameraLayer_Blend>().Get_TargetAlpha() <= 0.0f;
                if (bExiting)                               { return CkDebugStyle::Warn(); }
                if (Cam.Has<ck::FTag_CameraLayer_Active>()) { return CkDebugStyle::Status_Active(); }
                return CkDebugStyle::TextMute();
            });
    }

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto FCkInspector_Camera::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    if (ck::Is_NOT_Valid(Entity) || NOT Entity.Has<ck::FFragment_Camera_Current>())
    { return; }

    // Rebuild the stack section when the live modifier count changes (modifiers are added / pruned over time).
    {
        auto MutableEntity = Entity;
        auto Count = int32{0};
        ck::FUtils_RecordOfCameraLayers::ForEach_ValidEntry(MutableEntity,
        [&Count](FCk_Handle_CameraLayer) { ++Count; });

        if (Count != _LastModifierCount)
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
    const auto BoomColor = Pov._CollisionDistance.IsSet() ? CkDebugStyle::Err() : CkDebugStyle::Info();
    UCk_Utils_DebugDraw_UE::DrawDebugLine(EntityWorld, AnchorLocation, CameraLocation, BoomColor, NoDuration, 1.5f);

    // Camera pose: axes + a forward arrow showing view direction.
    UCk_Utils_DebugDraw_UE::DrawDebugCoordinateSystem(EntityWorld, CameraLocation, CameraRotation, 0.5f, NoDuration, 1.5f);
    UCk_Utils_DebugDraw_UE::DrawDebugArrow(
        EntityWorld, CameraLocation, CameraLocation + (CameraRotation.Vector() * 150.0f), 25.0f, CkDebugStyle::Accent(), NoDuration, 1.5f);

    // Look-at target (auto-reorient / lock-on).
    if (Current.Get_DominantLookAt().IsSet())
    {
        const auto Target = Current.Get_DominantLookAt().GetValue();
        UCk_Utils_DebugDraw_UE::DrawDebugLine(EntityWorld, CameraLocation, Target, CkDebugStyle::Warn(), NoDuration, 1.0f);
        UCk_Utils_DebugDraw_UE::DrawDebugSphere(EntityWorld, Target, 24.0f, 12, CkDebugStyle::Warn(), NoDuration, 1.0f);
    }

    UCk_Utils_DebugDraw_UE::DrawDebugString(
        EntityWorld, CameraLocation + FVector(0.0f, 0.0f, 20.0f), Entity.ToString(), CkDebugStyle::Text(), NoDuration);
}

// =====================================================================================================================
