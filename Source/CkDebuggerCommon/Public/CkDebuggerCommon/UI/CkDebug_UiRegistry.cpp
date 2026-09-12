#include "CkDebug_UiRegistry.h"

#include "CkSlateLayout/CkUiCheckbox.h"
#include "CkSlateLayout/CkUiColorPicker.h"
#include "CkSlateLayout/CkUiNumberInput.h"
#include "CkSlateLayout/CkUiInt32Input.h"
#include "CkSlateLayout/CkUiSelect.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_MeterBar.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Sparkline.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_InspectorPanel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Icon.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Switch.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"

#include "Math/UnrealMathUtility.h"

namespace ck_debug_ui_registry
{
    class FInspectorUpdate final : public ICkUiPreparedWidgetUpdate
    {
    public:
        FInspectorUpdate(TSharedRef<SCkDebug_InspectorPanel> InPanel, TAttribute<FText> InTitle, FText InCount)
            : Panel(MoveTemp(InPanel)), Title(MoveTemp(InTitle)), Count(MoveTemp(InCount)) {}
        virtual void Commit() noexcept override { Panel->Set_Title(Title); Panel->Set_CountText(Count); }

    private:
        TSharedRef<SCkDebug_InspectorPanel> Panel;
        TAttribute<FText> Title;
        FText Count;
    };

    class FInspectorComponent final : public ICkUiRetainedWidget
    {
    public:
        FInspectorComponent(const TSharedRef<SWidget>& InBody, const TAttribute<FText>& InTitle, const FText& InCount, bool InStartExpanded)
            : Body(InBody), Panel(SNew(SCkDebug_InspectorPanel).Title(InTitle).CountText(InCount).StartExpanded(InStartExpanded).Body()[InBody]) {}

        virtual auto GetWidget() const -> TSharedRef<SWidget> override { return Panel; }
        virtual auto PrepareReload(const FCkUiCustomWidgetArguments& InArguments, FString& OutFailure) const -> TUniquePtr<ICkUiPreparedWidgetUpdate> override
        {
            const auto* Title = InArguments.TextBindings.Find(TEXT("title"));
            if (Title == nullptr || !Title->IsSet() || InArguments.Slots.FindRef(TEXT("body")) != Body)
            { OutFailure = TEXT("debug-inspector requires a title binding and its stable body mount."); return nullptr; }
            // Start-expanded is a construction hint; accepted reloads retain the user's expansion state.
            return MakeUnique<FInspectorUpdate>(Panel, *Title, InArguments.TextProperties.FindRef(TEXT("count")));
        }

    private:
        TSharedRef<SWidget> Body;
        TSharedRef<SCkDebug_InspectorPanel> Panel;
    };

    struct FEntityRefConfiguration
    {
        TAttribute<FText> Id;
        TAttribute<FText> Name;
        bool ShowName = false;
        TAttribute<bool> CanDispatchEvents;
        FSimpleDelegate Action;
    };

    class FEntityRefComponent final : public ICkUiRetainedWidget, public TSharedFromThis<FEntityRefComponent>
    {
    public:
        explicit FEntityRefComponent(FEntityRefConfiguration InConfiguration)
            : Configuration(MoveTemp(InConfiguration))
        {
        }

        auto Initialize() -> void
        {
            const TWeakPtr<FEntityRefComponent> WeakEntityRef = AsShared();
            SAssignNew(Widget, SCkDebug_EntityRef)
                .PreviewIdText_Lambda([WeakEntityRef]()
                {
                    const TSharedPtr<FEntityRefComponent> EntityRef = WeakEntityRef.Pin();
                    return EntityRef.IsValid() ? EntityRef->Configuration.Id.Get(FText::GetEmpty()).ToString() : FString{};
                })
                .PreviewName_Lambda([WeakEntityRef]()
                {
                    const TSharedPtr<FEntityRefComponent> EntityRef = WeakEntityRef.Pin();
                    return EntityRef.IsValid() && EntityRef->Configuration.ShowName
                        ? EntityRef->Configuration.Name.Get(FText::GetEmpty()).ToString() : FString{};
                })
                .ShowName_Lambda([WeakEntityRef]()
                {
                    const TSharedPtr<FEntityRefComponent> EntityRef = WeakEntityRef.Pin();
                    return EntityRef.IsValid() && EntityRef->Configuration.ShowName;
                })
                .CanNavigate_Lambda([WeakEntityRef]()
                {
                    const TSharedPtr<FEntityRefComponent> EntityRef = WeakEntityRef.Pin();
                    return EntityRef.IsValid() && EntityRef->CanNavigate();
                })
                .OnNavigate_Lambda([WeakEntityRef]()
                {
                    if (const TSharedPtr<FEntityRefComponent> EntityRef = WeakEntityRef.Pin()) { EntityRef->Navigate(); }
                });
        }

