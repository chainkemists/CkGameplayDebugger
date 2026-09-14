#include "CkDebug_UiTabs.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_UnderlineTabs.h"
#include "CkSlateLayout/CkUiCollection.h"

namespace ck_debug_ui_tabs
{
    struct FConfiguration
    {
        TSharedPtr<FCkUiCollection> Tabs;
        TArray<FString> Keys;
        TAttribute<FString> Value;
        TAttribute<bool> Enabled;
        TAttribute<bool> CanDispatchEvents;
        FCkUiOnStringChanged Changed;
        FString TabsBinding;
        FString ValueBinding;
        FString ChangedBinding;
    };

    auto MakeConfiguration(const FCkUiCustomWidgetArguments& InArguments,
        FConfiguration& OutConfiguration, FString& OutFailure) -> bool
    {
        const auto* Tabs = InArguments.Collections.Find(TEXT("tabs"));
        const auto* Value = InArguments.StringBindings.Find(TEXT("value"));
        const auto* Changed = InArguments.StringChanged.Find(TEXT("changed"));
        if (Tabs == nullptr || NOT Tabs->IsValid() || Value == nullptr || NOT Value->IsSet()
            || Changed == nullptr || NOT Changed->IsBound())
        {
            OutFailure = TEXT("debug-tabs requires tabs, value, and changed bindings.");
            return false;
        }
        for (const auto& Expected : TArray<FCkUiFieldSchema>{
            {TEXT("label"), ECkUiFieldKind::Text}, {TEXT("count"), ECkUiFieldKind::Text},
            {TEXT("warning"), ECkUiFieldKind::Bool}})
        {
            const auto* Field = (*Tabs)->GetSchema().FindByPredicate([&Expected](const FCkUiFieldSchema& InField)
            { return InField.Name == Expected.Name; });
            if (Field == nullptr || Field->Kind != Expected.Kind || NOT Field->Required)
            {
                OutFailure = TEXT("debug-tabs requires label/count Text and warning Bool fields.");
                return false;
            }
        }
        TSet<FName> Names;
        for (const TSharedPtr<const FCkUiRecord>& Record : (*Tabs)->GetRecords())
        {
            if (NOT Record.IsValid() || Record->GetKey().IsEmpty() || Record->GetKey().Len() >= NAME_SIZE)
            {
                OutFailure = TEXT("debug-tabs requires nonempty native tab keys shorter than NAME_SIZE.");
                return false;
            }
            const FName KeyName{*Record->GetKey()};
            if (KeyName.IsNone() || Names.Contains(KeyName))
            {
                OutFailure = TEXT("debug-tabs requires non-None, case-distinct native tab keys.");
                return false;
            }
            Names.Add(KeyName);
            OutConfiguration.Keys.Add(Record->GetKey());
        }
        if (OutConfiguration.Keys.IsEmpty() || NOT OutConfiguration.Keys.Contains(Value->Get(FString{})))
        {
            OutFailure = TEXT("debug-tabs requires a nonempty tab set and a selected key in that set.");
            return false;
        }
        OutConfiguration.Tabs = *Tabs;
        OutConfiguration.Value = *Value;
        OutConfiguration.Changed = *Changed;
        OutConfiguration.Enabled = InArguments.BoolBindings.FindRef(TEXT("enabled"));
        OutConfiguration.CanDispatchEvents = InArguments.CanDispatchEvents;
        OutConfiguration.TabsBinding = InArguments.BindingNames.FindRef(TEXT("tabs"));
        OutConfiguration.ValueBinding = InArguments.BindingNames.FindRef(TEXT("value"));
        OutConfiguration.ChangedBinding = InArguments.BindingNames.FindRef(TEXT("changed"));
        if (OutConfiguration.TabsBinding.IsEmpty() || OutConfiguration.ValueBinding.IsEmpty()
            || OutConfiguration.ChangedBinding.IsEmpty())
        {
            OutFailure = TEXT("debug-tabs requires named collection, value, and change bindings.");
            return false;
        }
        return true;
    }

