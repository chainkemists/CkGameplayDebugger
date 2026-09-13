#include "CkInspector_Tween.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkTween/CkTween_Fragment.h"
#include "CkTween/CkTween_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Utils/CkDebug_InspectorEditGuard.h"
#include "CkDebuggerCommon/Utils/CkDebug_RequestGate.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NumericEditor.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Tween)

// =====================================================================================================================

namespace ck_inspector_tween
{
    static auto Get_StateTone(ECk_TweenState InState) -> ECk_Tone
    {
        switch (InState)
        {
            case ECk_TweenState::Playing:   return ECk_Tone::Ok;
            case ECk_TweenState::Paused:    return ECk_Tone::Warn;
            case ECk_TweenState::Completed: return ECk_Tone::Info;
            case ECk_TweenState::Cancelled: return ECk_Tone::Err;
            default:                        return ECk_Tone::Neutral;
        }
    }

    static auto Get_DurationSeconds(const FCk_Handle& InEntity) -> float
    {
        if (ck::Is_NOT_Valid(InEntity) || NOT InEntity.Has<ck::FFragment_Tween_Params>())
        { return 0.0f; }

        return InEntity.Get<ck::FFragment_Tween_Params>().Get_Duration();
    }

    static auto Get_CurrentTimeSeconds(const FCk_Handle& InEntity) -> float
    {
        if (ck::Is_NOT_Valid(InEntity) || NOT InEntity.Has<ck::FFragment_Tween_Current>())
        { return 0.0f; }

        return InEntity.Get<ck::FFragment_Tween_Current>().Get_CurrentTime();
    }

    auto TryGetTween(const FCk_Handle& InEntity, FCk_Handle_Tween& OutTween) -> bool
    {
        OutTween = {};
        if (ck::Is_NOT_Valid(InEntity) || NOT UCk_Utils_Tween_UE::Has(InEntity)) { return false; }
        auto MutableEntity = InEntity;
        OutTween = UCk_Utils_Tween_UE::Cast(MutableEntity);
        return ck::IsValid(OutTween) && OutTween.Has<ck::FFragment_Tween_Params>()
            && OutTween.Has<ck::FFragment_Tween_Current>();
    }

    auto HasCurrent(const FCk_Handle& InEntity) -> bool
    {
        return ck::IsValid(InEntity) && InEntity.Has<ck::FFragment_Tween_Current>()
            && NOT InEntity.Has_Any<ck::FTag_DestroyEntity_Initiate, ck::FTag_DestroyEntity_EndPlay,
                ck::FTag_DestroyEntity_Teardown, ck::FTag_DestroyEntity_Await, ck::FTag_DestroyEntity_Finalize>();
    }

    auto DiffColor(const bool bDiffMarked) -> FLinearColor
    {
        return bDiffMarked ? CkStyle::Accent() : CkStyle::Text();
    }
}

// =====================================================================================================================

auto FCkInspector_Tween::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("Tween"));
}

auto FCkInspector_Tween::CanInspect(const FCk_Handle& Entity) const -> bool { return ck_inspector_tween::HasCurrent(Entity); }

// =====================================================================================================================

