#include "CkInspector_SceneNode.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkDebuggerCommon/Navigation/CkDebug_Navigator.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/Registry/CkRegistry.h"
#include "CkEcsExt/SceneNode/CkSceneNode_Fragment.h"
#include "CkEcsExt/SceneNode/CkSceneNode_Utils.h"
#include "CkEcsExt/Transform/CkTransform_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"
#include "CkDebuggerCommon/UI/CkDebug_UiRegistry.h"
#include "CkDebuggerCommon/Utils/CkDebug_InspectorEditGuard.h"
#include "CkDebuggerCommon/Utils/CkDebug_RequestGate.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_NumericEditor.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkSlateLayout/CkUiCollection.h"
#include "CkSlateLayout/SCkUiSurface.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SBoxPanel.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_SceneNode)

// ====================================================================================================

namespace ck_inspector_scenenode
{
    auto TryGet(const FCk_Handle& InEntity, FCk_Handle_SceneNode& OutNode) -> bool
    {
        OutNode = {};
        if (ck::Is_NOT_Valid(InEntity) || NOT UCk_Utils_SceneNode_UE::Has(InEntity)
            || NOT UCk_Utils_Transform_UE::Has(InEntity)) { return false; }
        auto Mutable = InEntity;
        OutNode = UCk_Utils_SceneNode_UE::Cast(Mutable);
        if (NOT ck::IsValid(OutNode)) { return false; }
        const FCk_Handle_Transform Parent = UCk_Utils_SceneNode_UE::Get_Parent(OutNode);
        return ck::IsValid(Parent) && UCk_Utils_Transform_UE::Has(Parent);
    }

    auto CanRequest(const FCk_Handle& InEntity) -> bool
    {
        FCk_Handle_SceneNode Node;
        return TryGet(InEntity, Node)
            && ck::DebugRequestGate::Evaluate(InEntity, ECk_DebugRequest_Requirement::LocalOk).IsEnabled;
    }

    auto DisabledReason(const FCk_Handle& InEntity) -> FString
    {
        FCk_Handle_SceneNode Node;
        return TryGet(InEntity, Node)
            ? ck::DebugRequestGate::Evaluate(InEntity, ECk_DebugRequest_Requirement::LocalOk).Reason.ToString()
            : TEXT("SceneNode, Transform, or current parent is unavailable.");
    }

    auto Diff(const bool bMarked) -> FLinearColor { return bMarked ? CkStyle::Accent() : CkStyle::Text(); }
    auto Get_LayerIndex(const FCk_Handle& E) -> int32
    {
        if (ck::Is_NOT_Valid(E)) { return INDEX_NONE; }
        if (E.Has<ck::FTag_SceneNode_Layer0>()) { return 0; }
        if (E.Has<ck::FTag_SceneNode_Layer1>()) { return 1; }
        if (E.Has<ck::FTag_SceneNode_Layer2>()) { return 2; }
        if (E.Has<ck::FTag_SceneNode_Layer3>()) { return 3; }
        if (E.Has<ck::FTag_SceneNode_Layer4>()) { return 4; }
        if (E.Has<ck::FTag_SceneNode_Layer5>()) { return 5; }
        if (E.Has<ck::FTag_SceneNode_Layer6>()) { return 6; }
        if (E.Has<ck::FTag_SceneNode_Layer7>()) { return 7; }
        if (E.Has<ck::FTag_SceneNode_Layer8>()) { return 8; }
        if (E.Has<ck::FTag_SceneNode_Layer9>()) { return 9; }
        return INDEX_NONE;
    }

    auto GetHandleIdentityKey(const FCk_Handle& InHandle) -> FString
    {
        if (ck::Is_NOT_Valid(InHandle)) { return {}; }
        const FCk_Entity& Entity = InHandle.Get_Entity();
        const FCk_RegistryHandle Registry = InHandle.Get_RegistryView().Get_RegistryHandle();
        return FString::Printf(TEXT("registry|%d|%d/entity|%u|%u"),
            Registry.SlotIndex,
            Registry.Generation,
            static_cast<uint32>(Entity.Get_EntityNumber()),
            static_cast<uint32>(Entity.Get_VersionNumber()));
    }

    auto Gather_Siblings(const FCk_Handle& Entity) -> TArray<FCk_Handle>
    {
        auto Out = TArray<FCk_Handle>{};

        if (ck::Is_NOT_Valid(Entity) || NOT UCk_Utils_SceneNode_UE::Has(Entity))
        { return Out; }

        auto NodeMutable = Entity;
        const auto Node = UCk_Utils_SceneNode_UE::Cast(NodeMutable);
        if (ck::Is_NOT_Valid(Node))
        { return Out; }

        auto Parent = UCk_Utils_SceneNode_UE::Get_Parent(Node);
        if (ck::Is_NOT_Valid(Parent))
        { return Out; }

        UCk_Utils_SceneNode_UE::ForEach_SceneNode(Parent,
            [&Out](FCk_Handle_SceneNode InSibling) -> void
            {
                if (ck::IsValid(InSibling))
                {
                    Out.Add(FCk_Handle(InSibling));
                }
            });

        return Out;
    }