    class FComponent final : public ICkUiRetainedWidget, public TSharedFromThis<FComponent>
    {
    public:
        explicit FComponent(FConfiguration InConfiguration) : Configuration(MoveTemp(InConfiguration)) {}

        auto HasCurrentTopology() const -> bool
        {
            const auto& Records = Configuration.Tabs->GetRecords();
            if (Records.Num() != Configuration.Keys.Num()) { return false; }
            for (int32 Index = 0; Index < Records.Num(); ++Index)
            {
                if (NOT Records[Index].IsValid() || Records[Index]->GetKey() != Configuration.Keys[Index])
                { return false; }
            }
            return true;
        }

        auto CanDispatch() const -> bool
        {
            return Active && Configuration.CanDispatchEvents.Get(true) && Configuration.Enabled.Get(true)
                && HasCurrentTopology() && Configuration.Keys.Contains(Configuration.Value.Get(FString{}));
        }

        auto ReadActiveTabId() const -> FName
        {
            if (NOT Active || NOT HasCurrentTopology()) { return NAME_None; }
            const FString Value = Configuration.Value.Get(FString{});
            return Configuration.Keys.Contains(Value) ? FName{*Value} : NAME_None;
        }

        auto ReadText(const FString& InKey, const TCHAR* InField) const -> FText
        {
            if (NOT Active || NOT HasCurrentTopology()) { return FText::GetEmpty(); }
            const auto Record = Configuration.Tabs->FindRecord(InKey);
            const auto* Field = Record.IsValid() ? Record->FindField(InField) : nullptr;
            return Field != nullptr && Field->Kind == ECkUiFieldKind::Text ? Field->Text : FText::GetEmpty();
        }

        auto ReadWarning(const FString& InKey) const -> bool
        {
            if (NOT Active || NOT HasCurrentTopology()) { return false; }
            const auto Record = Configuration.Tabs->FindRecord(InKey);
            const auto* Field = Record.IsValid() ? Record->FindField(TEXT("warning")) : nullptr;
            return Field != nullptr && Field->Kind == ECkUiFieldKind::Bool && Field->Bool;
        }

        auto Initialize(const FString& InId) -> void
        {
            const TWeakPtr<FComponent> WeakTabs = AsShared();
            TArray<FCkDebug_UnderlineTabDesc> Tabs;
            for (const FString& Key : Configuration.Keys)
            {
                FCkDebug_UnderlineTabDesc Tab;
                Tab.Id = FName(*Key);
                Tab.LabelText = TAttribute<FText>::CreateLambda([WeakTabs, Key]()
                {
                    const auto Pinned = WeakTabs.Pin();
                    return Pinned.IsValid() ? Pinned->ReadText(Key, TEXT("label")) : FText::GetEmpty();
                });
                Tab.CountText = TAttribute<FText>::CreateLambda([WeakTabs, Key]()
                {
                    const auto Pinned = WeakTabs.Pin();
                    return Pinned.IsValid() ? Pinned->ReadText(Key, TEXT("count")) : FText::GetEmpty();
                });
                Tab.ShowWarnDot = TAttribute<bool>::CreateLambda([WeakTabs, Key]()
                {
                    const auto Pinned = WeakTabs.Pin();
                    return Pinned.IsValid() && Pinned->ReadWarning(Key);
                });
                Tabs.Add(MoveTemp(Tab));
            }
            Widget = SNew(SCkDebug_UnderlineTabs).Tag(FName(*InId)).Tabs(MoveTemp(Tabs))
                .CanDispatchEvents_Lambda([WeakTabs]()
                {
                    const auto Pinned = WeakTabs.Pin();
                    return Pinned.IsValid() && Pinned->CanDispatch();
                })
                .ActiveTabId_Lambda([WeakTabs]()
                {
                    const auto Pinned = WeakTabs.Pin();
                    return Pinned.IsValid() ? Pinned->ReadActiveTabId() : NAME_None;
                })
                .OnTabSelected_Lambda([WeakTabs](FName InKey)
                {
                    const auto Pinned = WeakTabs.Pin();
                    if (NOT Pinned.IsValid() || NOT Pinned->CanDispatch()) { return; }
                    for (const FString& Key : Pinned->Configuration.Keys)
                    {
                        if (FName(*Key) != InKey) { continue; }
                        const FCkUiOnStringChanged Changed = Pinned->Configuration.Changed;
                        Changed.ExecuteIfBound(Key);
                        return;
                    }
                });
        }