auto FCkInspector_Tween::Build_NativeBody(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder();
    Builder.SetEditGuard(Get_EditGuard());

    if (NOT ck_inspector_tween::HasCurrent(Entity))
    { return Builder.Build(Entity); }

    Builder.AddHeader(FText::FromString(TEXT("Tween")));

    const auto CapturedEntity = Entity;

    Builder.AddStatusPillRow(
        FText::FromString(TEXT("State:")),
        TAttribute<FText>::CreateLambda([CapturedEntity]()
        {
            if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Tween_Current>())
            { return FText::FromString(TEXT("--")); }
            const auto State = CapturedEntity.Get<ck::FFragment_Tween_Current>().Get_State();
            return FText::FromString(ck::Format_UE(TEXT("{}"), State));
        }),
        TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]()
        {
            if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Tween_Current>())
            { return ECk_Tone::Neutral; }
            return ck_inspector_tween::Get_StateTone(
                CapturedEntity.Get<ck::FFragment_Tween_Current>().Get_State());
        }));

    // The Params duration is fixed for the tween's life, so the row shape can be chosen once here;
    // without it there is no bound to meter against and the raw time is all there is to show.
    const auto HasDuration = ck_inspector_tween::Get_DurationSeconds(Entity) > 0.0f;

    if (HasDuration)
    {
        Builder.AddMeterRow(
            FText::FromString(TEXT("Time:")),
            TAttribute<float>::CreateLambda([CapturedEntity]()
            {
                const auto Duration = ck_inspector_tween::Get_DurationSeconds(CapturedEntity);
                if (Duration <= 0.0f) { return 0.0f; }
                return ck_inspector_tween::Get_CurrentTimeSeconds(CapturedEntity) / Duration;
            }),
            ECk_Tone::Accent,
            TAttribute<FText>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Tween_Current>())
                { return FText::FromString(TEXT("--")); }
                return FText::FromString(ck::Format_UE(TEXT("{:.4f} / {:.4f}s"),
                    ck_inspector_tween::Get_CurrentTimeSeconds(CapturedEntity),
                    ck_inspector_tween::Get_DurationSeconds(CapturedEntity)));
            }));
    }
    else
    {
        Builder.AddRow(
            FText::FromString(TEXT("Time:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Tween_Current>())
                { return FText::FromString(TEXT("--")); }
                const auto T = CapturedEntity.Get<ck::FFragment_Tween_Current>().Get_CurrentTime();
                return FText::FromString(FString::Printf(TEXT("%.4f"), T));
            },
            CkStyle::Value_Numeric());
    }

    Builder.AddRow(
        FText::FromString(TEXT("Loop:")),
        [CapturedEntity](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Tween_Current>())
            { return FText::FromString(TEXT("--")); }
            const auto Loop = CapturedEntity.Get<ck::FFragment_Tween_Current>().Get_CurrentLoop();
            return FText::FromString(ck::Format_UE(TEXT("{}"), Loop));
        },
        CkStyle::Value_Numeric());

    Builder.AddConditionalRow(
        FText::FromString(TEXT("Reversed:")),
        [CapturedEntity](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Tween_Current>())
            { return FText::FromString(TEXT("--")); }
            const auto IsReversed = CapturedEntity.Get<ck::FFragment_Tween_Current>().Get_IsReversed();
            return FText::FromString(IsReversed ? TEXT("Yes") : TEXT("No"));
        },
        [CapturedEntity](const FCk_Handle&) -> FLinearColor
        {
            if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Tween_Current>())
            { return CkStyle::None(); }
            const auto IsReversed = CapturedEntity.Get<ck::FFragment_Tween_Current>().Get_IsReversed();
            return IsReversed ? CkStyle::Status_Active() : CkStyle::Value_Bool_False();
        });

    Builder.AddRow(
        FText::FromString(TEXT("Time Multiplier:")),
        [CapturedEntity](const FCk_Handle&)
        {
            if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Tween_Current>())
            { return FText::FromString(TEXT("--")); }
            const auto Multiplier = CapturedEntity.Get<ck::FFragment_Tween_Current>().Get_TimeMultiplier();
            return FText::FromString(FString::Printf(TEXT("%.3f"), Multiplier));
        },
        CkStyle::Value_Numeric());

    // ---- Controls ----
    //
    // Public Tween Utils only, with the typed handle captured BY VALUE and re-validated on fire; the
    // live rows above remain the display. LocalOk throughout — a tween is a local animation driver and
    // FProcessor_Tween_HandleRequests runs on every net mode.
    auto MutableEntity = Entity;

    if (const auto CapturedTween = UCk_Utils_Tween_UE::Cast(MutableEntity);
        ck::IsValid(CapturedTween))
    {
        Builder.AddHeader(FText::FromString(TEXT("Controls")));

        Builder.AddActionRow(
            FText::FromString(TEXT("Playback:")),
            {
                FCkInspector_Action
                {
                    FText::FromString(TEXT("Pause")),
                    FText::FromString(TEXT("Pause — freezes the tween at its current time")),
                    [CapturedTween]
                    {
                        auto Tween = CapturedTween;
                        if (ck::Is_NOT_Valid(Tween))
                        { return; }

                        UCk_Utils_Tween_UE::Pause(Tween, {});
                    }
                },
                FCkInspector_Action
                {
                    FText::FromString(TEXT("Resume")),
                    FText::FromString(TEXT("Resume — continues from the current time")),
                    [CapturedTween]
                    {
                        auto Tween = CapturedTween;
                        if (ck::Is_NOT_Valid(Tween))
                        { return; }

                        UCk_Utils_Tween_UE::Resume(Tween, {});
                    }
                },
                FCkInspector_Action
                {
                    FText::FromString(TEXT("Restart")),
                    FText::FromString(TEXT("Restart — rewinds to the start and plays again")),
                    [CapturedTween]
                    {
                        auto Tween = CapturedTween;
                        if (ck::Is_NOT_Valid(Tween))
                        { return; }

                        UCk_Utils_Tween_UE::Restart(Tween, {});
                    }
                }
            });

        // Stop's behavior is an argument rather than tween state. Native fallback owns a box per build;
        // authored views below keep the same isolation at widget lifetime instead.
        const TSharedRef<ECk_TweenStopBehavior> StopBehavior =
            MakeShared<ECk_TweenStopBehavior>(ECk_TweenStopBehavior::DoNothing);

        // Stop's behavior is an ARGUMENT with no stored counterpart on the tween, so the dropdown holds
        // what the next Stop will carry rather than reading anything back.
        Builder.AddEnumDropdownRow(
            FText::FromString(TEXT("Stop Behavior:")),
            {
                FText::FromString(TEXT("DoNothing")),
                FText::FromString(TEXT("SelfDestruct"))
            },
            TAttribute<int32>::CreateLambda([Behavior = StopBehavior]()
            {
                return static_cast<int32>(*Behavior);
            }),
            [Behavior = StopBehavior](int32 InIndex)
            {
                *Behavior = static_cast<ECk_TweenStopBehavior>(InIndex);
            });

        Builder.AddActionRow(
            FText::FromString(TEXT("Stop:")),
            {
                FCkInspector_Action
                {
                    FText::FromString(TEXT("Stop")),
                    FText::FromString(TEXT("Stop with the behavior selected above — SelfDestruct also destroys the tween entity")),
                    [CapturedTween, Behavior = StopBehavior]
                    {
                        auto Tween = CapturedTween;
                        if (ck::Is_NOT_Valid(Tween))
                        { return; }

                        UCk_Utils_Tween_UE::Stop(Tween, *Behavior, {});
                    }
                }
            });

        Builder.AddNumericRow(
            FText::FromString(TEXT("Set Time Multiplier:")),
            TAttribute<float>::CreateLambda([CapturedEntity]()
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Tween_Current>())
                { return 1.0f; }

                return CapturedEntity.Get<ck::FFragment_Tween_Current>().Get_TimeMultiplier();
            }),
            [CapturedTween](float InMultiplier)
            {
                auto Tween = CapturedTween;
                if (ck::Is_NOT_Valid(Tween))
                { return; }

                UCk_Utils_Tween_UE::SetTimeMultiplier(Tween, InMultiplier, {});
            });
    }

    // ---- Chain ----
    if (Entity.Has<ck::FFragment_Tween_Chain>())
    {
        Builder.AddHeader(FText::FromString(TEXT("Chain")));

        Builder.AddRow(
            FText::FromString(TEXT("Next Tween:")),
            [CapturedEntity](const FCk_Handle&)
            {
                if (ck::Is_NOT_Valid(CapturedEntity) || NOT CapturedEntity.Has<ck::FFragment_Tween_Chain>())
                { return FText::FromString(TEXT("--")); }
                const auto& NextTween = CapturedEntity.Get<ck::FFragment_Tween_Chain>().Get_NextTween();
                if (NOT NextTween.IsSet())
                { return FText::FromString(TEXT("(None)")); }
                return FText::FromString(ck::IsValid(NextTween.GetValue())
                    ? ck::Format_UE(TEXT("[{}]"), NextTween.GetValue())
                    : FString(TEXT("(Invalid)")));
            },
            CkStyle::Value_Handle());
    }

    return Builder.Build(Entity);
}

