// Source file name: Squab.h
// Author: Igor Matiushin
// Brief description: Defines relay-side peer state and lightweight vector data used by Dovecote.

#ifndef LAMEPIGEON_SQUAB_H
#define LAMEPIGEON_SQUAB_H

#include <cstdint>
#include <enet/enet.h>

struct Vec3
{
    float X = 0.f;
    float Y = 0.f;
    float Z = 0.f;
};

struct Transform
{
    Vec3     Position;
    float    Yaw   = 0.f;
    float    Pitch = 0.f;
    Vec3     Velocity;
    uint8_t  Flags = 0;
};

struct Squab
{
    Squab() = default;
    Squab(uint32_t inId, ENetPeer* inEnetPeer)
        : Id(inId)
        , EnetPeer(inEnetPeer)
    {
    }

    uint32_t  Id       = 0;
    uint32_t  RoomId   = 0;
    Transform LastTransform;
    ENetPeer* EnetPeer = nullptr;
};

#endif
