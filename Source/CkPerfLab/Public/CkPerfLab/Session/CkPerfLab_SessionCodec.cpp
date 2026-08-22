#include "CkPerfLab_SessionCodec.h"

#include "CkPerfLab_Log.h"

#include <Dom/JsonObject.h>
#include <Serialization/JsonReader.h>
#include <Serialization/JsonSerializer.h>
#include <Serialization/JsonWriter.h>

// --------------------------------------------------------------------------------------------------------------------

namespace ck_perf_lab_codec
{
    using FJsonObjectPtr = TSharedPtr<FJsonObject>;

    // Enums travel as their C++ identifier rather than their display text or their integer value:
    // the identifier is stable across renames of the display string and readable in a diff, and an
    // integer would silently re-point at a different case if the enum ever gains a member.
    template <typename T_Enum>
    auto
        Get_EnumString(
            T_Enum InValue)
        -> FString
    {
        return StaticEnum<T_Enum>()->GetNameStringByValue(static_cast<int64>(InValue));
    }

    template <typename T_Enum>
    auto
        TryGet_EnumValue(
            const FString& InName,
            T_Enum& OutValue)
        -> bool
    {
        const auto Value = StaticEnum<T_Enum>()->GetValueByNameString(InName);

        if (Value == INDEX_NONE)
        {
            return false;
        }

        OutValue = static_cast<T_Enum>(Value);
        return true;
    }

