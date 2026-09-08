#include "CkDebug_UiRegistry.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_MeterBar.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_InspectorPanel.h"

#include "Math/UnrealMathUtility.h"

namespace ck_debug_ui_registry
{
    class FInspectorUpdate final : public ICkUiPreparedWidgetUpdate
    {
    public:
        FInspectorUpdate(TSharedRef<SCkDebug_InspectorPanel> InPanel, TAttribute<FText> InTitle)
            : Panel(MoveTemp(InPanel)), Title(MoveTemp(InTitle)) {}
        virtual void Commit() noexcept override { Panel->Set_Title(Title); }

    private:
        TSharedRef<SCkDebug_InspectorPanel> Panel;
        TAttribute<FText> Title;
    };

    class FInspectorComponent final : public ICkUiRetainedWidget
    {
    public:
        FInspectorComponent(const TSharedRef<SWidget>& InBody, const TAttribute<FText>& InTitle)
            : Body(InBody), Panel(SNew(SCkDebug_InspectorPanel).Title(InTitle).Body()[InBody]) {}

        virtual auto GetWidget() const -> TSharedRef<SWidget> override { return Panel; }
        virtual auto PrepareReload(const FCkUiCustomWidgetArguments& InArguments, FString& OutFailure) const -> TUniquePtr<ICkUiPreparedWidgetUpdate> override
        {
            const auto* Title = InArguments.TextBindings.Find(TEXT("title"));
            if (Title == nullptr || !Title->IsSet() || InArguments.Slots.FindRef(TEXT("body")) != Body)
            { OutFailure = TEXT("debug-inspector requires a title binding and its stable body mount."); return nullptr; }
            return MakeUnique<FInspectorUpdate>(Panel, *Title);
        }

    private:
        TSharedRef<SWidget> Body;
        TSharedRef<SCkDebug_InspectorPanel> Panel;
    };

    auto RegisterInspector(FCkUiWidgetRegistry& InRegistry) -> FCkUiLoadResult
    {
        auto Registration = FCkUiCustomWidgetRegistration{};
        Registration.Schema.Tag = TEXT("debug-inspector");
        Registration.Schema.Properties = {{TEXT("title"), ECkUiCustomPropertyKind::TextBinding}};
        Registration.Schema.Slots = {{TEXT("body"), true}};
        Registration.RetainedFactory = [](const FCkUiCustomWidgetArguments& Arguments, FString& OutFailure) -> TSharedPtr<ICkUiRetainedWidget>
        {
            const auto* Title = Arguments.TextBindings.Find(TEXT("title"));
            const auto Body = Arguments.Slots.FindRef(TEXT("body"));
            if (Title == nullptr || !Title->IsSet() || !Body.IsValid())
            { OutFailure = TEXT("debug-inspector requires a title binding and body slot."); return nullptr; }
            return MakeShared<FInspectorComponent>(Body.ToSharedRef(), *Title);
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
    if (const FCkUiLoadResult StatusResult = ck_debug_ui_registry::RegisterStatus(Staging); !StatusResult.Succeeded)
    {
        return StatusResult;
    }

    if (const FCkUiLoadResult InspectorResult = ck_debug_ui_registry::RegisterInspector(Staging); !InspectorResult.Succeeded)
    {
        return InspectorResult;
    }
    OutSnapshot = Staging.CreateSnapshot();
    auto Result = FCkUiLoadResult{};
    Result.Succeeded = true;
    return Result;
}
