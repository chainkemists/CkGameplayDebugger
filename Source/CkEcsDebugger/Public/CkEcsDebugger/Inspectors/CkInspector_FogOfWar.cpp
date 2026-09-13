#include "CkInspector_FogOfWar.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkMinimap/CkFogOfWar_Fragment.h"
#include "CkMinimap/CkFogOfWar_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Utils/CkDebug_InspectorEditGuard.h"
#include "CkDebuggerCommon/Utils/CkDebug_RequestGate.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NumericEditor.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "CkEditorTools/Style/CkStyle.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_FogOfWar)

// =====================================================================================================================

namespace ck_inspector_fog_of_war
{
    // Bounds are planar — two components, so AddAlignedNumericRow colors them X/Y and the
    // Center / Half-Extents rows line up column-for-column.
    auto TryGet(const FCk_Handle& InEntity, FCk_Handle_FogOfWar& OutFog) -> bool
    {
        OutFog = {};
        if (ck::Is_NOT_Valid(InEntity)
            || InEntity.Has<ck::FTag_DestroyEntity_Initiate>()
            || InEntity.Has<ck::FTag_DestroyEntity_EndPlay>()
            || InEntity.Has<ck::FTag_DestroyEntity_Teardown>()
            || InEntity.Has<ck::FTag_DestroyEntity_Await>()
            || InEntity.Has<ck::FTag_DestroyEntity_Finalize>())
        { return false; }
        auto Mutable = InEntity;
        OutFog = UCk_Utils_FogOfWar_UE::Cast(Mutable);
        return ck::IsValid(OutFog)
            && InEntity.Has<ck::FFragment_FogOfWar_Params>()
            && InEntity.Has<ck::FFragment_FogOfWar_Current>();
    }

    auto DiffColor(const bool bDiffMarked) -> FLinearColor
    {
        return bDiffMarked ? CkStyle::Accent() : CkStyle::Text();
    }

