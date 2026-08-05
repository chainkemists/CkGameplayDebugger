#include "CkCrowdDebugger/Viewport/SCkCrowdDebugger_3dViewport.h"

#include "EditorViewportClient.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "PrimitiveDrawInterface.h"
#include "PrimitiveDrawingUtils.h"
#include "PreviewScene.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_crowd_debugger_3d_viewport
{
	constexpr int32 MaxImmediateBoxesPerLayer = 10000;
	constexpr int32 AgentCapsuleSides = 16;
	constexpr float MaxVelocityVectorLength = 250.0f;

	struct FAgentRenderSnapshot
	{
		FVector _Feet = FVector::ZeroVector;
		FVector _Velocity = FVector::ZeroVector;
		FLinearColor _Color = FLinearColor::White;
		float _Radius = 0.0f;
		float _HalfHeight = 0.0f;
		int32 _SourceIndex = INDEX_NONE;
		bool _Selected = false;
	};

	auto Intersect_RaySphereNearT(
		const FVector& InOrigin,
		const FVector& InDirection,
		const FVector& InCenter,
		double InRadius) -> TOptional<double>
	{
		const auto ToCenter = InCenter - InOrigin;
		const auto AlongRay = FVector::DotProduct(ToCenter, InDirection);
		const auto CenterDistanceSq = FVector::DotProduct(ToCenter, ToCenter);
		const auto Discriminant = AlongRay * AlongRay - (CenterDistanceSq - InRadius * InRadius);
		if (Discriminant < 0.0)
		{ return {}; }

		const auto NearT = AlongRay - FMath::Sqrt(Discriminant);
		const auto FarT = AlongRay + FMath::Sqrt(Discriminant);
		if (FarT < 0.0)
		{ return {}; }
		return FMath::Max(NearT, 0.0);
	}

	auto Intersect_RayCapsuleNearT(
		const FVector& InOrigin,
		const FVector& InDirection,
		const FVector& InCenter,
		double InRadius,
		double InHalfHeight) -> TOptional<double>
	{
		const auto CylinderHalfHeight = FMath::Max(InHalfHeight - InRadius, 0.0);
		const auto Axis = FVector::UpVector;
		const auto Offset = InOrigin - InCenter;
		auto BestT = TNumericLimits<double>::Max();

		const auto DirectionAlongAxis = FVector::DotProduct(InDirection, Axis);
		const auto OffsetAlongAxis = FVector::DotProduct(Offset, Axis);
		const auto DirectionPerpendicular = InDirection - Axis * DirectionAlongAxis;
		const auto OffsetPerpendicular = Offset - Axis * OffsetAlongAxis;
		const auto A = FVector::DotProduct(DirectionPerpendicular, DirectionPerpendicular);
		const auto B = 2.0 * FVector::DotProduct(OffsetPerpendicular, DirectionPerpendicular);
		const auto C = FVector::DotProduct(OffsetPerpendicular, OffsetPerpendicular) - InRadius * InRadius;

		if (A > UE_DOUBLE_SMALL_NUMBER)
		{
			const auto Discriminant = B * B - 4.0 * A * C;
			if (Discriminant >= 0.0)
			{
				const auto Root = FMath::Sqrt(Discriminant);
				const double Candidates[2] =
				{
					(-B - Root) / (2.0 * A),
					(-B + Root) / (2.0 * A)
				};
				for (const auto CandidateT : Candidates)
				{
					if (CandidateT < 0.0)
					{ continue; }
					const auto HeightAtHit = OffsetAlongAxis + CandidateT * DirectionAlongAxis;
					if (FMath::Abs(HeightAtHit) <= CylinderHalfHeight)
					{ BestT = FMath::Min(BestT, CandidateT); }
				}
			}
		}

		const auto TopCenter = InCenter + Axis * CylinderHalfHeight;
		const auto BottomCenter = InCenter - Axis * CylinderHalfHeight;
		for (const auto& CapCenter : {TopCenter, BottomCenter})
		{
			const auto CapT = Intersect_RaySphereNearT(InOrigin, InDirection, CapCenter, InRadius);
			if (CapT.IsSet())
			{ BestT = FMath::Min(BestT, *CapT); }
		}

		return BestT < TNumericLimits<double>::Max()
			? TOptional<double>{BestT}
			: TOptional<double>{};
	}

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

	auto Get_AgentColor(ECkCrowdDebugger_AgentStatus InStatus) -> FLinearColor
	{
		switch (InStatus)
		{
			case ECkCrowdDebugger_AgentStatus::Walking:     return FLinearColor(0.20f, 0.75f, 1.00f, 1.00f);
			case ECkCrowdDebugger_AgentStatus::Idle:        return FLinearColor(0.55f, 0.55f, 0.55f, 1.00f);
			case ECkCrowdDebugger_AgentStatus::Replanning:  return FLinearColor(1.00f, 0.70f, 0.15f, 1.00f);
			case ECkCrowdDebugger_AgentStatus::Failed:      return FLinearColor(1.00f, 0.20f, 0.15f, 1.00f);
			case ECkCrowdDebugger_AgentStatus::Asleep:      return FLinearColor(0.35f, 0.35f, 0.35f, 1.00f);
			case ECkCrowdDebugger_AgentStatus::PlayerProxy: return FLinearColor(0.25f, 1.00f, 0.55f, 1.00f);
			default:                                        return FLinearColor(0.65f, 0.65f, 0.65f, 1.00f);
		}
	}
}

