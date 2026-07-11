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
#include "CkEcsDebugger/Styles/CkDebuggerStyle.h"

#include "CkEditorTools/Style/CkStyle.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_SceneNode)

// ====================================================================================================

namespace
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
    auto Builder = FCkInspectorWidgetBuilder();

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
    Builder.AddConditionalRow(
        FText::FromString(TEXT("Layer:")),
        [](const FCk_Handle& E)
        {
            const auto Idx = Get_LayerIndex(E);
            if (Idx == INDEX_NONE) { return FText::FromString(TEXT("—")); }
            return FText::FromString(ck::Format_UE(TEXT("Layer {}"), Idx));
        },
        [](const FCk_Handle& E)
        {
            return Get_LayerIndex(E) == INDEX_NONE ? CkStyle::None() : CkStyle::Text();
        });

    // ----- Dirty this frame -----
    Builder.AddConditionalRow(
        FText::FromString(TEXT("Dirty This Frame:")),
        [](const FCk_Handle& E)
        {
            return FText::FromString(E.Has<ck::FTag_SceneNode_RelativeTransformUpdated>()
                ? TEXT("Yes") : TEXT("No"));
        },
        [](const FCk_Handle& E)
        {
            return E.Has<ck::FTag_SceneNode_RelativeTransformUpdated>()
                ? CkStyle::State_Enabled() : CkStyle::None();
        });

    // ----- Relative transform -----
    Builder.AddHeader(FText::FromString(TEXT("Relative Transform")));

    Builder.AddRow(
        FText::FromString(TEXT("Location:")),
        [](const FCk_Handle& E)
        {
            if (NOT UCk_Utils_SceneNode_UE::Has(E)) { return FText::GetEmpty(); }
            auto Mut = E;
            const auto Node = UCk_Utils_SceneNode_UE::Cast(Mut);
            if (ck::Is_NOT_Valid(Node)) { return FText::GetEmpty(); }
            return FText::FromString(ck::Format_UE(TEXT("{}"),
                UCk_Utils_SceneNode_UE::Get_Offset(Node).GetLocation()));
        },
        CkStyle::Transform());

    Builder.AddRow(
        FText::FromString(TEXT("Rotation:")),
        [](const FCk_Handle& E)
        {
            if (NOT UCk_Utils_SceneNode_UE::Has(E)) { return FText::GetEmpty(); }
            auto Mut = E;
            const auto Node = UCk_Utils_SceneNode_UE::Cast(Mut);
            if (ck::Is_NOT_Valid(Node)) { return FText::GetEmpty(); }
            return FText::FromString(ck::Format_UE(TEXT("{}"),
                UCk_Utils_SceneNode_UE::Get_Offset(Node).GetRotation().Rotator()));
        },
        CkStyle::Transform());

    Builder.AddRow(
        FText::FromString(TEXT("Scale:")),
        [](const FCk_Handle& E)
        {
            if (NOT UCk_Utils_SceneNode_UE::Has(E)) { return FText::GetEmpty(); }
            auto Mut = E;
            const auto Node = UCk_Utils_SceneNode_UE::Cast(Mut);
            if (ck::Is_NOT_Valid(Node)) { return FText::GetEmpty(); }
            return FText::FromString(ck::Format_UE(TEXT("{}"),
                UCk_Utils_SceneNode_UE::Get_Offset(Node).GetScale3D()));
        },
        CkStyle::Transform());

    // ----- Resolved world transform -----
    Builder.AddHeader(FText::FromString(TEXT("Resolved World Transform")));

    Builder.AddRow(
        FText::FromString(TEXT("Location:")),
        [](const FCk_Handle& E)
        {
            if (NOT UCk_Utils_Transform_UE::Has(E)) { return FText::GetEmpty(); }
            return FText::FromString(ck::Format_UE(TEXT("{}"),
                UCk_Utils_Transform_TypeUnsafe_UE::Get_EntityCurrentTransform(E).GetLocation()));
        },
        CkStyle::Transform());

    Builder.AddRow(
        FText::FromString(TEXT("Rotation:")),
        [](const FCk_Handle& E)
        {
            if (NOT UCk_Utils_Transform_UE::Has(E)) { return FText::GetEmpty(); }
            return FText::FromString(ck::Format_UE(TEXT("{}"),
                UCk_Utils_Transform_TypeUnsafe_UE::Get_EntityCurrentTransform(E).GetRotation().Rotator()));
        },
        CkStyle::Transform());

    Builder.AddRow(
        FText::FromString(TEXT("Scale:")),
        [](const FCk_Handle& E)
        {
            if (NOT UCk_Utils_Transform_UE::Has(E)) { return FText::GetEmpty(); }
            return FText::FromString(ck::Format_UE(TEXT("{}"),
                UCk_Utils_Transform_TypeUnsafe_UE::Get_EntityCurrentTransform(E).GetScale3D()));
        },
        CkStyle::Transform());

    // ----- Siblings -----
    Builder.AddHeader(FText::FromString(TEXT("Siblings (under Parent)")));

    const auto Siblings = Gather_Siblings(Entity);
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
        const auto Siblings = Gather_Siblings(Entity);
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
