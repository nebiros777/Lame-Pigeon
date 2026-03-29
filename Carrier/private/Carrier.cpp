// Source file name: Carrier.cpp
// Author: Igor Matiushin
// Brief description: Implements the standalone Carrier client SDK transport, replication, and prediction hooks.

#include "Carrier.h"
#include "LamePigeonProtocol.h"
#include <enet/enet.h>
#include <algorithm>
#include <chrono>
#include <cstdio>

namespace {

double GetTimeSeconds()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

static constexpr uint8_t ChannelReliable   = LP_CHANNEL_RELIABLE;
static constexpr uint8_t ChannelUnreliable = LP_CHANNEL_UNRELIABLE;

float Lerp(float startValue, float endValue, float interpolationAlpha)
{
    return startValue + (endValue - startValue) * interpolationAlpha;
}

float LerpAngleDegrees(float startDegrees, float endDegrees, float interpolationAlpha)
{
    float angleDelta = endDegrees - startDegrees;
    while (angleDelta > 180.f)  angleDelta -= 360.f;
    while (angleDelta < -180.f) angleDelta += 360.f;
    return startDegrees + angleDelta * interpolationAlpha;
}

}

Carrier::Carrier() = default;

Carrier::~Carrier()
{
    Disconnect();
}

void Carrier::SetInterpolationDelay(float seconds)
{
    InterpolationDelay = seconds;
}

void Carrier::SetMaxExtrapolationTime(float seconds)
{
    MaxExtrapolationTime = seconds;
}

void Carrier::SetProxyVelocitySmoothHz(float hz)
{
    ProxyVelocitySmoothHz = hz > 1e-3f ? hz : 10.f;
}

bool Carrier::Connect(const char* serverHost, uint16_t serverPort)
{
    if (Host) return false;

    if (enet_initialize() != 0)
        return false;

    Host = enet_host_create(nullptr, 1, 2, 0, 0);
    if (!Host) return false;

    ENetAddress serverAddress;
    enet_address_set_host(&serverAddress, serverHost);
    serverAddress.port = serverPort;

    Peer = enet_host_connect(Host, &serverAddress, 2, 0);
    if (!Peer)
    {
        enet_host_destroy(Host);
        Host = nullptr;
        return false;
    }
    return true;
}

void Carrier::Disconnect()
{
    if (CurrentRoomId != -1) LeaveRoom();
    if (Peer) { enet_peer_disconnect(Peer, 0); Peer = nullptr; }
    if (Host)
    {
        ENetEvent disconnectEvent;
        enet_host_service(Host, &disconnectEvent, 200);
        enet_host_destroy(Host);
        Host = nullptr;
        enet_deinitialize();
    }
    bConnected = false;
}

void Carrier::JoinRoom(uint32_t roomId)
{
    if (CurrentRoomId >= 0) LeaveRoom();

    LamePigeon::WriteBuffer writeBuffer;
    writeBuffer.WriteHeader(LamePigeon::PacketType::JOIN_ROOM);
    writeBuffer.WriteU32(roomId);
    SendRaw(writeBuffer.Data.data(), writeBuffer.Data.size(), ChannelReliable, ENET_PACKET_FLAG_RELIABLE);
    CurrentRoomId = static_cast<int32_t>(roomId);
}

void Carrier::LeaveRoom()
{
    if (CurrentRoomId < 0) return;

    LamePigeon::WriteBuffer writeBuffer;
    writeBuffer.WriteHeader(LamePigeon::PacketType::LEAVE_ROOM);
    SendRaw(writeBuffer.Data.data(), writeBuffer.Data.size(), ChannelReliable, ENET_PACKET_FLAG_RELIABLE);

    ProxyStates.clear();
    ProxyPeerIds.clear();
    ProxyFloatVars.clear();
    ProxyIntVars.clear();
    ProxyBoolVars.clear();
    ProxyVectorVars.clear();
    CurrentRoomId = -1;
    NextOutgoingPositionSequenceNumber = 1;
}