    auto Make_BoundsComponents(
        const FCk_Handle& InEntity,
        TFunction<FVector2D(const FCk_Fragment_FogOfWar_ParamsData&)> InProjector)
        -> TArray<TAttribute<FText>>
    {
        auto Components = TArray<TAttribute<FText>>{};
        Components.Reserve(2);

        for (auto Axis = 0; Axis < 2; ++Axis)
        {
            Components.Emplace(TAttribute<FText>::CreateLambda([InEntity, InProjector, Axis]()
            {
                auto Fog = FCk_Handle_FogOfWar{};
                if (NOT TryGet(InEntity, Fog))
                { return FText::FromString(TEXT("--")); }

                const auto Value = InProjector(Fog.Get<ck::FFragment_FogOfWar_Params>());
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
    auto Fog = FCk_Handle_FogOfWar{};
    return ck_inspector_fog_of_war::TryGet(Entity, Fog);
}

auto FCkInspector_FogOfWar::Build_NativeBody(const FCk_Handle& Entity) const -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();
    Builder.SetEditGuard(Get_EditGuard());

    auto Fog = FCk_Handle_FogOfWar{};
    if (NOT ck_inspector_fog_of_war::TryGet(Entity, Fog))
    { return Builder.Build(Entity, FString()); }

    const auto CapturedEntity = Entity;

    Builder.AddConditionalRow(
        FText::FromString(TEXT("Grid:")),
        [CapturedEntity](const FCk_Handle&)
        {
            auto Fog = FCk_Handle_FogOfWar{};
            if (NOT ck_inspector_fog_of_war::TryGet(CapturedEntity, Fog)) { return FText::FromString(TEXT("--")); }

            const auto CellCounts = UCk_Utils_FogOfWar_UE::Get_CellCounts(Fog);

            if (CellCounts.X <= 0 || CellCounts.Y <= 0)
            { return FText::FromString(TEXT("UNALLOCATED (invalid bounds or cell budget)")); }

            const auto& Params = Fog.Get<ck::FFragment_FogOfWar_Params>();
            return FText::FromString(ck::Format_UE(TEXT("{} x {}  ({:.0f} cm cells)"),
                CellCounts.X, CellCounts.Y, Params.Get_CellSize()));
        },
        [CapturedEntity](const FCk_Handle&) -> FLinearColor
        {
            auto Fog = FCk_Handle_FogOfWar{};
            if (NOT ck_inspector_fog_of_war::TryGet(CapturedEntity, Fog)) { return CkStyle::None(); }
            const auto CellCounts = UCk_Utils_FogOfWar_UE::Get_CellCounts(Fog);
            return (CellCounts.X > 0 && CellCounts.Y > 0)
                ? CkStyle::Value_Numeric()
                : CkStyle::Value_Bool_False();
        });

    Builder.AddMeterRow(
        FText::FromString(TEXT("Explored:")),
        TAttribute<float>::CreateLambda([CapturedEntity]()
        {
            auto Fog = FCk_Handle_FogOfWar{};
            return ck_inspector_fog_of_war::TryGet(CapturedEntity, Fog)
                ? UCk_Utils_FogOfWar_UE::Get_ExploredFraction(Fog) : 0.0f;
        }),
        ECk_Tone::Info,
        TAttribute<FText>::CreateLambda([CapturedEntity]()
        {
            auto Fog = FCk_Handle_FogOfWar{};
            if (NOT ck_inspector_fog_of_war::TryGet(CapturedEntity, Fog)) { return FText::FromString(TEXT("--")); }

            const auto CellCounts = UCk_Utils_FogOfWar_UE::Get_CellCounts(Fog);
            const auto TotalCells = CellCounts.X * CellCounts.Y;
            const auto ExploredFraction = UCk_Utils_FogOfWar_UE::Get_ExploredFraction(Fog);

            return FText::FromString(ck::Format_UE(TEXT("{:.1f}%  ({} / {})"),
                ExploredFraction * 100.0f,
                FMath::RoundToInt32(ExploredFraction * static_cast<float>(TotalCells)), TotalCells));
        }));

    Builder.AddAlignedNumericRow(
        FText::FromString(TEXT("Bounds Center:")),
        ck_inspector_fog_of_war::Make_BoundsComponents(CapturedEntity,
            [](const FCk_Fragment_FogOfWar_ParamsData& InParams) { return InParams.Get_Bounds().Get_Center(); }));

    Builder.AddAlignedNumericRow(
        FText::FromString(TEXT("Bounds Half-Extents:")),
        ck_inspector_fog_of_war::Make_BoundsComponents(CapturedEntity,
            [](const FCk_Fragment_FogOfWar_ParamsData& InParams) { return InParams.Get_Bounds().Get_HalfExtents(); }));

    // ---- Write surface ----
    //
    // CosmeticOnly: exploration is what a LOCAL player has seen, so a dedicated server has no fog to
    // reveal — the controls grey there with the reason rather than firing into nothing.
    Builder.AddActionRow(
        FText::FromString(TEXT("Grid:")),
        {
            FCkInspector_Action
            {
                FText::FromString(TEXT("Reveal All")),
                FText::FromString(TEXT("UCk_Utils_FogOfWar_UE::Request_RevealAll")),
                [CapturedEntity]()
                {
                    auto Fog = FCk_Handle_FogOfWar{};
                    if (ck_inspector_fog_of_war::TryGet(CapturedEntity, Fog)
                        && ck::DebugRequestGate::Evaluate(CapturedEntity, ECk_DebugRequest_Requirement::CosmeticOnly).IsEnabled)
                    { UCk_Utils_FogOfWar_UE::Request_RevealAll(Fog, {}); }
                },
                ECk_DebugRequest_Requirement::CosmeticOnly
            },
            FCkInspector_Action
            {
                FText::FromString(TEXT("Reset")),
                FText::FromString(TEXT("UCk_Utils_FogOfWar_UE::Request_Reset")),
                [CapturedEntity]()
                {
                    auto Fog = FCk_Handle_FogOfWar{};
                    if (ck_inspector_fog_of_war::TryGet(CapturedEntity, Fog)
                        && ck::DebugRequestGate::Evaluate(CapturedEntity, ECk_DebugRequest_Requirement::CosmeticOnly).IsEnabled)
                    { UCk_Utils_FogOfWar_UE::Request_Reset(Fog, {}); }
                },
                ECk_DebugRequest_Requirement::CosmeticOnly
            },
        });

    // Reveal-at-a-point. The location/radius rows edit inspector-owned scratch (see the header) —
    // they are the request's PAYLOAD, not fog state, so nothing reads back from the grid.
    Builder.AddVectorRow(
        FText::FromString(TEXT("Reveal Location:")),
        TAttribute<FVector>::CreateLambda([CapturedEntity, Location = _RevealLocation]()
        {
            auto Fog = FCk_Handle_FogOfWar{};
            return ck_inspector_fog_of_war::TryGet(CapturedEntity, Fog)
                ? *Location : FVector::ZeroVector;
        }),
        [CapturedEntity, Location = _RevealLocation](const FVector& InLocation)
        {
            auto Fog = FCk_Handle_FogOfWar{};
            if (ck_inspector_fog_of_war::TryGet(CapturedEntity, Fog)
                && ck::DebugRequestGate::Evaluate(CapturedEntity, ECk_DebugRequest_Requirement::CosmeticOnly).IsEnabled)
            { *Location = InLocation; }
        },
        ECk_DebugRequest_Requirement::CosmeticOnly);

    Builder.AddNumericRow(
        FText::FromString(TEXT("Reveal Radius:")),
        TAttribute<float>::CreateLambda([CapturedEntity, Radius = _RevealRadius]()
        {
            auto Fog = FCk_Handle_FogOfWar{};
            return ck_inspector_fog_of_war::TryGet(CapturedEntity, Fog)
                ? *Radius : 0.0f;
        }),
        [CapturedEntity, Radius = _RevealRadius](float InRadius)
        {
            auto Fog = FCk_Handle_FogOfWar{};
            if (ck_inspector_fog_of_war::TryGet(CapturedEntity, Fog)
                && ck::DebugRequestGate::Evaluate(CapturedEntity, ECk_DebugRequest_Requirement::CosmeticOnly).IsEnabled)
            { *Radius = InRadius; }
        },
        0.0f,
        TOptional<float>{},
        ECk_DebugRequest_Requirement::CosmeticOnly);

    Builder.AddActionRow(
        FText::FromString(TEXT("Reveal:")),
        {
            FCkInspector_Action
            {
                FText::FromString(TEXT("Reveal Here")),
                FText::FromString(TEXT("UCk_Utils_FogOfWar_UE::Request_RevealLocation (radius 0 = fog params' RevealRadius)")),
                [CapturedEntity, Location = _RevealLocation, Radius = _RevealRadius]()
                {
                    auto Fog = FCk_Handle_FogOfWar{};
                    if (NOT ck_inspector_fog_of_war::TryGet(CapturedEntity, Fog)
                        || NOT ck::DebugRequestGate::Evaluate(CapturedEntity, ECk_DebugRequest_Requirement::CosmeticOnly).IsEnabled)
                    { return; }

                    auto Request = FCk_Request_FogOfWar_RevealLocation{*Location};
                    Request.Set_Radius(*Radius);

                    UCk_Utils_FogOfWar_UE::Request_RevealLocation(Fog, Request, {});
                },
                ECk_DebugRequest_Requirement::CosmeticOnly
            },
        });

    // Request_AddRevealer / Request_RemoveRevealer take an FCk_Handle — entity pickers are deferred.
    Builder.AddCountBadgeRow(
        FText::FromString(TEXT("Revealers:")),
        TAttribute<int32>::CreateLambda([CapturedEntity]()
        {
            auto Fog = FCk_Handle_FogOfWar{};
            return ck_inspector_fog_of_war::TryGet(CapturedEntity, Fog)
                ? Fog.Get<ck::FFragment_FogOfWar_Current>().Get_Revealers().Num() : 0;
        }),
        ECk_Tone::Info);

    Builder.AddRow(
        FText::FromString(TEXT("Reveal Radius:")),
        [CapturedEntity](const FCk_Handle&)
        {
            auto Fog = FCk_Handle_FogOfWar{};
            if (NOT ck_inspector_fog_of_war::TryGet(CapturedEntity, Fog)) { return FText::FromString(TEXT("--")); }
            const auto& Params = Fog.Get<ck::FFragment_FogOfWar_Params>();
            return FText::FromString(ck::Format_UE(TEXT("{:.0f}"), Params.Get_RevealRadius()));
        },
        CkStyle::Value_Numeric());

    Builder.AddRow(
        FText::FromString(TEXT("Interval:")),
        [CapturedEntity](const FCk_Handle&)
        {
            auto Fog = FCk_Handle_FogOfWar{};
            if (NOT ck_inspector_fog_of_war::TryGet(CapturedEntity, Fog)) { return FText::FromString(TEXT("--")); }
            const auto& Params = Fog.Get<ck::FFragment_FogOfWar_Params>();
            const auto UpdateInterval = Params.Get_UpdateInterval();
            return FText::FromString(UpdateInterval <= FCk_Time::ZeroSecond()
                ? TEXT("0 (every frame)")
                : ck::Format_UE(TEXT("{:.2f}s"), UpdateInterval.Get_Seconds()));
        },
        CkStyle::Value_Numeric());

    return Builder.Build(Entity, FString());
}

auto SCkInspector_FogOfWarAuthored::Build_AuthoredView() -> bool
{
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT RegistryResult.Succeeded || NOT Registry.IsValid() || NOT Plugin.IsValid())
    {
        _LoadError = FString::Join(RegistryResult.Errors, TEXT("\n"));
        if (_LoadError.IsEmpty()) { _LoadError = TEXT("Debugger UI registry or plugin is unavailable."); }
        return false;
    }
    const TWeakPtr<SCkInspector_FogOfWarAuthored> Weak{SharedThis(this)};
    auto Ports = FCkUiView::FNativeBindings{};
    Ports.Add(TEXT("fog-reveal-location-port"), Make_NativePort(Build_LocationPort()));
    Ports.Add(TEXT("fog-reveal-radius-port"), Make_NativePort(Build_RadiusPort()));
    auto Data = FCkUiView::FDataBindings{};
    const auto BindText = [&Data, Weak](const FString& Key, FString (SCkInspector_FogOfWarAuthored::* Getter)() const)
    { Data.Text.Add(Key, TAttribute<FText>::CreateLambda([Weak, Getter]() { const auto Widget = Weak.Pin(); return Widget.IsValid() && NOT Widget->Is_Inert() ? FText::FromString((Widget.Get()->*Getter)()) : FText::GetEmpty(); })); };
    BindText(TEXT("fog-grid"), &SCkInspector_FogOfWarAuthored::Get_GridText);
    BindText(TEXT("fog-explored"), &SCkInspector_FogOfWarAuthored::Get_ExploredText);
    BindText(TEXT("fog-center"), &SCkInspector_FogOfWarAuthored::Get_BoundsCenterText);
    BindText(TEXT("fog-extents"), &SCkInspector_FogOfWarAuthored::Get_BoundsHalfExtentsText);
    BindText(TEXT("fog-revealers"), &SCkInspector_FogOfWarAuthored::Get_RevealersText);
    BindText(TEXT("fog-params-radius"), &SCkInspector_FogOfWarAuthored::Get_ParamsRadiusText);
    BindText(TEXT("fog-interval"), &SCkInspector_FogOfWarAuthored::Get_IntervalText);
    Data.Number.Add(TEXT("fog-explored-fraction"), TAttribute<float>::CreateLambda([Weak]() { const auto Widget = Weak.Pin(); return Widget.IsValid() ? Widget->Get_ExploredFraction() : 0.0f; }));
    Data.Color.Add(TEXT("fog-explored-fill"), TAttribute<FLinearColor>::CreateLambda([]() { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("5FBFE8"))); }));
    Data.Visibility.Add(TEXT("fog-can-request"), TAttribute<bool>::CreateLambda([Weak]()
    {
        const auto Widget = Weak.Pin();
        return Widget.IsValid() && Widget->Get_CanRequest();
    }));
    Data.Text.Add(TEXT("fog-disabled-reason"), TAttribute<FText>::CreateLambda([Weak]() { const auto Widget = Weak.Pin(); return Widget.IsValid() ? FText::FromString(Widget->Get_RequestDisabledReason()) : FText::GetEmpty(); }));
    Data.Text.Add(TEXT("fog-reveal-all-label"), FText::FromString(TEXT("Reveal All")));
    Data.Text.Add(TEXT("fog-reset-label"), FText::FromString(TEXT("Reset")));
    Data.Text.Add(TEXT("fog-reveal-here-label"), FText::FromString(TEXT("Reveal Here")));
    for (const TPair<FString, FString>& Pair : TArray<TPair<FString, FString>>{{TEXT("grid"), TEXT("Grid:")}, {TEXT("explored"), TEXT("Explored:")}, {TEXT("center"), TEXT("Bounds Center:")}, {TEXT("extents"), TEXT("Bounds Half-Extents:")}, {TEXT("location"), TEXT("Reveal Location:")}, {TEXT("staged-radius"), TEXT("Reveal Radius:")}, {TEXT("reveal"), TEXT("Reveal:")}, {TEXT("revealers"), TEXT("Revealers:")}, {TEXT("params-radius"), TEXT("Reveal Radius:")}, {TEXT("interval"), TEXT("Interval:")}})
    { Data.Color.Add(TEXT("fog-") + Pair.Key + TEXT("-diff-color"), TAttribute<FLinearColor>::CreateLambda([Weak, Label = Pair.Value]() { const auto Widget = Weak.Pin(); return Widget.IsValid() && NOT Widget->Is_Inert() ? ck_inspector_fog_of_war::DiffColor(Widget->Is_DiffMarked(Label)) : FLinearColor::Transparent; })); }
    auto Actions = FCkUiView::FActions{};
    Actions.Add(TEXT("fog-reveal-all"), FSimpleDelegate::CreateLambda([Weak]() { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Request_RevealAll(); } }));
    Actions.Add(TEXT("fog-reset"), FSimpleDelegate::CreateLambda([Weak]() { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Request_Reset(); } }));
    Actions.Add(TEXT("fog-reveal-here"), FSimpleDelegate::CreateLambda([Weak]() { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Request_RevealHere(); } }));
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([Weak]() { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_CanRequest(); });
    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(MoveTemp(Ports), MoveTemp(Actions), {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString Root = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(FPaths::Combine(Root, TEXT("EcsInspectorFogOfWar.ui.html")), FPaths::Combine(Root, TEXT("EcsInspectorFogOfWar.ui.css")));
    Candidate->PollFiles();
    if (NOT Candidate->GetLastResult().Succeeded) { _LoadError = FString::Join(Candidate->GetLastResult().Errors, TEXT("\n")); return false; }
    _View = Candidate; _Mounted = true; _LoadError.Reset(); return true;
}

auto SCkInspector_FogOfWarAuthored::Tick(const FGeometry& Geometry, const double Time, const float DeltaTime) -> void
{
    SCompoundWidget::Tick(Geometry, Time, DeltaTime);
    if (NOT _Active || NOT _View.IsValid()) { return; }
    _View->PollFiles();
    _LoadError = _View->GetLastResult().Succeeded ? FString{} : FString::Join(_View->GetLastResult().Errors, TEXT("\n"));
}

auto SCkInspector_FogOfWarAuthored::Release() -> void
{
    if (NOT _Active) { return; }
    _Active = false;
    for (const TSharedPtr<FCkInspectorEditScope>& Scope : _EditScopes) { if (Scope.IsValid()) { Scope->Set_Active(false); } }
    _EditScopes.Reset(); Detach_NativePorts(); _Entity = {}; _EditGuard.Reset(); _View.Reset(); _Mounted = false;
}

auto FCkInspector_FogOfWar::Get_StructureMask(const FCk_Handle& Entity) const -> uint8
{
    return ck::IsValid(Entity) ? (Entity.Has<ck::FFragment_FogOfWar_Params>() ? 1 : 0) | (Entity.Has<ck::FFragment_FogOfWar_Current>() ? 2 : 0) : 0;
}

auto FCkInspector_FogOfWar::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> NativeBody = Build_NativeBody(Entity);
    if (FCkInspector_RowCaptureScope::Is_Active()) { return NativeBody; }
    _StructureMask = Get_StructureMask(Entity);
    auto DiffLabels = TSet<FString>{};
    for (const FString& Label : {TEXT("Grid:"), TEXT("Explored:"), TEXT("Bounds Center:"), TEXT("Bounds Half-Extents:"), TEXT("Reveal Location:"), TEXT("Reveal Radius:"), TEXT("Reveal:"), TEXT("Revealers:"), TEXT("Interval:")})
    { if (FCkInspector_DiffMarkScope::Is_LabelMarked(Label)) { DiffLabels.Add(Label); } }
    const TSharedRef<SCkInspector_FogOfWarAuthored> Authored = SNew(SCkInspector_FogOfWarAuthored).Entity(Entity).EditGuard(Get_EditGuard()).DiffLabels(MoveTemp(DiffLabels));
    if (NOT Authored->Is_Mounted()) { _LastAuthoredLoadError = Authored->Get_LoadError(); return NativeBody; }
    _LastAuthoredLoadError.Reset(); _AuthoredInstances.Add(Authored); return Authored;
}