// =====================================================================================================================

auto SCkInspector_TweenAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _EditGuard = InArgs._EditGuard;
    _DiffLabels = InArgs._DiffLabels;
    const TSharedRef<SBox> Host = SNew(SBox);
    ChildSlot[Host];
    if (Build_AuthoredView()) { Host->SetContent(_View->GetRegion(TEXT("main"))); return; }
    Release();
    Host->SetContent(SNullWidget::NullWidget);
}

SCkInspector_TweenAuthored::~SCkInspector_TweenAuthored() { Release(); }

auto SCkInspector_TweenAuthored::Get_IsAvailable() const -> bool
{
    return _Active && ck_inspector_tween::HasCurrent(_Entity);
}

auto SCkInspector_TweenAuthored::Get_CanRequest() const -> bool
{
    auto Tween = FCk_Handle_Tween{};
    return Get_IsAvailable() && ck_inspector_tween::TryGetTween(_Entity, Tween)
        && ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::LocalOk).IsEnabled;
}

auto SCkInspector_TweenAuthored::Get_RequestDisabledReason() const -> FString
{
    if (NOT Get_IsAvailable()) { return TEXT("Tween is unavailable."); }
    return ck::DebugRequestGate::Evaluate(_Entity, ECk_DebugRequest_Requirement::LocalOk).Reason.ToString();
}

