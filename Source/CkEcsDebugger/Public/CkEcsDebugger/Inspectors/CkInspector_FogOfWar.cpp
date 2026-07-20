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

    Builder.AddRow(
        FText::FromString(TEXT("Explored:")),
        [CapturedFog](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedFog)) { return FText::FromString(TEXT("--")); }

            const auto CellCounts = UCk_Utils_FogOfWar_UE::Get_CellCounts(CapturedFog);
            const auto TotalCells = CellCounts.X * CellCounts.Y;
            const auto ExploredFraction = UCk_Utils_FogOfWar_UE::Get_ExploredFraction(CapturedFog);

            return FText::FromString(ck::Format_UE(TEXT("{:.1f}%  ({} / {})"),
                ExploredFraction * 100.0f,
                FMath::RoundToInt32(ExploredFraction * static_cast<float>(TotalCells)), TotalCells));
        },
        CkStyle::Value_Numeric());

    Builder.AddRow(
        FText::FromString(TEXT("Bounds:")),
        [CapturedFog](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedFog)) { return FText::FromString(TEXT("--")); }
            const auto& Bounds = CapturedFog.Get<ck::FFragment_FogOfWar_Params>().Get_Bounds();
            return FText::FromString(ck::Format_UE(TEXT("C({:.0f}, {:.0f})  HE({:.0f}, {:.0f})"),
                Bounds.Get_Center().X, Bounds.Get_Center().Y,
                Bounds.Get_HalfExtents().X, Bounds.Get_HalfExtents().Y));
        },
        CkStyle::Value_Numeric());

    Builder.AddRow(
        FText::FromString(TEXT("Revealers:")),
        [CapturedFog](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedFog)) { return FText::FromString(TEXT("--")); }
            return FText::FromString(ck::Format_UE(TEXT("{}"),
                CapturedFog.Get<ck::FFragment_FogOfWar_Current>().Get_Revealers().Num()));
        },
        CkStyle::Value_Numeric());

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
