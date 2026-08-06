#include "CkDebug_EntityMarkers.h"

#include "CkCore/Debug/CkDebugDraw_Utils.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"

#include "CkEcsExt/Transform/CkTransform_Fragment.h"

// PMG fragment — debug-draw shape entities are excluded from the snapshot
// (each marker/line is itself a transform-bearing entity; without the
// exclusion every marker would receive its own marker next tick).
#include "CkPmg/CkPmg_Fragment.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#endif

// ====================================================================================================================

namespace
{
    // Hierarchy-depth gate for the shared entity-marker preview. -1 = unlimited;
    // 0 = top-level entities only; N = include entities up to N lifetime-owner
    // hops below the registry transient. Tune live: `ck.Debug.EntityMarkers.MaxDepth 0`.
    TAutoConsoleVariable<int32> CVar_EntityMarkers_MaxDepth(
        TEXT("ck.Debug.EntityMarkers.MaxDepth"),
        0,
        TEXT("Max hierarchy depth for the entity-marker preview (overlay + ECS picker). ")
        TEXT("-1 = unlimited, 0 = top-level only, N = up to N levels deep."),
        ECVF_Cheat);

    // When enabled, callers may provide a small call-scoped set of focus roots to
    // expand through every lifetime descendant. The roots themselves remain owned
    // by the caller; FCkDebug_EntityMarkers never caches FCk_Handle instances.
    TAutoConsoleVariable<int32> CVar_EntityMarkers_FocusFullDepth(
        TEXT("ck.Debug.EntityMarkers.FocusFullDepth"),
        0,
        TEXT("1 = expand each Gather FullDepthRoot and its lifetime descendants beyond MaxDepth; 0 = obey MaxDepth for all entities."),
        ECVF_Cheat);

    // Hard cap on the lifetime-owner walk — guards against ownership cycles.
    constexpr auto MaxDepthWalk = 32;

    // Dashed parent→child link presentation.
    constexpr auto LinkDashSize  = 14.0f;
    constexpr auto LinkThickness = 1.5f;
    constexpr auto LinkAlpha     = 0.9f;

    constexpr auto MarkerLabelGapPx = 4.0f;
}

// ====================================================================================================================

namespace ck::DebugMarkers
{
    auto Get_DepthTint(int32 InDepth) -> FLinearColor
    {
        static const FLinearColor Tints[] = {
            { 0.30f, 0.62f, 1.00f, 1.0f }, // 0 — blue (ECS picker default)
            { 0.10f, 1.00f, 0.15f, 1.0f }, // 1 — green
            { 1.00f, 0.90f, 0.00f, 1.0f }, // 2 — yellow
            { 1.00f, 0.45f, 0.00f, 1.0f }, // 3 — orange
            { 1.00f, 0.05f, 0.60f, 1.0f }, // 4+ — magenta
        };
        constexpr auto TintCount = static_cast<int32>(UE_ARRAY_COUNT(Tints));
        return Tints[FMath::Clamp(InDepth, 0, TintCount - 1)];
    }

    auto Get_MaxDepth() -> int32
    {
        return CVar_EntityMarkers_MaxDepth.GetValueOnGameThread();
    }

    auto Get_MaxDepthCVarName() -> const TCHAR*
    {
        return TEXT("ck.Debug.EntityMarkers.MaxDepth");
    }

    auto Get_FocusFullDepth() -> bool
    {
        return CVar_EntityMarkers_FocusFullDepth.GetValueOnGameThread() != 0;
    }

    auto Get_FocusFullDepthCVarName() -> const TCHAR*
    {
        return TEXT("ck.Debug.EntityMarkers.FocusFullDepth");
    }
}

// ====================================================================================================================

