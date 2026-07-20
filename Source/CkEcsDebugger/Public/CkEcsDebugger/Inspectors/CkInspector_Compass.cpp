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

    Builder.AddRow(
        FText::FromString(TEXT("Source:")),
        [CapturedCompass](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedCompass)) { return FText::FromString(TEXT("--")); }
            const auto& Params = CapturedCompass.Get<ck::FFragment_Compass_Params>();
            return FText::FromString(ck::Format_UE(TEXT("{}"), Params.Get_HeadingSource()));
        },
        CkStyle::Value_Enum());

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

    Builder.AddRow(
        FText::FromString(TEXT("Entries:")),
        [CapturedCompass](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedCompass)) { return FText::FromString(TEXT("--")); }
            const auto& Params = CapturedCompass.Get<ck::FFragment_Compass_Params>();
            return FText::FromString(ck::Format_UE(TEXT("{} / {}"),
                UCk_Utils_Compass_UE::Get_Entries(CapturedCompass).Num(), Params.Get_MaxEntries()));
        },
        CkStyle::Value_Numeric());

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
            return FText::FromString(UpdateInterval <= 0.0f
                ? TEXT("0 (every frame)")
                : ck::Format_UE(TEXT("{:.2f}s"), UpdateInterval));
        },
        CkStyle::Value_Numeric());

    return Builder.Build(Entity, FString());
}

// =====================================================================================================================