        virtual ~FEntityRefComponent() override { Active = false; }
        virtual auto GetWidget() const -> TSharedRef<SWidget> override { return Widget.ToSharedRef(); }
        virtual auto PrepareReload(const FCkUiCustomWidgetArguments& InArguments, FString& OutFailure) const -> TUniquePtr<ICkUiPreparedWidgetUpdate> override;

    private:
        class FPreparedUpdate final : public ICkUiPreparedWidgetUpdate
        {
        public:
            FPreparedUpdate(TSharedRef<FEntityRefComponent> InEntityRef, FEntityRefConfiguration InConfiguration)
                : EntityRef(MoveTemp(InEntityRef)), Configuration(MoveTemp(InConfiguration)) {}
            virtual void Commit() noexcept override { EntityRef->Configuration = MoveTemp(Configuration); }

        private:
            TSharedRef<FEntityRefComponent> EntityRef;
            FEntityRefConfiguration Configuration;
        };

        auto CanNavigate() const -> bool
        {
            return Active && Configuration.CanDispatchEvents.Get(false) && Configuration.Action.IsBound();
        }

        auto Navigate() const -> void
        {
            if (!CanNavigate()) { return; }
            const FSimpleDelegate Action = Configuration.Action;
            Action.ExecuteIfBound();
        }

        TSharedPtr<SCkDebug_EntityRef> Widget;
        FEntityRefConfiguration Configuration;
        bool Active = true;
    };

    auto MakeEntityRefConfiguration(const FCkUiCustomWidgetArguments& InArguments, FEntityRefConfiguration& OutConfiguration, FString& OutFailure) -> bool
    {
        const TAttribute<FText>* Id = InArguments.TextBindings.Find(TEXT("entity-id"));
        const FSimpleDelegate* Action = InArguments.Actions.Find(TEXT("action"));
        if (Id == nullptr || !Id->IsSet() || Action == nullptr || !Action->IsBound())
        {
            OutFailure = TEXT("debug-entity-ref requires entity-id and action bindings.");
            return false;
        }
        OutConfiguration.Id = *Id;
        OutConfiguration.Name = InArguments.TextBindings.FindRef(TEXT("name"));
        OutConfiguration.ShowName = InArguments.BoolProperties.FindRef(TEXT("show-name"), false);
        OutConfiguration.CanDispatchEvents = InArguments.CanDispatchEvents;
        OutConfiguration.Action = *Action;
        return true;
    }

    auto FEntityRefComponent::PrepareReload(const FCkUiCustomWidgetArguments& InArguments, FString& OutFailure) const -> TUniquePtr<ICkUiPreparedWidgetUpdate>
    {
        auto NextConfiguration = FEntityRefConfiguration{};
        if (!MakeEntityRefConfiguration(InArguments, NextConfiguration, OutFailure)) { return {}; }
        return MakeUnique<FPreparedUpdate>(ConstCastSharedRef<FEntityRefComponent>(AsShared()), MoveTemp(NextConfiguration));
    }

    auto RegisterEntityRef(FCkUiWidgetRegistry& InRegistry) -> FCkUiLoadResult
    {
        auto Registration = FCkUiCustomWidgetRegistration{};
        Registration.Schema.Tag = TEXT("debug-entity-ref");
        Registration.Schema.Properties = {
            {TEXT("entity-id"), ECkUiCustomPropertyKind::TextBinding},
            {TEXT("name"), ECkUiCustomPropertyKind::TextBinding, false},
            {TEXT("action"), ECkUiCustomPropertyKind::Action},
            {TEXT("show-name"), ECkUiCustomPropertyKind::Bool, false},
        };
        Registration.RetainedFactory = [](const FCkUiCustomWidgetArguments& Arguments, FString& OutFailure) -> TSharedPtr<ICkUiRetainedWidget>
        {
            auto Configuration = FEntityRefConfiguration{};
            if (!MakeEntityRefConfiguration(Arguments, Configuration, OutFailure)) { return {}; }
            const TSharedRef<FEntityRefComponent> EntityRef = MakeShared<FEntityRefComponent>(MoveTemp(Configuration));
            EntityRef->Initialize();
            return EntityRef;
        };
        return InRegistry.Register(MoveTemp(Registration));
    }