// --------------------------------------------------------------------------------------------------------------------

class FCkCrowdDebugger_3dViewportClient final : public FEditorViewportClient
{
public:
	FCkCrowdDebugger_3dViewportClient(
		FPreviewScene& InPreviewScene,
		const TSharedRef<SEditorViewport>& InViewportWidget,
		FOnCkCrowdDebugger_AgentPicked InOnAgentPicked)
		: FEditorViewportClient(nullptr, &InPreviewScene, InViewportWidget)
		, _OnAgentPicked(MoveTemp(InOnAgentPicked))
	{
		EngineShowFlags.SetEyeAdaptation(false);
		SetViewportType(LVT_Perspective);
		SetViewLocation(FVector(-1600.0, -1600.0, 1200.0));
		SetViewRotation(FRotator(-25.0, 45.0, 0.0));
		bSetListenerPosition = false;
		SetRealtime(true);
	}

	virtual auto GetBackgroundColor() const -> FLinearColor override
	{
		return FLinearColor::Black;
	}

	auto Set_Snapshot(const ck::voxelnav::FDebugSnapshot& InSnapshot) -> void
	{
		_Snapshot = InSnapshot;
		_HasSnapshot = true;
		_VoxelFrameBounds = ck_crowd_debugger_3d_viewport::Get_SnapshotBounds(_Snapshot);
		Invalidate();
	}

	auto Clear_Snapshot() -> void
	{
		_Snapshot = {};
		_VoxelFrameBounds = FBox{ForceInit};
		_HasSnapshot = false;
		Invalidate();
	}

	auto Set_AgentSnapshots(
		const TArray<FCkCrowdDebugger_AgentSnapshot>& InAgents,
		const FCk_Handle& InSelectedHandle) -> void
	{
		_Agents.Reset(InAgents.Num());
		_AgentFrameBounds = FBox{ForceInit};
		_SelectedAgentBounds = FBox{ForceInit};
		_SelectedAgentPosition = FVector::ZeroVector;
		_SelectedPath.Reset();

		const auto HasSelection = ck::IsValid(InSelectedHandle);
		for (auto AgentIndex = 0; AgentIndex < InAgents.Num(); ++AgentIndex)
		{
			const auto& Agent = InAgents[AgentIndex];
			const auto Radius = FMath::Max(Agent.Radius, 12.0f);
			const auto Height = FMath::Max(Agent.Height, Radius * 2.0f);
			const auto HalfHeight = Height * 0.5f;
			const auto Selected = HasSelection && Agent.Handle == InSelectedHandle;
			const auto Color = Selected
				? FLinearColor(1.00f, 0.90f, 0.20f, 1.00f)
				: ck_crowd_debugger_3d_viewport::Get_AgentColor(Agent.Status);

			_Agents.Add({Agent.Position, Agent.Velocity, Color, Radius, HalfHeight, AgentIndex, Selected});

			const auto AgentBounds = FBox(
				Agent.Position - FVector{Radius, Radius, 0.0},
				Agent.Position + FVector{Radius, Radius, Height});
			_AgentFrameBounds += AgentBounds;
			if (Selected)
			{
				_SelectedAgentBounds = AgentBounds;
				_SelectedAgentPosition = Agent.Position;
				_SelectedPath = Agent.PlannedPath;
			}
		}

		Invalidate();
	}