FCkInspector_FogOfWar::~FCkInspector_FogOfWar() { OnDeactivated(); }
auto FCkInspector_FogOfWar::Tick(const FCk_Handle& Entity, const float InDeltaTime) -> void
{
    static_cast<void>(InDeltaTime);
    const uint8 Mask = Get_StructureMask(Entity);
    if (Mask != _StructureMask) { _StructureMask = Mask; RequestRebuild(); }
    _LastAuthoredLoadError.Reset();
    for (const TWeakPtr<SCkInspector_FogOfWarAuthored>& Weak : _AuthoredInstances)
    { if (const auto Instance = Weak.Pin(); Instance.IsValid() && NOT Instance->Get_LoadError().IsEmpty()) { _LastAuthoredLoadError = Instance->Get_LoadError(); break; } }
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_FogOfWarAuthored>& Instance) { return NOT Instance.IsValid() || Instance.Pin()->Is_Inert(); });
}
auto FCkInspector_FogOfWar::OnDeactivated() -> void
{
    for (const TWeakPtr<SCkInspector_FogOfWarAuthored>& Weak : _AuthoredInstances) { if (const auto Instance = Weak.Pin(); Instance.IsValid()) { Instance->Release(); } }
    _AuthoredInstances.Reset();
}

// =====================================================================================================================