auto SCkInspector_TweenAuthored::Get_StateText() const -> FString
{
    return Get_IsAvailable() ? ck::Format_UE(TEXT("{}"), _Entity.Get<ck::FFragment_Tween_Current>().Get_State()) : TEXT("--");
}

auto SCkInspector_TweenAuthored::Get_StateForeground() const -> FLinearColor
{
    return Get_IsAvailable()
        ? CkStyle::GetToneColor(ck_inspector_tween::Get_StateTone(_Entity.Get<ck::FFragment_Tween_Current>().Get_State()))
        : CkStyle::None();
}

auto SCkInspector_TweenAuthored::Get_StateBackground() const -> FLinearColor
{
    return Get_IsAvailable()
        ? CkStyle::GetToneDimColor(ck_inspector_tween::Get_StateTone(_Entity.Get<ck::FFragment_Tween_Current>().Get_State()))
        : CkStyle::None();
}

auto SCkInspector_TweenAuthored::Get_TimeText() const -> FString
{
    if (NOT Get_IsAvailable()) { return TEXT("--"); }
    const float Current = _Entity.Get<ck::FFragment_Tween_Current>().Get_CurrentTime();
    if (NOT _Entity.Has<ck::FFragment_Tween_Params>()) { return FString::Printf(TEXT("%.4f"), Current); }
    const float Duration = _Entity.Get<ck::FFragment_Tween_Params>().Get_Duration();
    return Duration > 0.0f ? ck::Format_UE(TEXT("{:.4f} / {:.4f}s"), Current, Duration)
        : FString::Printf(TEXT("%.4f"), Current);
}

auto SCkInspector_TweenAuthored::Get_TimeFraction() const -> float
{
    auto Tween = FCk_Handle_Tween{};
    if (NOT ck_inspector_tween::TryGetTween(_Entity, Tween)) { return 0.0f; }
    const float Duration = Tween.Get<ck::FFragment_Tween_Params>().Get_Duration();
    return Duration > 0.0f ? FMath::Clamp(Tween.Get<ck::FFragment_Tween_Current>().Get_CurrentTime() / Duration, 0.0f, 1.0f) : 0.0f;
}

auto SCkInspector_TweenAuthored::Get_LoopText() const -> FString
{
    return Get_IsAvailable() ? ck::Format_UE(TEXT("{}"), _Entity.Get<ck::FFragment_Tween_Current>().Get_CurrentLoop()) : TEXT("--");
}

auto SCkInspector_TweenAuthored::Get_ReversedText() const -> FString
{
    return Get_IsAvailable() ? (_Entity.Get<ck::FFragment_Tween_Current>().Get_IsReversed() ? TEXT("Yes") : TEXT("No")) : TEXT("--");
}

auto SCkInspector_TweenAuthored::Get_ReversedColor() const -> FLinearColor
{
    if (NOT Get_IsAvailable()) { return CkStyle::None(); }
    return _Entity.Get<ck::FFragment_Tween_Current>().Get_IsReversed()
        ? CkStyle::Status_Active() : CkStyle::Value_Bool_False();
}

auto SCkInspector_TweenAuthored::Get_MultiplierText() const -> FString
{
    return Get_IsAvailable() ? ck::Format_UE(TEXT("{:.3f}"), _Entity.Get<ck::FFragment_Tween_Current>().Get_TimeMultiplier()) : TEXT("--");
}

