#include "CkInspector_Minimap.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Settings/CkDebuggerStyleSettings.h"
#include "CkDebuggerCommon/Styles/CkDebuggerAxes.h"
#include "CkDebuggerCommon/Utils/CkDebug_InspectorEditGuard.h"
#include "CkDebuggerCommon/Utils/CkDebug_RequestGate.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NumericEditor.h"
#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEditorTools/Style/CkStyle.h"
#include "CkMinimap/CkMinimap_Fragment.h"
#include "CkMinimap/CkMinimap_Utils.h"
#include "CkSlateLayout/SCkUiSurface.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Minimap)

namespace ck_inspector_minimap
{
auto TryGet(const FCk_Handle &InEntity, FCk_Handle_Minimap &OutMinimap) -> bool
{
    OutMinimap = {};
    if (ck::Is_NOT_Valid(InEntity) || NOT UCk_Utils_Minimap_UE::Has(InEntity))
    {
        return false;
    }
    auto Mutable = InEntity;
    OutMinimap = UCk_Utils_Minimap_UE::Cast(Mutable);
    return ck::IsValid(OutMinimap) && OutMinimap.Has<ck::FFragment_Minimap_Params>() &&
           OutMinimap.Has<ck::FFragment_Minimap_Current>();
}
auto CanEdit(const FCk_Handle &InEntity) -> bool
{
    FCk_Handle_Minimap Minimap;
    return TryGet(InEntity, Minimap) &&
           ck::DebugRequestGate::Evaluate(InEntity, ECk_DebugRequest_Requirement::CosmeticOnly).IsEnabled;
}
auto Projection(const FCk_Handle &E) -> FString
{
    FCk_Handle_Minimap M;
    return TryGet(E, M) ? ck::Format_UE(TEXT("{}"), UCk_Utils_Minimap_UE::Get_ProjectionMode(M)) : TEXT("--");
}
auto Rotation(const FCk_Handle &E) -> FString
{
    FCk_Handle_Minimap M;
    return TryGet(E, M) ? ck::Format_UE(TEXT("{}"), UCk_Utils_Minimap_UE::Get_RotationMode(M)) : TEXT("--");
}
auto Frame(const FCk_Handle &E) -> FString
{
    FCk_Handle_Minimap M;
    return TryGet(E, M) ? ck::Format_UE(TEXT("{}"), UCk_Utils_Minimap_UE::Get_FrameShape(M)) : TEXT("--");
}
auto Extent(const FCk_Handle &E) -> float
{
    FCk_Handle_Minimap M;
    return TryGet(E, M) ? UCk_Utils_Minimap_UE::Get_ViewExtent(M) : 0.0f;
}
auto Origin(const FCk_Handle &E) -> FString
{
    FCk_Handle_Minimap M;
    if (NOT TryGet(E, M))
        return TEXT("--");
    const FVector V = UCk_Utils_Minimap_UE::Get_ViewOrigin(M);
    return ck::Format_UE(TEXT("{:.0f}, {:.0f}, {:.0f}"), V.X, V.Y, V.Z);
}
auto Make_AxisComponents(const FCk_Handle& InEntity) -> TArray<TAttribute<FText>>
{
    auto Components = TArray<TAttribute<FText>>{};
    Components.Reserve(3);
    for (auto Axis = 0; Axis < 3; ++Axis)
    {
        Components.Emplace(TAttribute<FText>::CreateLambda([InEntity, Axis]()
        {
            auto Minimap = FCk_Handle_Minimap{};
            if (NOT TryGet(InEntity, Minimap))
            { return FText::FromString(TEXT("--")); }

            return FText::FromString(ck::Format_UE(
                TEXT("{:.0f}"), UCk_Utils_Minimap_UE::Get_ViewOrigin(Minimap)[Axis]));
        }));
    }
    return Components;
}
auto Yaw(const FCk_Handle &E) -> FString
{
    FCk_Handle_Minimap M;
    return TryGet(E, M) ? ck::Format_UE(TEXT("{:.1f}°"), UCk_Utils_Minimap_UE::Get_ViewYawDegrees(M)) : TEXT("--");
}
auto Observer(const FCk_Handle &E) -> FCk_Handle
{
    FCk_Handle_Minimap M;
    return TryGet(E, M) ? UCk_Utils_Minimap_UE::Get_Observer(M) : FCk_Handle{};
}
auto Entries(const FCk_Handle &E) -> FString
{
    FCk_Handle_Minimap M;
    if (NOT TryGet(E, M))
        return TEXT("--");
    return ck::Format_UE(TEXT("{} / {}"), UCk_Utils_Minimap_UE::Get_Entries(M).Num(),
                         M.Get<ck::FFragment_Minimap_Params>().Get_MaxEntries());
}
auto EntriesFraction(const FCk_Handle &E) -> float
{
    FCk_Handle_Minimap M;
    if (NOT TryGet(E, M))
        return 0;
    const int32 Max = M.Get<ck::FFragment_Minimap_Params>().Get_MaxEntries();
    return Max > 0 ? float(UCk_Utils_Minimap_UE::Get_Entries(M).Num()) / float(Max) : 0;
}
auto Bounds(const FCk_Handle &E) -> FString
{
    FCk_Handle_Minimap M;
    if (NOT TryGet(E, M))
        return TEXT("--");
    if (UCk_Utils_Minimap_UE::Get_ProjectionMode(M) != ECk_Minimap_ProjectionMode::FixedBounds)
        return TEXT("(observer-centric)");
    const auto B = UCk_Utils_Minimap_UE::Get_FixedBounds(M);
    return ck::Format_UE(TEXT("C({:.0f}, {:.0f})  HE({:.0f}, {:.0f})"), B.Get_Center().X, B.Get_Center().Y,
                         B.Get_HalfExtents().X, B.Get_HalfExtents().Y);
}
auto Diff(const bool InMarked) -> FLinearColor { return InMarked ? CkStyle::Accent() : CkStyle::Text(); }
} // namespace ck_inspector_minimap