    auto Get_RelativeTransform(const FCk_Handle& InEntity) -> FTransform
    {
        if (ck::Is_NOT_Valid(InEntity) || NOT UCk_Utils_SceneNode_UE::Has(InEntity))
        { return FTransform::Identity; }

        auto Mut = InEntity;
        const auto Node = UCk_Utils_SceneNode_UE::Cast(Mut);
        if (ck::Is_NOT_Valid(Node))
        { return FTransform::Identity; }

        return UCk_Utils_SceneNode_UE::Get_Offset(Node);
    }

    auto Get_LatestRequestedOrCurrentOffset(const FCk_Handle_SceneNode& InNode) -> FTransform
    {
        if (InNode.Has<ck::FFragment_SceneNode_Requests>())
        {
            const auto& Requests = InNode.Get<ck::FFragment_SceneNode_Requests>().Get_Requests();
            if (NOT Requests.IsEmpty())
            {
                return std::get<FCk_Request_SceneNode_UpdateRelativeTransform>(Requests.Last())
                    .Get_NewRelativeTransform();
            }
        }

        return UCk_Utils_SceneNode_UE::Get_Offset(InNode);
    }

    auto Get_WorldTransform(const FCk_Handle& InEntity) -> FTransform
    {
        if (ck::Is_NOT_Valid(InEntity) || NOT UCk_Utils_Transform_UE::Has(InEntity))
        { return FTransform::Identity; }

        return UCk_Utils_Transform_TypeUnsafe_UE::Get_EntityCurrentTransform(InEntity);
    }

    // Three fixed-precision components in X/Y/Z order, matching the Transform inspector so the two
    // surfaces read the same and the axis coloring means the same thing on both.
    auto Make_AxisComponents(
        const FCk_Handle& InEntity,
        TFunction<FTransform(const FCk_Handle&)> InSource,
        TFunction<FVector(const FTransform&)> InProjector)
        -> TArray<TAttribute<FText>>
    {
        auto Components = TArray<TAttribute<FText>>{};
        Components.Reserve(3);

        for (auto Axis = 0; Axis < 3; ++Axis)
        {
            Components.Emplace(TAttribute<FText>::CreateLambda([InEntity, InSource, InProjector, Axis]()
            {
                const auto Value = InProjector(InSource(InEntity));
                return FText::FromString(ck::Format_UE(TEXT("{:.3f}"), Value[Axis]));
            }));
        }

        return Components;
    }

    auto Get_Location(const FTransform& InTransform) -> FVector
    {
        return InTransform.GetLocation();
    }

    // Euler degrees reordered to the axis each angle turns about (Roll=X, Pitch=Y, Yaw=Z) so the
    // row's X/Y/Z coloring is truthful. The label states the order.
    auto Get_RollPitchYaw(const FTransform& InTransform) -> FVector
    {
        const auto Rotator = InTransform.GetRotation().Rotator();
        return FVector{Rotator.Roll, Rotator.Pitch, Rotator.Yaw};
    }

    auto Get_Scale(const FTransform& InTransform) -> FVector
    {
        return InTransform.GetScale3D();
    }
}

// ====================================================================================================

