#include "CkInspector_Chain.h"

#include "CkChain/CkChain_Utils.h"
#include "CkChain/CkChainLink_Utils.h"
#include "CkCore/Debug/CkDebugDraw_Utils.h"
#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcsExt/Transform/CkTransform_Utils.h"

#include "CkDebuggerCommon/Widgets/SCkDebug_EntityRef.h"
#include "CkDebuggerCommon/Widgets/SCkDebug_SelectableLabel.h"
#include "CkEcsDebugger/Inspectors/CkDebuggerInspectorRegistry.h"
#include "CkEcsDebugger/Inspectors/CkInspectorWidgetBuilder.h"
#include "CkEditorTools/Style/CkStyle.h"

#include "Engine/World.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SGridPanel.h"

CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_Chain)
CK_REGISTER_DEBUGGER_INSPECTOR(FCkInspector_ChainLink)

// --------------------------------------------------------------------------------------------------------------------

namespace ck_inspector_chain
{
    auto
        Get_Chain(const FCk_Handle& InEntity)
        -> FCk_Handle_Chain
    {
        if (ck::Is_NOT_Valid(InEntity) || NOT UCk_Utils_Chain_UE::Has(InEntity))
        {
            return {};
        }
        auto MutableEntity = InEntity;
        return UCk_Utils_Chain_UE::Cast(MutableEntity);
    }

    auto
        Get_Link(const FCk_Handle& InEntity)
        -> FCk_Handle_ChainLink
    {
        if (ck::Is_NOT_Valid(InEntity) || NOT UCk_Utils_ChainLink_UE::Has(InEntity))
        {
            return {};
        }
        auto MutableEntity = InEntity;
        return UCk_Utils_ChainLink_UE::Cast(MutableEntity);
    }

    auto
        Get_RosterLink(const FCk_Handle& InEntity, uint32 InId)
        -> FCk_Handle_ChainLink
    {
        const auto Chain = Get_Chain(InEntity);
        if (ck::Is_NOT_Valid(Chain))
        {
            return {};
        }
        for (const auto& Link : UCk_Utils_Chain_UE::Get_Links(Chain))
        {
            if (ck::IsValid(Link) && static_cast<uint32>(Link.Get_Entity().Get_ID()) == InId)
            {
                return Link;
            }
        }
        return {};
    }

    auto
        Get_Ids(const FCk_Handle_Chain& InChain)
        -> TArray<uint32>
    {
        auto Ids = TArray<uint32>{};
        for (const auto& Link : UCk_Utils_Chain_UE::Get_Links(InChain))
        {
            Ids.Add(static_cast<uint32>(Link.Get_Entity().Get_ID()));
        }
        return Ids;
    }

    auto
        Get_TargetError(const FCk_Handle_ChainLink& InLink)
        -> double
    {
        auto MutableLink = FCk_Handle{InLink};
        const auto Transform = UCk_Utils_Transform_UE::Cast(MutableLink);
        return FVector::Distance(UCk_Utils_Transform_UE::Get_EntityCurrentLocation(Transform),
            UCk_Utils_ChainLink_UE::Get_TargetPose(InLink).GetLocation());
    }