void Carrier::SendPositionUpdate(float locationX, float locationY, float locationZ, float yawDegrees, float pitchDegrees,
                                 float velocityX, float velocityY, float velocityZ, bool isFalling)
{
    LamePigeon::WriteBuffer writeBuffer;
    writeBuffer.WriteHeader(LamePigeon::PacketType::POSITION_UPDATE);
    writeBuffer.WriteF32(locationX); writeBuffer.WriteF32(locationY); writeBuffer.WriteF32(locationZ);
    writeBuffer.WriteF32(yawDegrees); writeBuffer.WriteF32(pitchDegrees);
    writeBuffer.WriteF32(velocityX); writeBuffer.WriteF32(velocityY); writeBuffer.WriteF32(velocityZ);
    writeBuffer.WriteU8(isFalling ? 1 : 0);
    writeBuffer.WriteU32(NextOutgoingPositionSequenceNumber++);
    SendRaw(writeBuffer.Data.data(), writeBuffer.Data.size(), ChannelUnreliable, 0);
}

void Carrier::SendReplicatedFloat(const std::string& varName, float value)
{
    LamePigeon::WriteBuffer writeBuffer;
    writeBuffer.WriteHeader(LamePigeon::PacketType::VAR_UPDATE);
    writeBuffer.WriteU32(0);
    writeBuffer.WriteString(varName);
    writeBuffer.WriteU8(0);
    writeBuffer.WriteF32(value);
    SendRaw(writeBuffer.Data.data(), writeBuffer.Data.size(), ChannelReliable, ENET_PACKET_FLAG_RELIABLE);
}

void Carrier::SendReplicatedBool(const std::string& varName, bool value)
{
    LamePigeon::WriteBuffer writeBuffer;
    writeBuffer.WriteHeader(LamePigeon::PacketType::VAR_UPDATE);
    writeBuffer.WriteU32(0);
    writeBuffer.WriteString(varName);
    writeBuffer.WriteU8(2);
    writeBuffer.WriteU8(value ? 1 : 0);
    SendRaw(writeBuffer.Data.data(), writeBuffer.Data.size(), ChannelReliable, ENET_PACKET_FLAG_RELIABLE);
}

void Carrier::SendReplicatedInt(const std::string& varName, int32_t value)
{
    LamePigeon::WriteBuffer writeBuffer;
    writeBuffer.WriteHeader(LamePigeon::PacketType::VAR_UPDATE);
    writeBuffer.WriteU32(0);
    writeBuffer.WriteString(varName);
    writeBuffer.WriteU8(1);
    writeBuffer.WriteU32(static_cast<uint32_t>(value));
    SendRaw(writeBuffer.Data.data(), writeBuffer.Data.size(), ChannelReliable, ENET_PACKET_FLAG_RELIABLE);
}

float Carrier::GetProxyFloat(uint32_t peerId, const std::string& varName, float defaultVal) const
{
    auto peerFloatVariablesIterator = ProxyFloatVars.find(peerId);
    if (peerFloatVariablesIterator == ProxyFloatVars.end()) return defaultVal;
    auto variableIterator = peerFloatVariablesIterator->second.find(varName);
    return (variableIterator != peerFloatVariablesIterator->second.end()) ? variableIterator->second : defaultVal;
}

bool Carrier::GetProxyBool(uint32_t peerId, const std::string& varName, bool defaultVal) const
{
    auto peerBoolVariablesIterator = ProxyBoolVars.find(peerId);
    if (peerBoolVariablesIterator == ProxyBoolVars.end()) return defaultVal;
    auto variableIterator = peerBoolVariablesIterator->second.find(varName);
    return (variableIterator != peerBoolVariablesIterator->second.end()) ? variableIterator->second : defaultVal;
}

int32_t Carrier::GetProxyInt(uint32_t peerId, const std::string& varName, int32_t defaultVal) const
{
    auto peerIntegerVariablesIterator = ProxyIntVars.find(peerId);
    if (peerIntegerVariablesIterator == ProxyIntVars.end()) return defaultVal;
    auto variableIterator = peerIntegerVariablesIterator->second.find(varName);
    return (variableIterator != peerIntegerVariablesIterator->second.end()) ? variableIterator->second : defaultVal;
}

void Carrier::BroadcastRPC(const std::string& funcName, const std::vector<float>& floatArgs)
{
    LamePigeon::WriteBuffer writeBuffer;
    writeBuffer.WriteHeader(LamePigeon::PacketType::RPC_CALL);
    writeBuffer.WriteU32(0xFFFFFFFF);
    writeBuffer.WriteString(funcName);
    writeBuffer.WriteU8(static_cast<uint8_t>(floatArgs.size()));
    for (float floatArgument : floatArgs) { writeBuffer.WriteU8(0); writeBuffer.WriteF32(floatArgument); }
    SendRaw(writeBuffer.Data.data(), writeBuffer.Data.size(), ChannelReliable, ENET_PACKET_FLAG_RELIABLE);
}