auto SCkInspector_SceneNodeAuthored::Get_IsAvailable() const -> bool
{ FCk_Handle_SceneNode Node; return _Active && ck_inspector_scenenode::TryGet(_Entity, Node); }
auto SCkInspector_SceneNodeAuthored::Get_Parent() const -> FCk_Handle
{
    FCk_Handle_SceneNode Node;
    return Get_IsAvailable() && ck_inspector_scenenode::TryGet(_Entity, Node)
        ? FCk_Handle{UCk_Utils_SceneNode_UE::Get_Parent(Node)} : FCk_Handle{};
}
auto SCkInspector_SceneNodeAuthored::Get_LayerText() const -> FString
{ const int32 Layer = Get_IsAvailable() ? ck_inspector_scenenode::Get_LayerIndex(_Entity) : INDEX_NONE; return Layer == INDEX_NONE ? TEXT("--") : ck::Format_UE(TEXT("Layer {}"), Layer); }
auto SCkInspector_SceneNodeAuthored::Get_DirtyText() const -> FString
{ return Get_IsAvailable() ? (_Entity.Has<ck::FTag_SceneNode_RelativeTransformUpdated>() ? TEXT("Yes") : TEXT("No")) : TEXT("--"); }
auto SCkInspector_SceneNodeAuthored::Get_RelativeTransform() const -> FTransform
{
    FCk_Handle_SceneNode Node;
    if (NOT Get_IsAvailable() || NOT ck_inspector_scenenode::TryGet(_Entity, Node))
    {
        return FTransform::Identity;
    }

    return ck_inspector_scenenode::Get_LatestRequestedOrCurrentOffset(Node);
}
auto SCkInspector_SceneNodeAuthored::Get_ResolvedTransform() const -> FTransform
{ return Get_IsAvailable() ? UCk_Utils_Transform_TypeUnsafe_UE::Get_EntityCurrentTransform(_Entity) : FTransform::Identity; }
auto SCkInspector_SceneNodeAuthored::Get_CanRequest() const -> bool
{ return _Active && ck_inspector_scenenode::CanRequest(_Entity); }
auto SCkInspector_SceneNodeAuthored::Get_RequestDisabledReason() const -> FString
{ return _Active ? ck_inspector_scenenode::DisabledReason(_Entity) : FString{}; }
auto SCkInspector_SceneNodeAuthored::Commit_Location(const FVector& InLocation) -> void
{
    FCk_Handle_SceneNode Node;
    if (NOT Get_CanRequest() || NOT ck_inspector_scenenode::TryGet(_Entity, Node)) { return; }
    auto Offset = ck_inspector_scenenode::Get_LatestRequestedOrCurrentOffset(Node);
    Offset.SetLocation(InLocation);
    UCk_Utils_SceneNode_UE::Request_UpdateOffset(Node, FCk_Request_SceneNode_UpdateRelativeTransform{Offset}, {});
}
auto SCkInspector_SceneNodeAuthored::Commit_Rotation(const FRotator& InRotation) -> void
{
    FCk_Handle_SceneNode Node;
    if (NOT Get_CanRequest() || NOT ck_inspector_scenenode::TryGet(_Entity, Node)) { return; }
    auto Offset = ck_inspector_scenenode::Get_LatestRequestedOrCurrentOffset(Node);
    Offset.SetRotation(InRotation.Quaternion());
    UCk_Utils_SceneNode_UE::Request_UpdateOffset(Node, FCk_Request_SceneNode_UpdateRelativeTransform{Offset}, {});
}
auto SCkInspector_SceneNodeAuthored::Commit_Scale(const FVector& InScale) -> void
{
    FCk_Handle_SceneNode Node;
    if (NOT Get_CanRequest() || NOT ck_inspector_scenenode::TryGet(_Entity, Node)) { return; }
    auto Offset = ck_inspector_scenenode::Get_LatestRequestedOrCurrentOffset(Node);
    Offset.SetScale3D(InScale);
    UCk_Utils_SceneNode_UE::Request_UpdateOffset(Node, FCk_Request_SceneNode_UpdateRelativeTransform{Offset}, {});
}
auto SCkInspector_SceneNodeAuthored::Request_Detach() -> void
{
    FCk_Handle_SceneNode Node;
    if (Get_CanRequest() && ck_inspector_scenenode::TryGet(_Entity, Node))
    {
        UCk_Utils_SceneNode_UE::Request_Detach(Node, {});
    }
}

auto SCkInspector_SceneNodeAuthored::Build_VectorPort(
    const FName InTag, TFunction<FVector()> InGetter, TFunction<void(const FVector&)> InCommit) -> TSharedRef<SWidget>
{
    const TWeakPtr<SCkInspector_SceneNodeAuthored> Weak{SharedThis(this)};
    const TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        const TSharedPtr<FCkInspectorEditScope> Scope = MakeShared<FCkInspectorEditScope>(_EditGuard);
        _EditScopes.Add(Scope);
        Row->AddSlot().AutoWidth()[SNew(SCkDebug_NumericEditor)
            .Tag(FName{*FString::Printf(TEXT("%s-%d"), *InTag.ToString(), Axis)})
            .Value_Lambda([InGetter, Axis]() { return static_cast<double>(InGetter()[Axis]); })
            .Kind(ECkDebug_NumericKind::Float).Width(72.0f)
            .OnValueCommitted_Lambda([InGetter, InCommit, Axis](const double InValue)
            { auto Value = InGetter(); Value[Axis] = InValue; InCommit(Value); })
            .OnEditStateChanged_Lambda([Weak, Scope](const bool bEditing)
            {
                const TSharedPtr<SCkInspector_SceneNodeAuthored> Widget = Weak.Pin();
                if (Scope.IsValid())
                {
                    Scope->Set_Active(Widget.IsValid() && NOT Widget->Is_Inert() && bEditing);
                }
            })
            .IsEnabled_Lambda([Weak]() { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_CanRequest(); })];
    }
    return Row;
}

auto SCkInspector_SceneNodeAuthored::Build_RotatorPort() -> TSharedRef<SWidget>
{
    const TWeakPtr<SCkInspector_SceneNodeAuthored> Weak{SharedThis(this)};
    return Build_VectorPort(TEXT("scene-node-rotation-input"), [Weak]()
    {
        const TSharedPtr<SCkInspector_SceneNodeAuthored> Widget = Weak.Pin();
        if (NOT Widget.IsValid())
        {
            return FVector::ZeroVector;
        }

        const FRotator Rotation = Widget->Get_RelativeTransform().GetRotation().Rotator();
        return FVector{Rotation.Roll, Rotation.Pitch, Rotation.Yaw};
    }, [Weak](const FVector& InValue)
    {
        if (const TSharedPtr<SCkInspector_SceneNodeAuthored> Widget = Weak.Pin(); Widget.IsValid())
        {
            Widget->Commit_Rotation(FRotator{InValue.Y, InValue.Z, InValue.X});
        }
    });
}

