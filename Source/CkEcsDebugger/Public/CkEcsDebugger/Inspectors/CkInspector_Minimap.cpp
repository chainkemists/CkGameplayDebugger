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

namespace ck_inspector_minimap
{
    // Three fixed-precision components in X/Y/Z order so AddAlignedNumericRow's index-based axis coloring
    // matches the Transform inspector's. Each component re-validates the handle on read — the row outlives
    // the entity (see CkInspector_Transform.cpp for the same idiom).
    static auto Make_AxisComponents(
        const FCk_Handle_Minimap& InMinimap,
        TFunction<FVector(const FCk_Handle_Minimap&)> InProjector)
        -> TArray<TAttribute<FText>>
    {
        auto Components = TArray<TAttribute<FText>>{};
        Components.Reserve(3);

        for (auto Axis = 0; Axis < 3; ++Axis)
        {
            Components.Emplace(TAttribute<FText>::CreateLambda([InMinimap, InProjector, Axis]()
            {
                if (ck::Is_NOT_Valid(InMinimap))
                { return FText::FromString(TEXT("--")); }

                return FText::FromString(ck::Format_UE(TEXT("{:.0f}"), InProjector(InMinimap)[Axis]));
            }));
        }

        return Components;
    }
}

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

    Builder.AddAlignedNumericRow(
        FText::FromString(TEXT("View Origin:")),
        ck_inspector_minimap::Make_AxisComponents(CapturedMinimap,
            [](const FCk_Handle_Minimap& InMinimap) { return UCk_Utils_Minimap_UE::Get_ViewOrigin(InMinimap); }));

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

    Builder.AddMeterRow(
        FText::FromString(TEXT("Entries:")),
        TAttribute<float>::CreateLambda([CapturedMinimap]() -> float
        {
            if (ck::Is_NOT_Valid(CapturedMinimap)) { return 0.0f; }
            const auto MaxEntries = CapturedMinimap.Get<ck::FFragment_Minimap_Params>().Get_MaxEntries();
            if (MaxEntries <= 0) { return 0.0f; }
            return static_cast<float>(UCk_Utils_Minimap_UE::Get_Entries(CapturedMinimap).Num()) / static_cast<float>(MaxEntries);
        }),
        ECk_Tone::Accent,
        TAttribute<FText>::CreateLambda([CapturedMinimap]() -> FText
        {
            if (ck::Is_NOT_Valid(CapturedMinimap)) { return FText::FromString(TEXT("--")); }
            const auto& Params = CapturedMinimap.Get<ck::FFragment_Minimap_Params>();
            return FText::FromString(ck::Format_UE(TEXT("{} / {}"),
                UCk_Utils_Minimap_UE::Get_Entries(CapturedMinimap).Num(), Params.Get_MaxEntries()));
        }));

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