auto SCkInspector_FogOfWarAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _EditGuard = InArgs._EditGuard;
    _DiffLabels = InArgs._DiffLabels;
    const TSharedRef<SBox> Host = SNew(SBox);
    ChildSlot[Host];
    if (Build_AuthoredView())
    {
        Host->SetContent(_View->GetRegion(TEXT("main")));
        return;
    }
    Release();
    Host->SetContent(SNullWidget::NullWidget);
}

SCkInspector_FogOfWarAuthored::~SCkInspector_FogOfWarAuthored()
{
    Release();
}

auto SCkInspector_FogOfWarAuthored::Get_IsAvailable() const -> bool
{
    auto Fog = FCk_Handle_FogOfWar{};
    return _Active && ck_inspector_fog_of_war::TryGet(_Entity, Fog);
}

auto SCkInspector_FogOfWarAuthored::Get_CanRequest() const -> bool
{
    auto Fog = FCk_Handle_FogOfWar{};
    return Get_IsAvailable() && ck_inspector_fog_of_war::TryGet(_Entity, Fog)
        && ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::CosmeticOnly).IsEnabled;
}

auto SCkInspector_FogOfWarAuthored::Get_RequestDisabledReason() const -> FString
{
    if (NOT Get_IsAvailable())
    { return TEXT("Fog of war is unavailable."); }
    return ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::CosmeticOnly).Reason.ToString();
}

