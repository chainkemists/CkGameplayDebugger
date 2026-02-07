#pragma once

#include "CoreMinimal.h"

// --------------------------------------------------------------------------------------------------------------------
// Minimal MessagePack encoder / decoder
//
// Supports only the types needed by the Argus debug protocol:
//   Write: bool, uint8, uint32, uint64, UTF-8 string, array header
//   Read:  bool, uint8, uint32, uint64, UTF-8 string, array header
//
// Spec reference: https://github.com/msgpack/msgpack/blob/master/spec.md
// --------------------------------------------------------------------------------------------------------------------

namespace Argus
{

// ====================================================================================================================
// Writer
// ====================================================================================================================

class FMsgPackWriter
{
public:
    auto Reserve(int32 InBytes) -> void;
    auto Get_Buffer() const -> const TArray<uint8>&;
    auto Move_Buffer() -> TArray<uint8>;

    // --- Primitives -------------------------------------------------------------------------------------------------

    auto WriteBool(bool Value) -> void;
    auto WriteUInt8(uint8 Value) -> void;
    auto WriteUInt32(uint32 Value) -> void;
    auto WriteUInt64(uint64 Value) -> void;
    auto WriteString(const FString& Value) -> void;

    // --- Containers -------------------------------------------------------------------------------------------------

    auto WriteArrayHeader(uint32 Count) -> void;

private:
    auto WriteByte(uint8 Byte) -> void;
    auto WriteBytes(const uint8* Data, int32 Num) -> void;
    auto WriteBigEndian16(uint16 Value) -> void;
    auto WriteBigEndian32(uint32 Value) -> void;
    auto WriteBigEndian64(uint64 Value) -> void;

    TArray<uint8> Buffer;
};

// ====================================================================================================================
// Reader
// ====================================================================================================================

class FMsgPackReader
{
public:
    explicit FMsgPackReader(const uint8* InData, int32 InSize);

    auto IsError() const -> bool;

    // --- Primitives -------------------------------------------------------------------------------------------------

    auto ReadBool() -> bool;
    auto ReadUInt8() -> uint8;
    auto ReadUInt32() -> uint32;
    auto ReadUInt64() -> uint64;
    auto ReadString() -> FString;

    // --- Containers -------------------------------------------------------------------------------------------------

    auto ReadArrayHeader() -> uint32;

private:
    auto PeekByte() const -> uint8;
    auto ReadByte() -> uint8;
    auto ReadBigEndian16() -> uint16;
    auto ReadBigEndian32() -> uint32;
    auto ReadBigEndian64() -> uint64;
    auto Remaining() const -> int32;

    const uint8* Data  = nullptr;
    int32        Size  = 0;
    int32        Pos   = 0;
    bool         Error = false;
};

} // namespace Argus