auto SCkInspector_TweenAuthored::Get_NextTweenText() const -> FString
{
    if (NOT Get_IsAvailable() || NOT _Entity.Has<ck::FFragment_Tween_Chain>()) { return TEXT("--"); }
    const TOptional<FCk_Handle_Tween>& Next = _Entity.Get<ck::FFragment_Tween_Chain>().Get_NextTween();
    if (NOT Next.IsSet()) { return TEXT("(None)"); }
    return ck::IsValid(Next.GetValue()) ? ck::Format_UE(TEXT("[{}]"), Next.GetValue()) : TEXT("(Invalid)");
}

auto SCkInspector_TweenAuthored::Request_Pause() -> void
{
    auto Tween = FCk_Handle_Tween{};
    if (Get_CanRequest() && ck_inspector_tween::TryGetTween(_Entity, Tween)) { UCk_Utils_Tween_UE::Pause(Tween, {}); }
}

auto SCkInspector_TweenAuthored::Request_Resume() -> void
{
    auto Tween = FCk_Handle_Tween{};
    if (Get_CanRequest() && ck_inspector_tween::TryGetTween(_Entity, Tween)) { UCk_Utils_Tween_UE::Resume(Tween, {}); }
}

auto SCkInspector_TweenAuthored::Request_Restart() -> void
{
    auto Tween = FCk_Handle_Tween{};
    if (Get_CanRequest() && ck_inspector_tween::TryGetTween(_Entity, Tween)) { UCk_Utils_Tween_UE::Restart(Tween, {}); }
}

auto SCkInspector_TweenAuthored::Request_Stop() -> void
{
    auto Tween = FCk_Handle_Tween{};
    if (Get_CanRequest() && ck_inspector_tween::TryGetTween(_Entity, Tween)) { UCk_Utils_Tween_UE::Stop(Tween, _StopBehavior, {}); }
}

auto SCkInspector_TweenAuthored::Commit_StopBehavior(const int32 InIndex) -> void
{
    if (_Active) { _StopBehavior = InIndex == 1 ? ECk_TweenStopBehavior::SelfDestruct : ECk_TweenStopBehavior::DoNothing; }
}

auto SCkInspector_TweenAuthored::Commit_TimeMultiplier(const float InMultiplier) -> void
{
    auto Tween = FCk_Handle_Tween{};
    if (Get_CanRequest() && ck_inspector_tween::TryGetTween(_Entity, Tween))
    { UCk_Utils_Tween_UE::SetTimeMultiplier(Tween, InMultiplier, {}); }
}

auto SCkInspector_TweenAuthored::Build_StopBehaviorPort() -> TSharedRef<SWidget>
{
    _StopBehaviorOptions = {MakeShared<FString>(TEXT("DoNothing")), MakeShared<FString>(TEXT("SelfDestruct"))};
    const TWeakPtr<SCkInspector_TweenAuthored> Weak{SharedThis(this)};
    const TSharedRef<SComboBox<TSharedPtr<FString>>> Combo = SNew(SComboBox<TSharedPtr<FString>>)
        .OptionsSource(&_StopBehaviorOptions)
        .InitiallySelectedItem(_StopBehaviorOptions[0])
        .OnGenerateWidget_Lambda([](const TSharedPtr<FString> Item)
        { return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : FString{})); })
        .OnSelectionChanged_Lambda([Weak](const TSharedPtr<FString> Item, const ESelectInfo::Type)
        { if (Item.IsValid()) { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Commit_StopBehavior(*Item == TEXT("SelfDestruct") ? 1 : 0); } } })
        [SNew(STextBlock).Text_Lambda([Weak]() { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->_StopBehavior == ECk_TweenStopBehavior::SelfDestruct ? FText::FromString(TEXT("SelfDestruct")) : FText::FromString(TEXT("DoNothing")); })];
    return SNew(SBox).Tag(TEXT("tween-stop-behavior-input")).IsEnabled_Lambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_CanRequest(); })[Combo];
}

