#include "CkInspector_Compass.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkCompass/CkCompass_Fragment.h"
#include "CkCompass/CkCompass_Utils.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"

#include "CkEditorTools/Style/CkStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Compass)

// =====================================================================================================================

auto FCkInspector_Compass::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Compass"));
}

auto FCkInspector_Compass::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity) && UCk_Utils_Compass_UE::Has(Entity);
}

auto FCkInspector_Compass::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}

auto FCkInspector_Compass::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();
    Builder.SetEditGuard(Get_EditGuard());

    auto MutableEntity = Entity;
    const auto CompassHandle = UCk_Utils_Compass_UE::Cast(MutableEntity);

    if (ck::Is_NOT_Valid(CompassHandle))
    { return Builder.Build(Entity, FString()); }

    const auto CapturedCompass = CompassHandle;

    Builder.AddRow(
        FText::FromString(TEXT("Heading:")),
        [CapturedCompass](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedCompass)) { return FText::FromString(TEXT("--")); }
            const auto Heading = UCk_Utils_Compass_UE::Get_Heading(CapturedCompass);
            const auto Cardinal = UCk_Utils_Compass_UE::Get_CardinalDirection(Heading);
            return FText::FromString(ck::Format_UE(TEXT("{:.1f}°  {}"), Heading, Cardinal));
        },
        CkStyle::Value_Numeric());

    // Editable heading. CosmeticOnly: a compass exists to drive a local HUD ribbon, so a dedicated
    // server has nothing to point — the row greys there with the reason instead of firing.
    // Get_Heading is the live read-back, so the field reverts on its own if the source is not Manual.
    Builder.AddNumericRow(
        FText::FromString(TEXT("Manual Heading:")),
        TAttribute<float>::CreateLambda([CapturedCompass]()
        {
            if (ck::Is_NOT_Valid(CapturedCompass)) { return 0.0f; }
            return UCk_Utils_Compass_UE::Get_Heading(CapturedCompass);
        }),
        [CapturedCompass](float InHeadingDegrees)
        {
            auto MutableCompass = CapturedCompass;
            if (ck::Is_NOT_Valid(MutableCompass)) { return; }
            UCk_Utils_Compass_UE::Request_SetManualHeading(MutableCompass, InHeadingDegrees, {});
        },
        0.0f,
        360.0f,
        ECk_DebugRequest_Requirement::CosmeticOnly);

    Builder.AddRow(
        FText::FromString(TEXT("Source:")),
        [CapturedCompass](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedCompass)) { return FText::FromString(TEXT("--")); }
            const auto& Params = CapturedCompass.Get<ck::FFragment_Compass_Params>();
            return FText::FromString(ck::Format_UE(TEXT("{}"), Params.Get_HeadingSource()));
        },
        CkStyle::Value_Enum());

    // Read-only: Request_SetObserver takes an FCk_Handle, which needs an entity picker this
    // vocabulary does not have yet. Deferred, not forgotten.
    Builder.AddWidgetRow(
        FText::FromString(TEXT("Observer:")),
        SNew(SCkDebug_EntityRef)
            .Entity_Lambda([CapturedCompass]() -> FCk_Handle
            {
                if (ck::Is_NOT_Valid(CapturedCompass)) { return {}; }
                return UCk_Utils_Compass_UE::Get_Observer(CapturedCompass);
            })
            .ShowName(true));

    Builder.AddRow(
        FText::FromString(TEXT("Arc:")),
        [CapturedCompass](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedCompass)) { return FText::FromString(TEXT("--")); }
            return FText::FromString(ck::Format_UE(TEXT("{:.0f}°"), UCk_Utils_Compass_UE::Get_ArcDegrees(CapturedCompass)));
        },
        CkStyle::Value_Numeric());

    // Entries against MaxEntries is the one genuinely bounded quantity here — heading and arc are angles, and a
    // fill bar would read them as magnitudes.
    Builder.AddMeterRow(
        FText::FromString(TEXT("Entries:")),
        TAttribute<float>::CreateLambda([CapturedCompass]() -> float
        {
            if (ck::Is_NOT_Valid(CapturedCompass)) { return 0.0f; }
            const auto MaxEntries = CapturedCompass.Get<ck::FFragment_Compass_Params>().Get_MaxEntries();
            if (MaxEntries <= 0) { return 0.0f; }
            return static_cast<float>(UCk_Utils_Compass_UE::Get_Entries(CapturedCompass).Num()) / static_cast<float>(MaxEntries);
        }),
        ECk_Tone::Accent,
        TAttribute<FText>::CreateLambda([CapturedCompass]() -> FText
        {
            if (ck::Is_NOT_Valid(CapturedCompass)) { return FText::FromString(TEXT("--")); }
            const auto& Params = CapturedCompass.Get<ck::FFragment_Compass_Params>();
            return FText::FromString(ck::Format_UE(TEXT("{} / {}"),
                UCk_Utils_Compass_UE::Get_Entries(CapturedCompass).Num(), Params.Get_MaxEntries()));
        }));

    Builder.AddRow(
        FText::FromString(TEXT("Filter:")),
        [CapturedCompass](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedCompass)) { return FText::FromString(TEXT("--")); }
            const auto& Params = CapturedCompass.Get<ck::FFragment_Compass_Params>();
            const auto& CategoryFilter = Params.Get_CategoryFilter();
            return FText::FromString(CategoryFilter.IsEmpty()
                ? TEXT("(accepts all)")
                : CategoryFilter.GetDescription());
        },
        CkStyle::Value_Tag());

    Builder.AddRow(
        FText::FromString(TEXT("Interval:")),
        [CapturedCompass](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedCompass)) { return FText::FromString(TEXT("--")); }
            const auto& Params = CapturedCompass.Get<ck::FFragment_Compass_Params>();
            const auto UpdateInterval = Params.Get_UpdateInterval();
            return FText::FromString(UpdateInterval <= FCk_Time::ZeroSecond()
                ? TEXT("0 (every frame)")
                : ck::Format_UE(TEXT("{:.2f}s"), UpdateInterval.Get_Seconds()));
        },
        CkStyle::Value_Numeric());

    return Builder.Build(Entity, FString());
}

// =====================================================================================================================