    auto
        Get_ObjectAsString(
            const FJsonObjectPtr& InObject)
        -> FString
    {
        auto Out = FString{};
        auto Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Out);
        FJsonSerializer::Serialize(InObject.ToSharedRef(), Writer);
        return Out;
    }

    auto
        TryGet_RootObject(
            const FString& InJson,
            FJsonObjectPtr& OutObject)
        -> bool
    {
        const auto Reader = TJsonReaderFactory<TCHAR>::Create(InJson);

        if (NOT FJsonSerializer::Deserialize(Reader, OutObject) || NOT OutObject.IsValid())
        {
            return false;
        }

        // The version gate comes before any field read, so an unknown schema can never be partially
        // interpreted using this version's field meanings.
        auto SchemaVersion = int32{0};

        if (NOT OutObject->TryGetNumberField(TEXT("schemaVersion"), SchemaVersion))
        {
            return false;
        }

        return SchemaVersion == ck::perf_lab::k_SchemaVersion;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Write_MetricStats(
            const FCk_PerfLab_MetricStats& InStats)
        -> FJsonObjectPtr
    {
        auto Object = MakeShared<FJsonObject>();

        Object->SetStringField(TEXT("availability"), Get_EnumString(InStats.Get_Availability()));
        Object->SetStringField(TEXT("reason"),       InStats.Get_UnavailableReason());
        Object->SetNumberField(TEXT("avgMs"),        InStats.Get_AvgMs());
        Object->SetNumberField(TEXT("worstMs"),      InStats.Get_WorstMs());
        Object->SetNumberField(TEXT("p99Ms"),        InStats.Get_P99Ms());
        Object->SetNumberField(TEXT("onePercentLowMs"), InStats.Get_OnePercentLowMs());
        Object->SetNumberField(TEXT("sampleCount"),  InStats.Get_SampleCount());
        Object->SetNumberField(TEXT("outlierCount"), InStats.Get_OutlierCount());

        return Object;
    }

    auto
        TryRead_MetricStats(
            const FJsonObjectPtr& InObject,
            FCk_PerfLab_MetricStats& OutStats)
        -> bool
    {
        if (NOT InObject.IsValid())
        {
            return false;
        }

        auto Availability = ECk_Stats_MetricAvailability::Unavailable_NotYetSampled;

        if (NOT TryGet_EnumValue(InObject->GetStringField(TEXT("availability")), Availability))
        {
            return false;
        }

        OutStats.Set_Availability(Availability)
                .Set_UnavailableReason(InObject->GetStringField(TEXT("reason")))
                .Set_AvgMs(InObject->GetNumberField(TEXT("avgMs")))
                .Set_WorstMs(InObject->GetNumberField(TEXT("worstMs")))
                .Set_P99Ms(InObject->GetNumberField(TEXT("p99Ms")))
                .Set_OnePercentLowMs(InObject->GetNumberField(TEXT("onePercentLowMs")))
                .Set_SampleCount(InObject->GetIntegerField(TEXT("sampleCount")))
                .Set_OutlierCount(InObject->GetIntegerField(TEXT("outlierCount")));

        return true;
    }

    auto
        Write_MetricSet(
            const FCk_PerfLab_MetricSet& InSet)
        -> FJsonObjectPtr
    {
        auto Object = MakeShared<FJsonObject>();

        Object->SetObjectField(TEXT("frame"),        Write_MetricStats(InSet.Get_Frame()));
        Object->SetObjectField(TEXT("gameThread"),   Write_MetricStats(InSet.Get_GameThread()));
        Object->SetObjectField(TEXT("renderThread"), Write_MetricStats(InSet.Get_RenderThread()));
        Object->SetObjectField(TEXT("rhiThread"),    Write_MetricStats(InSet.Get_RhiThread()));
        Object->SetObjectField(TEXT("gpu"),          Write_MetricStats(InSet.Get_Gpu()));

        return Object;
    }

    auto
        TryRead_MetricSet(
            const FJsonObjectPtr& InObject,
            FCk_PerfLab_MetricSet& OutSet)
        -> bool
    {
        if (NOT InObject.IsValid())
        {
            return false;
        }

        auto Frame = FCk_PerfLab_MetricStats{};
        auto Game  = FCk_PerfLab_MetricStats{};
        auto Render= FCk_PerfLab_MetricStats{};
        auto Rhi   = FCk_PerfLab_MetricStats{};
        auto Gpu   = FCk_PerfLab_MetricStats{};

        if (NOT TryRead_MetricStats(InObject->GetObjectField(TEXT("frame")), Frame) ||
            NOT TryRead_MetricStats(InObject->GetObjectField(TEXT("gameThread")), Game) ||
            NOT TryRead_MetricStats(InObject->GetObjectField(TEXT("renderThread")), Render) ||
            NOT TryRead_MetricStats(InObject->GetObjectField(TEXT("rhiThread")), Rhi) ||
            NOT TryRead_MetricStats(InObject->GetObjectField(TEXT("gpu")), Gpu))
        {
            return false;
        }

        OutSet.Set_Frame(Frame).Set_GameThread(Game).Set_RenderThread(Render).Set_RhiThread(Rhi).Set_Gpu(Gpu);

        return true;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Write_Request(
            const FCk_PerfLab_Request& InRequest,
            const FJsonObjectPtr& InObject)
        -> void
    {
        const auto& ModeParams = InRequest.Get_ModeParams();
        const auto& Settle     = InRequest.Get_Settle();

        InObject->SetStringField(TEXT("sessionId"),         InRequest.Get_SessionId());
        InObject->SetStringField(TEXT("mapPath"),           InRequest.Get_MapPath());
        InObject->SetNumberField(TEXT("budgetMs"),          InRequest.Get_BudgetMs());
        InObject->SetNumberField(TEXT("seed"),              InRequest.Get_Seed());
        InObject->SetNumberField(TEXT("requestingHostPid"), InRequest.Get_RequestingHostPid());
        InObject->SetStringField(TEXT("createdUtc"),        InRequest.Get_CreatedUtc());
        InObject->SetStringField(TEXT("mode"),              Get_EnumString(InRequest.Get_Mode()));

        auto ModeParamsObject = MakeShared<FJsonObject>();
        ModeParamsObject->SetNumberField(TEXT("positionBudget"),        ModeParams.Get_PositionBudget());
        ModeParamsObject->SetNumberField(TEXT("directionsPerPosition"), ModeParams.Get_DirectionsPerPosition());
        ModeParamsObject->SetNumberField(TEXT("repeats"),               ModeParams.Get_Repeats());
        ModeParamsObject->SetNumberField(TEXT("minPositionSpacingCm"),  ModeParams.Get_MinPositionSpacingCm());
        ModeParamsObject->SetNumberField(TEXT("eyeHeightCm"),           ModeParams.Get_EyeHeightCm());
        ModeParamsObject->SetNumberField(TEXT("censusRadiusCm"),        ModeParams.Get_CensusRadiusCm());
        ModeParamsObject->SetBoolField  (TEXT("warmSweep"),             ModeParams.Get_WarmSweep());
        ModeParamsObject->SetBoolField  (TEXT("adaptiveRevisit"),       ModeParams.Get_AdaptiveRevisit());
        ModeParamsObject->SetBoolField  (TEXT("retainRawSamples"),      ModeParams.Get_RetainRawSamples());
        InObject->SetObjectField(TEXT("modeParams"), ModeParamsObject);

        auto SettleObject = MakeShared<FJsonObject>();
        SettleObject->SetNumberField(TEXT("warmupFrames"),         Settle.Get_WarmupFrames());
        SettleObject->SetNumberField(TEXT("stabilityCv"),          Settle.Get_StabilityCv());
        SettleObject->SetNumberField(TEXT("stabilityFrames"),      Settle.Get_StabilityFrames());
        SettleObject->SetNumberField(TEXT("streamingQuietFrames"), Settle.Get_StreamingQuietFrames());
        SettleObject->SetNumberField(TEXT("settleTimeoutSec"),     Settle.Get_SettleTimeoutSec());
        SettleObject->SetNumberField(TEXT("targetSampleCount"),    Settle.Get_TargetSampleCount());
        InObject->SetObjectField(TEXT("settle"), SettleObject);

        auto OutlierObject = MakeShared<FJsonObject>();
        OutlierObject->SetNumberField(TEXT("madK"), InRequest.Get_OutlierMadK());
        InObject->SetObjectField(TEXT("outlier"), OutlierObject);

        auto WatchdogObject = MakeShared<FJsonObject>();
        WatchdogObject->SetNumberField(TEXT("childWallClockBudgetSec"), InRequest.Get_ChildWallClockBudgetSec());
        InObject->SetObjectField(TEXT("watchdog"), WatchdogObject);
    }

    auto
        TryRead_Request(
            const FJsonObjectPtr& InObject,
            FCk_PerfLab_Request& OutRequest)
        -> bool
    {
        if (NOT InObject.IsValid())
        {
            return false;
        }

        auto Mode = ECk_PerfLab_Mode::Standard;

        if (NOT TryGet_EnumValue(InObject->GetStringField(TEXT("mode")), Mode))
        {
            return false;
        }

        const auto& ModeParamsObject = InObject->GetObjectField(TEXT("modeParams"));
        const auto& SettleObject     = InObject->GetObjectField(TEXT("settle"));

        if (NOT ModeParamsObject.IsValid() || NOT SettleObject.IsValid())
        {
            return false;
        }

        auto ModeParams = FCk_PerfLab_ModeParams{};
        ModeParams.Set_PositionBudget(ModeParamsObject->GetIntegerField(TEXT("positionBudget")))
                  .Set_DirectionsPerPosition(ModeParamsObject->GetIntegerField(TEXT("directionsPerPosition")))
                  .Set_Repeats(ModeParamsObject->GetIntegerField(TEXT("repeats")))
                  .Set_MinPositionSpacingCm(ModeParamsObject->GetNumberField(TEXT("minPositionSpacingCm")))
                  .Set_EyeHeightCm(ModeParamsObject->GetNumberField(TEXT("eyeHeightCm")))
                  .Set_CensusRadiusCm(ModeParamsObject->GetNumberField(TEXT("censusRadiusCm")))
                  .Set_WarmSweep(ModeParamsObject->GetBoolField(TEXT("warmSweep")))
                  .Set_AdaptiveRevisit(ModeParamsObject->GetBoolField(TEXT("adaptiveRevisit")))
                  .Set_RetainRawSamples(ModeParamsObject->GetBoolField(TEXT("retainRawSamples")));

        auto Settle = FCk_PerfLab_SettleParams{};
        Settle.Set_WarmupFrames(SettleObject->GetIntegerField(TEXT("warmupFrames")))
              .Set_StabilityCv(SettleObject->GetNumberField(TEXT("stabilityCv")))
              .Set_StabilityFrames(SettleObject->GetIntegerField(TEXT("stabilityFrames")))
              .Set_StreamingQuietFrames(SettleObject->GetIntegerField(TEXT("streamingQuietFrames")))
              .Set_SettleTimeoutSec(SettleObject->GetNumberField(TEXT("settleTimeoutSec")))
              .Set_TargetSampleCount(SettleObject->GetIntegerField(TEXT("targetSampleCount")));

        const auto& OutlierObject  = InObject->GetObjectField(TEXT("outlier"));
        const auto& WatchdogObject = InObject->GetObjectField(TEXT("watchdog"));

        OutRequest.Set_SessionId(InObject->GetStringField(TEXT("sessionId")))
                  .Set_MapPath(InObject->GetStringField(TEXT("mapPath")))
                  .Set_BudgetMs(InObject->GetNumberField(TEXT("budgetMs")))
                  .Set_Seed(InObject->GetIntegerField(TEXT("seed")))
                  .Set_RequestingHostPid(InObject->GetIntegerField(TEXT("requestingHostPid")))
                  .Set_CreatedUtc(InObject->GetStringField(TEXT("createdUtc")))
                  .Set_Mode(Mode)
                  .Set_ModeParams(ModeParams)
                  .Set_Settle(Settle)
                  .Set_OutlierMadK(OutlierObject.IsValid() ? OutlierObject->GetNumberField(TEXT("madK")) : 3.5)
                  .Set_ChildWallClockBudgetSec(
                      WatchdogObject.IsValid() ? WatchdogObject->GetIntegerField(TEXT("childWallClockBudgetSec")) : 1800);

        return true;
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::perf_lab
{
    auto
        Write_RequestToJson(
            const FCk_PerfLab_Request& InRequest)
        -> FString
    {
        auto Root = MakeShared<FJsonObject>();
        Root->SetNumberField(TEXT("schemaVersion"), k_SchemaVersion);
        ck_perf_lab_codec::Write_Request(InRequest, Root);

        return ck_perf_lab_codec::Get_ObjectAsString(Root);
    }

    auto
        TryRead_RequestFromJson(
            const FString& InJson,
            FCk_PerfLab_Request& OutRequest)
        -> bool
    {
        auto Root = ck_perf_lab_codec::FJsonObjectPtr{};

        if (NOT ck_perf_lab_codec::TryGet_RootObject(InJson, Root))
        {
            return false;
        }

        auto Decoded = FCk_PerfLab_Request{};

        if (NOT ck_perf_lab_codec::TryRead_Request(Root, Decoded))
        {
            return false;
        }

        OutRequest = Decoded;
        return true;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Write_HeartbeatToJson(
            const FCk_PerfLab_Heartbeat& InHeartbeat)
        -> FString
    {
        auto Root = MakeShared<FJsonObject>();

        Root->SetNumberField(TEXT("schemaVersion"),     k_SchemaVersion);
        Root->SetStringField(TEXT("sessionId"),         InHeartbeat.Get_SessionId());
        Root->SetStringField(TEXT("state"),             ck_perf_lab_codec::Get_EnumString(InHeartbeat.Get_State()));
        Root->SetNumberField(TEXT("positionsTotal"),    InHeartbeat.Get_PositionsTotal());
        Root->SetNumberField(TEXT("positionsDone"),     InHeartbeat.Get_PositionsDone());
        Root->SetStringField(TEXT("currentPositionId"), InHeartbeat.Get_CurrentPositionId());
        Root->SetStringField(TEXT("message"),           InHeartbeat.Get_Message());
        Root->SetStringField(TEXT("lastUpdateUtc"),     InHeartbeat.Get_LastUpdateUtc());

        return ck_perf_lab_codec::Get_ObjectAsString(Root);
    }

    auto
        TryRead_HeartbeatFromJson(
            const FString& InJson,
            FCk_PerfLab_Heartbeat& OutHeartbeat)
        -> bool
    {
        auto Root = ck_perf_lab_codec::FJsonObjectPtr{};

        if (NOT ck_perf_lab_codec::TryGet_RootObject(InJson, Root))
        {
            return false;
        }

        auto State = ECk_PerfLab_RunState::Booting;

        if (NOT ck_perf_lab_codec::TryGet_EnumValue(Root->GetStringField(TEXT("state")), State))
        {
            return false;
        }

        OutHeartbeat = FCk_PerfLab_Heartbeat{}
            .Set_SessionId(Root->GetStringField(TEXT("sessionId")))
            .Set_State(State)
            .Set_PositionsTotal(Root->GetIntegerField(TEXT("positionsTotal")))
            .Set_PositionsDone(Root->GetIntegerField(TEXT("positionsDone")))
            .Set_CurrentPositionId(Root->GetStringField(TEXT("currentPositionId")))
            .Set_Message(Root->GetStringField(TEXT("message")))
            .Set_LastUpdateUtc(Root->GetStringField(TEXT("lastUpdateUtc")));

        return true;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Write_SessionToJson(
            const FCk_PerfLab_Session& InSession)
        -> FString
    {
        using namespace ck_perf_lab_codec;

        auto Root = MakeShared<FJsonObject>();

        Root->SetNumberField(TEXT("schemaVersion"), k_SchemaVersion);
        Root->SetStringField(TEXT("sessionId"),     InSession.Get_SessionId());
        Root->SetStringField(TEXT("state"),         Get_EnumString(InSession.Get_State()));

        auto RequestObject = MakeShared<FJsonObject>();
        Write_Request(InSession.Get_Request(), RequestObject);
        Root->SetObjectField(TEXT("request"), RequestObject);

        const auto& Environment = InSession.Get_Environment();

        auto EnvironmentObject = MakeShared<FJsonObject>();
        EnvironmentObject->SetStringField(TEXT("buildConfiguration"), Environment.Get_BuildConfiguration());
        EnvironmentObject->SetStringField(TEXT("instanceMode"),       Environment.Get_InstanceMode());
        EnvironmentObject->SetStringField(TEXT("rhiName"),            Environment.Get_RhiName());
        EnvironmentObject->SetStringField(TEXT("gpuBrand"),           Environment.Get_GpuBrand());
        EnvironmentObject->SetStringField(TEXT("cpuBrand"),           Environment.Get_CpuBrand());
        EnvironmentObject->SetNumberField(TEXT("cpuCoreCount"),       Environment.Get_CpuCoreCount());
        EnvironmentObject->SetStringField(TEXT("machineName"),        Environment.Get_MachineName());
        EnvironmentObject->SetStringField(TEXT("engineVersion"),      Environment.Get_EngineVersion());
        EnvironmentObject->SetStringField(TEXT("childBinary"),        Environment.Get_ChildBinary());
        EnvironmentObject->SetBoolField  (TEXT("sourceControlDisabled"), Environment.Get_SourceControlDisabled());
        EnvironmentObject->SetStringField(TEXT("startedUtc"),         Environment.Get_StartedUtc());
        EnvironmentObject->SetStringField(TEXT("finishedUtc"),        Environment.Get_FinishedUtc());

        auto ViewportObject = MakeShared<FJsonObject>();
        ViewportObject->SetNumberField(TEXT("width"),  Environment.Get_ViewportSizeActual().X);
        ViewportObject->SetNumberField(TEXT("height"), Environment.Get_ViewportSizeActual().Y);
        EnvironmentObject->SetObjectField(TEXT("viewportSizeActual"), ViewportObject);

        // Sorted so two runs that forced the same cvars serialise identically; TMap iteration order
        // is not a contract and a diff of two sessions must not show phantom changes.
        auto CvarKeys = TArray<FString>{};
        Environment.Get_ForcedCvars().GetKeys(CvarKeys);
        CvarKeys.Sort();

        auto CvarObject = MakeShared<FJsonObject>();

        for (const auto& Key : CvarKeys)
        {
            CvarObject->SetStringField(Key, Environment.Get_ForcedCvars()[Key]);
        }

        EnvironmentObject->SetObjectField(TEXT("forcedCvars"), CvarObject);
        Root->SetObjectField(TEXT("environment"), EnvironmentObject);

        auto PositionValues = TArray<TSharedPtr<FJsonValue>>{};

        for (const auto& Position : InSession.Get_Positions())
        {
            auto PositionObject = MakeShared<FJsonObject>();
            PositionObject->SetStringField(TEXT("positionId"),  Position.Get_PositionId());
            PositionObject->SetNumberField(TEXT("eyeHeightCm"), Position.Get_EyeHeightCm());
            PositionObject->SetBoolField  (TEXT("revisited"),   Position.Get_Revisited());

            auto LocationObject = MakeShared<FJsonObject>();
            LocationObject->SetNumberField(TEXT("x"), Position.Get_Location().X);
            LocationObject->SetNumberField(TEXT("y"), Position.Get_Location().Y);
            LocationObject->SetNumberField(TEXT("z"), Position.Get_Location().Z);
            PositionObject->SetObjectField(TEXT("location"), LocationObject);

            auto DirectionValues = TArray<TSharedPtr<FJsonValue>>{};

            for (const auto& Direction : Position.Get_Directions())
            {
                auto DirectionObject = MakeShared<FJsonObject>();
                DirectionObject->SetNumberField(TEXT("yawDegrees"),   Direction.Get_YawDegrees());
                DirectionObject->SetNumberField(TEXT("pitchDegrees"), Direction.Get_PitchDegrees());
                DirectionObject->SetObjectField(TEXT("metrics"),      Write_MetricSet(Direction.Get_Metrics()));

                DirectionValues.Add(MakeShared<FJsonValueObject>(DirectionObject));
            }

            PositionObject->SetArrayField(TEXT("directions"), DirectionValues);
            PositionObject->SetObjectField(TEXT("aggregate"), Write_MetricSet(Position.Get_Aggregate()));

            auto ConfidenceObject = MakeShared<FJsonObject>();
            ConfidenceObject->SetStringField(TEXT("rating"), Get_EnumString(Position.Get_Confidence().Get_Rating()));

            auto ReasonValues = TArray<TSharedPtr<FJsonValue>>{};

            for (const auto& Reason : Position.Get_Confidence().Get_Reasons())
            {
                ReasonValues.Add(MakeShared<FJsonValueString>(Get_EnumString(Reason)));
            }

            ConfidenceObject->SetArrayField(TEXT("reasons"), ReasonValues);
            PositionObject->SetObjectField(TEXT("confidence"), ConfidenceObject);

            auto CensusValues = TArray<TSharedPtr<FJsonValue>>{};

            for (const auto& Row : Position.Get_ActorCensus())
            {
                auto RowObject = MakeShared<FJsonObject>();
                RowObject->SetStringField(TEXT("objectPath"), Row.Get_ObjectPath());
                RowObject->SetStringField(TEXT("className"),  Row.Get_ClassName());
                RowObject->SetNumberField(TEXT("distanceCm"), Row.Get_DistanceCm());

                auto YawValues = TArray<TSharedPtr<FJsonValue>>{};

                for (const auto& Yaw : Row.Get_InViewYaws())
                {
                    YawValues.Add(MakeShared<FJsonValueNumber>(Yaw));
                }

                RowObject->SetArrayField(TEXT("inViewYaws"), YawValues);

                auto ProxyObject = MakeShared<FJsonObject>();
                ProxyObject->SetNumberField(TEXT("triangleCount"),         Row.Get_TriangleCount());
                ProxyObject->SetNumberField(TEXT("materialSlotCount"),     Row.Get_MaterialSlotCount());
                ProxyObject->SetNumberField(TEXT("lightCount"),            Row.Get_LightCount());
                ProxyObject->SetNumberField(TEXT("niagaraComponentCount"), Row.Get_NiagaraComponentCount());
                ProxyObject->SetBoolField  (TEXT("tickEnabled"),           Row.Get_TickEnabled());
                ProxyObject->SetBoolField  (TEXT("castsDynamicShadow"),    Row.Get_CastsDynamicShadow());
                ProxyObject->SetBoolField  (TEXT("hasCollision"),          Row.Get_HasCollision());
                RowObject->SetObjectField(TEXT("costProxies"), ProxyObject);

                CensusValues.Add(MakeShared<FJsonValueObject>(RowObject));
            }

            PositionObject->SetArrayField(TEXT("actorCensus"), CensusValues);

            PositionValues.Add(MakeShared<FJsonValueObject>(PositionObject));
        }

        Root->SetArrayField(TEXT("positions"), PositionValues);

        // The child never writes analysis; the host fills it in later. Emitting an explicit null
        // says "not analysed yet" rather than leaving a reader to wonder if the key was dropped.
        Root->SetField(TEXT("analysis"), MakeShared<FJsonValueNull>());

        return Get_ObjectAsString(Root);
    }

    auto
        TryRead_SessionSummaryFromJson(
            const FString& InJson,
            FCk_PerfLab_Session& OutSummary)
        -> bool
    {
        auto Root = ck_perf_lab_codec::FJsonObjectPtr{};

        if (NOT ck_perf_lab_codec::TryGet_RootObject(InJson, Root))
        {
            return false;
        }

        auto State = ECk_PerfLab_RunState::Booting;

        if (NOT ck_perf_lab_codec::TryGet_EnumValue(Root->GetStringField(TEXT("state")), State))
        {
            return false;
        }

        auto Request = FCk_PerfLab_Request{};

        // Deliberately tolerant of a malformed request body: the summary exists to draw a row in a
        // list, and a session whose header is readable should still be listable.
        const auto& RequestObject = Root->GetObjectField(TEXT("request"));

        if (RequestObject.IsValid())
        {
            Request.Set_MapPath(RequestObject->GetStringField(TEXT("mapPath")));
        }

        OutSummary = FCk_PerfLab_Session{}
            .Set_SessionId(Root->GetStringField(TEXT("sessionId")))
            .Set_State(State)
            .Set_Request(Request);

        return true;
    }

    auto
        TryRead_SessionFromJson(
            const FString& InJson,
            FCk_PerfLab_Session& OutSession)
        -> bool
    {
        using namespace ck_perf_lab_codec;

        auto Root = FJsonObjectPtr{};

        if (NOT TryGet_RootObject(InJson, Root))
        {
            return false;
        }

        auto State = ECk_PerfLab_RunState::Booting;

        if (NOT TryGet_EnumValue(Root->GetStringField(TEXT("state")), State))
        {
            return false;
        }

        auto Request = FCk_PerfLab_Request{};

        if (NOT TryRead_Request(Root->GetObjectField(TEXT("request")), Request))
        {
            return false;
        }

        const auto& EnvironmentObject = Root->GetObjectField(TEXT("environment"));

        if (NOT EnvironmentObject.IsValid())
        {
            return false;
        }

        auto Environment = FCk_PerfLab_Environment{};
        Environment.Set_BuildConfiguration(EnvironmentObject->GetStringField(TEXT("buildConfiguration")))
                   .Set_InstanceMode(EnvironmentObject->GetStringField(TEXT("instanceMode")))
                   .Set_RhiName(EnvironmentObject->GetStringField(TEXT("rhiName")))
                   .Set_GpuBrand(EnvironmentObject->GetStringField(TEXT("gpuBrand")))
                   .Set_CpuBrand(EnvironmentObject->GetStringField(TEXT("cpuBrand")))
                   .Set_CpuCoreCount(EnvironmentObject->GetIntegerField(TEXT("cpuCoreCount")))
                   .Set_MachineName(EnvironmentObject->GetStringField(TEXT("machineName")))
                   .Set_EngineVersion(EnvironmentObject->GetStringField(TEXT("engineVersion")))
                   .Set_ChildBinary(EnvironmentObject->GetStringField(TEXT("childBinary")))
                   .Set_SourceControlDisabled(EnvironmentObject->GetBoolField(TEXT("sourceControlDisabled")))
                   .Set_StartedUtc(EnvironmentObject->GetStringField(TEXT("startedUtc")))
                   .Set_FinishedUtc(EnvironmentObject->GetStringField(TEXT("finishedUtc")));

        const auto& ViewportObject = EnvironmentObject->GetObjectField(TEXT("viewportSizeActual"));

        if (ViewportObject.IsValid())
        {
            Environment.Set_ViewportSizeActual(FIntPoint
            {
                ViewportObject->GetIntegerField(TEXT("width")),
                ViewportObject->GetIntegerField(TEXT("height"))
            });
        }

        const auto& CvarObject = EnvironmentObject->GetObjectField(TEXT("forcedCvars"));

        if (CvarObject.IsValid())
        {
            auto Cvars = TMap<FString, FString>{};

            for (const auto& Pair : CvarObject->Values)
            {
                Cvars.Add(Pair.Key, Pair.Value->AsString());
            }

            Environment.Set_ForcedCvars(Cvars);
        }

        auto Positions = TArray<FCk_PerfLab_PositionResult>{};

        const auto* PositionValues = static_cast<const TArray<TSharedPtr<FJsonValue>>*>(nullptr);

        if (Root->TryGetArrayField(TEXT("positions"), PositionValues))
        {
            for (const auto& PositionValue : *PositionValues)
            {
                const auto& PositionObject = PositionValue->AsObject();

                if (NOT PositionObject.IsValid())
                {
                    return false;
                }

                auto Position = FCk_PerfLab_PositionResult{};
                Position.Set_PositionId(PositionObject->GetStringField(TEXT("positionId")))
                        .Set_EyeHeightCm(PositionObject->GetNumberField(TEXT("eyeHeightCm")))
                        .Set_Revisited(PositionObject->GetBoolField(TEXT("revisited")));

                const auto& LocationObject = PositionObject->GetObjectField(TEXT("location"));

                if (NOT LocationObject.IsValid())
                {
                    return false;
                }

                Position.Set_Location(FVector
                {
                    LocationObject->GetNumberField(TEXT("x")),
                    LocationObject->GetNumberField(TEXT("y")),
                    LocationObject->GetNumberField(TEXT("z"))
                });

                auto Directions = TArray<FCk_PerfLab_DirectionResult>{};

                const auto* DirectionValues = static_cast<const TArray<TSharedPtr<FJsonValue>>*>(nullptr);

                if (PositionObject->TryGetArrayField(TEXT("directions"), DirectionValues))
                {
                    for (const auto& DirectionValue : *DirectionValues)
                    {
                        const auto& DirectionObject = DirectionValue->AsObject();

                        if (NOT DirectionObject.IsValid())
                        {
                            return false;
                        }

                        auto Metrics = FCk_PerfLab_MetricSet{};

                        if (NOT TryRead_MetricSet(DirectionObject->GetObjectField(TEXT("metrics")), Metrics))
                        {
                            return false;
                        }

                        Directions.Add(FCk_PerfLab_DirectionResult{}
                            .Set_YawDegrees(DirectionObject->GetNumberField(TEXT("yawDegrees")))
                            .Set_PitchDegrees(DirectionObject->GetNumberField(TEXT("pitchDegrees")))
                            .Set_Metrics(Metrics));
                    }
                }

                Position.Set_Directions(Directions);

                auto Aggregate = FCk_PerfLab_MetricSet{};

                if (NOT TryRead_MetricSet(PositionObject->GetObjectField(TEXT("aggregate")), Aggregate))
                {
                    return false;
                }

                Position.Set_Aggregate(Aggregate);

                const auto& ConfidenceObject = PositionObject->GetObjectField(TEXT("confidence"));

                if (NOT ConfidenceObject.IsValid())
                {
                    return false;
                }

                auto Rating = ECk_PerfLab_Confidence::High;

                if (NOT TryGet_EnumValue(ConfidenceObject->GetStringField(TEXT("rating")), Rating))
                {
                    return false;
                }

                auto Reasons = TArray<ECk_PerfLab_ConfidenceReason>{};

                const auto* ReasonValues = static_cast<const TArray<TSharedPtr<FJsonValue>>*>(nullptr);

                if (ConfidenceObject->TryGetArrayField(TEXT("reasons"), ReasonValues))
                {
                    for (const auto& ReasonValue : *ReasonValues)
                    {
                        auto Reason = ECk_PerfLab_ConfidenceReason::SettleTimedOut;

                        if (NOT TryGet_EnumValue(ReasonValue->AsString(), Reason))
                        {
                            return false;
                        }

                        Reasons.Add(Reason);
                    }
                }

                Position.Set_Confidence(FCk_PerfLab_ConfidenceResult{}.Set_Rating(Rating).Set_Reasons(Reasons));

                auto Census = TArray<FCk_PerfLab_ActorCensusRow>{};

                const auto* CensusValues = static_cast<const TArray<TSharedPtr<FJsonValue>>*>(nullptr);

                if (PositionObject->TryGetArrayField(TEXT("actorCensus"), CensusValues))
                {
                    for (const auto& CensusValue : *CensusValues)
                    {
                        const auto& RowObject = CensusValue->AsObject();

                        if (NOT RowObject.IsValid())
                        {
                            return false;
                        }

                        auto Row = FCk_PerfLab_ActorCensusRow{};
                        Row.Set_ObjectPath(RowObject->GetStringField(TEXT("objectPath")))
                           .Set_ClassName(RowObject->GetStringField(TEXT("className")))
                           .Set_DistanceCm(RowObject->GetNumberField(TEXT("distanceCm")));

                        auto Yaws = TArray<float>{};

                        const auto* YawValues = static_cast<const TArray<TSharedPtr<FJsonValue>>*>(nullptr);

                        if (RowObject->TryGetArrayField(TEXT("inViewYaws"), YawValues))
                        {
                            for (const auto& YawValue : *YawValues)
                            {
                                Yaws.Add(YawValue->AsNumber());
                            }
                        }

                        Row.Set_InViewYaws(Yaws);

                        const auto& ProxyObject = RowObject->GetObjectField(TEXT("costProxies"));

                        if (ProxyObject.IsValid())
                        {
                            Row.Set_TriangleCount(static_cast<int64>(ProxyObject->GetNumberField(TEXT("triangleCount"))))
                               .Set_MaterialSlotCount(ProxyObject->GetIntegerField(TEXT("materialSlotCount")))
                               .Set_LightCount(ProxyObject->GetIntegerField(TEXT("lightCount")))
                               .Set_NiagaraComponentCount(ProxyObject->GetIntegerField(TEXT("niagaraComponentCount")))
                               .Set_TickEnabled(ProxyObject->GetBoolField(TEXT("tickEnabled")))
                               .Set_CastsDynamicShadow(ProxyObject->GetBoolField(TEXT("castsDynamicShadow")))
                               .Set_HasCollision(ProxyObject->GetBoolField(TEXT("hasCollision")));
                        }

                        Census.Add(Row);
                    }
                }

                Position.Set_ActorCensus(Census);

                Positions.Add(Position);
            }
        }

        OutSession = FCk_PerfLab_Session{}
            .Set_SessionId(Root->GetStringField(TEXT("sessionId")))
            .Set_State(State)
            .Set_Request(Request)
            .Set_Environment(Environment)
            .Set_Positions(Positions);

        return true;
    }
}

// --------------------------------------------------------------------------------------------------------------------