auto SCkInspector_TweenAuthored::Build_TimeMultiplierPort() -> TSharedRef<SWidget>
{
    const TWeakPtr<SCkInspector_TweenAuthored> Weak{SharedThis(this)};
    const TSharedPtr<FCkInspectorEditScope> Scope = MakeShared<FCkInspectorEditScope>(_EditGuard);
    _EditScopes.Add(Scope);
    const TSharedRef<SCkDebug_NumericEditor> Editor = SNew(SCkDebug_NumericEditor)
        .Tag(TEXT("tween-multiplier-input"))
        .Value_Lambda([Weak]() { const auto Widget = Weak.Pin(); return Widget.IsValid() ? FCString::Atod(*Widget->Get_MultiplierText()) : 0.0; })
        .Kind(ECkDebug_NumericKind::Float).Width(72.0f).ForegroundColor(CkStyle::Value_Numeric())
        .OnValueCommitted_Lambda([Weak](const double Value)
        { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Commit_TimeMultiplier(static_cast<float>(Value)); } })
        .OnEditStateChanged_Lambda([Scope](const bool bEditing) { if (Scope.IsValid()) { Scope->Set_Active(bEditing); } });
    Editor->SetEnabled(TAttribute<bool>::CreateLambda([Weak]() { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_CanRequest(); }));
    Editor->SetToolTipText(TAttribute<FText>::CreateLambda([Weak]() { const auto Widget = Weak.Pin(); return Widget.IsValid() ? FText::FromString(Widget->Get_RequestDisabledReason()) : FText::GetEmpty(); }));
    return Editor;
}

auto SCkInspector_TweenAuthored::Make_NativePort(TSharedRef<SWidget> InLeaf) -> TSharedRef<SBox>
{
    const TSharedRef<SBox> Port = SNew(SBox); Port->SetContent(MoveTemp(InLeaf)); _NativePorts.Add(Port); return Port;
}

auto SCkInspector_TweenAuthored::Detach_NativePorts() -> void
{
    for (const TSharedPtr<SBox>& Port : _NativePorts) { if (Port.IsValid()) { Port->SetContent(SNullWidget::NullWidget); } }
    _NativePorts.Reset();
}