auto SCkInspector_SceneNodeAuthored::Refresh_Siblings() -> bool
{
    if (NOT _Siblings.IsValid()) { return false; }
    auto Records = TArray<FCkUiRecordData>{};
    auto SiblingsByKey = TMap<FString, FCk_Handle>{};
    for (const FCk_Handle& Sibling : ck_inspector_scenenode::Gather_Siblings(_Entity))
    {
        const FString StableKey = ck_inspector_scenenode::GetHandleIdentityKey(Sibling);
        if (StableKey.IsEmpty()) { continue; }
        auto Record = FCkUiRecordData{};
        Record.Key = StableKey;
        Record.Fields.Add(TEXT("sibling"), FCkUiFieldValue{.Kind = ECkUiFieldKind::Text,
            .Text = FText::FromString(ck::Format_UE(TEXT("{}"), Sibling))});
        Records.Add(MoveTemp(Record));
        SiblingsByKey.Add(StableKey, Sibling);
    }
    const FCkUiLoadResult Result = _Siblings->TrySetRecords(MoveTemp(Records));
    if (NOT Result.Succeeded) { _LoadError = FString::Join(Result.Errors, TEXT("\n")); return false; }
    _SiblingsByKey = MoveTemp(SiblingsByKey);
    return true;
}

auto SCkInspector_SceneNodeAuthored::Navigate_Sibling(const FString& InStableKey) -> void
{
    if (NOT Get_IsAvailable()) { return; }
    const FCk_Handle* RoutedSibling = _SiblingsByKey.Find(InStableKey);
    if (RoutedSibling == nullptr || ck::Is_NOT_Valid(*RoutedSibling)) { return; }

    for (const FCk_Handle& CurrentSibling : ck_inspector_scenenode::Gather_Siblings(_Entity))
    {
        if (CurrentSibling != *RoutedSibling
            || ck_inspector_scenenode::GetHandleIdentityKey(CurrentSibling) != InStableKey)
        { continue; }
        if (const TSharedPtr<FCkDebuggerModel_EntitySelection> Selection = _SelectionModel.Pin(); Selection.IsValid())
        { Selection->Set_SelectedEntities({CurrentSibling}); }
        else
        { ck::DebugNav::Goto_Entity(CurrentSibling); }
        return;
    }
}

auto SCkInspector_SceneNodeAuthored::Construct(const FArguments& InArgs) -> void
{
    _Entity = InArgs._Entity;
    _EditGuard = InArgs._EditGuard;
    _SelectionModel = InArgs._SelectionModel;
    _DiffLabels = InArgs._DiffLabels;
    const TSharedRef<SBox> Host = SNew(SBox); ChildSlot[Host];
    if (Build_AuthoredView()) { Host->SetContent(_View->GetRegion(TEXT("main"))); return; }
    Release(); Host->SetContent(SNullWidget::NullWidget);
}
SCkInspector_SceneNodeAuthored::~SCkInspector_SceneNodeAuthored() { Release(); }