    auto
        Make_Cell(TAttribute<FText> InText)
        -> TSharedRef<SWidget>
    {
        return SNew(SCkDebug_SelectableLabel).Text(InText).ColorAndOpacity(CkStyle::Text()).Font(CkStyle::RegularFont(CkStyle::FontSizeBody()));
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkInspector_Chain::Get_ComponentName() const
    -> FText
{
    return FText::FromString(TEXT("Chain"));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkInspector_Chain::CanInspect(const FCk_Handle& InEntity) const
    -> bool
{
    return ck::IsValid(InEntity) && UCk_Utils_Chain_UE::Has(InEntity);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkInspector_Chain::Build_NativeBody(const FCk_Handle& InEntity)
    -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder{};
    const auto Chain = ck_inspector_chain::Get_Chain(InEntity);
    if (ck::Is_NOT_Valid(Chain))
    {
        return Builder.Build(InEntity, FString{});
    }
    Builder.AddWidgetRow(FText::FromString(TEXT("Head:")), SNew(SCkDebug_EntityRef).Entity_Lambda([InEntity]() -> FCk_Handle
    {
        const auto Current = ck_inspector_chain::Get_Chain(InEntity);
        return ck::IsValid(Current) ? FCk_Handle{UCk_Utils_Chain_UE::Get_Head(Current)} : FCk_Handle{};
    }));
    Builder.AddRow(FText::FromString(TEXT("Solver:")), [](const FCk_Handle& InCurrent)
    {
        const auto Current = ck_inspector_chain::Get_Chain(InCurrent);
        return FText::FromString(ck::IsValid(Current) ? ck::Format_UE(TEXT("{}"), UCk_Utils_Chain_UE::Get_Solver(Current)) : TEXT("--"));
    }, CkStyle::Value_Enum());
    Builder.AddStatusPillRow(FText::FromString(TEXT("Enabled:")), TAttribute<FText>::CreateLambda([InEntity]()
    {
        const auto Current = ck_inspector_chain::Get_Chain(InEntity);
        return FText::FromString(ck::IsValid(Current) && UCk_Utils_Chain_UE::Get_IsEnabled(Current) ? TEXT("Enabled") : TEXT("Disabled"));
    }), TAttribute<ECk_Tone>::CreateLambda([InEntity]()
    {
        const auto Current = ck_inspector_chain::Get_Chain(InEntity);
        return ck::IsValid(Current) && UCk_Utils_Chain_UE::Get_IsEnabled(Current) ? ECk_Tone::Accent : ECk_Tone::Neutral;
    }));
    Builder.AddRow(FText::FromString(TEXT("Net policy:")), [](const FCk_Handle& InCurrent)
    {
        const auto Current = ck_inspector_chain::Get_Chain(InCurrent);
        return FText::FromString(ck::IsValid(Current) ? ck::Format_UE(TEXT("{}"), UCk_Utils_Chain_UE::Get_NetPolicy(Current)) : TEXT("--"));
    }, CkStyle::Value_Enum());
    Builder.AddRow(FText::FromString(TEXT("Links / length:")), [](const FCk_Handle& InCurrent)
    {
        const auto Current = ck_inspector_chain::Get_Chain(InCurrent);
        return FText::FromString(ck::IsValid(Current) ? ck::Format_UE(TEXT("{} / {} cm"),
            UCk_Utils_Chain_UE::Get_NumLinks(Current), UCk_Utils_Chain_UE::Get_LengthCm(Current)) : TEXT("--"));
    }, CkStyle::Value_Numeric());
    Builder.AddRow(FText::FromString(TEXT("History:")), [](const FCk_Handle& InCurrent)
    {
        const auto Current = ck_inspector_chain::Get_Chain(InCurrent);
        return FText::FromString(ck::IsValid(Current) ? ck::Format_UE(TEXT("{} samples / {} cm"),
            UCk_Utils_Chain_UE::Get_NumHistorySamples(Current), UCk_Utils_Chain_UE::Get_HistoryLengthCm(Current)) : TEXT("--"));
    }, CkStyle::Value_Numeric());
    const auto Draw = _Draw;
    Builder.AddWidgetRow(FText::FromString(TEXT("Draw:")), SNew(SCheckBox)
        .IsChecked_Lambda([Draw]() { return *Draw ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
        .OnCheckStateChanged_Lambda([Draw](ECheckBoxState InState) { *Draw = InState == ECheckBoxState::Checked; }));

    auto Table = SNew(SGridPanel);
    const auto Headers = TArray<FString>{TEXT("Index"), TEXT("Entity"), TEXT("Distance cm"), TEXT("Error cm"), TEXT("Held")};
    for (auto Column = 0; Column < Headers.Num(); ++Column)
    {
        Table->AddSlot(Column, 0)[ck_inspector_chain::Make_Cell(FText::FromString(Headers[Column]))];
    }
    const auto Ids = ck_inspector_chain::Get_Ids(Chain);
    for (auto Index = 0; Index < Ids.Num(); ++Index)
    {
        const auto Id = Ids[Index];
        Table->AddSlot(0, Index + 1)[ck_inspector_chain::Make_Cell(FText::AsNumber(Index))];
        Table->AddSlot(1, Index + 1)[SNew(SCkDebug_EntityRef).Entity_Lambda([InEntity, Id]() -> FCk_Handle
        {
            return ck_inspector_chain::Get_RosterLink(InEntity, Id);
        })];
        Table->AddSlot(2, Index + 1)[ck_inspector_chain::Make_Cell(TAttribute<FText>::CreateLambda([InEntity, Id]()
        {
            const auto Link = ck_inspector_chain::Get_RosterLink(InEntity, Id);
            return ck::IsValid(Link) ? FText::AsNumber(UCk_Utils_ChainLink_UE::Get_DistanceFromHeadCm(Link)) : FText::FromString(TEXT("--"));
        }))];
        Table->AddSlot(3, Index + 1)[ck_inspector_chain::Make_Cell(TAttribute<FText>::CreateLambda([InEntity, Id]()
        {
            const auto Link = ck_inspector_chain::Get_RosterLink(InEntity, Id);
            return ck::IsValid(Link) ? FText::AsNumber(ck_inspector_chain::Get_TargetError(Link)) : FText::FromString(TEXT("--"));
        }))];
        Table->AddSlot(4, Index + 1)[ck_inspector_chain::Make_Cell(TAttribute<FText>::CreateLambda([InEntity, Id]()
        {
            const auto Link = ck_inspector_chain::Get_RosterLink(InEntity, Id);
            return FText::FromString(ck::IsValid(Link) ? (UCk_Utils_ChainLink_UE::Get_IsHeld(Link) ? TEXT("Yes") : TEXT("No")) : TEXT("--"));
        }))];
    }
    Builder.AddWidgetRow(FText::FromString(TEXT("Roster:")), Table);
    if (NOT FCkInspector_RowCaptureScope::Is_Active())
    {
        _LinkIds = Ids;
        _EntityId = static_cast<uint32>(InEntity.Get_Entity().Get_ID());
    }
    return Builder.Build(InEntity, FString{});
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkInspector_Chain::Tick(const FCk_Handle& InEntity, float InDeltaTime)
    -> void
{
    const auto Chain = ck_inspector_chain::Get_Chain(InEntity);
    if (ck::Is_NOT_Valid(Chain))
    {
        return;
    }
    const auto EntityId = static_cast<uint32>(InEntity.Get_Entity().Get_ID());
    const auto Ids = ck_inspector_chain::Get_Ids(Chain);
    if (NOT _EntityId.IsSet() || _EntityId.GetValue() != EntityId || Ids != _LinkIds)
    {
        _EntityId = EntityId;
        _LinkIds = Ids;
        RequestRebuild();
    }
    if (NOT *_Draw)
    {
        return;
    }
    auto* World = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(InEntity);
    if (ck::Is_NOT_Valid(World))
    {
        return;
    }
    const auto Samples = UCk_Utils_Chain_UE::Get_HistorySamples(Chain);
    for (auto Index = 1; Index < Samples.Num(); ++Index)
    {
        UCk_Utils_DebugDraw_UE::DrawDebugLine(World, Samples[Index - 1].Get_Location(), Samples[Index].Get_Location(), FLinearColor{0.05f, 0.75f, 0.7f}, 0.0f, 2.0f);
    }
    for (const auto& Link : UCk_Utils_Chain_UE::Get_Links(Chain))
    {
        if (ck::Is_NOT_Valid(Link))
        {
            continue;
        }
        auto MutableLink = FCk_Handle{Link};
        const auto Transform = UCk_Utils_Transform_UE::Cast(MutableLink);
        const auto Target = UCk_Utils_ChainLink_UE::Get_TargetPose(Link);
        const auto Color = UCk_Utils_ChainLink_UE::Get_IsHeld(Link) ? FLinearColor::Gray : FLinearColor::Green;
        UCk_Utils_DebugDraw_UE::DrawDebugArrow(World, Target.GetLocation(),
            Target.GetLocation() + Target.GetRotation().GetForwardVector() * 35.0, 8.0f, Color, 0.0f, 2.0f);
        UCk_Utils_DebugDraw_UE::DrawDebugLine(World, UCk_Utils_Transform_UE::Get_EntityCurrentLocation(Transform), Target.GetLocation(), Color, 0.0f, 1.0f);
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkInspector_Chain::OnDeactivated()
    -> void
{
    *_Draw = false;
    _Draw = MakeShared<bool>(false);
    _LinkIds.Reset();
    _EntityId.Reset();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkInspector_ChainLink::Get_ComponentName() const
    -> FText
{
    return FText::FromString(TEXT("Chain Link"));
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkInspector_ChainLink::CanInspect(const FCk_Handle& InEntity) const
    -> bool
{
    return ck::IsValid(InEntity) && UCk_Utils_ChainLink_UE::Has(InEntity);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkInspector_ChainLink::Build_NativeBody(const FCk_Handle& InEntity)
    -> TSharedRef<SWidget>
{
    auto Builder = FCkInspectorWidgetBuilder{};
    Builder.AddWidgetRow(FText::FromString(TEXT("Chain:")), SNew(SCkDebug_EntityRef).Entity_Lambda([InEntity]() -> FCk_Handle
    {
        const auto Link = ck_inspector_chain::Get_Link(InEntity);
        return ck::IsValid(Link) ? FCk_Handle{UCk_Utils_ChainLink_UE::Get_Chain(Link)} : FCk_Handle{};
    }));
    Builder.AddRow(FText::FromString(TEXT("Index:")), [](const FCk_Handle& InCurrent)
    {
        const auto Link = ck_inspector_chain::Get_Link(InCurrent);
        return ck::IsValid(Link) ? FText::AsNumber(UCk_Utils_ChainLink_UE::Get_Index(Link)) : FText::FromString(TEXT("--"));
    }, CkStyle::Value_Numeric());
    Builder.AddRow(FText::FromString(TEXT("Distance:")), [](const FCk_Handle& InCurrent)
    {
        const auto Link = ck_inspector_chain::Get_Link(InCurrent);
        return FText::FromString(ck::IsValid(Link) ? ck::Format_UE(TEXT("{} cm"), UCk_Utils_ChainLink_UE::Get_DistanceFromHeadCm(Link)) : TEXT("--"));
    }, CkStyle::Value_Numeric());
    Builder.AddRow(FText::FromString(TEXT("Orientation:")), [](const FCk_Handle& InCurrent)
    {
        const auto Link = ck_inspector_chain::Get_Link(InCurrent);
        return FText::FromString(ck::IsValid(Link) ? ck::Format_UE(TEXT("{}"), UCk_Utils_ChainLink_UE::Get_Orientation(Link)) : TEXT("--"));
    }, CkStyle::Value_Enum());
    return Builder.Build(InEntity, FString{});
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkInspector_Chain::Build_Inspector(const FCk_Handle& InEntity)
    -> TSharedRef<SWidget>
{
    return Build_NativeBody(InEntity);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkInspector_ChainLink::Build_Inspector(const FCk_Handle& InEntity)
    -> TSharedRef<SWidget>
{
    return Build_NativeBody(InEntity);
}
