#include "CkJoltBakeInspector/Window/SCkJoltBakeInspectorWindow.h"
#include "CkJoltBakeInspector/CkJoltBakeInspector_Policy.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkJolt/Settings/CkJolt_ProjectSettings.h"
#include "CkJoltEditor/Cook/CkJoltCook_EditorSubsystem.h"
#include "CkJoltEditor/Cook/CkJoltCook_MeshShapeAudit.h"
#include "CkJoltBakeInspector/Viewport/SCkJoltBakeInspectorPreview.h"
#include "CkDebuggerCommon/Search/SCkDebug_SearchBar.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Card.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_Chip.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_InspectorPanel.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_PaneHost.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatPair.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_StatusPill.h"
#include "CkDebuggerCommon/Window/SCkDebug_WindowChrome.h"
#include "CkEditorTools/Style/CkStyle.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include <AssetRegistry/AssetRegistryModule.h>
#include <AssetRegistry/IAssetRegistry.h>
#include <Editor.h>
#include <Interfaces/IPluginManager.h>
#include <Engine/StaticMesh.h>
#include <Framework/Application/SlateApplication.h>
#include <Framework/Application/SlateUser.h>
#include <Layout/WidgetPath.h>
#include <Misc/FileHelper.h>
#include <Subsystems/AssetEditorSubsystem.h>
#include <UObject/StrongObjectPtr.h>
#include <Widgets/Input/SButton.h>
#include <Widgets/SBoxPanel.h>
#include <Widgets/Layout/SBox.h>
#include <Widgets/Layout/SScrollBox.h>
#include <Widgets/Layout/SSplitter.h>
#include <Widgets/SNullWidget.h>
#include <Widgets/Text/STextBlock.h>
#include <Widgets/Views/SListView.h>

const FName SCkJoltBakeInspectorWindow::WindowId{TEXT("CkJoltBakeInspector")};

namespace ck_jolt_bake_inspector_window
{
    auto PathContainsWidget(const FWidgetPath& InPath, const TSharedRef<SWidget>& InRoot) -> bool
    {
        for (auto Index = int32{0}; Index < InPath.Widgets.Num(); ++Index)
        {
            if (InPath.Widgets[Index].Widget == InRoot) { return true; }
        }
        return false;
    }

    auto ReleaseOwnedSlateInput(const TSharedRef<SWidget>& InRoot) -> void
    {
        if (NOT FSlateApplication::IsInitialized()) { return; }
        FSlateApplication& Slate = FSlateApplication::Get();
        Slate.ForEachUser([&Slate, &InRoot](FSlateUser& InUser)
        {
            const auto IsUnderRoot = [&Slate, &InRoot](const TSharedPtr<SWidget>& InWidget)
            {
                FWidgetPath Path;
                return InWidget.IsValid() && Slate.GeneratePathToWidgetUnchecked(InWidget.ToSharedRef(), Path, EVisibility::All)
                    && PathContainsWidget(Path, InRoot);
            };

            const int32 UserIndex = InUser.GetUserIndex();
            if (IsUnderRoot(Slate.GetUserFocusedWidget(UserIndex)))
            { Slate.ClearUserFocus(UserIndex, EFocusCause::SetDirectly); }

            if (IsUnderRoot(InUser.GetCursorCaptor())) { InUser.ReleaseCursorCapture(); }
            TSet<uint32> PointerIndices{FSlateApplication::CursorPointerIndex};
            for (const auto& Entry : InUser.GetWidgetsUnderPointerLastEventByIndex()) { PointerIndices.Add(Entry.Key); }
            for (const uint32 PointerIndex : PointerIndices)
            {
                if (IsUnderRoot(InUser.GetPointerCaptor(PointerIndex))) { InUser.ReleaseCapture(PointerIndex); }
            }
        }, true);
    }

    auto AuthoredStyleTokens() -> FCkUiView::FTokens
    {
        const auto Color = [](const FLinearColor& InColor) { return TEXT("#") + InColor.ToFColorSRGB().ToHex(); };
        return {
            {TEXT("--jolt-bake-surface"), Color(CkStyle::Bg2())},
            {TEXT("--jolt-bake-border"), Color(CkStyle::Border())},
            {TEXT("--jolt-bake-text"), Color(CkStyle::Text())},
            {TEXT("--jolt-bake-muted"), Color(CkStyle::TextMute())},
            {TEXT("--jolt-bake-info"), Color(CkStyle::Info())},
            {TEXT("--jolt-bake-accent"), Color(CkStyle::Accent())},
            {TEXT("--jolt-bake-warn"), Color(CkStyle::Warn())},
            {TEXT("--jolt-bake-err"), Color(CkStyle::Err())},
            {TEXT("--jolt-bake-radius"), FString::SanitizeFloat(CkStyle::RadiusS())},
            {TEXT("--jolt-bake-ring-width"), FString::SanitizeFloat(CkStyle::RingWidth())},
            {TEXT("--jolt-bake-card-value-size"), FString::FromInt(CkStyle::FontSizeH3())},
        };
    }

    auto HaveSameTokens(const FCkUiView::FTokens& InLeft, const TMap<FString, FString>& InRight) -> bool
    {
        if (InLeft.Num() != InRight.Num()) { return false; }
        for (const auto& [Name, Value] : InLeft)
        {
            const FString* Other = InRight.Find(Name);
            if (Other == nullptr || *Other != Value) { return false; }
        }
        return true;
    }

    auto MakeButton(const TCHAR* InText, const FOnClicked& InClicked) -> TSharedRef<SWidget>
    {
        return SNew(SButton).Text(FText::FromString(InText)).OnClicked(InClicked);
    }

    auto ToText(ck::jolt::cook::ECk_Jolt_MeshShapeAuditAction InAction) -> FString
    {
        using enum ck::jolt::cook::ECk_Jolt_MeshShapeAuditAction;
        switch (InAction)
        {
            case None: return TEXT("No action");
            case CookMissing: return TEXT("Cook missing shape");
            case RebuildStale: return TEXT("Rebuild stale shape");
            case RebuildInsideOut: return TEXT("Rebuild inside-out shape");
            case FixSource: return TEXT("Fix source collision");
            case DeleteOrphan: return TEXT("Delete orphaned generated shape");
        }
        return TEXT("Unknown action");
    }

    auto ToText(ck::jolt::cook::ECk_Jolt_MeshShapeAuditSourceState InState) -> FString
    {
        using enum ck::jolt::cook::ECk_Jolt_MeshShapeAuditSourceState;
        switch (InState)
        {
            case Ready: return TEXT("Source ready");
            case MissingBodySetup: return TEXT("Missing BodySetup");
            case NotWorthPreBaking: return TEXT("Not worth pre-baking");
            case MissingTriMesh: return TEXT("Missing tri-mesh");
            case InvalidTriMesh: return TEXT("Invalid tri-mesh");
            case InvalidTriangleIndices: return TEXT("Invalid triangle indices");
            case UnsupportedCookedPath: return TEXT("Unsupported cooked-shape path");
        }
        return TEXT("Unknown source state");
    }

    auto ToText(ck::jolt::cook::ECk_Jolt_MeshShapeAuditCookedState InState) -> FString
    {
        using enum ck::jolt::cook::ECk_Jolt_MeshShapeAuditCookedState;
        switch (InState)
        {
            case Missing: return TEXT("Missing");
            case Current: return TEXT("Current");
            case StaleCookVersion: return TEXT("Stale cook version");
            case StaleJoltVersion: return TEXT("Stale Jolt version");
            case StaleBodySetup: return TEXT("Stale BodySetup");
            case StaleTraceFlag: return TEXT("Stale collision trace flag");
            case Corrupt: return TEXT("Corrupt");
            case Orphan: return TEXT("Orphan");
        }
        return TEXT("Unknown cooked state");
    }