auto SCkInspector_SceneNodeAuthored::Build_AuthoredView() -> bool
{
    TSharedPtr<const FCkUiWidgetRegistrySnapshot> Registry;
    const FCkUiLoadResult RegistryResult = FCkDebug_UiRegistry::TryCreate(Registry);
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CkDebugger"));
    if (NOT RegistryResult.Succeeded || NOT Registry.IsValid() || NOT Plugin.IsValid())
    { _LoadError = FString::Join(RegistryResult.Errors, TEXT("\n")); return false; }
    const FCkUiLoadResult SiblingResult = FCkUiCollection::TryCreate({FCkUiFieldSchema{TEXT("sibling"), ECkUiFieldKind::Text}}, _Siblings);
    if (NOT SiblingResult.Succeeded || NOT Refresh_Siblings()) { if (_LoadError.IsEmpty()) { _LoadError = FString::Join(SiblingResult.Errors, TEXT("\n")); } return false; }
    const TWeakPtr<SCkInspector_SceneNodeAuthored> Weak{SharedThis(this)};
    FCkUiView::FNativeBindings Ports;
    Ports.Add(TEXT("scene-node-parent-port"), SNew(SCkDebug_EntityRef).Entity_Lambda([Weak]() { const auto Widget = Weak.Pin(); return Widget.IsValid() ? Widget->Get_Parent() : FCk_Handle{}; }).ShowName(true));
    Ports.Add(TEXT("scene-node-location-port"), Build_VectorPort(TEXT("scene-node-location-input"), [Weak]() { const auto Widget = Weak.Pin(); return Widget.IsValid() ? Widget->Get_RelativeTransform().GetLocation() : FVector::ZeroVector; }, [Weak](const FVector& Value) { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Commit_Location(Value); } }));
    Ports.Add(TEXT("scene-node-rotation-port"), Build_RotatorPort());
    Ports.Add(TEXT("scene-node-scale-port"), Build_VectorPort(TEXT("scene-node-scale-input"), [Weak]() { const auto Widget = Weak.Pin(); return Widget.IsValid() ? Widget->Get_RelativeTransform().GetScale3D() : FVector::OneVector; }, [Weak](const FVector& Value) { if (const auto Widget = Weak.Pin(); Widget.IsValid()) { Widget->Commit_Scale(Value); } }));
    auto Data = FCkUiView::FDataBindings{};
    const auto BindText = [&Data, Weak](const FString& Name, FString (SCkInspector_SceneNodeAuthored::*Getter)() const)
    { Data.Text.Add(Name, TAttribute<FText>::CreateLambda([Weak, Getter]() { const auto Widget = Weak.Pin(); return Widget.IsValid() && NOT Widget->Is_Inert() ? FText::FromString((Widget.Get()->*Getter)()) : FText::GetEmpty(); })); };
    BindText(TEXT("scene-node-layer"), &SCkInspector_SceneNodeAuthored::Get_LayerText);
    BindText(TEXT("scene-node-dirty"), &SCkInspector_SceneNodeAuthored::Get_DirtyText);
    const auto BindTransform = [&Data, Weak](const FString& Name, FTransform (SCkInspector_SceneNodeAuthored::*Getter)() const, int32 Axis, TFunction<FVector(const FTransform&)> Project)
    { Data.Text.Add(Name, TAttribute<FText>::CreateLambda([Weak, Getter, Axis, Project]() { const auto Widget = Weak.Pin(); return Widget.IsValid() && NOT Widget->Is_Inert() ? FText::FromString(ck::Format_UE(TEXT("{:.3f}"), Project((Widget.Get()->*Getter)())[Axis])) : FText::GetEmpty(); })); };
    const TArray<TPair<FString, TFunction<FVector(const FTransform&)>>> Projections = {
        {TEXT("location"), &ck_inspector_scenenode::Get_Location},
        {TEXT("rotation"), &ck_inspector_scenenode::Get_RollPitchYaw},
        {TEXT("scale"), &ck_inspector_scenenode::Get_Scale}};
    for (const auto& Projection : Projections)
    {
        for (int32 Axis = 0; Axis < 3; ++Axis)
        {
            BindTransform(FString::Printf(TEXT("scene-node-relative-%s-%d"), *Projection.Key, Axis),
                &SCkInspector_SceneNodeAuthored::Get_RelativeTransform, Axis, Projection.Value);
            BindTransform(FString::Printf(TEXT("scene-node-resolved-%s-%d"), *Projection.Key, Axis),
                &SCkInspector_SceneNodeAuthored::Get_ResolvedTransform, Axis, Projection.Value);
        }
    }
    Data.Text.Add(TEXT("scene-node-detach-label"), FText::FromString(TEXT("Detach (Keep World)")));
    Data.Text.Add(TEXT("scene-node-detach-tooltip"), FText::FromString(
        TEXT("Request_Detach immediately severs the parent link and preserves the current world transform.")));
    Data.Text.Add(TEXT("scene-node-disabled-reason"), TAttribute<FText>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() ? FText::FromString(Widget->Get_RequestDisabledReason()) : FText::GetEmpty(); }));
    Data.Visibility.Add(TEXT("scene-node-available"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_IsAvailable(); }));
    Data.Visibility.Add(TEXT("scene-node-can-request"), TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_CanRequest(); }));
    const auto BindDiff = [&Data, Weak](const FString& InKey, const FString& InLabel)
    {
        Data.Color.Add(InKey, TAttribute<FLinearColor>::CreateLambda([Weak, InLabel]()
        {
            const auto Widget = Weak.Pin();
            return Widget.IsValid() && NOT Widget->Is_Inert()
                ? ck_inspector_scenenode::Diff(Widget->Is_DiffMarked(InLabel))
                : FLinearColor::Transparent;
        }));
    };
    for (const auto& Binding : TArray<TPair<FString, FString>>{
        {TEXT("parent"), TEXT("Parent:")}, {TEXT("layer"), TEXT("Layer:")},
        {TEXT("dirty"), TEXT("Dirty This Frame:")}, {TEXT("location"), TEXT("Location:")},
        {TEXT("rotation"), TEXT("Rotation (R,P,Y):")}, {TEXT("scale"), TEXT("Scale:")},
        {TEXT("set-location"), TEXT("Set Location:")},
        {TEXT("set-rotation"), TEXT("Set Rotation (R,P,Y):")},
        {TEXT("set-scale"), TEXT("Set Scale:")}, {TEXT("attachment"), TEXT("Attachment:")},
        {TEXT("nodes"), TEXT("Nodes:")}})
    { BindDiff(TEXT("scene-node-") + Binding.Key + TEXT("-diff-color"), Binding.Value); }
    Data.Collections.Add(TEXT("scene-node-siblings"), _Siblings);
    Data.CanDispatchEvents = TAttribute<bool>::CreateLambda([Weak]()
    { const auto Widget = Weak.Pin(); return Widget.IsValid() && Widget->Get_IsAvailable(); });
    Data.ItemActions.Add(TEXT("scene-node-navigate-sibling"), FCkUiOnItemAction::CreateLambda(
        [Weak](const FString& InStableKey)
        { if (const auto Widget = Weak.Pin(); Widget.IsValid() && NOT Widget->Is_Inert()) { Widget->Navigate_Sibling(InStableKey); } }));
    auto Actions = FCkUiView::FActions{};
    Actions.Add(TEXT("scene-node-detach"), FSimpleDelegate::CreateLambda([Weak]()
    { if (const auto Widget = Weak.Pin(); Widget.IsValid() && NOT Widget->Is_Inert()) { Widget->Request_Detach(); } }));
    const TSharedRef<FCkUiView> Candidate = FCkUiView::Create(MoveTemp(Ports), MoveTemp(Actions), {}, FCoreStyle::Get().GetFontStyle("NormalFont"), MoveTemp(Data), Registry);
    Candidate->GetRegion(TEXT("main"));
    const FString Root = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/UI"));
    Candidate->SetFiles(FPaths::Combine(Root, TEXT("EcsInspectorSceneNode.ui.html")), FPaths::Combine(Root, TEXT("EcsInspectorSceneNode.ui.css")));
    Candidate->PollFiles();
    if (NOT Candidate->GetLastResult().Succeeded) { _LoadError = FString::Join(Candidate->GetLastResult().Errors, TEXT("\n")); return false; }
    _View = Candidate; _Mounted = true; _LoadError.Reset(); return true;
}
auto SCkInspector_SceneNodeAuthored::Tick(const FGeometry& G, double T, float D) -> void
{
    SCompoundWidget::Tick(G, T, D);
    if (NOT _Active || NOT _View.IsValid()) { return; }
    Refresh_Siblings();
    _View->PollFiles();
    if (NOT _View->GetLastResult().Succeeded)
    {
        _LoadError = FString::Join(_View->GetLastResult().Errors, TEXT("\n"));
    }
}
auto SCkInspector_SceneNodeAuthored::Release() -> void
{
    if (NOT _Active) { return; }
    _Active = false;
    for (const auto& Scope : _EditScopes)
    {
        if (Scope.IsValid()) { Scope->Set_Active(false); }
    }
    _EditScopes.Reset();
    _Entity = {};
    _EditGuard.Reset();
    _SelectionModel.Reset();
    _SiblingsByKey.Reset();
    _Siblings.Reset();
    _View.Reset();
    _Mounted = false;
}

