// Source file name: Main.cpp
// Author: Igor Matiushin
// Brief description: Runs a standalone Carrier client sample that connects, joins a room, and streams movement.

#include <chrono>
#include <thread>

#include "public/Carrier.h"

int main()
{
    Carrier client;

    client.SetOnSpawnProxy([](uint32_t peerId, float x, float y, float z, float yaw, float vx, float vy, float vz, bool isFalling)
    {
        (void)peerId; (void)x; (void)y; (void)z; (void)yaw; (void)vx; (void)vy; (void)vz; (void)isFalling;
    });
    client.SetOnDespawnProxy([](uint32_t peerId) { (void)peerId; });
    client.SetOnProxyInterpolated([](uint32_t peerId, const Carrier::InterpolatedState& state) { (void)peerId; (void)state; });
    client.SetOnConnected([]() {});
    client.SetOnDisconnected([]() {});

    if (!client.Connect("127.0.0.1", 7777))
        return 1;

    for (int i = 0; i < 100; ++i)
    {
        client.Pump(0.01f);
        if (client.IsConnected()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!client.IsConnected())
        return 1;

    client.JoinRoom(42);

    float sendAccum = 0.f;
    auto lastTime = std::chrono::steady_clock::now();

    while (client.IsConnected())
    {
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;
        if (dt <= 0.f || dt > 0.5f) dt = 0.016f;

        client.Pump(dt);

        sendAccum += dt;
        if (sendAccum >= 1.f / 20.f)
        {
            sendAccum = 0.f;
            client.SendPositionUpdate(100.f, 0.f, 200.f, 45.f, 0.f, 0.f, 0.f, 0.f, false);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    client.Disconnect();
    return 0;
}
