#include "CkEqsDebugger/Data/CkEqsDebugger_DataCollector.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"
#include "CkEcs/Handle/CkHandle.h"
#include "CkEcs/Handle/CkHandle_Utils.h"

#include "CkEqs/Query/CkEqs_Fragment.h"
#include "CkEqs/Query/CkEqs_Fragment_Data.h"

#include "Engine/World.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkEqsDebugger_DataCollector::
    Collect(
        UWorld*             InWorld,
        FCk_Handle_EqsQuery InSelectedHandle,
        bool                InDeepPopulateAll)
    -> void
{
    _Queries.Reset();

    if (NOT IsValid(InWorld))
    { return; }

    auto TransientEntity = UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(InWorld);

    if (ck::Is_NOT_Valid(TransientEntity))
    { return; }

    TransientEntity.View<FFragment_EqsQuery_Params>().ForEach(
        [this, &TransientEntity, &InSelectedHandle, InDeepPopulateAll](FCk_Entity InEntity, const FFragment_EqsQuery_Params&)
        {
            auto GenericHandle = ck::MakeHandle(InEntity, TransientEntity);
            auto QueryHandle = ck::StaticCast<FCk_Handle_EqsQuery>(GenericHandle);

            auto Info = CollectShallow(QueryHandle);

            // Deep-populate the candidate + per-test breakdown when this is the selected query OR when
            // the caller asked for all queries (the "show all" overlay mode needs every candidate location).
            const auto IsSelected = ck::IsValid(InSelectedHandle) && QueryHandle == InSelectedHandle;
            if (IsSelected || InDeepPopulateAll)
            { PopulateCandidatesAndBreakdown(QueryHandle, Info); }

            _Queries.Add(MoveTemp(Info));
        });
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkEqsDebugger_DataCollector::
    Get_AllQueries() const
    -> const TArray<FCkEqsDebugger_QueryInfo>&
{
    return _Queries;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkEqsDebugger_DataCollector::
    Find_QueryInfo(
        FCk_Handle_EqsQuery InHandle) const
    -> const FCkEqsDebugger_QueryInfo*
{
    if (ck::Is_NOT_Valid(InHandle))
    { return nullptr; }

    for (const auto& Info : _Queries)
    {
        if (Info.QueryHandle == InHandle)
        { return &Info; }
    }
    return nullptr;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkEqsDebugger_DataCollector::
    CollectShallow(
        const FCk_Handle_EqsQuery& InQueryHandle)
    -> FCkEqsDebugger_QueryInfo
{
    auto Info = FCkEqsDebugger_QueryInfo{};
    Info.QueryHandle = InQueryHandle;
    Info.DebugName   = UCk_Utils_Handle_UE::Get_DebugName(InQueryHandle).ToString();
    Info.Status      = DetermineStatus(InQueryHandle);

    if (InQueryHandle.Has<FFragment_EqsQuery_Params>())
    {
        const auto& Params = InQueryHandle.Get<FFragment_EqsQuery_Params>();
        Info.Querier       = Params.Get_Querier();
        Info.ContextEntity = Params.Get_Context();
        const auto& Gen    = Params.Get_GeneratorParams();
        Info.GeneratorType = Gen.Get_GeneratorType();
        Info.TestCount     = Params.Get_Tests().Num();
        Info.RunMode       = Params.Get_RunMode();

        // Grid params surfaced for SimpleGrid / Grid only — the overlay uses these to build a matching
        // CkGrid 2dGridSystem entity for the in-world lattice. Other generator types leave them at zero
        // and the overlay's grid code path no-ops.
        if (Info.GeneratorType == ECk_Eqs_GeneratorType::SimpleGrid ||
            Info.GeneratorType == ECk_Eqs_GeneratorType::Grid)
        {
            Info.GridSpaceBetween = Gen.Get_SpaceBetween();
            Info.GridHalfSize     = Gen.Get_GridHalfSize();
        }

        Info.TestTypes.Reserve(Info.TestCount);
        for (const auto& Test : Params.Get_Tests())
        { Info.TestTypes.Add(Test.Get_TestType()); }
    }

    if (InQueryHandle.Has<FFragment_EqsQuery_State>())
    {
        const auto& State = InQueryHandle.Get<FFragment_EqsQuery_State>();
        Info.NextTestIndex = State.Get_NextTestIndex();
    }

    if (InQueryHandle.Has<FFragment_EqsQuery_Results>())
    {
        const auto& Results = InQueryHandle.Get<FFragment_EqsQuery_Results>();
        Info.HasResults     = Results.Get_HasResults();
        Info.CandidateCount = Results.Get_Candidates().Num();
        Info.BestLocation   = Results.Get_BestLocation();
        Info.BestEntity     = Results.Get_BestEntity();
    }

    return Info;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkEqsDebugger_DataCollector::
    PopulateCandidatesAndBreakdown(
        const FCk_Handle_EqsQuery& InQueryHandle,
        FCkEqsDebugger_QueryInfo&  InOutInfo)
    -> void
{
    if (NOT InQueryHandle.Has<FFragment_EqsQuery_Results>() ||
        NOT InQueryHandle.Has<FFragment_EqsQuery_DebugInfo>() ||
        NOT InQueryHandle.Has<FFragment_EqsQuery_Params>())
    { return; }

    const auto& Results   = InQueryHandle.Get<FFragment_EqsQuery_Results>();
    const auto& DebugInfo = InQueryHandle.Get<FFragment_EqsQuery_DebugInfo>();
    const auto& Params    = InQueryHandle.Get<FFragment_EqsQuery_Params>();
    const auto& Tests     = Params.Get_Tests();

    const auto& Candidates    = Results.Get_Candidates();
    const auto& PerCandidate  = DebugInfo.Get_PerCandidate();

    InOutInfo.Candidates.Reserve(Candidates.Num());

    for (auto i = 0; i < Candidates.Num(); ++i)
    {
        const auto& Cand = Candidates[i];

        auto CandidateInfo = FCkEqsDebugger_CandidateInfo{};
        CandidateInfo.ResultIndex   = i;
        CandidateInfo.Location      = Cand.Get_Location();
        CandidateInfo.EntityHandle  = Cand.Get_EntityHandle();
        CandidateInfo.FinalScore    = Cand.Get_Score();
        CandidateInfo.Passed        = Cand.Get_Passed();
        CandidateInfo.IsBestPick    = (i == 0);

        // Pull per-test breakdown from the parallel debug array. Pass-5.1: indices align
        // post-Finalize because DoFinalize reorders _PerCandidate in lockstep with Results._Candidates.
        if (PerCandidate.IsValidIndex(i))
        {
            const auto& Row = PerCandidate[i].Get_PerTest();
            CandidateInfo.PerTest.Reserve(Row.Num());
            for (auto t = 0; t < Row.Num(); ++t)
            {
                auto Per = FCkEqsDebugger_PerTestInfo{};
                Per.TestIndex            = t;
                if (Tests.IsValidIndex(t))
                {
                    Per.TestType         = Tests[t].Get_TestType();
                    Per.TestPurpose      = Tests[t].Get_Purpose();
                    Per.Weight           = Tests[t].Get_Weight();
                }
                Per.RawValue             = Row[t].Get_RawValue();
                Per.NormalizedScore      = Row[t].Get_NormalizedScore();
                Per.WeightedContribution = Row[t].Get_WeightedContribution();
                Per.PassedThisTest       = Row[t].Get_PassedThisTest();
                CandidateInfo.PerTest.Add(MoveTemp(Per));
            }
        }

        InOutInfo.Candidates.Add(MoveTemp(CandidateInfo));
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkEqsDebugger_DataCollector::
    DetermineStatus(
        const FCk_Handle_EqsQuery& InQueryHandle)
    -> ECkEqsDebugger_QueryStatus
{
    // Cancelled is checked first because Test+Cleanup pipeline may have set Failed/Complete tags
    // alongside Cancelled (the query gets all three when caller-cancelled mid-flight).
    if (InQueryHandle.Has<ck::FTag_EqsQuery_Cancelled>()) { return ECkEqsDebugger_QueryStatus::Cancelled; }
    if (InQueryHandle.Has<ck::FTag_EqsQuery_Failed>())    { return ECkEqsDebugger_QueryStatus::Failed; }
    if (InQueryHandle.Has<ck::FTag_EqsQuery_Complete>())  { return ECkEqsDebugger_QueryStatus::Complete; }
    if (InQueryHandle.Has<ck::FTag_EqsQuery_InProgress>()){ return ECkEqsDebugger_QueryStatus::InProgress; }
    if (InQueryHandle.Has<ck::FTag_EqsQuery_Pending>())   { return ECkEqsDebugger_QueryStatus::Pending; }
    return ECkEqsDebugger_QueryStatus::Unknown;
}

// --------------------------------------------------------------------------------------------------------------------