    auto ToText(ck::jolt::cook::ECk_Jolt_MeshShapeAuditWindingVerdict InVerdict) -> FString
    {
        using enum ck::jolt::cook::ECk_Jolt_MeshShapeAuditWindingVerdict;
        switch (InVerdict)
        {
            case NotTriMesh: return TEXT("Not tri-mesh");
            case NoVerdict: return TEXT("No winding verdict");
            case Outward: return TEXT("Outward");
            case InsideOut: return TEXT("Inside-out");
        }
        return TEXT("Unknown winding verdict");
    }

    auto ToText(ck::jolt::cook::ECk_Jolt_MeshShapeAuditCookedPreviewAvailability InAvailability) -> FString
    {
        using enum ck::jolt::cook::ECk_Jolt_MeshShapeAuditCookedPreviewAvailability;
        switch (InAvailability)
        {
            case MissingCookedAsset: return TEXT("No cooked asset");
            case IncompatibleStale: return TEXT("Stale blob is incompatible with this Jolt version");
            case CorruptBlob: return TEXT("Cooked blob is corrupt");
            case NonTriMesh: return TEXT("Cooked shape is not a tri-mesh");
            case Available: return TEXT("Available");
        }
        return TEXT("Unknown availability");
    }

    auto GetTone(const FCkJoltBakeInspectorRow& InRow) -> ECk_Tone
    {
        if (NOT InRow.Audit.IsSet())
        {
            return InRow.Classification == TEXT("Not analyzed")
                ? ECk_Tone::Neutral
                : ECk_Tone::Err;
        }
        if (InRow.Audit->_bWouldFailBake)
        { return ECk_Tone::Err; }
        if (InRow.Audit->_bWouldUseHeuristic)
        { return ECk_Tone::Warn; }
        return InRow.Audit->_RecommendedAction == ck::jolt::cook::ECk_Jolt_MeshShapeAuditAction::None
            ? ECk_Tone::Ok
            : ECk_Tone::Info;
    }

    auto GetToneColor(const FCkJoltBakeInspectorRow* InRow) -> FLinearColor
    { return CkStyle::GetToneColor(InRow != nullptr ? GetTone(*InRow) : ECk_Tone::Neutral); }

    auto GetToneBackground(const FCkJoltBakeInspectorRow* InRow) -> FLinearColor
    { return CkStyle::GetToneDimColor(InRow != nullptr ? GetTone(*InRow) : ECk_Tone::Neutral); }
}