    struct FSwitchConfiguration
    {
        TAttribute<bool> Checked;
        TAttribute<bool> Enabled;
        TAttribute<bool> CanDispatchEvents;
        FCkUiOnBoolChanged Changed;
        FString CheckedBindingName;
    };

    class FSwitchComponent final : public ICkUiRetainedWidget, public TSharedFromThis<FSwitchComponent>
    {
    public:
        explicit FSwitchComponent(FSwitchConfiguration InConfiguration)
            : Configuration(MoveTemp(InConfiguration))
        {
        }

        auto Initialize(const FString& InId) -> void
        {
            const TWeakPtr<FSwitchComponent> WeakSwitch = AsShared();
            SAssignNew(Widget, SCkDebug_Switch)
                .Tag(FName(*InId))
                .IsOn(TAttribute<bool>::CreateLambda([WeakSwitch]()
                {
                    const TSharedPtr<FSwitchComponent> Switch = WeakSwitch.Pin();
                    return Switch.IsValid() && Switch->Configuration.Checked.Get(false);
                }))
                .IsEnabled(TAttribute<bool>::CreateLambda([WeakSwitch]()
                {
                    const TSharedPtr<FSwitchComponent> Switch = WeakSwitch.Pin();
                    return Switch.IsValid() && Switch->Configuration.Enabled.Get(true);
                }))
                .OnStateChanged_Lambda([WeakSwitch](const bool InChecked)
                {
                    if (const TSharedPtr<FSwitchComponent> Switch = WeakSwitch.Pin()) { Switch->OnStateChanged(InChecked); }
                });
        }

        virtual ~FSwitchComponent() override { Active = false; }
        virtual auto GetWidget() const -> TSharedRef<SWidget> override { return Widget.ToSharedRef(); }
        virtual auto PrepareReload(const FCkUiCustomWidgetArguments& InArguments, FString& OutFailure) const -> TUniquePtr<ICkUiPreparedWidgetUpdate> override;

    private:
        class FPreparedUpdate final : public ICkUiPreparedWidgetUpdate
        {
        public:
            FPreparedUpdate(TSharedRef<FSwitchComponent> InSwitch, FSwitchConfiguration InConfiguration)
                : Switch(MoveTemp(InSwitch)), Configuration(MoveTemp(InConfiguration)) {}
            virtual void Commit() noexcept override { Switch->Configuration = MoveTemp(Configuration); }

        private:
            TSharedRef<FSwitchComponent> Switch;
            FSwitchConfiguration Configuration;
        };

        auto CanDispatch() const -> bool
        {
            return Active && Configuration.CanDispatchEvents.Get(false) && Configuration.Enabled.Get(true);
        }

        auto OnStateChanged(const bool InChecked) -> void
        {
            if (!CanDispatch()) { return; }
            const FCkUiOnBoolChanged Changed = Configuration.Changed;
            Changed.ExecuteIfBound(InChecked);
        }

        TSharedPtr<SCkDebug_Switch> Widget;
        FSwitchConfiguration Configuration;
        bool Active = true;
    };

    auto MakeSwitchConfiguration(const FCkUiCustomWidgetArguments& InArguments, FSwitchConfiguration& OutConfiguration, FString& OutFailure) -> bool
    {
        const TAttribute<bool>* Checked = InArguments.BoolBindings.Find(TEXT("checked"));
        const FCkUiOnBoolChanged* Changed = InArguments.BoolChanged.Find(TEXT("changed"));
        const FString* CheckedBindingName = InArguments.BindingNames.Find(TEXT("checked"));
        if (Checked == nullptr || !Checked->IsSet() || Changed == nullptr || !Changed->IsBound()
            || CheckedBindingName == nullptr || CheckedBindingName->IsEmpty())
        {
            OutFailure = TEXT("debug-switch requires checked and changed bindings.");
            return false;
        }
        OutConfiguration.Checked = *Checked;
        OutConfiguration.Enabled = InArguments.BoolBindings.FindRef(TEXT("enabled"));
        OutConfiguration.CanDispatchEvents = InArguments.CanDispatchEvents;
        OutConfiguration.Changed = *Changed;
        OutConfiguration.CheckedBindingName = *CheckedBindingName;
        return true;
    }

