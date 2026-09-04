#include "CkCrowdDebugger/Data/CkCrowdDebugger_DataCollector.h"

#include "CkCore/Macros/CkMacros.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/Handle/CkHandle.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/OwningActor/CkOwningActor_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"

#include "CkDebuggerCommon/Navigation/CkDebug_SelectionSync.h"
#include "CkDebuggerCommon/Utils/CkDebug_NameClean_Utils.h"

#include "CkCrowd/Agent/CkCrowdAgent_Fragment.h"
#include "CkCrowd/Agent/CkCrowdAgent_Fragment_Data.h"
#include "CkCrowd/Agent/CkCrowdAgent_Neighbors_Fragment.h"
#include "CkCrowd/AvoidanceVolume/CkCrowdAvoidanceVolume_Utils.h"

#include "CkEcsExt/Transform/CkTransform_Utils.h"

#include "CkGroundNav/Debug/CkGroundNav_DebugSnapshot.h"
#include "CkGroundNav/Facade/CkGroundNav_WorldFieldRegistry.h"
#include "CkGroundNav/Field/CkGroundNav_Field.h"
#include "CkGroundNav/Shadow/CkGroundNav_Shadow_Fragment.h"
#include "CkGroundNav/Volume/CkGroundNavVolume_Utils.h"

#include "CkNavigation/Nav/CkNav_Algorithm.h"
#include "CkNavigation/Nav/CkNav_Fragment.h"
#include "CkNavigation/Nav/CkNav_Fragment_Data.h"
#include "CkNavigation/NavSurface/CkNavSurface_Utils.h"
#include "CkNavigation/NavSurface/Recast/CkNavSurface_RecastAdapter.h"

#include "CkPathNetwork/Actor/CkPathNetwork_Actor.h"
#include "CkPathNetwork/Network/CkPathNetwork_Fragment.h"
#include "CkPathNetwork/Network/CkPathNetwork_Utils.h"
#include "CkQueue/Queue/CkQueue_Utils.h"

#include "Engine/Engine.h"
#include "EngineGlobals.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "HAL/IConsoleManager.h"

#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Class.h"

// --------------------------------------------------------------------------------------------------------------------

namespace
{
	// The window's copy of a field draws PLATE outlines and BOUNDARY runs and nothing per-cell, so the
	// per-cell list is capped hard. The counts every panel reads stay exact whatever the cap
	// (Make_DebugSnapshotFromField caps the list only), and an uncapped field is tens of thousands of
	// cell values copied on every rebake for a view that never draws one.
	constexpr auto GroundNavSnapshotMaxCells = 4096;

	auto EnumName(ECk_PathNetwork_RouteFailReason InReason) -> FString
	{
		const auto* Enum = StaticEnum<ECk_PathNetwork_RouteFailReason>();
		return Enum != nullptr
			? Enum->GetNameStringByValue(static_cast<int64>(InReason))
			: TEXT("Unknown");
	}

	auto EnumName(ECk_Nav_PathStatus InStatus) -> FString
	{
		const auto* Enum = StaticEnum<ECk_Nav_PathStatus>();
		return Enum != nullptr
			? Enum->GetNameStringByValue(static_cast<int64>(InStatus))
			: TEXT("Unknown");
	}

	auto EnumName(ECk_Nav_PathFailReason InReason) -> FString
	{
		const auto* Enum = StaticEnum<ECk_Nav_PathFailReason>();
		return Enum != nullptr
			? Enum->GetNameStringByValue(static_cast<int64>(InReason))
			: TEXT("Unknown");
	}

	auto MakePathTroubleSummary(const FCkCrowdDebugger_AgentSnapshot& InSnapshot) -> FString
	{
		if (NOT InSnapshot.HasPathTroubleEvent)
		{ return {}; }

		if (InSnapshot.HadPathNetworkFailure)
		{
			return FString::Printf(
				TEXT("SIDEWALK: %s -> UNREAL NAV: %s"),
				*EnumName(InSnapshot.PathNetworkFailReason),
				*EnumName(InSnapshot.TroubleNavigationStatus));
		}

		const auto Status = EnumName(InSnapshot.TroubleNavigationStatus);
		return InSnapshot.TroubleNavigationFailReason == ECk_Nav_PathFailReason::None
			? FString::Printf(TEXT("UNREAL NAV: %s"), *Status)
			: FString::Printf(
				TEXT("UNREAL NAV: %s (%s)"),
				*Status,
				*EnumName(InSnapshot.TroubleNavigationFailReason));
	}
}