auto SCkJoltBakeInspectorWindow::BuildNativeContent() -> TSharedRef<SWidget>
{
    const TWeakPtr<SCkJoltBakeInspectorWindow> WeakWindow = SharedThis(this);
    _ListPaneMount->SetContent(SNullWidget::NullWidget);
    _PreviewPaneMount->SetContent(SNullWidget::NullWidget);
    _FallbackSearchHost->SetContent(_SearchBar.ToSharedRef());
    _FallbackListHost->SetContent(_ListPane.ToSharedRef());
    _FallbackPreviewHost->SetContent(_PreviewPane.ToSharedRef());
    return SAssignNew(_NativeChrome, SCkDebug_WindowChrome)
        .WindowId(WindowId)
        .ToolTabId(TEXT("CkJoltBakeInspector"))
        .StatusText_Lambda([WeakWindow]() { const auto Window = WeakWindow.Pin(); return Window.IsValid() && Window->_UsingNativeFallback && NOT Window->_AuthoredPresentationReleased ? Window->GetSummaryText() : FText::GetEmpty(); })
        .ToolbarContent()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
            [ _FallbackSearchHost.ToSharedRef() ]
            + SHorizontalBox::Slot().AutoWidth().Padding(CkStyle::SpaceXS)
            [ ck_jolt_bake_inspector_window::MakeButton(TEXT("Refresh"), FOnClicked::CreateLambda([WeakWindow]() { if (const auto Window = WeakWindow.Pin(); Window.IsValid() && Window->_UsingNativeFallback && NOT Window->_AuthoredPresentationReleased) { return Window->OnRefreshClicked(); } return FReply::Handled(); })) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(CkStyle::SpaceXS)
            [ SNew(SButton).Text(FText::FromString(TEXT("Analyze All"))).IsEnabled_Lambda([WeakWindow] { const auto Window = WeakWindow.Pin(); return Window.IsValid() && Window->_UsingNativeFallback && NOT Window->_AuthoredPresentationReleased && NOT Window->GetIsAnalyzing(); }).OnClicked_Lambda([WeakWindow] { if (const auto Window = WeakWindow.Pin(); Window.IsValid() && Window->_UsingNativeFallback && NOT Window->_AuthoredPresentationReleased) { return Window->StartAnalyzeAll(); } return FReply::Handled(); }) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(CkStyle::SpaceXS)
            [ SNew(SButton).Text(FText::FromString(TEXT("Cancel"))).IsEnabled_Lambda([WeakWindow] { const auto Window = WeakWindow.Pin(); return Window.IsValid() && Window->_UsingNativeFallback && NOT Window->_AuthoredPresentationReleased && Window->GetIsAnalyzing(); }).OnClicked_Lambda([WeakWindow] { if (const auto Window = WeakWindow.Pin(); Window.IsValid() && Window->_UsingNativeFallback && NOT Window->_AuthoredPresentationReleased) { return Window->CancelAnalyzeAll(); } return FReply::Handled(); }) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(CkStyle::SpaceXS)
            [ ck_jolt_bake_inspector_window::MakeButton(TEXT("All"), FOnClicked::CreateLambda([WeakWindow] { if (const auto Window = WeakWindow.Pin(); Window.IsValid() && Window->_UsingNativeFallback && NOT Window->_AuthoredPresentationReleased) { return Window->SetFilterMode(0); } return FReply::Handled(); })) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(CkStyle::SpaceXS)
            [ ck_jolt_bake_inspector_window::MakeButton(TEXT("Heuristic"), FOnClicked::CreateLambda([WeakWindow] { if (const auto Window = WeakWindow.Pin(); Window.IsValid() && Window->_UsingNativeFallback && NOT Window->_AuthoredPresentationReleased) { return Window->SetFilterMode(1); } return FReply::Handled(); })) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(CkStyle::SpaceXS)
            [ ck_jolt_bake_inspector_window::MakeButton(TEXT("Would Fail"), FOnClicked::CreateLambda([WeakWindow] { if (const auto Window = WeakWindow.Pin(); Window.IsValid() && Window->_UsingNativeFallback && NOT Window->_AuthoredPresentationReleased) { return Window->SetFilterMode(2); } return FReply::Handled(); })) ]
        ]
        .Content()
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(CkStyle::SpaceM, CkStyle::SpaceS)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, CkStyle::SpaceS, 0.0f)
                    [ SNew(SCkDebug_Card).StripeColor(CkStyle::Info()).BodyPadding(FMargin{CkStyle::SpaceS})
                    [ SNew(SCkDebug_StatPair).Layout(ECkDebug_StatPairLayout::Stacked_ValueOnTop).Value_Lambda([WeakWindow] { const auto Window = WeakWindow.Pin(); return Window.IsValid() && Window->_UsingNativeFallback && NOT Window->_AuthoredPresentationReleased ? Window->GetMetricText(0) : FText::GetEmpty(); }).Label(FText::FromString(TEXT("Inventory"))) ] ]
                + SHorizontalBox::Slot().FillWidth(1.0f).Padding(CkStyle::SpaceXS, 0.0f, CkStyle::SpaceS, 0.0f)
                    [ SNew(SCkDebug_Card).StripeColor(CkStyle::Accent()).BodyPadding(FMargin{CkStyle::SpaceS})
                    [ SNew(SCkDebug_StatPair).Layout(ECkDebug_StatPairLayout::Stacked_ValueOnTop).Value_Lambda([WeakWindow] { const auto Window = WeakWindow.Pin(); return Window.IsValid() && Window->_UsingNativeFallback && NOT Window->_AuthoredPresentationReleased ? Window->GetMetricText(1) : FText::GetEmpty(); }).Label(FText::FromString(TEXT("Analyzed"))) ] ]
                + SHorizontalBox::Slot().FillWidth(1.0f).Padding(CkStyle::SpaceXS, 0.0f, CkStyle::SpaceS, 0.0f)
                    [ SNew(SCkDebug_Card).StripeColor(CkStyle::Warn()).BodyPadding(FMargin{CkStyle::SpaceS})
                    [ SNew(SCkDebug_StatPair).Layout(ECkDebug_StatPairLayout::Stacked_ValueOnTop).Value_Lambda([WeakWindow] { const auto Window = WeakWindow.Pin(); return Window.IsValid() && Window->_UsingNativeFallback && NOT Window->_AuthoredPresentationReleased ? Window->GetMetricText(2) : FText::GetEmpty(); }).Label(FText::FromString(TEXT("Heuristic"))) ] ]
                + SHorizontalBox::Slot().FillWidth(1.0f).Padding(CkStyle::SpaceXS, 0.0f, 0.0f, 0.0f)
                    [ SNew(SCkDebug_Card).StripeColor(CkStyle::Err()).BodyPadding(FMargin{CkStyle::SpaceS})
                    [ SNew(SCkDebug_StatPair).Layout(ECkDebug_StatPairLayout::Stacked_ValueOnTop).Value_Lambda([WeakWindow] { const auto Window = WeakWindow.Pin(); return Window.IsValid() && Window->_UsingNativeFallback && NOT Window->_AuthoredPresentationReleased ? Window->GetMetricText(3) : FText::GetEmpty(); }).Label(FText::FromString(TEXT("Would fail"))) ] ]
            ]
            + SVerticalBox::Slot().FillHeight(1.0f).Padding(CkStyle::SpaceM)
            [
                SNew(SSplitter).Orientation(Orient_Horizontal)
                + SSplitter::Slot().Value(0.42f)
                [
                    _FallbackListHost.ToSharedRef()
                ]
                + SSplitter::Slot().Value(0.58f)
                [
                    SNew(SCkDebug_PaneHost)
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().FillHeight(0.48f)
                        [ _FallbackPreviewHost.ToSharedRef() ]
                        + SVerticalBox::Slot().FillHeight(0.52f).Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
                        [
                            SNew(SScrollBox)
                            + SScrollBox::Slot()
                            [
                                SNew(SVerticalBox)
                                + SVerticalBox::Slot().AutoHeight()
                                [ SNew(SCkDebug_InspectorPanel).Title(FText::FromString(TEXT("Bake status"))).Body()[SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight()[_SelectedStatus.ToSharedRef()] + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceXS)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([WeakWindow] { const auto Window = WeakWindow.Pin(); return Window.IsValid() && Window->_UsingNativeFallback && NOT Window->_AuthoredPresentationReleased ? Window->GetSelectedDiagnosisText() : FText::GetEmpty(); })]] ]
                                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceXS)
                                [ SNew(SCkDebug_InspectorPanel).Title(FText::FromString(TEXT("Source topology"))).Body()[SNew(STextBlock).AutoWrapText(true).Text_Lambda([WeakWindow] { const auto Window = WeakWindow.Pin(); return Window.IsValid() && Window->_UsingNativeFallback && NOT Window->_AuthoredPresentationReleased ? Window->GetSelectedSourceText() : FText::GetEmpty(); })] ]
                                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceXS)
                                [ SNew(SCkDebug_InspectorPanel).Title(FText::FromString(TEXT("Cooked shape"))).Body()[SNew(STextBlock).AutoWrapText(true).Text_Lambda([WeakWindow] { const auto Window = WeakWindow.Pin(); return Window.IsValid() && Window->_UsingNativeFallback && NOT Window->_AuthoredPresentationReleased ? Window->GetSelectedCookedText() : FText::GetEmpty(); })] ]
                                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceS, 0.0f, 0.0f)
                                [
                                    SNew(SHorizontalBox)
                                    + SHorizontalBox::Slot().AutoWidth().Padding(CkStyle::SpaceXS)[ck_jolt_bake_inspector_window::MakeButton(TEXT("Show in Content Browser"), FOnClicked::CreateLambda([WeakWindow] { if (const auto Window = WeakWindow.Pin(); Window.IsValid() && Window->_UsingNativeFallback && NOT Window->_AuthoredPresentationReleased) { return Window->BrowseSelectedAsset(); } return FReply::Handled(); }))]
                                    + SHorizontalBox::Slot().AutoWidth().Padding(CkStyle::SpaceXS)[ck_jolt_bake_inspector_window::MakeButton(TEXT("Open Asset"), FOnClicked::CreateLambda([WeakWindow] { if (const auto Window = WeakWindow.Pin(); Window.IsValid() && Window->_UsingNativeFallback && NOT Window->_AuthoredPresentationReleased) { return Window->OpenSelectedAsset(); } return FReply::Handled(); }))]
                                    + SHorizontalBox::Slot().AutoWidth().Padding(CkStyle::SpaceXS)[SNew(SButton).Text(FText::FromString(TEXT("Bake Selected"))).IsEnabled_Lambda([WeakWindow] { const auto Window = WeakWindow.Pin(); return Window.IsValid() && Window->_UsingNativeFallback && NOT Window->_AuthoredPresentationReleased && Window->CanBakeSelected(); }).OnClicked_Lambda([WeakWindow] { if (const auto Window = WeakWindow.Pin(); Window.IsValid() && Window->_UsingNativeFallback && NOT Window->_AuthoredPresentationReleased && Window->CanBakeSelected()) { return Window->BakeSelected(); } return FReply::Handled(); })]
                                    + SHorizontalBox::Slot().AutoWidth().Padding(CkStyle::SpaceXS)[SNew(SButton).Text(FText::FromString(TEXT("Bake Repairable"))).IsEnabled_Lambda([WeakWindow] { const auto Window = WeakWindow.Pin(); return Window.IsValid() && Window->_UsingNativeFallback && NOT Window->_AuthoredPresentationReleased && Window->CanBakeAll(); }).OnClicked_Lambda([WeakWindow] { if (const auto Window = WeakWindow.Pin(); Window.IsValid() && Window->_UsingNativeFallback && NOT Window->_AuthoredPresentationReleased && Window->CanBakeAll()) { return Window->BakeAll(); } return FReply::Handled(); })]
                                ]
                            ]
                        ]
                    ]
                ]
            ]
        ];
}

auto SCkJoltBakeInspectorWindow::Construct(const FArguments& InArgs) -> void
{
#if WITH_DEV_AUTOMATION_TESTS
    _TestResourceDirectory = InArgs._TestResourceDirectory;
#endif
    Register_WithGate();
    _SearchBar = SNew(SCkDebug_SearchBar)
        .HintText(FText::FromString(TEXT("Filter baked meshes")))
        .OnSearchTextChanged(this, &SCkJoltBakeInspectorWindow::OnSearchChanged);
    _ListView = SNew(SListView<FRowPtr>)
        .ListItemsSource(&_VisibleRows)
        .OnGenerateRow(this, &SCkJoltBakeInspectorWindow::GenerateRow)
        .OnSelectionChanged(this, &SCkJoltBakeInspectorWindow::OnSelectionChanged);
    _Preview = SNew(SCkJoltBakeInspectorPreview);
    _ListPane = SNew(SCkDebug_PaneHost)[_ListView.ToSharedRef()];
    _PreviewPane = SNew(SCkDebug_PaneHost).ContentMode(ECkDebugPaneContent::OpaqueRenderer)[_Preview.ToSharedRef()];
    _ListPaneMount = SNew(SBox)[_ListPane.ToSharedRef()];
    _PreviewPaneMount = SNew(SBox)[_PreviewPane.ToSharedRef()];
    _FallbackSearchHost = SNew(SBox);
    _FallbackListHost = SNew(SBox);
    _FallbackPreviewHost = SNew(SBox);
    const TWeakPtr<SCkJoltBakeInspectorWindow> WeakWindow = SharedThis(this);
    _SelectedStatus = SNew(SCkDebug_StatusPill)
        .Text_Lambda([WeakWindow] { const auto Window = WeakWindow.Pin(); return Window.IsValid() && Window->_UsingNativeFallback && NOT Window->_AuthoredPresentationReleased && Window->_SelectedRow.IsValid() ? FText::FromString(Window->_SelectedRow->Classification) : FText::FromString(TEXT("Select mesh")); })
        .Tone_Lambda([WeakWindow] { const auto Window = WeakWindow.Pin(); return Window.IsValid() && Window->_UsingNativeFallback && NOT Window->_AuthoredPresentationReleased && Window->_SelectedRow.IsValid() ? ck_jolt_bake_inspector_window::GetTone(*Window->_SelectedRow) : ECk_Tone::Neutral; });
    RefreshInventory();
    BuildAuthoredPresentation();
    if (_UsingNativeFallback)
    { ChildSlot[BuildNativeContent()]; }
}

