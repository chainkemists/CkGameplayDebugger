#include "CkCrowdDebugger/Viewport/SCkCrowdDebugger_3dViewport.h"

#include "AdvancedPreviewScene.h"
#include "EditorViewportClient.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "PrimitiveDrawInterface.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_crowd_debugger_3d_viewport
{
	constexpr int32 MaxImmediateBoxesPerLayer = 10000;

	auto Get_SnapshotBounds(const ck::voxelnav::FDebugSnapshot& InSnapshot) -> FBox
	{
		if (InSnapshot._NavigationBounds.IsValid != 0)
		{ return InSnapshot._NavigationBounds; }
		return InSnapshot._AuthoredBounds;
	}

	auto Get_BoxColor(int32 InLayer) -> FLinearColor
	{
		switch (InLayer)
		{
			case 0: return FLinearColor(0.20f, 0.85f, 1.00f, 0.90f); // merged free
			case 1: return FLinearColor(0.20f, 1.00f, 0.45f, 0.45f); // raw free
			default:return FLinearColor(1.00f, 0.20f, 0.15f, 0.45f); // occupied
		}
	}

	auto Get_StatusColor(ck::voxelnav::EDebugSnapshotStatus InStatus) -> FLinearColor
	{
		using ck::voxelnav::EDebugSnapshotStatus;
		switch (InStatus)
		{
			case EDebugSnapshotStatus::Building: return FLinearColor(0.70f, 0.35f, 1.00f, 0.85f);
			case EDebugSnapshotStatus::StaleCook: return FLinearColor(1.00f, 0.55f, 0.05f, 0.85f);
			case EDebugSnapshotStatus::MissingCook:
			case EDebugSnapshotStatus::Failed: return FLinearColor(1.00f, 0.10f, 0.10f, 0.90f);
			default: return FLinearColor(1.00f, 0.75f, 0.20f, 0.90f);
		}
	}

	auto Get_CanDrawRetainedGeometry(ck::voxelnav::EDebugSnapshotStatus InStatus) -> bool
	{
		using ck::voxelnav::EDebugSnapshotStatus;
		return InStatus == EDebugSnapshotStatus::Current ||
			InStatus == EDebugSnapshotStatus::Building ||
			InStatus == EDebugSnapshotStatus::StaleCook ||
			InStatus == EDebugSnapshotStatus::RuntimeOnly;
	}
}

// --------------------------------------------------------------------------------------------------------------------

class FCkCrowdDebugger_3dViewportClient final : public FEditorViewportClient
{
public:
	FCkCrowdDebugger_3dViewportClient(
		FAdvancedPreviewScene& InPreviewScene,
		const TSharedRef<SEditorViewport>& InViewportWidget)
		: FEditorViewportClient(nullptr, &InPreviewScene, InViewportWidget)
	{
		SetViewportType(LVT_Perspective);
		SetViewLocation(FVector(-1600.0, -1600.0, 1200.0));
		SetViewRotation(FRotator(-25.0, 45.0, 0.0));
		bSetListenerPosition = false;
		SetRealtime(true);
	}

	auto Set_Snapshot(const ck::voxelnav::FDebugSnapshot& InSnapshot) -> void
	{
		_Snapshot = InSnapshot;
		_HasSnapshot = true;
		_FrameBounds = ck_crowd_debugger_3d_viewport::Get_SnapshotBounds(_Snapshot);
	}

	auto Clear_Snapshot() -> void
	{
		_Snapshot = {};
		_FrameBounds = FBox{ForceInit};
		_HasSnapshot = false;
	}

	auto ApplyPreset(ECkCrowdDebugger_CameraPreset InPreset) -> void
	{
		if (InPreset == ECkCrowdDebugger_CameraPreset::FrameAll ||
			InPreset == ECkCrowdDebugger_CameraPreset::FrameSelection)
		{
			FrameBounds();
			return;
		}

		SetViewportType(LVT_Perspective);
		bUsingOrbitCamera = true;

		auto Rotation = FRotator(-25.0, 45.0, 0.0);
		switch (InPreset)
		{
			case ECkCrowdDebugger_CameraPreset::Top:    Rotation = FRotator(-89.9,   0.0, 0.0); break;
			case ECkCrowdDebugger_CameraPreset::Bottom: Rotation = FRotator( 89.9,   0.0, 0.0); break;
			case ECkCrowdDebugger_CameraPreset::Left:   Rotation = FRotator(  0.0, 180.0, 0.0); break;
			case ECkCrowdDebugger_CameraPreset::Right:  Rotation = FRotator(  0.0,   0.0, 0.0); break;
			case ECkCrowdDebugger_CameraPreset::Front:  Rotation = FRotator(  0.0,  90.0, 0.0); break;
			case ECkCrowdDebugger_CameraPreset::Back:   Rotation = FRotator(  0.0, -90.0, 0.0); break;
			default: break;
		}

		SetViewRotation(Rotation);
		FrameBounds();
	}

