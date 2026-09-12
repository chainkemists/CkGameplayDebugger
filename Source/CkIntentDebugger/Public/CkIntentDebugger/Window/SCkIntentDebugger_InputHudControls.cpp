#include "CkIntentDebugger/Window/SCkIntentDebugger_InputHudControls.h"

#include "CkInputHudOverlay/Settings/CkInputHud_Settings.h"
#include "CkInputHudOverlay/Settings/CkInputHud_UserSettings.h"

#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "CkSlateLayout/SCkUiSurface.h"

#include "HAL/IConsoleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Widgets/Text/STextBlock.h"

// ====================================================================================================================

namespace ck_intent_debugger_input_hud_controls
{
    template <typename TEnum>
    auto Cycle(const TEnum InValue, const int32 InDirection, const int32 InCount) -> TEnum
    {
        const int32 Current = static_cast<int32>(InValue);
        return static_cast<TEnum>(((Current + InDirection) % InCount + InCount) % InCount);
    }

    auto Get_CVarFloat(const TCHAR* InName, const float InFallback) -> float
    {
        const IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(InName);
        return CVar != nullptr ? CVar->GetFloat() : InFallback;
    }

    auto Get_CVarInt(const TCHAR* InName, const int32 InFallback) -> int32
    {
        const IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(InName);
        return CVar != nullptr ? CVar->GetInt() : InFallback;
    }

    auto Set_CVarFloat(const TCHAR* InName, const float InValue) -> void
    {
        if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(InName))
        { CVar->Set(InValue, ECVF_SetByConsole); }
    }

    auto Set_CVarInt(const TCHAR* InName, const int32 InValue) -> void
    {
        if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(InName))
        { CVar->Set(InValue, ECVF_SetByConsole); }
    }

    auto MetadataLabel(const ECk_InputHud_MetadataMode InValue) -> FText
    {
        switch (InValue)
        {
            case ECk_InputHud_MetadataMode::Keys: return FText::FromString(TEXT("Keys"));
            case ECk_InputHud_MetadataMode::Compact: return FText::FromString(TEXT("Compact"));
            case ECk_InputHud_MetadataMode::Full: return FText::FromString(TEXT("Full"));
            default: return FText::FromString(TEXT("Keys"));
        }
    }

    auto FrameLabel(const ECk_InputHud_FrameNotation InValue) -> FText
    {
        switch (InValue)
        {
            case ECk_InputHud_FrameNotation::Press: return FText::FromString(TEXT("Press"));
            case ECk_InputHud_FrameNotation::Delta: return FText::FromString(TEXT("Delta"));
            case ECk_InputHud_FrameNotation::Range: return FText::FromString(TEXT("Range"));
            default: return FText::FromString(TEXT("Press"));
        }
    }

    auto ModeLabel(const int32 InValue) -> FText
    {
        return InValue == 1 ? FText::FromString(TEXT("Keyboard")) : FText::FromString(TEXT("Auto"));
    }

    auto CornerLabel(const int32 InValue) -> FText
    {
        static const TCHAR* Labels[] = {TEXT("Top left"), TEXT("Top right"), TEXT("Bottom left"), TEXT("Bottom right")};
        return FText::FromString(Labels[FMath::Clamp(InValue, 0, 3)]);
    }

    auto Save_ProjectIfChanged(const bool InChanged) -> void
    {
        if (NOT InChanged) { return; }
        GetMutableDefault<UCk_InputHud_Settings>()->SaveConfig();
        UCk_InputHud_UserSettings::Get_Mutable()->NotifyChanged();
    }
}

// ====================================================================================================================