auto SCkJoltBakeInspectorWindow::BuildAuthoredPresentation() -> void
{
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT RegistryResult.Succeeded || NOT Registry.IsValid() || NOT Plugin.IsValid()
        || NOT _SearchBar.IsValid() || NOT _ListView.IsValid() || NOT _Preview.IsValid())
    { return; }

    auto Ports = FCkUiView::FNativeBindings{};
    Ports.Add(TEXT("jolt-bake-search"), _SearchBar);
    Ports.Add(TEXT("jolt-bake-list"), _ListPaneMount);
    Ports.Add(TEXT("jolt-bake-preview"), _PreviewPaneMount);
    const TWeakPtr<SCkJoltBakeInspectorWindow> WeakWindow = SharedThis(this);
    auto Data = FCkUiView::FDataBindings{};
    Data.SlateUserIndex = 0;
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([WeakWindow]()
    {
        const TSharedPtr<SCkJoltBakeInspectorWindow> Window = WeakWindow.Pin();
        return Window.IsValid() && NOT Window->_AuthoredPresentationReleased;
    });
    Data.Text.Add(TEXT("jolt-bake-inventory"), TAttribute<FText>::CreateLambda([WeakWindow]()
    { const auto Window = WeakWindow.Pin(); return Window.IsValid() ? Window->GetMetricText(0) : FText::GetEmpty(); }));
    Data.Text.Add(TEXT("jolt-bake-analyzed"), TAttribute<FText>::CreateLambda([WeakWindow]()
    { const auto Window = WeakWindow.Pin(); return Window.IsValid() ? Window->GetMetricText(1) : FText::GetEmpty(); }));
    Data.Text.Add(TEXT("jolt-bake-heuristic"), TAttribute<FText>::CreateLambda([WeakWindow]()
    { const auto Window = WeakWindow.Pin(); return Window.IsValid() ? Window->GetMetricText(2) : FText::GetEmpty(); }));
    Data.Text.Add(TEXT("jolt-bake-would-fail"), TAttribute<FText>::CreateLambda([WeakWindow]()
    { const auto Window = WeakWindow.Pin(); return Window.IsValid() ? Window->GetMetricText(3) : FText::GetEmpty(); }));
    Data.Text.Add(TEXT("jolt-bake-diagnosis"), TAttribute<FText>::CreateLambda([WeakWindow]()
    { const auto Window = WeakWindow.Pin(); return Window.IsValid() ? Window->GetSelectedDiagnosisText() : FText::GetEmpty(); }));
    Data.Text.Add(TEXT("jolt-bake-source"), TAttribute<FText>::CreateLambda([WeakWindow]()
    { const auto Window = WeakWindow.Pin(); return Window.IsValid() ? Window->GetSelectedSourceText() : FText::GetEmpty(); }));
    Data.Text.Add(TEXT("jolt-bake-cooked"), TAttribute<FText>::CreateLambda([WeakWindow]()
    { const auto Window = WeakWindow.Pin(); return Window.IsValid() ? Window->GetSelectedCookedText() : FText::GetEmpty(); }));
    Data.Text.Add(TEXT("jolt-bake-status"), TAttribute<FText>::CreateLambda([WeakWindow]()
    { const auto Window = WeakWindow.Pin(); return Window.IsValid() && Window->_SelectedRow.IsValid()
        ? FText::FromString(Window->_SelectedRow->Classification) : FText::FromString(TEXT("Select mesh")); }));
    Data.Color.Add(TEXT("jolt-bake-status-foreground"), TAttribute<FLinearColor>::CreateLambda([WeakWindow]()
    { const auto Window = WeakWindow.Pin(); return Window.IsValid()
        ? ck_jolt_bake_inspector_window::GetToneColor(Window->_SelectedRow.Get()) : CkStyle::GetToneColor(ECk_Tone::Neutral); }));
    Data.Color.Add(TEXT("jolt-bake-status-background"), TAttribute<FLinearColor>::CreateLambda([WeakWindow]()
    { const auto Window = WeakWindow.Pin(); return Window.IsValid()
        ? ck_jolt_bake_inspector_window::GetToneBackground(Window->_SelectedRow.Get()) : CkStyle::GetToneDimColor(ECk_Tone::Neutral); }));
    Data.Visibility.Add(TEXT("jolt-bake-can-bake"), TAttribute<bool>::CreateLambda([WeakWindow]()
    { const auto Window = WeakWindow.Pin(); return Window.IsValid() && Window->CanBakeSelected(); }));
    Data.Visibility.Add(TEXT("jolt-bake-can-bake-all"), TAttribute<bool>::CreateLambda([WeakWindow]()
    { const auto Window = WeakWindow.Pin(); return Window.IsValid() && Window->CanBakeAll(); }));
    Data.Visibility.Add(TEXT("jolt-bake-can-analyze"), TAttribute<bool>::CreateLambda([WeakWindow]()
    { const auto Window = WeakWindow.Pin(); return Window.IsValid() && NOT Window->_AuthoredPresentationReleased && NOT Window->GetIsAnalyzing(); }));
    Data.Visibility.Add(TEXT("jolt-bake-can-cancel"), TAttribute<bool>::CreateLambda([WeakWindow]()
    { const auto Window = WeakWindow.Pin(); return Window.IsValid() && NOT Window->_AuthoredPresentationReleased && Window->GetIsAnalyzing(); }));
    auto Actions = FCkUiView::FActions{};
    Actions.Add(TEXT("jolt-bake-refresh"), FSimpleDelegate::CreateLambda([WeakWindow]() { if (const auto Window = WeakWindow.Pin(); Window.IsValid() && NOT Window->_AuthoredPresentationReleased) { Window->OnRefreshClicked(); } }));
    Actions.Add(TEXT("jolt-bake-analyze-all"), FSimpleDelegate::CreateLambda([WeakWindow]() { if (const auto Window = WeakWindow.Pin(); Window.IsValid() && NOT Window->_AuthoredPresentationReleased && NOT Window->GetIsAnalyzing()) { Window->StartAnalyzeAll(); } }));
    Actions.Add(TEXT("jolt-bake-cancel"), FSimpleDelegate::CreateLambda([WeakWindow]() { if (const auto Window = WeakWindow.Pin(); Window.IsValid() && NOT Window->_AuthoredPresentationReleased && Window->GetIsAnalyzing()) { Window->CancelAnalysis(); } }));
    Actions.Add(TEXT("jolt-bake-filter-all"), FSimpleDelegate::CreateLambda([WeakWindow]() { if (const auto Window = WeakWindow.Pin(); Window.IsValid() && NOT Window->_AuthoredPresentationReleased) { Window->SetFilterMode(0); } }));
    Actions.Add(TEXT("jolt-bake-filter-heuristic"), FSimpleDelegate::CreateLambda([WeakWindow]() { if (const auto Window = WeakWindow.Pin(); Window.IsValid() && NOT Window->_AuthoredPresentationReleased) { Window->SetFilterMode(1); } }));
    Actions.Add(TEXT("jolt-bake-filter-fail"), FSimpleDelegate::CreateLambda([WeakWindow]() { if (const auto Window = WeakWindow.Pin(); Window.IsValid() && NOT Window->_AuthoredPresentationReleased) { Window->SetFilterMode(2); } }));
    Actions.Add(TEXT("jolt-bake-browse"), FSimpleDelegate::CreateLambda([WeakWindow]() { if (const auto Window = WeakWindow.Pin(); Window.IsValid() && NOT Window->_AuthoredPresentationReleased) { Window->BrowseSelectedAsset(); } }));
    Actions.Add(TEXT("jolt-bake-open"), FSimpleDelegate::CreateLambda([WeakWindow]() { if (const auto Window = WeakWindow.Pin(); Window.IsValid() && NOT Window->_AuthoredPresentationReleased) { Window->OpenSelectedAsset(); } }));
    Actions.Add(TEXT("jolt-bake-selected"), FSimpleDelegate::CreateLambda([WeakWindow]() { if (const auto Window = WeakWindow.Pin(); Window.IsValid() && NOT Window->_AuthoredPresentationReleased && Window->CanBakeSelected()) { Window->BakeSelected(); } }));
    Actions.Add(TEXT("jolt-bake-repairable"), FSimpleDelegate::CreateLambda([WeakWindow]() { if (const auto Window = WeakWindow.Pin(); Window.IsValid() && NOT Window->_AuthoredPresentationReleased && Window->CanBakeAll()) { Window->BakeAll(); } }));
    const TSharedRef<FCkUiView> View = FCkUiView::Create(MoveTemp(Ports), MoveTemp(Actions), ck_jolt_bake_inspector_window::AuthoredStyleTokens(),
        CkStyle::RegularFont(CkStyle::FontSizeBody()), MoveTemp(Data), Registry);
    const TSharedRef<SWidget> ActionsRegion = View->GetRegion(TEXT("actions"));
    const TSharedRef<SWidget> Main = View->GetRegion(TEXT("main"));
    FString Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
