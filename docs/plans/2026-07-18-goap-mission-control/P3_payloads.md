# P3 payloads — CkGoap engine hooks (apply only while no build is running)

Exact code to apply per file. House rules: trailing returns except UFUNCTION decls; `NOT`;
`CK_ENSURE_IF_NOT`; friends for direct `_Member` writes; `CK_DEFINE_CUSTOM_FORMATTER_ENUM` on new
UENUMs; requests carry `CK_REQUEST_DEFINE_DEBUG_NAME`? (existing Goap requests DON'T — they are
plain `CK_GENERATED_BODY` structs; match the module, not the doctrine default.)

## 1) `CkGoap/CkGoap_Fragment_Data.h` — after the ECk_Goap_ReplanPolicy block

```cpp
UENUM(BlueprintType)
enum class ECk_Goap_ReplanOrigin : uint8
{
    // No replan recorded yet.
    None,
    // First plan after activation (_PlanOnStart).
    PlanOnStart,
    // Consumer called Request_Plan directly.
    Explicit,
    // AutoReplan fired on a world-state change.
    WorldStateDirty,
    // AutoReplan fired on an action-cost change.
    CostDirty,
    // AutoReplan fired with both dirty flags set in the same window.
    WorldStateAndCostDirty,
    // Request_SetGoal replans with the new goal.
    GoalChanged,
    // AddAction / Request_RemoveAction rebuilt the operator set.
    CatalogChanged,
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_Goap_ReplanOrigin);

UENUM(BlueprintType)
enum class ECk_Goap_WorldStateMutator : uint8
{
    SetValue,
    OverridePush,
    OverridePop,
    OverrideClear,
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_Goap_WorldStateMutator);

// One recorded world-state mutation whose EFFECTIVE value changed. The ring
// on the WS entity keeps the most recent Goap_WorldStateChangeLog_Capacity of
// these; the debugger's timeline and the replan-cause attribution both read it.
USTRUCT(BlueprintType)
struct CKGOAP_API FCk_Goap_WorldStateChange
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_Goap_WorldStateChange);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    FGameplayTag _Key;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    bool _OldValue = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    bool _NewValue = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    int64 _FrameNumber = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    ECk_Goap_WorldStateMutator _Mutator = ECk_Goap_WorldStateMutator::SetValue;

public:
    CK_PROPERTY_GET(_Key);
    CK_PROPERTY_GET(_OldValue);
    CK_PROPERTY_GET(_NewValue);
    CK_PROPERTY_GET(_FrameNumber);
    CK_PROPERTY_GET(_Mutator);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_Goap_WorldStateChange, _Key, _OldValue, _NewValue, _FrameNumber, _Mutator);
};

// Why the last replan fired: origin + the world-state changes recorded since
// the previous replan (coalesced count = ChangedKeys.Num() when > 1).
USTRUCT(BlueprintType)
struct CKGOAP_API FCk_Goap_ReplanCauseInfo
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_Goap_ReplanCauseInfo);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    ECk_Goap_ReplanOrigin _Origin = ECk_Goap_ReplanOrigin::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    TArray<FCk_Goap_WorldStateChange> _ChangedKeys;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    int32 _AttemptNumber = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    int64 _FrameNumber = 0;

public:
    CK_PROPERTY_GET(_Origin);
    CK_PROPERTY_GET(_ChangedKeys);
    CK_PROPERTY_GET(_AttemptNumber);
    CK_PROPERTY_GET(_FrameNumber);
};

// Post-search statistics for the Planner's most recent A* run.
USTRUCT(BlueprintType)
struct CKGOAP_API FCk_Goap_SearchStats
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_Goap_SearchStats);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    ECk_GoapPlanStatus _PlanStatus = ECk_GoapPlanStatus::Idle;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    int32 _Iterations = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    int64 _ElapsedMicroseconds = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    int32 _StatePoolSize = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    int32 _PlanLength = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    float _PlanCost = 0.0f;

public:
    CK_PROPERTY_GET(_PlanStatus);
    CK_PROPERTY_GET(_Iterations);
    CK_PROPERTY_GET(_ElapsedMicroseconds);
    CK_PROPERTY_GET(_StatePoolSize);
    CK_PROPERTY_GET(_PlanLength);
    CK_PROPERTY_GET(_PlanCost);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_Goap_SearchStats, _PlanStatus, _Iterations, _ElapsedMicroseconds, _StatePoolSize, _PlanLength, _PlanCost);
};

// One regressive-search node from the last completed search, in discovery
// order: the constraint set, the action whose reverse application produced it,
// and how many of its constraints the (seed-time) world state left unsatisfied.
USTRUCT(BlueprintType)
struct CKGOAP_API FCk_Goap_SearchDebugRow
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_Goap_SearchDebugRow);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    TArray<FCk_GoapWS_Condition_Authored> _Conditions;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    TSubclassOf<UCk_GoapAction_EntityScript> _ViaActionClass;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    int32 _UnsatisfiedCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = true))
    bool _SatisfiedByWorldState = false;

public:
    CK_PROPERTY_GET(_Conditions);
    CK_PROPERTY_GET(_ViaActionClass);
    CK_PROPERTY_GET(_UnsatisfiedCount);
    CK_PROPERTY_GET(_SatisfiedByWorldState);
};
```