void Carrier::SendRPCToPeer(uint32_t targetPeerId, const std::string& funcName, const std::vector<float>& floatArgs)
{
    LamePigeon::WriteBuffer writeBuffer;
    writeBuffer.WriteHeader(LamePigeon::PacketType::RPC_CALL);
    writeBuffer.WriteU32(targetPeerId);
    writeBuffer.WriteString(funcName);
    writeBuffer.WriteU8(static_cast<uint8_t>(floatArgs.size()));
    for (float floatArgument : floatArgs) { writeBuffer.WriteU8(0); writeBuffer.WriteF32(floatArgument); }
    SendRaw(writeBuffer.Data.data(), writeBuffer.Data.size(), ChannelReliable, ENET_PACKET_FLAG_RELIABLE);
}

void Carrier::SendInteractionPredict(uint32_t eventId, uint32_t otherPeerId, float deltaToContact,
                                     const std::string& myTag, const std::string& theirTag)
{
    if (!Peer || !bConnected)
        return;
    LamePigeon::WriteBuffer writeBuffer;
    writeBuffer.WriteHeader(LamePigeon::PacketType::INTERACTION_PREDICT);
    writeBuffer.WriteU32(eventId);
    writeBuffer.WriteU32(otherPeerId);
    writeBuffer.WriteF32(deltaToContact);
    writeBuffer.WriteString(myTag);
    writeBuffer.WriteString(theirTag);
    SendRaw(writeBuffer.Data.data(), writeBuffer.Data.size(), ChannelReliable, ENET_PACKET_FLAG_RELIABLE);
}

void Carrier::Pump(float deltaTime)
{
    PumpENet();
    RunInterpolation(deltaTime);

    if (bConnected)
    {
        PingAccumulator += deltaTime;
        if (PingAccumulator >= 2.f)
        {
            PingAccumulator = 0.f;
            PingTimestamp = GetTimeSeconds();
            LamePigeon::WriteBuffer writeBuffer;
            writeBuffer.WriteHeader(LamePigeon::PacketType::PING);
            SendRaw(writeBuffer.Data.data(), writeBuffer.Data.size(), ChannelReliable, ENET_PACKET_FLAG_RELIABLE);
        }
    }
}

void Carrier::PumpENet()
{
    if (!Host) return;
    ENetEvent networkEvent;
    while (enet_host_service(Host, &networkEvent, 0) > 0)
    {
        switch (networkEvent.type)
        {
        case ENET_EVENT_TYPE_CONNECT:
            bConnected = true;
            if (OnConnected) OnConnected();
            break;
        case ENET_EVENT_TYPE_DISCONNECT:
            bConnected = false;
            CurrentRoomId = -1;
            Peer = nullptr;
            if (OnDisconnected) OnDisconnected();
            break;
        case ENET_EVENT_TYPE_RECEIVE:
            HandlePacket(networkEvent.packet->data, networkEvent.packet->dataLength);
            enet_packet_destroy(networkEvent.packet);
            break;
        default:
            break;
        }
    }
}

void Carrier::SendRaw(const void* data, size_t size, uint8_t channel, uint32_t flags)
{
    if (!Peer || !bConnected || size == 0) return;
    ENetPacket* networkPacket = enet_packet_create(data, size, flags);
    if (networkPacket) enet_peer_send(Peer, channel, networkPacket);
}

void Carrier::PushSnapshot(uint32_t peerId, float locationX, float locationY, float locationZ, float yawDegrees, float pitchDegrees,
                          float velocityX, float velocityY, float velocityZ, bool isFalling, double receiveTime)
{
    ProxyState& proxyState = ProxyStates[peerId];
    if (proxyState.snapshots.size() >= static_cast<size_t>(MaxSnapshotsPerPeer))
        proxyState.snapshots.erase(proxyState.snapshots.begin());
    proxyState.snapshots.push_back({ locationX, locationY, locationZ, yawDegrees, pitchDegrees, velocityX, velocityY, velocityZ, isFalling, receiveTime });
    if (std::find(ProxyPeerIds.begin(), ProxyPeerIds.end(), peerId) == ProxyPeerIds.end())
        ProxyPeerIds.push_back(peerId);
}

