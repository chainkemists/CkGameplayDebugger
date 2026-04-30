#include "CkCrowdDebugger/Data/CkCrowdDebugger_DataCollector.h"

#include "CkCore/Macros/CkMacros.h"

#include "CkEcs/Handle/CkHandle.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"

#include "CkCrowd/Agent/CkCrowdAgent_Fragment.h"
#include "CkCrowd/Agent/CkCrowdAgent_Fragment_Data.h"

#include "Engine/World.h"

// --------------------------------------------------------------------------------------------------------------------

auto
	FCkCrowdDebugger_DataCollector::
	Collect(UWorld* InWorld)
	-> void
{
	_Agents.Reset();

	// Per CkDebuggerCommon convention: guard on world validity AND HasBegunPlay
	// (worlds appear in GEngine->GetWorldContexts before BeginPlay).
	if (NOT IsValid(InWorld))
	{ return; }

	if (NOT InWorld->HasBegunPlay())
	{ return; }

	auto TransientEntity = UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(InWorld);
	if (NOT ck::IsValid(TransientEntity))
	{ return; }

	// Walk every entity carrying the CrowdAgent params fragment. Gate 0 only
	// emits Identity-shape snapshots; subsequent gates layer in Position /
	// Velocity / Status / Neighbors as those fragments come online.
	TransientEntity.View<ck::FFragment_CrowdAgent_Params>().ForEach(
		[this, &TransientEntity](FCk_Entity InEntity, const ck::FFragment_CrowdAgent_Params&)
		{
			auto Handle = ck::MakeHandle(InEntity, TransientEntity);
			SampleAgent(Handle);
		});

	// Navmesh status is populated in Gate 1; until then leave _Sampled = false
	// so the panel knows to render the placeholder text.
	_NavmeshStatus = FCkCrowdDebugger_NavmeshStatus{};
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

	// Display string: first tag if present, "—" otherwise.
	if (Snapshot.Tags.Num() > 0)
	{
		const auto FirstTag = Snapshot.Tags.First();
		Snapshot.PrimaryTag = FirstTag.IsValid() ? FirstTag.ToString() : TEXT("—");
	}
	else
	{
		Snapshot.PrimaryTag = TEXT("—");
	}

	// Gate 0: status is None until Gate 2's locomotion lands.
	Snapshot.Status = ECkCrowdDebugger_AgentStatus::None;

	_Agents.Add(MoveTemp(Snapshot));
}

// --------------------------------------------------------------------------------------------------------------------