	virtual void Draw(const FSceneView* InView, FPrimitiveDrawInterface* InPdi) override
	{
		FEditorViewportClient::Draw(InView, InPdi);
		if (InPdi == nullptr || NOT _HasSnapshot)
		{ return; }

		const auto StatusColor = ck_crowd_debugger_3d_viewport::Get_StatusColor(_Snapshot._Status);
		DrawBounds(InPdi, _Snapshot._AuthoredBounds, StatusColor, 2.0f);
		if (NOT ck_crowd_debugger_3d_viewport::Get_CanDrawRetainedGeometry(_Snapshot._Status))
		{ return; }

		DrawBounds(InPdi, _Snapshot._PendingDirtyBounds, FLinearColor(1.0f, 0.50f, 0.10f, 0.95f), 2.0f);
		DrawBounds(InPdi, _Snapshot._ActiveDirtyBounds, FLinearColor(1.0f, 0.10f, 0.10f, 0.95f), 2.0f);
		for (const auto& Chunk : _Snapshot._Chunks)
		{ DrawBounds(InPdi, Chunk._Bounds, _Snapshot._Status == ck::voxelnav::EDebugSnapshotStatus::Current
			? FLinearColor(0.70f, 0.35f, 1.00f, 0.85f) : StatusColor, 1.5f); }
		DrawLayer(InPdi, _Snapshot._MergedFree, 0, StatusColor);
		DrawLayer(InPdi, _Snapshot._RawFree, 1, StatusColor);
		DrawLayer(InPdi, _Snapshot._Occupied, 2, StatusColor);

		for (const auto& Portal : _Snapshot._Portals)
		{ DrawLine(InPdi, Portal._From, Portal._ConnectionPoint, FLinearColor(0.90f, 0.45f, 1.00f), 2.0f);
		  DrawLine(InPdi, Portal._ConnectionPoint, Portal._To, FLinearColor(0.90f, 0.45f, 1.00f), 2.0f); }
	}

private:
	auto FrameBounds() -> void
	{
		if (_FrameBounds.IsValid == 0)
		{ return; }

		const auto Radius = FMath::Max(_FrameBounds.GetExtent().Size(), 100.0);
		const auto Direction = GetViewRotation().Vector();
		SetViewLocation(_FrameBounds.GetCenter() - Direction * Radius * 2.5);
	}

	auto DrawLayer(
		FPrimitiveDrawInterface* InPdi,
		const ck::voxelnav::FDebugSnapshotLayerOutput& InLayer,
		int32 InColorLayer,
		const FLinearColor& InStatusColor) const -> void
	{
		const auto Count = FMath::Min(InLayer._Cells.Num(), ck_crowd_debugger_3d_viewport::MaxImmediateBoxesPerLayer);
		const auto Color = _Snapshot._Status == ck::voxelnav::EDebugSnapshotStatus::Current ||
			_Snapshot._Status == ck::voxelnav::EDebugSnapshotStatus::RuntimeOnly
			? ck_crowd_debugger_3d_viewport::Get_BoxColor(InColorLayer)
			: InStatusColor;
		for (auto Index = 0; Index < Count; ++Index)
		{ DrawBounds(InPdi, InLayer._Cells[Index]._Bounds, Color, 1.0f); }
	}

	auto DrawBounds(FPrimitiveDrawInterface* InPdi, const FBox& InBounds, const FLinearColor& InColor, float InThickness) const -> void
	{
		if (InBounds.IsValid == 0)
		{ return; }

		const FVector Corners[8] =
		{
			{InBounds.Min.X, InBounds.Min.Y, InBounds.Min.Z}, {InBounds.Max.X, InBounds.Min.Y, InBounds.Min.Z},
			{InBounds.Max.X, InBounds.Max.Y, InBounds.Min.Z}, {InBounds.Min.X, InBounds.Max.Y, InBounds.Min.Z},
			{InBounds.Min.X, InBounds.Min.Y, InBounds.Max.Z}, {InBounds.Max.X, InBounds.Min.Y, InBounds.Max.Z},
			{InBounds.Max.X, InBounds.Max.Y, InBounds.Max.Z}, {InBounds.Min.X, InBounds.Max.Y, InBounds.Max.Z}
		};
		constexpr int32 Edges[12][2] = {{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
		for (const auto& Edge : Edges)
		{ DrawLine(InPdi, Corners[Edge[0]], Corners[Edge[1]], InColor, InThickness); }
	}

	auto DrawLine(FPrimitiveDrawInterface* InPdi, const FVector& InFrom, const FVector& InTo, const FLinearColor& InColor, float InThickness) const -> void
	{
		InPdi->DrawLine(InFrom, InTo, InColor, SDPG_Foreground, InThickness);
	}

private:
	ck::voxelnav::FDebugSnapshot _Snapshot;
	FBox _FrameBounds = FBox{ForceInit};
	bool _HasSnapshot = false;
};

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_3dViewport::Construct(const FArguments& InArgs) -> void
{
	_PreviewScene = MakeShared<FAdvancedPreviewScene>(FPreviewScene::ConstructionValues());
	SEditorViewport::Construct(SEditorViewport::FArguments());
}

auto SCkCrowdDebugger_3dViewport::MakeEditorViewportClient() -> TSharedRef<FEditorViewportClient>
{
	check(_PreviewScene.IsValid());
	_ViewportClient = MakeShared<FCkCrowdDebugger_3dViewportClient>(*_PreviewScene, SharedThis(this));
	return _ViewportClient.ToSharedRef();
}

auto SCkCrowdDebugger_3dViewport::Set_VoxelNavSnapshot(const ck::voxelnav::FDebugSnapshot& InSnapshot) -> void
{
	if (_ViewportClient.IsValid())
	{ _ViewportClient->Set_Snapshot(InSnapshot); }
}

auto SCkCrowdDebugger_3dViewport::Clear_VoxelNavSnapshot() -> void
{
	if (_ViewportClient.IsValid())
	{ _ViewportClient->Clear_Snapshot(); }
}

auto SCkCrowdDebugger_3dViewport::Apply_CameraPreset(ECkCrowdDebugger_CameraPreset InPreset) -> void
{
	if (_ViewportClient.IsValid())
	{ _ViewportClient->ApplyPreset(InPreset); }
}

// --------------------------------------------------------------------------------------------------------------------