auto
    SCkIntentDebugger_InputHudControls::
    Construct(
        const FArguments& InArgs)
    -> void
{
    _CanDispatchEvents = InArgs._CanDispatchEvents;
    ChildSlot[Build_Controls()];
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebugger_InputHudControls::
    Get_ControlsLayoutError() const
    -> FText
{
    return FText::FromString(_ControlsPublicationError);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    SCkIntentDebugger_InputHudControls::
    Build_Controls()
    -> TSharedRef<SWidget>
{
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    if (NOT RegistryResult.Succeeded)
    {
        _ControlsPublicationError = FString::Join(RegistryResult.Errors, TEXT("\n"));
        return SNew(STextBlock).Text(Get_ControlsLayoutError());
    }

    const TWeakPtr<SCkIntentDebugger_InputHudControls> WeakPane{SharedThis(this)};
    auto Data = FCkUiView::FDataBindings{};
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakPane]()
    {
        const TSharedPtr<SCkIntentDebugger_InputHudControls> Pane = WeakPane.Pin();
        return Pane.IsValid() && Pane->_CanDispatchEvents.Get(true);
    });
    Data.SlateUserIndex = 0;
    Data.Text.Add(TEXT("intent-hud-metadata-value"), TAttribute<FText>::CreateLambda([] { return ck_intent_debugger_input_hud_controls::MetadataLabel(UCk_InputHud_UserSettings::Get_MetadataMode()); }));
    Data.Text.Add(TEXT("intent-hud-frame-value"), TAttribute<FText>::CreateLambda([] { return ck_intent_debugger_input_hud_controls::FrameLabel(UCk_InputHud_UserSettings::Get_FrameNotation()); }));
    Data.Text.Add(TEXT("intent-hud-mode-value"), TAttribute<FText>::CreateLambda([] { return ck_intent_debugger_input_hud_controls::ModeLabel(ck_intent_debugger_input_hud_controls::Get_CVarInt(TEXT("ck.InputOverlay"), 2)); }));
    Data.Text.Add(TEXT("intent-hud-corner-value"), TAttribute<FText>::CreateLambda([] { return ck_intent_debugger_input_hud_controls::CornerLabel(ck_intent_debugger_input_hud_controls::Get_CVarInt(TEXT("ck.InputOverlay.Corner"), 1)); }));

    const auto AddNumber = [&Data, WeakPane](const TCHAR* InName, TFunction<float()> InGetter, TFunction<void(float)> InSetter)
    {
        Data.Number.Add(InName, TAttribute<float>::CreateLambda([Getter = InGetter] { return Getter(); }));
        Data.NumberCommitted.Add(InName, FCkUiOnNumberCommitted::CreateLambda([WeakPane, Getter = InGetter, Setter = MoveTemp(InSetter)](const float InValue, ETextCommit::Type)
        {
            if (WeakPane.IsValid() && !FMath::IsNearlyEqual(Getter(), InValue)) { Setter(InValue); }
        }));
    };
    AddNumber(TEXT("intent-hud-scale"),
        [] { return ck_intent_debugger_input_hud_controls::Get_CVarFloat(TEXT("ck.InputOverlay.Scale"), 1.0f); },
        [](const float Value) { ck_intent_debugger_input_hud_controls::Set_CVarFloat(TEXT("ck.InputOverlay.Scale"), FMath::Max(0.0f, Value)); });
    AddNumber(TEXT("intent-hud-opacity"),
        [] { return ck_intent_debugger_input_hud_controls::Get_CVarFloat(TEXT("ck.InputOverlay.Opacity"), 1.0f); },
        [](const float Value) { ck_intent_debugger_input_hud_controls::Set_CVarFloat(TEXT("ck.InputOverlay.Opacity"), FMath::Clamp(Value, 0.15f, 1.0f)); });
    AddNumber(TEXT("intent-hud-offset-x"),
        [] { return ck_intent_debugger_input_hud_controls::Get_CVarFloat(TEXT("ck.InputOverlay.OffsetX"), 0.0f); },
        [](const float Value) { ck_intent_debugger_input_hud_controls::Set_CVarFloat(TEXT("ck.InputOverlay.OffsetX"), FMath::Clamp(Value, 0.0f, 512.0f)); });
    AddNumber(TEXT("intent-hud-offset-y"),
        [] { return ck_intent_debugger_input_hud_controls::Get_CVarFloat(TEXT("ck.InputOverlay.OffsetY"), 0.0f); },
        [](const float Value) { ck_intent_debugger_input_hud_controls::Set_CVarFloat(TEXT("ck.InputOverlay.OffsetY"), FMath::Clamp(Value, 0.0f, 512.0f)); });
    AddNumber(TEXT("intent-hud-history-cap"),
        [] { return static_cast<float>(UCk_InputHud_Settings::Get_HistoryCap()); },
        [](const float Value)
        {
            auto* Settings = GetMutableDefault<UCk_InputHud_Settings>();
            const int32 Next = FMath::Clamp(FMath::RoundToInt(Value), 3, 20);
            const bool Changed = Settings->HistoryCap != Next;
            if (Changed) { Settings->HistoryCap = Next; }
            ck_intent_debugger_input_hud_controls::Save_ProjectIfChanged(Changed);
        });
    AddNumber(TEXT("intent-hud-fade-seconds"),
        [] { return UCk_InputHud_Settings::Get_FadeLifetimeSeconds(); },
        [](const float Value)
        {
            auto* Settings = GetMutableDefault<UCk_InputHud_Settings>();
            const float Next = FMath::Clamp(Value, 3.0f, 30.0f);
            const bool Changed = !FMath::IsNearlyEqual(Settings->FadeLifetimeSeconds, Next);
            if (Changed) { Settings->FadeLifetimeSeconds = Next; }
            ck_intent_debugger_input_hud_controls::Save_ProjectIfChanged(Changed);
        });
    AddNumber(TEXT("intent-hud-tap-hold-ms"),
        [] { return UCk_InputHud_Settings::Get_TapHoldThresholdMs(); },
        [](const float Value)
        {
            auto* Settings = GetMutableDefault<UCk_InputHud_Settings>();
            const float Next = FMath::Clamp(Value, 50.0f, 2000.0f);
            const bool Changed = !FMath::IsNearlyEqual(Settings->TapHoldThresholdMs, Next);
            if (Changed) { Settings->TapHoldThresholdMs = Next; }
            ck_intent_debugger_input_hud_controls::Save_ProjectIfChanged(Changed);
        });
    Data.Visibility.Add(TEXT("intent-hud-frames-allowed"), TAttribute<bool>::CreateLambda([] { return UCk_InputHud_Settings::Get_ShowFrameNumbers(); }));
    Data.Text.Add(TEXT("intent-hud-frames-label"), FText::FromString(TEXT("Show frame numbers")));
    Data.BoolChanged.Add(TEXT("intent-hud-frames-allowed"), FCkUiOnBoolChanged::CreateLambda([WeakPane](const bool InValue)
    {
        if (WeakPane.IsValid())
        {
            auto* Settings = GetMutableDefault<UCk_InputHud_Settings>();
            const bool Changed = Settings->ShowFrameNumbers != InValue;
            if (Changed) { Settings->ShowFrameNumbers = InValue; }
            ck_intent_debugger_input_hud_controls::Save_ProjectIfChanged(Changed);
        }
    }));

    auto Actions = FCkUiView::FActions{};
    const auto AddCycle = [&Actions, WeakPane](const TCHAR* InName, TFunction<void(int32)> InCycle)
    {
        Actions.Add(FString(InName) + TEXT("-previous"), FSimpleDelegate::CreateLambda([WeakPane, Cycle = InCycle]() { if (WeakPane.IsValid()) { Cycle(-1); } }));
        Actions.Add(FString(InName) + TEXT("-next"), FSimpleDelegate::CreateLambda([WeakPane, Cycle = MoveTemp(InCycle)]() { if (WeakPane.IsValid()) { Cycle(1); } }));
    };
    AddCycle(TEXT("intent-hud-metadata"), [](const int32 Direction) { auto* Settings = UCk_InputHud_UserSettings::Get_Mutable(); Settings->Set_MetadataMode(ck_intent_debugger_input_hud_controls::Cycle(Settings->MetadataMode, Direction, 3)); });
    AddCycle(TEXT("intent-hud-frame"), [](const int32 Direction) { auto* Settings = UCk_InputHud_UserSettings::Get_Mutable(); Settings->Set_FrameNotation(ck_intent_debugger_input_hud_controls::Cycle(Settings->FrameNotation, Direction, 3)); });
    AddCycle(TEXT("intent-hud-mode"), [](const int32 Direction) { const int32 Current = ck_intent_debugger_input_hud_controls::Get_CVarInt(TEXT("ck.InputOverlay"), 2); const int32 Next = Current == 1 ? 2 : 1; ck_intent_debugger_input_hud_controls::Set_CVarInt(TEXT("ck.InputOverlay"), Direction == 0 ? Current : Next); });
    AddCycle(TEXT("intent-hud-corner"), [](const int32 Direction)
    {
        const int32 Current = FMath::Clamp(ck_intent_debugger_input_hud_controls::Get_CVarInt(TEXT("ck.InputOverlay.Corner"), 1), 0, 3);
        ck_intent_debugger_input_hud_controls::Set_CVarInt(TEXT("ck.InputOverlay.Corner"), ((Current + Direction) % 4 + 4) % 4);
    });
    Actions.Add(TEXT("intent-hud-reset-readout"), FSimpleDelegate::CreateLambda([WeakPane]() { if (WeakPane.IsValid()) { UCk_InputHud_UserSettings::Get_Mutable()->Reset_ReadoutTuning(); } }));

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT Plugin.IsValid())
    {
        _ControlsPublicationError = TEXT("CkDebugger plugin is unavailable; Intent HUD controls cannot load their authored layout.");
        return SNew(STextBlock).Text(Get_ControlsLayoutError());
    }

    const TSharedRef<FCkUiView> View = FCkUiView::Create({}, MoveTemp(Actions), {}, CkStyle::RegularFont(CkStyle::FontSizeBody()), MoveTemp(Data), Registry);
    const TSharedRef<SWidget> Region = View->GetRegion(TEXT("main"));
    const FString Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    View->SetFiles(FPaths::Combine(Directory, TEXT("IntentInputHudControls.ui.html")), FPaths::Combine(Directory, TEXT("IntentInputHudControls.ui.css")));
    View->PollFiles();
    if (NOT View->GetLastResult().Succeeded)
    {
        _ControlsPublicationError = FString::Join(View->GetLastResult().Errors, TEXT("\n"));
        return SNew(STextBlock).Text(Get_ControlsLayoutError());
    }

    _ControlsPublicationError.Reset();
    _ControlsView = View;
    return Region;
}

// ====================================================================================================================