// --------------------------------------------------------------------------------------------------------------------

auto FCkCrowdDebugger_DataCollector::Reset_ForWorldChange() -> void
{
	_Agents.Reset();
	_NavmeshStatus = FCkCrowdDebugger_NavmeshStatus{};
	_NavTriVerts.Reset();
	++_NavGeometryRevision;
	_PathNetworkRibbons.Reset();
	_Queues.Reset();
	_AvoidanceVolumes.Reset();
	_NavGeomLastPullTime = -1.0;
	_NavGeomSignature = 0;
	_PlayerPawnEntity = FCk_Handle{};

	// The copy and the diagnostics name ground in the world that is going away, so they go with it. The
	// snapshot cache is left alone: its key carries the world's name, so the first key built in the new
	// world differs and the cache replaces itself on its own terms.
	_GroundNavField = FCkCrowdDebugger_GroundNavField{};
	_GroundNavFieldSampled = false;
	_GroundNavRevision = 0;
	_ShadowParity = FCkCrowdDebugger_ShadowParity{};
}

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkCrowdDebugger_DataCollector::
	Collect(UWorld* InWorld, const FCk_Handle& InSelectedAgent)
	-> void
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CkCrowdDbg_Collect);
	_Agents.Reset();
	_PathNetworkRibbons.Reset();
	_Queues.Reset();
	_AvoidanceVolumes.Reset();

	// Reset only the per-tick-sampled fields. Health-check fields are sticky across
	// ticks (set explicitly by Run_HealthCheckProbe; not derived from the live world).
	// The neutral values are sampled too, so a tick that reaches no world leaves behind the
	// provider it read last tick rather than the one answering now.
	_NavmeshStatus._Sampled = false;
	_NavmeshStatus._Provider = ECk_NavSurface_Provider::Recast;
	_NavmeshStatus._ProviderHealth = ECk_NavSurface_ProviderHealth::NoData;
	_NavmeshStatus._SurfaceRevision = 0;
	_NavmeshStatus._ProviderIsRecast = false;
	_NavmeshStatus._NavSystemPresent = false;
	_NavmeshStatus._NavDataClassName.Empty();
	_NavmeshStatus._DefaultFilterValid = false;
	_NavmeshStatus._SupportedAgents = 0;
	_NavmeshStatus._LastRegenTimestamp = -1.0;
	_NavmeshStatus._NavBoundsValid = false;

	// Only the two flags, for the same reason: a tick that reaches no world, or a world that stopped
	// publishing a field, must read as NOT SAMPLED rather than silently keep showing the last bake as
	// current. The values behind them stay, so a paused window still has something to draw.
	_GroundNavFieldSampled = false;
	_ShadowParity._Sampled = false;

	// Per CkDebuggerCommon convention: guard on world validity AND HasBegunPlay
	// (worlds appear in GEngine->GetWorldContexts before BeginPlay).
	if (NOT IsValid(InWorld))
	{ return; }

	if (NOT InWorld->HasBegunPlay())
	{ return; }

	// Local player's CURRENT controller (not GetFirstPlayerController, which can return a stale PC).
	// Resolved solely to reach the pawn's entity for PlayerProxy row tagging.
	auto* ViewPC = static_cast<APlayerController*>(nullptr);
	{
		if (GEngine != nullptr)
		{
			if (auto* LocalPlayer = GEngine->GetFirstGamePlayer(InWorld))
			{ ViewPC = LocalPlayer->GetPlayerController(InWorld); }
		}
		if (ViewPC == nullptr)
		{ ViewPC = InWorld->GetFirstPlayerController(); }
	}

	// The pawn keeps existing while ejected, so the proxy row stays tagged either way.
	_PlayerPawnEntity = FCk_Handle{};
	if (ViewPC != nullptr)
	{
		if (auto* Pawn = ViewPC->GetPawn().Get(); IsValid(Pawn))
		{ _PlayerPawnEntity = UCk_Utils_OwningActor_UE::TryGet_ActorEntityHandle(Pawn); }
	}

	// Sample navmesh state once per tick. The provider-neutral values are read first and for every
	// world: whichever provider answers, the panel can name it, say how it is, and say which revision
	// of the surface these values came from. Everything after that is one provider's own detail, and
	// is pulled only while that provider is the one answering.
	{
		_NavmeshStatus._Provider         = UCk_Utils_NavSurface_UE::Get_Provider(InWorld);
		_NavmeshStatus._ProviderHealth   = UCk_Utils_NavSurface_UE::Get_ProviderHealth(InWorld);
		_NavmeshStatus._SurfaceRevision  = UCk_Utils_NavSurface_UE::Get_SurfaceRevision(InWorld);
		_NavmeshStatus._ProviderIsRecast = _NavmeshStatus._Provider == ECk_NavSurface_Provider::Recast;

		const auto NavBounds = UCk_Utils_NavSurface_UE::Get_SurfaceBounds(InWorld);
		if (NavBounds.IsValid != 0)
		{
			_NavmeshStatus._NavBoundsValid = true;
			_NavmeshStatus._NavBoundsMin = NavBounds.Min;
			_NavmeshStatus._NavBoundsMax = NavBounds.Max;
		}

		// Whenever no Recast tiles can be read, whatever the last Recast world left behind stops
		// standing for anything: drop it and bump the revision so the retained scene stops drawing it.
		const auto Drop_NavGeometry = [this]()
		{
			const auto HadNavGeometryState = NOT _NavTriVerts.IsEmpty() || _NavGeomLastPullTime >= 0.0;
			_NavTriVerts.Reset();
			_NavGeomLastPullTime = -1.0;
			if (HadNavGeometryState)
			{ ++_NavGeometryRevision; }
		};

		if (NOT _NavmeshStatus._ProviderIsRecast)
		{
			Drop_NavGeometry();
		}
		else
		{
			auto* NavSys = ck::nav_surface_recast::TryGet_NavSystem(InWorld);
			_NavmeshStatus._NavSystemPresent = (NavSys != nullptr);

			if (NavSys != nullptr)
			{
				auto* NavData = ck::nav_surface_recast::TryGet_NavData(InWorld);
				if (NavData != nullptr)
				{
					_NavmeshStatus._NavDataClassName = NavData->GetClass()->GetName();
					_NavmeshStatus._DefaultFilterValid = NavData->GetDefaultQueryFilter().IsValid();
					_NavmeshStatus._SupportedAgents = 1; // Default-nav-data path = 1 supported agent

					// Refresh the walkable-triangle soup on a throttle — the geometry rarely changes and a
					// full all-tiles pull every frame would be wasteful at scale.
					const auto Now = FPlatformTime::Seconds();
					if (_NavGeomLastPullTime < 0.0 || (Now - _NavGeomLastPullTime) > 1.0)
					{
						// Prime suspect: gathers EVERY navmesh tile, then unconditionally bumps the
						// revision so the whole mesh is re-copied, rebuilt and re-uploaded downstream.
						TRACE_CPUPROFILER_EVENT_SCOPE(CkCrowdDbg_NavmeshGeometryPull);
						_NavGeomLastPullTime = Now;

						auto Geom = FRecastDebugGeometry{};
						NavData->GetDebugGeometryForTile(Geom, FNavTileRef{}); // invalid TileRef = gather all tiles

						auto PulledVerts = TArray<FVector>{};
						for (auto Area = 0; Area < RECAST_MAX_AREAS; ++Area)
						{
							const auto& Indices = Geom.AreaIndices[Area];
							PulledVerts.Reserve(PulledVerts.Num() + Indices.Num());
							for (const auto Idx : Indices)
							{
								if (Geom.MeshVerts.IsValidIndex(Idx))
								{ PulledVerts.Add(Geom.MeshVerts[Idx]); }
							}
						}

						// Bump the revision only when the geometry ACTUALLY changed. This used to bump
						// on every pull, i.e. once a second forever, and the revision is what makes the
						// scene adapter rebuild the whole navmesh static mesh — a ~245ms job on a real
						// map. A navmesh that never rebakes must never pay that more than once.
						auto Signature = GetTypeHash(PulledVerts.Num());
						for (const auto& Vert : PulledVerts)
						{ Signature = HashCombineFast(Signature, GetTypeHash(Vert)); }

						if (Signature != _NavGeomSignature)
						{
							_NavGeomSignature = Signature;
							_NavTriVerts = MoveTemp(PulledVerts);
							++_NavGeometryRevision;
						}
					}
				}
				else
				{
					_NavmeshStatus._NavDataClassName = TEXT("(no NavData)");
					_NavmeshStatus._DefaultFilterValid = false;
					Drop_NavGeometry();
				}
			}
			else
			{
				Drop_NavGeometry();
			}
		}

		_NavmeshStatus._Sampled = true;
	}

	// The GroundNav field the viewport draws, copied through a snapshot cache of this collector's own.
	//
	// Only while GroundNav is the provider actually serving this world. A Recast world has no field to
	// show, and a copy left standing from a world that had one would put ground on screen that nothing
	// is planning over.
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(CkCrowdDbg_GroundNavField);

		if (_NavmeshStatus._Provider == ECk_NavSurface_Provider::GroundNav)
		{
			// Which volume's field: the one nearest the SELECTION when there is a selection, because the
			// field worth looking at is the one under the agent being looked at; otherwise the first that
			// has published, so a world with a single field draws it without anybody selecting anything.
			// A volume that has published nothing is skipped either way - its surface bounds are empty by
			// contract, which is also how "has published" is read here.
			auto SelectionLocation = FVector::ZeroVector;
			auto HasSelection = false;

			if (const auto SelectedTransform = UCk_Utils_Transform_UE::Cast(InSelectedAgent);
				ck::IsValid(SelectedTransform))
			{
				SelectionLocation = UCk_Utils_Transform_UE::Get_EntityCurrentLocation(SelectedTransform);
				HasSelection = true;
			}

			auto ChosenVolume = FCk_Handle{};
			auto ChosenLocation = FVector::ZeroVector;
			auto ChosenDistanceSq = TNumericLimits<double>::Max();

			for (auto& VolumeEntity : ck::groundnav::world_fields::Get_VolumeEntities(InWorld))
			{
				const auto Volume = UCk_Utils_GroundNavVolume_UE::Cast(VolumeEntity);

				if (ck::Is_NOT_Valid(Volume))
				{ continue; }

				const auto Bounds = UCk_Utils_GroundNavVolume_UE::Get_SurfaceBounds(Volume);

				if (Bounds.IsValid == 0)
				{ continue; }

				const auto Centre = Bounds.GetCenter();

				if (NOT HasSelection)
				{
					ChosenVolume = VolumeEntity;
					ChosenLocation = Centre;
					break;
				}

				const auto DistanceSq = FVector::DistSquared(Centre, SelectionLocation);

				if (DistanceSq >= ChosenDistanceSq)
				{ continue; }

				ChosenVolume = VolumeEntity;
				ChosenLocation = Centre;
				ChosenDistanceSq = DistanceSq;
			}

			// Resolved through the registry rather than off the volume handle: that read is the neutral
			// one, hands back its own reference to an immutable field, and answers the same field a query
			// at that location would be planned over.
			const auto Field = ck::IsValid(ChosenVolume)
				? ck::groundnav::world_fields::TryGet_Field(InWorld, ChosenLocation)
				: ck::groundnav::FCk_GroundNav_FieldPtr{};

			if (Field.IsValid())
			{
				const auto VolumeEntity = ChosenVolume.Get_Entity();

				// The newest tile's epoch, read the way the capture reads it: any tile that moved moves
				// this, which is the whole of what makes a rebuilt field a different key.
				auto NewestTileEpoch = int64{0};

				for (const auto& Tile : Field->_Tiles)
				{ NewestTileEpoch = FMath::Max(NewestTileEpoch, Tile._Epoch._Value); }

				auto Key = ck::groundnav::FCk_GroundNav_DebugSnapshotCacheKey{};
				Key._WorldName = InWorld->GetFName();
				Key._VolumeEntityNumber = static_cast<int32>(VolumeEntity.Get_EntityNumber());
				Key._VolumeEntityVersion = static_cast<int32>(VolumeEntity.Get_VersionNumber());
				Key._NewestTileEpoch = NewestTileEpoch;
				Key._SurfaceRevision = UCk_Utils_NavSurface_UE::Get_SurfaceRevision(InWorld);

				// The key is checked FIRST and the snapshot is built only when it moved, which is what makes
				// a static field cost a five-member comparison a tick instead of a whole flatten.
				if (NOT _GroundNavCache.Get_Key().Get_IsEqual(Key))
				{
					_GroundNavCache.Replace(Key,
						ck::groundnav::Make_DebugSnapshotFromField(*Field, GroundNavSnapshotMaxCells));
				}

				_GroundNavField._Snapshot = _GroundNavCache.TryGet_Current(Key);
				_GroundNavField._Key = Key;
				_GroundNavField._Source = ECkCrowdDebugger_SnapshotSource::LivePie;
				_GroundNavRevision = Get_GroundNavRevisionFromKey(Key);
				_GroundNavFieldSampled = true;
			}
		}
	}

	// Retain only value snapshots across debugger refreshes and world teardown.
	auto AppendPathNetworkRibbons = [this](const TArray<FCk_PathNetwork_Ribbon>& InRibbons)
	{
		for (const auto& Ribbon : InRibbons)
		{
			const auto& RibbonPoints = Ribbon.Get_Points();
			if (RibbonPoints.Num() < 2)
			{ continue; }

			auto Snapshot = FCkCrowdDebugger_PathNetworkRibbonSnapshot{};
			Snapshot.Points.Reserve(RibbonPoints.Num());
			Snapshot.HalfWidths.Reserve(RibbonPoints.Num());
			for (const auto& Point : RibbonPoints)
			{
				Snapshot.Points.Add(Point.Get_Location());
				Snapshot.HalfWidths.Add(Point.Get_HalfWidth());
			}
			_PathNetworkRibbons.Add(MoveTemp(Snapshot));
		}
	};

	auto TransientEntity = UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(InWorld);
	if (ck::IsValid(TransientEntity))
	{
		// Shadow-parity diagnostics live on the world's transient entity, one bucket set per world, so
		// this is where they are read. Copied WHOLE - a name, a map of fixed-size counters and a list of
		// ids, none of which points back at the world - so the panel goes on rendering the last run after
		// PIE stops. Sampled only when the fragment is actually there: an empty copy and a world that
		// never ran a shadow comparison are different answers, and the flag is what tells them apart.
		if (TransientEntity.Has<ck::FFragment_GroundNav_ShadowDiagnostics>())
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(CkCrowdDbg_ShadowParity);

			_ShadowParity._Diagnostics = TransientEntity.Get<ck::FFragment_GroundNav_ShadowDiagnostics>();
			_ShadowParity._Sampled = true;
		}

		TRACE_CPUPROFILER_EVENT_SCOPE(CkCrowdDbg_PathNetworkRibbons_Ecs);
		// Runtime-created networks, including the Path Network gym, have no actor. Authority-side
		// actor networks are bridged into the same ECS representation at BeginPlay.
		const auto WorldRibbons = UCk_Utils_PathNetwork_UE::Get_AllRibbonsInWorld(TransientEntity);
		AppendPathNetworkRibbons(WorldRibbons);

		static const auto* TraceCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("ck.CrowdDebugger.PathNetworkTrace"));
		if (TraceCVar != nullptr && TraceCVar->GetInt() != 0)
		{
			auto QueryEntityCount = 0;
			TransientEntity.View<
				ck::FFragment_PathNetwork_Params,
				ck::FFragment_PathNetwork_Graph,
				CK_IGNORE_PENDING_KILL>().ForEach(
				[&QueryEntityCount](
					FCk_Entity,
					const ck::FFragment_PathNetwork_Params&,
					const ck::FFragment_PathNetwork_Graph&)
				{
					++QueryEntityCount;
				});

			auto TotalPointCount = 0;
			for (const auto& Ribbon : WorldRibbons)
			{ TotalPointCount += Ribbon.Get_Points().Num(); }

			auto FirstPosition = FVector::ZeroVector;
			auto LastPosition = FVector::ZeroVector;
			auto FirstHalfWidth = 0.0f;
			auto LastHalfWidth = 0.0f;
			if (WorldRibbons.Num() > 0 && WorldRibbons[0].Get_Points().Num() > 0)
			{
				const auto& Points = WorldRibbons[0].Get_Points();
				FirstPosition = Points[0].Get_Location();
				LastPosition = Points.Last().Get_Location();
				FirstHalfWidth = Points[0].Get_HalfWidth();
				LastHalfWidth = Points.Last().Get_HalfWidth();
			}

			UE_LOG(
				LogTemp,
				Display,
				TEXT("CkCrowdDebugger.PathNetworkTrace stage=collector frame=%llu world=%s world_ptr=%p transient=%s query_entities=%d query_ribbons=%d query_points=%d first=%s last=%s first_half_width=%.3f last_half_width=%.3f collector_after_ecs=%d"),
				static_cast<unsigned long long>(GFrameCounter),
				*InWorld->GetName(),
				InWorld,
				*TransientEntity.ToString(),
				QueryEntityCount,
				WorldRibbons.Num(),
				TotalPointCount,
				*FirstPosition.ToCompactString(),
				*LastPosition.ToCompactString(),
				FirstHalfWidth,
				LastHalfWidth,
				_PathNetworkRibbons.Num());
		}
	}

	// The path-network actor deliberately constructs ECS state on authority only. Preserve the
	// previous actor-authored debugger view on clients without duplicating authority-side ribbons.
	{
	TRACE_CPUPROFILER_EVENT_SCOPE(CkCrowdDbg_PathNetworkRibbons_ActorIterator);
	for (TActorIterator<ACk_PathNetwork_UE> It(InWorld); It; ++It)
	{
		const auto* NetworkActor = *It;
		if (NOT IsValid(NetworkActor)
			|| NetworkActor->IsActorBeingDestroyed()
			|| NetworkActor->HasAuthority())
		{ continue; }

		AppendPathNetworkRibbons(NetworkActor->Get_WorldRibbons());
	}
	}

	if (NOT ck::IsValid(TransientEntity))
	{ return; }

	{
	TRACE_CPUPROFILER_EVENT_SCOPE(CkCrowdDbg_SampleAllAgents);
	TransientEntity.View<ck::FFragment_CrowdAgent_Params>().ForEach(
		[this, &TransientEntity, &InSelectedAgent](FCk_Entity InEntity, const ck::FFragment_CrowdAgent_Params&)
		{
			auto Handle = ck::MakeHandle(InEntity, TransientEntity);
			SampleAgent(Handle, InSelectedAgent);
		});
	}

	// Queue is collected as a detached runtime DTO then projected into debugger-local values.  The
	// resulting Crowd snapshot contains no queue fragment, handle or registry reference, and stays
	// safe after PIE teardown exactly like existing path/network rows.
	{
	TRACE_CPUPROFILER_EVENT_SCOPE(CkCrowdDbg_QueueProjection);
	for (const auto& Queue : UCk_Utils_Queue_UE::Get_DebugSnapshots(TransientEntity))
	{
		auto QueueCopy = FCkCrowdDebugger_QueueSnapshot{};
		QueueCopy.Identity = static_cast<uint64>(Queue.Get_QueueIdentity());
		QueueCopy.Revision = static_cast<uint64>(Queue.Get_Revision());
		QueueCopy.DebugName = Queue.Get_QueueDebugName().ToString();
		QueueCopy.Category = Queue.Get_Category().ToString();
		QueueCopy.State = StaticEnum<ECk_Queue_State>()->GetNameStringByValue(static_cast<int64>(Queue.Get_State()));
		QueueCopy.OwnerTargetLocation = Queue.Get_OwnerWorldTransform().GetLocation();
		QueueCopy.OwnerTargetForward = Queue.Get_OwnerWorldTransform().GetRotation().GetForwardVector();
		for (const auto& Member : Queue.Get_Members())
		{
			const auto AgentIdentity = static_cast<uint64>(Member.Get_MoverIdentity());
			const auto HasReservation = Member.Get_Rank() != INDEX_NONE;
			QueueCopy.Members.Add({AgentIdentity, Member.Get_Rank(), Member.Get_TargetWorldTransform().GetLocation(),
				Member.Get_TargetWorldTransform().GetRotation().GetForwardVector(), HasReservation});
			for (auto& Agent : _Agents)
			{
				if (static_cast<uint64>(Agent.Handle.Get_Entity().Get_ID()) != AgentIdentity)
				{ continue; }
				Agent.QueueDebugName = QueueCopy.DebugName;
				Agent.QueueCategory = QueueCopy.Category;
				Agent.QueueState = QueueCopy.State;
				Agent.QueueRank = Member.Get_Rank();
				break;
			}
		}
		_Queues.Add(MoveTemp(QueueCopy));
	}
	}

	// CkCrowd owns this public detached-debug surface. Project it once here so all UI and retained
	// preview layers consume values only, even after the producer world/registry has gone away.
	{
	TRACE_CPUPROFILER_EVENT_SCOPE(CkCrowdDbg_AvoidanceVolumeProjection);
	for (const auto& Volume : UCk_Utils_CrowdAvoidanceVolume_UE::Get_DebugSnapshots(TransientEntity))
	{
		auto Copy = FCkCrowdDebugger_AvoidanceVolumeSnapshot{};
		Copy.Identity = static_cast<uint64>(Volume.Get_VolumeIdentity());
		Copy.Revision = Volume.Get_ConfirmationSerial();
		Copy.DebugName = Volume.Get_VolumeDebugName().ToString();
		Copy.YawWorldTransform = Volume.Get_YawWorldTransform();
		Copy.PhysicalWorldHalfExtents = Volume.Get_PhysicalWorldHalfExtents();
		Copy.InfluenceWorldHalfExtents = Volume.Get_InfluenceWorldHalfExtents();
		Copy.PaintedWorldHalfExtents = Volume.Get_PaintedWorldHalfExtents();
		Copy.SecondsSincePaint = Volume.Get_SecondsSincePaint();
		Copy.NavigationRevisionAtUnregister = Volume.Get_NavigationRevisionAtUnregister();
		Copy.HasValidGeometry = Volume.Get_HasValidGeometry();
		switch (Volume.Get_TraversalPolicy())
		{
		case ECk_CrowdAvoidanceVolume_TraversalPolicy::HardExclude:
			Copy.TraversalPolicy = ECkCrowdDebugger_AvoidanceVolumeTraversalPolicy::HardExclude;
			break;
		case ECk_CrowdAvoidanceVolume_TraversalPolicy::CostOnly:
			Copy.TraversalPolicy = ECkCrowdDebugger_AvoidanceVolumeTraversalPolicy::CostOnly;
			break;
		case ECk_CrowdAvoidanceVolume_TraversalPolicy::AvoidIfPossible:
		default:
			Copy.TraversalPolicy = ECkCrowdDebugger_AvoidanceVolumeTraversalPolicy::AvoidIfPossible;
			break;
		}
		switch (Volume.Get_State())
		{
		case ECk_CrowdAvoidanceVolume_DebugState::Confirmed:
			Copy.State = ECkCrowdDebugger_AvoidanceVolumeState::Confirmed;
			break;
		case ECk_CrowdAvoidanceVolume_DebugState::Invalid:
			Copy.State = ECkCrowdDebugger_AvoidanceVolumeState::Invalid;
			break;
		case ECk_CrowdAvoidanceVolume_DebugState::Retiring:
			Copy.State = ECkCrowdDebugger_AvoidanceVolumeState::Retiring;
			break;
		case ECk_CrowdAvoidanceVolume_DebugState::PendingSetup:
		default:
			Copy.State = ECkCrowdDebugger_AvoidanceVolumeState::Pending;
			break;
		}
		_AvoidanceVolumes.Add(MoveTemp(Copy));
	}
	}

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

	auto* NavSys = ck::nav_surface_recast::TryGet_NavSystem(InWorld);
	if (NavSys == nullptr)              { FailEarly(TEXT("NoNavSystem")); return; }

	auto* NavData = ck::nav_surface_recast::TryGet_NavData(InWorld);
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
	SampleAgent(FCk_Handle InHandle, const FCk_Handle& InSelectedAgent)
	-> void
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CkCrowdDbg_SampleAgent);
	if (NOT ck::IsValid(InHandle))
	{ return; }

	auto Snapshot = FCkCrowdDebugger_AgentSnapshot{};
	Snapshot.Handle = InHandle;

	// Immediate lifetime owner/transport anchor — this is not guaranteed to be the conceptual NPC.
	// Consumers that need a selectable gameplay target must resolve the full ownership chain.
	Snapshot.OwnerHandle = UCk_Utils_EntityLifetime_UE::Get_LifetimeOwner(InHandle);
	if (ck::IsValid(Snapshot.OwnerHandle))
	{
		Snapshot.OwnerName = ck::DebugNameClean::Get_CleanName(
			UCk_Utils_Handle_UE::Get_DebugName(Snapshot.OwnerHandle).ToString());
	}

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
		const auto& Waypoints = InHandle.Get<ck::FFragment_Nav_PathResult>().Get_Waypoints();
		Snapshot.PlannedPathPointCount = Waypoints.Num();

		// Only the selected agent's polyline is ever drawn, and only the count is ever displayed
		// for the rest. Copying every agent's waypoints here fed three deep copies per frame.
		if (ck::IsValid(InSelectedAgent) && InHandle == InSelectedAgent)
		{ Snapshot.PlannedPath = Waypoints; }
	}
	if (InHandle.Has<ck::FFragment_CrowdAgent_PathFollow>())
	{
		Snapshot.ActiveProvider = InHandle.Get<ck::FFragment_CrowdAgent_PathFollow>().Get_ActiveProvider();
	}
	if (InHandle.Has<ck::FFragment_CrowdAgent_PathTrouble>())
	{
		const auto& Trouble = InHandle.Get<ck::FFragment_CrowdAgent_PathTrouble>();
		Snapshot.HasPathTroubleEvent = Trouble.Get_HasEvent();
		Snapshot.HadPathNetworkFailure = Trouble.Get_HadPathNetworkFailure();
		Snapshot.UsedNavigationFallback = Trouble.Get_UsedNavigationFallback();
		Snapshot.PathNetworkFailReason = Trouble.Get_PathNetworkFailReason();
		Snapshot.TroubleNavigationStatus = Trouble.Get_NavigationStatus();
		Snapshot.TroubleNavigationFailReason = Trouble.Get_NavigationFailReason();
		Snapshot.PathTroubleAgentPosition = Trouble.Get_AgentLocation();
		Snapshot.PathTroubleGoal = Trouble.Get_GoalLocation();
		Snapshot.PathTroubleEventTimeSeconds = Trouble.Get_EventTimeSeconds();
		Snapshot.PathTroubleSummary = MakePathTroubleSummary(Snapshot);
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

	// The player's own crowd agent (if the possessed pawn is ECS-bridged and carries
	// one) reads as PlayerProxy, not as an AI behavior state.
	if (ck::IsValid(_PlayerPawnEntity) &&
		ck::DebugSelectionSync::Is_SameLineage(InHandle, _PlayerPawnEntity))
	{
		Snapshot.Status = ECkCrowdDebugger_AgentStatus::PlayerProxy;
	}

	_Agents.Add(MoveTemp(Snapshot));
}

// --------------------------------------------------------------------------------------------------------------------
