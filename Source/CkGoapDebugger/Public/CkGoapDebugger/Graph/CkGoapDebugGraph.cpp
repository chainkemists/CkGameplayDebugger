#include "CkGoapDebugGraph.h"
#include "CkGoapDebugNode_Action.h"
#include "CkGoapDebugNode_Goal.h"
#include "CkGoapDebugGraphSchema.h"

// ====================================================================================================================

auto
	UCkGoapDebugGraph::
	UpdateFromGoapInfo(
		const FCkGoapDebugger_GoapInfo& InInfo)
	-> void
{
	const auto NewHash = ComputeTopologyHash(InInfo);

	if (NewHash != _TopologyHash)
	{
		_TopologyHash = NewHash;
		SetSuppressNotifications(true);
		RebuildTopology(InInfo);
		SetSuppressNotifications(false);
		NotifyGraphChanged();
	}
	else
	{
		UpdateRuntimeState(InInfo);
	}
}

// ====================================================================================================================

auto
	UCkGoapDebugGraph::
	RebuildTopology(
		const FCkGoapDebugger_GoapInfo& InInfo)
	-> void
{
	Nodes.Empty();
	_ActionNodes.Empty();
	_GoalNode = nullptr;

	// Build effect→provider map
	TMap<FString, TArray<int32>> EffectProviders;
	for (auto Index = 0; Index < InInfo.Actions.Num(); ++Index)
	{
		for (const auto& [Key, Value] : InInfo.Actions[Index].Effects)
		{
			EffectProviders.FindOrAdd(Key.ToString()).Add(Index);
		}
	}

	// Create action nodes
	for (auto Index = 0; Index < InInfo.Actions.Num(); ++Index)
	{
		auto* ActionNode = NewObject<UCkGoapDebugNode_Action>(this);
		ActionNode->PopulateFromActionInfo(InInfo.Actions[Index], Index);
		ActionNode->CreateNewGuid();
		ActionNode->AllocateDefaultPins();

		const auto PlanIdx = InInfo.PlanActionNames.IndexOfByKey(InInfo.Actions[Index].ClassName);
		ActionNode->UpdatePlanState(PlanIdx != INDEX_NONE, PlanIdx);

		AddNode(ActionNode, false, false);
		_ActionNodes.Add(ActionNode);
	}

	// Create edges: effect→precondition connections
	for (auto TargetIdx = 0; TargetIdx < InInfo.Actions.Num(); ++TargetIdx)
	{
		auto* TargetNode = _ActionNodes[TargetIdx].Get();
		auto* InputPin = TargetNode->FindPin(TEXT("In"), EGPD_Input);
		if (InputPin == nullptr) { continue; }

		for (const auto& [PreKey, PreValue] : InInfo.Actions[TargetIdx].Preconditions)
		{
			const auto KeyStr = PreKey.ToString();
			const auto* Providers = EffectProviders.Find(KeyStr);
			if (Providers == nullptr) { continue; }

			for (const auto SourceIdx : *Providers)
			{
				if (SourceIdx == TargetIdx) { continue; }
				auto* SourceNode = _ActionNodes[SourceIdx].Get();
				auto* OutputPin = SourceNode->FindPin(TEXT("Out"), EGPD_Output);
				if (OutputPin == nullptr) { continue; }

				OutputPin->MakeLinkTo(InputPin);
			}
		}
	}

	// Create goal node for the active goal
	for (const auto& Goal : InInfo.Goals)
	{
		if (Goal.IsActiveGoal)
		{
			_GoalNode = NewObject<UCkGoapDebugNode_Goal>(this);
			_GoalNode->PopulateFromGoalInfo(Goal);
			_GoalNode->CreateNewGuid();
			_GoalNode->AllocateDefaultPins();
			AddNode(_GoalNode, false, false);

			// Connect last plan action to goal
			if (InInfo.PlanActionNames.Num() > 0)
			{
				const auto& LastActionName = InInfo.PlanActionNames.Last();
				for (auto Idx = 0; Idx < _ActionNodes.Num(); ++Idx)
				{
					auto* Node = _ActionNodes[Idx].Get();
					if (Node->Get_ActionName() == LastActionName)
					{
						auto* OutPin = Node->FindPin(TEXT("Out"), EGPD_Output);
						auto* GoalInPin = _GoalNode->FindPin(TEXT("In"), EGPD_Input);
						if (OutPin && GoalInPin) { OutPin->MakeLinkTo(GoalInPin); }
						break;
					}
				}
			}
			break;
		}
	}

	PerformLayout();
}

// ====================================================================================================================

