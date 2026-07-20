#include "CkInspector_Minimap.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkMinimap/CkMinimap_Fragment.h"
#include "CkMinimap/CkMinimap_Utils.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"

#include "CkEditorTools/Style/CkStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Minimap)

// =====================================================================================================================

auto FCkInspector_Minimap::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Minimap"));
}

auto FCkInspector_Minimap::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity) && UCk_Utils_Minimap_UE::Has(Entity);
}

auto FCkInspector_Minimap::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}

auto FCkInspector_Minimap::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();

    auto MutableEntity = Entity;
    const auto MinimapHandle = UCk_Utils_Minimap_UE::Cast(MutableEntity);

    if (ck::Is_NOT_Valid(MinimapHandle))
    { return Builder.Build(Entity, FString()); }

    const auto CapturedMinimap = MinimapHandle;

    Builder.AddRow(
        FText::FromString(TEXT("Projection:")),
        [CapturedMinimap](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedMinimap)) { return FText::FromString(TEXT("--")); }
            return FText::FromString(ck::Format_UE(TEXT("{}"), UCk_Utils_Minimap_UE::Get_ProjectionMode(CapturedMinimap)));
        },
        CkStyle::Value_Enum());

    Builder.AddRow(
        FText::FromString(TEXT("Rotation:")),
        [CapturedMinimap](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedMinimap)) { return FText::FromString(TEXT("--")); }
            return FText::FromString(ck::Format_UE(TEXT("{}"), UCk_Utils_Minimap_UE::Get_RotationMode(CapturedMinimap)));
        },
        CkStyle::Value_Enum());

    Builder.AddRow(
        FText::FromString(TEXT("Frame:")),
        [CapturedMinimap](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedMinimap)) { return FText::FromString(TEXT("--")); }
            return FText::FromString(ck::Format_UE(TEXT("{}"), UCk_Utils_Minimap_UE::Get_FrameShape(CapturedMinimap)));
        },
        CkStyle::Value_Enum());

    Builder.AddRow(
        FText::FromString(TEXT("View Extent:")),
        [CapturedMinimap](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedMinimap)) { return FText::FromString(TEXT("--")); }
            return FText::FromString(ck::Format_UE(TEXT("{:.0f}"), UCk_Utils_Minimap_UE::Get_ViewExtent(CapturedMinimap)));
        },
        CkStyle::Value_Numeric());

    Builder.AddRow(
        FText::FromString(TEXT("View Origin:")),
        [CapturedMinimap](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedMinimap)) { return FText::FromString(TEXT("--")); }
            const auto ViewOrigin = UCk_Utils_Minimap_UE::Get_ViewOrigin(CapturedMinimap);
            return FText::FromString(ck::Format_UE(TEXT("X {:.0f}  Y {:.0f}  Z {:.0f}"),
                ViewOrigin.X, ViewOrigin.Y, ViewOrigin.Z));
        },
        CkStyle::Value_Numeric());

    Builder.AddRow(
        FText::FromString(TEXT("View Yaw:")),
        [CapturedMinimap](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedMinimap)) { return FText::FromString(TEXT("--")); }
            return FText::FromString(ck::Format_UE(TEXT("{:.1f}°"), UCk_Utils_Minimap_UE::Get_ViewYawDegrees(CapturedMinimap)));
        },
        CkStyle::Value_Numeric());

    Builder.AddWidgetRow(
        FText::FromString(TEXT("Observer:")),
        SNew(SCkDebug_EntityRef)
            .Entity_Lambda([CapturedMinimap]() -> FCk_Handle
            {
                if (ck::Is_NOT_Valid(CapturedMinimap)) { return {}; }
                return UCk_Utils_Minimap_UE::Get_Observer(CapturedMinimap);
            })
            .ShowName(true));

    Builder.AddRow(
        FText::FromString(TEXT("Entries:")),
        [CapturedMinimap](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedMinimap)) { return FText::FromString(TEXT("--")); }
            const auto& Params = CapturedMinimap.Get<ck::FFragment_Minimap_Params>();
            return FText::FromString(ck::Format_UE(TEXT("{} / {}"),
                UCk_Utils_Minimap_UE::Get_Entries(CapturedMinimap).Num(), Params.Get_MaxEntries()));
        },
        CkStyle::Value_Numeric());

    Builder.AddRow(
        FText::FromString(TEXT("Fixed Bounds:")),
        [CapturedMinimap](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedMinimap)) { return FText::FromString(TEXT("--")); }

            if (UCk_Utils_Minimap_UE::Get_ProjectionMode(CapturedMinimap) != ECk_Minimap_ProjectionMode::FixedBounds)
            { return FText::FromString(TEXT("(observer-centric)")); }

            const auto FixedBounds = UCk_Utils_Minimap_UE::Get_FixedBounds(CapturedMinimap);
            return FText::FromString(ck::Format_UE(TEXT("C({:.0f}, {:.0f})  HE({:.0f}, {:.0f})"),
                FixedBounds.Get_Center().X, FixedBounds.Get_Center().Y,
                FixedBounds.Get_HalfExtents().X, FixedBounds.Get_HalfExtents().Y));
        },
        CkStyle::Value_Numeric());

    return Builder.Build(Entity, FString());
}

// =====================================================================================================================
