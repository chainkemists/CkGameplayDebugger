#include "CkInspector_SceneNode.h"

#include "CkCore/Validation/CkIsValid.h"
#include "CkCore/Debug/CkDebugDraw_Utils.h"
#include "CkEcs/Handle/CkHandle_Utils.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcsExt/SceneNode/CkSceneNode_Fragment.h"
#include "CkEcsExt/SceneNode/CkSceneNode_Utils.h"
#include "CkEcsExt/Transform/CkTransform_Utils.h"

#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEcsDebugger/Models/CkDebuggerModel_EntitySelection.h"
#include "CkDebuggerCommon/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkDebuggerCommon/Navigation/CkDebug_ViewportView.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_SceneNode)

// ====================================================================================================

namespace ck_inspector_scenenode
{
    auto Get_LayerIndex(const FCk_Handle& E) -> int32
    {
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

auto FCkInspector_SceneNode::Get_ComponentName() const -> FText
{
    return FText::FromString(TEXT("SceneNode"));
}

auto FCkInspector_SceneNode::CanInspect(const FCk_Handle& Entity) const -> bool
{
    return ck::IsValid(Entity) && UCk_Utils_SceneNode_UE::Has(Entity);
}

auto FCkInspector_SceneNode::Build_Inspector(const FCk_Handle& Entity) -> TSharedRef<SWidget>
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

auto FCkInspector_SceneNode::Tick(const FCk_Handle& Entity, float InDeltaTime) -> void
{
    if (ck::Is_NOT_Valid(Entity) || NOT UCk_Utils_SceneNode_UE::Has(Entity))
    { return; }

    const auto EntityWorld = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(Entity);
    if (ck::Is_NOT_Valid(EntityWorld))
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

    // Debug draw: world gizmo for this SceneNode, plus one for its parent — persistent
    // PMG triads instead of per-tick DrawDebugTransformGizmo (gate-capped one-frame
    // lines blink; see FCkDebug_PmgGizmoSet).
    if (NOT UCk_Utils_Transform_UE::Has(Entity))
    {
        _Gizmos.Remove(Entity);
        return;
    }

    // Possessed first person + inspecting your own pawn's nodes: the triads + label
    // sit inside the camera. Suppressed until ejected (mirrors the overlay's
    // self-marker suppression).
    if (ck::DebugViewportView::Get_IsLocalPlayerSelf(Entity))
    {
        _Gizmos.Reset();
        _LastParentGizmoKey = FCk_Handle{};
        return;
    }

    const auto NodeWorld = UCk_Utils_Transform_TypeUnsafe_UE::Get_EntityCurrentTransform(Entity);
    _Gizmos.UpdateGizmo(EntityWorld, Entity, NodeWorld);

    auto ParentGizmoKey = FCk_Handle{};
    auto Mut = Entity;
    const auto Node = UCk_Utils_SceneNode_UE::Cast(Mut);
    if (ck::IsValid(Node))
    {
        const auto Parent = UCk_Utils_SceneNode_UE::Get_Parent(Node);
        if (ck::IsValid(Parent) && UCk_Utils_Transform_UE::Has(FCk_Handle(Parent)))
        {
            const auto ParentWorld = UCk_Utils_Transform_TypeUnsafe_UE::Get_EntityCurrentTransform(FCk_Handle(Parent));
            ParentGizmoKey = FCk_Handle(Parent);
            _Gizmos.UpdateGizmo(EntityWorld, ParentGizmoKey, ParentWorld);
        }
    }

    if (_LastParentGizmoKey != ParentGizmoKey && ck::IsValid(_LastParentGizmoKey))
    {
        _Gizmos.Remove(_LastParentGizmoKey);
    }
    _LastParentGizmoKey = ParentGizmoKey;

    const auto TextLocation = NodeWorld.GetLocation() + FVector(0.0f, 0.0f, 50.0f);
    UCk_Utils_DebugDraw_UE::DrawDebugString(
        EntityWorld,
        TextLocation,
        Entity.ToString(),
        FLinearColor::White,
        0.0f);
}

auto FCkInspector_SceneNode::OnDeactivated() -> void
{
    _Gizmos.Reset();
    _LastParentGizmoKey = FCk_Handle{};
}