    auto FSwitchComponent::PrepareReload(const FCkUiCustomWidgetArguments& InArguments, FString& OutFailure) const -> TUniquePtr<ICkUiPreparedWidgetUpdate>
    {
        auto NextConfiguration = FSwitchConfiguration{};
        if (!MakeSwitchConfiguration(InArguments, NextConfiguration, OutFailure)) { return {}; }
        if (NextConfiguration.CheckedBindingName != Configuration.CheckedBindingName)
        {
            OutFailure = TEXT("debug-switch checked binding cannot change for a retained id.");
            return {};
        }
        return MakeUnique<FPreparedUpdate>(ConstCastSharedRef<FSwitchComponent>(AsShared()), MoveTemp(NextConfiguration));
    }

    auto RegisterSwitch(FCkUiWidgetRegistry& InRegistry) -> FCkUiLoadResult
    {
        auto Registration = FCkUiCustomWidgetRegistration{};
        Registration.Schema.Tag = TEXT("debug-switch");
        Registration.Schema.Properties = {
            {TEXT("checked"), ECkUiCustomPropertyKind::BoolBinding},
            {TEXT("changed"), ECkUiCustomPropertyKind::BoolChanged},
            {TEXT("enabled"), ECkUiCustomPropertyKind::BoolBinding, false},
        };
        Registration.RetainedFactory = [](const FCkUiCustomWidgetArguments& Arguments, FString& OutFailure) -> TSharedPtr<ICkUiRetainedWidget>
        {
            auto Configuration = FSwitchConfiguration{};
            if (!MakeSwitchConfiguration(Arguments, Configuration, OutFailure)) { return {}; }
            const TSharedRef<FSwitchComponent> Switch = MakeShared<FSwitchComponent>(MoveTemp(Configuration));
            Switch->Initialize(Arguments.Id);
            return Switch;
        };
        return InRegistry.Register(MoveTemp(Registration));
    }

    auto RegisterInspector(FCkUiWidgetRegistry& InRegistry) -> FCkUiLoadResult
    {
        auto Registration = FCkUiCustomWidgetRegistration{};
        Registration.Schema.Tag = TEXT("debug-inspector");
        Registration.Schema.Properties = {{TEXT("title"), ECkUiCustomPropertyKind::TextBinding},
            {TEXT("count"), ECkUiCustomPropertyKind::Text, false},
            {TEXT("start-expanded"), ECkUiCustomPropertyKind::Bool, false}};
        Registration.Schema.Slots = {{TEXT("body"), true}};
        Registration.RetainedFactory = [](const FCkUiCustomWidgetArguments& Arguments, FString& OutFailure) -> TSharedPtr<ICkUiRetainedWidget>
        {
            const auto* Title = Arguments.TextBindings.Find(TEXT("title"));
            const auto Body = Arguments.Slots.FindRef(TEXT("body"));
            if (Title == nullptr || !Title->IsSet() || !Body.IsValid())
            { OutFailure = TEXT("debug-inspector requires a title binding and body slot."); return nullptr; }
            return MakeShared<FInspectorComponent>(Body.ToSharedRef(), *Title,
                Arguments.TextProperties.FindRef(TEXT("count")), Arguments.BoolProperties.FindRef(TEXT("start-expanded"), true));
        };
        return InRegistry.Register(MoveTemp(Registration));
    }

    auto FindRequiredNumberBinding(const FCkUiCustomWidgetArguments& InArguments, const TCHAR* InName, FString& OutFailure) -> const TAttribute<float>*
    {
        const TAttribute<float>* Value = InArguments.NumberBindings.Find(InName);
        if (Value == nullptr || !Value->IsSet())
        {
            OutFailure = FString::Printf(TEXT("debug-meter requires the '%s' number binding."), InName);
            return nullptr;
        }
        return Value;
    }

