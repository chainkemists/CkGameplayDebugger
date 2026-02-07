#include "ArgusMsgPack.h"

// --------------------------------------------------------------------------------------------------------------------

namespace Argus
{

// ====================================================================================================================
// Writer
// ====================================================================================================================

auto FMsgPackWriter::Reserve(int32 InBytes) -> void
{
    Buffer.Reserve(InBytes);
}

auto FMsgPackWriter::Get_Buffer() const -> const TArray<uint8>&
{
    return Buffer;
}

auto FMsgPackWriter::Move_Buffer() -> TArray<uint8>
{
    return MoveTemp(Buffer);
}

// --- Primitives -----------------------------------------------------------------------------------------------------

auto FMsgPackWriter::WriteBool(bool Value) -> void
{
    WriteByte(Value ? 0xc3 : 0xc2);
}

auto FMsgPackWriter::WriteUInt8(uint8 Value) -> void
{
    if (Value < 128)
    {
        WriteByte(Value);
    }
    else
    {
        WriteByte(0xcc);
        WriteByte(Value);
    }
}

auto FMsgPackWriter::WriteUInt32(uint32 Value) -> void
{
    if (Value < 128)
    {
        WriteByte(static_cast<uint8>(Value));
    }
    else if (Value <= 0xFF)
    {
        WriteByte(0xcc);
        WriteByte(static_cast<uint8>(Value));
    }
    else if (Value <= 0xFFFF)
    {
        WriteByte(0xcd);
        WriteBigEndian16(static_cast<uint16>(Value));
    }
    else
    {
        WriteByte(0xce);
        WriteBigEndian32(Value);
    }
}

auto FMsgPackWriter::WriteUInt64(uint64 Value) -> void
{
    if (Value <= 0xFFFFFFFF)
    {
        WriteUInt32(static_cast<uint32>(Value));
    }
    else
    {
        WriteByte(0xcf);
        WriteBigEndian64(Value);
    }
}

auto FMsgPackWriter::WriteString(const FString& Value) -> void
{
    const auto Utf8 = StringCast<UTF8CHAR>(*Value);
    const auto Len = Utf8.Length();

    if (Len < 32)
    {
        WriteByte(static_cast<uint8>(0xa0 | Len));
    }
    else if (Len <= 0xFF)
    {
        WriteByte(0xd9);
        WriteByte(static_cast<uint8>(Len));
    }
    else if (Len <= 0xFFFF)
    {
        WriteByte(0xda);
        WriteBigEndian16(static_cast<uint16>(Len));
    }
    else
    {
        WriteByte(0xdb);
        WriteBigEndian32(static_cast<uint32>(Len));
    }

    WriteBytes(reinterpret_cast<const uint8*>(Utf8.Get()), Len);
}

// --- Containers -----------------------------------------------------------------------------------------------------

auto FMsgPackWriter::WriteArrayHeader(uint32 Count) -> void
{
    if (Count < 16)
    {
        WriteByte(static_cast<uint8>(0x90 | Count));
    }
    else if (Count <= 0xFFFF)
    {
        WriteByte(0xdc);
        WriteBigEndian16(static_cast<uint16>(Count));
    }
    else
    {
        WriteByte(0xdd);
        WriteBigEndian32(Count);
    }
}

// --- Helpers --------------------------------------------------------------------------------------------------------

auto FMsgPackWriter::WriteByte(uint8 Byte) -> void
{
    Buffer.Add(Byte);
}

auto FMsgPackWriter::WriteBytes(const uint8* Data, int32 Num) -> void
{
    Buffer.Append(Data, Num);
}

auto FMsgPackWriter::WriteBigEndian16(uint16 Value) -> void
{
    WriteByte(static_cast<uint8>((Value >> 8) & 0xFF));
    WriteByte(static_cast<uint8>(Value & 0xFF));
}

auto FMsgPackWriter::WriteBigEndian32(uint32 Value) -> void
{
    WriteByte(static_cast<uint8>((Value >> 24) & 0xFF));
    WriteByte(static_cast<uint8>((Value >> 16) & 0xFF));
    WriteByte(static_cast<uint8>((Value >> 8) & 0xFF));
    WriteByte(static_cast<uint8>(Value & 0xFF));
}

auto FMsgPackWriter::WriteBigEndian64(uint64 Value) -> void
{
    WriteByte(static_cast<uint8>((Value >> 56) & 0xFF));
    WriteByte(static_cast<uint8>((Value >> 48) & 0xFF));
    WriteByte(static_cast<uint8>((Value >> 40) & 0xFF));
    WriteByte(static_cast<uint8>((Value >> 32) & 0xFF));
    WriteByte(static_cast<uint8>((Value >> 24) & 0xFF));
    WriteByte(static_cast<uint8>((Value >> 16) & 0xFF));
    WriteByte(static_cast<uint8>((Value >> 8) & 0xFF));
    WriteByte(static_cast<uint8>(Value & 0xFF));
}

// ====================================================================================================================
// Reader
// ====================================================================================================================

FMsgPackReader::FMsgPackReader(const uint8* InData, int32 InSize)
    : Data(InData)
    , Size(InSize)
{
}

auto FMsgPackReader::IsError() const -> bool
{
    return Error;
}

auto FMsgPackReader::Remaining() const -> int32
{
    return Size - Pos;
}

auto FMsgPackReader::PeekByte() const -> uint8
{
    if (Pos >= Size)
    {
        return 0;
    }
    return Data[Pos];
}

auto FMsgPackReader::ReadByte() -> uint8
{
    if (Pos >= Size)
    {
        Error = true;
        return 0;
    }
    return Data[Pos++];
}

auto FMsgPackReader::ReadBigEndian16() -> uint16
{
    if (Remaining() < 2)
    {
        Error = true;
        return 0;
    }
    const auto Value = static_cast<uint16>(
        (static_cast<uint16>(Data[Pos]) << 8) |
         static_cast<uint16>(Data[Pos + 1]));
    Pos += 2;
    return Value;
}

auto FMsgPackReader::ReadBigEndian32() -> uint32
{
    if (Remaining() < 4)
    {
        Error = true;
        return 0;
    }
    const auto Value =
        (static_cast<uint32>(Data[Pos]) << 24) |
        (static_cast<uint32>(Data[Pos + 1]) << 16) |
        (static_cast<uint32>(Data[Pos + 2]) << 8) |
         static_cast<uint32>(Data[Pos + 3]);
    Pos += 4;
    return Value;
}

auto FMsgPackReader::ReadBigEndian64() -> uint64
{
    if (Remaining() < 8)
    {
        Error = true;
        return 0;
    }
    const auto Value =
        (static_cast<uint64>(Data[Pos]) << 56) |
        (static_cast<uint64>(Data[Pos + 1]) << 48) |
        (static_cast<uint64>(Data[Pos + 2]) << 40) |
        (static_cast<uint64>(Data[Pos + 3]) << 32) |
        (static_cast<uint64>(Data[Pos + 4]) << 24) |
        (static_cast<uint64>(Data[Pos + 5]) << 16) |
        (static_cast<uint64>(Data[Pos + 6]) << 8) |
         static_cast<uint64>(Data[Pos + 7]);
    Pos += 8;
    return Value;
}

// --- Primitives -----------------------------------------------------------------------------------------------------

auto FMsgPackReader::ReadBool() -> bool
{
    const auto Byte = ReadByte();
    if (Byte == 0xc3) { return true; }
    if (Byte == 0xc2) { return false; }
    Error = true;
    return false;
}

auto FMsgPackReader::ReadUInt8() -> uint8
{
    const auto Byte = PeekByte();

    if (Byte < 0x80)
    {
        Pos++;
        return Byte;
    }
    if (Byte == 0xcc)
    {
        Pos++;
        return ReadByte();
    }

    Error = true;
    return 0;
}

auto FMsgPackReader::ReadUInt32() -> uint32
{
    const auto Byte = PeekByte();

    if (Byte < 0x80)
    {
        Pos++;
        return Byte;
    }

    switch (Byte)
    {
    case 0xcc:
        Pos++;
        return ReadByte();
    case 0xcd:
        Pos++;
        return ReadBigEndian16();
    case 0xce:
        Pos++;
        return ReadBigEndian32();
    default:
        Error = true;
        return 0;
    }
}

auto FMsgPackReader::ReadUInt64() -> uint64
{
    const auto Byte = PeekByte();

    if (Byte < 0x80)
    {
        Pos++;
        return Byte;
    }

    switch (Byte)
    {
    case 0xcc:
        Pos++;
        return ReadByte();
    case 0xcd:
        Pos++;
        return ReadBigEndian16();
    case 0xce:
        Pos++;
        return ReadBigEndian32();
    case 0xcf:
        Pos++;
        return ReadBigEndian64();
    default:
        Error = true;
        return 0;
    }
}

auto FMsgPackReader::ReadString() -> FString
{
    const auto Byte = ReadByte();
    int32 Len = 0;

    if ((Byte & 0xe0) == 0xa0)
    {
        Len = Byte & 0x1f;
    }
    else if (Byte == 0xd9)
    {
        Len = ReadByte();
    }
    else if (Byte == 0xda)
    {
        Len = ReadBigEndian16();
    }
    else if (Byte == 0xdb)
    {
        Len = static_cast<int32>(ReadBigEndian32());
    }
    else
    {
        Error = true;
        return FString();
    }

    if (Error || Remaining() < Len)
    {
        Error = true;
        return FString();
    }

    const auto Utf8View = StringCast<TCHAR>(reinterpret_cast<const UTF8CHAR*>(Data + Pos), Len);
    Pos += Len;

    return FString(Utf8View.Length(), Utf8View.Get());
}

// --- Containers -----------------------------------------------------------------------------------------------------

auto FMsgPackReader::ReadArrayHeader() -> uint32
{
    const auto Byte = PeekByte();

    if ((Byte & 0xf0) == 0x90)
    {
        Pos++;
        return Byte & 0x0f;
    }

    switch (Byte)
    {
    case 0xdc:
        Pos++;
        return ReadBigEndian16();
    case 0xdd:
        Pos++;
        return ReadBigEndian32();
    default:
        Error = true;
        return 0;
    }
}

} // namespace Argus