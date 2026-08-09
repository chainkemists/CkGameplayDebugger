#include "CkIntentDebugger/Data/CkIntentDebugger_DataCollector.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/Handle/CkHandle.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"

#include "CkInput/CkInputButtonMap_Utils.h"
#include "CkInput/CkInputLayer_Fragment.h"
#include "CkInput/CkInputLayer_Utils.h"
#include "CkInput/CkInputSource_Utils.h"
#include "CkInput/Subsystem/CkInputSource_Subsystem.h"

#include "CkIntent/CkIntentGrammar_Utils.h"
#include "CkIntent/CkIntentMatcher_Fragment.h"
#include "CkIntent/CkIntentMatcher_Utils.h"
#include "CkIntent/CkIntentSampler_Fragment.h"
#include "CkIntent/CkIntentSampler_Utils.h"

#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_intent_debugger_collector
{
    auto
        Get_AtomLabel(
            const FCk_Intent_CompiledAtom& InAtom)
        -> FString
    {
        if (InAtom.Get_Kind() == ECk_Intent_AtomKind::Direction)
        { return ck::intent_debugger::Get_Label(InAtom.Get_Direction()); }

        return InAtom.Get_Button().Get_Name().ToString();
    }

    auto
        Get_StepLabel(
            const FCk_Intent_CompiledStep& InStep)
        -> FString
    {
        auto Parts = TArray<FString>{};
        for (const auto& Atom : InStep.Get_Atoms())
        { Parts.Add(Get_AtomLabel(Atom)); }

        return FString::Join(Parts, TEXT("+"));
    }

    // The compiled steps read back as notation. This is a RENDERING of the artifact the bake produced, not a
    // re-parse: the atoms are already resolved, and nothing here consults the grammar to decide what they mean.
    auto
        Get_Notation(
            const FCk_Intent_CompiledIntent& InIntent)
        -> FString
    {
        auto Parts = TArray<FString>{};
        for (const auto& Step : InIntent.Get_Steps())
        { Parts.Add(Get_StepLabel(Step)); }

        auto Notation = FString::Join(Parts, TEXT(" "));

        if (InIntent.Get_WindowFrames() > 0)
        { Notation += ck::Format_UE(TEXT(" w={}"), InIntent.Get_WindowFrames()); }

        if (InIntent.Get_HoldFrames() > 0)
        { Notation += ck::Format_UE(TEXT(" hold={}"), InIntent.Get_HoldFrames()); }

        if (InIntent.Get_Lenience() == ECk_Intent_Lenience::Lenient)
        { Notation += TEXT(" lenient"); }

        return Notation;
    }

    auto
        Get_TerminalLabel(
            const FCk_Intent_CompiledIntent& InIntent)
        -> FString
    {
        const auto& Steps = InIntent.Get_Steps();
        if (Steps.IsEmpty())
        { return {}; }

        return Get_StepLabel(Steps.Last());
    }

    auto
        Collect_Buttons(
            const TArray<FCk_Input_ButtonId>& InButtons,
            const FCk_Handle_InputButtonMap& InMap,
            const TArray<FCk_Input_ButtonId>& InPressed,
            const TArray<FCk_Input_ButtonId>& InReleased)
        -> TArray<FCkIntentDebugger_ButtonState>
    {
        auto Rows = TArray<FCkIntentDebugger_ButtonState>{};
        Rows.Reserve(InButtons.Num());

        for (const auto& Button : InButtons)
        {
            auto Row = FCkIntentDebugger_ButtonState{};
            Row.Label = ck::intent_debugger::Get_Label(Button);
            Row.Tier = Button.Get_Tier();
            Row.WentDown = InPressed.Contains(Button);
            Row.WentUp = InReleased.Contains(Button);

            if (ck::IsValid(InMap))
            { Row.Key = UCk_Utils_InputButtonMap_UE::TryGet_KeyForButton(InMap, Button); }

            Rows.Add(MoveTemp(Row));
        }

        return Rows;
    }

    auto
        Collect_Frames(
            const FCk_Handle_IntentSampler& InSampler,
            const FCk_Handle_InputButtonMap& InMap,
            const TArray<FCkIntentDebugger_LayerRow>& InLayers,
            int32 InMaxFrames)
        -> TArray<FCkIntentDebugger_FrameRow>
    {
        auto Rows = TArray<FCkIntentDebugger_FrameRow>{};

        const auto FrameCount = UCk_Utils_IntentSampler_UE::Get_FrameCount(InSampler);
        const auto Wanted = FMath::Min(FrameCount, InMaxFrames);

        Rows.Reserve(Wanted);

        for (auto Offset = Wanted - 1; Offset >= 0; --Offset)
        {
            const auto Record = UCk_Utils_IntentSampler_UE::TryGet_FrameAtOffset(InSampler, Offset);
            if (Record.Get_FrameIndex() < 0)
            { continue; }

            auto Row = FCkIntentDebugger_FrameRow{};
            Row.FrameIndex = Record.Get_FrameIndex();
            Row.AxisX = Record.Get_ConditionedAxisX();
            Row.AxisY = Record.Get_ConditionedAxisY();
            Row.Octant = Record.Get_Octant();
            Row.CleanedHorizontal = Record.Get_CleanedHorizontal();
            Row.CleanedVertical = Record.Get_CleanedVertical();
            Row.Held = Collect_Buttons(Record.Get_Held(), InMap, Record.Get_Pressed(), Record.Get_Released());

            for (const auto& Routed : Record.Get_RoutedEvents())
            {
                ++Row.NumRoutedEvents;

                switch (Routed.Get_Outcome())
                {
                    case ECk_InputLayer_DeliveryOutcome::ConsumedByLayer:
                    {
                        ++Row.NumConsumed;

                        const auto& Consumer = Routed.Get_ConsumingLayer();
                        for (const auto& Layer : InLayers)
                        {
                            if (Layer.Layer != Consumer)
                            { continue; }

                            Row.ConsumingLayerPriorities.AddUnique(Layer.Priority);
                            break;
                        }
                        break;
                    }
                    case ECk_InputLayer_DeliveryOutcome::PassedThrough:
                    {
                        ++Row.NumPassedThrough;
                        break;
                    }
                    case ECk_InputLayer_DeliveryOutcome::DroppedNoOwner:
                    default:
                    {
                        ++Row.NumDropped;
                        break;
                    }
                }
            }

            Rows.Add(MoveTemp(Row));
        }

        return Rows;
    }

    auto
        Collect_MatcherDetail(
            const FCk_Handle_IntentMatcher& InMatcher,
            const FCk_Handle_InputButtonMap& InMap,
            FCkIntentDebugger_LayerRow& OutRow)
        -> void
    {
        const auto& Current = InMatcher.Get<ck::FFragment_IntentMatcher_Current>();
        const auto& Set = Current.Get_ActiveSet();
        const auto& Intents = Set.Get_Intents();

        OutRow.ChordWindowFrames = Set.Get_ChordWindowFrames();

        const auto& PhaseRows = Current.Get_PhaseRows();

        OutRow.Intents.Reserve(Intents.Num());
        for (auto Index = 0; Index < Intents.Num(); ++Index)
        {
            const auto& Intent = Intents[Index];

            auto Row = FCkIntentDebugger_IntentRow{};
            Row.Name = Intent.Get_Name();
            Row.Tag = Intent.Get_IntentTag();
            Row.Priority = Intent.Get_Priority();
            Row.WindowFrames = Intent.Get_WindowFrames();
            Row.HoldFrames = Intent.Get_HoldFrames();
            Row.Lenience = Intent.Get_Lenience();
            Row.Notation = Get_Notation(Intent);
            Row.TerminalLabel = Get_TerminalLabel(Intent);

            if (PhaseRows.IsValidIndex(Index))
            {
                const auto& PhaseRow = PhaseRows[Index];
                Row.Phase = PhaseRow.Get_Phase();
                Row.PhaseFrame = PhaseRow.Get_PhaseFrame();

                const auto& Claimant = PhaseRow.Get_ClaimedBy();
                Row.IsClaimed = ck::IsValid(Claimant);
                if (Row.IsClaimed)
                { Row.ClaimedBy = UCk_Utils_Handle_UE::Get_DebugName(Claimant).ToString(); }
            }

            Row.CompletionFrame = UCk_Utils_IntentMatcher_UE::TryGet_CompletionFrame_ByName(InMatcher, Row.Name);

            OutRow.Intents.Add(MoveTemp(Row));
        }

        for (const auto& Episode : Current.Get_PendingEpisodes())
        {
            auto Row = FCkIntentDebugger_EpisodeRow{};
            Row.ButtonLabel = ck::intent_debugger::Get_Label(Episode.Get_Button());
            Row.PressFrame = Episode.Get_PressFrame();
            Row.ChordWindowFrames = Episode.Get_ChordWindowFrames();
            Row.HoldSiblingFrames = Episode.Get_HoldSiblingFrames();
            Row.ChordCauseArmed = Episode.Get_ChordCauseArmed();
            Row.HoldCauseArmed = Episode.Get_HoldCauseArmed();

            for (const auto CandidateIndex : Episode.Get_Candidates())
            {
                if (NOT Intents.IsValidIndex(CandidateIndex))
                { continue; }

                Row.CandidateNames.Add(Intents[CandidateIndex].Get_Name());
            }

            OutRow.Episodes.Add(MoveTemp(Row));
        }

        for (const auto& Accumulator : Current.Get_HoldAccumulators())
        {
            auto Row = FCkIntentDebugger_HoldRow{};
            Row.ButtonLabel = ck::intent_debugger::Get_Label(Accumulator.Get_Button());
            Row.HeldFrames = Accumulator.Get_HeldFrames();

            OutRow.Holds.Add(MoveTemp(Row));
        }

        for (const auto& Resolution : Set.Get_ResolutionTable())
        {
            const auto& Terminal = Resolution.Get_TerminalButton();

            auto Row = FCkIntentDebugger_ResolutionRow{};
            Row.TerminalLabel = ck::intent_debugger::Get_Label(Terminal);
            Row.Tier = Terminal.Get_Tier();

            if (ck::IsValid(InMap))
            { Row.ResolvedKey = UCk_Utils_InputButtonMap_UE::TryGet_KeyForButton(InMap, Terminal); }

            for (const auto IntentIndex : Resolution.Get_IntentIndices())
            {
                if (NOT Intents.IsValidIndex(IntentIndex))
                { continue; }

                Row.IntentNames.Add(Intents[IntentIndex].Get_Name());
            }

            const auto Verdict = UCk_Utils_IntentGrammar_UE::Get_DeferralVerdict(Set, Terminal);
            Row.HoldSiblingFrames = Verdict.Get_HoldSiblingFrames();
            Row.ChordMemberFrames = Verdict.Get_ChordMemberFrames();

            OutRow.Resolutions.Add(MoveTemp(Row));
        }

        OutRow.ScanDiagnostics = UCk_Utils_IntentMatcher_UE::Get_ScanDiagnostics(InMatcher);
    }

    auto
        Collect_Layers(
            const FCk_Handle_InputSource& InSource,
            const FCk_Handle_InputButtonMap& InMap)
        -> TArray<FCkIntentDebugger_LayerRow>
    {
        auto Rows = TArray<FCkIntentDebugger_LayerRow>{};

        auto Context = InSource.ConvertToHandle();
        const auto GlobalActionPriority = UCk_Utils_InputLayer_UE::Get_GlobalActionPriority();

        Context.View<ck::FFragment_InputLayer_Params>().ForEach(
            [&](FCk_Entity InEntity, const ck::FFragment_InputLayer_Params& InParams)
            {
                if (InParams.Get_InputSource() != InSource)
                { return; }

                auto Handle = ck::MakeHandle(InEntity, Context);
                if (ck::Is_NOT_Valid(Handle))
                { return; }

                const auto Layer = UCk_Utils_InputLayer_UE::Cast(Handle);
                if (ck::Is_NOT_Valid(Layer))
                { return; }

                auto Row = FCkIntentDebugger_LayerRow{};
                Row.Layer = Layer;
                Row.Priority = InParams.Get_Priority();
                Row.IsGlobalActionLayer = Row.Priority == GlobalActionPriority;
                Row.DebugName = UCk_Utils_Handle_UE::Get_DebugName(Handle).ToString();

                for (const auto& Capture : UCk_Utils_InputLayer_UE::Get_Captures(Layer))
                {
                    auto CaptureRow = FCkIntentDebugger_CaptureRow{};
                    CaptureRow.MatchMode = Capture.Get_MatchMode();
                    CaptureRow.Key = Capture.Get_Key();
                    CaptureRow.Behavior = Capture.Get_Behavior();

                    Row.Captures.Add(MoveTemp(CaptureRow));
                }

                const auto Matcher = UCk_Utils_IntentMatcher_UE::Cast(Handle);
                if (ck::IsValid(Matcher))
                {
                    Row.Matcher = Matcher;
                    Row.HasMatcher = true;
                    Row.HasActiveSet = UCk_Utils_IntentMatcher_UE::Get_HasActiveSet(Matcher);
                    Row.ActiveIntentCount = UCk_Utils_IntentMatcher_UE::Get_ActiveIntentCount(Matcher);
                    Row.RegisteredCaptureKeys = UCk_Utils_IntentMatcher_UE::Get_RegisteredCaptureKeys(Matcher);

                    const auto& Params = Handle.Get<ck::FFragment_IntentMatcher_Params>();
                    Row.MatcherCaptureBehavior = Params.Get_CaptureBehavior();
                    Row.LatchDecayFrames = Params.Get_LatchDecayFrames();

                    Collect_MatcherDetail(Matcher, InMap, Row);
                }

                Rows.Add(MoveTemp(Row));
            });

        // Stack order, top-down. Priority is unique per source, so this is a total order and never reshuffles
        // between refreshes.
        Rows.Sort([](const FCkIntentDebugger_LayerRow& InA, const FCkIntentDebugger_LayerRow& InB)
        {
            return InA.Priority > InB.Priority;
        });

        return Rows;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkIntentDebugger_DataCollector::
    Collect(
        UWorld* InWorld,
        int32 InMaxRecordedFrames)
    -> FCkIntentDebugger_Snapshot
{
    auto Snapshot = FCkIntentDebugger_Snapshot{};

    if (ck::Is_NOT_Valid(InWorld) || NOT InWorld->HasBegunPlay())
    { return Snapshot; }

    const auto TransientEntity = UCk_Utils_EcsWorld_Subsystem_UE::Get_TransientEntity(InWorld);
    if (ck::Is_NOT_Valid(TransientEntity))
    { return Snapshot; }

    auto* GameInstance = InWorld->GetGameInstance();
    if (GameInstance == nullptr)
    { return Snapshot; }

    Snapshot.HasWorld = true;
    Snapshot.ScanDiagnosticsEnabled = UCk_Utils_IntentMatcher_UE::Get_ScanDiagnosticsEnabled();

    for (auto* LocalPlayer : GameInstance->GetLocalPlayers())
    {
        if (LocalPlayer == nullptr)
        { continue; }

        auto* SourceSubsystem = LocalPlayer->GetSubsystem<UCk_InputSource_Subsystem>();
        if (SourceSubsystem == nullptr)
        { continue; }

        const auto Source = SourceSubsystem->Get_InputSource();
        if (ck::Is_NOT_Valid(Source))
        { continue; }

        auto SourceHandle = Source.ConvertToHandle();

        auto Entry = FCkIntentDebugger_SourceSnapshot{};
        Entry.Source = Source;
        Entry.LocalPlayerIndex = UCk_Utils_InputSource_UE::Get_LocalPlayerIndex(Source);
        Entry.Label = ck::Format_UE(TEXT("Player {}"), Entry.LocalPlayerIndex);

        const auto ButtonMap = UCk_Utils_InputButtonMap_UE::Cast(SourceHandle);
        Entry.HasButtonMap = ck::IsValid(ButtonMap);

        Entry.Layers = ck_intent_debugger_collector::Collect_Layers(Source, ButtonMap);

        const auto Sampler = UCk_Utils_IntentSampler_UE::Cast(SourceHandle);
        if (ck::IsValid(Sampler))
        {
            Entry.Sampler = Sampler;
            Entry.HasSampler = true;
            Entry.FrameCount = UCk_Utils_IntentSampler_UE::Get_FrameCount(Sampler);

            const auto& SamplerParams = SourceHandle.Get<ck::FFragment_IntentSampler_Params>();
            Entry.RingCapacity = SamplerParams.Get_RingCapacity();
            Entry.OctantNeutralRadius = SamplerParams.Get_OctantNeutralRadius();
            Entry.OctantHysteresisMarginDegrees = SamplerParams.Get_OctantHysteresisMarginDegrees();

            Entry.Frames = ck_intent_debugger_collector::Collect_Frames(
                Sampler, ButtonMap, Entry.Layers, InMaxRecordedFrames);
        }

        Snapshot.Sources.Add(MoveTemp(Entry));
    }

    return Snapshot;
}

// --------------------------------------------------------------------------------------------------------------------