    auto FindRequiredColorBinding(const FCkUiCustomWidgetArguments& InArguments, const TCHAR* InName, FString& OutFailure) -> const TAttribute<FLinearColor>*
    {
        const TAttribute<FLinearColor>* Value = InArguments.ColorBindings.Find(InName);
        if (Value == nullptr || !Value->IsSet())
        {
            OutFailure = FString::Printf(TEXT("debug widget requires the '%s' color binding."), InName);
            return nullptr;
        }
        return Value;
    }

    auto FindRequiredTextBinding(const FCkUiCustomWidgetArguments& InArguments, const TCHAR* InName, FString& OutFailure) -> const TAttribute<FText>*
    {
        const TAttribute<FText>* Value = InArguments.TextBindings.Find(InName);
        if (Value == nullptr || !Value->IsSet())
        {
            OutFailure = FString::Printf(TEXT("debug-status requires the '%s' text binding."), InName);
            return nullptr;
        }
        return Value;
    }

    auto RegisterIcon(FCkUiWidgetRegistry& InRegistry) -> FCkUiLoadResult
    {
        auto Registration = FCkUiCustomWidgetRegistration{};
        Registration.Schema.Tag = TEXT("debug-icon");
        Registration.Schema.Properties = {
            {TEXT("icon"), ECkUiCustomPropertyKind::ImageBinding},
            {TEXT("meaning"), ECkUiCustomPropertyKind::TextBinding},
            {TEXT("color"), ECkUiCustomPropertyKind::ColorBinding, false},
            {TEXT("size"), ECkUiCustomPropertyKind::Number, false},
        };
        Registration.Factory = [](const FCkUiCustomWidgetArguments& Arguments, FString& OutFailure) -> TSharedPtr<SWidget>
        {
            const TAttribute<const FSlateBrush*>* Icon = Arguments.ImageBindings.Find(TEXT("icon"));
            if (Icon == nullptr || !Icon->IsSet())
            {
                OutFailure = TEXT("debug-icon requires an icon binding.");
                return nullptr;
            }

            const TAttribute<FText>* Meaning = FindRequiredTextBinding(Arguments, TEXT("meaning"), OutFailure);
            if (Meaning == nullptr) { return nullptr; }

            const float Size = Arguments.NumberProperties.FindRef(TEXT("size"), 16.0f);
            if (!FMath::IsFinite(Size) || Size < 8.0f || Size > 24.0f)
            {
                OutFailure = TEXT("debug-icon size must be a finite number from 8 through 24.");
                return nullptr;
            }

            const TAttribute<FLinearColor>* Color = Arguments.ColorBindings.Find(TEXT("color"));
            const TAttribute<FSlateColor> Foreground = Color != nullptr
                ? TAttribute<FSlateColor>::CreateLambda([Value = *Color]() -> FSlateColor { return FSlateColor{Value.Get(FLinearColor::White)}; })
                : TAttribute<FSlateColor>(FSlateColor::UseForeground());
            const TAttribute<FLinearColor> Accent = Color != nullptr
                ? *Color : TAttribute<FLinearColor>(FLinearColor::Transparent);
            return SNew(SCkDebug_Icon)
                .Brush(*Icon)
                .Meaning(*Meaning)
                .ColorAndOpacity(Foreground)
                .Accent(Accent)
                .Size(FVector2D{Size, Size});
        };
        return InRegistry.Register(MoveTemp(Registration));
    }

