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

namespace
{
    auto Format_EnableDisable_Color(ECk_EnableDisable InState) -> FLinearColor
    {
        return InState == ECk_EnableDisable::Enable
            ? CkStyle::Value_Bool_True()
            : CkStyle::TextMute();
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

        Builder.AddConditionalRow(
            FText::FromString(TEXT("State:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Marker_Current>())
                { return FText::FromString(TEXT("--")); }
                const auto State = CapturedEntity.Get<ck::FFragment_Marker_Current>().Get_EnableDisable();
                return FText::FromString(ck::Format_UE(TEXT("{}"), State));
            },
            [CapturedEntity](const FCk_Handle&) -> FLinearColor
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Marker_Current>())
                { return CkStyle::None(); }
                return Format_EnableDisable_Color(CapturedEntity.Get<ck::FFragment_Marker_Current>().Get_EnableDisable());
            });

        Builder.AddConditionalRow(
            FText::FromString(TEXT("Shape:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Marker_Current>())
                { return FText::FromString(TEXT("--")); }
                const auto& Marker = CapturedEntity.Get<ck::FFragment_Marker_Current>().Get_Marker();
                return FText::FromString(Marker.IsValid() ? TEXT("Valid") : TEXT("None"));
            },
            [CapturedEntity](const FCk_Handle&) -> FLinearColor
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Marker_Current>())
                { return CkStyle::None(); }
                const auto& Marker = CapturedEntity.Get<ck::FFragment_Marker_Current>().Get_Marker();
                return Marker.IsValid() ? CkStyle::Value_Bool_True() : CkStyle::Value_Bool_False();
            });
    }

    // ---- Sensor ----
    if (Entity.Has<ck::FFragment_Sensor_Current>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Sensor")));

        const auto CapturedEntity = Entity;

        Builder.AddConditionalRow(
            FText::FromString(TEXT("State:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Sensor_Current>())
                { return FText::FromString(TEXT("--")); }
                const auto State = CapturedEntity.Get<ck::FFragment_Sensor_Current>().Get_EnableDisable();
                return FText::FromString(ck::Format_UE(TEXT("{}"), State));
            },
            [CapturedEntity](const FCk_Handle&) -> FLinearColor
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Sensor_Current>())
                { return CkStyle::None(); }
                return Format_EnableDisable_Color(CapturedEntity.Get<ck::FFragment_Sensor_Current>().Get_EnableDisable());
            });

        Builder.AddConditionalRow(
            FText::FromString(TEXT("Shape:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Sensor_Current>())
                { return FText::FromString(TEXT("--")); }
                const auto& Sensor = CapturedEntity.Get<ck::FFragment_Sensor_Current>().Get_Sensor();
                return FText::FromString(Sensor.IsValid() ? TEXT("Valid") : TEXT("None"));
            },
            [CapturedEntity](const FCk_Handle&) -> FLinearColor
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Sensor_Current>())
                { return CkStyle::None(); }
                const auto& Sensor = CapturedEntity.Get<ck::FFragment_Sensor_Current>().Get_Sensor();
                return Sensor.IsValid() ? CkStyle::Value_Bool_True() : CkStyle::Value_Bool_False();
            });

        Builder.AddRow(
            FText::FromString(TEXT("Marker Overlaps:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Sensor_Current>())
                { return FText::FromString(TEXT("--")); }
                const auto Count = CapturedEntity.Get<ck::FFragment_Sensor_Current>()
                    .Get_CurrentMarkerOverlaps().Get_Overlaps().Num();
                return FText::FromString(ck::Format_UE(TEXT("{}"), Count));
            },
            CkStyle::Value_Numeric());

        Builder.AddRow(
            FText::FromString(TEXT("Non-Marker Overlaps:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Sensor_Current>())
                { return FText::FromString(TEXT("--")); }
                const auto Count = CapturedEntity.Get<ck::FFragment_Sensor_Current>()
                    .Get_CurrentNonMarkerOverlaps().Get_Overlaps().Num();
                return FText::FromString(ck::Format_UE(TEXT("{}"), Count));
            },
            CkStyle::Value_Numeric());
    }

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto FCkInspector_OverlapBody::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}

// =====================================================================================================================
