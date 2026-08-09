#include "CkInspector_OverlapBody.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkOverlapBody/Marker/CkMarker_Fragment.h"
#include "CkOverlapBody/Sensor/CkSensor_Fragment.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_OverlapBody)

// =====================================================================================================================

namespace ck_inspector_overlapbody
{
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

auto FCkInspector_OverlapBody::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();

    // ---- Marker ----
    if (Entity.Has<ck::FFragment_Marker_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Marker")));

        const auto CapturedEntity = Entity;

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

auto FCkInspector_OverlapBody::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}

// =====================================================================================================================