auto FCkInspector_SceneNode::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("SceneNode"));
}

auto FCkInspector_SceneNode::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity) && UCk_Utils_SceneNode_UE::Has(Entity);
}

auto FCkInspector_SceneNode::Build_NativeBody(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    namespace inspector = ck_inspector_scenenode;

    auto Builder = FCkInspectorWidgetBuilder();
    Builder.SetEditGuard(Get_EditGuard());

    const auto CapturedEntity = Entity;

    auto MutableNodeEntity = Entity;
    const auto CapturedNode = UCk_Utils_SceneNode_UE::Cast(MutableNodeEntity);

    // ----- Parent entity (clickable) -----
    const auto ParentHandle = [&]() -> FCk_Handle
    {
        if (NOT UCk_Utils_SceneNode_UE::Has(Entity)) { return FCk_Handle{}; }
        auto Mut = Entity;
        const auto Node = UCk_Utils_SceneNode_UE::Cast(Mut);
        if (ck::Is_NOT_Valid(Node)) { return FCk_Handle{}; }
        const auto Parent = UCk_Utils_SceneNode_UE::Get_Parent(Node);
        return ck::IsValid(Parent) ? FCk_Handle{Parent} : FCk_Handle{};
    }();

    Builder.AddWidgetRow(
        FText::FromString(TEXT("Parent:")),
        SNew(SCkDebug_EntityRef)
            .Entity(ParentHandle)
            .ShowName(true));

    // ----- Layer -----
    Builder.AddStatusPillRow(
        FText::FromString(TEXT("Layer:")),
        TAttribute<FText>::CreateLambda([CapturedEntity]()
        {
            if (ck::Is_NOT_Valid(CapturedEntity)) { return FText::FromString(TEXT("—")); }
            const auto Idx = inspector::Get_LayerIndex(CapturedEntity);
            if (Idx == INDEX_NONE) { return FText::FromString(TEXT("—")); }
            return FText::FromString(ck::Format_UE(TEXT("Layer {}"), Idx));
        }),
        TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]()
        {
            if (ck::Is_NOT_Valid(CapturedEntity)) { return ECk_Tone::Neutral; }
            return inspector::Get_LayerIndex(CapturedEntity) == INDEX_NONE ? ECk_Tone::Neutral : ECk_Tone::Info;
        }));

    // ----- Dirty this frame -----
    Builder.AddStatusPillRow(
        FText::FromString(TEXT("Dirty This Frame:")),
        TAttribute<FText>::CreateLambda([CapturedEntity]()
        {
            if (ck::Is_NOT_Valid(CapturedEntity)) { return FText::FromString(TEXT("--")); }
            return FText::FromString(CapturedEntity.Has<ck::FTag_SceneNode_RelativeTransformUpdated>()
                ? TEXT("Yes") : TEXT("No"));
        }),
        TAttribute<ECk_Tone>::CreateLambda([CapturedEntity]()
        {
            if (ck::Is_NOT_Valid(CapturedEntity)) { return ECk_Tone::Neutral; }
            return CapturedEntity.Has<ck::FTag_SceneNode_RelativeTransformUpdated>()
                ? ECk_Tone::Accent : ECk_Tone::Neutral;
        }));

    // ----- Relative transform -----
    Builder.AddHeader(FText::FromString(TEXT("Relative Transform")));

    Builder.AddAlignedNumericRow(
        FText::FromString(TEXT("Location:")),
        inspector::Make_AxisComponents(CapturedEntity, &inspector::Get_RelativeTransform, &inspector::Get_Location));

    Builder.AddAlignedNumericRow(
        FText::FromString(TEXT("Rotation (R,P,Y):")),
        inspector::Make_AxisComponents(CapturedEntity, &inspector::Get_RelativeTransform, &inspector::Get_RollPitchYaw));

    Builder.AddAlignedNumericRow(
        FText::FromString(TEXT("Scale:")),
        inspector::Make_AxisComponents(CapturedEntity, &inspector::Get_RelativeTransform, &inspector::Get_Scale));

    // ----- Editable relative offset -----
    //
    // Request_UpdateOffset carries the WHOLE FTransform, so each of these three rows is a
    // read-modify-write over the live Get_Offset: commit one component, re-read the other two, send
    // one composed transform. The read-only rows above stay as the unambiguous display.
    // LocalOk: the SceneNode request processor has no authority gate.
    if (ck::IsValid(CapturedNode))
    {
        const auto Request_UpdateOffset = [CapturedNode](const FTransform& InNewOffset)
        {
            auto MutableNode = CapturedNode;
            if (ck::Is_NOT_Valid(MutableNode)) { return; }

            UCk_Utils_SceneNode_UE::Request_UpdateOffset(MutableNode,
                FCk_Request_SceneNode_UpdateRelativeTransform{InNewOffset}, {});
        };

        Builder.AddVectorRow(
            FText::FromString(TEXT("Set Location:")),
            TAttribute<FVector>::CreateLambda([CapturedNode]()
            {
                if (ck::Is_NOT_Valid(CapturedNode)) { return FVector::ZeroVector; }
                return UCk_Utils_SceneNode_UE::Get_Offset(CapturedNode).GetLocation();
            }),
            [CapturedNode, Request_UpdateOffset](const FVector& InLocation)
            {
                if (ck::Is_NOT_Valid(CapturedNode)) { return; }

                auto Offset = UCk_Utils_SceneNode_UE::Get_Offset(CapturedNode);
                Offset.SetLocation(InLocation);
                Request_UpdateOffset(Offset);
            },
            ECk_DebugRequest_Requirement::LocalOk);

        Builder.AddRotatorRow(
            FText::FromString(TEXT("Set Rotation (R,P,Y):")),
            TAttribute<FRotator>::CreateLambda([CapturedNode]()
            {
                if (ck::Is_NOT_Valid(CapturedNode)) { return FRotator::ZeroRotator; }
                return UCk_Utils_SceneNode_UE::Get_Offset(CapturedNode).GetRotation().Rotator();
            }),
            [CapturedNode, Request_UpdateOffset](const FRotator& InRotator)
            {
                if (ck::Is_NOT_Valid(CapturedNode)) { return; }

                auto Offset = UCk_Utils_SceneNode_UE::Get_Offset(CapturedNode);
                Offset.SetRotation(InRotator.Quaternion());
                Request_UpdateOffset(Offset);
            },
            ECk_DebugRequest_Requirement::LocalOk);

        Builder.AddVectorRow(
            FText::FromString(TEXT("Set Scale:")),
            TAttribute<FVector>::CreateLambda([CapturedNode]()
            {
                if (ck::Is_NOT_Valid(CapturedNode)) { return FVector::OneVector; }
                return UCk_Utils_SceneNode_UE::Get_Offset(CapturedNode).GetScale3D();
            }),
            [CapturedNode, Request_UpdateOffset](const FVector& InScale)
            {
                if (ck::Is_NOT_Valid(CapturedNode)) { return; }

                auto Offset = UCk_Utils_SceneNode_UE::Get_Offset(CapturedNode);
                Offset.SetScale3D(InScale);
                Request_UpdateOffset(Offset);
            },
            ECk_DebugRequest_Requirement::LocalOk);

        // Detach is an IMMEDIATE mutator (links severed inline, delegate fires on this stack), so
        // there is no request-drain lag. The "Parent:" row above still shows the OLD parent until the
        // next inspector rebuild — it binds a compose-time handle, not a live getter — which is the
        // same staleness that row has always had.
        Builder.AddActionRow(
            FText::FromString(TEXT("Attachment:")),
            {
                FCkInspector_Action
                {
                    FText::FromString(TEXT("Detach (Keep World)")),
                    FText::FromString(TEXT("UCk_Utils_SceneNode_UE::Request_Detach")),
                    [CapturedNode]()
                    {
                        auto MutableNode = CapturedNode;
                        if (ck::Is_NOT_Valid(MutableNode)) { return; }
                        UCk_Utils_SceneNode_UE::Request_Detach(MutableNode, {});
                    },
                    ECk_DebugRequest_Requirement::LocalOk
                },
            });
    }

    // ----- Resolved world transform -----
    Builder.AddHeader(FText::FromString(TEXT("Resolved World Transform")));

    Builder.AddAlignedNumericRow(
        FText::FromString(TEXT("Location:")),
        inspector::Make_AxisComponents(CapturedEntity, &inspector::Get_WorldTransform, &inspector::Get_Location));

    Builder.AddAlignedNumericRow(
        FText::FromString(TEXT("Rotation (R,P,Y):")),
        inspector::Make_AxisComponents(CapturedEntity, &inspector::Get_WorldTransform, &inspector::Get_RollPitchYaw));

    Builder.AddAlignedNumericRow(
        FText::FromString(TEXT("Scale:")),
        inspector::Make_AxisComponents(CapturedEntity, &inspector::Get_WorldTransform, &inspector::Get_Scale));

    // ----- Siblings -----
    Builder.AddHeader(FText::FromString(TEXT("Siblings (under Parent)")));

    const auto Siblings = inspector::Gather_Siblings(Entity);
    _LastSiblingCount = Siblings.Num();
    _SiblingsBox = FCkInspectorWidgetBuilder::MakeBadgeBox(Siblings);
    Builder.AddWidgetRow(FText::FromString(TEXT("Nodes:")), _SiblingsBox.ToSharedRef());

    return Builder.Build(Entity);
}

