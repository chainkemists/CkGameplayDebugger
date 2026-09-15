#include "CkAudioDebugger/Data/CkAudioDebugger_DataCollector.h"

#include "CkAudio/AudioDirector/CkAudioDirector_Utils.h"
#include "CkAudio/AudioTrack/CkAudioTrack_Utils.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"

#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace ck_audio_debugger_data_collector_tests
{
    struct FWorldScope final
    {
        FWorldScope()
            : World{UWorld::CreateWorld(EWorldType::Game, false)}
        {}

        ~FWorldScope()
        {
            if (World.IsValid())
            { World->DestroyWorld(false); }
        }

        TStrongObjectPtr<UWorld> World;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkAudioDebugger_DataCollector_RealWorld,
    "Ck.AudioDebugger.Collector.RealWorld",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

auto FCkAudioDebugger_DataCollector_RealWorld::RunTest(const FString&) -> bool
{
    using namespace ck_audio_debugger_data_collector_tests;

    const auto WorldScope = FWorldScope{};
    auto* const World = WorldScope.World.Get();
    if (NOT TestNotNull(TEXT("real collector world is created"), World))
    { return false; }

    auto* const EcsWorld = World->GetSubsystem<UCk_EcsWorld_Subsystem_UE>();
    if (NOT TestNotNull(TEXT("real collector ECS world subsystem is created"), EcsWorld))
    { return false; }

    auto TransientEntity = EcsWorld->Get_TransientEntity();
    if (NOT TestTrue(TEXT("real collector ECS world has a transient entity"), ck::IsValid(TransientEntity)))
    { return false; }

    auto AlphaDirectorParams = FCk_Fragment_AudioDirector_ParamsData{}
        .Set_DefaultCrossfadeDuration(FCk_Time{0.75f})
        .Set_MaxConcurrentTracks(7)
        .Set_SamePriorityBehavior(ECk_SamePriorityBehavior::Allow);
    auto ZuluDirectorParams = FCk_Fragment_AudioDirector_ParamsData{}
        .Set_MaxConcurrentTracks(3)
        .Set_SamePriorityBehavior(ECk_SamePriorityBehavior::Block);

    auto AlphaDirector = UCk_Utils_AudioDirector_UE::Create(TransientEntity, AlphaDirectorParams);
    auto ZuluDirector = UCk_Utils_AudioDirector_UE::Create(TransientEntity, ZuluDirectorParams);
    UCk_Utils_Handle_UE::Set_DebugName(AlphaDirector, TEXT("Alpha Director"));
    UCk_Utils_Handle_UE::Set_DebugName(ZuluDirector, TEXT("Zulu Director"));

    if (NOT TestTrue(TEXT("production AudioDirector factories create both directors"),
        ck::IsValid(AlphaDirector) && ck::IsValid(ZuluDirector)))
    { return false; }

    const auto HighSound = TSoftObjectPtr<USoundBase>{
        FSoftObjectPath{TEXT("/Game/Audio/Collector/CollectorHigh.CollectorHigh")}};
    const auto LowSound = TSoftObjectPtr<USoundBase>{
        FSoftObjectPath{TEXT("/Game/Audio/Collector/CollectorLow.CollectorLow")}};
    auto HighTrackParams = FCk_Fragment_AudioTrack_ParamsData{HighSound}
        .Set_Priority(90)
        .Set_OverrideBehavior(ECk_AudioTrack_OverrideBehavior::Queue)
        .Set_LoopBehavior(ECk_LoopBehavior::PlayOnce);
    auto LowTrackParams = FCk_Fragment_AudioTrack_ParamsData{LowSound}
        .Set_Priority(10)
        .Set_OverrideBehavior(ECk_AudioTrack_OverrideBehavior::Interrupt)
        .Set_LoopBehavior(ECk_LoopBehavior::Loop);

    // Create the lower-priority entry first: the collector must not expose record insertion order.
    auto LowTrack = UCk_Utils_AudioTrack_UE::Create(AlphaDirector, LowTrackParams);
    auto HighTrack = UCk_Utils_AudioTrack_UE::Create(AlphaDirector, HighTrackParams);
    if (NOT TestTrue(TEXT("production AudioTrack factories create both tracks"),
        ck::IsValid(HighTrack) && ck::IsValid(LowTrack)))
    { return false; }

    FCkAudioDebugger_DataCollector Collector;
    Collector.Collect(World);
    const auto& Snapshot = Collector.Get_Snapshot();

    if (NOT TestTrue(TEXT("collector recognizes the real ECS world"), Snapshot.HasWorld))
    { return false; }

    if (NOT TestEqual(TEXT("collector visits both real AudioDirectors"), Snapshot.Directors.Num(), 2))
    { return false; }

    const auto& AlphaInfo = Snapshot.Directors[0];
    const auto& ZuluInfo = Snapshot.Directors[1];
    TestTrue(TEXT("directors are deterministically ordered by production debug name"),
        AlphaInfo.DirectorName == TEXT("Alpha Director") && ZuluInfo.DirectorName == TEXT("Zulu Director"));
    TestTrue(TEXT("director entity identities survive the production collector"),
        AlphaInfo.DirectorEntity.Get_Entity() == AlphaDirector.Get_Entity()
            && ZuluInfo.DirectorEntity.Get_Entity() == ZuluDirector.Get_Entity());
    TestTrue(TEXT("configured director policies are collected from public params"),
        AlphaInfo.MaxConcurrentTracks == 7
            && AlphaInfo.SamePriorityBehavior == ECk_SamePriorityBehavior::Allow
            && AlphaInfo.DefaultCrossfadeSeconds.IsSet()
            && FMath::IsNearlyEqual(AlphaInfo.DefaultCrossfadeSeconds.GetValue(), 0.75f)
            && ZuluInfo.MaxConcurrentTracks == 3
            && ZuluInfo.SamePriorityBehavior == ECk_SamePriorityBehavior::Block
            && NOT ZuluInfo.DefaultCrossfadeSeconds.IsSet());

    if (NOT TestEqual(TEXT("collector traverses the real director track record"), AlphaInfo.Tracks.Num(), 2))
    { return false; }

    const auto& HighInfo = AlphaInfo.Tracks[0];
    const auto& LowInfo = AlphaInfo.Tracks[1];
    TestTrue(TEXT("tracks are deterministically ordered by descending priority then name"),
        HighInfo.TrackName == TEXT("CollectorHigh") && HighInfo.Priority == 90
            && LowInfo.TrackName == TEXT("CollectorLow") && LowInfo.Priority == 10);
    TestTrue(TEXT("track identities and public params survive production collection"),
        HighInfo.TrackEntity.Get_Entity() == HighTrack.Get_Entity()
            && LowInfo.TrackEntity.Get_Entity() == LowTrack.Get_Entity()
            && HighInfo.OverrideBehavior == ECk_AudioTrack_OverrideBehavior::Queue
            && HighInfo.LoopBehavior == ECk_LoopBehavior::PlayOnce
            && LowInfo.OverrideBehavior == ECk_AudioTrack_OverrideBehavior::Interrupt
            && LowInfo.LoopBehavior == ECk_LoopBehavior::Loop);
    TestTrue(TEXT("new production tracks expose their public current defaults"),
        HighInfo.State == ECk_AudioTrack_State::Stopped
            && FMath::IsNearlyZero(HighInfo.CurrentVolume)
            && FMath::IsNearlyZero(HighInfo.TargetVolume)
            && FMath::IsNearlyZero(HighInfo.FadeSpeed)
            && FMath::IsNearlyZero(HighInfo.PlaybackPercent)
            && NOT HighInfo.IsVirtualized
            && NOT HighInfo.HasAudioComponent);
    TestTrue(TEXT("collector reports the soft sound path without forcing an asset load"),
        HighInfo.SoundPath == HighSound.ToSoftObjectPath().ToString()
            && LowInfo.SoundPath == LowSound.ToSoftObjectPath().ToString()
            && NOT HighSound.IsValid() && NOT LowSound.IsValid());

    Collector.Collect(nullptr);
    TestTrue(TEXT("Collect(nullptr) clears the previous output"),
        NOT Collector.Get_Snapshot().HasWorld && Collector.Get_Snapshot().Directors.IsEmpty());

    Collector.Collect(World);
    Collector.Reset();
    TestTrue(TEXT("Reset clears collected real-world output"),
        NOT Collector.Get_Snapshot().HasWorld && Collector.Get_Snapshot().Directors.IsEmpty());

    return true;
}

#endif
