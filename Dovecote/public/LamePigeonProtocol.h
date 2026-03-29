// Source file name: LamePigeonProtocol.h
// Author: Igor Matiushin
// Brief description: Defines packet ids and binary read-write helpers shared by relay-side code.

#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <winsock2.h>

static constexpr uint8_t LP_CHANNEL_RELIABLE   = 0;
static constexpr uint8_t LP_CHANNEL_UNRELIABLE = 1;
static constexpr uint8_t CHANNEL_RELIABLE   = LP_CHANNEL_RELIABLE;
static constexpr uint8_t CHANNEL_UNRELIABLE = LP_CHANNEL_UNRELIABLE;

namespace LamePigeon {

enum class PacketType : uint8_t
{
    JOIN_ROOM       = 0x01,
    POSITION_UPDATE = 0x02,
    SPAWN_PROXY     = 0x03,
    MOVE_PROXY      = 0x04,
    DESPAWN_PROXY   = 0x05,
    RPC_CALL        = 0x06,
    LEAVE_ROOM      = 0x07,
    PING            = 0x08,
    PONG            = 0x09,
    VAR_UPDATE = 0x0A,
    INTERACTION_PREDICT = 0x0B,
    INTERACTION_CONFIRM  = 0x0C,
    INTERACTION_REJECT   = 0x0D,
};

struct WriteBuffer
{
    std::vector<uint8_t> Data;

    void WriteU8(uint8_t Value)
    {
        Data.push_back(Value);
    }

    void WriteU16(uint16_t Value)
    {
        Value = htons(Value);
        uint8_t Bytes[2];
        memcpy(Bytes, &Value, 2);
        Data.insert(Data.end(), Bytes, Bytes + 2);
    }

    void WriteU32(uint32_t Value)
    {
        Value = htonl(Value);
        uint8_t Bytes[4];
        memcpy(Bytes, &Value, 4);
        Data.insert(Data.end(), Bytes, Bytes + 4);
    }

    void WriteF32(float Value)
    {
        uint32_t Bits;
        memcpy(&Bits, &Value, 4);
        WriteU32(Bits);
    }

    void WriteHeader(PacketType Type)
    {
        WriteU8(static_cast<uint8_t>(Type));
    }

    void WriteString(const std::string& Value)
    {
        const uint16_t Len = static_cast<uint16_t>(Value.size());
        WriteU16(Len);
        Data.insert(Data.end(), Value.begin(), Value.end());
    }
};

struct ReadBuffer
{
    const uint8_t* Ptr;
    size_t         Remaining;

    ReadBuffer(const uint8_t* Data, size_t Size)
        : Ptr(Data), Remaining(Size) {}

    bool CanRead(size_t N) const { return Remaining >= N; }

    uint8_t ReadU8()
    {
        uint8_t Value = *Ptr++;
        Remaining--;
        return Value;
    }

    uint16_t ReadU16()
    {
        uint16_t Value;
        memcpy(&Value, Ptr, 2);
        Ptr += 2; Remaining -= 2;
        return ntohs(Value);
    }

    uint32_t ReadU32()
    {
        uint32_t Value;
        memcpy(&Value, Ptr, 4);
        Ptr += 4; Remaining -= 4;
        return ntohl(Value);
    }

    float ReadF32()
    {
        uint32_t Bits = ReadU32();
        float Value;
        memcpy(&Value, &Bits, 4);
        return Value;
    }

    PacketType ReadType() { return static_cast<PacketType>(ReadU8()); }

    std::string ReadString()
    {
        uint16_t Len = ReadU16();
        if (!CanRead(Len))
            return {};
        std::string Result(reinterpret_cast<const char*>(Ptr), Len);
        Ptr += Len;
        Remaining -= Len;
        return Result;
    }
};

}
