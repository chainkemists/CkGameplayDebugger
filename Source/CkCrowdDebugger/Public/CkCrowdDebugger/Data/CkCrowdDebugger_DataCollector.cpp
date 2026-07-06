#include "CkCrowdDebugger/Data/CkCrowdDebugger_DataCollector.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkEcs/Handle/CkHandle.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"

#include "CkCrowd/Agent/CkCrowdAgent_Fragment.h"
#include "CkCrowd/Agent/CkCrowdAgent_Fragment_Data.h"
#include "CkCrowd/Agent/CkCrowdAgent_Neighbors_Fragment.h"

#include "CkEcsExt/Transform/CkTransform_Utils.h"

#include "CkNavigation/Nav/CkNav_Algorithm.h"
#include "CkNavigation/Nav/CkNav_Fragment.h"
#include "CkNavigation/Nav/CkNav_Fragment_Data.h"

#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"

#if WITH_EDITOR
#include "Editor.h"
#include "LevelEditorViewport.h"
#endif
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Class.h"

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkCrowdDebugger_DataCollector::
	Collect(UWorld* InWorld)
	-> void
{
	_Agents.Reset();

	// Reset only the per-tick-sampled fields. Health-check fields are sticky across
	// ticks (set explicitly by Run_HealthCheckProbe; not derived from the live world).
	_NavmeshStatus._Sampled = false;
	_NavmeshStatus._NavSystemPresent = false;
	_NavmeshStatus._NavDataClassName.Empty();
	_NavmeshStatus._DefaultFilterValid = false;
	_NavmeshStatus._SupportedAgents = 0;
	_NavmeshStatus._LastRegenTimestamp = -1.0;
	_NavmeshStatus._NavBoundsValid = false;
	_ViewYawValid = false;

	// Per CkDebuggerCommon convention: guard on world validity AND HasBegunPlay
	// (worlds appear in GEngine->GetWorldContexts before BeginPlay).
	if (NOT IsValid(InWorld))
	{ return; }

	if (NOT InWorld->HasBegunPlay())
	{ return; }

	// Sample the view yaw so the viewport can orient to the camera.
	auto GotView = false;

#if WITH_EDITOR
	// When ejected from PIE (F8) the live camera is the editor's perspective viewport, NOT any PIE
	// player controller (whose view freezes on the abandoned pawn). GEditor->bIsSimulatingInEditor is
	// true in that ejected/Simulate state, so pull the yaw straight from the level-editor viewport.
	if (GEditor != nullptr && GEditor->bIsSimulatingInEditor)
	{
		auto* ViewportClient = GCurrentLevelEditingViewportClient;
		if (ViewportClient == nullptr || NOT ViewportClient->IsPerspective())
		{
			for (auto* Client : GEditor->GetLevelViewportClients())
			{
				if (Client != nullptr && Client->IsPerspective())
				{ ViewportClient = Client; break; }
			}
		}
		if (ViewportClient != nullptr)
		{
			_ViewYawDegrees = ViewportClient->GetViewRotation().Yaw;
			_ViewYawValid = true;
			GotView = true;
		}
	}
#endif

	// Possessed-play path: the local player's CURRENT controller (not GetFirstPlayerController, which
	// can return a stale PC).
	if (NOT GotView)
	{
		auto* ViewPC = static_cast<APlayerController*>(nullptr);
		if (GEngine != nullptr)
		{
			if (auto* LocalPlayer = GEngine->GetFirstGamePlayer(InWorld))
			{ ViewPC = LocalPlayer->GetPlayerController(InWorld); }
		}
		if (ViewPC == nullptr)
		{ ViewPC = InWorld->GetFirstPlayerController(); }

		if (ViewPC != nullptr)
		{
			auto ViewLocation = FVector::ZeroVector;
			auto ViewRotation = FRotator::ZeroRotator;
			ViewPC->GetPlayerViewPoint(ViewLocation, ViewRotation);
			_ViewYawDegrees = ViewRotation.Yaw;
			_ViewYawValid = true;
		}
	}

	// Sample navmesh state once per tick.
	{
		auto* NavSys = UNavigationSystemV1::GetCurrent(InWorld);
		_NavmeshStatus._NavSystemPresent = (NavSys != nullptr);

		if (NavSys != nullptr)
		{
			auto* NavData = Cast<ARecastNavMesh>(NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate));
			if (NavData != nullptr)
			{
				_NavmeshStatus._NavDataClassName = NavData->GetClass()->GetName();
				_NavmeshStatus._DefaultFilterValid = NavData->GetDefaultQueryFilter().IsValid();
				_NavmeshStatus._SupportedAgents = 1; // Default-nav-data path = 1 supported agent

				const auto NavBounds = NavData->GetBounds();
				if (NavBounds.IsValid != 0)
				{
					_NavmeshStatus._NavBoundsValid = true;
					_NavmeshStatus._NavBoundsMin = NavBounds.Min;
					_NavmeshStatus._NavBoundsMax = NavBounds.Max;
				}

				// Refresh the walkable-triangle soup on a throttle — the geometry rarely changes and a
				// full all-tiles pull every frame would be wasteful at scale.
				const auto Now = FPlatformTime::Seconds();
				if (_NavGeomLastPullTime < 0.0 || (Now - _NavGeomLastPullTime) > 1.0)
				{
					_NavGeomLastPullTime = Now;
					_NavTriVerts.Reset();

					auto Geom = FRecastDebugGeometry{};
					NavData->GetDebugGeometryForTile(Geom, FNavTileRef{}); // invalid TileRef = gather all tiles

					for (auto Area = 0; Area < RECAST_MAX_AREAS; ++Area)
					{
						const auto& Indices = Geom.AreaIndices[Area];
						_NavTriVerts.Reserve(_NavTriVerts.Num() + Indices.Num());
						for (const auto Idx : Indices)
						{
							if (Geom.MeshVerts.IsValidIndex(Idx))
							{ _NavTriVerts.Add(Geom.MeshVerts[Idx]); }
						}
					}
				}
			}
			else
			{
				_NavmeshStatus._NavDataClassName = TEXT("(no NavData)");
				_NavmeshStatus._DefaultFilterValid = false;
				_NavTriVerts.Reset();
				_NavGeomLastPullTime = -1.0;
			}
		}

		_NavmeshStatus._Sampled = true;
	}

	auto TransientEntity = UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(InWorld);
	if (NOT ck::IsValid(TransientEntity))
	{ return; }

	TransientEntity.View<ck::FFragment_CrowdAgent_Params>().ForEach(
		[this, &TransientEntity](FCk_Entity InEntity, const ck::FFragment_CrowdAgent_Params&)
		{
			auto Handle = ck::MakeHandle(InEntity, TransientEntity);
			SampleAgent(Handle);
		});

	// Note: per-tick Collect does NOT clear _HealthCheckRun fields — they're sticky
	// across ticks until the user explicitly re-runs the probe. The early
	// `_NavmeshStatus = FCkCrowdDebugger_NavmeshStatus{}` reset above is overridden
	// by re-copying the prior health-check state below if it was previously run.
	// (See Run_HealthCheckProbe — it sets the fields to non-default values; subsequent
	// Collect() calls preserve them by writing only the non-health-check fields.)
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkCrowdDebugger_DataCollector::
	Run_HealthCheckProbe(UWorld* InWorld)
	-> void
{
	_NavmeshStatus._HealthCheckRun = true;
	_NavmeshStatus._HealthCheckTimestamp = FPlatformTime::Seconds();

	auto FailEarly = [this](const FString& InReason)
	{
		_NavmeshStatus._HealthCheckPassed = false;
		_NavmeshStatus._HealthCheckFailReason = InReason;
		_NavmeshStatus._HealthCheckDurationMs = 0.0f;
		_NavmeshStatus._HealthCheckWaypoints = 0;
	};

	if (NOT IsValid(InWorld))           { FailEarly(TEXT("NoWorld"));     return; }
	if (NOT InWorld->HasBegunPlay())    { FailEarly(TEXT("WorldNotPlaying")); return; }

	auto* NavSys = UNavigationSystemV1::GetCurrent(InWorld);
	if (NavSys == nullptr)              { FailEarly(TEXT("NoNavSystem")); return; }

	auto* NavData = Cast<ARecastNavMesh>(NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate));
	if (NavData == nullptr)             { FailEarly(TEXT("NoNavData"));   return; }

	const auto Start = FVector::ZeroVector;
	const auto End   = FVector{200.0, 0.0, 0.0};

	auto Result = FCk_Nav_PathResult{};
	const auto bSucceeded = FCk_Nav_Algorithm::FindPathSync(
		*NavSys, *NavData, Start, End,
		/*allowPartial*/ true,
		/*projectionHalfExtent*/ 500.0f,
		/*projectionVerticalHalfExtent*/ -1.0f,
		/*agentRadiusForFirstSkip*/ 0.0f,
		Result);

	_NavmeshStatus._HealthCheckPassed       = bSucceeded;
	_NavmeshStatus._HealthCheckDurationMs   = Result.Get_Diagnostics().Get_LastQueryDurationMs();
	_NavmeshStatus._HealthCheckWaypoints    = Result.Get_Waypoints().Num();

	if (bSucceeded)
	{
		_NavmeshStatus._HealthCheckFailReason.Empty();
	}
	else
	{
		const auto Reason = Result.Get_Diagnostics().Get_LastFailReason();
		_NavmeshStatus._HealthCheckFailReason = StaticEnum<ECk_Nav_PathFailReason>()
			? StaticEnum<ECk_Nav_PathFailReason>()->GetNameStringByValue(static_cast<int64>(Reason))
			: TEXT("Unknown");
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkCrowdDebugger_DataCollector::
	SampleAgent(FCk_Handle InHandle)
	-> void
{
	if (NOT ck::IsValid(InHandle))
	{ return; }

	auto Snapshot = FCkCrowdDebugger_AgentSnapshot{};
	Snapshot.Handle = InHandle;

	if (InHandle.Has<ck::FFragment_CrowdAgent_Params>())
	{
		const auto& Params = InHandle.Get<ck::FFragment_CrowdAgent_Params>();
		Snapshot.Tags = Params.Get_Tags();
		Snapshot.Radius = Params.Get_Radius();
		Snapshot.Height = Params.Get_Height();
		Snapshot.SeparationRadius = Params.Get_SeparationRadius();
		Snapshot.SeparationWeight = Params.Get_SeparationWeight();
		Snapshot.MaxSpeed = Params.Get_MaxSpeed();
		Snapshot.MaxTurnRate = Params.Get_MaxTurnRate();
		Snapshot.MaxAcceleration = Params.Get_MaxAcceleration();
		Snapshot.ArrivalRadius = Params.Get_ArrivalRadius();
	}

	// Live position (Transform) + steering output (DesiredVelocity) — feeds speed / dist-to-goal.
	if (auto TransformHandle = UCk_Utils_Transform_UE::Cast(InHandle); ck::IsValid(TransformHandle))
	{
		Snapshot.Position = UCk_Utils_Transform_UE::Get_EntityCurrentLocation(TransformHandle);
	}

	if (InHandle.Has<ck::FFragment_CrowdAgent_DesiredVelocity>())
	{
		Snapshot.Velocity = InHandle.Get<ck::FFragment_CrowdAgent_DesiredVelocity>().Get_Velocity();
	}

	// Active-goal state — only meaningful while Walking; a stale goal on an idle agent is ignored.
	Snapshot.IsWalking = InHandle.Has<ck::FTag_CrowdAgent_Walking>();
	if (InHandle.Has<ck::FFragment_CrowdAgent_PathFollow>())
	{
		const auto& PathFollow = InHandle.Get<ck::FFragment_CrowdAgent_PathFollow>();
		Snapshot.ActiveGoal = PathFollow.Get_ActiveGoal();
		Snapshot.ActiveArrivalRadius = PathFollow.Get_ActiveArrivalRadius();
	}
	if (InHandle.Has<ck::FFragment_Nav_PathResult>())
	{
		Snapshot.PlannedPath = InHandle.Get<ck::FFragment_Nav_PathResult>().Get_Waypoints();
	}

	// Gate 3 — neighbor cache + separation force. The fragments may be absent on agents
	// that haven't completed Setup yet (FTag_CrowdAgent_HasProbe gates probe-child spawning),
	// in which case Neighbors stays empty and SeparationForce stays zero.
	if (InHandle.Has<ck::FFragment_CrowdAgent_NeighborCache>())
	{
		const auto& Cache = InHandle.Get<ck::FFragment_CrowdAgent_NeighborCache>();
		const auto& Neighbors = Cache.Get_Neighbors();
		Snapshot.NeighborCount = Neighbors.Num();
		Snapshot.Neighbors.Reserve(Neighbors.Num());
		for (const auto& Nbr : Neighbors)
		{
			auto Info = FCkCrowdDebugger_NeighborInfo{};
			Info.Handle         = Nbr.Get_Handle();
			Info.Distance       = Nbr.Get_Distance();
			Info.RelativeOffset = Nbr.Get_RelativeOffset();
			Snapshot.Neighbors.Add(Info);
		}
	}

	if (InHandle.Has<ck::FFragment_CrowdAgent_SeparationForce>())
	{
		Snapshot.SeparationForce = InHandle.Get<ck::FFragment_CrowdAgent_SeparationForce>().Get_Force();
	}

	if (Snapshot.Tags.Num() > 0)
	{
		const auto FirstTag = Snapshot.Tags.First();
		Snapshot.PrimaryTag = FirstTag.IsValid() ? FirstTag.ToString() : TEXT("—");
	}
	else
	{
		Snapshot.PrimaryTag = TEXT("—");
	}

	// Derive the agent's status. Order matters — a Failed nav result trumps the agent state
	// tag because the failure is the most actionable thing to surface in the UI. Asleep wins
	// over the active states because we want to know it's parked.
	Snapshot.Status = ECkCrowdDebugger_AgentStatus::None;
	if (InHandle.Has<ck::FFragment_Nav_PathResult>()
		&& InHandle.Get<ck::FFragment_Nav_PathResult>().Get_Status() == ECk_Nav_PathStatus::Failed)
	{
		Snapshot.Status = ECkCrowdDebugger_AgentStatus::Failed;
		Snapshot.PathFailReason = InHandle.Get<ck::FFragment_Nav_PathResult>().Get_Diagnostics().Get_LastFailReason();
	}
	else if (InHandle.Has<ck::FTag_CrowdAgent_Asleep>())
	{
		Snapshot.Status = ECkCrowdDebugger_AgentStatus::Asleep;
	}
	else if (InHandle.Has<ck::FTag_CrowdAgent_Walking>())
	{
		Snapshot.Status = ECkCrowdDebugger_AgentStatus::Walking;
	}
	else if (InHandle.Has<ck::FTag_CrowdAgent_PathPending>())
	{
		Snapshot.Status = ECkCrowdDebugger_AgentStatus::Replanning;  // closest existing enum for "in flight"
	}
	else if (InHandle.Has<ck::FTag_CrowdAgent_Idle>())
	{
		Snapshot.Status = ECkCrowdDebugger_AgentStatus::Idle;
	}

	_Agents.Add(MoveTemp(Snapshot));
}

// --------------------------------------------------------------------------------------------------------------------
