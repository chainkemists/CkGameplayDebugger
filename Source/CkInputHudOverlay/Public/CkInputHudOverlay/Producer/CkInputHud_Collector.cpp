#include "CkInputHudOverlay/Producer/CkInputHud_Collector.h"

#include "CkInputHudOverlay/Model/CkInputHud_Model.h"
#include "CkInputHudOverlay/Settings/CkInputHud_Settings.h"

#include "CkDebuggerCommon/Devices/CkDebug_KeyActivityObserver.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/Handle/CkHandle.h"
#include "CkEcs/Handle/CkHandle_Utils.h"

#include "CkInput/CkInputButtonMap_Utils.h"
#include "CkInput/CkInputLayer_Fragment.h"
#include "CkInput/CkInputLayer_Utils.h"
#include "CkInput/CkInputSource_Utils.h"
#include "CkInput/Subsystem/CkInputSource_Subsystem.h"

#include "CkIntent/CkIntentMatcher_Fragment.h"
#include "CkIntent/CkIntentMatcher_Utils.h"
#include "CkIntent/CkIntentSampler_Utils.h"

#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_input_hud_collector
{
    constexpr auto UnroutedLabel = TEXT("unrouted");
    constexpr auto ConsumedLabel = TEXT("consumed");

    // ----------------------------------------------------------------------------------------------------------------

    struct FLayerRow
    {
        FCk_Handle_InputLayer    Layer;
        FCk_Handle_IntentMatcher Matcher;
        int32                    Priority = 0;
        bool                     IsGlobalActionLayer = false;
        FString                  DebugName;
    };

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Resolve_Source(
            UWorld* InWorld)
        -> FCk_Handle_InputSource
    {
        if (ck::Is_NOT_Valid(InWorld) || NOT InWorld->HasBegunPlay())
        { return {}; }

        auto* GameInstance = InWorld->GetGameInstance();
        if (ck::Is_NOT_Valid(GameInstance))
        { return {}; }

        // v1 is PLAYER 0 ONLY. Split-screen would need one HUD instance per local player and a per-player corner,
        // which the cvar surface deliberately does not express yet.
        auto* LocalPlayer = GameInstance->GetFirstGamePlayer();
        if (ck::Is_NOT_Valid(LocalPlayer))
        { return {}; }

        auto* SourceSubsystem = LocalPlayer->GetSubsystem<UCk_InputSource_Subsystem>();
        if (ck::Is_NOT_Valid(SourceSubsystem))
        { return {}; }

        return SourceSubsystem->Get_InputSource();
    }

    // ----------------------------------------------------------------------------------------------------------------

    // The layer stack, top-down. Read as a const registry view because CkInput exposes no stack enumeration Util —
    // the same gap CkIntentDebugger's collector documents.
    auto
        Gather_Layers(
            const FCk_Handle_InputSource& InSource)
        -> TArray<FLayerRow>
    {
        auto Rows = TArray<FLayerRow>{};

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

                auto Row = FLayerRow{};
                Row.Layer               = Layer;
                Row.Matcher             = UCk_Utils_IntentMatcher_UE::Cast(Handle);
                Row.Priority            = InParams.Get_Priority();
                Row.IsGlobalActionLayer = Row.Priority == GlobalActionPriority;
                Row.DebugName           = UCk_Utils_Handle_UE::Get_DebugName(Handle).ToString();

                // Request_CreateEntity stamps every entity "NO NAME" until someone renames it; the sentinel on a
                // HUD reads as a bug, and the priority beside it is the identity that always exists.
                if (Row.DebugName.IsEmpty() || Row.DebugName.StartsWith(TEXT("NO NAME")))
                { Row.DebugName = TEXT("Layer"); }

                Rows.Add(MoveTemp(Row));
            });

        Rows.Sort([](const FLayerRow& InA, const FLayerRow& InB)
        {
            return InA.Priority > InB.Priority;
        });

        return Rows;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_IsModifierKey(
            const FKey& InKey)
        -> bool
    {
        return InKey == EKeys::LeftShift   || InKey == EKeys::RightShift ||
               InKey == EKeys::LeftControl || InKey == EKeys::RightControl ||
               InKey == EKeys::LeftAlt     || InKey == EKeys::RightAlt;
    }

    // A chip is ~17-22px wide, so the label is a SHORT form rather than the engine display name. The table is
    // injective — the model addresses an open press by its label, so two keys sharing one would cross their
    // release edges.
    auto
        Make_KeyLabel(
            const FKey& InKey)
        -> FString
    {
        static const auto ShortNames = TMap<FKey, FString>
        {
            {EKeys::LeftShift,         TEXT("LShf")}, {EKeys::RightShift,        TEXT("RShf")},
            {EKeys::LeftControl,       TEXT("LCtl")}, {EKeys::RightControl,      TEXT("RCtl")},
            {EKeys::LeftAlt,           TEXT("LAlt")}, {EKeys::RightAlt,          TEXT("RAlt")},
            {EKeys::LeftMouseButton,   TEXT("LMB")},  {EKeys::RightMouseButton,  TEXT("RMB")},
            {EKeys::MiddleMouseButton, TEXT("MMB")},
            {EKeys::MouseScrollUp,     TEXT("WhUp")}, {EKeys::MouseScrollDown,   TEXT("WhDn")},
            {EKeys::SpaceBar,          TEXT("Spc")},  {EKeys::Enter,             TEXT("Ent")},
            {EKeys::BackSpace,         TEXT("Bksp")}, {EKeys::Escape,            TEXT("Esc")},
            {EKeys::Tab,               TEXT("Tab")},
            {EKeys::Up,                TEXT("Up")},   {EKeys::Down,              TEXT("Dn")},
            {EKeys::Left,              TEXT("Lt")},   {EKeys::Right,             TEXT("Rt")},
        };

        if (const auto* Found = ShortNames.Find(InKey))
        { return *Found; }

        // Everything else keeps its own short name: a letter/digit key is already one character, and a gamepad key
        // reads better as its FName than as the long localized display name.
        const auto Name = InKey.GetFName().ToString();

        constexpr auto GamepadPrefix = TEXT("Gamepad_");

        return Name.StartsWith(GamepadPrefix)
            ? Name.RightChop(FCString::Strlen(GamepadPrefix))
            : Name;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Refresh_LayerLine(
            const TArray<FLayerRow>& InLayers,
            FCk_InputHud_Model&      OutModel)
        -> void
    {
        auto Entries = TArray<TPair<int32, FString>>{};
        Entries.Reserve(InLayers.Num());

        for (const auto& Layer : InLayers)
        {
            // The global-action layer sits at int32-min by construction; printing that number would be noise on a
            // HUD, so it is named rather than ranked.
            auto Display = Layer.IsGlobalActionLayer
                ? FString{TEXT("GLOBAL")}
                : ck::Format_UE(TEXT("{}({})"), Layer.DebugName, Layer.Priority);

            Entries.Emplace(Layer.Priority, MoveTemp(Display));
        }

        OutModel.Set_Layers(Entries);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Refresh_Sticks(
            const FCkDebug_KeyActivityObserver* InObserver,
            FCk_InputHud_Model&                 OutModel)
        -> void
    {
        if (InObserver == nullptr)
        {
            OutModel.Set_LeftStick(FVector2f::ZeroVector);
            OutModel.Set_RightStick(FVector2f::ZeroVector);
            return;
        }

        OutModel.Set_LeftStick(FVector2f{
            InObserver->Get_AnalogMagnitude(EKeys::Gamepad_LeftX),
            InObserver->Get_AnalogMagnitude(EKeys::Gamepad_LeftY)});

        OutModel.Set_RightStick(FVector2f{
            InObserver->Get_AnalogMagnitude(EKeys::Gamepad_RightX),
            InObserver->Get_AnalogMagnitude(EKeys::Gamepad_RightY)});
    }

    // ----------------------------------------------------------------------------------------------------------------

    // The intent a press would complete, read off the matcher's baked resolution table. Read-only: the table is an
    // artifact the bake already produced, so nothing here re-resolves anything.
    auto
        TryGet_IntentNameForButton(
            const TArray<FLayerRow>&  InLayers,
            const FCk_Input_ButtonId& InButton)
        -> FString
    {
        for (const auto& Layer : InLayers)
        {
            if (ck::Is_NOT_Valid(Layer.Matcher))
            { continue; }

            if (NOT UCk_Utils_IntentMatcher_UE::Get_HasActiveSet(Layer.Matcher))
            { continue; }

            const auto& Current = Layer.Matcher.Get<ck::FFragment_IntentMatcher_Current>();
            const auto& Set     = Current.Get_ActiveSet();
            const auto& Intents = Set.Get_Intents();

            for (const auto& Resolution : Set.Get_ResolutionTable())
            {
                if (Resolution.Get_TerminalButton() != InButton)
                { continue; }

                for (const auto IntentIndex : Resolution.Get_IntentIndices())
                {
                    if (NOT Intents.IsValidIndex(IntentIndex))
                    { continue; }

                    // Most-dominant first is the order the bake settled; the first entry is the answer.
                    return Intents[IntentIndex].Get_Name().ToString();
                }
            }
        }

        return {};
    }

    auto
        TryGet_IntentNameForKey(
            const FCk_Handle_InputButtonMap& InMap,
            const TArray<FLayerRow>&         InLayers,
            const FKey&                      InKey)
        -> FString
    {
        if (ck::Is_NOT_Valid(InMap))
        { return {}; }

        for (const auto& Button : UCk_Utils_InputButtonMap_UE::Get_ButtonIdsForKey(InMap, InKey))
        {
            auto Name = TryGet_IntentNameForButton(InLayers, Button);

            if (NOT Name.IsEmpty())
            { return Name; }
        }

        return {};
    }

    auto
        TryGet_ConsumerName(
            const TArray<FLayerRow>&     InLayers,
            const FCk_Handle_InputLayer& InConsumer)
        -> FString
    {
        for (const auto& Layer : InLayers)
        {
            if (Layer.Layer != InConsumer)
            { continue; }

            return Layer.IsGlobalActionLayer ? FString{TEXT("GLOBAL")} : Layer.DebugName;
        }

        return {};
    }

    // ----------------------------------------------------------------------------------------------------------------

    // One frame's worth of routed events, key-level: a press opens a chip, a release closes the newest open chip
    // for that key. Repeated presses of one key are independent chips by construction — nothing here merges them.
    auto
        Consume_RoutedEvents(
            const TArray<FCk_InputLayer_RoutedEvent>& InRoutedEvents,
            const FCk_Handle_InputButtonMap&          InMap,
            const TArray<FLayerRow>&                  InLayers,
            int32                                     InFrameIndex,
            double                                    InNowSeconds,
            FCk_InputHud_Model&                       OutModel)
        -> void
    {
        for (const auto& Routed : InRoutedEvents)
        {
            const auto& Event = Routed.Get_Event();
            const auto& Key   = Event.Get_Key();

            if (NOT Key.IsValid())
            { continue; }

            const auto Label = Make_KeyLabel(Key);

            switch (Event.Get_EventType())
            {
                case ECk_InputSource_EventType::Pressed:
                {
                    const auto Index = OutModel.Open_Event(
                        Label, Key.GetFName(), InFrameIndex, InNowSeconds, Get_IsModifierKey(Key));

                    const auto IntentName = TryGet_IntentNameForKey(InMap, InLayers, Key);

                    if (NOT IntentName.IsEmpty())
                    {
                        // An intent name is the most specific thing we can say about a press, so it wins over the
                        // layer that consumed it.
                        OutModel.Set_EventResolution(Index, IntentName, true);
                        break;
                    }

                    if (Routed.Get_Outcome() == ECk_InputLayer_DeliveryOutcome::ConsumedByLayer)
                    {
                        const auto ConsumerName = TryGet_ConsumerName(InLayers, Routed.Get_ConsumingLayer());

                        OutModel.Set_EventResolution(
                            Index, ConsumerName.IsEmpty() ? FString{ConsumedLabel} : ConsumerName, true);
                        break;
                    }

                    OutModel.Set_EventResolution(Index, FString{UnroutedLabel}, false);
                    break;
                }
                case ECk_InputSource_EventType::Released:
                {
                    const auto Index = OutModel.TryGet_OpenEvent(Label);

                    if (Index == INDEX_NONE)
                    { break; }

                    OutModel.Close_Event(Index, InFrameIndex, InNowSeconds);
                    break;
                }
                case ECk_InputSource_EventType::AnalogAxis:
                default:
                { break; }
            }
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Refresh_Events(
            const FCk_Handle_InputSource&    InSource,
            const FCk_Handle_InputButtonMap& InMap,
            const TArray<FLayerRow>&         InLayers,
            double                           InNowSeconds,
            FCk_InputHud_Model&              OutModel)
        -> void
    {
        auto SourceHandle = InSource.ConvertToHandle();

        const auto Sampler = UCk_Utils_IntentSampler_UE::Cast(SourceHandle);

        if (ck::Is_NOT_Valid(Sampler))
        {
            // Fallback for a source with no sampler: the router's per-frame retention. It is cleared at the top of
            // every routing pass, so a ticker callback that lands on a frame the player pressed nothing reads
            // empty — accepted, because the alternative is composing a sampler onto production state from a debug
            // HUD. There is no frame record to name here, hence INDEX_NONE and an empty frame deck.
            Consume_RoutedEvents(
                UCk_Utils_InputLayer_UE::Get_RoutedEventsThisFrame(InSource),
                InMap, InLayers, INDEX_NONE, InNowSeconds, OutModel);
            return;
        }

        const auto FrameCount = UCk_Utils_IntentSampler_UE::Get_FrameCount(Sampler);
        const auto LastSeen   = OutModel.Get_LastSeenSamplerFrame();

        auto NewestSeen = LastSeen;

        // Walk oldest-first over the ring so a burst of catch-up rows lands in press order. Bounded by the ring's
        // own capacity — there is nothing older to read.
        for (auto Offset = FrameCount - 1; Offset >= 0; --Offset)
        {
            const auto Record = UCk_Utils_IntentSampler_UE::TryGet_FrameAtOffset(Sampler, Offset);

            const auto FrameIndex = Record.Get_FrameIndex();

            if (FrameIndex < 0 || FrameIndex <= LastSeen)
            { continue; }

            Consume_RoutedEvents(Record.Get_RoutedEvents(), InMap, InLayers, FrameIndex, InNowSeconds, OutModel);

            NewestSeen = FMath::Max(NewestSeen, FrameIndex);
        }

        OutModel.Set_LastSeenSamplerFrame(NewestSeen);
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCk_InputHud_Collector::
    Collect(
        const FCk_InputHud_CollectParams& InParams,
        FCk_InputHud_Model&               OutModel)
    -> void
{
    using namespace ck_input_hud_collector;

    const auto HistoryCap   = UCk_InputHud_Settings::Get_HistoryCap();
    const auto FadeLifetime = UCk_InputHud_Settings::Get_FadeLifetimeSeconds();

    const auto Source = Resolve_Source(InParams.World);

    if (ck::Is_NOT_Valid(Source))
    {
        OutModel.Set_HasSource(false);
        OutModel.Reset_VolatileState(InParams.NowSeconds);
        Refresh_Sticks(InParams.Observer, OutModel);
        OutModel.Prune_FadedEvents(InParams.NowSeconds, FadeLifetime);
        return;
    }

    OutModel.Set_HasSource(true);

    auto SourceHandle = Source.ConvertToHandle();
    const auto ButtonMap = UCk_Utils_InputButtonMap_UE::Cast(SourceHandle);

    const auto Layers = Gather_Layers(Source);

    Refresh_LayerLine(Layers, OutModel);
    Refresh_Sticks(InParams.Observer, OutModel);

    // Neither the frame record nor the raw event it carries holds a timestamp — the whole pipeline is indexed in
    // logic FRAMES — so every edge this pass consumes is stamped with the pass's own wall clock. Under sampler
    // catch-up several rows are claimed in one pass and share that stamp, so a hold's duration can be under-read
    // by up to one tick. The frame deck is the exact reading; the bar and the seconds are the readable one.
    Refresh_Events(Source, ButtonMap, Layers, InParams.NowSeconds, OutModel);

    // A routed release is NOT guaranteed (viewport-focus flush emits none; within-frame ordering can orphan a
    // rapid click's press) — cross-check every open chip against the observer's physical key state and close
    // what the hardware no longer holds. The observer's own tick releases everything on app deactivation, so
    // an alt-tab mid-hold resolves through this same path on the next collect.
    if (InParams.Observer != nullptr)
    {
        OutModel.Close_OpenEventsNotHeld(
            [&](const FName& InKeyName) { return InParams.Observer->Get_IsHeld(FKey{InKeyName}); },
            InParams.NowSeconds);
    }

    OutModel.Enforce_HistoryCap(HistoryCap);
    OutModel.Prune_FadedEvents(InParams.NowSeconds, FadeLifetime);
}

// --------------------------------------------------------------------------------------------------------------------
