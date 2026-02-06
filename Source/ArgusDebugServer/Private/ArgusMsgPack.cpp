#include "ArgusMsgPack.h"

// --------------------------------------------------------------------------------------------------------------------
// Minimal MessagePack encoder / decoder
// Spec: https://github.com/msgpack/msgpack/blob/master/spec.md
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

auto FMsgPackWriter::GetBuffer() const -> const TArray<uint8>&
{
    return Buffer;
}

auto FMsgPackWriter::MoveBuffer() -> TArray<uint8>
{
    return MoveTemp(Buffer);
}

// --- Primitives -----------------------------------------------------------------------------------------------------

auto FMsgPackWriter::WriteBool(bool bValue) -> void
{
    // true = 0xc3, false = 0xc2
    WriteByte(bValue ? 0xc3 : 0xc2);
}

auto FMsgPackWriter::WriteUInt8(uint8 Value) -> void
{
    if (Value < 128)
    {
        // positive fixint: 0x00 - 0x7f
        WriteByte(Value);
    }
    else
    {
        // uint 8: 0xcc + 1 byte
        WriteByte(0xcc);
        WriteByte(Value);
    }
}

auto FMsgPackWriter::WriteUInt32(uint32 Value) -> void
{
    if (Value < 128)
    {
        // positive fixint
        WriteByte(static_cast<uint8>(Value));
    }
    else if (Value <= 0xFF)
    {
        // uint 8
        WriteByte(0xcc);
        WriteByte(static_cast<uint8>(Value));
    }
    else if (Value <= 0xFFFF)
    {
        // uint 16
        WriteByte(0xcd);
        WriteBigEndian16(static_cast<uint16>(Value));
    }
    else
    {
        // uint 32
        WriteByte(0xce);
        WriteBigEndian32(Value);
    }
}

auto FMsgPackWriter::WriteUInt64(uint64 Value) -> void
{
    if (Value <= 0xFFFFFFFF)
    {
        // Fits in uint32 — use smaller encoding
        WriteUInt32(static_cast<uint32>(Value));
    }
    else
    {
        // uint 64
        WriteByte(0xcf);
        WriteBigEndian64(Value);
    }
}

auto FMsgPackWriter::WriteString(const FString& Value) -> void
{
    const auto Utf8 = StringCast<UTF8CHAR>(*Value);
    const int32 Len = Utf8.Length();

    if (Len < 32)
    {
        // fixstr: 0xa0 - 0xbf
        WriteByte(static_cast<uint8>(0xa0 | Len));
    }
    else if (Len <= 0xFF)
    {
        // str 8
        WriteByte(0xd9);
        WriteByte(static_cast<uint8>(Len));
    }
    else if (Len <= 0xFFFF)
    {
        // str 16
        WriteByte(0xda);
        WriteBigEndian16(static_cast<uint16>(Len));
    }
    else
    {
        // str 32
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
        // fixarray: 0x90 - 0x9f
        WriteByte(static_cast<uint8>(0x90 | Count));
    }
    else if (Count <= 0xFFFF)
    {
        // array 16
        WriteByte(0xdc);
        WriteBigEndian16(static_cast<uint16>(Count));
    }
    else
    {
        // array 32
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
    return bError;
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
        bError = true;
        return 0;
    }
    return Data[Pos++];
}

auto FMsgPackReader::ReadBigEndian16() -> uint16
{
    if (Remaining() < 2)
    {
        bError = true;
        return 0;
    }
    const uint16 Value = (static_cast<uint16>(Data[Pos]) << 8) |
                          static_cast<uint16>(Data[Pos + 1]);
    Pos += 2;
    return Value;
}

auto FMsgPackReader::ReadBigEndian32() -> uint32
{
    if (Remaining() < 4)
    {
        bError = true;
        return 0;
    }
    const uint32 Value = (static_cast<uint32>(Data[Pos]) << 24) |
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
        bError = true;
        return 0;
    }
    const uint64 Value = (static_cast<uint64>(Data[Pos]) << 56) |
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
    const uint8 Byte = ReadByte();
    if (Byte == 0xc3) return true;
    if (Byte == 0xc2) return false;
    bError = true;
    return false;
}

auto FMsgPackReader::ReadUInt8() -> uint8
{
    const uint8 Byte = PeekByte();

    if (Byte < 0x80)
    {
        // positive fixint
        Pos++;
        return Byte;
    }
    if (Byte == 0xcc)
    {
        // uint 8
        Pos++;
        return ReadByte();
    }

    bError = true;
    return 0;
}

auto FMsgPackReader::ReadUInt32() -> uint32
{
    const uint8 Byte = PeekByte();

    if (Byte < 0x80)
    {
        // positive fixint
        Pos++;
        return Byte;
    }

    switch (Byte)
    {
    case 0xcc: // uint 8
        Pos++;
        return ReadByte();
    case 0xcd: // uint 16
        Pos++;
        return ReadBigEndian16();
    case 0xce: // uint 32
        Pos++;
        return ReadBigEndian32();
    default:
        bError = true;
        return 0;
    }
}

auto FMsgPackReader::ReadUInt64() -> uint64
{
    const uint8 Byte = PeekByte();

    if (Byte < 0x80)
    {
        // positive fixint
        Pos++;
        return Byte;
    }

    switch (Byte)
    {
    case 0xcc: // uint 8
        Pos++;
        return ReadByte();
    case 0xcd: // uint 16
        Pos++;
        return ReadBigEndian16();
    case 0xce: // uint 32
        Pos++;
        return ReadBigEndian32();
    case 0xcf: // uint 64
        Pos++;
        return ReadBigEndian64();
    default:
        bError = true;
        return 0;
    }
}

auto FMsgPackReader::ReadString() -> FString
{
    const uint8 Byte = ReadByte();
    int32 Len = 0;

    if ((Byte & 0xe0) == 0xa0)
    {
        // fixstr (5-bit length)
        Len = Byte & 0x1f;
    }
    else if (Byte == 0xd9)
    {
        // str 8
        Len = ReadByte();
    }
    else if (Byte == 0xda)
    {
        // str 16
        Len = ReadBigEndian16();
    }
    else if (Byte == 0xdb)
    {
        // str 32
        Len = static_cast<int32>(ReadBigEndian32());
    }
    else
    {
        bError = true;
        return FString();
    }

    if (bError || Remaining() < Len)
    {
        bError = true;
        return FString();
    }

    const auto Utf8View = StringCast<TCHAR>(reinterpret_cast<const UTF8CHAR*>(Data + Pos), Len);
    Pos += Len;

    return FString(Utf8View.Length(), Utf8View.Get());
}

// --- Containers -----------------------------------------------------------------------------------------------------

auto FMsgPackReader::ReadArrayHeader() -> uint32
{
    const uint8 Byte = PeekByte();

    if ((Byte & 0xf0) == 0x90)
    {
        // fixarray (4-bit count)
        Pos++;
        return Byte & 0x0f;
    }

    switch (Byte)
    {
    case 0xdc: // array 16
        Pos++;
        return ReadBigEndian16();
    case 0xdd: // array 32
        Pos++;
        return ReadBigEndian32();
    default:
        bError = true;
        return 0;
    }
}

} // namespace Argus