void Carrier::HandlePacket(const uint8_t* packetData, size_t packetSize)
{
    LamePigeon::ReadBuffer readBuffer(packetData, packetSize);
    if (!readBuffer.CanRead(1)) return;
    LamePigeon::PacketType packetType = readBuffer.ReadType();
    const double receiveTimeSeconds = GetTimeSeconds();
    if (packetType == LamePigeon::PacketType::SPAWN_PROXY)
    {
        if (!readBuffer.CanRead(33)) return;
        uint32_t remotePeerId = readBuffer.ReadU32();
        float locationX = readBuffer.ReadF32();
        float locationY = readBuffer.ReadF32();
        float locationZ = readBuffer.ReadF32();
        float yawDegrees = readBuffer.ReadF32();
        float velocityX = readBuffer.ReadF32();
        float velocityY = readBuffer.ReadF32();
        float velocityZ = readBuffer.ReadF32();
        uint8_t movementFlags = readBuffer.ReadU8();
        PushSnapshot(remotePeerId, locationX, locationY, locationZ, yawDegrees, 0.f, velocityX, velocityY, velocityZ,
                     (movementFlags & 1) != 0, receiveTimeSeconds);
        if (OnSpawnProxy) OnSpawnProxy(remotePeerId, locationX, locationY, locationZ, yawDegrees, velocityX, velocityY, velocityZ, (movementFlags & 1) != 0);
    }
    else if (packetType == LamePigeon::PacketType::MOVE_PROXY)
    {
        if (!readBuffer.CanRead(37)) return;
        uint32_t remotePeerId = readBuffer.ReadU32();
        float locationX = readBuffer.ReadF32();
        float locationY = readBuffer.ReadF32();
        float locationZ = readBuffer.ReadF32();
        float yawDegrees = readBuffer.ReadF32();
        float pitchDegrees = readBuffer.ReadF32();
        float velocityX = readBuffer.ReadF32();
        float velocityY = readBuffer.ReadF32();
        float velocityZ = readBuffer.ReadF32();
        uint8_t movementFlags = readBuffer.ReadU8();
        PushSnapshot(remotePeerId, locationX, locationY, locationZ, yawDegrees, pitchDegrees, velocityX, velocityY, velocityZ,
                     (movementFlags & 1) != 0, receiveTimeSeconds);
    }
    else if (packetType == LamePigeon::PacketType::DESPAWN_PROXY)
    {
        if (!readBuffer.CanRead(4)) return;
        uint32_t remotePeerId = readBuffer.ReadU32();
        ProxyStates.erase(remotePeerId);
        ProxyPeerIds.erase(std::remove(ProxyPeerIds.begin(), ProxyPeerIds.end(), remotePeerId), ProxyPeerIds.end());
        ProxyFloatVars.erase(remotePeerId);
        ProxyIntVars.erase(remotePeerId);
        ProxyBoolVars.erase(remotePeerId);
        ProxyVectorVars.erase(remotePeerId);
        if (OnDespawnProxy) OnDespawnProxy(remotePeerId);
    }
    else if (packetType == LamePigeon::PacketType::PONG)
    {
        LastPingMs = static_cast<float>((GetTimeSeconds() - PingTimestamp) * 1000.0);
    }
    else if (packetType == LamePigeon::PacketType::INTERACTION_CONFIRM)
    {
        if (!readBuffer.CanRead(8)) return;
        (void)readBuffer.ReadU32();
        (void)readBuffer.ReadU32();
    }
    else if (packetType == LamePigeon::PacketType::INTERACTION_REJECT)
    {
        if (!readBuffer.CanRead(5)) return;
        const uint32_t localEventId = readBuffer.ReadU32();
        const uint8_t rejectReason = readBuffer.ReadU8();
        if (OnInteractionRejected) OnInteractionRejected(localEventId, rejectReason);
    }
    else if (packetType == LamePigeon::PacketType::VAR_UPDATE)
    {
        if (!readBuffer.CanRead(4 + 2)) return;
        uint32_t remotePeerId = readBuffer.ReadU32();
        std::string variableName = readBuffer.ReadString();
        if (!readBuffer.CanRead(1)) return;
        uint8_t variableType = readBuffer.ReadU8();
        if (variableType == 0 && readBuffer.CanRead(4))
        {
            const float floatValue = readBuffer.ReadF32();
            ProxyFloatVars[remotePeerId][variableName] = floatValue;
            if (OnProxyFloatVarUpdated) OnProxyFloatVarUpdated(remotePeerId, variableName);
        }
        else if (variableType == 1 && readBuffer.CanRead(4))
            ProxyIntVars[remotePeerId][variableName] = static_cast<int32_t>(readBuffer.ReadU32());
        else if (variableType == 2 && readBuffer.CanRead(1))
            ProxyBoolVars[remotePeerId][variableName] = readBuffer.ReadU8() != 0;
        else if (variableType == 3 && readBuffer.CanRead(12))
        {
            float vectorX = readBuffer.ReadF32();
            float vectorY = readBuffer.ReadF32();
            float vectorZ = readBuffer.ReadF32();
            ProxyVectorVars[remotePeerId][variableName] = { vectorX, vectorY, vectorZ };
        }
    }
    else if (packetType == LamePigeon::PacketType::RPC_CALL)
    {
        if (!readBuffer.CanRead(4 + 2)) return;
        uint32_t senderPeerId = readBuffer.ReadU32();
        std::string functionName = readBuffer.ReadString();
        if (!readBuffer.CanRead(1)) return;
        uint8_t argumentCount = readBuffer.ReadU8();
        std::vector<float> floatArguments;
        for (uint8_t argumentIndex = 0; argumentIndex < argumentCount; ++argumentIndex)
        {
            if (!readBuffer.CanRead(1)) break;
            uint8_t argumentType = readBuffer.ReadU8();
            if (argumentType == 0 && readBuffer.CanRead(4)) floatArguments.push_back(readBuffer.ReadF32());
        }
        if (OnRpcReceived) OnRpcReceived(senderPeerId, functionName, floatArguments);
    }
}

