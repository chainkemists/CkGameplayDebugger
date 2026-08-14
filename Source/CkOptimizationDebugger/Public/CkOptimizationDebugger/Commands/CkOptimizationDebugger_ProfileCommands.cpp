#include "CkOptimizationDebugger/Commands/CkOptimizationDebugger_ProfileCommands.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "Engine/Engine.h"
#include "Misc/OutputDeviceRedirector.h"

#if WITH_EDITOR
#include "Editor.h"
#include "LevelEditorViewport.h"
#endif

// --------------------------------------------------------------------------------------------------------------------

// Named rather than anonymous: this module compiles unity, and a same-named anonymous helper in another .cpp would
// collide in the merged translation unit.
namespace ck_optimization_debugger_profile_commands_impl
{
    auto
        Make_Failure(
            const FString& InMessage)
        -> FCkOptimizationDebugger_ProfileCommandResult
    {
        auto Result = FCkOptimizationDebugger_ProfileCommandResult{};
        Result.DidRun = false;
        Result.Message = InMessage;

        return Result;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Make_Success(
            const FString& InMessage)
        -> FCkOptimizationDebugger_ProfileCommandResult
    {
        auto Result = FCkOptimizationDebugger_ProfileCommandResult{};
        Result.DidRun = true;
        Result.Message = InMessage;

        return Result;
    }

#if WITH_EDITOR
    // ----------------------------------------------------------------------------------------------------------------
    // The one thing this page remembers, and why
    // ----------------------------------------------------------------------------------------------------------------

    /** Which viewport client this tool raised real time on, or null when it has not.
     *
     *  The page's rule is that on/off state is READ off the viewport and never recorded — that rule is about which
     *  entries are ACTIVE, and it still holds. This is a different thing: an undo record for a side effect. Enabling
     *  a stat needs the viewport in real time to draw it, `FEditorViewportClient::SetRealtime` writes a value the
     *  editor persists BETWEEN SESSIONS, and a tool that raised it and never lowered it left the reader's viewport
     *  running in real time forever with nothing on screen saying why.
     *
     *  Held as a bare pointer and NEVER dereferenced through — it is only ever compared against the viewport client
     *  the current call already has in hand. A client that was destroyed and replaced at the same address is the
     *  worst case, and it costs a missed restore, not a crash. */
    FEditorViewportClient* g_RealtimeRaisedOnClient = nullptr;

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Note_RealtimeRaised(
            FEditorViewportClient* InClient)
        -> void
    {
        if (InClient == nullptr)
        { return; }

        g_RealtimeRaisedOnClient = InClient;
        InClient->SetRealtime(true);
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** Whether ANY catalog stat overlay is still on. What decides that the last one just went off — asked over the
     *  catalog rather than over a set this page keeps, so a stat the reader enabled from the console counts too. */
    auto
        Has_AnyCatalogStatEnabled(
            FEditorViewportClient* InClient)
        -> bool
    {
        if (InClient == nullptr)
        { return false; }

        for (const auto& Command : ck_optimization_debugger_profile_commands::Get_AllCommands())
        {
            if (Command.Lever != ECkOptimizationDebugger_ProfileLever::EngineStat)
            { continue; }

            if (InClient->IsStatEnabled(Command.StatName))
            { return true; }
        }

        return false;
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** Puts real time back if THIS tool is the thing that raised it. Returns whether it did.
     *
     *  A reader who had the viewport in real time before they ever opened this page keeps it: the raise is only
     *  recorded when `IsRealtime()` was false at the moment the first overlay went on. */
    auto
        TryRestore_Realtime(
            FEditorViewportClient* InClient)
        -> bool
    {
        if (InClient == nullptr || g_RealtimeRaisedOnClient != InClient)
        { return false; }

        g_RealtimeRaisedOnClient = nullptr;

        if (NOT InClient->IsRealtime())
        { return false; }

        InClient->SetRealtime(false);

        return true;
    }
#endif

    // ----------------------------------------------------------------------------------------------------------------

    /** A stat overlay. `ExecEngineStat` is the one call that serves both the engine stats (fps / unit / unitgraph,
     *  matched against `UEngine::EngineStats`) and the plain stat GROUPS (game / rhi / …, matched against the
     *  registered `STATGROUP_` names) — so one entry kind covers both and nothing here has to know which is which. */
    auto
        Make_StatCommand(
            const TCHAR* InIdSuffix,
            const TCHAR* InDisplayName,
            const TCHAR* InStatName,
            const TCHAR* InDescription)
        -> FCkOptimizationDebugger_ProfileCommand
    {
        auto Command = FCkOptimizationDebugger_ProfileCommand{};

        Command.Id           = FName{*ck::Format_UE(TEXT("Timing.{}"), InIdSuffix)};
        Command.DisplayName  = FString{InDisplayName};
        Command.Description  = FString{InDescription};
        Command.Kind         = ECkOptimizationDebugger_ProfileCommandKind::Toggle;
        Command.Group        = ECkOptimizationDebugger_ProfileGroup::Timing;
        Command.Lever        = ECkOptimizationDebugger_ProfileLever::EngineStat;
        Command.StatName     = FString{InStatName};
        Command.ConsoleCommand = ck::Format_UE(TEXT("stat {}"), InStatName);

        return Command;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Make_ViewModeCommand(
            const TCHAR* InIdSuffix,
            const TCHAR* InDisplayName,
            EViewModeIndex InViewModeIndex,
            const TCHAR* InDescription)
        -> FCkOptimizationDebugger_ProfileCommand
    {
        auto Command = FCkOptimizationDebugger_ProfileCommand{};

        Command.Id            = FName{*ck::Format_UE(TEXT("ViewMode.{}"), InIdSuffix)};
        Command.DisplayName   = FString{InDisplayName};
        Command.Description   = FString{InDescription};
        Command.Kind          = ECkOptimizationDebugger_ProfileCommandKind::ViewMode;
        Command.Group         = ECkOptimizationDebugger_ProfileGroup::ViewModes;
        Command.Lever         = ECkOptimizationDebugger_ProfileLever::ViewMode;
        Command.ViewModeIndex = InViewModeIndex;

        return Command;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Make_VisualizationCommand(
            ECkOptimizationDebugger_ProfileGroup InGroup,
            ECkOptimizationDebugger_ProfileLever InLever,
            EViewModeIndex InViewModeIndex,
            const TCHAR* InIdPrefix,
            const TCHAR* InDisplayName,
            const TCHAR* InVisualizationMode,
            const TCHAR* InDescription)
        -> FCkOptimizationDebugger_ProfileCommand
    {
        auto Command = FCkOptimizationDebugger_ProfileCommand{};

        Command.Id                = FName{*ck::Format_UE(TEXT("{}.{}"), InIdPrefix, InVisualizationMode)};
        Command.DisplayName       = FString{InDisplayName};
        Command.Description       = FString{InDescription};
        Command.Kind              = ECkOptimizationDebugger_ProfileCommandKind::ViewMode;
        Command.Group             = InGroup;
        Command.Lever             = InLever;
        Command.ViewModeIndex     = InViewModeIndex;
        Command.VisualizationMode = FName{InVisualizationMode};

        return Command;
    }

    // ----------------------------------------------------------------------------------------------------------------

    /** The catalog, built once. Every command string, view-mode index and visualization mode name below was read out
     *  of this engine's own source rather than remembered — the module CLAUDE.md's profiling section carries the
     *  file:line for each family, and names the modes that were deliberately left out and why. */
    auto
        Build_Catalog()
        -> TArray<FCkOptimizationDebugger_ProfileCommand>
    {
        auto Commands = TArray<FCkOptimizationDebugger_ProfileCommand>{};

        // ---- Timing ----
        Commands.Add(Make_StatCommand(TEXT("Fps"), TEXT("FPS"), TEXT("fps"),
            TEXT("Frames per second and the frame time behind it. Free — one number drawn per frame")));

        Commands.Add(Make_StatCommand(TEXT("Unit"), TEXT("Unit"), TEXT("unit"),
            TEXT("Frame / Game / Draw / GPU / RHIT split — the first thing to read, because it says which thread ")
            TEXT("the frame is waiting on. Free")));

        Commands.Add(Make_StatCommand(TEXT("UnitGraph"), TEXT("Unit graph"), TEXT("unitgraph"),
            TEXT("The same split drawn as a rolling graph, so a spike is visible after it has passed. Cheap, but it ")
            TEXT("draws over a good part of the viewport")));

        Commands.Add(Make_StatCommand(TEXT("Game"), TEXT("Game"), TEXT("game"),
            TEXT("Game-thread cost broken down by the engine's own tick groups. Moderate — the stats system has to ")
            TEXT("gather every scope in the group")));

        Commands.Add(Make_StatCommand(TEXT("SceneRendering"), TEXT("Scene rendering"), TEXT("scenerendering"),
            TEXT("Render-thread breakdown: draw calls, primitives, and the passes they went through. Moderate")));

        Commands.Add(Make_StatCommand(TEXT("Rhi"), TEXT("RHI"), TEXT("rhi"),
            TEXT("What the RHI is holding: draw calls, triangles, render-target and texture memory. Moderate")));

        Commands.Add(Make_StatCommand(TEXT("InitViews"), TEXT("Init views"), TEXT("initviews"),
            TEXT("Visibility and culling cost — the answer to \"why is the render thread busy before it draws ")
            TEXT("anything\". Moderate")));

        Commands.Add(Make_StatCommand(TEXT("Streaming"), TEXT("Streaming"), TEXT("streaming"),
            TEXT("Texture and mesh streaming pool sizes, wanted-versus-resident mips, and streaming requests in ")
            TEXT("flight. Moderate")));

        // ---- GPU ----
        {
            auto Command = FCkOptimizationDebugger_ProfileCommand{};

            Command.Id             = FName{TEXT("Gpu.ProfileGpu")};
            Command.DisplayName    = FString{TEXT("Profile GPU")};
            Command.Description    = FString{
                TEXT("Captures ONE frame on the GPU and opens the per-pass breakdown. Expensive and deliberately ")
                TEXT("one-shot: the frame it captures stalls, so the timings it prints are that frame's, not the ")
                TEXT("session's average")};
            Command.Kind           = ECkOptimizationDebugger_ProfileCommandKind::OneShot;
            Command.Group          = ECkOptimizationDebugger_ProfileGroup::Gpu;
            Command.Lever          = ECkOptimizationDebugger_ProfileLever::ConsoleCommand;
            Command.ConsoleCommand = FString{TEXT("profilegpu")};

            Commands.Add(MoveTemp(Command));
        }

        // ---- Viewport view modes ----
        Commands.Add(Make_ViewModeCommand(TEXT("Lit"), TEXT("Lit"), VMI_Lit,
            TEXT("The normal view. This is the way BACK from every visualizer on this page — they are all one ")
            TEXT("exclusive choice, because a viewport has exactly one view mode")));

        Commands.Add(Make_ViewModeCommand(TEXT("LightComplexity"), TEXT("Light complexity"), VMI_LightComplexity,
            TEXT("How many dynamic lights affect each pixel, green through red. Free to display — it replaces the ")
            TEXT("shading rather than adding to it")));

        Commands.Add(Make_ViewModeCommand(TEXT("ShaderComplexity"), TEXT("Shader complexity"), VMI_ShaderComplexity,
            TEXT("Estimated pixel-shader instruction cost per pixel. Green is cheap, white is far past budget. Free ")
            TEXT("to display")));

        Commands.Add(Make_ViewModeCommand(TEXT("QuadOverdraw"), TEXT("Quad overdraw"), VMI_QuadOverdraw,
            TEXT("How many times the GPU shaded each 2x2 quad — the cost of thin triangles and dense geometry. ")
            TEXT("Expensive: it re-renders the scene with an instrumented shader")));

        Commands.Add(Make_ViewModeCommand(TEXT("LightmapDensity"), TEXT("Lightmap density"), VMI_LightmapDensity,
            TEXT("Texel density of baked lighting, as a coloured checker. Free to display, and the honest way to ")
            TEXT("find the one wall carrying a 1024 lightmap")));

        Commands.Add(Make_ViewModeCommand(TEXT("StationaryLightOverlap"), TEXT("Stationary light overlap"),
            VMI_StationaryLightOverlap,
            TEXT("Where more stationary lights overlap than the engine can bake channels for — those revert to fully ")
            TEXT("dynamic. Free to display")));

        // ---- Nanite ----
        //
        // `VMI_VisualizeNanite` is what `FEditorViewportClient::ChangeNaniteVisualizationMode` itself sets, and
        // recording it here is what makes "is this visualizer the one showing" answerable without asking the
        // engine a second question.
        {
            const auto AddNanite = [&Commands](const TCHAR* InDisplayName, const TCHAR* InMode,
                const TCHAR* InDescription) -> void
            {
                Commands.Add(Make_VisualizationCommand(ECkOptimizationDebugger_ProfileGroup::Nanite,
                    ECkOptimizationDebugger_ProfileLever::NaniteVisualization, VMI_VisualizeNanite,
                    TEXT("Nanite"), InDisplayName, InMode, InDescription));
            };

            AddNanite(TEXT("Overview"), TEXT("Overview"),
                TEXT("Several Nanite views tiled at once — the place to start when you do not yet know which one you ")
                TEXT("want. Moderate: it renders every tile"));

            AddNanite(TEXT("Mask"), TEXT("Mask"),
                TEXT("Which pixels Nanite is actually rasterizing. The answer to \"is this mesh Nanite at all\". Cheap"));

            AddNanite(TEXT("Triangles"), TEXT("Triangles"),
                TEXT("Per-pixel triangle identity — dense noise means Nanite is drawing sub-pixel triangles here. Cheap"));

            AddNanite(TEXT("Clusters"), TEXT("Clusters"),
                TEXT("Cluster identity, i.e. the LOD granularity Nanite chose. Large flat patches mean it culled ")
                TEXT("aggressively. Cheap"));

            AddNanite(TEXT("Overdraw"), TEXT("Overdraw"),
                TEXT("How many times Nanite rasterized each pixel. The one to reach for when Nanite geometry is ")
                TEXT("costing more than its triangle count suggests. Moderate"));
        }

        // ---- Lumen ----
        {
            const auto AddLumen = [&Commands](const TCHAR* InDisplayName, const TCHAR* InMode,
                const TCHAR* InDescription) -> void
            {
                Commands.Add(Make_VisualizationCommand(ECkOptimizationDebugger_ProfileGroup::Lumen,
                    ECkOptimizationDebugger_ProfileLever::LumenVisualization, VMI_VisualizeLumen,
                    TEXT("Lumen"), InDisplayName, InMode, InDescription));
            };

            AddLumen(TEXT("Overview"), TEXT("Overview"),
                TEXT("Every Lumen view tiled at once. Moderate: it renders each tile"));

            AddLumen(TEXT("Performance overview"), TEXT("PerformanceOverview"),
                TEXT("The Lumen views that are about COST rather than correctness, tiled together. Moderate"));

            AddLumen(TEXT("Lumen scene"), TEXT("LumenScene"),
                TEXT("The simplified scene representation Lumen actually gathers light from — where it disagrees ")
                TEXT("with the real geometry, the GI is wrong for a reason you can see. Moderate"));

            AddLumen(TEXT("Reflection view"), TEXT("ReflectionView"),
                TEXT("What Lumen reflections are tracing against. Moderate"));
        }

        // ---- Virtual shadow maps ----
        {
            // All three use the default view-mode index of `AddVisualizationMode`
            // (VirtualShadowMapVisualizationData.h:92). The one registered mode that does NOT — `casters`, which
            // takes VMI_ShadowCasters — is deliberately absent; see the module CLAUDE.md's omission note.
            const auto AddVsm = [&Commands](const TCHAR* InDisplayName, const TCHAR* InMode,
                const TCHAR* InDescription) -> void
            {
                Commands.Add(Make_VisualizationCommand(ECkOptimizationDebugger_ProfileGroup::VirtualShadowMaps,
                    ECkOptimizationDebugger_ProfileLever::VirtualShadowMapVisualization,
                    VMI_VisualizeVirtualShadowMap, TEXT("Vsm"), InDisplayName, InMode, InDescription));
            };

            AddVsm(TEXT("Shadow mask"), TEXT("mask"),
                TEXT("The final shadow mask shading reads. The sanity check: if this is wrong, nothing further down ")
                TEXT("is worth reading. Cheap"));

            AddVsm(TEXT("Clipmap / mip level"), TEXT("mip"),
                TEXT("Which clipmap level (directional) or mip (local) each pixel resolved to — resolution actually ")
                TEXT("spent, rather than resolution requested. Cheap"));

            AddVsm(TEXT("Cached page"), TEXT("cache"),
                TEXT("Green where a shadow page was reused, red where it had to be re-rendered. Red everywhere is ")
                TEXT("what an invalidating light looks like. Cheap"));
        }

        return Commands;
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck_optimization_debugger_profile_commands
{
    auto
        Get_AllCommands()
        -> const TArray<FCkOptimizationDebugger_ProfileCommand>&
    {
        // Built once, on first use. A namespace-scope object would be a static initializer, and `FName` construction
        // during static init is exactly the ordering hazard this codebase keeps out of headers.
        static const auto Commands = ck_optimization_debugger_profile_commands_impl::Build_Catalog();

        return Commands;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_AllGroups()
        -> TArray<ECkOptimizationDebugger_ProfileGroup>
    {
        return TArray<ECkOptimizationDebugger_ProfileGroup>{
            ECkOptimizationDebugger_ProfileGroup::Timing,
            ECkOptimizationDebugger_ProfileGroup::Gpu,
            ECkOptimizationDebugger_ProfileGroup::ViewModes,
            ECkOptimizationDebugger_ProfileGroup::Nanite,
            ECkOptimizationDebugger_ProfileGroup::Lumen,
            ECkOptimizationDebugger_ProfileGroup::VirtualShadowMaps};
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_CommandsInGroup(
            ECkOptimizationDebugger_ProfileGroup InGroup)
        -> TArray<FCkOptimizationDebugger_ProfileCommand>
    {
        auto Commands = TArray<FCkOptimizationDebugger_ProfileCommand>{};

        // A filter over one ordered table, never a sort: the catalog's order IS the answer, so there is no tie-break
        // to get wrong and two calls cannot come back different.
        for (const auto& Command : Get_AllCommands())
        {
            if (Command.Group != InGroup)
            { continue; }

            Commands.Add(Command);
        }

        return Commands;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        TryGet_Command(
            FName InId)
        -> const FCkOptimizationDebugger_ProfileCommand*
    {
        for (const auto& Command : Get_AllCommands())
        {
            if (Command.Id == InId)
            { return &Command; }
        }

        return nullptr;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_GroupLabel(
            ECkOptimizationDebugger_ProfileGroup InGroup)
        -> FString
    {
        switch (InGroup)
        {
            case ECkOptimizationDebugger_ProfileGroup::Timing:            return FString{TEXT("Timing stats")};
            case ECkOptimizationDebugger_ProfileGroup::Gpu:               return FString{TEXT("GPU")};
            case ECkOptimizationDebugger_ProfileGroup::ViewModes:         return FString{TEXT("Viewport visualizers")};
            case ECkOptimizationDebugger_ProfileGroup::Nanite:            return FString{TEXT("Nanite")};
            case ECkOptimizationDebugger_ProfileGroup::Lumen:             return FString{TEXT("Lumen")};
            case ECkOptimizationDebugger_ProfileGroup::VirtualShadowMaps: return FString{TEXT("Virtual shadow maps")};
            default:                                                     return FString{TEXT("Profiling")};
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_GroupSubText(
            ECkOptimizationDebugger_ProfileGroup InGroup)
        -> FString
    {
        switch (InGroup)
        {
            case ECkOptimizationDebugger_ProfileGroup::Timing:
                return FString{TEXT("on-screen overlays — they stay up until you switch them off")};

            case ECkOptimizationDebugger_ProfileGroup::Gpu:
                return FString{TEXT("one captured frame, not a running measurement")};

            case ECkOptimizationDebugger_ProfileGroup::ViewModes:
                return FString{TEXT("one exclusive choice — Lit is the way back from any of them")};

            case ECkOptimizationDebugger_ProfileGroup::Nanite:
                return FString{TEXT("what Nanite rasterized, and how hard it worked")};

            case ECkOptimizationDebugger_ProfileGroup::Lumen:
                return FString{TEXT("what Lumen gathers light from, not what the level contains")};

            case ECkOptimizationDebugger_ProfileGroup::VirtualShadowMaps:
                return FString{TEXT("resolution spent and pages re-rendered")};

            default:
                return FString{};
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_CommandSummary(
            const FCkOptimizationDebugger_ProfileCommand& InCommand)
        -> FString
    {
        switch (InCommand.Lever)
        {
            case ECkOptimizationDebugger_ProfileLever::EngineStat:
            case ECkOptimizationDebugger_ProfileLever::ConsoleCommand:
                return InCommand.ConsoleCommand;

            case ECkOptimizationDebugger_ProfileLever::ViewMode:
                return ck::Format_UE(TEXT("view mode: {}"), InCommand.DisplayName);

            case ECkOptimizationDebugger_ProfileLever::NaniteVisualization:
                return ck::Format_UE(TEXT("Nanite visualization: {}"), InCommand.VisualizationMode);

            case ECkOptimizationDebugger_ProfileLever::LumenVisualization:
                return ck::Format_UE(TEXT("Lumen visualization: {}"), InCommand.VisualizationMode);

            case ECkOptimizationDebugger_ProfileLever::VirtualShadowMapVisualization:
                return ck::Format_UE(TEXT("virtual shadow map visualization: {}"), InCommand.VisualizationMode);

            default:
                return InCommand.DisplayName;
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Is_CommandActive(
            const FCkOptimizationDebugger_ProfileCommand& InCommand,
            const FCkOptimizationDebugger_ProfileViewportState& InState)
        -> bool
    {
        switch (InCommand.Lever)
        {
            case ECkOptimizationDebugger_ProfileLever::EngineStat:
                // `FString` comparison is case-insensitive, which is what makes "fps" match the "FPS" the engine's
                // own stat menu enables — the two surfaces must agree about one overlay being on.
                return InState.EnabledStats.Contains(InCommand.StatName);

            case ECkOptimizationDebugger_ProfileLever::ConsoleCommand:
                // A one-shot is never "on". It happened, or it did not.
                return false;

            case ECkOptimizationDebugger_ProfileLever::ViewMode:
                return InState.ViewModeIndex == InCommand.ViewModeIndex;

            case ECkOptimizationDebugger_ProfileLever::NaniteVisualization:
                // BOTH halves. The mode name outlives the view mode on the viewport client — switching back to Lit
                // leaves it set — so the name alone would report a visualizer nobody is looking at as still on.
                return InState.ViewModeIndex == InCommand.ViewModeIndex &&
                       InState.NaniteVisualizationMode == InCommand.VisualizationMode;

            case ECkOptimizationDebugger_ProfileLever::LumenVisualization:
                return InState.ViewModeIndex == InCommand.ViewModeIndex &&
                       InState.LumenVisualizationMode == InCommand.VisualizationMode;

            case ECkOptimizationDebugger_ProfileLever::VirtualShadowMapVisualization:
                return InState.ViewModeIndex == InCommand.ViewModeIndex &&
                       InState.VirtualShadowMapVisualizationMode == InCommand.VisualizationMode;

            default:
                return false;
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_ActiveCommandCount(
            const FCkOptimizationDebugger_ProfileViewportState& InState)
        -> int32
    {
        static const auto k_LitCommandId = FName{TEXT("ViewMode.Lit")};

        auto Count = 0;

        for (const auto& Command : Get_AllCommands())
        {
            if (Command.Id == k_LitCommandId)
            { continue; }

            if (Is_CommandActive(Command, InState))
            { ++Count; }
        }

        return Count;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        TryGet_ActiveViewportCommandId(
            const FCkOptimizationDebugger_ProfileViewportState& InState)
        -> FName
    {
        for (const auto& Command : Get_AllCommands())
        {
            if (Command.Kind != ECkOptimizationDebugger_ProfileCommandKind::ViewMode)
            { continue; }

            if (Is_CommandActive(Command, InState))
            { return Command.Id; }
        }

        return NAME_None;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Push_RecentCommand(
            TArray<FString>& InOutRecent,
            const FString& InCommand,
            int32 InMaxEntries)
        -> void
    {
        const auto Trimmed = InCommand.TrimStartAndEnd();

        if (Trimmed.IsEmpty() || InMaxEntries <= 0)
        { return; }

        // Re-running a command MOVES it to the front rather than adding a second copy: a history whose five slots are
        // the same command five times is a history that has forgotten the other four.
        InOutRecent.RemoveAll([&Trimmed](const FString& InExisting) -> bool
        {
            return InExisting.Equals(Trimmed, ESearchCase::IgnoreCase);
        });

        InOutRecent.Insert(Trimmed, 0);

        while (InOutRecent.Num() > InMaxEntries)
        { InOutRecent.Pop(); }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_CanExecute()
        -> bool
    {
#if WITH_EDITOR
        return GEditor != nullptr && GCurrentLevelEditingViewportClient != nullptr;
#else
        // The module ships in packaged Development/DebugGame builds, where there is no editor level viewport to point
        // a stat overlay or a view mode at. Disabled buttons plus a hint beat live buttons that quietly do nothing.
        return false;
#endif
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_ViewportState()
        -> FCkOptimizationDebugger_ProfileViewportState
    {
        auto State = FCkOptimizationDebugger_ProfileViewportState{};

#if WITH_EDITOR
        auto* Client = GCurrentLevelEditingViewportClient;

        if (Client == nullptr)
        { return State; }

        State.ViewModeIndex = Client->GetViewMode();

        if (const auto* EnabledStats = Client->GetEnabledStats())
        { State.EnabledStats = *EnabledStats; }

        // The three visualization mode names are not exposed by a getter, so they are recovered through the engine's
        // own "is this one selected" predicates over the catalog. That keeps this in step with what the viewport
        // menu itself considers selected, including the view-mode half of the question.
        for (const auto& Command : Get_AllCommands())
        {
            switch (Command.Lever)
            {
                case ECkOptimizationDebugger_ProfileLever::NaniteVisualization:
                {
                    if (Client->IsNaniteVisualizationModeSelected(Command.VisualizationMode))
                    { State.NaniteVisualizationMode = Command.VisualizationMode; }

                    break;
                }
                case ECkOptimizationDebugger_ProfileLever::LumenVisualization:
                {
                    if (Client->IsLumenVisualizationModeSelected(Command.VisualizationMode))
                    { State.LumenVisualizationMode = Command.VisualizationMode; }

                    break;
                }
                case ECkOptimizationDebugger_ProfileLever::VirtualShadowMapVisualization:
                {
                    if (Client->IsVirtualShadowMapVisualizationModeSelected(Command.VisualizationMode))
                    { State.VirtualShadowMapVisualizationMode = Command.VisualizationMode; }

                    break;
                }
                default:
                    break;
            }
        }
#endif

        return State;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_IsCommandActive(
            const FCkOptimizationDebugger_ProfileCommand& InCommand)
        -> bool
    {
#if WITH_EDITOR
        auto* Client = GCurrentLevelEditingViewportClient;

        if (Client == nullptr)
        { return false; }

        // The stat case short-circuits deliberately. It is the SAME rule `Is_CommandActive` applies — the viewport
        // client's `IsStatEnabled` is a `Contains` over the very array the state struct would carry — but asked
        // directly it does not copy that array once per button, per paint.
        if (InCommand.Lever == ECkOptimizationDebugger_ProfileLever::EngineStat)
        { return Client->IsStatEnabled(InCommand.StatName); }

        auto State = FCkOptimizationDebugger_ProfileViewportState{};
        State.ViewModeIndex = Client->GetViewMode();

        switch (InCommand.Lever)
        {
            case ECkOptimizationDebugger_ProfileLever::NaniteVisualization:
            {
                if (Client->IsNaniteVisualizationModeSelected(InCommand.VisualizationMode))
                { State.NaniteVisualizationMode = InCommand.VisualizationMode; }

                break;
            }
            case ECkOptimizationDebugger_ProfileLever::LumenVisualization:
            {
                if (Client->IsLumenVisualizationModeSelected(InCommand.VisualizationMode))
                { State.LumenVisualizationMode = InCommand.VisualizationMode; }

                break;
            }
            case ECkOptimizationDebugger_ProfileLever::VirtualShadowMapVisualization:
            {
                if (Client->IsVirtualShadowMapVisualizationModeSelected(InCommand.VisualizationMode))
                { State.VirtualShadowMapVisualizationMode = InCommand.VisualizationMode; }

                break;
            }
            default:
                break;
        }

        return Is_CommandActive(InCommand, State);
#else
        (void)InCommand;
        return false;
#endif
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_ActiveCommandCountLive()
        -> int32
    {
        static const auto k_LitCommandId = FName{TEXT("ViewMode.Lit")};

        auto Count = 0;

        for (const auto& Command : Get_AllCommands())
        {
            // Lit is the resting state — see `Get_ActiveCommandCount` for why a badge that counted it could never
            // be cleared.
            if (Command.Id == k_LitCommandId)
            { continue; }

            if (Get_IsCommandActive(Command))
            { ++Count; }
        }

        return Count;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        TryGet_ActiveViewportCommandIdLive()
        -> FName
    {
#if WITH_EDITOR
        auto* Client = GCurrentLevelEditingViewportClient;

        if (Client == nullptr)
        { return NAME_None; }

        for (const auto& Command : Get_AllCommands())
        {
            if (Command.Lever == ECkOptimizationDebugger_ProfileLever::EngineStat ||
                Command.Lever == ECkOptimizationDebugger_ProfileLever::ConsoleCommand)
            { continue; }

            if (Get_IsCommandActive(Command))
            { return Command.Id; }
        }

        return NAME_None;
#else
        return NAME_None;
#endif
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Execute_Command(
            const FCkOptimizationDebugger_ProfileCommand& InCommand)
        -> FCkOptimizationDebugger_ProfileCommandResult
    {
        using namespace ck_optimization_debugger_profile_commands_impl;

#if WITH_EDITOR
        if (GEditor == nullptr)
        { return Make_Failure(TEXT("No editor session — profiling actions drive an editor viewport.")); }

        auto* Client = GCurrentLevelEditingViewportClient;

        if (Client == nullptr)
        {
            return Make_Failure(TEXT("No level editor viewport to drive. Click in a level viewport first — that is ")
                TEXT("the one these actions apply to."));
        }

        auto* World = GEditor->GetEditorWorldContext().World();

        switch (InCommand.Lever)
        {
            case ECkOptimizationDebugger_ProfileLever::EngineStat:
            {
                if (GEngine == nullptr)
                { return Make_Failure(TEXT("No engine to toggle a stat on.")); }

                // The same call the editor's own Show > Stats menu makes. It parks the viewport client in the
                // stats system's "who asked" slot and then execs `STAT <name>`, which is what makes an engine stat
                // and a plain stat group one action here instead of two.
                GEngine->ExecEngineStat(World, Client, *InCommand.StatName);

                const auto NowEnabled = Client->IsStatEnabled(InCommand.StatName);

                auto RestoredRealtime = false;

                if (NowEnabled)
                {
                    // A stat that is enabled but invisible is indistinguishable from one that did not turn on. The
                    // viewport only draws stats while it is showing them AND running in real time, so turning the
                    // overlay on turns those on with it — and says so, because both are viewport state the reader
                    // did not ask for directly.
                    Client->SetShowStats(true);

                    // Raised only if it was not already up, and REMEMBERED so the last stat going off can put it
                    // back. `SetRealtime` writes a value the editor persists between sessions, so a tool that raised
                    // it and never lowered it left every future session running the viewport in real time with
                    // nothing on screen explaining why.
                    if (NOT Client->IsRealtime())
                    { Note_RealtimeRaised(Client); }
                }
                else if (NOT Has_AnyCatalogStatEnabled(Client))
                {
                    // The last overlay just went off. Real time was ours to lower only if it was ours to raise.
                    RestoredRealtime = TryRestore_Realtime(Client);
                }

                Client->Invalidate();

                if (NowEnabled)
                {
                    return Make_Success(ck::Format_UE(
                        TEXT("Ran: {} — on (viewport set to real time so it updates)"),
                        Get_CommandSummary(InCommand)));
                }

                return Make_Success(RestoredRealtime
                    ? ck::Format_UE(TEXT("Ran: {} — off (viewport put back out of real time)"),
                        Get_CommandSummary(InCommand))
                    : ck::Format_UE(TEXT("Ran: {} — off"), Get_CommandSummary(InCommand)));
            }

            case ECkOptimizationDebugger_ProfileLever::ConsoleCommand:
            {
                if (GEngine == nullptr)
                { return Make_Failure(TEXT("No engine to run a console command on.")); }

                // Null between map transitions, and a number of exec handlers dereference it without checking.
                if (World == nullptr)
                {
                    return Make_Failure(TEXT("No editor world to run this against — wait for the map to finish ")
                        TEXT("loading."));
                }

                if (NOT GEngine->Exec(World, *InCommand.ConsoleCommand, *GLog))
                {
                    return Make_Failure(ck::Format_UE(TEXT("Nothing claimed {} — it may not be available in this build."),
                        Get_CommandSummary(InCommand)));
                }

                return Make_Success(ck::Format_UE(TEXT("Ran: {}"), Get_CommandSummary(InCommand)));
            }

            case ECkOptimizationDebugger_ProfileLever::ViewMode:
            {
                Client->SetViewMode(InCommand.ViewModeIndex);
                Client->Invalidate();

                return Make_Success(ck::Format_UE(TEXT("Ran: {}"), Get_CommandSummary(InCommand)));
            }

            case ECkOptimizationDebugger_ProfileLever::NaniteVisualization:
            {
                // Sets the view mode AND the mode name in one call — the console variable alone would leave the
                // viewport on whatever it was showing.
                Client->ChangeNaniteVisualizationMode(InCommand.VisualizationMode);
                Client->Invalidate();

                return Make_Success(ck::Format_UE(TEXT("Ran: {}"), Get_CommandSummary(InCommand)));
            }

            case ECkOptimizationDebugger_ProfileLever::LumenVisualization:
            {
                Client->ChangeLumenVisualizationMode(InCommand.VisualizationMode);
                Client->Invalidate();

                return Make_Success(ck::Format_UE(TEXT("Ran: {}"), Get_CommandSummary(InCommand)));
            }

            case ECkOptimizationDebugger_ProfileLever::VirtualShadowMapVisualization:
            {
                Client->ChangeVirtualShadowMapVisualizationMode(InCommand.VisualizationMode);
                Client->Invalidate();

                return Make_Success(ck::Format_UE(TEXT("Ran: {}"), Get_CommandSummary(InCommand)));
            }

            default:
                return Make_Failure(TEXT("Unknown profiling lever."));
        }
#else
        return Make_Failure(ck::Format_UE(
            TEXT("{} needs an editor viewport — this build has none."), Get_CommandSummary(InCommand)));
#endif
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Execute_CustomCommand(
            const FString& InCommand)
        -> FCkOptimizationDebugger_ProfileCommandResult
    {
        using namespace ck_optimization_debugger_profile_commands_impl;

        const auto Trimmed = InCommand.TrimStartAndEnd();

        if (Trimmed.IsEmpty())
        { return Make_Failure(TEXT("Nothing to run.")); }

#if WITH_EDITOR
        if (GEditor == nullptr || GEngine == nullptr)
        { return Make_Failure(TEXT("No editor session to run a console command in.")); }

        auto* World = GEditor->GetEditorWorldContext().World();

        // Null between map transitions, and a number of exec handlers dereference it without checking.
        if (World == nullptr)
        {
            return Make_Failure(TEXT("No editor world to run a console command against — wait for the map to finish ")
                TEXT("loading."));
        }

        // Deliberately NOT routed through the viewport client: an arbitrary string is not a stat and not a view
        // mode, and pretending otherwise would silently swallow every command that is neither.
        const auto WasHandled = GEngine->Exec(World, *Trimmed, *GLog);

        if (WasHandled)
        { return Make_Success(ck::Format_UE(TEXT("Ran: {}"), Trimmed)); }

        // A command nobody claimed is a FAILURE, not a success with a footnote. Reporting `DidRun` here toned the
        // status strip Ok and pushed a typo'd cvar into the RECENT rail as though it had worked — the rail is a list
        // of things worth running again, and a name the engine rejected is not one of them.
        return Make_Failure(ck::Format_UE(
            TEXT("Nothing claimed \"{}\" — check the name in the output log. Nothing ran."), Trimmed));
#else
        return Make_Failure(ck::Format_UE(TEXT("{} needs an editor session — this build has none."), Trimmed));
#endif
    }
}

// --------------------------------------------------------------------------------------------------------------------