    auto RegisterMeter(FCkUiWidgetRegistry& InRegistry) -> FCkUiLoadResult
    {
        auto Registration = FCkUiCustomWidgetRegistration{};
        Registration.Schema.Tag = TEXT("debug-meter");
        Registration.Schema.Properties = {
            {TEXT("fraction"), ECkUiCustomPropertyKind::NumberBinding},
            {TEXT("fill"), ECkUiCustomPropertyKind::ColorBinding},
            {TEXT("width"), ECkUiCustomPropertyKind::Number, false},
            {TEXT("height"), ECkUiCustomPropertyKind::Number, false},
            {TEXT("tooltip"), ECkUiCustomPropertyKind::TextBinding, false},
        };
        Registration.Factory = [](const FCkUiCustomWidgetArguments& Arguments, FString& OutFailure) -> TSharedPtr<SWidget>
        {
            const TAttribute<float>* Fraction = FindRequiredNumberBinding(Arguments, TEXT("fraction"), OutFailure);
            const TAttribute<FLinearColor>* Fill = FindRequiredColorBinding(Arguments, TEXT("fill"), OutFailure);
            if (Fraction == nullptr || Fill == nullptr) { return nullptr; }

            const float ResolvedWidth = Arguments.NumberProperties.FindRef(TEXT("width"), 96.0f);
            const float ResolvedHeight = Arguments.NumberProperties.FindRef(TEXT("height"), 4.0f);
            if (!FMath::IsFinite(ResolvedWidth) || ResolvedWidth <= 0.0f || !FMath::IsFinite(ResolvedHeight) || ResolvedHeight <= 0.0f)
            {
                OutFailure = TEXT("debug-meter width and height must be finite positive numbers.");
                return nullptr;
            }

            const TSharedRef<SCkDebug_MeterBar> Meter = SNew(SCkDebug_MeterBar)
                .Fraction(*Fraction)
                .FillColor(*Fill)
                .DesiredSize(FVector2D(ResolvedWidth, ResolvedHeight));
            if (const TAttribute<FText>* Tooltip = Arguments.TextBindings.Find(TEXT("tooltip")); Tooltip != nullptr)
            {
                Meter->SetToolTipText(*Tooltip);
            }
            return Meter;
        };
        return InRegistry.Register(MoveTemp(Registration));
    }

    auto RegisterSparkline(FCkUiWidgetRegistry& InRegistry) -> FCkUiLoadResult
    {
        auto Registration = FCkUiCustomWidgetRegistration{};
        Registration.Schema.Tag = TEXT("debug-sparkline");
        Registration.Schema.Properties = {
            {TEXT("samples"), ECkUiCustomPropertyKind::FloatSeriesBinding},
            {TEXT("width"), ECkUiCustomPropertyKind::Number, false},
            {TEXT("height"), ECkUiCustomPropertyKind::Number, false},
            {TEXT("color"), ECkUiCustomPropertyKind::Color, false},
            {TEXT("fill-opacity"), ECkUiCustomPropertyKind::Number, false},
        };
        Registration.Factory = [](const FCkUiCustomWidgetArguments& Arguments, FString& OutFailure) -> TSharedPtr<SWidget>
        {
            const TWeakPtr<const FCkUiFloatSeries>* Samples = Arguments.FloatSeriesBindings.Find(TEXT("samples"));
            if (Samples == nullptr || !Samples->IsValid())
            {
                OutFailure = TEXT("debug-sparkline requires a live samples binding.");
                return nullptr;
            }

            const float Width = Arguments.NumberProperties.FindRef(TEXT("width"), 120.0f);
            const float Height = Arguments.NumberProperties.FindRef(TEXT("height"), 26.0f);
            const float FillOpacity = Arguments.NumberProperties.FindRef(TEXT("fill-opacity"), 0.0f);
            if (!FMath::IsFinite(Width) || Width <= 0.0f || !FMath::IsFinite(Height) || Height <= 0.0f)
            {
                OutFailure = TEXT("debug-sparkline width and height must be finite positive numbers.");
                return nullptr;
            }
            if (!FMath::IsFinite(FillOpacity) || FillOpacity < 0.0f || FillOpacity > 1.0f)
            {
                OutFailure = TEXT("debug-sparkline fill-opacity must be a finite number from 0 through 1.");
                return nullptr;
            }

            return SNew(SCkDebug_Sparkline)
                .FloatSeries(*Samples)
                .Color(Arguments.ColorProperties.FindRef(TEXT("color"), FLinearColor::White))
                .FillOpacity(FillOpacity)
                .DesiredSize(FVector2D(Width, Height));
        };
        return InRegistry.Register(MoveTemp(Registration));
    }