auto
    FCkDebug_EntityMarkers::
    Gather(
        UWorld* InWorld,
        const FGatherParams& InParams)
    -> int32
{
    _Entries.Reset();
    _EntryNums.Reset();

    if (ck::Is_NOT_Valid(InWorld))
    { return 0; }

    auto TransientEntity = UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(InWorld);
    if (ck::Is_NOT_Valid(TransientEntity))
    { return 0; }

    const auto MaxDepth     = ck::DebugMarkers::Get_MaxDepth();
    const auto CullRadiusSq = InParams.CullRadius * InParams.CullRadius;

    // Validate roots once for this synchronous Gather call. Do not publish or
    // retain a partial/mutable root set: invalid roots simply contribute nothing.
    auto FullDepthRoots = TArray<FCk_Handle>{};
    if (ck::DebugMarkers::Get_FocusFullDepth())
    {
        FullDepthRoots.Reserve(InParams.FullDepthRoots.Num());
        for (const auto& Root : InParams.FullDepthRoots)
        {
            if (ck::Is_NOT_Valid(Root))
            { continue; }

            FullDepthRoots.Add(Root);
        }
    }

    TransientEntity.View<ck::FFragment_Transform, CK_IGNORE_PENDING_KILL>().ForEach(
    [&](FCk_Entity InEntity, const ck::FFragment_Transform& InTransform)
    {
        const auto Handle = ck::MakeHandle(InEntity, TransientEntity);
        if (ck::Is_NOT_Valid(Handle))
        { return; }

        if (Handle.Has<ck::FFragment_Pmg_DebugShape_Common>())
        { return; }

        const auto WorldPos = InTransform.Get_Transform().GetLocation();

        if (InParams.CullOrigin.IsSet() &&
            FVector::DistSquared(WorldPos, InParams.CullOrigin.GetValue()) > CullRadiusSq)
        { return; }

        // Hierarchy depth = lifetime-owner hops to the transient (0 = top-level).
        auto Depth          = 0;
        auto OwnerEntityNum = MAX_uint32;
        auto IsInFullDepthSubtree = false;
        {
            for (const auto& Root : FullDepthRoots)
            {
                if (Handle == Root)
                {
                    IsInFullDepthSubtree = true;
                    break;
                }
            }

            auto Owner = UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Handle);
            if (ck::IsValid(Owner) && NOT (Owner == TransientEntity))
            {
                OwnerEntityNum = static_cast<uint32>(Owner.Get_Entity().Get_EntityNumber());
            }
            while (ck::IsValid(Owner) && NOT (Owner == TransientEntity) && Depth < MaxDepthWalk)
            {
                ++Depth;

                for (const auto& Root : FullDepthRoots)
                {
                    if (Owner == Root)
                    {
                        IsInFullDepthSubtree = true;
                        break;
                    }
                }

                Owner = UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(Owner);
            }
        }
        if (NOT IsInFullDepthSubtree && MaxDepth >= 0 && Depth > MaxDepth)
        { return; }

        if (InParams.Filter && NOT InParams.Filter(Handle))
        { return; }

        auto Entry           = FEntry{};
        Entry.Entity         = Handle;
        Entry.WorldPos       = WorldPos;
        Entry.Depth          = Depth;
        Entry.EntityNum      = static_cast<uint32>(InEntity.Get_EntityNumber());
        Entry.OwnerEntityNum = OwnerEntityNum;

        _EntryNums.Add(Entry.EntityNum);
        _Entries.Add(MoveTemp(Entry));
    });

    return _Entries.Num();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebug_EntityMarkers::
    DrawLinks(
        UWorld* InWorld)
    const -> void
{
    if (ck::Is_NOT_Valid(InWorld))
    { return; }

    auto PosByEntity = TMap<uint32, FVector>{};
    PosByEntity.Reserve(_Entries.Num());
    for (const auto& Entry : _Entries)
    {
        PosByEntity.Add(Entry.EntityNum, Entry.WorldPos);
    }

    for (const auto& Entry : _Entries)
    {
        if (Entry.OwnerEntityNum == MAX_uint32)
        { continue; }

        const auto* ParentPos = PosByEntity.Find(Entry.OwnerEntityNum);
        if (ParentPos == nullptr)
        { continue; }

        auto LinkColor = ck::DebugMarkers::Get_DepthTint(Entry.Depth);
        LinkColor.A = LinkAlpha;

        UCk_Utils_DebugDraw_UE::DrawDebugDashedLine(
            InWorld,
            *ParentPos,
            Entry.WorldPos,
            LinkDashSize,
            LinkColor,
            /*InDuration=*/0.0f,
            LinkThickness);
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebug_EntityMarkers::
    DrawMarkers(
        UCanvas* InCanvas,
        const FDrawParams& InParams)
    -> void
{
    if (InCanvas == nullptr)
    { return; }

    EnsureTextures();

    // If the textures failed to load outright — bad path, missing asset — bail
    // silently; consumers stay functional, just without the visual preview.
    auto* MarkerTex = _MarkerTexture.Get();
    auto* HoverTex  = _MarkerHoverTexture.IsValid() ? _MarkerHoverTexture.Get() : MarkerTex;
    if (MarkerTex == nullptr || HoverTex == nullptr)
    { return; }

    const auto* MarkerResource = MarkerTex->GetResource();
    const auto* HoverResource  = HoverTex->GetResource();
    if (MarkerResource == nullptr || HoverResource == nullptr)
    { return; }

    const auto CanvasSizeX = static_cast<float>(InCanvas->SizeX);
    const auto CanvasSizeY = static_cast<float>(InCanvas->SizeY);

    // Defer the emphasized entry's draw so it renders on top of any cluster.
    auto Emphasized = TOptional<FEntry>{};

    for (const auto& Entry : _Entries)
    {
        if (Entry.EntityNum == InParams.EmphasizedEntityNum)
        {
            Emphasized = Entry;
            continue;
        }

        if (InParams.SuppressedEntityNums != nullptr &&
            InParams.SuppressedEntityNums->Contains(Entry.EntityNum))
        { continue; }

        const auto Projected = InCanvas->Project(Entry.WorldPos);
        if (Projected.Z <= 0.0f)
        { continue; }
        if (Projected.X < 0.0f || Projected.Y < 0.0f || Projected.X >= CanvasSizeX || Projected.Y >= CanvasSizeY)
        { continue; }

        auto Tint = ck::DebugMarkers::Get_DepthTint(Entry.Depth);
        Tint.A = InParams.DefaultAlpha;

        const auto TopLeft = FVector2D(
            static_cast<float>(Projected.X) - InParams.TileSizePx * 0.5f,
            static_cast<float>(Projected.Y) - InParams.TileSizePx * 0.5f);

        auto Tile = FCanvasTileItem(
            TopLeft, MarkerResource, FVector2D(InParams.TileSizePx, InParams.TileSizePx), Tint);
        Tile.BlendMode = SE_BLEND_Translucent;
        InCanvas->DrawItem(Tile);
    }

    if (Emphasized.IsSet())
    {
        const auto Projected = InCanvas->Project(Emphasized->WorldPos);
        if (Projected.Z > 0.0f)
        {
            const auto EmphSize = InParams.TileSizePx * InParams.EmphasizedScale;

            auto Tint = ck::DebugMarkers::Get_DepthTint(Emphasized->Depth);
            Tint.A = 1.0f;

            const auto TopLeft = FVector2D(
                static_cast<float>(Projected.X) - EmphSize * 0.5f,
                static_cast<float>(Projected.Y) - EmphSize * 0.5f);

            auto Tile = FCanvasTileItem(TopLeft, HoverResource, FVector2D(EmphSize, EmphSize), Tint);
            Tile.BlendMode = SE_BLEND_Translucent;
            InCanvas->DrawItem(Tile);

            if (NOT InParams.EmphasizedLabel.IsEmpty())
            {
                if (auto* Font = GEngine != nullptr ? GEngine->GetSmallFont() : nullptr)
                {
                    auto TextItem = FCanvasTextItem(
                        FVector2D(
                            static_cast<float>(Projected.X),
                            static_cast<float>(Projected.Y) - EmphSize * 0.5f - MarkerLabelGapPx),
                        FText::FromString(InParams.EmphasizedLabel), Font, Tint);
                    TextItem.bCentreX = true;
                    TextItem.EnableShadow(FLinearColor::Black);
                    InCanvas->DrawItem(TextItem);
                }
            }
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebug_EntityMarkers::
    EnsureTextures()
    -> void
{
    if (NOT _MarkerTexture.IsValid())
    {
        _MarkerTexture.Reset(LoadObject<UTexture2D>(
            nullptr, TEXT("/CkDebugger/Textures/T_ECSPicker_Marker.T_ECSPicker_Marker")));
    }
    if (NOT _MarkerHoverTexture.IsValid())
    {
        _MarkerHoverTexture.Reset(LoadObject<UTexture2D>(
            nullptr, TEXT("/CkDebugger/Textures/T_ECSPicker_Marker_Hover.T_ECSPicker_Marker_Hover")));
    }

#if WITH_EDITOR
    // Freshly-imported textures compile platform data asynchronously; LoadObject
    // returns before that finishes, so GetResource() can point at a placeholder
    // whose FRHITexture is never properly initialized. Force completion before
    // anyone calls GetResource().
    auto TexturesToFinish = TArray<UTexture*>{};
    if (_MarkerTexture.IsValid())      { TexturesToFinish.Add(_MarkerTexture.Get()); }
    if (_MarkerHoverTexture.IsValid()) { TexturesToFinish.Add(_MarkerHoverTexture.Get()); }
    if (TexturesToFinish.Num() > 0)
    {
        FTextureCompilingManager::Get().FinishCompilation(TexturesToFinish);
    }
#endif
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebug_EntityMarkers::
    Reset()
    -> void
{
    _Entries.Reset();
    _EntryNums.Reset();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkDebug_EntityMarkers::
    Contains(
        const FCk_Handle& InEntity)
    const -> bool
{
    if (ck::Is_NOT_Valid(InEntity))
    { return false; }

    return _EntryNums.Contains(
        static_cast<uint32>(InEntity.Get_Entity().Get_EntityNumber()));
}

// --------------------------------------------------------------------------------------------------------------------