#if WITH_DEV_AUTOMATION_TESTS
    if (NOT _TestResourceDirectory.IsEmpty()) { Directory = _TestResourceDirectory; }
#endif
    _AuthoredMarkupPath = FPaths::Combine(Directory, TEXT("JoltBakeInspector.ui.html"));
    _AuthoredStylesheetPath = FPaths::Combine(Directory, TEXT("JoltBakeInspector.ui.css"));
    View->SetFiles(_AuthoredMarkupPath, _AuthoredStylesheetPath);
    _AuthoredView = View;
    View->PollFiles(ck_jolt_bake_inspector_window::AuthoredStyleTokens());
    if (NOT View->GetLastResult().Succeeded)
    { return; }
    _UsingNativeFallback = false;
    ChildSlot
    [
        SAssignNew(_NativeChrome, SCkDebug_WindowChrome).WindowId(WindowId).ToolTabId(TEXT("CkJoltBakeInspector"))
        .StatusText(this, &SCkJoltBakeInspectorWindow::GetSummaryText).MenuActionsContent()[ActionsRegion].Content()[Main]
    ];
}

auto SCkJoltBakeInspectorWindow::PollAuthoredPresentation(double InCurrentTime) -> void
{
    if (NOT _AuthoredView.IsValid() || InCurrentTime < _NextAuthoredPollSeconds)
    { return; }
    _NextAuthoredPollSeconds = InCurrentTime + 0.5;
    if (_UsingNativeFallback)
    {
        // The retained candidate notices only a complete file/token change while the fallback remains visible.
        // A changed candidate cannot take ports from the fallback, so only then detach and retry it unparented.
        const FCkUiView::FTokens StyleTokens = ck_jolt_bake_inspector_window::AuthoredStyleTokens();
        const bool ContentChanged = _AuthoredView->PollFiles(StyleTokens);
        if (NOT ContentChanged) { return; }

        if (_NativeChrome.IsValid()) { ck_jolt_bake_inspector_window::ReleaseOwnedSlateInput(_NativeChrome.ToSharedRef()); }
        _FallbackSearchHost->SetContent(SNullWidget::NullWidget);
        _FallbackListHost->SetContent(SNullWidget::NullWidget);
        _FallbackPreviewHost->SetContent(SNullWidget::NullWidget);
        _ListPaneMount->SetContent(_ListPane.ToSharedRef());
        _PreviewPaneMount->SetContent(_PreviewPane.ToSharedRef());
        ChildSlot[SNullWidget::NullWidget];
        _NativeChrome.Reset();
        _AuthoredView->SetFiles(_AuthoredMarkupPath, _AuthoredStylesheetPath);
        _AuthoredView->PollFiles(StyleTokens);
        if (NOT _AuthoredView->GetLastResult().Succeeded)
        {
            ChildSlot[BuildNativeContent()];
            return;
        }
        _UsingNativeFallback = false;
        _ListView->RequestListRefresh();
        ChildSlot
        [
            SAssignNew(_NativeChrome, SCkDebug_WindowChrome).WindowId(WindowId).ToolTabId(TEXT("CkJoltBakeInspector"))
            .StatusText(this, &SCkJoltBakeInspectorWindow::GetSummaryText)
            .MenuActionsContent()[_AuthoredView->GetRegion(TEXT("actions"))]
            .Content()[_AuthoredView->GetRegion(TEXT("main"))]
        ];
        return;
    }
    _AuthoredView->PollFiles(ck_jolt_bake_inspector_window::AuthoredStyleTokens());
    if (_RowMarkupPath.IsEmpty() || _RowStylesheetPath.IsEmpty())
    { return; }
    const FCkUiView::FTokens RowStyleTokens = ck_jolt_bake_inspector_window::AuthoredStyleTokens();

    FString Markup;
    FString Stylesheet;
    const bool ReadSucceeded = FFileHelper::LoadFileToString(Markup, *_RowMarkupPath)
        && FFileHelper::LoadFileToString(Stylesheet, *_RowStylesheetPath);
    if (NOT ReadSucceeded
        || (_HasRowSourceState && Markup == _AppliedRowMarkup && Stylesheet == _AppliedRowStylesheet
            && ck_jolt_bake_inspector_window::HaveSameTokens(RowStyleTokens, _AppliedRowStyleTokens)))
    { return; }

    for (const FRowPtr& Row : _AllRows)
    {
        if (Row.IsValid() && Row->Presentation.IsValid())
        { Row->Presentation->TryReloadWithTokens(Markup, Stylesheet, RowStyleTokens, _RowMarkupPath); }
    }
    _AppliedRowMarkup = MoveTemp(Markup);
    _AppliedRowStylesheet = MoveTemp(Stylesheet);
    _AppliedRowStyleTokens = RowStyleTokens;
    _HasRowSourceState = true;
}

SCkJoltBakeInspectorWindow::~SCkJoltBakeInspectorWindow()
{ Release_AuthoredPresentation(); }

auto SCkJoltBakeInspectorWindow::Release_AuthoredPresentation() -> void
{
    if (_AuthoredPresentationReleased)
    { return; }

    _AuthoredPresentationReleased = true;
    CancelAnalysis();

    if (_NativeChrome.IsValid()) { ck_jolt_bake_inspector_window::ReleaseOwnedSlateInput(_NativeChrome.ToSharedRef()); }
    ChildSlot[SNullWidget::NullWidget];
    _NativeChrome.Reset();
    if (_FallbackSearchHost.IsValid()) { _FallbackSearchHost->SetContent(SNullWidget::NullWidget); }
    if (_FallbackListHost.IsValid()) { _FallbackListHost->SetContent(SNullWidget::NullWidget); }
    if (_FallbackPreviewHost.IsValid()) { _FallbackPreviewHost->SetContent(SNullWidget::NullWidget); }
    if (_ListView.IsValid())
    {
        _ListView->ClearSelection();
        _ListView->ClearItemsSource();
    }

    // The target owns preview-world components. A module close/pre-exit may leave external Slate references alive,
    // so widget destruction is not a sufficient lifetime boundary.
    if (_Preview.IsValid())
    { _Preview->Teardown(); }
    if (_ListPaneMount.IsValid()) { _ListPaneMount->SetContent(SNullWidget::NullWidget); }
    if (_PreviewPaneMount.IsValid()) { _PreviewPaneMount->SetContent(SNullWidget::NullWidget); }
    _Preview.Reset();
    _ListView.Reset();
    _SearchBar.Reset();
    _ListPane.Reset();
    _PreviewPane.Reset();
    _ListPaneMount.Reset();
    _PreviewPaneMount.Reset();
    _FallbackSearchHost.Reset();
    _FallbackListHost.Reset();
    _FallbackPreviewHost.Reset();
    _SelectedStatus.Reset();
    _SelectedRow.Reset();
    for (const FRowPtr& Row : _AllRows) { if (Row.IsValid()) { Row->Presentation.Reset(); } }
    _VisibleRows.Reset();
    _AllRows.Reset();
    _AuthoredView.Reset();
}