        virtual auto GetWidget() const -> TSharedRef<SWidget> override { return Widget.ToSharedRef(); }
        virtual auto GetFocusTransferTarget() const -> TSharedPtr<SWidget> override
        { return Widget->GetPopupFocusTarget(); }
        virtual void ReleaseTransientInteraction() override { Widget->ReleaseTransientInteraction(); }
        virtual void ReleaseOwnerInteraction() override
        {
            Active = false;
            Widget->ReleaseOwnerInteraction();
            Configuration.Changed.Unbind();
        }

        class FPreparedUpdate final : public ICkUiPreparedWidgetUpdate
        {
        public:
            FPreparedUpdate(TSharedRef<FComponent> InOwner, FConfiguration InConfiguration)
                : Owner(MoveTemp(InOwner)), Next(MoveTemp(InConfiguration)) {}
            virtual void Commit() noexcept override { Owner->Configuration = MoveTemp(Next); }
        private:
            TSharedRef<FComponent> Owner;
            FConfiguration Next;
        };

        virtual auto PrepareReload(const FCkUiCustomWidgetArguments& InArguments,
            FString& OutFailure) const -> TUniquePtr<ICkUiPreparedWidgetUpdate> override
        {
            FConfiguration Candidate;
            if (NOT MakeConfiguration(InArguments, Candidate, OutFailure)) { return {}; }
            if (Candidate.Tabs != Configuration.Tabs || Candidate.Keys != Configuration.Keys
                || Candidate.TabsBinding != Configuration.TabsBinding || Candidate.ValueBinding != Configuration.ValueBinding
                || Candidate.ChangedBinding != Configuration.ChangedBinding)
            {
                OutFailure = TEXT("debug-tabs retained IDs require stable ordered keys, collection, and binding identities.");
                return {};
            }
            return MakeUnique<FPreparedUpdate>(ConstCastSharedRef<FComponent>(AsShared()), MoveTemp(Candidate));
        }

    private:
        FConfiguration Configuration;
        TSharedPtr<SCkDebug_UnderlineTabs> Widget;
        bool Active = true;
    };
}

auto FCkDebug_UiTabs::Register(FCkUiWidgetRegistry& InRegistry) -> FCkUiLoadResult
{
    FCkUiCustomWidgetRegistration Registration;
    Registration.Schema.Tag = TEXT("debug-tabs");
    Registration.Schema.Properties = {
        {TEXT("tabs"), ECkUiCustomPropertyKind::CollectionBinding},
        {TEXT("value"), ECkUiCustomPropertyKind::StringBinding},
        {TEXT("changed"), ECkUiCustomPropertyKind::StringChanged},
        {TEXT("enabled"), ECkUiCustomPropertyKind::BoolBinding, false}};
    Registration.RetainedFactory = [](const FCkUiCustomWidgetArguments& InArguments,
        FString& OutFailure) -> TSharedPtr<ICkUiRetainedWidget>
    {
        ck_debug_ui_tabs::FConfiguration Configuration;
        if (NOT ck_debug_ui_tabs::MakeConfiguration(InArguments, Configuration, OutFailure)) { return {}; }
        const auto Component = MakeShared<ck_debug_ui_tabs::FComponent>(MoveTemp(Configuration));
        Component->Initialize(InArguments.Id);
        return Component;
    };
    return InRegistry.Register(MoveTemp(Registration));
}
