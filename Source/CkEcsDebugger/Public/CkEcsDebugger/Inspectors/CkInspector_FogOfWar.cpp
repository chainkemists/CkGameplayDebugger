#include "CkInspector_FogOfWar.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkMinimap/CkFogOfWar_Fragment.h"
#include "CkMinimap/CkFogOfWar_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"

#include "CkEditorTools/Style/CkStyle.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_FogOfWar)

// =====================================================================================================================

namespace ck_inspector_fog_of_war
{
    // Bounds are planar — two components, so AddAlignedNumericRow colors them X/Y and the
    // Center / Half-Extents rows line up column-for-column.
    static auto Make_BoundsComponents(
        const FCk_Handle_FogOfWar& InFog,
        TFunction<FVector2D(const FCk_Fragment_FogOfWar_ParamsData&)> InProjector)
        -> TArray<TAttribute<FText>>
    {
        auto Components = TArray<TAttribute<FText>>{};
        Components.Reserve(2);

        for (auto Axis = 0; Axis < 2; ++Axis)
        {
            Components.Emplace(TAttribute<FText>::CreateLambda([InFog, InProjector, Axis]()
            {
                if (ck::Is_NOT_Valid(InFog))
                { return FText::FromString(TEXT("--")); }

                const auto Value = InProjector(InFog.Get<ck::FFragment_FogOfWar_Params>());
                return FText::FromString(ck::Format_UE(TEXT("{:.0f}"), Value[Axis]));
            }));
        }

        return Components;
    }
}

// =====================================================================================================================

auto FCkInspector_FogOfWar::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Fog Of War"));
}

auto FCkInspector_FogOfWar::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity) && UCk_Utils_FogOfWar_UE::Has(Entity);
}

auto FCkInspector_FogOfWar::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
}

auto FCkInspector_FogOfWar::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();

    auto MutableEntity = Entity;
    const auto FogHandle = UCk_Utils_FogOfWar_UE::Cast(MutableEntity);

    if (ck::Is_NOT_Valid(FogHandle))
    { return Builder.Build(Entity, FString()); }

    const auto CapturedFog = FogHandle;

    Builder.AddConditionalRow(
        FText::FromString(TEXT("Grid:")),
        [CapturedFog](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedFog)) { return FText::FromString(TEXT("--")); }

            const auto CellCounts = UCk_Utils_FogOfWar_UE::Get_CellCounts(CapturedFog);

            if (CellCounts.X <= 0 || CellCounts.Y <= 0)
            { return FText::FromString(TEXT("UNALLOCATED (invalid bounds or cell budget)")); }

            const auto& Params = CapturedFog.Get<ck::FFragment_FogOfWar_Params>();
            return FText::FromString(ck::Format_UE(TEXT("{} x {}  ({:.0f} cm cells)"),
                CellCounts.X, CellCounts.Y, Params.Get_CellSize()));
        },
        [CapturedFog](const FCk_Handle&) -> FLinearColor
        {
            if (ck::Is_NOT_Valid(CapturedFog)) { return CkStyle::None(); }
            const auto CellCounts = UCk_Utils_FogOfWar_UE::Get_CellCounts(CapturedFog);
            return (CellCounts.X > 0 && CellCounts.Y > 0)
                ? CkStyle::Value_Numeric()
                : CkStyle::Value_Bool_False();
        });

    Builder.AddMeterRow(
        FText::FromString(TEXT("Explored:")),
        TAttribute<float>::CreateLambda([CapturedFog]()
        {
            if (ck::Is_NOT_Valid(CapturedFog)) { return 0.0f; }
            return UCk_Utils_FogOfWar_UE::Get_ExploredFraction(CapturedFog);
        }),
        ECk_Tone::Info,
        TAttribute<FText>::CreateLambda([CapturedFog]()
        {
            if (ck::Is_NOT_Valid(CapturedFog)) { return FText::FromString(TEXT("--")); }

            const auto CellCounts = UCk_Utils_FogOfWar_UE::Get_CellCounts(CapturedFog);
            const auto TotalCells = CellCounts.X * CellCounts.Y;
            const auto ExploredFraction = UCk_Utils_FogOfWar_UE::Get_ExploredFraction(CapturedFog);

            return FText::FromString(ck::Format_UE(TEXT("{:.1f}%  ({} / {})"),
                ExploredFraction * 100.0f,
                FMath::RoundToInt32(ExploredFraction * static_cast<float>(TotalCells)), TotalCells));
        }));

    Builder.AddAlignedNumericRow(
        FText::FromString(TEXT("Bounds Center:")),
        ck_inspector_fog_of_war::Make_BoundsComponents(CapturedFog,
            [](const FCk_Fragment_FogOfWar_ParamsData& InParams) { return InParams.Get_Bounds().Get_Center(); }));

    Builder.AddAlignedNumericRow(
        FText::FromString(TEXT("Bounds Half-Extents:")),
        ck_inspector_fog_of_war::Make_BoundsComponents(CapturedFog,
            [](const FCk_Fragment_FogOfWar_ParamsData& InParams) { return InParams.Get_Bounds().Get_HalfExtents(); }));

    Builder.AddCountBadgeRow(
        FText::FromString(TEXT("Revealers:")),
        TAttribute<int32>::CreateLambda([CapturedFog]()
        {
            if (ck::Is_NOT_Valid(CapturedFog)) { return 0; }
            return CapturedFog.Get<ck::FFragment_FogOfWar_Current>().Get_Revealers().Num();
        }),
        ECk_Tone::Info);

    Builder.AddRow(
        FText::FromString(TEXT("Reveal Radius:")),
        [CapturedFog](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedFog)) { return FText::FromString(TEXT("--")); }
            const auto& Params = CapturedFog.Get<ck::FFragment_FogOfWar_Params>();
            return FText::FromString(ck::Format_UE(TEXT("{:.0f}"), Params.Get_RevealRadius()));
        },
        CkStyle::Value_Numeric());

    Builder.AddRow(
        FText::FromString(TEXT("Interval:")),
        [CapturedFog](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedFog)) { return FText::FromString(TEXT("--")); }
            const auto& Params = CapturedFog.Get<ck::FFragment_FogOfWar_Params>();
            const auto UpdateInterval = Params.Get_UpdateInterval();
            return FText::FromString(UpdateInterval <= FCk_Time::ZeroSecond()
                ? TEXT("0 (every frame)")
                : ck::Format_UE(TEXT("{:.2f}s"), UpdateInterval.Get_Seconds()));
        },
        CkStyle::Value_Numeric());

    return Builder.Build(Entity, FString());
}

// =====================================================================================================================