auto SCkJoltBakeInspectorWindow::Tick(const FGeometry& InGeometry, double InTime, float InDeltaTime) -> void
{
    SCkDebugger_WindowBase::Tick(InGeometry, InTime, InDeltaTime);
    if (_AuthoredPresentationReleased)
    { return; }
    PollAuthoredPresentation(InTime);
    if (NOT GetIsAnalyzing()) { return; }
    const auto NextIndex = _AnalysisState.TryTakeNext();
    if (NOT NextIndex.IsSet() || NOT _AnalysisQueue.IsValidIndex(*NextIndex)) { CancelAnalysis(); return; }
    AnalyzeRow(_AnalysisQueue[*NextIndex]);
    // All-rows mode keeps the stable item source and reads row text through attributes. Filtered modes alone need a
    // list-source change when a just-audited row enters or leaves the result.
    if (_FilterMode != 0 || NOT _FilterText.IsEmpty()) { RefilterRows(); }
    if (NOT GetIsAnalyzing()) { CancelAnalysis(); }
}

auto SCkJoltBakeInspectorWindow::RefreshInventory() -> void
{
    CancelAnalysis();
    const auto SelectedPath = _SelectedRow.IsValid() ? _SelectedRow->Asset.GetSoftObjectPath() : FSoftObjectPath{};
    auto ExistingRows = TMap<FSoftObjectPath, FRowPtr>{};
    for (const auto& Existing : _AllRows)
    { ExistingRows.Add(Existing->Asset.GetSoftObjectPath(), Existing); }
    _AllRows.Reset();
    _SelectedRow.Reset();

    auto& Registry = FAssetRegistryModule::GetRegistry();
    auto Filter = FARFilter{};
    Filter.ClassPaths.Emplace(UStaticMesh::StaticClass()->GetClassPathName());
    Filter.bRecursiveClasses = true;
    Filter.bRecursivePaths = true;
    for (const auto& Root : UCk_Utils_Jolt_ProjectSettings::Get_BakedMeshShapeRoots())
    { Filter.PackagePaths.Emplace(*Root); }

    auto Assets = TArray<FAssetData>{};
    Registry.GetAssets(Filter, Assets);
    Assets.Sort([](const FAssetData& A, const FAssetData& B)
    { return A.GetSoftObjectPath().ToString() < B.GetSoftObjectPath().ToString(); });

    for (const auto& Asset : Assets)
    {
        const auto AssetPath = Asset.GetSoftObjectPath();
        const auto Row = ExistingRows.FindRef(AssetPath).IsValid()
            ? ExistingRows.FindRef(AssetPath)
            : MakeShared<FCkJoltBakeInspectorRow>();
        Row->Asset = Asset;
        Row->PackagePath = Asset.PackageName.ToString();
        Row->DisplayName = Asset.AssetName.ToString();
        // The audit adapter fills these values only after a user selects a row. Inventory remains registry-only.
        Row->Classification = TEXT("Not analyzed");
        Row->Detail = TEXT("Select this mesh to run the read-only Jolt bake audit.");
        Row->Audit.Reset();
        _AllRows.Add(Row);
        if (Asset.GetSoftObjectPath() == SelectedPath)
        { _SelectedRow = Row; }
    }

    RefilterRows();
    if (_SelectedRow.IsValid() && _ListView.IsValid())
    {
        _ListView->SetSelection(_SelectedRow, ESelectInfo::Direct);
        AnalyzeSelectedRow();
        if (_Preview.IsValid() && _SelectedRow->Audit.IsSet()) { _Preview->Show_Audit(_SelectedRow->Audit.GetValue()); }
    }
}

auto SCkJoltBakeInspectorWindow::OnRefreshClicked() -> FReply
{
    if (_AuthoredPresentationReleased) { return FReply::Handled(); }
    RefreshInventory();
    return FReply::Handled();
}

auto SCkJoltBakeInspectorWindow::RefilterRows() -> void
{
    _VisibleRows.Reset();
    for (const auto& Row : _AllRows)
    {
        const auto MatchesText = _FilterText.IsEmpty() || Row->DisplayName.Contains(_FilterText, ESearchCase::IgnoreCase) ||
            Row->PackagePath.Contains(_FilterText, ESearchCase::IgnoreCase) ||
            Row->Classification.Contains(_FilterText, ESearchCase::IgnoreCase);
        const auto MatchesMode = _FilterMode == 0 || (_FilterMode == 1 && Row->Audit.IsSet() && Row->Audit->_bWouldUseHeuristic)
            || (_FilterMode == 2 && Row->Audit.IsSet() && Row->Audit->_bWouldFailBake);
        if (MatchesText && MatchesMode)
        { _VisibleRows.Add(Row); }
    }
    if (_ListView.IsValid())
    { _ListView->RequestListRefresh(); }
}

auto SCkJoltBakeInspectorWindow::OnSearchChanged(const FString& InText) -> void
{
    if (_AuthoredPresentationReleased) { return; }
    _FilterText = InText;
    RefilterRows();
}

auto SCkJoltBakeInspectorWindow::OnSelectionChanged(FRowPtr InRow, ESelectInfo::Type InSelection) -> void
{
    if (_AuthoredPresentationReleased) { return; }
    if (InSelection == ESelectInfo::Direct) { return; }
    _SelectedRow = MoveTemp(InRow);
    AnalyzeSelectedRow();
    if (_Preview.IsValid())
    {
        if (_SelectedRow.IsValid() && _SelectedRow->Audit.IsSet()) { _Preview->Show_Audit(_SelectedRow->Audit.GetValue()); }
        else { _Preview->Clear(); }
    }
}

auto SCkJoltBakeInspectorWindow::AnalyzeSelectedRow() -> void
{
    if (NOT _SelectedRow.IsValid() || _SelectedRow->Audit.IsSet())
    { return; }
    AnalyzeRow(_SelectedRow);
}

auto SCkJoltBakeInspectorWindow::AnalyzeRow(const FRowPtr& InRow) -> void
{
    if (NOT InRow.IsValid() || InRow->Audit.IsSet()) { return; }
    const TStrongObjectPtr<UStaticMesh> Mesh{Cast<UStaticMesh>(InRow->Asset.GetAsset())};
    if (NOT Mesh.IsValid())
    {
        InRow->Classification = TEXT("Could not load source mesh");
        InRow->Detail = TEXT("The Asset Registry entry no longer resolves to a UStaticMesh.");
        return;
    }

    // Shared CkJoltEditor inspection is explicitly mutation-free and owns the same classification semantics as the
    // cooker. This tab deliberately does not calculate a parallel freshness or winding verdict.
    InRow->Audit = ck::jolt::cook::Analyze_MeshShape(*Mesh);
    const auto& Audit = InRow->Audit.GetValue();
    InRow->Classification = ck_jolt_bake_inspector_window::ToText(Audit._RecommendedAction);
    InRow->Detail = ck::Format_UE(
        TEXT("Source: {}\nCooked: {}\nCooked preview: {}\nRepairs: {} individual, {} aggregate no-verdict\n"
             "Heuristic: {}\nWould fail bake: {}\n{}"),
        ck_jolt_bake_inspector_window::ToText(Audit._SourceState),
        ck_jolt_bake_inspector_window::ToText(Audit._CookedState),
        ck_jolt_bake_inspector_window::ToText(Audit._CookedPreviewAvailability),
        Audit._IndividualHeuristicRepairCount, Audit._AggregateHeuristicRepairCount,
        Audit._bWouldUseHeuristic ? TEXT("yes") : TEXT("no"),
        Audit._bWouldFailBake ? TEXT("yes") : TEXT("no"),
        Audit._Failure);
}

