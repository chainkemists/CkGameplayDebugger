#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

// Deliberate red contract: Common owns neutral interaction sequencing; feature
// adapters resolve identities and execute feature-specific selection/drag
// behavior. This header does not exist until the router is implemented.
#include "CkDebuggerCommon/Viewport/CkDebug3dInteractionRouter.h"

namespace ck_debug_3d_interaction_router_spec
{
constexpr auto TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

struct FFakeAdapter final : ICkDebug3dInteractionAdapter
{
    virtual auto
    TryHit(const FCkDebug3dCursorRay&) -> TOptional<FCkDebug3dInteractionHit> override
    {
        ++_HitQueries;
        return _NextHit;
    }
    virtual auto
    Select(uint64 InIdentity, bool InAdditive) -> void override
    {
        _Selections.Add({InIdentity, InAdditive});
    }
    virtual auto
    ArmDrag(const FCkDebug3dInteractionHit& InHit) -> void override
    {
        ++_DragArms;
        _LastDragHit = InHit;
    }
    virtual auto
    UpdateDrag(const FCkDebug3dCursorRay&) -> void override
    {
        ++_DragUpdates;
    }
    virtual auto
    ReleaseDrag() -> void override
    {
        ++_DragReleases;
    }
    virtual auto
    ShiftDragPlane(float InDirection) -> void override
    {
        _PlaneShifts.Add(InDirection);
    }
    virtual auto
    SetHover(TOptional<uint64> InIdentity) -> void override
    {
        _Hover = InIdentity;
        ++_HoverWrites;
    }
    virtual auto
    Command(ECkDebug3dNeutralCommand InCommand) -> void override
    {
        _Commands.Add(InCommand);
    }
    virtual auto
    Command_AtRay(const FCkDebug3dCursorRay& InRay) -> void override
    {
        _RayCommands.Add(InRay._Origin);
    }

    TOptional<FCkDebug3dInteractionHit> _NextHit = FCkDebug3dInteractionHit{101, FVector{10.0, 0.0, 0.0}, 25.0f};
    int32 _HitQueries = 0;
    int32 _DragArms = 0;
    int32 _DragUpdates = 0;
    int32 _DragReleases = 0;
    int32 _HoverWrites = 0;
    FCkDebug3dInteractionHit _LastDragHit;
    TOptional<uint64> _Hover;
    TArray<TPair<uint64, bool>> _Selections;
    TArray<float> _PlaneShifts;
    TArray<ECkDebug3dNeutralCommand> _Commands;
    TArray<FVector> _RayCommands;
};

auto
MakeRouter(const TSharedPtr<FFakeAdapter>& InAdapter) -> FCkDebug3dInteractionRouter
{
    return FCkDebug3dInteractionRouter{
        InAdapter, FCkDebug3dInteractionConfig{}.Set_ClickMovementThresholdPixels(4.0f).Set_HoverThrottleSeconds(0.06)};
}
} // namespace ck_debug_3d_interaction_router_spec

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebug3dInteractionRouter_PlainClickRelease,
                                 "Ck.DebuggerCommon.Viewport3d.Interaction.PlainClickRelease",
                                 ck_debug_3d_interaction_router_spec::TestFlags)
