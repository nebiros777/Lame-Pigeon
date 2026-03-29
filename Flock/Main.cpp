// Source file name: Main.cpp
// Author: Igor Matiushin
// Brief description: Runs the standalone Flock stress client that simulates many Carrier-based peers.

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

#include "Carrier.h"

namespace Config {
    constexpr int   NumBots            = 100;
    constexpr int   RoomId             = 42;
    constexpr float WorldHalfExtent    = 5200.f;
    constexpr float MinSpawnSeparation = 650.f;
    constexpr float SendRateHz         = 20.f;
    constexpr float RunSpeed           = 350.f;
    constexpr float JumpIntervalMin    = 1.2f;
    constexpr float JumpIntervalMax    = 3.2f;
}
namespace {

constexpr int   NUM_BOTS        = Config::NumBots;
constexpr int   ROOM_ID         = Config::RoomId;
constexpr float WORLD_MIN       = -Config::WorldHalfExtent;
constexpr float WORLD_MAX       = Config::WorldHalfExtent;
constexpr float FLOOR_Z       = 100.f;
constexpr float SEND_INTERVAL   = 1.f / Config::SendRateHz;
constexpr float RUN_SPEED       = Config::RunSpeed;
constexpr float JUMP_VZ         = 450.f;
constexpr float GRAVITY         = -1200.f;
constexpr float DIR_CHANGE_INTERVAL_MIN = 1.5f;
constexpr float DIR_CHANGE_INTERVAL_MAX = 4.f;
constexpr float JUMP_INTERVAL_MIN       = Config::JumpIntervalMin;
constexpr float JUMP_INTERVAL_MAX       = Config::JumpIntervalMax;
constexpr float DASH_COOLDOWN_SECONDS   = 1.4f;
constexpr float DASH_TRIGGER_CHANCE     = 0.12f;

struct BotState
{
    float x = 0.f, y = 0.f, z = FLOOR_Z;
    float yawDegrees = 0.f;
    float velocityX = 0.f, velocityY = 0.f, velocityZ = 0.f;
    bool  isFalling = false;
    float directionChangeTimerSeconds = 0.f;
    float jumpTimerSeconds            = 0.f;
    float dashCooldownSeconds         = 0.f;
};

float RandFloat(float minimumValue, float maximumValue)
{
    float normalizedRandom = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    return minimumValue + normalizedRandom * (maximumValue - minimumValue);
}

void UpdateBotSimulation(BotState& botState, float deltaTimeSeconds, Carrier* jumpRpcCarrier)
{
    botState.directionChangeTimerSeconds -= deltaTimeSeconds;
    if (botState.directionChangeTimerSeconds <= 0.f)
    {
        botState.yawDegrees = RandFloat(0.f, 360.f);
        botState.directionChangeTimerSeconds =
            RandFloat(DIR_CHANGE_INTERVAL_MIN, DIR_CHANGE_INTERVAL_MAX);
    }

    const float yawRadians = botState.yawDegrees * 3.14159265f / 180.f;
    botState.velocityX = RUN_SPEED * std::cos(yawRadians);
    botState.velocityY = RUN_SPEED * std::sin(yawRadians);

    botState.dashCooldownSeconds -= deltaTimeSeconds;
    if (botState.dashCooldownSeconds <= 0.f && RandFloat(0.f, 1.f) < DASH_TRIGGER_CHANCE)
    {
        const float dashBoost = 900.f;
        botState.velocityX += dashBoost * std::cos(yawRadians);
        botState.velocityY += dashBoost * std::sin(yawRadians);
        botState.dashCooldownSeconds = DASH_COOLDOWN_SECONDS;
    }

    if (botState.isFalling)
    {
        botState.velocityZ += GRAVITY * deltaTimeSeconds;
        botState.z += botState.velocityZ * deltaTimeSeconds;
        if (botState.z <= FLOOR_Z)
        {
            botState.z         = FLOOR_Z;
            botState.velocityZ = 0.f;
            botState.isFalling = false;
        }
    }
    else
    {
        botState.jumpTimerSeconds -= deltaTimeSeconds;
        if (botState.jumpTimerSeconds <= 0.f)
        {
            botState.velocityZ        = JUMP_VZ;
            botState.isFalling        = true;
            botState.jumpTimerSeconds = RandFloat(JUMP_INTERVAL_MIN, JUMP_INTERVAL_MAX);
            if (jumpRpcCarrier)
                jumpRpcCarrier->BroadcastRPC(std::string("ReplicateJump"), {});
        }
        else
        {
            botState.velocityZ = 0.f;
        }
    }

    botState.x += botState.velocityX * deltaTimeSeconds;
    botState.y += botState.velocityY * deltaTimeSeconds;
    if (!botState.isFalling)
        botState.z = FLOOR_Z;

    if (botState.x < WORLD_MIN) botState.x = WORLD_MIN;
    if (botState.x > WORLD_MAX) botState.x = WORLD_MAX;
    if (botState.y < WORLD_MIN) botState.y = WORLD_MIN;
    if (botState.y > WORLD_MAX) botState.y = WORLD_MAX;
    if (botState.z < FLOOR_Z) botState.z = FLOOR_Z;
}

void SpawnBotsSpread(std::vector<BotState>& bots)
{
    constexpr float MinSep   = Config::MinSpawnSeparation;
    constexpr float MinSepSq = MinSep * MinSep;
    for (size_t i = 0; i < bots.size(); ++i)
    {
        int attempts = 0;
        for (; attempts < 400; ++attempts)
        {
            bots[i].x = RandFloat(WORLD_MIN, WORLD_MAX);
            bots[i].y = RandFloat(WORLD_MIN, WORLD_MAX);
            bots[i].z = FLOOR_Z;
            bool ok = true;
            for (size_t j = 0; j < i; ++j)
            {
                const float dx = bots[i].x - bots[j].x;
                const float dy = bots[i].y - bots[j].y;
                if (dx * dx + dy * dy < MinSepSq)
                {
                    ok = false;
                    break;
                }
            }
            if (ok)
                break;
        }
        if (attempts >= 400)
        {
            const float angle = static_cast<float>(i) * 6.2831853f / static_cast<float>(bots.size());
            const float radius =
                MinSep * (1.f + static_cast<float>(i % 5)) + RandFloat(0.f, MinSep * 0.25f);
            bots[i].x = radius * std::cos(angle);
            bots[i].y = radius * std::sin(angle);
            bots[i].z = FLOOR_Z;
        }
        bots[i].yawDegrees              = RandFloat(0.f, 360.f);
        bots[i].velocityX = bots[i].velocityY = bots[i].velocityZ = 0.f;
        bots[i].isFalling               = false;
        bots[i].directionChangeTimerSeconds =
            RandFloat(DIR_CHANGE_INTERVAL_MIN, DIR_CHANGE_INTERVAL_MAX);
        bots[i].jumpTimerSeconds = RandFloat(JUMP_INTERVAL_MIN, JUMP_INTERVAL_MAX);
        bots[i].dashCooldownSeconds = 0.f;
    }
}

}