## 2) `Action/CkGoap_Action_Fragment_Data.h` — extend the Plan request

```cpp
USTRUCT(BlueprintType)
struct CKGOAP_API FCk_Request_Goap_Planner_Plan
{
    GENERATED_BODY()
    CK_GENERATED_BODY(FCk_Request_Goap_Planner_Plan);

private:
    // Who asked for this plan — recorded onto the Planner's ReplanCause when
    // the request is consumed. Defaults to Explicit so existing callers keep
    // their meaning; AutoReplan / SetGoal / catalog paths stamp their own.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    ECk_Goap_ReplanOrigin _Origin = ECk_Goap_ReplanOrigin::Explicit;

public:
    CK_PROPERTY(_Origin);
};
```

## 3) `Planner/CkGoap_Planner_Fragment.h` — new fragment (near ReplanThrottle)

```cpp
// --------------------------------------------------------------------------------------------------------------------
// Why the last replan fired — stamped by HandleRequests when it consumes a
// Plan request; read by the debugger via Get_LastReplanCause.

    struct CKGOAP_API FFragment_Goap_Planner_ReplanCause
    {
    public:
        CK_GENERATED_BODY(FFragment_Goap_Planner_ReplanCause);

        friend class ::UCk_Utils_Goap_Planner_UE;
        friend class FProcessor_Goap_Planner_HandleRequests;

    private:
        FCk_Goap_ReplanCauseInfo _Info;
        int64 _LastReplanFrame = 0;

    public:
        CK_PROPERTY_GET(_Info);
        CK_PROPERTY_GET(_LastReplanFrame);
    };
```

## 4) `WorldState/CkGoap_WorldState_Fragment.h` — change-log ring (near Subscribers)

```cpp
// --------------------------------------------------------------------------------------------------------------------
// Bounded ring of effective-value changes (base writes AND override
// push/pop/clear deltas). Feeds the debugger's timeline WS lane and the
// Planner's replan-cause attribution. Oldest entries drop first.

    struct CKGOAP_API FFragment_Goap_WorldState_ChangeLog
    {
    public:
        CK_GENERATED_BODY(FFragment_Goap_WorldState_ChangeLog);

        friend class ::UCk_Utils_Goap_WorldState_UE;
        friend class FProcessor_Goap_WorldState_HandleRequests;

        static constexpr int32 Capacity = 32;

    private:
        TArray<FCk_Goap_WorldStateChange> _Entries;

    public:
        CK_PROPERTY_GET(_Entries);

        auto Record(FCk_Goap_WorldStateChange InChange) -> void
        {
            _Entries.Add(MoveTemp(InChange));
            if (_Entries.Num() > Capacity)
            { _Entries.RemoveAt(0, _Entries.Num() - Capacity); }
        }
    };
```

## 5) `Action/CkGoap_Action_Processor.cpp` AutoReplan (line ~338)

Replace:
```cpp
    auto& Requests = InHandle.AddOrGet<FFragment_Goap_Planner_Requests>();
    Requests._Requests.Add(FCk_Request_Goap_Planner_Plan{});
```
with:
```cpp
    const auto Origin = [&]
    {
        if (IsInitialPlanPending)   { return ECk_Goap_ReplanOrigin::PlanOnStart; }
        if (WSDirty && CostDirty)   { return ECk_Goap_ReplanOrigin::WorldStateAndCostDirty; }
        if (CostDirty)              { return ECk_Goap_ReplanOrigin::CostDirty; }
        return ECk_Goap_ReplanOrigin::WorldStateDirty;
    }();

    auto& Requests = InHandle.AddOrGet<FFragment_Goap_Planner_Requests>();
    Requests._Requests.Add(FCk_Request_Goap_Planner_Plan{}.Set_Origin(Origin));
```

## 6) `HandleRequests` Plan branch (after `++InPlanState._PlanAttemptCount;`)