	auto ApplyPreset(ECkCrowdDebugger_CameraPreset InPreset) -> void
	{
		if (InPreset == ECkCrowdDebugger_CameraPreset::FrameAll ||
			InPreset == ECkCrowdDebugger_CameraPreset::FrameSelection)
		{
			const auto Bounds = InPreset == ECkCrowdDebugger_CameraPreset::FrameSelection &&
				_SelectedAgentBounds.IsValid != 0
				? _SelectedAgentBounds
				: Get_AllFrameBounds();
			FrameBounds(Bounds);
			return;
		}

		auto DesiredViewportType = LVT_Perspective;
		switch (InPreset)
		{
			case ECkCrowdDebugger_CameraPreset::Top:    DesiredViewportType = LVT_OrthoTop; break;
			case ECkCrowdDebugger_CameraPreset::Bottom: DesiredViewportType = LVT_OrthoBottom; break;
			case ECkCrowdDebugger_CameraPreset::Left:   DesiredViewportType = LVT_OrthoLeft; break;
			case ECkCrowdDebugger_CameraPreset::Right:  DesiredViewportType = LVT_OrthoRight; break;
			case ECkCrowdDebugger_CameraPreset::Front:  DesiredViewportType = LVT_OrthoFront; break;
			case ECkCrowdDebugger_CameraPreset::Back:   DesiredViewportType = LVT_OrthoBack; break;
			default: break;
		}

		SetViewportType(DesiredViewportType);
		if (DesiredViewportType == LVT_Perspective)
		{ SetViewRotation(FRotator(-25.0, 45.0, 0.0)); }
		FrameBounds(Get_AllFrameBounds());
	}

	virtual auto InputKey(const FInputKeyEventArgs& InEventArgs) -> bool override
	{
		const auto IsPlainLeftClick = InEventArgs.Key == EKeys::LeftMouseButton &&
			InEventArgs.Event == IE_Pressed &&
			InEventArgs.Viewport != nullptr &&
			NOT InEventArgs.Viewport->KeyState(EKeys::LeftAlt) &&
			NOT InEventArgs.Viewport->KeyState(EKeys::RightAlt) &&
			NOT InEventArgs.Viewport->KeyState(EKeys::MiddleMouseButton) &&
			NOT InEventArgs.Viewport->KeyState(EKeys::RightMouseButton);
		if (IsPlainLeftClick && _OnAgentPicked.IsBound())
		{
			const auto Cursor = GetCursorWorldLocationFromMousePos();
			const auto PickedAgentIndex = PickAgent(Cursor.GetOrigin(), Cursor.GetDirection());
			if (PickedAgentIndex.IsSet())
			{
				_OnAgentPicked.Execute(*PickedAgentIndex);
				return true;
			}
		}

		const auto IsMouseWheel = InEventArgs.Key == EKeys::MouseScrollUp ||
			InEventArgs.Key == EKeys::MouseScrollDown;
		const auto IsSpeedChange = IsPerspective() &&
			InEventArgs.Event == IE_Pressed &&
			IsMouseWheel &&
			InEventArgs.Viewport != nullptr &&
			InEventArgs.Viewport->KeyState(EKeys::RightMouseButton);
		if (IsSpeedChange)
		{
			auto SpeedSettings = GetCameraSpeedSettings();
			const auto CurrentSpeed = SpeedSettings.GetCurrentSpeed();
			const auto Direction = InEventArgs.Key == EKeys::MouseScrollDown ? -1.0f : 1.0f;
			SpeedSettings.SetCurrentSpeed(CurrentSpeed + CurrentSpeed * 0.1f * Direction);
			SetCameraSpeedSettings(SpeedSettings);
			return true;
		}

		return FEditorViewportClient::InputKey(InEventArgs);
	}