auto SCkInspector_FogOfWarAuthored::Get_GridText() const -> FString
{
    auto Fog = FCk_Handle_FogOfWar{};
    if (NOT ck_inspector_fog_of_war::TryGet(_Entity, Fog)) { return TEXT("--"); }
    const FIntPoint Counts = UCk_Utils_FogOfWar_UE::Get_CellCounts(Fog);
    if (Counts.X <= 0 || Counts.Y <= 0) { return TEXT("UNALLOCATED (invalid bounds or cell budget)"); }
    return ck::Format_UE(TEXT("{} x {}  ({:.0f} cm cells)"), Counts.X, Counts.Y,
        Fog.Get<ck::FFragment_FogOfWar_Params>().Get_CellSize());
}

auto SCkInspector_FogOfWarAuthored::Get_ExploredFraction() const -> float
{
    auto Fog = FCk_Handle_FogOfWar{};
    return ck_inspector_fog_of_war::TryGet(_Entity, Fog) ? UCk_Utils_FogOfWar_UE::Get_ExploredFraction(Fog) : 0.0f;
}

auto SCkInspector_FogOfWarAuthored::Get_ExploredText() const -> FString
{
    auto Fog = FCk_Handle_FogOfWar{};
    if (NOT ck_inspector_fog_of_war::TryGet(_Entity, Fog)) { return TEXT("--"); }
    const FIntPoint Counts = UCk_Utils_FogOfWar_UE::Get_CellCounts(Fog);
    const int32 Total = Counts.X * Counts.Y;
    const float Fraction = UCk_Utils_FogOfWar_UE::Get_ExploredFraction(Fog);
    return ck::Format_UE(TEXT("{:.1f}%  ({} / {})"), Fraction * 100.0f, FMath::RoundToInt32(Fraction * Total), Total);
}