int main()
{
    srand(static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count()));

    std::vector<Carrier>  carriers(NUM_BOTS);
    std::vector<BotState> bots(NUM_BOTS);

    SpawnBotsSpread(bots);

    for (int botIndex = 0; botIndex < NUM_BOTS; ++botIndex)
    {
        if (!carriers[botIndex].Connect("127.0.0.1", 7777))
            return 1;
    }

    for (int waitIteration = 0; waitIteration < 300; ++waitIteration)
    {
        int connectedCount = 0;
        for (int botIndex = 0; botIndex < NUM_BOTS; ++botIndex)
        {
            carriers[botIndex].Pump(0.01f);
            if (carriers[botIndex].IsConnected())
                ++connectedCount;
        }
        if (connectedCount == NUM_BOTS)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    int connectedCount = 0;
    for (int botIndex = 0; botIndex < NUM_BOTS; ++botIndex)
        if (carriers[botIndex].IsConnected())
            ++connectedCount;
    if (connectedCount <= 0)
        return 1;

    for (int botIndex = 0; botIndex < NUM_BOTS; ++botIndex)
    {
        if (!carriers[botIndex].IsConnected())
            continue;
        carriers[botIndex].SendPositionUpdate(
            bots[botIndex].x,
            bots[botIndex].y,
            bots[botIndex].z,
            bots[botIndex].yawDegrees,
            0.f,
            0.f,
            0.f,
            0.f,
            false);
    }

    for (int warmupIteration = 0; warmupIteration < 12; ++warmupIteration)
    {
        for (int botIndex = 0; botIndex < NUM_BOTS; ++botIndex)
        {
            if (!carriers[botIndex].IsConnected())
                continue;
            carriers[botIndex].Pump(0.01f);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    for (int botIndex = 0; botIndex < NUM_BOTS; ++botIndex)
    {
        if (!carriers[botIndex].IsConnected())
            continue;
        carriers[botIndex].JoinRoom(static_cast<uint32_t>(ROOM_ID));
        for (int p = 0; p < 4; ++p)
            carriers[botIndex].Pump(0.01f);
        std::this_thread::sleep_for(std::chrono::milliseconds(35));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(120));

    for (int botIndex = 0; botIndex < NUM_BOTS; ++botIndex)
    {
        if (!carriers[botIndex].IsConnected())
            continue;
        carriers[botIndex].Pump(0.02f);
        carriers[botIndex].SendPositionUpdate(
            bots[botIndex].x,
            bots[botIndex].y,
            bots[botIndex].z,
            bots[botIndex].yawDegrees,
            0.f,
            0.f,
            0.f,
            0.f,
            false);
    }
    for (int flushIteration = 0; flushIteration < 20; ++flushIteration)
        for (int botIndex = 0; botIndex < NUM_BOTS; ++botIndex)
            carriers[botIndex].Pump(0.02f);

    float sendAccumulatorSeconds = 0.f;
    auto lastTime                = std::chrono::steady_clock::now();

    while (true)
    {
        const auto now = std::chrono::steady_clock::now();
        float deltaTimeSeconds = std::chrono::duration<float>(now - lastTime).count();
        lastTime               = now;
        if (deltaTimeSeconds <= 0.f || deltaTimeSeconds > 0.5f)
            deltaTimeSeconds = 0.016f;

        for (int botIndex = 0; botIndex < NUM_BOTS; ++botIndex)
        {
            carriers[botIndex].Pump(deltaTimeSeconds);
            UpdateBotSimulation(bots[botIndex], deltaTimeSeconds, &carriers[botIndex]);
        }

        sendAccumulatorSeconds += deltaTimeSeconds;
        if (sendAccumulatorSeconds >= SEND_INTERVAL)
        {
            sendAccumulatorSeconds = 0.f;
            for (int botIndex = 0; botIndex < NUM_BOTS; ++botIndex)
            {
                if (!carriers[botIndex].IsConnected())
                    continue;
                carriers[botIndex].SendPositionUpdate(
                    bots[botIndex].x,
                    bots[botIndex].y,
                    bots[botIndex].z,
                    bots[botIndex].yawDegrees,
                    0.f,
                    bots[botIndex].velocityX,
                    bots[botIndex].velocityY,
                    bots[botIndex].velocityZ,
                    bots[botIndex].isFalling);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    for (int botIndex = 0; botIndex < NUM_BOTS; ++botIndex)
        carriers[botIndex].Disconnect();
    return 0;
}