auto SCkInspector_MinimapAuthored::Construct(const FArguments &Args) -> void
{
    _Entity = Args._Entity;
    _DiffLabels = Args._DiffLabels;
    _ViewExtentEditScope = MakeShared<FCkInspectorEditScope>(Args._EditGuard);
    const TSharedRef<SBox> Host = SNew(SBox);
    ChildSlot[Host];
    if (Build_AuthoredView())
    {
        Host->SetContent(_View->GetRegion(TEXT("main")));
        return;
    }
    Release();
    Host->SetContent(SNullWidget::NullWidget);
}
SCkInspector_MinimapAuthored::~SCkInspector_MinimapAuthored() { Release(); }
auto SCkInspector_MinimapAuthored::Get_IsAvailable() const -> bool
{
    FCk_Handle_Minimap M;
    return _Active && ck_inspector_minimap::TryGet(_Entity, M);
}
auto SCkInspector_MinimapAuthored::Get_ProjectionText() const -> FString
{
    return _Active ? ck_inspector_minimap::Projection(_Entity) : FString{};
}
auto SCkInspector_MinimapAuthored::Get_RotationText() const -> FString
{
    return _Active ? ck_inspector_minimap::Rotation(_Entity) : FString{};
}
auto SCkInspector_MinimapAuthored::Get_FrameText() const -> FString
{
    return _Active ? ck_inspector_minimap::Frame(_Entity) : FString{};
}
auto SCkInspector_MinimapAuthored::Get_ViewExtent() const -> float
{
    return _Active ? ck_inspector_minimap::Extent(_Entity) : 0;
}
auto SCkInspector_MinimapAuthored::Get_ViewOriginText() const -> FString
{
    return _Active ? ck_inspector_minimap::Origin(_Entity) : FString{};
}
auto SCkInspector_MinimapAuthored::Get_ViewYawText() const -> FString
{
    return _Active ? ck_inspector_minimap::Yaw(_Entity) : FString{};
}
auto SCkInspector_MinimapAuthored::Get_Observer() const -> FCk_Handle
{
    return _Active ? ck_inspector_minimap::Observer(_Entity) : FCk_Handle{};
}
auto SCkInspector_MinimapAuthored::Get_EntriesText() const -> FString
{
    return _Active ? ck_inspector_minimap::Entries(_Entity) : FString{};
}
auto SCkInspector_MinimapAuthored::Get_EntriesFraction() const -> float
{
    return _Active ? ck_inspector_minimap::EntriesFraction(_Entity) : 0;
}
auto SCkInspector_MinimapAuthored::Get_FixedBoundsText() const -> FString
{
    return _Active ? ck_inspector_minimap::Bounds(_Entity) : FString{};
}
auto SCkInspector_MinimapAuthored::Commit_Rotation(const int32 Index) -> void
{
    FCk_Handle_Minimap M;
    if (_Active && ck_inspector_minimap::TryGet(_Entity, M) && ck_inspector_minimap::CanEdit(_Entity))
        UCk_Utils_Minimap_UE::Request_SetRotationMode(
            M,
            FCk_Request_Minimap_SetRotationMode{Index == 1 ? ECk_Minimap_RotationMode::RotateWithObserver
                                                           : ECk_Minimap_RotationMode::NorthLocked},
            {});
}
auto SCkInspector_MinimapAuthored::Commit_ViewExtent(const float V) -> void
{
    FCk_Handle_Minimap M;
    if (_Active && ck_inspector_minimap::TryGet(_Entity, M) && ck_inspector_minimap::CanEdit(_Entity))
        UCk_Utils_Minimap_UE::Request_SetViewExtent(M, FCk_Request_Minimap_SetViewExtent{V}, {});
}
auto SCkInspector_MinimapAuthored::Build_RotationValue() -> TSharedRef<SWidget>
{
    _RotationOptions = {MakeShared<FString>(TEXT("NorthLocked")), MakeShared<FString>(TEXT("RotateWithObserver"))};
    const TWeakPtr<SCkInspector_MinimapAuthored> Weak{SharedThis(this)};
    const TSharedRef<SComboBox<TSharedPtr<FString>>> Box =
        SNew(SComboBox<TSharedPtr<FString>>)
            .OptionsSource(&_RotationOptions)
            .OnGenerateWidget_Lambda([](TSharedPtr<FString> V)
                                     { return SNew(STextBlock).Text(FText::FromString(V.IsValid() ? *V : FString{})); })
            .OnSelectionChanged_Lambda(
                [Weak](TSharedPtr<FString> V, ESelectInfo::Type Why)
                {
                    if (Why != ESelectInfo::Direct && V.IsValid())
                        if (const auto P = Weak.Pin(); P.IsValid())
                            P->Commit_Rotation(*V == TEXT("RotateWithObserver") ? 1 : 0);
                })[SNew(STextBlock)
                       .Text_Lambda(
                           [Weak]()
                           {
                               const auto P = Weak.Pin();
                               return P.IsValid() ? FText::FromString(P->Get_RotationText()) : FText::GetEmpty();
                           })];
    const TSharedRef<SBox> InputHost = SNew(SBox)
        .Tag(TEXT("minimap-rotation-input"))
        .IsEnabled_Lambda([Weak]()
        {
            const TSharedPtr<SCkInspector_MinimapAuthored> Widget = Weak.Pin();
            return Widget.IsValid() && Widget->Get_IsAvailable()
                && ck_inspector_minimap::CanEdit(Widget->_Entity);
        })
        [Box];
    Box->SetToolTipText(TAttribute<FText>::CreateLambda(
        [Weak]()
        {
            const auto P = Weak.Pin();
            if (!P.IsValid())
                return FText::GetEmpty();
            const auto Gate = ck::DebugRequestGate::Evaluate(P->_Entity, ECk_DebugRequest_Requirement::CosmeticOnly);
            return Gate.Reason;
        }));
    const FCkDebuggerStyleSelection& Selection = UCkDebuggerStyleSettings::Get_Selection();
    if (NOT ck::debug_axes::EditControls_AreVisible(Selection))
    {
        return SNew(STextBlock)
            .Tag(FName{TEXT("minimap-rotation-read-only")})
            .Text_Lambda([Weak]()
            {
                const TSharedPtr<SCkInspector_MinimapAuthored> Widget = Weak.Pin();
                return Widget.IsValid() ? FText::FromString(Widget->Get_RotationText()) : FText::GetEmpty();
            });
    }
    if (NOT ck::debug_axes::EditControls_RevealOnHover(Selection)) { return InputHost; }

    const TSharedRef<SBox> HoverHost = SNew(SBox).Tag(FName{TEXT("minimap-rotation-hover-host")});
    const TWeakPtr<SBox> WeakHoverHost{HoverHost};
    const auto IsHovered = [WeakHoverHost]()
    {
        const TSharedPtr<SBox> Host = WeakHoverHost.Pin();
        return Host.IsValid() && Host->IsHovered();
    };
    const TSharedRef<STextBlock> ReadOnly = SNew(STextBlock)
        .Tag(FName{TEXT("minimap-rotation-read-only")})
        .Text_Lambda([Weak]()
        {
            const TSharedPtr<SCkInspector_MinimapAuthored> Widget = Weak.Pin();
            return Widget.IsValid() ? FText::FromString(Widget->Get_RotationText()) : FText::GetEmpty();
        });
    ReadOnly->SetVisibility(TAttribute<EVisibility>::CreateLambda([IsHovered]()
    { return IsHovered() ? EVisibility::Hidden : EVisibility::Visible; }));
    InputHost->SetVisibility(TAttribute<EVisibility>::CreateLambda([IsHovered]()
    { return IsHovered() ? EVisibility::Visible : EVisibility::Hidden; }));
    HoverHost->SetContent(SNew(SOverlay) + SOverlay::Slot()[ReadOnly] + SOverlay::Slot()[InputHost]);
    return HoverHost;
}
auto SCkInspector_MinimapAuthored::Build_ViewExtentValue() -> TSharedRef<SWidget>
{
    const TWeakPtr<SCkInspector_MinimapAuthored> Weak{SharedThis(this)};
    const TSharedPtr<FCkInspectorEditScope> Scope = _ViewExtentEditScope;
    const TSharedRef<SCkDebug_NumericEditor> Editor =
        SNew(SCkDebug_NumericEditor)
            .Tag(TEXT("minimap-view-extent-input"))
            .Value_Lambda(
                [Weak]()
                {
                    const auto P = Weak.Pin();
                    return P.IsValid() ? double(P->Get_ViewExtent()) : 0.0;
                })
            .Kind(ECkDebug_NumericKind::Float)
            .MinValue(1.0)
            .Width(78)
            .OnValueCommitted_Lambda(
                [Weak](double V)
                {
                    if (const auto P = Weak.Pin(); P.IsValid())
                        P->Commit_ViewExtent(float(V));
                })
            .OnEditStateChanged_Lambda(
                [Scope](bool A)
                {
                    if (Scope.IsValid())
                        Scope->Set_Active(A);
                });
    Editor->SetEnabled(TAttribute<bool>::CreateLambda(
        [Weak]()
        {
            const auto P = Weak.Pin();
            return P.IsValid() && P->Get_IsAvailable() && ck_inspector_minimap::CanEdit(P->_Entity);
        }));
    Editor->SetToolTipText(TAttribute<FText>::CreateLambda(
        [Weak]()
        {
            const auto P = Weak.Pin();
            if (!P.IsValid())
                return FText::GetEmpty();
            return ck::DebugRequestGate::Evaluate(P->_Entity, ECk_DebugRequest_Requirement::CosmeticOnly).Reason;
        }));
    const FCkDebuggerStyleSelection& Selection = UCkDebuggerStyleSettings::Get_Selection();
    if (NOT ck::debug_axes::EditControls_AreVisible(Selection))
    {
        return SNew(STextBlock)
            .Tag(FName{TEXT("minimap-view-extent-read-only")})
            .Text_Lambda([Weak]()
            {
                const TSharedPtr<SCkInspector_MinimapAuthored> Widget = Weak.Pin();
                return Widget.IsValid()
                    ? FText::FromString(ck::Format_UE(TEXT("{}"), Widget->Get_ViewExtent()))
                    : FText::GetEmpty();
            })
            .ColorAndOpacity(CkStyle::Value_Numeric());
    }
    if (NOT ck::debug_axes::EditControls_RevealOnHover(Selection))
    { return Editor; }

    const TSharedRef<SBox> HoverHost = SNew(SBox).Tag(FName{TEXT("minimap-view-extent-hover-host")});
    const TWeakPtr<SBox> WeakHoverHost{HoverHost};
    const auto IsHovered = [WeakHoverHost]()
    {
        const TSharedPtr<SBox> Host = WeakHoverHost.Pin();
        return Host.IsValid() && Host->IsHovered();
    };
    const TSharedRef<STextBlock> ReadOnly = SNew(STextBlock)
        .Tag(FName{TEXT("minimap-view-extent-read-only")})
        .Text_Lambda([Weak]()
        {
            const TSharedPtr<SCkInspector_MinimapAuthored> Widget = Weak.Pin();
            return Widget.IsValid()
                ? FText::FromString(ck::Format_UE(TEXT("{}"), Widget->Get_ViewExtent()))
                : FText::GetEmpty();
        })
        .ColorAndOpacity(CkStyle::TextDim());
    ReadOnly->SetVisibility(TAttribute<EVisibility>::CreateLambda([IsHovered]()
    { return IsHovered() ? EVisibility::Hidden : EVisibility::Visible; }));
    Editor->SetVisibility(TAttribute<EVisibility>::CreateLambda([IsHovered]()
    { return IsHovered() ? EVisibility::Visible : EVisibility::Hidden; }));
    HoverHost->SetContent(SNew(SOverlay)
        + SOverlay::Slot()[ReadOnly]
        + SOverlay::Slot()[Editor]);
    return HoverHost;
}
auto SCkInspector_MinimapAuthored::Build_AuthoredView() -> bool
{
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult Result = FCkDebug_UiRegistry::TryCreate(Registry);
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT Result.Succeeded || NOT Registry.IsValid() || NOT Plugin.IsValid())
    {
        auto Errors = Result.Errors;
        if (NOT Registry.IsValid())
        { Errors.Add(TEXT("Debugger UI registry is unavailable.")); }
        if (NOT Plugin.IsValid())
        { Errors.Add(TEXT("CkDebugger plugin is unavailable.")); }
        _LoadError = FString::Join(Errors, TEXT("\n"));
        return false;
    }
    const TWeakPtr<SCkInspector_MinimapAuthored> Weak{SharedThis(this)};
    FCkUiView::FNativeBindings Ports;
    Ports.Add(TEXT("minimap-rotation-port"), Build_RotationValue());
    Ports.Add(TEXT("minimap-view-extent-port"), Build_ViewExtentValue());
    Ports.Add(TEXT("minimap-observer-port"), SNew(SCkDebug_EntityRef)
                                                 .Entity_Lambda(
                                                     [Weak]()
                                                     {
                                                         const auto P = Weak.Pin();
                                                         return P.IsValid() ? P->Get_Observer() : FCk_Handle{};
                                                     })
                                                 .ShowName(true));
    FCkUiView::FDataBindings Data;
    const auto Bind = [&](const FString &N, FString (SCkInspector_MinimapAuthored::*G)() const)
    {
        Data.Text.Add(N, TAttribute<FText>::CreateLambda(
                             [Weak, G]()
                             {
                                 const auto P = Weak.Pin();
                                 return P.IsValid() && !P->Is_Inert() ? FText::FromString((P.Get()->*G)())
                                                                      : FText::GetEmpty();
                             }));
    };
    Bind(TEXT("minimap-projection"), &SCkInspector_MinimapAuthored::Get_ProjectionText);
    Bind(TEXT("minimap-rotation"), &SCkInspector_MinimapAuthored::Get_RotationText);
    Bind(TEXT("minimap-origin"), &SCkInspector_MinimapAuthored::Get_ViewOriginText);
    Bind(TEXT("minimap-yaw"), &SCkInspector_MinimapAuthored::Get_ViewYawText);
    Bind(TEXT("minimap-entries"), &SCkInspector_MinimapAuthored::Get_EntriesText);
    Bind(TEXT("minimap-bounds"), &SCkInspector_MinimapAuthored::Get_FixedBoundsText);
    Data.Number.Add(TEXT("minimap-entries-fraction"), TAttribute<float>::CreateLambda(
                                                          [Weak]()
                                                          {
                                                              const auto P = Weak.Pin();
                                                              return P.IsValid() ? P->Get_EntriesFraction() : 0;
                                                          }));
    Data.Color.Add(TEXT("minimap-entries-fill"),
                   TAttribute<FLinearColor>::CreateLambda([]() { return CkStyle::GetToneColor(ECk_Tone::Accent); }));
    Data.Text.Add(TEXT("minimap-frame"), TAttribute<FText>::CreateLambda(
                                             [Weak]()
                                             {
                                                 const auto P = Weak.Pin();
                                                 return P.IsValid() && !P->Is_Inert()
                                                            ? FText::FromString(P->Get_FrameText())
                                                            : FText::GetEmpty();
                                             }));
    for (const FString &L :
         {TEXT("Projection:"), TEXT("Rotation:"), TEXT("Frame:"), TEXT("View Extent:"), TEXT("View Origin:"),
          TEXT("View Yaw:"), TEXT("Observer:"), TEXT("Entries:"), TEXT("Fixed Bounds:")})
    {
        const FString Key =
            TEXT("minimap-") + L.LeftChop(1).Replace(TEXT(" "), TEXT("-")).ToLower() + TEXT("-diff-color");
        Data.Color.Add(Key, TAttribute<FLinearColor>::CreateLambda(
                                [Weak, L]()
                                {
                                    const auto P = Weak.Pin();
                                    return P.IsValid() && !P->Is_Inert()
                                               ? ck_inspector_minimap::Diff(P->Is_DiffMarked(L))
                                               : FLinearColor::Transparent;
                                }));
    }
    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(
        MoveTemp(Ports), {}, {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString Root = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(FPaths::Combine(Root, TEXT("EcsInspectorMinimap.ui.html")),
                        FPaths::Combine(Root, TEXT("EcsInspectorMinimap.ui.css")));
    Candidate->PollFiles();
    if (NOT Candidate->GetLastResult().Succeeded)
    {
        _LoadError = FString::Join(Candidate->GetLastResult().Errors, TEXT("\n"));
        return false;
    }
    _View = Candidate;
    _Mounted = true;
    _LoadError.Reset();
    return true;
}
auto SCkInspector_MinimapAuthored::Tick(const FGeometry &G, double T, float D) -> void
{
    SCompoundWidget::Tick(G, T, D);
    if (NOT _Active || NOT _View.IsValid())
    { return; }

    _View->PollFiles();
    if (NOT _View->GetLastResult().Succeeded)
    { _LoadError = FString::Join(_View->GetLastResult().Errors, TEXT("\n")); }
    else
    { _LoadError.Reset(); }
}
auto SCkInspector_MinimapAuthored::Release() -> void
{
    if (NOT _Active)
        return;
    _Active = false;
    if (_ViewExtentEditScope.IsValid())
        _ViewExtentEditScope->Set_Active(false);
    _Entity = {};
    _View.Reset();
    _ViewExtentEditScope.Reset();
    _Mounted = false;
}

FCkInspector_Minimap::~FCkInspector_Minimap() { OnDeactivated(); }
auto FCkInspector_Minimap::Get_ComponentName() const -> FText { return FText::FromString(TEXT("Minimap")); }
auto FCkInspector_Minimap::CanInspect(const FCk_Handle &E) const -> bool
{
    FCk_Handle_Minimap M;
    return ck_inspector_minimap::TryGet(E, M);
}
auto FCkInspector_Minimap::Build_NativeBody(const FCk_Handle &E) const -> TSharedRef<SWidget>
{
    auto B = FCkInspectorWidgetBuilder();
    B.SetEditGuard(Get_EditGuard());
    const FCk_Handle C = E;
    B.AddRow(
        FText::FromString(TEXT("Projection:")), [C](const FCk_Handle &)
        { return FText::FromString(ck_inspector_minimap::Projection(C)); }, CkStyle::Value_Enum());
    B.AddEnumDropdownRow(
        FText::FromString(TEXT("Rotation:")),
        {FText::FromString(TEXT("NorthLocked")), FText::FromString(TEXT("RotateWithObserver"))},
        TAttribute<int32>::CreateLambda(
            [C]()
            {
                FCk_Handle_Minimap M;
                return ck_inspector_minimap::TryGet(C, M) &&
                               UCk_Utils_Minimap_UE::Get_RotationMode(M) == ECk_Minimap_RotationMode::RotateWithObserver
                           ? 1
                           : 0;
            }),
        [C](int32 I)
        {
            FCk_Handle_Minimap M;
            if (ck_inspector_minimap::TryGet(C, M) && ck_inspector_minimap::CanEdit(C))
                UCk_Utils_Minimap_UE::Request_SetRotationMode(
                    M,
                    FCk_Request_Minimap_SetRotationMode{I == 1 ? ECk_Minimap_RotationMode::RotateWithObserver
                                                               : ECk_Minimap_RotationMode::NorthLocked},
                    {});
        },
        ECk_DebugRequest_Requirement::CosmeticOnly);
    B.AddRow(
        FText::FromString(TEXT("Frame:")),
        [C](const FCk_Handle &) { return FText::FromString(ck_inspector_minimap::Frame(C)); }, CkStyle::Value_Enum());
    B.AddNumericRow(
        FText::FromString(TEXT("View Extent:")),
        TAttribute<float>::CreateLambda([C]() { return ck_inspector_minimap::Extent(C); }),
        [C](float V)
        {
            FCk_Handle_Minimap M;
            if (ck_inspector_minimap::TryGet(C, M) && ck_inspector_minimap::CanEdit(C))
                UCk_Utils_Minimap_UE::Request_SetViewExtent(M, FCk_Request_Minimap_SetViewExtent{V}, {});
        },
        1.0f, {}, ECk_DebugRequest_Requirement::CosmeticOnly);
    B.AddAlignedNumericRow(
        FText::FromString(TEXT("View Origin:")),
        ck_inspector_minimap::Make_AxisComponents(C));
    B.AddRow(
        FText::FromString(TEXT("View Yaw:")),
        [C](const FCk_Handle &) { return FText::FromString(ck_inspector_minimap::Yaw(C)); }, CkStyle::Value_Numeric());
    B.AddWidgetRow(
        FText::FromString(TEXT("Observer:")),
        SNew(SCkDebug_EntityRef).Entity_Lambda([C]() { return ck_inspector_minimap::Observer(C); }).ShowName(true),
        [C]()
        {
            const FCk_Handle ObserverHandle = ck_inspector_minimap::Observer(C);
            return ck::IsValid(ObserverHandle) ? ObserverHandle.Get_Entity().ToString() : FString{TEXT("Invalid")};
        });
    B.AddMeterRow(
        FText::FromString(TEXT("Entries:")),
        TAttribute<float>::CreateLambda([C]() { return ck_inspector_minimap::EntriesFraction(C); }), ECk_Tone::Accent,
        TAttribute<FText>::CreateLambda([C]() { return FText::FromString(ck_inspector_minimap::Entries(C)); }));
    B.AddRow(
        FText::FromString(TEXT("Fixed Bounds:")), [C](const FCk_Handle &)
        { return FText::FromString(ck_inspector_minimap::Bounds(C)); }, CkStyle::Value_Numeric());
    return B.Build(E, FString());
}
auto FCkInspector_Minimap::Build_Inspector(const FCk_Handle &E) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> Native = Build_NativeBody(E);
    if (FCkInspector_RowCaptureScope::Is_Active())
        return Native;
    TSet<FString> D;
    for (const FString &L :
         {TEXT("Projection:"), TEXT("Rotation:"), TEXT("Frame:"), TEXT("View Extent:"), TEXT("View Origin:"),
          TEXT("View Yaw:"), TEXT("Observer:"), TEXT("Entries:"), TEXT("Fixed Bounds:")})
        if (FCkInspector_DiffMarkScope::Is_LabelMarked(L))
            D.Add(L);
    const TSharedRef<SCkInspector_MinimapAuthored> A =
        SNew(SCkInspector_MinimapAuthored).Entity(E).EditGuard(Get_EditGuard()).DiffLabels(D);
    if (NOT A->Is_Mounted())
    {
        _LastAuthoredLoadError = A->Get_LoadError();
        return Native;
    }
    _LastAuthoredLoadError.Reset();
    _AuthoredInstances.Add(A);
    return A;
}
auto FCkInspector_Minimap::Tick(const FCk_Handle &, float) -> void
{
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_MinimapAuthored> &W)
                                 { return !W.IsValid() || W.Pin()->Is_Inert(); });
}
auto FCkInspector_Minimap::OnDeactivated() -> void
{
    for (const auto &W : _AuthoredInstances)
        if (const auto P = W.Pin(); P.IsValid())
            P->Release();
    _AuthoredInstances.Reset();
}