auto SCkJoltBakeInspectorWindow::StartAnalyzeAll() -> FReply
{
    if (_AuthoredPresentationReleased || GetIsAnalyzing()) { return FReply::Handled(); }
    CancelAnalysis();
    for (const auto& Row : _AllRows) { if (NOT Row->Audit.IsSet()) { _AnalysisQueue.Add(Row); } }
    _AnalysisState.Start(_AnalysisQueue.Num());
    if (GetIsAnalyzing()) { _AnalysisJoltLease = MakeUnique<ck::jolt::FCk_Jolt_ScopedGlobalInit>(); }
    return FReply::Handled();
}
auto SCkJoltBakeInspectorWindow::CancelAnalyzeAll() -> FReply { if (NOT _AuthoredPresentationReleased) { CancelAnalysis(); } return FReply::Handled(); }
auto SCkJoltBakeInspectorWindow::CancelAnalysis() -> void { _AnalysisQueue.Reset(); _AnalysisState.Cancel(); _AnalysisJoltLease.Reset(); }
auto SCkJoltBakeInspectorWindow::SetFilterMode(int32 InMode) -> FReply { if (NOT _AuthoredPresentationReleased) { _FilterMode = InMode; RefilterRows(); } return FReply::Handled(); }
auto SCkJoltBakeInspectorWindow::GetIsAnalyzing() const -> bool { return _AnalysisState.IsActive(); }

auto SCkJoltBakeInspectorWindow::GenerateRow(FRowPtr InRow, const TSharedRef<STableViewBase>& InOwnerTable) -> TSharedRef<ITableRow>
{
    if (const TSharedPtr<SWidget> AuthoredRow = BuildAuthoredRow(InRow); AuthoredRow.IsValid())
    {
        return SNew(STableRow<FRowPtr>, InOwnerTable)[AuthoredRow.ToSharedRef()];
    }
    return SNew(STableRow<FRowPtr>, InOwnerTable)
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.0f)
            [ SNew(STextBlock).Text_Lambda([InRow] { return FText::FromString(InRow->DisplayName); }).Font(CkStyle::BoldFont(CkStyle::FontSizeSmall())) ]
            + SHorizontalBox::Slot().AutoWidth()
            [ SNew(SCkDebug_StatusPill).ShowDot(false).Text_Lambda([InRow] { return FText::FromString(InRow->Classification); }).Tone_Lambda([InRow] { return ck_jolt_bake_inspector_window::GetTone(*InRow); }) ]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, CkStyle::SpaceXS, 0.0f, 0.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.0f)
            [ SNew(STextBlock).Text_Lambda([InRow] { return FText::FromString(InRow->PackagePath); }).ColorAndOpacity(CkStyle::TextMute()) ]
            + SHorizontalBox::Slot().AutoWidth()
            [ SNew(SBox).Visibility_Lambda([InRow] { return InRow->Audit.IsSet() && InRow->Audit->_bWouldUseHeuristic ? EVisibility::Visible : EVisibility::Collapsed; })
                [ SNew(SCkDebug_Chip).Text(FText::FromString(TEXT("heuristic"))).Kind(ECkDebug_ChipKind::Neutral).ShowDot(false) ] ]
        ]
    ];
}

auto SCkJoltBakeInspectorWindow::BuildAuthoredRow(const FRowPtr& InRow) -> TSharedPtr<SWidget>
{
    if (_AuthoredPresentationReleased || _UsingNativeFallback || NOT InRow.IsValid()) { return {}; }
    if (InRow->Presentation.IsValid()) { return InRow->Presentation->GetRegion(TEXT("main")); }

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    if (NOT Plugin.IsValid() || NOT RegistryResult.Succeeded || NOT Registry.IsValid()) { return {}; }

    const TWeakPtr<FCkJoltBakeInspectorRow> WeakRow = InRow;
    auto Data = FCkUiView::FDataBindings{};
    Data.Text.Add(TEXT("jolt-bake-row-name"), TAttribute<FText>::CreateLambda([WeakRow]()
    { const auto Row = WeakRow.Pin(); return Row.IsValid() ? FText::FromString(Row->DisplayName) : FText::GetEmpty(); }));
    Data.Text.Add(TEXT("jolt-bake-row-classification"), TAttribute<FText>::CreateLambda([WeakRow]()
    { const auto Row = WeakRow.Pin(); return Row.IsValid() ? FText::FromString(Row->Classification) : FText::GetEmpty(); }));
    Data.Color.Add(TEXT("jolt-bake-row-foreground"), TAttribute<FLinearColor>::CreateLambda([WeakRow]()
    { const auto Row = WeakRow.Pin(); return ck_jolt_bake_inspector_window::GetToneColor(Row.Get()); }));
    Data.Color.Add(TEXT("jolt-bake-row-background"), TAttribute<FLinearColor>::CreateLambda([WeakRow]()
    { const auto Row = WeakRow.Pin(); return ck_jolt_bake_inspector_window::GetToneBackground(Row.Get()); }));
    Data.Text.Add(TEXT("jolt-bake-row-path"), TAttribute<FText>::CreateLambda([WeakRow]()
    { const auto Row = WeakRow.Pin(); return Row.IsValid() ? FText::FromString(Row->PackagePath) : FText::GetEmpty(); }));
    Data.Visibility.Add(TEXT("jolt-bake-row-heuristic"), TAttribute<bool>::CreateLambda([WeakRow]()
    { const auto Row = WeakRow.Pin(); return Row.IsValid() && Row->Audit.IsSet() && Row->Audit->_bWouldUseHeuristic; }));
    const TSharedRef<FCkUiView> View = FCkUiView::Create({}, {}, ck_jolt_bake_inspector_window::AuthoredStyleTokens(), CkStyle::RegularFont(CkStyle::FontSizeBody()), MoveTemp(Data), Registry);
    const TSharedRef<SWidget> Main = View->GetRegion(TEXT("main"));
    const FString Directory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    _RowMarkupPath = FPaths::Combine(Directory, TEXT("JoltBakeInspectorRow.ui.html"));
    _RowStylesheetPath = FPaths::Combine(Directory, TEXT("JoltBakeInspectorRow.ui.css"));
    View->SetFiles(_RowMarkupPath, _RowStylesheetPath);
    View->PollFiles(ck_jolt_bake_inspector_window::AuthoredStyleTokens());
    if (NOT View->GetLastResult().Succeeded) { return {}; }

    InRow->Presentation = View;
    return Main;
}

auto SCkJoltBakeInspectorWindow::BrowseSelectedAsset() -> FReply
{
    if (_AuthoredPresentationReleased || NOT _SelectedRow.IsValid() || GEditor == nullptr)
    { return FReply::Handled(); }
    const TStrongObjectPtr<UObject> Asset{_SelectedRow->Asset.GetAsset()};
    if (Asset.IsValid())
    { GEditor->SyncBrowserToObjects(TArray<UObject*>{Asset.Get()}); }
    return FReply::Handled();
}

auto SCkJoltBakeInspectorWindow::OpenSelectedAsset() -> FReply
{
    // Refresh is deliberately an explicit command only. It never runs from Tick, filtering, or attributes.
    if (_AuthoredPresentationReleased || NOT _SelectedRow.IsValid())
    { return FReply::Handled(); }
    if (GEditor != nullptr)
    {
        const TStrongObjectPtr<UObject> Asset{_SelectedRow->Asset.GetAsset()};
        if (Asset.IsValid())
        {
            const TWeakObjectPtr<UAssetEditorSubsystem> Editors{GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()};
            if (Editors.IsValid())
            { Editors->OpenEditorForAsset(Asset.Get()); }
        }
    }
    return FReply::Handled();
}