auto SCkInspector_FogOfWarAuthored::Get_BoundsCenterText() const -> FString
{
    auto Fog = FCk_Handle_FogOfWar{};
    if (NOT ck_inspector_fog_of_war::TryGet(_Entity, Fog)) { return TEXT("--"); }
    return ck::Format_UE(TEXT("{:.0f}  {:.0f}"), Fog.Get<ck::FFragment_FogOfWar_Params>().Get_Bounds().Get_Center().X,
        Fog.Get<ck::FFragment_FogOfWar_Params>().Get_Bounds().Get_Center().Y);
}

auto SCkInspector_FogOfWarAuthored::Get_BoundsHalfExtentsText() const -> FString
{
    auto Fog = FCk_Handle_FogOfWar{};
    if (NOT ck_inspector_fog_of_war::TryGet(_Entity, Fog)) { return TEXT("--"); }
    return ck::Format_UE(TEXT("{:.0f}  {:.0f}"), Fog.Get<ck::FFragment_FogOfWar_Params>().Get_Bounds().Get_HalfExtents().X,
        Fog.Get<ck::FFragment_FogOfWar_Params>().Get_Bounds().Get_HalfExtents().Y);
}

auto SCkInspector_FogOfWarAuthored::Get_RevealersText() const -> FString
{
    auto Fog = FCk_Handle_FogOfWar{};
    return ck_inspector_fog_of_war::TryGet(_Entity, Fog)
        ? FString::FromInt(Fog.Get<ck::FFragment_FogOfWar_Current>().Get_Revealers().Num()) : TEXT("--");
}