auto FCkInspector_SceneNode::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
{
    const TSharedRef<SWidget> NativeBody = Build_NativeBody(Entity);
    if (FCkInspector_RowCaptureScope::Is_Active()) { return NativeBody; }
    auto DiffLabels = TSet<FString>{};
    for (const TCHAR* Label : {TEXT("Parent:"), TEXT("Layer:"), TEXT("Dirty This Frame:"), TEXT("Location:"), TEXT("Rotation (R,P,Y):"), TEXT("Scale:"), TEXT("Set Location:"), TEXT("Set Rotation (R,P,Y):"), TEXT("Set Scale:"), TEXT("Attachment:"), TEXT("Nodes:")})
    { if (FCkInspector_DiffMarkScope::Is_LabelMarked(Label)) { DiffLabels.Add(Label); } }
    const TSharedRef<SCkInspector_SceneNodeAuthored> Authored = SNew(SCkInspector_SceneNodeAuthored)
        .Entity(Entity)
        .EditGuard(Get_EditGuard())
        .SelectionModel(Get_SelectionModel())
        .DiffLabels(MoveTemp(DiffLabels));
    if (NOT Authored->Is_Mounted()) { _LastAuthoredLoadError = Authored->Get_LoadError(); return NativeBody; }
    _LastAuthoredLoadError.Reset(); _AuthoredInstances.Add(Authored); return Authored;
}