void Carrier::RunInterpolation(float deltaTime)
{
    const double currentTime = GetTimeSeconds();
    const double renderTime = currentTime - InterpolationDelay;
    const float maxExtrapolationSeconds = MaxExtrapolationTime;

    for (auto proxyStateIterator = ProxyStates.begin(); proxyStateIterator != ProxyStates.end(); ++proxyStateIterator)
    {
        uint32_t peerId = proxyStateIterator->first;
        ProxyState& proxyState = proxyStateIterator->second;
        const std::vector<Snapshot>& snapshots = proxyState.snapshots;
        if (snapshots.empty()) continue;

        InterpolatedState interpolatedState;
        if (snapshots.size() == 1)
        {
            const Snapshot& latestSnapshot = snapshots[0];
            double elapsedSeconds = (std::min)((std::max)(currentTime - latestSnapshot.receiveTime, 0.0), static_cast<double>(maxExtrapolationSeconds));
            interpolatedState.x = latestSnapshot.x + latestSnapshot.vx * static_cast<float>(elapsedSeconds);
            interpolatedState.y = latestSnapshot.y + latestSnapshot.vy * static_cast<float>(elapsedSeconds);
            interpolatedState.z = latestSnapshot.z + latestSnapshot.vz * static_cast<float>(elapsedSeconds);
            interpolatedState.yaw = latestSnapshot.yaw; interpolatedState.pitch = latestSnapshot.pitch;
            interpolatedState.vx = latestSnapshot.vx; interpolatedState.vy = latestSnapshot.vy; interpolatedState.vz = latestSnapshot.vz;
            interpolatedState.isFalling = latestSnapshot.isFalling;
        }
        else if (renderTime <= snapshots[0].receiveTime)
        {
            const Snapshot& earliestSnapshot = snapshots[0];
            interpolatedState.x = earliestSnapshot.x; interpolatedState.y = earliestSnapshot.y; interpolatedState.z = earliestSnapshot.z;
            interpolatedState.yaw = earliestSnapshot.yaw; interpolatedState.pitch = earliestSnapshot.pitch;
            interpolatedState.vx = earliestSnapshot.vx; interpolatedState.vy = earliestSnapshot.vy; interpolatedState.vz = earliestSnapshot.vz;
            interpolatedState.isFalling = earliestSnapshot.isFalling;
        }
        else if (renderTime >= snapshots.back().receiveTime)
        {
            const Snapshot& latestSnapshot = snapshots.back();
            double elapsedSeconds = (std::min)((std::max)(renderTime - latestSnapshot.receiveTime, 0.0), static_cast<double>(maxExtrapolationSeconds));
            interpolatedState.x = latestSnapshot.x + latestSnapshot.vx * static_cast<float>(elapsedSeconds);
            interpolatedState.y = latestSnapshot.y + latestSnapshot.vy * static_cast<float>(elapsedSeconds);
            interpolatedState.z = latestSnapshot.z + latestSnapshot.vz * static_cast<float>(elapsedSeconds);
            interpolatedState.yaw = latestSnapshot.yaw; interpolatedState.pitch = latestSnapshot.pitch;
            interpolatedState.vx = latestSnapshot.vx; interpolatedState.vy = latestSnapshot.vy; interpolatedState.vz = latestSnapshot.vz;
            interpolatedState.isFalling = latestSnapshot.isFalling;
        }
        else
        {
            size_t segmentIndex = 0;
            for (size_t snapshotIndex = 0; snapshotIndex < snapshots.size() - 1; ++snapshotIndex)
            {
                if (snapshots[snapshotIndex + 1].receiveTime >= renderTime) { segmentIndex = snapshotIndex; break; }
            }
            const Snapshot& previousSnapshot = snapshots[segmentIndex];
            const Snapshot& nextSnapshot = snapshots[segmentIndex + 1];
            double timeSpanSeconds = nextSnapshot.receiveTime - previousSnapshot.receiveTime;
            float interpolationAlpha = (timeSpanSeconds > 1e-9)
                ? static_cast<float>((std::max)(0.0, (std::min)(1.0, (renderTime - previousSnapshot.receiveTime) / timeSpanSeconds)))
                : 1.f;

            interpolatedState.x = Lerp(previousSnapshot.x, nextSnapshot.x, interpolationAlpha);
            interpolatedState.y = Lerp(previousSnapshot.y, nextSnapshot.y, interpolationAlpha);
            interpolatedState.z = Lerp(previousSnapshot.z, nextSnapshot.z, interpolationAlpha);
            interpolatedState.yaw = LerpAngleDegrees(previousSnapshot.yaw, nextSnapshot.yaw, interpolationAlpha);
            interpolatedState.pitch = Lerp(previousSnapshot.pitch, nextSnapshot.pitch, interpolationAlpha);
            interpolatedState.vx = Lerp(previousSnapshot.vx, nextSnapshot.vx, interpolationAlpha);
            interpolatedState.vy = Lerp(previousSnapshot.vy, nextSnapshot.vy, interpolationAlpha);
            interpolatedState.vz = Lerp(previousSnapshot.vz, nextSnapshot.vz, interpolationAlpha);
            interpolatedState.isFalling = (interpolationAlpha < 0.5f) ? previousSnapshot.isFalling : nextSnapshot.isFalling;
        }

        const float smoothingRate = ProxyVelocitySmoothHz * deltaTime;
        proxyState.displayVx = Lerp(proxyState.displayVx, interpolatedState.vx, (std::min)(1.f, smoothingRate));
        proxyState.displayVy = Lerp(proxyState.displayVy, interpolatedState.vy, (std::min)(1.f, smoothingRate));
        proxyState.displayVz = Lerp(proxyState.displayVz, interpolatedState.vz, (std::min)(1.f, smoothingRate));
        interpolatedState.smoothedVx = proxyState.displayVx;
        interpolatedState.smoothedVy = proxyState.displayVy;
        interpolatedState.smoothedVz = proxyState.displayVz;

        if (OnProxyInterpolated) OnProxyInterpolated(peerId, interpolatedState);
    }
}