auto SCkInspector_FogOfWarAuthored::Get_ParamsRadiusText() const -> FString
{
    auto Fog = FCk_Handle_FogOfWar{};
    return ck_inspector_fog_of_war::TryGet(_Entity, Fog)
        ? ck::Format_UE(TEXT("{:.0f}"), Fog.Get<ck::FFragment_FogOfWar_Params>().Get_RevealRadius()) : TEXT("--");
}

auto SCkInspector_FogOfWarAuthored::Get_IntervalText() const -> FString
{
    auto Fog = FCk_Handle_FogOfWar{};
    if (NOT ck_inspector_fog_of_war::TryGet(_Entity, Fog)) { return TEXT("--"); }
    const FCk_Time Interval = Fog.Get<ck::FFragment_FogOfWar_Params>().Get_UpdateInterval();
    return Interval <= FCk_Time::ZeroSecond() ? TEXT("0 (every frame)") : ck::Format_UE(TEXT("{:.2f}s"), Interval.Get_Seconds());
}

auto SCkInspector_FogOfWarAuthored::Get_StagedLocation() const -> FVector { return Get_IsAvailable() ? _RevealLocation : FVector::ZeroVector; }
auto SCkInspector_FogOfWarAuthored::Get_StagedRadius() const -> float { return Get_IsAvailable() ? _RevealRadius : 0.0f; }
auto SCkInspector_FogOfWarAuthored::Commit_Location(const FVector& InValue) -> void { if (Get_CanRequest()) { _RevealLocation = InValue; } }
auto SCkInspector_FogOfWarAuthored::Commit_Radius(const float InValue) -> void { if (Get_CanRequest()) { _RevealRadius = FMath::Max(0.0f, InValue); } }

auto SCkInspector_FogOfWarAuthored::Request_RevealAll() -> void
{
    auto Fog = FCk_Handle_FogOfWar{};
    if (Get_CanRequest() && ck_inspector_fog_of_war::TryGet(_Entity, Fog)) { UCk_Utils_FogOfWar_UE::Request_RevealAll(Fog, {}); }
}

