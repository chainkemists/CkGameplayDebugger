#include "CkSaveDebugger/Model/CkSaveDebugger_Model.h"

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"

#include "CkEcs/Entity/CkEntity.h"

#include "CkSnapshot/Inspection/CkSnapshot_Inspection.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/World.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"

// --------------------------------------------------------------------------------------------------------------------

auto
    GetTypeHash(
        const FCkSaveDebugger_NodeKey& InKey)
    -> uint32
{
    return HashCombine(::GetTypeHash(static_cast<uint8>(InKey.Kind)), ::GetTypeHash(InKey.SavedId));
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck_save_debugger_model_internal
{
    enum class EWalkColor : uint8
    {
        Unvisited,
        Visiting,
        Done,
    };

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_SyntheticDisplayText(
            ECkSaveDebugger_NodeKind InKind,
            uint32 InSavedId)
        -> FString
    {
        switch (InKind)
        {
            case ECkSaveDebugger_NodeKind::NonPersistedOwnerRoot:
                return ck::Format_UE(TEXT("Owner [{}] — Not Persisted"), InSavedId);
            case ECkSaveDebugger_NodeKind::CycleRoot:
                return TEXT("Owner Cycle");
            default:
                return TEXT("Entity");
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_ToneName(
            ECk_Tone InTone)
        -> const TCHAR*
    {
        switch (InTone)
        {
            case ECk_Tone::Neutral: return TEXT("Neutral");
            case ECk_Tone::Info:    return TEXT("Info");
            case ECk_Tone::Ok:      return TEXT("Ok");
            case ECk_Tone::Warn:    return TEXT("Warn");
            case ECk_Tone::Err:     return TEXT("Err");
            case ECk_Tone::Accent:  return TEXT("Accent");
            default:                return TEXT("Neutral");
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_ValueNodeJson(
            const TSharedPtr<FCk_SnapshotInspection_ValueNode>& InNode)
        -> TSharedPtr<FJsonObject>
    {
        if (NOT InNode.IsValid())
        { return {}; }

        auto Object = MakeShared<FJsonObject>();

        Object->SetStringField(TEXT("field"), InNode->Get_FieldName());
        Object->SetStringField(TEXT("kind"), ck::snapshot::Get_ValueKindText(InNode->Get_Kind()));
        Object->SetStringField(TEXT("type"), InNode->Get_TypeName());
        Object->SetStringField(TEXT("value"), InNode->Get_ValueText());

        if (InNode->Get_Kind() == ECk_SnapshotInspection_ValueKind::Enum)
        { Object->SetNumberField(TEXT("enumValue"), static_cast<double>(InNode->Get_EnumUnderlyingValue())); }

        if (InNode->Get_Kind() == ECk_SnapshotInspection_ValueKind::SavedEntityRef)
        { Object->SetNumberField(TEXT("savedId"), static_cast<double>(InNode->Get_SavedId())); }

        // Child ORDER is the data (property order, container order) — never sorted.
        auto Children = TArray<TSharedPtr<FJsonValue>>{};
        for (const auto& Child : InNode->Get_Children())
        {
            const auto ChildObject = Build_ValueNodeJson(Child);
            if (ChildObject.IsValid())
            { Children.Add(MakeShared<FJsonValueObject>(ChildObject)); }
        }

        if (Children.Num() > 0)
        { Object->SetArrayField(TEXT("children"), Children); }

        return Object;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_DecodeJson(
            const FCk_SnapshotInspection_DecodeResult& InResult)
        -> TSharedPtr<FJsonObject>
    {
        auto Object = MakeShared<FJsonObject>();

        Object->SetStringField(TEXT("status"), ck::snapshot::Get_DecodeStatusText(InResult.Get_Status()));
        Object->SetStringField(TEXT("declaredTypePath"), InResult.Get_DeclaredTypePath());
        Object->SetStringField(TEXT("decodedTypePath"), InResult.Get_DecodedTypePath());
        Object->SetStringField(TEXT("error"), InResult.Get_ErrorText());

        const auto ValueJson = Build_ValueNodeJson(InResult.Get_ValueTreeRoot());
        if (ValueJson.IsValid())
        { Object->SetObjectField(TEXT("value"), ValueJson); }

        return Object;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        ToJsonString(
            const TSharedRef<FJsonObject>& InRoot)
        -> FString
    {
        auto Out = FString{};
        const auto Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Out);
        FJsonSerializer::Serialize(InRoot, Writer);
        return Out;
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSaveDebugger_Model::
    Set_Document(
        const FCk_SnapshotInspection_Document& InDocument)
    -> void
{
    _Document = InDocument;

    // Decode results and selection are addressed by table index / saved id — both meaningless against a new file.
    _PayloadDecodes.Reset();
    _SpawnParamsDecodes.Reset();
    _SelectedEntitySavedId = ck::snapshot::k_NoSavedEntity;
    _SelectedPayloadIndex = INDEX_NONE;

    _TreeRoots.Reset();
    _NodesByKey.Reset();

    // A diff names the OLD current side. Replacing the document — including a reload of the same path — leaves it
    // describing a comparison nothing on screen is still made of.
    Clear_Diff();

    Rebuild_Tree();
    Apply_Filters();
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSaveDebugger_Model::
    Reset()
    -> void
{
    Set_Document(FCk_SnapshotInspection_Document{});
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSaveDebugger_Model::
    Rebuild_Tree()
    -> bool
{
    using namespace ck_save_debugger_model;
    using namespace ck_save_debugger_model_internal;

    const auto Layout = Build_OwnershipLayout(_Document);
    const auto& Entities = _Document.Get_Entities();

    auto Existing = MoveTemp(_NodesByKey);
    _NodesByKey.Reset();

    auto SetChanged = false;

    auto NodesByLayoutIndex = TArray<TSharedPtr<FCkSaveDebugger_TreeNode>>{};
    NodesByLayoutIndex.SetNum(Layout.Nodes.Num());

    for (auto LayoutIndex = 0; LayoutIndex < Layout.Nodes.Num(); ++LayoutIndex)
    {
        const auto& LayoutNode = Layout.Nodes[LayoutIndex];
        const auto Key = FCkSaveDebugger_NodeKey{LayoutNode.Kind, LayoutNode.SavedId};

        auto Item = TSharedPtr<FCkSaveDebugger_TreeNode>{};
        if (auto* Found = Existing.Find(Key))
        {
            Item = *Found;
            Existing.Remove(Key);
        }
        else
        {
            Item = MakeShared<FCkSaveDebugger_TreeNode>();
            SetChanged = true;
        }

        Item->Kind = LayoutNode.Kind;
        Item->SavedId = LayoutNode.SavedId;
        Item->EntityIndex = LayoutNode.EntityIndex;
        Item->Children.Reset();

        if (LayoutNode.Kind == ECkSaveDebugger_NodeKind::Entity && Entities.IsValidIndex(LayoutNode.EntityIndex))
        {
            const auto& Summary = Entities[LayoutNode.EntityIndex];
            Item->DisplayText = Build_EntityDisplayText(Summary, _SaveKeyAnnotations);
            Item->ProvenanceText = ck::snapshot::Get_ProvenanceText(Summary.Get_Entry().Get_Provenance());
            Item->IconId = Get_ProvenanceIconId(Summary.Get_Entry().Get_Provenance());
            Item->SearchText = Build_EntitySearchText(Summary, _SaveKeyAnnotations);
            Item->HasProblems = Summary.Get_HasProblems();
        }
        else
        {
            Item->DisplayText = Get_SyntheticDisplayText(LayoutNode.Kind, LayoutNode.SavedId);
            Item->ProvenanceText = FString{};
            Item->IconId = Get_NodeKindIconId(LayoutNode.Kind);
            Item->SearchText = Item->DisplayText;
            // A cycle is always a defect; a non-persisted owner is a normal top-level shape, so the group only
            // carries a problem badge when one of its member rows does.
            Item->HasProblems = LayoutNode.Kind == ECkSaveDebugger_NodeKind::CycleRoot;
        }

        NodesByLayoutIndex[LayoutIndex] = Item;
        _NodesByKey.Add(Key, Item);
    }

    for (auto LayoutIndex = 0; LayoutIndex < Layout.Nodes.Num(); ++LayoutIndex)
    {
        for (const auto ChildLayoutIndex : Layout.Nodes[LayoutIndex].ChildLayoutIndices)
        {
            NodesByLayoutIndex[LayoutIndex]->Children.Add(NodesByLayoutIndex[ChildLayoutIndex]);

            if (NodesByLayoutIndex[LayoutIndex]->Kind == ECkSaveDebugger_NodeKind::NonPersistedOwnerRoot &&
                NodesByLayoutIndex[ChildLayoutIndex]->HasProblems)
            {
                NodesByLayoutIndex[LayoutIndex]->HasProblems = true;
            }
        }
    }

    _TreeRoots.Reset();
    for (const auto RootLayoutIndex : Layout.RootLayoutIndices)
    {
        _TreeRoots.Add(NodesByLayoutIndex[RootLayoutIndex]);
    }

    if (Existing.Num() > 0)
    { SetChanged = true; }

    // A pure reparent adds and removes nothing, so key churn alone cannot see it — hash the whole shape instead.
    auto Signature = uint32{0};
    for (const auto& LayoutNode : Layout.Nodes)
    {
        Signature = HashCombine(Signature, ::GetTypeHash(static_cast<uint8>(LayoutNode.Kind)));
        Signature = HashCombine(Signature, ::GetTypeHash(LayoutNode.SavedId));
        for (const auto ChildLayoutIndex : LayoutNode.ChildLayoutIndices)
        { Signature = HashCombine(Signature, ::GetTypeHash(ChildLayoutIndex)); }
    }
    for (const auto RootLayoutIndex : Layout.RootLayoutIndices)
    { Signature = HashCombine(Signature, ::GetTypeHash(RootLayoutIndex)); }

    if (Signature != _LayoutSignature)
    {
        _LayoutSignature = Signature;
        SetChanged = true;
    }

    return SetChanged;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSaveDebugger_Model::
    Apply_Filters()
    -> void
{
    using namespace ck_save_debugger_model;

    const auto& Entities = _Document.Get_Entities();

    const auto DoFinalize = [&](FCkSaveDebugger_TreeNode* InNode) -> void
    {
        auto SelfVisible = false;

        if (InNode->Kind == ECkSaveDebugger_NodeKind::Entity && Entities.IsValidIndex(InNode->EntityIndex))
        {
            const auto& Summary = Entities[InNode->EntityIndex];
            SelfVisible = Passes_TextFilter(InNode->SearchText, _FilterString)
                && Passes_ProvenanceMask(Summary.Get_Entry().Get_Provenance(), _ProvenanceMask)
                && (NOT _ProblemsOnly || Summary.Get_HasProblems());
        }

        auto AnyChildVisible = false;
        for (const auto& Child : InNode->Children)
        {
            if (Child.IsValid() && Child->IsVisible)
            { AnyChildVisible = true; break; }
        }

        InNode->IsVisible = SelfVisible || AnyChildVisible;
        InNode->IsSearchMatch = _HighlightString.IsEmpty() || Passes_TextFilter(InNode->SearchText, _HighlightString);
    };

    // Explicit post-order stack: a save's ownership depth is untrusted input and recursion here would be a crash
    // waiting on a pathological file.
    auto Stack = TArray<TPair<FCkSaveDebugger_TreeNode*, int32>>{};

    for (const auto& Root : _TreeRoots)
    {
        if (NOT Root.IsValid())
        { continue; }

        Stack.Reset();
        Stack.Emplace(Root.Get(), 0);

        while (Stack.Num() > 0)
        {
            const auto TopIndex = Stack.Num() - 1;
            auto* Node = Stack[TopIndex].Key;

            if (Stack[TopIndex].Value < Node->Children.Num())
            {
                const auto ChildIndex = Stack[TopIndex].Value;
                ++Stack[TopIndex].Value;

                const auto& Child = Node->Children[ChildIndex];
                if (Child.IsValid())
                { Stack.Emplace(Child.Get(), 0); }

                continue;
            }

            DoFinalize(Node);
            Stack.Pop();
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSaveDebugger_Model::
    Get_PayloadRows_ForEntity(
        uint32 InEntitySavedId) const
    -> TArray<FCkSaveDebugger_PayloadRow>
{
    using namespace ck_save_debugger_model;

    auto Rows = TArray<FCkSaveDebugger_PayloadRow>{};

    for (const auto& Payload : _Document.Get_Payloads())
    {
        if (Payload.Get_Entry().Get_OwnerSavedId() != InEntitySavedId)
        { continue; }

        auto Row = FCkSaveDebugger_PayloadRow{};
        Row.PayloadIndex = Payload.Get_TableIndex();
        Row.OwnerSavedId = Payload.Get_Entry().Get_OwnerSavedId();
        Row.TypePath = Payload.Get_Entry().Get_TypePath();
        Row.ShortTypeName = Build_ShortTypeName(Row.TypePath);
        Row.ByteCount = Payload.Get_ByteCount();
        Row.TypeAvailable = Payload.Get_TypeAvailable();
        Row.HasProblems = Payload.Get_HasProblems();

        Rows.Add(MoveTemp(Row));
    }

    return Rows;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSaveDebugger_Model::
    Build_DiagnosticRows() const
    -> TArray<FCkSaveDebugger_DiagnosticRow>
{
    using namespace ck_save_debugger_model;

    auto Rows = TArray<FCkSaveDebugger_DiagnosticRow>{};

    const auto& Diagnostics = _Document.Get_Diagnostics();
    for (auto Index = 0; Index < Diagnostics.Num(); ++Index)
    {
        const auto& Diagnostic = Diagnostics[Index];

        auto Row = FCkSaveDebugger_DiagnosticRow{};
        Row.DiagnosticIndex = Index;
        Row.Severity = Diagnostic.Get_Severity();
        Row.CodeText = ck::snapshot::Get_DiagnosticCodeText(Diagnostic.Get_Code());
        Row.Message = Diagnostic.Get_Message();
        Row.Target = Get_DiagnosticTarget(_Document, Diagnostic);

        Rows.Add(MoveTemp(Row));
    }

    // Errors read first so the list's opening screen answers the summary tile — a wall of Info observations must
    // never bury the rows that turned it red. DiagnosticIndex still addresses the document, so selection mapping
    // is unaffected; within one severity, document order (the entity-table walk) is preserved.
    Rows.Sort([](const FCkSaveDebugger_DiagnosticRow& InLeft, const FCkSaveDebugger_DiagnosticRow& InRight)
    {
        if (InLeft.Severity != InRight.Severity)
        { return static_cast<uint8>(InLeft.Severity) > static_cast<uint8>(InRight.Severity); }

        return InLeft.DiagnosticIndex < InRight.DiagnosticIndex;
    });

    return Rows;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSaveDebugger_Model::
    Set_Diff(
        const FCk_SnapshotInspection_Diff& InDiff,
        const FString& InBaselineSourceDescription)
    -> void
{
    _Diff = InDiff;
    _DiffBaselineSourceDescription = InBaselineSourceDescription;
    _HasDiff = true;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSaveDebugger_Model::
    Clear_Diff()
    -> void
{
    _Diff = FCk_SnapshotInspection_Diff{};
    _DiffBaselineSourceDescription.Reset();
    _HasDiff = false;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSaveDebugger_Model::
    Build_DiffGroupRows() const
    -> TArray<FCkSaveDebugger_DiffGroupRow>
{
    using namespace ck_save_debugger_model;

    auto Rows = TArray<FCkSaveDebugger_DiffGroupRow>{};

    if (NOT _Diff.Get_Valid())
    { return Rows; }

    const auto& Groups = _Diff.Get_EntityGroups();

    // The diff already sorted these — biggest population change first — and re-sorting here would throw that away.
    for (auto Index = 0; Index < Groups.Num(); ++Index)
    {
        const auto& Group = Groups[Index];

        if (Group.Get_Kind() == ECk_SnapshotInspection_DiffGroupKind::Unchanged && NOT _DiffShowUnchanged)
        { continue; }

        auto Row = FCkSaveDebugger_DiffGroupRow{};
        Row.GroupIndex = Index;
        Row.IdentityPath = Group.Get_IdentityPath();
        Row.DisplayName = Group.Get_DisplayName();
        Row.Kind = Group.Get_Kind();
        Row.KindText = ck::snapshot::Get_DiffGroupKindText(Group.Get_Kind());
        Row.CountBaseline = Group.Get_CountBaseline();
        Row.CountCurrent = Group.Get_CountCurrent();
        Row.PayloadBytesBaseline = Group.Get_PayloadBytesBaseline();
        Row.PayloadBytesCurrent = Group.Get_PayloadBytesCurrent();
        Row.Tone = Get_DiffGroupTone(Row.Kind, Row.CountCurrent - Row.CountBaseline);

        if (Group.Get_SavedIdsCurrent().Num() > 0)
        { Row.FirstCurrentSavedId = Group.Get_SavedIdsCurrent()[0]; }

        Rows.Add(MoveTemp(Row));
    }

    return Rows;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSaveDebugger_Model::
    Store_PayloadDecode(
        int32 InPayloadIndex,
        const FCk_SnapshotInspection_DecodeResult& InResult)
    -> void
{
    _PayloadDecodes.Add(InPayloadIndex, InResult);
}

auto
    FCkSaveDebugger_Model::
    TryGet_PayloadDecode(
        int32 InPayloadIndex) const
    -> const FCk_SnapshotInspection_DecodeResult*
{
    return _PayloadDecodes.Find(InPayloadIndex);
}

auto
    FCkSaveDebugger_Model::
    Store_SpawnParamsDecode(
        int32 InEntityIndex,
        const FCk_SnapshotInspection_DecodeResult& InResult)
    -> void
{
    _SpawnParamsDecodes.Add(InEntityIndex, InResult);
}

auto
    FCkSaveDebugger_Model::
    TryGet_SpawnParamsDecode(
        int32 InEntityIndex) const
    -> const FCk_SnapshotInspection_DecodeResult*
{
    return _SpawnParamsDecodes.Find(InEntityIndex);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    FCkSaveDebugger_Model::
    TryGet_SelectedEntity() const
    -> const FCk_SnapshotInspection_EntitySummary*
{
    const auto Index = _Document.TryGet_EntityIndexForSavedId(_SelectedEntitySavedId);
    if (NOT _Document.Get_Entities().IsValidIndex(Index))
    { return nullptr; }

    return &_Document.Get_Entities()[Index];
}

auto
    FCkSaveDebugger_Model::
    TryGet_SelectedPayload() const
    -> const FCk_SnapshotInspection_PayloadSummary*
{
    if (NOT _Document.Get_Payloads().IsValidIndex(_SelectedPayloadIndex))
    { return nullptr; }

    return &_Document.Get_Payloads()[_SelectedPayloadIndex];
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck_save_debugger_model
{
    auto
        Get_ProvenanceBit(
            ECk_Snapshot_V3_Provenance InProvenance)
        -> uint8
    {
        return static_cast<uint8>(1u << static_cast<uint8>(InProvenance));
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_AllProvenanceMask()
        -> uint8
    {
        return k_AllProvenanceMask;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Passes_ProvenanceMask(
            ECk_Snapshot_V3_Provenance InProvenance,
            uint8 InMask)
        -> bool
    {
        return (InMask & Get_ProvenanceBit(InProvenance)) != 0;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Passes_TextFilter(
            const FString& InSearchText,
            const FString& InFilter)
        -> bool
    {
        if (InFilter.IsEmpty())
        { return true; }

        return InSearchText.Contains(InFilter, ESearchCase::IgnoreCase);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_SavedIdText(
            uint32 InSavedId)
        -> FString
    {
        if (InSavedId == ck::snapshot::k_NoSavedEntity)
        { return TEXT("none"); }

        const auto Entity = FCk_Entity{static_cast<FCk_Entity::IdType>(InSavedId)};
        return ck::Format_UE(TEXT("{} (id {} | v{})"),
            static_cast<int64>(InSavedId),
            static_cast<int32>(Entity.Get_EntityNumber()),
            static_cast<int32>(Entity.Get_VersionNumber()));
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_EntityDisplayText(
            const FCk_SnapshotInspection_EntitySummary& InSummary)
        -> FString
    {
        return Build_EntityDisplayText(InSummary, {});
    }

    auto
        Build_EntityDisplayText(
            const FCk_SnapshotInspection_EntitySummary& InSummary,
            const TMap<FGuid, FString>& InSaveKeyAnnotations)
        -> FString
    {
        const auto Entity = FCk_Entity{static_cast<FCk_Entity::IdType>(InSummary.Get_Entry().Get_SavedId())};

        auto Text = ck::Format_UE(TEXT("[{}|v{}] {}"),
            static_cast<int32>(Entity.Get_EntityNumber()),
            static_cast<int32>(Entity.Get_VersionNumber()),
            InSummary.Get_IdentityText());

        if (const auto& SaveKey = InSummary.Get_Entry().Get_SaveKey();
            SaveKey.IsValid())
        {
            if (const auto* Annotation = InSaveKeyAnnotations.Find(SaveKey))
            { Text = ck::Format_UE(TEXT("{} — {}"), Text, *Annotation); }
        }

        return Text;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_EntitySearchText(
            const FCk_SnapshotInspection_EntitySummary& InSummary)
        -> FString
    {
        return Build_EntitySearchText(InSummary, {});
    }

    auto
        Build_EntitySearchText(
            const FCk_SnapshotInspection_EntitySummary& InSummary,
            const TMap<FGuid, FString>& InSaveKeyAnnotations)
        -> FString
    {
        const auto& Entry = InSummary.Get_Entry();

        auto Parts = TArray<FString>{};
        Parts.Add(Build_SavedIdText(Entry.Get_SavedId()));
        Parts.Add(InSummary.Get_IdentityText());
        Parts.Add(ck::snapshot::Get_ProvenanceText(Entry.Get_Provenance()));
        Parts.Add(Entry.Get_Label());
        Parts.Add(Entry.Get_ScriptClassPath());
        Parts.Add(Entry.Get_ActorClassPath());
        Parts.Add(Entry.Get_PlayerId());

        if (Entry.Get_SaveKey().IsValid())
        {
            Parts.Add(Entry.Get_SaveKey().ToString(EGuidFormats::DigitsWithHyphens));

            if (const auto* Annotation = InSaveKeyAnnotations.Find(Entry.Get_SaveKey()))
            { Parts.Add(*Annotation); }
        }

        for (const auto& Step : Entry.Get_BuildRecipe())
        {
            Parts.Add(Step.Get_ScriptClassPath());
            Parts.Add(Step.Get_ArchetypePath());
        }

        return FString::Join(Parts, TEXT(" "));
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_ShortTypeName(
            const FString& InTypePath)
        -> FString
    {
        if (InTypePath.IsEmpty())
        { return TEXT("(no type path)"); }

        auto Left = FString{};
        auto Right = FString{};

        if (InTypePath.Split(TEXT("."), &Left, &Right, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
        { return Right; }

        if (InTypePath.Split(TEXT("/"), &Left, &Right, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
        { return Right; }

        return InTypePath;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_OwnershipLayout(
            const FCk_SnapshotInspection_Document& InDocument)
        -> FCkSaveDebugger_TreeLayout
    {
        using namespace ck_save_debugger_model_internal;

        auto Layout = FCkSaveDebugger_TreeLayout{};

        const auto& Entities = InDocument.Get_Entities();
        const auto EntityCount = Entities.Num();

        if (EntityCount == 0)
        { return Layout; }

        auto OwnerIndexOf = TArray<int32>{};
        OwnerIndexOf.Init(INDEX_NONE, EntityCount);

        auto NonPersistedOwnerIdOf = TArray<uint32>{};
        NonPersistedOwnerIdOf.Init(ck::snapshot::k_NoSavedEntity, EntityCount);

        for (auto Index = 0; Index < EntityCount; ++Index)
        {
            const auto OwnerSavedId = Entities[Index].Get_Entry().Get_LifetimeOwnerSavedId();
            if (OwnerSavedId == ck::snapshot::k_NoSavedEntity)
            { continue; }

            const auto OwnerIndex = InDocument.TryGet_EntityIndexForSavedId(OwnerSavedId);
            if (OwnerIndex == INDEX_NONE)
            { NonPersistedOwnerIdOf[Index] = OwnerSavedId; }
            else
            { OwnerIndexOf[Index] = OwnerIndex; }
        }

        // Coloured owner-chain walk: every row is entered once, a chain is followed only through Unvisited rows, and
        // a re-entered Visiting row IS the cycle. Bounded by construction — no visited set to forget to check.
        auto Color = TArray<EWalkColor>{};
        Color.Init(EWalkColor::Unvisited, EntityCount);

        auto IsCycleMember = TArray<bool>{};
        IsCycleMember.Init(false, EntityCount);

        auto Path = TArray<int32>{};

        for (auto Index = 0; Index < EntityCount; ++Index)
        {
            if (Color[Index] != EWalkColor::Unvisited)
            { continue; }

            Path.Reset();

            auto Current = Index;
            while (Current != INDEX_NONE && Color[Current] == EWalkColor::Unvisited)
            {
                Color[Current] = EWalkColor::Visiting;
                Path.Add(Current);
                Current = OwnerIndexOf[Current];
            }

            if (Current != INDEX_NONE && Color[Current] == EWalkColor::Visiting)
            {
                const auto CycleStart = Path.Find(Current);
                for (auto PathIndex = CycleStart; PathIndex < Path.Num(); ++PathIndex)
                { IsCycleMember[Path[PathIndex]] = true; }
            }

            for (const auto Visited : Path)
            { Color[Visited] = EWalkColor::Done; }
        }

        Layout.Nodes.Reserve(EntityCount + 2);

        auto NodeIndexOfEntity = TArray<int32>{};
        NodeIndexOfEntity.Init(INDEX_NONE, EntityCount);

        for (auto Index = 0; Index < EntityCount; ++Index)
        {
            auto Node = FCkSaveDebugger_LayoutNode{};
            Node.Kind = ECkSaveDebugger_NodeKind::Entity;
            Node.SavedId = Entities[Index].Get_Entry().Get_SavedId();
            Node.EntityIndex = Index;

            NodeIndexOfEntity[Index] = Layout.Nodes.Add(MoveTemp(Node));
        }

        const auto MakeSyntheticRoot = [&Layout](ECkSaveDebugger_NodeKind InKind, uint32 InSavedId) -> int32
        {
            auto Node = FCkSaveDebugger_LayoutNode{};
            Node.Kind = InKind;
            Node.SavedId = InSavedId;
            Node.EntityIndex = INDEX_NONE;

            return Layout.Nodes.Add(MoveTemp(Node));
        };

        auto NonPersistedOwnerRootIndices = TMap<uint32, int32>{};
        auto CycleRootIndex = int32{INDEX_NONE};
        auto EntityRootIndices = TArray<int32>{};

        for (auto Index = 0; Index < EntityCount; ++Index)
        {
            const auto NodeIndex = NodeIndexOfEntity[Index];

            if (IsCycleMember[Index])
            {
                if (CycleRootIndex == INDEX_NONE)
                { CycleRootIndex = MakeSyntheticRoot(ECkSaveDebugger_NodeKind::CycleRoot, ck::snapshot::k_NoSavedEntity); }

                Layout.Nodes[CycleRootIndex].ChildLayoutIndices.Add(NodeIndex);
                continue;
            }

            if (NonPersistedOwnerIdOf[Index] != ck::snapshot::k_NoSavedEntity)
            {
                const auto OwnerSavedId = NonPersistedOwnerIdOf[Index];
                auto* RootIndex = NonPersistedOwnerRootIndices.Find(OwnerSavedId);
                if (RootIndex == nullptr)
                {
                    RootIndex = &NonPersistedOwnerRootIndices.Add(OwnerSavedId,
                        MakeSyntheticRoot(ECkSaveDebugger_NodeKind::NonPersistedOwnerRoot, OwnerSavedId));
                }

                Layout.Nodes[*RootIndex].ChildLayoutIndices.Add(NodeIndex);
                continue;
            }

            if (OwnerIndexOf[Index] != INDEX_NONE)
            {
                Layout.Nodes[NodeIndexOfEntity[OwnerIndexOf[Index]]].ChildLayoutIndices.Add(NodeIndex);
                continue;
            }

            EntityRootIndices.Add(NodeIndex);
        }

        // A cycle is broken structure and reads first. Non-persisted-owner groups come next — they are the save's
        // normal top-level content (transient-owned roots), one group per absent owner id, sorted for determinism.
        if (CycleRootIndex != INDEX_NONE)
        { Layout.RootLayoutIndices.Add(CycleRootIndex); }

        auto SortedOwnerIds = TArray<uint32>{};
        NonPersistedOwnerRootIndices.GenerateKeyArray(SortedOwnerIds);
        SortedOwnerIds.Sort();
        for (const auto OwnerSavedId : SortedOwnerIds)
        { Layout.RootLayoutIndices.Add(NonPersistedOwnerRootIndices[OwnerSavedId]); }

        Layout.RootLayoutIndices.Append(EntityRootIndices);

        return Layout;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_DiagnosticTarget(
            const FCk_SnapshotInspection_Document& InDocument,
            const FCk_SnapshotInspection_Diagnostic& InDiagnostic)
        -> FCkSaveDebugger_RowTarget
    {
        auto Target = FCkSaveDebugger_RowTarget{};

        const auto& PayloadIndices = InDiagnostic.Get_RelatedPayloadIndices();
        if (PayloadIndices.Num() > 0 && InDocument.Get_Payloads().IsValidIndex(PayloadIndices[0]))
        {
            Target.PayloadIndex = PayloadIndices[0];
            Target.EntitySavedId = InDocument.Get_Payloads()[PayloadIndices[0]].Get_Entry().Get_OwnerSavedId();
        }

        const auto& SavedIds = InDiagnostic.Get_RelatedSavedIds();
        if (SavedIds.Num() > 0 && InDocument.TryGet_EntityIndexForSavedId(SavedIds[0]) != INDEX_NONE)
        { Target.EntitySavedId = SavedIds[0]; }

        if (InDocument.TryGet_EntityIndexForSavedId(Target.EntitySavedId) == INDEX_NONE)
        { Target.EntitySavedId = ck::snapshot::k_NoSavedEntity; }

        return Target;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Format_HexPreview(
            const TArray<uint8>& InBytes,
            int32 InMaxBytes)
        -> FString
    {
        if (InBytes.Num() == 0)
        { return TEXT("(empty)"); }

        const auto Limit = FMath::Clamp(InMaxBytes, 0, InBytes.Num());

        auto Lines = TArray<FString>{};

        for (auto Offset = 0; Offset < Limit; Offset += k_HexBytesPerLine)
        {
            auto Line = FString::Printf(TEXT("%04x:"), Offset);

            const auto LineEnd = FMath::Min(Offset + k_HexBytesPerLine, Limit);
            for (auto Index = Offset; Index < LineEnd; ++Index)
            { Line.Append(FString::Printf(TEXT(" %02x"), InBytes[Index])); }

            Lines.Add(MoveTemp(Line));
        }

        const auto Remaining = InBytes.Num() - Limit;
        if (Remaining > 0)
        { Lines.Add(ck::Format_UE(TEXT("... {} more byte(s) not shown"), Remaining)); }

        return FString::Join(Lines, TEXT("\n"));
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_CompatibilityTone(
            const FCk_SnapshotInspection_Document& InDocument)
        -> ECk_Tone
    {
        switch (InDocument.Get_ReadStatus())
        {
            case ECk_SnapshotInspection_ReadStatus::FileNotFound:
            case ECk_SnapshotInspection_ReadStatus::IoFailure:
            case ECk_SnapshotInspection_ReadStatus::NotCkSnapshot:
            case ECk_SnapshotInspection_ReadStatus::CorruptEnvelope:
            case ECk_SnapshotInspection_ReadStatus::CorruptTables:
                return ECk_Tone::Err;

            case ECk_SnapshotInspection_ReadStatus::UnsupportedFormat:
                return ECk_Tone::Warn;

            default:
                break;
        }

        if (InDocument.Get_Compatibility() != ECk_SnapshotInspection_Compatibility::Current)
        { return ECk_Tone::Warn; }

        if (InDocument.Get_ErrorCount() > 0)
        { return ECk_Tone::Err; }

        if (InDocument.Get_WarningCount() > 0 || Get_OpaquePayloadCount(InDocument) > 0)
        { return ECk_Tone::Warn; }

        return ECk_Tone::Ok;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_DecodeStatusTone(
            ECk_SnapshotInspection_DecodeStatus InStatus)
        -> ECk_Tone
    {
        switch (InStatus)
        {
            case ECk_SnapshotInspection_DecodeStatus::Decoded:
                return ECk_Tone::Ok;

            // Not a defect in the file — this editor simply cannot see the type, or the blob has no offline route.
            case ECk_SnapshotInspection_DecodeStatus::TypeUnavailable:
            case ECk_SnapshotInspection_DecodeStatus::UnsupportedBlob:
                return ECk_Tone::Neutral;

            case ECk_SnapshotInspection_DecodeStatus::DeserializeFailed:
            case ECk_SnapshotInspection_DecodeStatus::DeclaredTypeMismatch:
                return ECk_Tone::Err;

            default:
                return ECk_Tone::Neutral;
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_SeverityTone(
            ECk_SnapshotInspection_Severity InSeverity)
        -> ECk_Tone
    {
        switch (InSeverity)
        {
            case ECk_SnapshotInspection_Severity::Error:   return ECk_Tone::Err;
            case ECk_SnapshotInspection_Severity::Warning: return ECk_Tone::Warn;
            default:                                       return ECk_Tone::Info;
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_ProvenanceIconId(
            ECk_Snapshot_V3_Provenance InProvenance)
        -> FName
    {
        switch (InProvenance)
        {
            case ECk_Snapshot_V3_Provenance::EngineOwned:      return FName{TEXT("World")};
            case ECk_Snapshot_V3_Provenance::ConstructSpawned: return FName{TEXT("Cube")};
            case ECk_Snapshot_V3_Provenance::RuntimeSpawned:   return FName{TEXT("Rocket")};
            case ECk_Snapshot_V3_Provenance::DefinitionBuilt:  return FName{TEXT("Factory")};
            default:                                           return FName{TEXT("EntityInfo")};
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_NodeKindIconId(
            ECkSaveDebugger_NodeKind InKind)
        -> FName
    {
        switch (InKind)
        {
            case ECkSaveDebugger_NodeKind::NonPersistedOwnerRoot: return FName{TEXT("Ghost")};
            case ECkSaveDebugger_NodeKind::CycleRoot:             return FName{TEXT("Trap")};
            default:                                              return FName{TEXT("EntityInfo")};
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_NodeKindTone(
            ECkSaveDebugger_NodeKind InKind)
        -> ECk_Tone
    {
        // Only a cycle is a defect: a Non-Persisted Owner group is normal top-level shape, and colouring it red
        // teaches the reader that every save with a transient root is broken.
        return InKind == ECkSaveDebugger_NodeKind::CycleRoot ? ECk_Tone::Err : ECk_Tone::Neutral;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_OpaquePayloadCount(
            const FCk_SnapshotInspection_Document& InDocument)
        -> int32
    {
        auto Count = 0;
        for (const auto& Payload : InDocument.Get_Payloads())
        {
            if (NOT Payload.Get_TypeAvailable())
            { ++Count; }
        }
        return Count;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        TryGet_VisualizationTransform(
            const FCk_Snapshot_V3_EntityEntry& InEntry)
        -> TOptional<FTransform>
    {
        if (const auto& Saved = InEntry.Get_SavedWorldTransform();
            NOT Saved.Equals(FTransform::Identity))
        { return Saved; }

        if (const auto& SpawnSeed = InEntry.Get_ActorSpawnTransform();
            NOT SpawnSeed.Equals(FTransform::Identity))
        { return SpawnSeed; }

        return {};
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_VisualizationRows(
            const FCk_SnapshotInspection_Document& InDocument,
            const TMap<FGuid, FString>& InSaveKeyAnnotations)
        -> TArray<FCkSaveDebugger_VisualizationRow>
    {
        const auto& Entities = InDocument.Get_Entities();

        auto Rows = TArray<FCkSaveDebugger_VisualizationRow>{};
        auto RowIndexBySavedId = TMap<uint32, int32>{};

        for (auto EntityIndex = 0; EntityIndex < Entities.Num(); ++EntityIndex)
        {
            const auto& Summary = Entities[EntityIndex];
            const auto& Entry = Summary.Get_Entry();

            const auto Transform = TryGet_VisualizationTransform(Entry);
            if (NOT Transform.IsSet())
            { continue; }

            auto& Row = Rows.AddDefaulted_GetRef();
            Row.SavedId = Entry.Get_SavedId();
            Row.EntityIndex = EntityIndex;
            Row.WorldTransform = *Transform;
            Row.DisplayText = Build_EntityDisplayText(Summary, InSaveKeyAnnotations);
            Row.Provenance = Entry.Get_Provenance();
            Row.HasProblems = Summary.Get_HasProblems();
            Row.ActorClassPath = Entry.Get_ActorClassPath();
            Row.ScriptClassPath = Entry.Get_ScriptClassPath();
            Row.VisualKind = Get_VisualKind(Entry);

            RowIndexBySavedId.Add(Entry.Get_SavedId(), Rows.Num() - 1);
        }

        for (auto RowIndex = 0; RowIndex < Rows.Num(); ++RowIndex)
        {
            auto& Row = Rows[RowIndex];
            const auto OwnerSavedId = Entities[Row.EntityIndex].Get_Entry().Get_LifetimeOwnerSavedId();

            // A save is untrusted input: a row naming itself as owner would otherwise draw a zero-length chain line.
            if (const auto* OwnerRowIndex = RowIndexBySavedId.Find(OwnerSavedId);
                OwnerRowIndex != nullptr && *OwnerRowIndex != RowIndex)
            { Row.OwnerRowIndex = *OwnerRowIndex; }
        }

        return Rows;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_VisualKind(
            const FCk_Snapshot_V3_EntityEntry& InEntry)
        -> ECkSaveDebugger_VisualKind
    {
        if (NOT InEntry.Get_ActorClassPath().IsEmpty())
        { return ECkSaveDebugger_VisualKind::MeshGhost; }

        if (InEntry.Get_Provenance() == ECk_Snapshot_V3_Provenance::RuntimeSpawned
            && NOT InEntry.Get_ScriptClassPath().IsEmpty())
        { return ECkSaveDebugger_VisualKind::ConstructionPreview; }

        return ECkSaveDebugger_VisualKind::DiamondOnly;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        TryGet_SavedIdForSaveKey(
            const FCk_SnapshotInspection_Document& InDocument,
            const FGuid& InSaveKey)
        -> uint32
    {
        if (NOT InSaveKey.IsValid())
        { return ck::snapshot::k_NoSavedEntity; }

        for (const auto& Summary : InDocument.Get_Entities())
        {
            if (Summary.Get_Entry().Get_SaveKey() == InSaveKey)
            { return Summary.Get_Entry().Get_SavedId(); }
        }

        return ck::snapshot::k_NoSavedEntity;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_ProvenanceVisualizationColor(
            ECk_Snapshot_V3_Provenance InProvenance)
        -> FLinearColor
    {
        switch (InProvenance)
        {
            case ECk_Snapshot_V3_Provenance::EngineOwned:      return CkStyle::Info();
            case ECk_Snapshot_V3_Provenance::ConstructSpawned: return CkStyle::Ok();
            case ECk_Snapshot_V3_Provenance::RuntimeSpawned:   return CkStyle::Accent();
            case ECk_Snapshot_V3_Provenance::DefinitionBuilt:  return CkStyle::Network();
            default:                                           return CkStyle::TextDim();
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_LevelMatch(
            const FCk_SnapshotInspection_Document& InDocument,
            const FString& InCurrentWorldPackageName)
        -> ECkSaveDebugger_LevelMatch
    {
        const auto SavedPackage = UWorld::RemovePIEPrefix(
            InDocument.Get_Header().Get_WorldAssetPath().GetLongPackageName());

        if (SavedPackage.IsEmpty())
        { return ECkSaveDebugger_LevelMatch::SaveHasNoWorld; }

        const auto CurrentPackage = UWorld::RemovePIEPrefix(InCurrentWorldPackageName);
        if (CurrentPackage.IsEmpty())
        { return ECkSaveDebugger_LevelMatch::NoCurrentWorld; }

        return SavedPackage.Equals(CurrentPackage, ESearchCase::IgnoreCase)
            ? ECkSaveDebugger_LevelMatch::Match
            : ECkSaveDebugger_LevelMatch::Mismatch;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_DiffGroupTone(
            ECk_SnapshotInspection_DiffGroupKind InKind,
            int32 InCountDelta)
        -> ECk_Tone
    {
        switch (InKind)
        {
            // A population that appeared, or grew, is the shape a leak makes — that is the one the reader must not
            // scroll past. Content that vanished is a fact to notice, not a suspect.
            case ECk_SnapshotInspection_DiffGroupKind::Added:
                return ECk_Tone::Warn;

            case ECk_SnapshotInspection_DiffGroupKind::Removed:
                return ECk_Tone::Info;

            case ECk_SnapshotInspection_DiffGroupKind::CountChanged:
                return InCountDelta > 0 ? ECk_Tone::Warn : ECk_Tone::Info;

            case ECk_SnapshotInspection_DiffGroupKind::PayloadsChanged:
                return ECk_Tone::Accent;

            default:
                return ECk_Tone::Neutral;
        }
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_DiffDeltaTone(
            int64 InDelta)
        -> ECk_Tone
    {
        if (InDelta > 0)
        { return ECk_Tone::Warn; }

        if (InDelta < 0)
        { return ECk_Tone::Info; }

        return ECk_Tone::Neutral;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_DiffDeltaText(
            int64 InDelta)
        -> FString
    {
        if (InDelta == 0)
        { return FString{}; }

        return InDelta > 0
            ? ck::Format_UE(TEXT("+{}"), InDelta)
            : ck::Format_UE(TEXT("{}"), InDelta);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_DiffCountText(
            int32 InBaseline,
            int32 InCurrent)
        -> FString
    {
        const auto DeltaText = Build_DiffDeltaText(static_cast<int64>(InCurrent) - static_cast<int64>(InBaseline));

        return DeltaText.IsEmpty()
            ? ck::Format_UE(TEXT("{} -> {}"), InBaseline, InCurrent)
            : ck::Format_UE(TEXT("{} -> {} ({})"), InBaseline, InCurrent, DeltaText);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_DiffBytesText(
            int64 InBaseline,
            int64 InCurrent)
        -> FString
    {
        const auto DeltaText = Build_DiffDeltaText(InCurrent - InBaseline);

        return DeltaText.IsEmpty()
            ? ck::Format_UE(TEXT("{} -> {} B"), InBaseline, InCurrent)
            : ck::Format_UE(TEXT("{} -> {} B ({})"), InBaseline, InCurrent, DeltaText);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_DiffPayloadTypeText(
            const FCk_SnapshotInspection_DiffPayloadTypeRow& InRow)
        -> FString
    {
        const auto Text = ck::Format_UE(TEXT("{}, {}"),
            Build_DiffCountText(InRow.Get_CountBaseline(), InRow.Get_CountCurrent()),
            Build_DiffBytesText(InRow.Get_BytesBaseline(), InRow.Get_BytesCurrent()));

        return InRow.Get_ContentDiffers()
            ? Text + TEXT(" (content differs)")
            : Text;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_DiffReportText(
            const FCkSaveDebugger_Model& InModel)
        -> FString
    {
        const auto& Diff = InModel.Get_Diff();

        auto Lines = TArray<FString>{};

        Lines.Add(TEXT("CK Save Diff report"));
        Lines.Add(ck::Format_UE(TEXT("baseline    : {}"), InModel.Get_DiffBaselineSourceDescription()));
        Lines.Add(ck::Format_UE(TEXT("current     : {}"), InModel.Get_Document().Get_SourceDescription()));

        if (NOT Diff.Get_Valid())
        {
            Lines.Add(ck::Format_UE(TEXT("status      : INVALID - {}"), Diff.Get_InvalidReason()));
            return FString::Join(Lines, TEXT("\n"));
        }

        Lines.Add(TEXT("status      : VALID"));
        Lines.Add(FString{});

        Lines.Add(ck::Format_UE(TEXT("entities    : {}"),
            Build_DiffCountText(Diff.Get_EntityCountBaseline(), Diff.Get_EntityCountCurrent())));
        Lines.Add(ck::Format_UE(TEXT("payloads    : {}"),
            Build_DiffCountText(Diff.Get_PayloadCountBaseline(), Diff.Get_PayloadCountCurrent())));
        Lines.Add(ck::Format_UE(TEXT("table bytes : {}"),
            Build_DiffBytesText(Diff.Get_SnapshotBytesBaseline(), Diff.Get_SnapshotBytesCurrent())));

        Lines.Add(FString{});
        Lines.Add(TEXT("census by provenance:"));
        for (const auto& Row : Diff.Get_Census())
        {
            Lines.Add(ck::Format_UE(TEXT("  {} : {}"),
                ck::snapshot::Get_ProvenanceText(Row.Get_Provenance()),
                Build_DiffCountText(Row.Get_CountBaseline(), Row.Get_CountCurrent())));
        }

        Lines.Add(FString{});
        Lines.Add(ck::Format_UE(TEXT("groups: added {} | removed {} | count-changed {} | payloads-changed {} | unchanged {}"),
            Diff.Get_GroupCountOfKind(ECk_SnapshotInspection_DiffGroupKind::Added),
            Diff.Get_GroupCountOfKind(ECk_SnapshotInspection_DiffGroupKind::Removed),
            Diff.Get_GroupCountOfKind(ECk_SnapshotInspection_DiffGroupKind::CountChanged),
            Diff.Get_GroupCountOfKind(ECk_SnapshotInspection_DiffGroupKind::PayloadsChanged),
            Diff.Get_GroupCountOfKind(ECk_SnapshotInspection_DiffGroupKind::Unchanged)));

        // Every array below is walked in the order the diff already put it in — sorting again here would both throw
        // away the "biggest change first" contract and add a second place for the ordering to drift.
        for (const auto& Group : Diff.Get_EntityGroups())
        {
            Lines.Add(ck::Format_UE(TEXT("  [{}] {} | {} | {} | {}"),
                ck::snapshot::Get_DiffGroupKindText(Group.Get_Kind()),
                Group.Get_DisplayName(),
                Build_DiffCountText(Group.Get_CountBaseline(), Group.Get_CountCurrent()),
                Build_DiffBytesText(Group.Get_PayloadBytesBaseline(), Group.Get_PayloadBytesCurrent()),
                Group.Get_IdentityPath()));
        }

        Lines.Add(FString{});
        Lines.Add(ck::Format_UE(TEXT("payload types ({}):"), Diff.Get_PayloadTypes().Num()));
        for (const auto& Row : Diff.Get_PayloadTypes())
        {
            Lines.Add(ck::Format_UE(TEXT("  {} : {}"), Row.Get_TypePath(), Build_DiffPayloadTypeText(Row)));
        }

        return FString::Join(Lines, TEXT("\n"));
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_JsonExport(
            const FCkSaveDebugger_Model& InModel)
        -> FString
    {
        using namespace ck_save_debugger_model_internal;

        const auto& Document = InModel.Get_Document();
        const auto& Header = Document.Get_Header();
        const auto& Census = Document.Get_Census();

        auto Root = MakeShared<FJsonObject>();

        {
            auto Source = MakeShared<FJsonObject>();
            Source->SetNumberField(TEXT("kind"), static_cast<double>(static_cast<uint8>(Document.Get_SourceKind())));
            Source->SetStringField(TEXT("description"), Document.Get_SourceDescription());
            Source->SetNumberField(TEXT("byteCount"), static_cast<double>(Document.Get_SourceByteCount()));
            Source->SetStringField(TEXT("sha256"), Document.Get_SourceHashHex());
            Source->SetStringField(TEXT("saveGameClass"), Document.Get_SaveGameClassPath());
            Root->SetObjectField(TEXT("source"), Source);
        }

        {
            auto Read = MakeShared<FJsonObject>();
            Read->SetStringField(TEXT("status"), ck::snapshot::Get_ReadStatusText(Document.Get_ReadStatus()));
            Read->SetStringField(TEXT("compatibility"), ck::snapshot::Get_CompatibilityText(Document.Get_Compatibility()));
            Read->SetStringField(TEXT("tone"), Get_ToneName(Get_CompatibilityTone(Document)));
            Read->SetBoolField(TEXT("headerRecovered"), Document.Get_HeaderRecovered());
            Read->SetBoolField(TEXT("tableParseAttempted"), Document.Get_TableParseAttempted());
            Read->SetBoolField(TEXT("tableParseSucceeded"), Document.Get_TableParseSucceeded());
            Root->SetObjectField(TEXT("read"), Read);
        }

        {
            auto HeaderJson = MakeShared<FJsonObject>();
            HeaderJson->SetNumberField(TEXT("formatVersion"), static_cast<double>(Header.Get_FormatVersion()));
            HeaderJson->SetStringField(TEXT("engineVersion"), Header.Get_EngineVersion());
            HeaderJson->SetStringField(TEXT("pluginBuildHash"), Header.Get_PluginBuildHash().ToString(EGuidFormats::DigitsWithHyphens));
            HeaderJson->SetStringField(TEXT("timestampUtc"), Header.Get_TimestampUTC().ToIso8601());
            HeaderJson->SetStringField(TEXT("worldAssetPath"), Header.Get_WorldAssetPath().ToString());
            HeaderJson->SetNumberField(TEXT("entityCount"), static_cast<double>(Header.Get_EntityCount()));
            HeaderJson->SetNumberField(TEXT("engineOwnedCount"), static_cast<double>(Header.Get_EngineOwnedCount()));
            HeaderJson->SetNumberField(TEXT("constructSpawnedCount"), static_cast<double>(Header.Get_ConstructSpawnedCount()));
            HeaderJson->SetNumberField(TEXT("runtimeSpawnedCount"), static_cast<double>(Header.Get_RuntimeSpawnedCount()));
            HeaderJson->SetNumberField(TEXT("definitionBuiltCount"), static_cast<double>(Header.Get_DefinitionBuiltCount()));
            HeaderJson->SetNumberField(TEXT("payloadCount"), static_cast<double>(Header.Get_PayloadCount()));
            HeaderJson->SetNumberField(TEXT("unlabeledConstructSkippedCount"), static_cast<double>(Header.Get_UnlabeledConstructSkippedCount()));
            HeaderJson->SetNumberField(TEXT("anonymousSkippedCount"), static_cast<double>(Header.Get_AnonymousSkippedCount()));
            HeaderJson->SetNumberField(TEXT("unlabeledWithPayloadAuditCount"), static_cast<double>(Header.Get_UnlabeledWithPayloadAuditCount()));
            Root->SetObjectField(TEXT("header"), HeaderJson);
        }

        {
            auto SnapshotJson = MakeShared<FJsonObject>();
            SnapshotJson->SetNumberField(TEXT("byteCount"), static_cast<double>(Document.Get_SnapshotByteCount()));
            SnapshotJson->SetStringField(TEXT("sha256"), Document.Get_SnapshotHashHex());
            Root->SetObjectField(TEXT("snapshot"), SnapshotJson);
        }

        {
            auto CensusJson = MakeShared<FJsonObject>();
            CensusJson->SetNumberField(TEXT("entityCount"), static_cast<double>(Census.Get_EntityCount()));
            CensusJson->SetNumberField(TEXT("engineOwnedCount"), static_cast<double>(Census.Get_EngineOwnedCount()));
            CensusJson->SetNumberField(TEXT("constructSpawnedCount"), static_cast<double>(Census.Get_ConstructSpawnedCount()));
            CensusJson->SetNumberField(TEXT("runtimeSpawnedCount"), static_cast<double>(Census.Get_RuntimeSpawnedCount()));
            CensusJson->SetNumberField(TEXT("definitionBuiltCount"), static_cast<double>(Census.Get_DefinitionBuiltCount()));
            CensusJson->SetNumberField(TEXT("payloadCount"), static_cast<double>(Census.Get_PayloadCount()));
            Root->SetObjectField(TEXT("census"), CensusJson);
        }

        {
            auto Summary = MakeShared<FJsonObject>();
            Summary->SetNumberField(TEXT("errorCount"), static_cast<double>(Document.Get_ErrorCount()));
            Summary->SetNumberField(TEXT("warningCount"), static_cast<double>(Document.Get_WarningCount()));
            Summary->SetNumberField(TEXT("opaquePayloadCount"), static_cast<double>(Get_OpaquePayloadCount(Document)));
            Summary->SetNumberField(TEXT("diagnosticCount"), static_cast<double>(Document.Get_Diagnostics().Num()));
            Root->SetObjectField(TEXT("summary"), Summary);
        }

        {
            auto EntityOrder = TArray<int32>{};
            for (auto Index = 0; Index < Document.Get_Entities().Num(); ++Index)
            { EntityOrder.Add(Index); }

            const auto& Entities = Document.Get_Entities();
            EntityOrder.Sort([&Entities](int32 InLeft, int32 InRight)
            {
                const auto LeftId = Entities[InLeft].Get_Entry().Get_SavedId();
                const auto RightId = Entities[InRight].Get_Entry().Get_SavedId();
                return LeftId != RightId ? LeftId < RightId : InLeft < InRight;
            });

            auto EntityValues = TArray<TSharedPtr<FJsonValue>>{};
            for (const auto Index : EntityOrder)
            {
                const auto& Summary = Entities[Index];
                const auto& Entry = Summary.Get_Entry();

                auto Object = MakeShared<FJsonObject>();
                Object->SetNumberField(TEXT("savedId"), static_cast<double>(Entry.Get_SavedId()));
                Object->SetNumberField(TEXT("tableIndex"), static_cast<double>(Summary.Get_TableIndex()));
                Object->SetStringField(TEXT("provenance"), ck::snapshot::Get_ProvenanceText(Entry.Get_Provenance()));
                Object->SetStringField(TEXT("identity"), Summary.Get_IdentityText());
                Object->SetNumberField(TEXT("lifetimeOwnerSavedId"), static_cast<double>(Entry.Get_LifetimeOwnerSavedId()));
                Object->SetNumberField(TEXT("contextOwnerSavedId"), static_cast<double>(Entry.Get_ContextOwnerSavedId()));
                Object->SetStringField(TEXT("saveKey"), Entry.Get_SaveKey().ToString(EGuidFormats::DigitsWithHyphens));
                Object->SetStringField(TEXT("playerId"), Entry.Get_PlayerId());
                Object->SetStringField(TEXT("label"), Entry.Get_Label());
                Object->SetStringField(TEXT("scriptClassPath"), Entry.Get_ScriptClassPath());
                Object->SetStringField(TEXT("actorClassPath"), Entry.Get_ActorClassPath());
                Object->SetNumberField(TEXT("buildStepCount"), static_cast<double>(Summary.Get_BuildStepCount()));
                Object->SetNumberField(TEXT("spawnParamsByteCount"), static_cast<double>(Summary.Get_SpawnParamsByteCount()));
                Object->SetNumberField(TEXT("actorSaveFieldByteCount"), static_cast<double>(Summary.Get_ActorSaveFieldByteCount()));
                Object->SetBoolField(TEXT("hasProblems"), Summary.Get_HasProblems());

                auto StepValues = TArray<TSharedPtr<FJsonValue>>{};
                for (const auto& Step : Entry.Get_BuildRecipe())
                {
                    auto StepObject = MakeShared<FJsonObject>();
                    StepObject->SetStringField(TEXT("scriptClassPath"), Step.Get_ScriptClassPath());
                    StepObject->SetStringField(TEXT("archetypePath"), Step.Get_ArchetypePath());
                    StepValues.Add(MakeShared<FJsonValueObject>(StepObject));
                }
                Object->SetArrayField(TEXT("buildRecipe"), StepValues);

                // Bounded preview only — a raw blob leaves this tool through its own per-blob export action.
                Object->SetStringField(TEXT("spawnParamsPreview"),
                    Format_HexPreview(Entry.Get_SpawnParamsBytes(), k_MaxHexPreviewBytes));
                Object->SetStringField(TEXT("actorSaveFieldPreview"),
                    Format_HexPreview(Entry.Get_ActorSaveFieldBytes(), k_MaxHexPreviewBytes));

                if (const auto* Decode = InModel.TryGet_SpawnParamsDecode(Summary.Get_TableIndex()))
                { Object->SetObjectField(TEXT("spawnParamsDecode"), Build_DecodeJson(*Decode)); }

                EntityValues.Add(MakeShared<FJsonValueObject>(Object));
            }

            Root->SetArrayField(TEXT("entities"), EntityValues);
        }

        {
            auto PayloadOrder = TArray<int32>{};
            for (auto Index = 0; Index < Document.Get_Payloads().Num(); ++Index)
            { PayloadOrder.Add(Index); }

            const auto& Payloads = Document.Get_Payloads();
            PayloadOrder.Sort([&Payloads](int32 InLeft, int32 InRight)
            {
                const auto LeftOwner = Payloads[InLeft].Get_Entry().Get_OwnerSavedId();
                const auto RightOwner = Payloads[InRight].Get_Entry().Get_OwnerSavedId();
                if (LeftOwner != RightOwner)
                { return LeftOwner < RightOwner; }

                const auto Compare = Payloads[InLeft].Get_Entry().Get_TypePath().Compare(
                    Payloads[InRight].Get_Entry().Get_TypePath(), ESearchCase::CaseSensitive);
                return Compare != 0 ? Compare < 0 : InLeft < InRight;
            });

            auto PayloadValues = TArray<TSharedPtr<FJsonValue>>{};
            for (const auto Index : PayloadOrder)
            {
                const auto& Summary = Payloads[Index];

                auto Object = MakeShared<FJsonObject>();
                Object->SetNumberField(TEXT("tableIndex"), static_cast<double>(Summary.Get_TableIndex()));
                Object->SetNumberField(TEXT("ownerSavedId"), static_cast<double>(Summary.Get_Entry().Get_OwnerSavedId()));
                Object->SetStringField(TEXT("typePath"), Summary.Get_Entry().Get_TypePath());
                Object->SetNumberField(TEXT("byteCount"), static_cast<double>(Summary.Get_ByteCount()));
                Object->SetStringField(TEXT("sha256"), Summary.Get_PayloadHashHex());
                Object->SetBoolField(TEXT("typeAvailable"), Summary.Get_TypeAvailable());
                Object->SetBoolField(TEXT("hasProblems"), Summary.Get_HasProblems());
                Object->SetStringField(TEXT("preview"),
                    Format_HexPreview(Summary.Get_Entry().Get_PayloadBytes(), k_MaxHexPreviewBytes));

                if (const auto* Decode = InModel.TryGet_PayloadDecode(Summary.Get_TableIndex()))
                { Object->SetObjectField(TEXT("decode"), Build_DecodeJson(*Decode)); }

                PayloadValues.Add(MakeShared<FJsonValueObject>(Object));
            }

            Root->SetArrayField(TEXT("payloads"), PayloadValues);
        }

        {
            const auto Rows = InModel.Build_DiagnosticRows();

            auto Order = TArray<int32>{};
            for (auto Index = 0; Index < Rows.Num(); ++Index)
            { Order.Add(Index); }

            Order.Sort([&Rows](int32 InLeft, int32 InRight)
            {
                const auto LeftSeverity = static_cast<uint8>(Rows[InLeft].Severity);
                const auto RightSeverity = static_cast<uint8>(Rows[InRight].Severity);
                if (LeftSeverity != RightSeverity)
                { return LeftSeverity > RightSeverity; }

                const auto CodeCompare = Rows[InLeft].CodeText.Compare(Rows[InRight].CodeText, ESearchCase::CaseSensitive);
                if (CodeCompare != 0)
                { return CodeCompare < 0; }

                const auto MessageCompare = Rows[InLeft].Message.Compare(Rows[InRight].Message, ESearchCase::CaseSensitive);
                return MessageCompare != 0 ? MessageCompare < 0 : InLeft < InRight;
            });

            auto DiagnosticValues = TArray<TSharedPtr<FJsonValue>>{};
            for (const auto Index : Order)
            {
                const auto& Row = Rows[Index];

                auto Object = MakeShared<FJsonObject>();
                Object->SetStringField(TEXT("severity"), ck::snapshot::Get_SeverityText(Row.Severity));
                Object->SetStringField(TEXT("code"), Row.CodeText);
                Object->SetStringField(TEXT("message"), Row.Message);
                Object->SetNumberField(TEXT("targetEntitySavedId"), static_cast<double>(Row.Target.EntitySavedId));
                Object->SetNumberField(TEXT("targetPayloadIndex"), static_cast<double>(Row.Target.PayloadIndex));

                DiagnosticValues.Add(MakeShared<FJsonValueObject>(Object));
            }

            Root->SetArrayField(TEXT("diagnostics"), DiagnosticValues);
        }

        return ToJsonString(Root);
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Build_TextReport(
            const FCkSaveDebugger_Model& InModel)
        -> FString
    {
        const auto& Document = InModel.Get_Document();
        const auto& Header = Document.Get_Header();
        const auto& Census = Document.Get_Census();

        auto Lines = TArray<FString>{};

        Lines.Add(TEXT("CK Save Debugger report"));
        Lines.Add(ck::Format_UE(TEXT("source      : {}"), Document.Get_SourceDescription()));
        Lines.Add(ck::Format_UE(TEXT("file bytes  : {} (sha256 {})"),
            Document.Get_SourceByteCount(), Document.Get_SourceHashHex()));
        Lines.Add(ck::Format_UE(TEXT("read status : {}"), ck::snapshot::Get_ReadStatusText(Document.Get_ReadStatus())));
        Lines.Add(ck::Format_UE(TEXT("compat      : {} (format version {})"),
            ck::snapshot::Get_CompatibilityText(Document.Get_Compatibility()),
            static_cast<int32>(Header.Get_FormatVersion())));
        Lines.Add(ck::Format_UE(TEXT("engine      : {}"), Header.Get_EngineVersion()));
        Lines.Add(ck::Format_UE(TEXT("build hash  : {}"), Header.Get_PluginBuildHash().ToString(EGuidFormats::DigitsWithHyphens)));
        Lines.Add(ck::Format_UE(TEXT("captured    : {}"), Header.Get_TimestampUTC().ToIso8601()));
        Lines.Add(ck::Format_UE(TEXT("map         : {}"), Header.Get_WorldAssetPath().ToString()));
        Lines.Add(ck::Format_UE(TEXT("tables      : {} (sha256 {})"),
            Document.Get_SnapshotByteCount(), Document.Get_SnapshotHashHex()));
        Lines.Add(FString{});
        Lines.Add(ck::Format_UE(TEXT("entities {} | engine-owned {} | construct-spawned {} | runtime-spawned {} | definition-built {}"),
            Census.Get_EntityCount(),
            Census.Get_EngineOwnedCount(),
            Census.Get_ConstructSpawnedCount(),
            Census.Get_RuntimeSpawnedCount(),
            Census.Get_DefinitionBuiltCount()));
        Lines.Add(ck::Format_UE(TEXT("payloads {} | opaque {} | errors {} | warnings {}"),
            Census.Get_PayloadCount(),
            Get_OpaquePayloadCount(Document),
            Document.Get_ErrorCount(),
            Document.Get_WarningCount()));

        const auto Rows = InModel.Build_DiagnosticRows();
        if (Rows.Num() > 0)
        {
            Lines.Add(FString{});
            Lines.Add(TEXT("diagnostics:"));

            for (const auto& Row : Rows)
            {
                Lines.Add(ck::Format_UE(TEXT("  [{}] {} - {}"),
                    ck::snapshot::Get_SeverityText(Row.Severity),
                    Row.CodeText,
                    Row.Message));
            }
        }

        return FString::Join(Lines, TEXT("\n"));
    }
}

// --------------------------------------------------------------------------------------------------------------------