auto FCkInspector_SceneNode::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    static_cast<void>(InDeltaTime);
    if (ck::Is_NOT_Valid(Entity) || NOT UCk_Utils_SceneNode_UE::Has(Entity))
    { return; }

    // Keep sibling badges in sync when SceneNodes come and go.
    if (_SiblingsBox.IsValid())
    {
        const auto Siblings = ck_inspector_scenenode::Gather_Siblings(Entity);
        if (Siblings.Num() != _LastSiblingCount)
        {
            _LastSiblingCount = Siblings.Num();
            FCkInspectorWidgetBuilder::PopulateBadgeBox(*_SiblingsBox, Siblings);
        }
    }
    _AuthoredInstances.RemoveAll([](const TWeakPtr<SCkInspector_SceneNodeAuthored>& Instance)
    { return NOT Instance.IsValid() || Instance.Pin()->Is_Inert(); });
}

FCkInspector_SceneNode::~FCkInspector_SceneNode() { OnDeactivated(); }
auto FCkInspector_SceneNode::OnDeactivated() -> void
{ for (const auto& Weak : _AuthoredInstances) { if (const auto Instance = Weak.Pin(); Instance.IsValid()) { Instance->Release(); } } _AuthoredInstances.Reset(); _SiblingsBox.Reset(); _LastSiblingCount = -1; }