bool Carrier::GetInterpolatedState(uint32_t peerId, InterpolatedState& outState) const
{
    auto proxyStateIterator = ProxyStates.find(peerId);
    if (proxyStateIterator == ProxyStates.end() || proxyStateIterator->second.snapshots.empty()) return false;
    const double currentTime = GetTimeSeconds();
    const double renderTime = currentTime - InterpolationDelay;
    const std::vector<Snapshot>& snapshots = proxyStateIterator->second.snapshots;
    const ProxyState& proxyState = proxyStateIterator->second;
    const float maxExtrapolationSeconds = MaxExtrapolationTime;

    if (snapshots.size() == 1)
    {
        const Snapshot& latestSnapshot = snapshots[0];
        double elapsedSeconds = (std::min)((std::max)(currentTime - latestSnapshot.receiveTime, 0.0), static_cast<double>(maxExtrapolationSeconds));
        outState.x = latestSnapshot.x + latestSnapshot.vx * static_cast<float>(elapsedSeconds);
        outState.y = latestSnapshot.y + latestSnapshot.vy * static_cast<float>(elapsedSeconds);
        outState.z = latestSnapshot.z + latestSnapshot.vz * static_cast<float>(elapsedSeconds);
        outState.yaw = latestSnapshot.yaw; outState.pitch = latestSnapshot.pitch;
        outState.vx = latestSnapshot.vx; outState.vy = latestSnapshot.vy; outState.vz = latestSnapshot.vz;
        outState.smoothedVx = proxyState.displayVx; outState.smoothedVy = proxyState.displayVy; outState.smoothedVz = proxyState.displayVz;
        outState.isFalling = latestSnapshot.isFalling;
        return true;
    }
    if (renderTime <= snapshots[0].receiveTime)
    {
        const Snapshot& earliestSnapshot = snapshots[0];
        outState.x = earliestSnapshot.x; outState.y = earliestSnapshot.y; outState.z = earliestSnapshot.z;
        outState.yaw = earliestSnapshot.yaw; outState.pitch = earliestSnapshot.pitch;
        outState.vx = earliestSnapshot.vx; outState.vy = earliestSnapshot.vy; outState.vz = earliestSnapshot.vz;
        outState.smoothedVx = proxyState.displayVx; outState.smoothedVy = proxyState.displayVy; outState.smoothedVz = proxyState.displayVz;
        outState.isFalling = earliestSnapshot.isFalling;
        return true;
    }
    if (renderTime >= snapshots.back().receiveTime)
    {
        const Snapshot& latestSnapshot = snapshots.back();
        double elapsedSeconds = (std::min)((std::max)(renderTime - latestSnapshot.receiveTime, 0.0), static_cast<double>(maxExtrapolationSeconds));
        outState.x = latestSnapshot.x + latestSnapshot.vx * static_cast<float>(elapsedSeconds);
        outState.y = latestSnapshot.y + latestSnapshot.vy * static_cast<float>(elapsedSeconds);
        outState.z = latestSnapshot.z + latestSnapshot.vz * static_cast<float>(elapsedSeconds);
        outState.yaw = latestSnapshot.yaw; outState.pitch = latestSnapshot.pitch;
        outState.vx = latestSnapshot.vx; outState.vy = latestSnapshot.vy; outState.vz = latestSnapshot.vz;
        outState.smoothedVx = proxyState.displayVx; outState.smoothedVy = proxyState.displayVy; outState.smoothedVz = proxyState.displayVz;
        outState.isFalling = latestSnapshot.isFalling;
        return true;
    }
    size_t segmentIndex = 0;
    for (size_t snapshotIndex = 0; snapshotIndex < snapshots.size() - 1; ++snapshotIndex)
    {
        if (snapshots[snapshotIndex + 1].receiveTime >= renderTime) { segmentIndex = snapshotIndex; break; }
    }
    const Snapshot& previousSnapshot = snapshots[segmentIndex];
    const Snapshot& nextSnapshot = snapshots[segmentIndex + 1];
    double timeSpanSeconds = nextSnapshot.receiveTime - previousSnapshot.receiveTime;
    float interpolationAlpha = (timeSpanSeconds > 1e-9)
        ? static_cast<float>((std::max)(0.0, (std::min)(1.0, (renderTime - previousSnapshot.receiveTime) / timeSpanSeconds)))
        : 1.f;
    outState.x = Lerp(previousSnapshot.x, nextSnapshot.x, interpolationAlpha);
    outState.y = Lerp(previousSnapshot.y, nextSnapshot.y, interpolationAlpha);
    outState.z = Lerp(previousSnapshot.z, nextSnapshot.z, interpolationAlpha);
    outState.yaw = LerpAngleDegrees(previousSnapshot.yaw, nextSnapshot.yaw, interpolationAlpha);
    outState.pitch = Lerp(previousSnapshot.pitch, nextSnapshot.pitch, interpolationAlpha);
    outState.vx = Lerp(previousSnapshot.vx, nextSnapshot.vx, interpolationAlpha);
    outState.vy = Lerp(previousSnapshot.vy, nextSnapshot.vy, interpolationAlpha);
    outState.vz = Lerp(previousSnapshot.vz, nextSnapshot.vz, interpolationAlpha);
    outState.smoothedVx = proxyState.displayVx; outState.smoothedVy = proxyState.displayVy; outState.smoothedVz = proxyState.displayVz;
    outState.isFalling = (interpolationAlpha < 0.5f) ? previousSnapshot.isFalling : nextSnapshot.isFalling;
    return true;
}