auto
	UCkGoapDebugGraph::
	UpdateRuntimeState(
		const FCkGoapDebugger_GoapInfo& InInfo)
	-> void
{
	for (auto Idx = 0; Idx < _ActionNodes.Num(); ++Idx)
	{
		auto* Node = _ActionNodes[Idx].Get();
		const auto PlanIdx = InInfo.PlanActionNames.IndexOfByKey(Node->Get_ActionName());
		Node->UpdatePlanState(PlanIdx != INDEX_NONE, PlanIdx);
	}

	if (_GoalNode != nullptr)
	{
		for (const auto& Goal : InInfo.Goals)
		{
			if (Goal.ClassName == _GoalNode->Get_GoalName())
			{
				_GoalNode->Set_IsActiveGoal(Goal.IsActiveGoal);
				break;
			}
		}
	}
}

// ====================================================================================================================

auto
	UCkGoapDebugGraph::
	PerformLayout()
	-> void
{
	TMap<FString, TArray<int32>> EffectProviders;
	for (auto Index = 0; Index < _ActionNodes.Num(); ++Index)
	{
		auto* Node = _ActionNodes[Index].Get();
		for (const auto& [Key, Value] : Node->Get_Effects())
		{
			EffectProviders.FindOrAdd(Key.ToString()).Add(Index);
		}
	}

	TMap<int32, int32> LayerMap;
	TSet<int32> Visiting;

	TFunction<int32(int32)> ComputeLayer = [&](int32 Idx) -> int32
	{
		if (const auto* Found = LayerMap.Find(Idx)) { return *Found; }
		if (Visiting.Contains(Idx)) { return 0; }
		Visiting.Add(Idx);

		auto MaxDep = -1;
		auto* Node = _ActionNodes[Idx].Get();
		for (const auto& [PreKey, PreVal] : Node->Get_Preconditions())
		{
			const auto* Providers = EffectProviders.Find(PreKey.ToString());
			if (Providers == nullptr) { continue; }
			for (const auto ProvIdx : *Providers)
			{
				if (ProvIdx != Idx)
				{
					MaxDep = FMath::Max(MaxDep, ComputeLayer(ProvIdx));
				}
			}
		}

		Visiting.Remove(Idx);
		const auto Layer = MaxDep + 1;
		LayerMap.Add(Idx, Layer);
		return Layer;
	};

	for (auto Index = 0; Index < _ActionNodes.Num(); ++Index)
	{
		ComputeLayer(Index);
	}

	auto MaxLayer = 0;
	TMap<int32, TArray<int32>> Buckets;
	for (const auto& [Idx, Layer] : LayerMap)
	{
		Buckets.FindOrAdd(Layer).Add(Idx);
		MaxLayer = FMath::Max(MaxLayer, Layer);
	}

	constexpr auto NodeWidth = 220;
	constexpr auto NodeBaseHeight = 80;
	constexpr auto HGap = 80;
	constexpr auto VGap = 30;
	constexpr auto StartX = 50;
	constexpr auto StartY = 50;

	for (auto Layer = 0; Layer <= MaxLayer; ++Layer)
	{
		const auto* Bucket = Buckets.Find(Layer);
		if (Bucket == nullptr) { continue; }

		const auto X = StartX + Layer * (NodeWidth + HGap);
		auto Y = StartY;

		for (const auto Idx : *Bucket)
		{
			auto* Node = _ActionNodes[Idx].Get();
			const auto PreCount = Node->Get_Preconditions().Num();
			const auto EffCount = Node->Get_Effects().Num();
			const auto PortCount = FMath::Max(PreCount, EffCount);
			const auto NodeHeight = NodeBaseHeight + PortCount * 16;

			Node->NodePosX = X;
			Node->NodePosY = Y;

			Y += NodeHeight + VGap;
		}
	}

	if (_GoalNode != nullptr)
	{
		_GoalNode->NodePosX = StartX + (MaxLayer + 1) * (NodeWidth + HGap);
		_GoalNode->NodePosY = StartY + 120;
	}
}

// ====================================================================================================================

auto
	UCkGoapDebugGraph::
	FindActionNode(int32 InIndex) const
	-> UCkGoapDebugNode_Action*
{
	return _ActionNodes.IsValidIndex(InIndex) ? _ActionNodes[InIndex].Get() : nullptr;
}

// ====================================================================================================================

auto
	UCkGoapDebugGraph::
	ComputeTopologyHash(
		const FCkGoapDebugger_GoapInfo& InInfo) const
	-> uint32
{
	auto Hash = uint32{0};
	Hash = HashCombine(Hash, GetTypeHash(InInfo.Actions.Num()));
	Hash = HashCombine(Hash, GetTypeHash(InInfo.Goals.Num()));

	for (const auto& Action : InInfo.Actions)
	{
		Hash = HashCombine(Hash, GetTypeHash(Action.ClassName));
		Hash = HashCombine(Hash, GetTypeHash(Action.Preconditions.Num()));
		Hash = HashCombine(Hash, GetTypeHash(Action.Effects.Num()));
	}

	return Hash;
}

// ====================================================================================================================
