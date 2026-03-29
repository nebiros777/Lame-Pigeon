#pragma once

// Source file name: Carrier.h
// Author: Igor Matiushin
// Brief description: Declares the standalone Carrier client SDK interface and replicated proxy accessors.

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

struct _ENetHost;
struct _ENetPeer;
struct _ENetEvent;
struct _ENetPacket;
typedef struct _ENetHost ENetHost;
typedef struct _ENetPeer ENetPeer;
typedef struct _ENetEvent ENetEvent;

namespace LamePigeon { struct ReadBuffer; struct WriteBuffer; enum class PacketType : uint8_t; }

class Carrier
{
public:
    static constexpr int   MaxSnapshotsPerPeer = 16;
    static constexpr float DefaultInterpolationDelay   = 0.1f;
    static constexpr float DefaultMaxExtrapolationTime = 0.25f;

    struct Snapshot
    {
        float  x = 0.f, y = 0.f, z = 0.f;
        float  yaw = 0.f, pitch = 0.f;
        float  vx = 0.f, vy = 0.f, vz = 0.f;
        bool   isFalling = false;
        double receiveTime = 0.0;
    };

    struct InterpolatedState
    {
        float x = 0.f, y = 0.f, z = 0.f;
        float yaw = 0.f, pitch = 0.f;
        float vx = 0.f, vy = 0.f, vz = 0.f;
        float smoothedVx = 0.f, smoothedVy = 0.f, smoothedVz = 0.f;
        bool  isFalling = false;
    };

    using OnSpawnProxyFn     = std::function<void(uint32_t peerId, float x, float y, float z, float yaw, float vx, float vy, float vz, bool isFalling)>;
    using OnDespawnProxyFn   = std::function<void(uint32_t peerId)>;
    using OnProxyInterpolatedFn = std::function<void(uint32_t peerId, const InterpolatedState& state)>;
    using OnRpcReceivedFn    = std::function<void(uint32_t senderPeerId, const std::string& funcName, const std::vector<float>& floatArgs)>;
    using OnConnectedFn      = std::function<void()>;
    using OnDisconnectedFn   = std::function<void()>;
    using OnInteractionRejectedFn  = std::function<void(uint32_t yourEventId, uint8_t reason)>;
    using OnProxyFloatVarUpdatedFn = std::function<void(uint32_t peerId, const std::string& varName)>;

    Carrier();
    ~Carrier();

    void SetInterpolationDelay(float seconds);
    void SetMaxExtrapolationTime(float seconds);
    void SetProxyVelocitySmoothHz(float hz);

    float GetProxyVelocitySmoothHz() const { return ProxyVelocitySmoothHz; }

    bool Connect(const char* host, uint16_t port);
    void Disconnect();
    bool IsConnected() const { return bConnected; }
    float GetPingMs() const { return LastPingMs; }

    void JoinRoom(uint32_t roomId);
    void LeaveRoom();
    int32_t GetCurrentRoomId() const { return CurrentRoomId; }

    void SendPositionUpdate(float x, float y, float z, float yaw, float pitch,
                            float vx, float vy, float vz, bool isFalling);

    void SendReplicatedFloat(const std::string& varName, float value);
    void SendReplicatedBool(const std::string& varName, bool value);
    void SendReplicatedInt(const std::string& varName, int32_t value);

    float  GetProxyFloat(uint32_t peerId, const std::string& varName, float defaultVal = 0.f) const;
    bool   GetProxyBool(uint32_t peerId, const std::string& varName, bool defaultVal = false) const;
    int32_t GetProxyInt(uint32_t peerId, const std::string& varName, int32_t defaultVal = 0) const;

    void BroadcastRPC(const std::string& funcName, const std::vector<float>& floatArgs);
    void SendRPCToPeer(uint32_t targetPeerId, const std::string& funcName, const std::vector<float>& floatArgs);

    void SendInteractionPredict(uint32_t eventId, uint32_t otherPeerId, float deltaToContact,
                                const std::string& myTag, const std::string& theirTag);

    void Pump(float deltaTime);

    void SetOnSpawnProxy(OnSpawnProxyFn f) { OnSpawnProxy = std::move(f); }
    void SetOnDespawnProxy(OnDespawnProxyFn f) { OnDespawnProxy = std::move(f); }
    void SetOnProxyInterpolated(OnProxyInterpolatedFn f) { OnProxyInterpolated = std::move(f); }
    void SetOnRpcReceived(OnRpcReceivedFn f) { OnRpcReceived = std::move(f); }
    void SetOnConnected(OnConnectedFn f) { OnConnected = std::move(f); }
    void SetOnDisconnected(OnDisconnectedFn f) { OnDisconnected = std::move(f); }
    void SetOnInteractionRejected(OnInteractionRejectedFn f) { OnInteractionRejected = std::move(f); }
    void SetOnProxyFloatVarUpdated(OnProxyFloatVarUpdatedFn f) { OnProxyFloatVarUpdated = std::move(f); }

    const std::vector<uint32_t>& GetProxyPeerIds() const { return ProxyPeerIds; }

    bool GetInterpolatedState(uint32_t peerId, InterpolatedState& outState) const;

private:
    void PumpENet();
    void HandlePacket(const uint8_t* data, size_t size);
    void SendRaw(const void* data, size_t size, uint8_t channel, uint32_t flags);
    void PushSnapshot(uint32_t peerId, float x, float y, float z, float yaw, float pitch,
                     float vx, float vy, float vz, bool isFalling, double receiveTime);
    void RunInterpolation(float deltaTime);

    ENetHost* Host = nullptr;
    ENetPeer* Peer = nullptr;
    bool      bConnected = false;

    int32_t   CurrentRoomId = -1;
    float     PingAccumulator = 0.f;
    double    PingTimestamp = 0.0;
    float     LastPingMs = 0.f;

    float InterpolationDelay = DefaultInterpolationDelay;
    float MaxExtrapolationTime = DefaultMaxExtrapolationTime;
    float ProxyVelocitySmoothHz = 10.f;

    struct ProxyState
    {
        std::vector<Snapshot> snapshots;
        float displayVx = 0.f, displayVy = 0.f, displayVz = 0.f;
    };
    std::map<uint32_t, ProxyState> ProxyStates;
    std::vector<uint32_t> ProxyPeerIds;

    std::map<uint32_t, std::map<std::string, float>>  ProxyFloatVars;
    std::map<uint32_t, std::map<std::string, int32_t>> ProxyIntVars;
    std::map<uint32_t, std::map<std::string, bool>>    ProxyBoolVars;
    std::map<uint32_t, std::map<std::string, std::vector<float>>> ProxyVectorVars;

    OnSpawnProxyFn       OnSpawnProxy;
    OnDespawnProxyFn     OnDespawnProxy;
    OnProxyInterpolatedFn OnProxyInterpolated;
    OnRpcReceivedFn      OnRpcReceived;
    OnConnectedFn        OnConnected;
    OnDisconnectedFn     OnDisconnected;
    OnInteractionRejectedFn  OnInteractionRejected;
    OnProxyFloatVarUpdatedFn OnProxyFloatVarUpdated;

    uint32_t NextOutgoingPositionSequenceNumber = 1;
};