    auto RegisterStatus(FCkUiWidgetRegistry& InRegistry) -> FCkUiLoadResult
    {
        auto Registration = FCkUiCustomWidgetRegistration{};
        Registration.Schema.Tag = TEXT("debug-status");
        Registration.Schema.Properties = {
            {TEXT("label"), ECkUiCustomPropertyKind::TextBinding},
            {TEXT("foreground"), ECkUiCustomPropertyKind::ColorBinding},
            {TEXT("background"), ECkUiCustomPropertyKind::ColorBinding},
            {TEXT("show-dot"), ECkUiCustomPropertyKind::Bool, false},
            {TEXT("tooltip"), ECkUiCustomPropertyKind::TextBinding, false},
        };
        Registration.Factory = [](const FCkUiCustomWidgetArguments& Arguments, FString& OutFailure) -> TSharedPtr<SWidget>
        {
            const TAttribute<FText>* Label = FindRequiredTextBinding(Arguments, TEXT("label"), OutFailure);
            const TAttribute<FLinearColor>* Foreground = FindRequiredColorBinding(Arguments, TEXT("foreground"), OutFailure);
            const TAttribute<FLinearColor>* Background = FindRequiredColorBinding(Arguments, TEXT("background"), OutFailure);
            if (Label == nullptr || Foreground == nullptr || Background == nullptr) { return nullptr; }

            const TSharedRef<SCkDebug_StatusPill> Status = SNew(SCkDebug_StatusPill)
                .Text(*Label)
                .ForegroundColor(*Foreground)
                .BackgroundColor(*Background)
                .ShowDot(Arguments.BoolProperties.FindRef(TEXT("show-dot"), true));
            if (const TAttribute<FText>* Tooltip = Arguments.TextBindings.Find(TEXT("tooltip")); Tooltip != nullptr)
            {
                Status->SetToolTipText(*Tooltip);
            }
            return Status;
        };
        return InRegistry.Register(MoveTemp(Registration));
    }
}

auto FCkDebug_UiRegistry::TryCreate(TSharedPtr<const FCkUiWidgetRegistrySnapshot>& OutSnapshot) -> FCkUiLoadResult
{
    auto Staging = FCkUiWidgetRegistry{};
    if (const FCkUiLoadResult MeterResult = ck_debug_ui_registry::RegisterMeter(Staging); !MeterResult.Succeeded)
    {
        return MeterResult;
    }
    if (const FCkUiLoadResult SparklineResult = ck_debug_ui_registry::RegisterSparkline(Staging); !SparklineResult.Succeeded)
    {
        return SparklineResult;
    }
    if (const FCkUiLoadResult StatusResult = ck_debug_ui_registry::RegisterStatus(Staging); !StatusResult.Succeeded)
    {
        return StatusResult;
    }
    if (const FCkUiLoadResult IconResult = ck_debug_ui_registry::RegisterIcon(Staging); !IconResult.Succeeded)
    {
        return IconResult;
    }

    if (const FCkUiLoadResult InspectorResult = ck_debug_ui_registry::RegisterInspector(Staging); !InspectorResult.Succeeded)
    {
        return InspectorResult;
    }
    if (const FCkUiLoadResult SwitchResult = ck_debug_ui_registry::RegisterSwitch(Staging); !SwitchResult.Succeeded)
    { return SwitchResult; }
    if (const FCkUiLoadResult EntityRefResult = ck_debug_ui_registry::RegisterEntityRef(Staging); !EntityRefResult.Succeeded)
    { return EntityRefResult; }
    if (const FCkUiLoadResult NumberResult = FCkUiNumberInput::Register(Staging); !NumberResult.Succeeded)
    { return NumberResult; }
    if (const FCkUiLoadResult IntegerResult = FCkUiInt32Input::Register(Staging); !IntegerResult.Succeeded)
    { return IntegerResult; }
    if (const FCkUiLoadResult ColorResult = FCkUiColorPicker::Register(Staging); !ColorResult.Succeeded)
    { return ColorResult; }
    if (const FCkUiLoadResult CheckboxResult = FCkUiCheckbox::Register(Staging); !CheckboxResult.Succeeded)
    { return CheckboxResult; }
    if (const FCkUiLoadResult SelectResult = FCkUiSelect::Register(Staging); !SelectResult.Succeeded)
    { return SelectResult; }
    OutSnapshot = Staging.CreateSnapshot();
    auto Result = FCkUiLoadResult{};
    Result.Succeeded = true;
    return Result;
}
