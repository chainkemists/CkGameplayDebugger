#include "CkCrowdDebugger/Data/CkCrowdDebugger_DataCollector.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkEcs/Handle/CkHandle.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"

#include "CkCrowd/Agent/CkCrowdAgent_Fragment.h"
#include "CkCrowd/Agent/CkCrowdAgent_Fragment_Data.h"

#include "Engine/World.h"
#include "NavigationSystem.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavFilters/NavigationQueryFilter.h"

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkCrowdDebugger_DataCollector::
	Collect(UWorld* InWorld)
	-> void
{
	_Agents.Reset();
	_NavmeshStatus = FCkCrowdDebugger_NavmeshStatus{};

	// Per CkDebuggerCommon convention: guard on world validity AND HasBegunPlay
	// (worlds appear in GEngine->GetWorldContexts before BeginPlay).
	if (NOT IsValid(InWorld))
	{ return; }

	if (NOT InWorld->HasBegunPlay())
	{ return; }

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
			}
			else
			{
				_NavmeshStatus._NavDataClassName = TEXT("(no NavData)");
				_NavmeshStatus._DefaultFilterValid = false;
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

	Snapshot.Status = ECkCrowdDebugger_AgentStatus::None;

	_Agents.Add(MoveTemp(Snapshot));
}

// --------------------------------------------------------------------------------------------------------------------
