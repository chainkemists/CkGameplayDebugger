#include "Misc/AutomationTest.h"

#include "CkEntityDebugOverlay/Input/CkDebugOverlay_InputProcessor.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCkDebugOverlay_SelectionInput_Test,
    "Ck.DebugOverlay.Selection.Input",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCkDebugOverlay_SelectionInput_Test::RunTest(const FString&)
{
    auto Input = FCkDebugOverlay_InputProcessor{};
    Input.SetSelectionBindings({ EKeys::Comma, EKeys::LeftBracket, EKeys::RightBracket, EKeys::Backslash, EKeys::Comma, true, 0.4 });

    TestTrue(TEXT("select tap key-down is consumed while focus gate permits it"),
        Input.RouteSelectionKeyDown(EKeys::Comma, false, false, false, false, false, true, 1.0));
    TestTrue(TEXT("recognized select repeat is consumed without another action"),
        Input.RouteSelectionKeyDown(EKeys::Comma, true, false, false, false, false, false));
    TestTrue(TEXT("recognized select key-up is paired and consumed"), Input.RouteSelectionKeyUp(EKeys::Comma, 1.2));
    TestTrue(TEXT("next is consumed in order"),
        Input.RouteSelectionKeyDown(EKeys::RightBracket, false, false, false, false, false, true, 1.3));
    TestTrue(TEXT("family press is consumed"),
        Input.RouteSelectionKeyDown(EKeys::Backslash, false, false, false, false, false, true, 1.4));
    TestTrue(TEXT("family release is paired"), Input.RouteSelectionKeyUp(EKeys::Backslash, 1.5));
    const auto Actions = Input.ConsumeSelectionActions();
    TestEqual(TEXT("action queue retains order without repeat"), Actions.Num(), 4);
    if (Actions.Num() == 4)
    {
        TestEqual(TEXT("first action is select"), Actions[0], ECkDebugOverlaySelectionInputAction::Select);
        TestEqual(TEXT("second action is next"), Actions[1], ECkDebugOverlaySelectionInputAction::Next);
        TestEqual(TEXT("third action is family pressed"), Actions[2], ECkDebugOverlaySelectionInputAction::FamilyPressed);
        TestEqual(TEXT("fourth action is family released"), Actions[3], ECkDebugOverlaySelectionInputAction::FamilyReleased);
    }

    TestFalse(TEXT("uncaptured repeat is never consumed"),
        Input.RouteSelectionKeyDown(EKeys::Comma, true, false, false, false, false, true, 2.0));
    TestFalse(TEXT("modified select is not consumed"),
        Input.RouteSelectionKeyDown(EKeys::Comma, false, false, false, true, false, true, 2.0));
    TestFalse(TEXT("focus gate blocks new selection press"),
        Input.RouteSelectionKeyDown(EKeys::Comma, false, false, false, false, false, false, 2.0));
    TestTrue(TEXT("exact Ctrl settings chord is consumed"),
        Input.RouteSelectionKeyDown(EKeys::Comma, false, true, false, false, false, true, 2.0));
    TestFalse(TEXT("Ctrl Alt settings chord is rejected"),
        Input.RouteSelectionKeyDown(EKeys::Comma, false, true, true, false, false, true, 2.0));
    TestFalse(TEXT("semicolon is not a default selection binding"),
        Input.RouteSelectionKeyDown(EKeys::Semicolon, false, false, false, false, false, true, 2.0));
    const auto SettingsActions = Input.ConsumeSelectionActions();
    TestEqual(TEXT("only exact settings chord was queued"), SettingsActions.Num(), 1);
    if (SettingsActions.Num() == 1)
    { TestEqual(TEXT("settings action is correct"), SettingsActions[0], ECkDebugOverlaySelectionInputAction::Settings); }
    TestTrue(TEXT("settings key-up remains paired and consumed"), Input.RouteSelectionKeyUp(EKeys::Comma, 2.1));

    TestTrue(TEXT("select can be pending before focus loss"),
        Input.RouteSelectionKeyDown(EKeys::Comma, false, false, false, false, false, true, 3.0));
    TestTrue(TEXT("family can be held before focus loss"),
        Input.RouteSelectionKeyDown(EKeys::Backslash, false, false, false, false, false, true, 3.0));
    Input.ClearSelectionState();
    TestTrue(TEXT("keyup after cleared focus remains consumed"), Input.RouteSelectionKeyUp(EKeys::Comma, 4.0));
    TestTrue(TEXT("keyup after cleared focus is consumed without double-release"), Input.RouteSelectionKeyUp(EKeys::Backslash, 4.0));
    const auto BlurActions = Input.ConsumeSelectionActions();
    TestEqual(TEXT("focus clear drops pending actions and releases family once"), BlurActions.Num(), 1);
    if (BlurActions.Num() == 1)
    { TestEqual(TEXT("focus clear release is only action"), BlurActions[0], ECkDebugOverlaySelectionInputAction::FamilyReleased); }

    TestTrue(TEXT("family is captured before repeated focus-loss events"),
        Input.RouteSelectionKeyDown(EKeys::Backslash, false, false, false, false, false, true, 5.0));
    Input.ClearSelectionForFocusLoss();
    Input.ClearSelectionForFocusLoss();
    TestTrue(TEXT("blurred family key-up remains paired"), Input.RouteSelectionKeyUp(EKeys::Backslash, 5.1));
    const auto RepeatedBlur = Input.ConsumeSelectionActions();
    TestEqual(TEXT("repeated focus loss preserves one family release"), RepeatedBlur.Num(), 1);
    if (RepeatedBlur.Num() == 1)
    { TestEqual(TEXT("repeated focus loss action is family release"), RepeatedBlur[0], ECkDebugOverlaySelectionInputAction::FamilyReleased); }

    TestTrue(TEXT("select is captured before focus loss"),
        Input.RouteSelectionKeyDown(EKeys::Comma, false, false, false, false, false, true, 6.0));
    Input.ClearSelectionForFocusLoss();
    TestTrue(TEXT("blurred select key-up remains paired"), Input.RouteSelectionKeyUp(EKeys::Comma, 7.0));
    TestEqual(TEXT("focus loss cancels held select before key-up"), Input.ConsumeSelectionActions().Num(), 0);

    TestTrue(TEXT("hold select starts captured"),
        Input.RouteSelectionKeyDown(EKeys::Comma, false, false, false, false, false, true, 10.0));
    Input.AdvanceSelectionHold(10.39);
    TestEqual(TEXT("short hold has no action before release"), Input.ConsumeSelectionActions().Num(), 0);
    TestTrue(TEXT("short hold release is captured"), Input.RouteSelectionKeyUp(EKeys::Comma, 10.399));
    const auto ShortTap = Input.ConsumeSelectionActions();
    TestEqual(TEXT("short tap emits select on release"), ShortTap.Num(), 1);
    if (ShortTap.Num() == 1)
    { TestEqual(TEXT("short tap action is select"), ShortTap[0], ECkDebugOverlaySelectionInputAction::Select); }

    TestTrue(TEXT("long hold starts captured"),
        Input.RouteSelectionKeyDown(EKeys::Comma, false, false, false, false, false, true, 20.0));
    TestTrue(TEXT("captured hold repeat is consumed"),
        Input.RouteSelectionKeyDown(EKeys::Comma, true, false, false, false, false, false, 20.2));
    Input.AdvanceSelectionHold(20.41);
    Input.AdvanceSelectionHold(21.0);
    const auto LongHold = Input.ConsumeSelectionActions();
    TestEqual(TEXT("long hold toggles exactly once"), LongHold.Num(), 1);
    if (LongHold.Num() == 1)
    { TestEqual(TEXT("long hold action toggles selection lock"), LongHold[0], ECkDebugOverlaySelectionInputAction::ToggleSelectionLock); }
    TestTrue(TEXT("long hold release remains consumed"), Input.RouteSelectionKeyUp(EKeys::Comma, 21.1));
    TestEqual(TEXT("long hold release does not select"), Input.ConsumeSelectionActions().Num(), 0);

    TestTrue(TEXT("release timestamp advances a hold without OS repeat"),
        Input.RouteSelectionKeyDown(EKeys::Comma, false, false, false, false, false, true, 25.0));
    TestTrue(TEXT("timestamp-only long release is consumed"), Input.RouteSelectionKeyUp(EKeys::Comma, 25.41));
    const auto TimestampRelease = Input.ConsumeSelectionActions();
    TestEqual(TEXT("timestamp-only long release toggles once"), TimestampRelease.Num(), 1);
    if (TimestampRelease.Num() == 1)
    { TestEqual(TEXT("timestamp-only long release does not select"), TimestampRelease[0], ECkDebugOverlaySelectionInputAction::ToggleSelectionLock); }

    TestTrue(TEXT("second long hold supports unlock toggle"),
        Input.RouteSelectionKeyDown(EKeys::Comma, false, false, false, false, false, true, 30.0));
    Input.AdvanceSelectionHold(30.41);
    const auto SecondHold = Input.ConsumeSelectionActions();
    TestEqual(TEXT("second hold emits one toggle action"), SecondHold.Num(), 1);
    TestTrue(TEXT("second long hold release is consumed"), Input.RouteSelectionKeyUp(EKeys::Comma, 30.5));

    TestTrue(TEXT("nonfinite select timestamp stays captured"),
        Input.RouteSelectionKeyDown(EKeys::Comma, false, false, false, false, false, true, std::numeric_limits<double>::quiet_NaN()));
    TestTrue(TEXT("nonfinite select release remains consumed"), Input.RouteSelectionKeyUp(EKeys::Comma, 35.0));
    TestEqual(TEXT("nonfinite timestamp cannot select or lock"), Input.ConsumeSelectionActions().Num(), 0);

    TestTrue(TEXT("retrograde select timestamp stays captured"),
        Input.RouteSelectionKeyDown(EKeys::Comma, false, false, false, false, false, true, 36.0));
    TestTrue(TEXT("retrograde select release remains consumed"), Input.RouteSelectionKeyUp(EKeys::Comma, 35.9));
    TestEqual(TEXT("retrograde timestamp cannot select or lock"), Input.ConsumeSelectionActions().Num(), 0);

    TestTrue(TEXT("blur before threshold captures select"),
        Input.RouteSelectionKeyDown(EKeys::Comma, false, false, false, false, false, true, 40.0));
    Input.ClearSelectionState();
    TestTrue(TEXT("blurred select release is consumed without a tap"), Input.RouteSelectionKeyUp(EKeys::Comma, 41.0));
    TestEqual(TEXT("blur before threshold leaves no action"), Input.ConsumeSelectionActions().Num(), 0);

    TestTrue(TEXT("blur after threshold captures select"),
        Input.RouteSelectionKeyDown(EKeys::Comma, false, false, false, false, false, true, 50.0));
    Input.AdvanceSelectionHold(50.41);
    Input.ClearSelectionState();
    TestTrue(TEXT("post-toggle blur release is consumed"), Input.RouteSelectionKeyUp(EKeys::Comma, 51.0));
    TestEqual(TEXT("blur clears an already queued toggle"), Input.ConsumeSelectionActions().Num(), 0);

    Input.SetSelectionBindings({ EKeys::Period, EKeys::LeftBracket, EKeys::RightBracket, EKeys::Backslash, EKeys::Comma, true, 0.4 });
    TestTrue(TEXT("rebound select supports the same hold path"),
        Input.RouteSelectionKeyDown(EKeys::Period, false, false, false, false, false, true, 60.0));
    Input.AdvanceSelectionHold(60.41);
    TestEqual(TEXT("rebound select hold toggles"), Input.ConsumeSelectionActions().Num(), 1);
    TestTrue(TEXT("rebound select release is consumed"), Input.RouteSelectionKeyUp(EKeys::Period, 60.5));

    Input.SetSelectionBindings({ EKeys::Comma, EKeys::LeftBracket, EKeys::RightBracket, EKeys::Backslash,
        EKeys::Comma, true, std::numeric_limits<double>::quiet_NaN() });
    TestTrue(TEXT("nonfinite hold setting still captures select"),
        Input.RouteSelectionKeyDown(EKeys::Comma, false, false, false, false, false, true, 70.0));
    Input.AdvanceSelectionHold(70.1);
    TestEqual(TEXT("nonfinite hold setting cannot toggle early"), Input.ConsumeSelectionActions().Num(), 0);
    Input.AdvanceSelectionHold(70.41);
    TestEqual(TEXT("nonfinite hold setting falls back to default toggle time"), Input.ConsumeSelectionActions().Num(), 1);
    TestTrue(TEXT("normalized hold release is consumed"), Input.RouteSelectionKeyUp(EKeys::Comma, 70.5));

    auto DefaultBindings = FCkDebugOverlaySelectionInputBindings{};
    DefaultBindings.SelectKey = EKeys::Comma;
    Input.SetSelectionBindings(DefaultBindings);
    TestTrue(TEXT("default hold setting captures select"),
        Input.RouteSelectionKeyDown(EKeys::Comma, false, false, false, false, false, true, 80.0));
    Input.AdvanceSelectionHold(80.41);
    const auto DefaultHold = Input.ConsumeSelectionActions();
    TestEqual(TEXT("default 0.4 second hold toggles once"), DefaultHold.Num(), 1);
    if (DefaultHold.Num() == 1)
    { TestEqual(TEXT("default hold action toggles selection lock"), DefaultHold[0], ECkDebugOverlaySelectionInputAction::ToggleSelectionLock); }
    TestTrue(TEXT("default hold release is consumed"), Input.RouteSelectionKeyUp(EKeys::Comma, 80.5));
    return true;
}