auto SCkInspector_TweenAuthored::Build_AuthoredView() -> bool
{
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT RegistryResult.Succeeded || NOT Registry.IsValid() || NOT Plugin.IsValid())
    { _LoadError = FString::Join(RegistryResult.Errors, TEXT("\n")); return false; }

    const TWeakPtr<SCkInspector_TweenAuthored> Weak{SharedThis(this)};
    FCkUiView::FNativeBindings Ports;
    Ports.Add(TEXT("tween-stop-behavior-port"), Make_NativePort(Build_StopBehaviorPort()));
    Ports.Add(TEXT("tween-multiplier-port"), Make_NativePort(Build_TimeMultiplierPort()));

    auto Data = FCkUiView::FDataBindings{};
    const auto BindText = [&Data, Weak](const FString& Key, FString (SCkInspector_TweenAuthored::* Getter)() const)
    { Data.Text.Add(Key, TAttribute<FText>::CreateLambda([Weak, Getter]() { const auto Widget = Weak.Pin(); return Widget.IsValid() && NOT Widget->Is_Inert() ? FText::FromString((Widget.Get()->*Getter)()) : FText::GetEmpty(); })); };
    BindText(TEXT("tween-state"), &SCkInspector_TweenAuthored::Get_StateText);
    BindText(TEXT("tween-time"), &SCkInspector_TweenAuthored::Get_TimeText);
    BindText(TEXT("tween-loop"), &SCkInspector_TweenAuthored::Get_LoopText);
    BindText(TEXT("tween-reversed"), &SCkInspector_TweenAuthored::Get_ReversedText);
    BindText(TEXT("tween-multiplier"), &SCkInspector_TweenAuthored::Get_MultiplierText);
    BindText(TEXT("tween-next"), &SCkInspector_TweenAuthored::Get_NextTweenText);
    Data.Number.Add(TEXT("tween-time-fraction"), TAttribute<float>::CreateLambda([Weak]() { const auto Widget = Weak.Pin(); return Widget.IsValid() ? Widget->Get_TimeFraction() : 0.0f; }));
    Data.Color.Add(TEXT("tween-state-foreground"), TAttribute<FLinearColor>::CreateLambda([Weak]() { const auto Widget = Weak.Pin(); return Widget.IsValid() ? Widget->Get_StateForeground() : CkStyle::None(); }));
    Data.Color.Add(TEXT("tween-state-background"), TAttribute<FLinearColor>::CreateLambda([Weak]() { const auto Widget = Weak.Pin(); return Widget.IsValid() ? Widget->Get_StateBackground() : CkStyle::None(); }));
    Data.Color.Add(TEXT("tween-time-fill"), TAttribute<FLinearColor>::CreateLambda([]() { return FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("5FBFE8"))); }));
    Data.Color.Add(TEXT("tween-reversed-color"), TAttribute<FLinearColor>::CreateLambda([Weak]() { const auto Widget = Weak.Pin(); return Widget.IsValid() ? Widget->Get_ReversedColor() : CkStyle::None(); }));
    Data.Visibility.Add(TEXT("tween-has-duration"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); if (NOT Widget.IsValid()) { return false; } auto Tween = FCk_Handle_Tween{}; return ck_inspector_tween::TryGetTween(Widget->_Entity, Tween) && Tween.Get<ck::FFragment_Tween_Params>().Get_Duration() > 0.0f; }));
    Data.Visibility.Add(TEXT("tween-no-duration"), TAttribute<bool>::CreateLambda([Weak]()
    {
        const auto Widget = Weak.Pin();
        return Widget.IsValid() && Widget->Get_IsAvailable()
            && (NOT Widget->_Entity.Has<ck::FFragment_Tween_Params>()
                || Widget->_Entity.Get<ck::FFragment_Tween_Params>().Get_Duration() <= 0.0f);
    }));
    Data.Visibility.Add(TEXT("tween-has-chain"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_IsAvailable() && Widget->_Entity.Has<ck::FFragment_Tween_Chain>(); }));
    Data.Visibility.Add(TEXT("tween-can-controls"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); auto Tween = FCk_Handle_Tween{}; return Widget.IsValid() && ck_inspector_tween::TryGetTween(Widget->_Entity, Tween); }));
    Data.Visibility.Add(TEXT("tween-can-request"), TAttribute<bool>::CreateLambda([Weak]() { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_CanRequest(); }));
    Data.Text.Add(TEXT("tween-disabled-reason"), TAttribute<FText>::CreateLambda([Weak]() { const auto Widget = Weak.Pin(); return Widget.IsValid() ? FText::FromString(Widget->Get_RequestDisabledReason()) : FText::GetEmpty(); }));
    const auto BindDiff = [&Data, Weak](const FString& Key, const FString& Label)
    { Data.Color.Add(Key, TAttribute<FLinearColor>::CreateLambda([Weak, Label]() { const auto Widget = Weak.Pin(); return Widget.IsValid() && NOT Widget->Is_Inert() ? ck_inspector_tween::DiffColor(Widget->Is_DiffMarked(Label)) : FLinearColor::Transparent; })); };
    for (const TPair<FString, FString>& Pair : TArray<TPair<FString, FString>>{{TEXT("state"), TEXT("State:")}, {TEXT("time"), TEXT("Time:")}, {TEXT("loop"), TEXT("Loop:")}, {TEXT("reversed"), TEXT("Reversed:")}, {TEXT("multiplier"), TEXT("Time Multiplier:")}, {TEXT("playback"), TEXT("Playback:")}, {TEXT("stop-behavior"), TEXT("Stop Behavior:")}, {TEXT("stop"), TEXT("Stop:")}, {TEXT("set-multiplier"), TEXT("Set Time Multiplier:")}, {TEXT("next"), TEXT("Next Tween:")}})
    { BindDiff(TEXT("tween-") + Pair.Key + TEXT("-diff-color"), Pair.Value); }

    Data.Text.Add(TEXT("tween-pause-label"), FText::FromString(TEXT("Pause")));
    Data.Text.Add(TEXT("tween-resume-label"), FText::FromString(TEXT("Resume")));
    Data.Text.Add(TEXT("tween-restart-label"), FText::FromString(TEXT("Restart")));
    Data.Text.Add(TEXT("tween-stop-label"), FText::FromString(TEXT("Stop")));
    auto Actions = FCkUiView::FActions{};
    Actions.Add(TEXT("tween-pause"), FSimpleDelegate::CreateLambda([Weak]() { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Request_Pause(); } }));
    Actions.Add(TEXT("tween-resume"), FSimpleDelegate::CreateLambda([Weak]() { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Request_Resume(); } }));
    Actions.Add(TEXT("tween-restart"), FSimpleDelegate::CreateLambda([Weak]() { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Request_Restart(); } }));
    Actions.Add(TEXT("tween-stop"), FSimpleDelegate::CreateLambda([Weak]() { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Request_Stop(); } }));
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([Weak]() { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_IsAvailable(); });

    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(MoveTemp(Ports), MoveTemp(Actions), {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString Root = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(FPaths::Combine(Root, TEXT("EcsInspectorTween.ui.html")), FPaths::Combine(Root, TEXT("EcsInspectorTween.ui.css")));
    Candidate->PollFiles();
    if (NOT Candidate->GetLastResult().Succeeded) { _LoadError = FString::Join(Candidate->GetLastResult().Errors, TEXT("\n")); return false; }
    _View = Candidate; _Mounted = true; _LoadError.Reset(); return true;
}

auto SCkInspector_TweenAuthored::Tick(const FGeometry& Geometry, const double Time, const float DeltaTime) -> void
{
    SCompoundWidget::Tick(Geometry, Time, DeltaTime);
    if (NOT _Active || NOT _View.IsValid()) { return; }
    _View->PollFiles();
    if (NOT _View->GetLastResult().Succeeded) { _LoadError = FString::Join(_View->GetLastResult().Errors, TEXT("\n")); }
    else { _LoadError.Reset(); }
}

auto SCkInspector_TweenAuthored::Release() -> void
{
    if (NOT _Active) { return; }
    _Active = false;
    for (const TSharedPtr<FCkInspectorEditScope>& Scope : _EditScopes) { if (Scope.IsValid()) { Scope->Set_Active(false); } }
    _EditScopes.Reset(); Detach_NativePorts(); _StopBehaviorOptions.Reset(); _Entity = {}; _EditGuard.Reset(); _View.Reset(); _Mounted = false;
}

auto FCkInspector_Tween::Get_StructureMask(const FCk_Handle& Entity) const -> uint8
{
    if (ck::Is_NOT_Valid(Entity)) { return 0; }
    return (Entity.Has<ck::FFragment_Tween_Params>() ? 1 : 0) | (Entity.Has<ck::FFragment_Tween_Current>() ? 2 : 0)
        | (Entity.Has<ck::FFragment_Tween_Chain>() ? 4 : 0);
}

auto FCkInspector_Tween::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> NativeBody = Build_NativeBody(Entity);
    if (FCkInspector_RowCaptureScope::Is_Active()) { return NativeBody; }
    _StructureMask = Get_StructureMask(Entity);
    auto DiffLabels = TSet<FString>{};
    for (const FString& Label : {TEXT("State:"), TEXT("Time:"), TEXT("Loop:"), TEXT("Reversed:"), TEXT("Time Multiplier:"), TEXT("Playback:"), TEXT("Stop Behavior:"), TEXT("Stop:"), TEXT("Set Time Multiplier:"), TEXT("Next Tween:")})
    { if (FCkInspector_DiffMarkScope::Is_LabelMarked(Label)) { DiffLabels.Add(Label); } }
    const TSharedRef<SCkInspector_TweenAuthored> Authored = SNew(SCkInspector_TweenAuthored).Entity(Entity).EditGuard(Get_EditGuard()).DiffLabels(MoveTemp(DiffLabels));
    if (NOT Authored->Is_Mounted()) { _LastAuthoredLoadError = Authored->Get_LoadError(); return NativeBody; }
    _LastAuthoredLoadError.Reset(); _AuthoredInstances.Add(Authored); return Authored;
}

FCkInspector_Tween::~FCkInspector_Tween() { OnDeactivated(); }

auto FCkInspector_Tween::Tick(const FCk_Handle& Entity, const float InDeltaTime) -> void
{
    static_cast<void>(InDeltaTime);
    const uint8 StructureMask = Get_StructureMask(Entity);
    if (StructureMask != _StructureMask) { _StructureMask = StructureMask; RequestRebuild(); }
    _LastAuthoredLoadError.Reset();
    for (const TWeakPtr<SCkInspector_TweenAuthored>& Weak : _AuthoredInstances)
    { if (const auto Instance = Weak.Pin(); Instance.IsValid() && NOT Instance->Get_LoadError().IsEmpty()) { _LastAuthoredLoadError = Instance->Get_LoadError(); break; } }
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_TweenAuthored>& Instance) { return NOT Instance.IsValid() || Instance.Pin()->Is_Inert(); });
}

auto FCkInspector_Tween::OnDeactivated() -> void
{
    for (const TWeakPtr<SCkInspector_TweenAuthored>& Weak : _AuthoredInstances) { if (const auto Instance = Weak.Pin(); Instance.IsValid()) { Instance->Release(); } }
    _AuthoredInstances.Reset();
}

// =====================================================================================================================
