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

#endif