auto SCkInspector_FogOfWarAuthored::Request_Reset() -> void
{
    auto Fog = FCk_Handle_FogOfWar{};
    if (Get_CanRequest() && ck_inspector_fog_of_war::TryGet(_Entity, Fog)) { UCk_Utils_FogOfWar_UE::Request_Reset(Fog, {}); }
}

auto SCkInspector_FogOfWarAuthored::Request_RevealHere() -> void
{
    auto Fog = FCk_Handle_FogOfWar{};
    if (Get_CanRequest() && ck_inspector_fog_of_war::TryGet(_Entity, Fog))
    {
        auto Request = FCk_Request_FogOfWar_RevealLocation{_RevealLocation};
        Request.Set_Radius(_RevealRadius);
        UCk_Utils_FogOfWar_UE::Request_RevealLocation(Fog, Request, {});
    }
}

auto SCkInspector_FogOfWarAuthored::Make_NativePort(TSharedRef<SWidget> InLeaf) -> TSharedRef<SBox>
{
    const TSharedRef<SBox> Port = SNew(SBox);
    Port->SetContent(MoveTemp(InLeaf));
    _NativePorts.Add(Port);
    return Port;
}

auto SCkInspector_FogOfWarAuthored::Detach_NativePorts() -> void
{
    for (const TSharedPtr<SBox>& Port : _NativePorts)
    { if (Port.IsValid()) { Port->SetContent(SNullWidget::NullWidget); } }
    _NativePorts.Reset();
}

auto SCkInspector_FogOfWarAuthored::Build_LocationPort() -> TSharedRef<SWidget>
{
    const TWeakPtr<SCkInspector_FogOfWarAuthored> Weak{SharedThis(this)};
    const TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        const TSharedPtr<FCkInspectorEditScope> Scope = MakeShared<FCkInspectorEditScope>(_EditGuard);
        _EditScopes.Add(Scope);
        Row->AddSlot().AutoWidth()[SNew(SCkDebug_NumericEditor)
            .Tag(FName{*FString::Printf(TEXT("fog-reveal-location-%d"), Axis)})
            .Value_Lambda([Weak, Axis]() { const auto Widget = Weak.Pin(); return Widget.IsValid() ? Widget->Get_StagedLocation()[Axis] : 0.0; })
            .Kind(ECkDebug_NumericKind::Float).Width(72.0f)
            .OnValueCommitted_Lambda([Weak, Axis](const double InValue)
            { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { auto Value = Widget->Get_StagedLocation(); Value[Axis] = InValue; Widget->Commit_Location(Value); } })
            .OnEditStateChanged_Lambda([Weak, Scope](const bool Editing)
            {
                const auto Widget = Weak.Pin();
                if (Scope.IsValid())
                { Scope->Set_Active(Editing && Widget.IsValid() && NOT Widget->Is_Inert()); }
            })
            .IsEnabled_Lambda([Weak]() { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_CanRequest(); })];
    }
    return Row;
}

auto SCkInspector_FogOfWarAuthored::Build_RadiusPort() -> TSharedRef<SWidget>
{
    const TWeakPtr<SCkInspector_FogOfWarAuthored> Weak{SharedThis(this)};
    const TSharedPtr<FCkInspectorEditScope> Scope = MakeShared<FCkInspectorEditScope>(_EditGuard);
    _EditScopes.Add(Scope);
    return SNew(SCkDebug_NumericEditor).Tag(TEXT("fog-reveal-radius-input"))
        .Value_Lambda([Weak]() { const auto Widget = Weak.Pin(); return Widget.IsValid() ? Widget->Get_StagedRadius() : 0.0; })
        .Kind(ECkDebug_NumericKind::Float).Width(72.0f)
        .OnValueCommitted_Lambda([Weak](const double Value) { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Commit_Radius(Value); } })
        .OnEditStateChanged_Lambda([Weak, Scope](const bool Editing)
        {
            const auto Widget = Weak.Pin();
            if (Scope.IsValid())
            { Scope->Set_Active(Editing && Widget.IsValid() && NOT Widget->Is_Inert()); }
        })
        .IsEnabled_Lambda([Weak]() { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_CanRequest(); });
}