	virtual void Draw(const FSceneView* InView, FPrimitiveDrawInterface* InPdi) override
	{
		FEditorViewportClient::Draw(InView, InPdi);
		if (InPdi == nullptr)
		{ return; }

		if (_HasSnapshot)
		{
			const auto StatusColor = ck_crowd_debugger_3d_viewport::Get_StatusColor(_Snapshot._Status);
			DrawBounds(InPdi, _Snapshot._AuthoredBounds, StatusColor, 2.0f);
			if (ck_crowd_debugger_3d_viewport::Get_CanDrawRetainedGeometry(_Snapshot._Status))
			{
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
		}

		DrawAgents(InPdi);
	}

private:
	auto PickAgent(const FVector& InOrigin, const FVector& InDirection) const -> TOptional<int32>
	{
		const auto RayDirection = InDirection.GetSafeNormal();
		if (RayDirection.IsNearlyZero())
		{ return {}; }

		auto BestT = TNumericLimits<double>::Max();
		int32 BestAgentIndex = INDEX_NONE;
		for (const auto& Agent : _Agents)
		{
			const auto Center = Agent._Feet + FVector::UpVector * Agent._HalfHeight;
			const auto HitT = ck_crowd_debugger_3d_viewport::Intersect_RayCapsuleNearT(
				InOrigin,
				RayDirection,
				Center,
				Agent._Radius,
				Agent._HalfHeight);
			if (NOT HitT.IsSet() || *HitT >= BestT)
			{ continue; }

			BestT = *HitT;
			BestAgentIndex = Agent._SourceIndex;
		}

		return BestAgentIndex != INDEX_NONE
			? TOptional<int32>{BestAgentIndex}
			: TOptional<int32>{};
	}

	auto Get_AllFrameBounds() const -> FBox
	{
		auto Bounds = _VoxelFrameBounds;
		Bounds += _AgentFrameBounds;
		return Bounds;
	}

	auto FrameBounds(const FBox& InBounds) -> void
	{
		if (InBounds.IsValid != 0)
		{ FocusViewportOnBox(InBounds, true); }
	}

	auto DrawAgents(FPrimitiveDrawInterface* InPdi) const -> void
	{
		for (const auto& Agent : _Agents)
		{
			const auto Center = Agent._Feet + FVector::UpVector * Agent._HalfHeight;
			const auto Thickness = Agent._Selected ? 3.0f : 1.5f;
			DrawWireCapsule(
				InPdi,
				Center,
				FVector::XAxisVector,
				FVector::YAxisVector,
				FVector::ZAxisVector,
				Agent._Color,
				Agent._Radius,
				Agent._HalfHeight,
				ck_crowd_debugger_3d_viewport::AgentCapsuleSides,
				SDPG_Foreground,
				Thickness);

			if (NOT Agent._Velocity.IsNearlyZero())
			{
				const auto VelocityEnd = Center + Agent._Velocity.GetClampedToMaxSize(
					ck_crowd_debugger_3d_viewport::MaxVelocityVectorLength);
				DrawLine(InPdi, Center, VelocityEnd, Agent._Color, Thickness);
			}
		}

		auto Previous = _SelectedAgentPosition;
		for (const auto& Waypoint : _SelectedPath)
		{
			DrawLine(InPdi, Previous, Waypoint, FLinearColor(1.00f, 0.90f, 0.20f, 1.00f), 3.0f);
			Previous = Waypoint;
		}
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
	TArray<ck_crowd_debugger_3d_viewport::FAgentRenderSnapshot> _Agents;
	TArray<FVector> _SelectedPath;
	FBox _VoxelFrameBounds = FBox{ForceInit};
	FBox _AgentFrameBounds = FBox{ForceInit};
	FBox _SelectedAgentBounds = FBox{ForceInit};
	FVector _SelectedAgentPosition = FVector::ZeroVector;
	FOnCkCrowdDebugger_AgentPicked _OnAgentPicked;
	bool _HasSnapshot = false;
};

// --------------------------------------------------------------------------------------------------------------------

auto SCkCrowdDebugger_3dViewport::Construct(const FArguments& InArgs) -> void
{
	_OnAgentPicked = InArgs._OnAgentPicked;
	_PreviewScene = MakeShared<FPreviewScene>(
		FPreviewScene::ConstructionValues()
			.SetCreateDefaultLighting(false)
			.SetCreatePhysicsScene(false));
	SEditorViewport::Construct(SEditorViewport::FArguments());
}

auto SCkCrowdDebugger_3dViewport::MakeEditorViewportClient() -> TSharedRef<FEditorViewportClient>
{
	check(_PreviewScene.IsValid());
	_ViewportClient = MakeShared<FCkCrowdDebugger_3dViewportClient>(
		*_PreviewScene,
		SharedThis(this),
		_OnAgentPicked);
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

auto SCkCrowdDebugger_3dViewport::Set_AgentSnapshots(
	const TArray<FCkCrowdDebugger_AgentSnapshot>& InAgents,
	const FCk_Handle& InSelectedHandle) -> void
{
	if (_ViewportClient.IsValid())
	{ _ViewportClient->Set_AgentSnapshots(InAgents, InSelectedHandle); }
}

auto SCkCrowdDebugger_3dViewport::Apply_CameraPreset(ECkCrowdDebugger_CameraPreset InPreset) -> void
{
	if (_ViewportClient.IsValid())
	{ _ViewportClient->ApplyPreset(InPreset); }
}

// --------------------------------------------------------------------------------------------------------------------