```cpp
                // Record why this replan fired + the WS changes since the last
                // one (coalescing evidence for the debugger's timeline).
                {
                    auto& Cause = InHandle.AddOrGet<FFragment_Goap_Planner_ReplanCause>();
                    Cause._Info = FCk_Goap_ReplanCauseInfo{};
                    Cause._Info._Origin = InTypedRequest.Get_Origin();
                    Cause._Info._AttemptNumber = InPlanState.Get_PlanAttemptCount();
                    Cause._Info._FrameNumber = static_cast<int64>(GFrameCounter);

                    if (const auto WS = InWSSource.Get_Resolved(); ck::IsValid(WS))
                    {
                        if (WS.Has<FFragment_Goap_WorldState_ChangeLog>())
                        {
                            const auto& Log = WS.Get<FFragment_Goap_WorldState_ChangeLog>();
                            for (const auto& Change : Log.Get_Entries())
                            {
                                if (Change.Get_FrameNumber() > Cause._LastReplanFrame)
                                { Cause._Info._ChangedKeys.Add(Change); }
                            }
                        }
                    }
                    Cause._LastReplanFrame = static_cast<int64>(GFrameCounter);
                }
```
(FFragment_Goap_Planner_ReplanCause needs `friend class FProcessor_Goap_Planner_HandleRequests;`
— included above. `_Info._Origin` etc. are private writes from the processor → the CAUSE INFO
struct's fields are written directly; FCk_Goap_ReplanCauseInfo therefore needs
`friend class ck::FProcessor_Goap_Planner_HandleRequests;` + `friend class ::UCk_Utils_Goap_Planner_UE;`
OR build via a constructor — simplest: give ReplanCauseInfo CK_DEFINE_CONSTRUCTORS(…, _Origin,
_ChangedKeys?, …) is clumsy for array append; ADD the friends to the USTRUCT instead.)

## 7) WS processor value-change branch (`CkGoap_WorldState_Processor.cpp`)

Locate where a Set request ACTUALLY changes a value (same place the
OnGoapWorldStateValueChanged broadcast + subscriber-dirty stamping happens) and add:

```cpp
            auto& ChangeLog = InHandle.AddOrGet<FFragment_Goap_WorldState_ChangeLog>();
            ChangeLog.Record(FCk_Goap_WorldStateChange{
                Key, OldValue, NewValue, static_cast<int64>(GFrameCounter),
                ECk_Goap_WorldStateMutator::SetValue});
```

## 8) WS Utils override paths (`CkGoap_WorldState_Utils.cpp`)

Push_Override / Push_Override_SingleKey / Pop_Override_ByName / Clear_Overrides already compute
per-key effective-value deltas to decide dirty-firing. At each detected effective change, record
into the ChangeLog with the matching mutator enum. Then add:

```cpp
UFUNCTION(BlueprintPure, Category = "Ck|GOAP|WorldState",
    DisplayName="[Ck][GOAP|WS] Get Recent Changes")
static TArray<FCk_Goap_WorldStateChange>
Get_RecentChanges(const FCk_Handle_Goap_WorldState& InWorldState);
```
impl: return ChangeLog fragment entries (empty if fragment absent).

## 9) Planner Utils getters (`Planner/CkGoap_Planner_Utils.h/.cpp`)

All BlueprintPure, Category "Ck|Utils|Goap|Planner", reading fragments with
`CK_ENSURE_IF_NOT(ck::IsValid(InPlanner), …) { return {}; }` guards:

- `Get_ReplanPolicy(Planner) -> ECk_Goap_ReplanPolicy` (Params)
- `Get_MinReplanInterval(Planner) -> float`
- `Get_SearchBudgetMicroseconds(Planner) -> int64`
- `Get_CostThreshold(Planner) -> float`
- `Get_PlanOnStart(Planner) -> bool`
- `Get_AllowPlanFailed(Planner) -> bool`
- `Get_PlannerTag(Planner) -> FGameplayTag`
- `Get_HasUnconditionalFallback(Planner) -> bool` (Current fragment — surfaces the tenet check)
- `Get_LastReplanCause(Planner) -> FCk_Goap_ReplanCauseInfo` (fragment or default)
- `Get_LastSearchStats(Planner) -> FCk_Goap_SearchStats` — from
  `FFragment_Goap_Planner_Result` (`_TotalIterations`, `_TotalTimeMicroseconds`) +
  `FFragment_Goap_Planner_PlanContext` (`Get_Graph().Get_StatePoolSize()`) +
  PlanState (status, plan length, cost)
- `Get_LastSearchDebug(Planner) -> TArray<FCk_Goap_SearchDebugRow>` — walk
  `PlanContext.Get_Graph()`: build to→(from,action) from EdgeActions
  (`PackEdgeKey` layout: from<<32|to; expose an iteration accessor on FGoapGraph:
  `Get_EdgeActions() const -> const TMap<int64,int32>&` via the shared data — ADD a
  const accessor mirroring Get_StatePool), then per pool index:
  conditions from `FConstraintSet::Get(key)` over `0..Registry.Num()` resolved to
  tags via the WS-source's `FFragment_Goap_WorldState_KeyRegistry.Get_Registry().GetTag(k)`;
  `_SatisfiedByWorldState = Set.IsSatisfiedBy(Graph.Get_CurrentWorldState())`;
  `_UnsatisfiedCount = Set.CountUnsatisfied(...)`; `_ViaActionClass` from the
  edge action's `Get_Actions()[ActionIdx].ActionClass`.

Also add `Get_EdgeActions()` to `Algorithm/CkGoap_Graph.h` (one-liner beside Get_StatePool).

## Gate

`--test-pattern Goap` (engine tests + debugger specs) — compare against baseline 33/33.