auto
    FCkDebug3dInteractionRouter_PlainClickRelease::
    RunTest(const FString&)
    -> bool
{
    using namespace ck_debug_3d_interaction_router_spec;
    const auto Adapter = MakeShared<FFakeAdapter>();
    auto Router = MakeRouter(Adapter);
    Router.OnPointerPressed(ECkDebug3dPointerButton::Left, FVector2D{10, 10}, {});
    Router.OnPointerReleased(ECkDebug3dPointerButton::Left, FVector2D{12, 12}, {});
    TestEqual(TEXT("plain click resolves only on release"), Adapter->_HitQueries, 1);
    TestEqual(TEXT("plain click replaces selection"), Adapter->_Selections.Num(), 1);
    Router.OnPointerPressed(ECkDebug3dPointerButton::Left, FVector2D::ZeroVector, {});
    Router.OnPointerReleased(ECkDebug3dPointerButton::Left, FVector2D{5, 0}, {});
    TestEqual(TEXT("movement beyond threshold suppresses pick"), Adapter->_HitQueries, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebug3dInteractionRouter_CtrlDragUsesOneHit,
                                 "Ck.DebuggerCommon.Viewport3d.Interaction.CtrlDragUsesOneHit",
                                 ck_debug_3d_interaction_router_spec::TestFlags)
auto
    FCkDebug3dInteractionRouter_CtrlDragUsesOneHit::
    RunTest(const FString&)
    -> bool
{
    using namespace ck_debug_3d_interaction_router_spec;
    const auto Adapter = MakeShared<FFakeAdapter>();
    auto Router = MakeRouter(Adapter);
    auto Ctrl = FCkDebug3dInteractionModifiers{};
    Ctrl._Control = true;
    Router.OnPointerPressed(ECkDebug3dPointerButton::Left, FVector2D::ZeroVector, Ctrl);
    TestEqual(TEXT("Ctrl press performs one exact hit query"), Adapter->_HitQueries, 1);
    TestEqual(TEXT("same hit adds selection"), Adapter->_Selections[0].Key, uint64{101});
    TestTrue(TEXT("Ctrl selection is additive"), Adapter->_Selections[0].Value);
    TestEqual(TEXT("same hit arms drag"), Adapter->_DragArms, 1);
    Router.OnDragRay({});
    TestEqual(TEXT("active drag forwards ray"), Adapter->_DragUpdates, 1);
    TestTrue(TEXT("Ctrl wheel shifts drag plane"), Router.OnWheel(+1.0f, Ctrl));
    TestEqual(TEXT("plane shift is forwarded"), Adapter->_PlaneShifts.Num(), 1);
    TestFalse(TEXT("ordinary wheel is left for camera"), Router.OnWheel(+1.0f, {}));
    Router.OnPointerReleased(ECkDebug3dPointerButton::Left, FVector2D::ZeroVector, Ctrl);
    Router.OnFocusLost();
    TestEqual(TEXT("release and focus loss release once"), Adapter->_DragReleases, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebug3dInteractionRouter_CommandsAndHover,
                                 "Ck.DebuggerCommon.Viewport3d.Interaction.CommandsAndHover",
                                 ck_debug_3d_interaction_router_spec::TestFlags)
auto
    FCkDebug3dInteractionRouter_CommandsAndHover::
    RunTest(const FString&)
    -> bool
{
    using namespace ck_debug_3d_interaction_router_spec;
    const auto Adapter = MakeShared<FFakeAdapter>();
    auto Router = MakeRouter(Adapter);
    TestTrue(TEXT("bare Space routes pause"), Router.OnKey(EKeys::SpaceBar, {}, 0.0));
    TestTrue(TEXT("bare Enter routes step"), Router.OnKey(EKeys::Enter, {}, 0.0));
    TestTrue(TEXT("bare I routes isolate"), Router.OnKey(EKeys::I, {}, 0.0));
    auto Ctrl = FCkDebug3dInteractionModifiers{};
    Ctrl._Control = true;
    TestFalse(TEXT("modified command keys fall through"), Router.OnKey(EKeys::I, Ctrl, 0.0));
    Router.TickHover({}, {}, 0.0);
    Router.TickHover({}, {}, 0.01);
    TestEqual(TEXT("hover is throttled"), Adapter->_HitQueries, 1);
    Router.OnPointerPressed(ECkDebug3dPointerButton::Left, FVector2D::ZeroVector, {});
    Router.TickHover({}, {}, 1.0);
    TestEqual(TEXT("hover suppresses during gesture"), Adapter->_HitQueries, 1);
    Router.OnFocusLost();
    TestFalse(TEXT("focus loss clears hover"), Adapter->_Hover.IsSet());
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebug3dInteractionRouter_RightClickCommands,
                                 "Ck.DebuggerCommon.Viewport3d.Interaction.RightClickCommands",
                                 ck_debug_3d_interaction_router_spec::TestFlags)
auto
    FCkDebug3dInteractionRouter_RightClickCommands::
    RunTest(const FString&)
    -> bool
{
    using namespace ck_debug_3d_interaction_router_spec;
    const auto Adapter = MakeShared<FFakeAdapter>();
    auto Router = MakeRouter(Adapter);

    auto Ray = FCkDebug3dCursorRay{};
    Ray._Origin = FVector{7.0, 8.0, 9.0};

    // A press must never claim the button -- the camera owns RMB-drag.
    TestFalse(TEXT("right press leaves the gesture to the camera"),
              Router.OnPointerPressed(ECkDebug3dPointerButton::Right, FVector2D{10, 10}, {}, Ray));
    TestTrue(TEXT("motionless right release commands"),
             Router.OnPointerReleased(ECkDebug3dPointerButton::Right, FVector2D{10, 10}, {}, Ray));
    TestEqual(TEXT("command delivered once"), Adapter->_RayCommands.Num(), 1);
    TestEqual(TEXT("command carries the cursor ray"), Adapter->_RayCommands[0], FVector{7.0, 8.0, 9.0});
    TestEqual(TEXT("a command is not a selection"), Adapter->_Selections.Num(), 0);

    // A free-look recentres the cursor, so press and release positions can match after a large
    // gesture. Accumulated axis movement is the only honest discriminator.
    Router.OnPointerPressed(ECkDebug3dPointerButton::Right, FVector2D{10, 10}, {}, Ray);
    Router.OnPointerMovement(6.0f);
    TestFalse(TEXT("a look gesture is not a command even when the cursor returns"),
              Router.OnPointerReleased(ECkDebug3dPointerButton::Right, FVector2D{10, 10}, {}, Ray));
    TestEqual(TEXT("look gesture issued no command"), Adapter->_RayCommands.Num(), 1);

    // Sub-threshold jitter still counts as a click.
    Router.OnPointerPressed(ECkDebug3dPointerButton::Right, FVector2D{10, 10}, {}, Ray);
    Router.OnPointerMovement(1.0f);
    Router.OnPointerMovement(-1.0f);
    TestTrue(TEXT("jitter under the threshold still commands"),
             Router.OnPointerReleased(ECkDebug3dPointerButton::Right, FVector2D{10, 10}, {}, Ray));
    TestEqual(TEXT("jittered click commanded"), Adapter->_RayCommands.Num(), 2);

    // Losing focus mid-press must not leave a command armed.
    Router.OnPointerPressed(ECkDebug3dPointerButton::Right, FVector2D{10, 10}, {}, Ray);
    Router.OnFocusLost();
    TestFalse(TEXT("focus loss disarms the pending command"),
              Router.OnPointerReleased(ECkDebug3dPointerButton::Right, FVector2D{10, 10}, {}, Ray));
    TestEqual(TEXT("no command after focus loss"), Adapter->_RayCommands.Num(), 2);

    // A release with no press at all (button already down when the viewport gained focus).
    TestFalse(TEXT("unpaired right release is inert"),
              Router.OnPointerReleased(ECkDebug3dPointerButton::Right, FVector2D{99, 99}, {}, Ray));
    TestEqual(TEXT("unpaired release issued no command"), Adapter->_RayCommands.Num(), 2);
    return true;
}

// --------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCkDebug3dInteractionRouter_RightClickDefaultsInert,
                                 "Ck.DebuggerCommon.Viewport3d.Interaction.RightClickDefaultsInert",
                                 ck_debug_3d_interaction_router_spec::TestFlags)
auto
    FCkDebug3dInteractionRouter_RightClickDefaultsInert::
    RunTest(const FString&)
    -> bool
{
    using namespace ck_debug_3d_interaction_router_spec;

    // Every debugger except Crowd leaves Command_AtRay defaulted. Right-click must stay a pure
    // camera gesture for them -- no selection, no hit query, no behaviour change.
    struct FDefaultAdapter final : ICkDebug3dInteractionAdapter
    {
        virtual auto
        TryHit(const FCkDebug3dCursorRay&) -> TOptional<FCkDebug3dInteractionHit> override
        {
            ++_HitQueries;
            return {};
        }
        virtual auto
        Select(uint64, bool) -> void override
        {
            ++_Selections;
        }
        int32 _HitQueries = 0;
        int32 _Selections = 0;
    };

    const auto Adapter = MakeShared<FDefaultAdapter>();
    auto Router = FCkDebug3dInteractionRouter{
        Adapter, FCkDebug3dInteractionConfig{}.Set_ClickMovementThresholdPixels(4.0f)};

    Router.OnPointerPressed(ECkDebug3dPointerButton::Right, FVector2D{10, 10}, {});
    Router.OnPointerReleased(ECkDebug3dPointerButton::Right, FVector2D{10, 10}, {});
    TestEqual(TEXT("default adapter is never hit-queried by a right click"), Adapter->_HitQueries, 0);
    TestEqual(TEXT("default adapter selection is untouched"), Adapter->_Selections, 0);
    return true;
}

#endif