auto SCkJoltBakeInspectorWindow::BakeSelected() -> FReply
{
    if (_AuthoredPresentationReleased || NOT CanBakeSelected())
    { return FReply::Handled(); }

    const TWeakObjectPtr<UCk_JoltCook_EditorSubsystem_UE> Cooker{
        GEditor->GetEditorSubsystem<UCk_JoltCook_EditorSubsystem_UE>()};
    if (Cooker.IsValid())
    {
        // Explicit mutation only. The subsystem owns slicing, save policy, and completion; this window does not
        // reimplement any cook/write path and deliberately does not poll to infer completion.
        Cooker->Request_CookMeshShapes_ForAssets(TArray<FAssetData>{_SelectedRow->Asset});
    }
    return FReply::Handled();
}

auto SCkJoltBakeInspectorWindow::BakeAll() -> FReply
{
    if (_AuthoredPresentationReleased || NOT CanBakeAll())
    { return FReply::Handled(); }

    const TWeakObjectPtr<UCk_JoltCook_EditorSubsystem_UE> Cooker{
        GEditor->GetEditorSubsystem<UCk_JoltCook_EditorSubsystem_UE>()};
    auto Repairable = TArray<FAssetData>{};
    for (const auto& Row : _AllRows)
    {
        if (Row->Audit.IsSet() && ck::jolt_bake_inspector::Get_IsRepairableBakeAction(
            Row->Audit->_RecommendedAction, Row->Audit->_bWouldFailBake))
        { Repairable.Add(Row->Asset); }
    }
    if (Cooker.IsValid() && NOT Repairable.IsEmpty())
    { Cooker->Request_CookMeshShapes_ForAssets(Repairable); }
    return FReply::Handled();
}

auto SCkJoltBakeInspectorWindow::CanBakeSelected() const -> bool
{
    if (_AuthoredPresentationReleased || NOT _SelectedRow.IsValid() || NOT _SelectedRow->Audit.IsSet() || _SelectedRow->Audit->_bWouldFailBake || GEditor == nullptr)
    { return false; }

    return ck::jolt_bake_inspector::Get_IsRepairableBakeAction(
        _SelectedRow->Audit->_RecommendedAction, _SelectedRow->Audit->_bWouldFailBake);
}

auto SCkJoltBakeInspectorWindow::CanBakeAll() const -> bool
{
    return NOT _AuthoredPresentationReleased && GEditor != nullptr && _AllRows.ContainsByPredicate([](const FRowPtr& Row)
    {
        return Row->Audit.IsSet() && ck::jolt_bake_inspector::Get_IsRepairableBakeAction(
            Row->Audit->_RecommendedAction, Row->Audit->_bWouldFailBake);
    });
}

auto SCkJoltBakeInspectorWindow::GetSummaryText() const -> FText
{
    auto NumAnalyzed = 0;
    auto NumWouldFail = 0;
    auto NumHeuristic = 0;
    for (const auto& Row : _AllRows)
    {
        if (NOT Row->Audit.IsSet())
        { continue; }
        ++NumAnalyzed;
        NumWouldFail += Row->Audit->_bWouldFailBake ? 1 : 0;
        NumHeuristic += Row->Audit->_bWouldUseHeuristic ? 1 : 0;
    }
    return FText::FromString(ck::Format_UE(TEXT("{} baked-root meshes; {} shown; {} analyzed; {} would fail; {} repair heuristics"),
        _AllRows.Num(), _VisibleRows.Num(), NumAnalyzed, NumWouldFail, NumHeuristic)
         + (GetIsAnalyzing() ? ck::Format_UE(TEXT("; analyzing {}/{}"), _AnalysisState.Get_Processed(), _AnalysisState.Get_Total()) : FString{}));
}

auto SCkJoltBakeInspectorWindow::GetMetricText(int32 InMetric) const -> FText
{
    auto NumAnalyzed = 0;
    auto NumWouldFail = 0;
    auto NumHeuristic = 0;
    for (const auto& Row : _AllRows)
    {
        if (NOT Row->Audit.IsSet()) { continue; }
        ++NumAnalyzed;
        NumWouldFail += Row->Audit->_bWouldFailBake ? 1 : 0;
        NumHeuristic += Row->Audit->_bWouldUseHeuristic ? 1 : 0;
    }

    switch (InMetric)
    {
        case 0: return FText::AsNumber(_AllRows.Num());
        case 1: return FText::AsNumber(NumAnalyzed);
        case 2: return FText::AsNumber(NumHeuristic);
        case 3: return FText::AsNumber(NumWouldFail);
    }
    return FText::GetEmpty();
}

auto SCkJoltBakeInspectorWindow::GetSelectedSourceText() const -> FText
{
    if (NOT _SelectedRow.IsValid())
    { return FText::FromString(TEXT("Select a mesh to inspect its source collision.")); }
    if (NOT _SelectedRow->Audit.IsSet())
    { return FText::FromString(TEXT("Source audit is pending for this mesh.")); }

    const auto& Audit = _SelectedRow->Audit.GetValue();
    return FText::FromString(ck::Format_UE(
        TEXT("Package: {}\nState: {}\nTriangles: {}\nSource winding: {} ({})\nNormalized winding: {} ({})\n"
             "Repairs: {} individual, {} aggregate no-verdict"),
        _SelectedRow->PackagePath,
        ck_jolt_bake_inspector_window::ToText(Audit._SourceState),
        Audit._SourceTriangleCount,
        ck_jolt_bake_inspector_window::ToText(Audit._SourceWinding), Audit._SourceWindingRatio,
        ck_jolt_bake_inspector_window::ToText(Audit._NormalizedSourceWinding), Audit._NormalizedSourceWindingRatio,
        Audit._IndividualHeuristicRepairCount, Audit._AggregateHeuristicRepairCount));
}

auto SCkJoltBakeInspectorWindow::GetSelectedCookedText() const -> FText
{
    if (NOT _SelectedRow.IsValid())
    { return FText::FromString(TEXT("Select a mesh to inspect its generated Jolt shape.")); }
    if (NOT _SelectedRow->Audit.IsSet())
    { return FText::FromString(TEXT("Cooked-shape audit is pending for this mesh.")); }

    const auto& Audit = _SelectedRow->Audit.GetValue();
    return FText::FromString(ck::Format_UE(
        TEXT("Asset: {}\nState: {}\nPreview: {}{}\nCooked winding: {} ({})"),
        Audit._CookedShapeObjectPath,
        ck_jolt_bake_inspector_window::ToText(Audit._CookedState),
        ck_jolt_bake_inspector_window::ToText(Audit._CookedPreviewAvailability),
        Audit._bCookedPreviewTruncated ? TEXT(" (triangle cap reached)") : TEXT(""),
        ck_jolt_bake_inspector_window::ToText(Audit._CookedWinding), Audit._CookedWindingRatio));
}

auto SCkJoltBakeInspectorWindow::GetSelectedDiagnosisText() const -> FText
{
    if (NOT _SelectedRow.IsValid())
    { return FText::FromString(TEXT("Select a mesh. The preview and read-only audit appear here.")); }
    if (NOT _SelectedRow->Audit.IsSet())
    { return FText::FromString(_SelectedRow->Detail); }

    const auto& Audit = _SelectedRow->Audit.GetValue();
    const auto FailureSuffix = Audit._Failure.IsEmpty()
        ? FString{}
        : ck::Format_UE(TEXT("\n{}"), Audit._Failure);
    return FText::FromString(ck::Format_UE(TEXT("Recommended action: {}\nHeuristic repair: {}\nWould fail bake: {}{}"),
        _SelectedRow->Classification,
        Audit._bWouldUseHeuristic ? TEXT("yes") : TEXT("no"),
        Audit._bWouldFailBake ? TEXT("yes") : TEXT("no"),
        FailureSuffix));
}
