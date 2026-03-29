// Source file name: LamePigeonSubsystem.h
// Author: Igor Matiushin
// Brief description: Declares the Unreal game-instance subsystem that exposes Carrier replication and prediction APIs.

#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "GameFramework/Character.h"

#include "LamePigeonSubsystem.generated.h"

class Carrier;
class ULamePigeonGameRpcListenerComponent;
class ULamePigeonSettings;

DECLARE_LOG_CATEGORY_EXTERN(LogLamePigeon, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRelayConnected);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRelayDisconnected);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnRpcReceived, int32, SenderPeerId, FString, FuncName, const TArray<float>&, FloatArgs);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInteractionRejected, int32, YourEventId, int32, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnProxyReplicatedFloatUpdated, int32, PeerId, FString, VarName);

UCLASS()
class LAMEPIGEON_API ULamePigeonSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
    GENERATED_BODY()

public:
    ULamePigeonSubsystem();
    ~ULamePigeonSubsystem() override;

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickable() const override { return true; }

    UFUNCTION(BlueprintCallable, Category="LamePigeon|Replication")
    bool Connect(const FString& ServerHost, int32 Port = 7777);

    UFUNCTION(BlueprintCallable, Category="LamePigeon|Replication")
    void Disconnect();

    UFUNCTION(BlueprintCallable, Category="LamePigeon|Replication")
    void JoinRoom(int32 RoomId);

    UFUNCTION(BlueprintCallable, Category="LamePigeon")
    void LeaveRoom();

    UFUNCTION(BlueprintCallable, Category="LamePigeon|Replication")
    void SendPositionUpdate(float X, float Y, float Z, float Yaw, float Pitch,
                            float VelocityX, float VelocityY, float VelocityZ,
                            bool bIsFalling);

    UFUNCTION(BlueprintCallable, Category="LamePigeon|Replication")
    void PumpCarrierOncePerFrame(float DeltaTime);

    UFUNCTION(BlueprintPure, Category="LamePigeon|Replication")
    bool IsConnected() const;

    UFUNCTION(BlueprintPure, Category="LamePigeon|Replication")
    int32 GetCurrentRoomId() const;

    UFUNCTION(BlueprintPure, Category="LamePigeon|Replication")
    float GetPingMs() const;

    UPROPERTY(BlueprintAssignable, Category="LamePigeon|Replication")
    FOnRelayConnected OnRelayConnected;

    UPROPERTY(BlueprintAssignable, Category="LamePigeon|Replication")
    FOnRelayDisconnected OnRelayDisconnected;

    UFUNCTION(BlueprintCallable, Category="LamePigeon|Replication")
    void SendReplicatedFloat(const FString& VarName, float Value);

    UFUNCTION(BlueprintCallable, Category="LamePigeon|Replication")
    void SendReplicatedBool(const FString& VarName, bool Value);

    UFUNCTION(BlueprintCallable, Category="LamePigeon|Replication")
    void SendReplicatedInt(const FString& VarName, int32 Value);

    UFUNCTION(BlueprintPure, Category="LamePigeon|Replication")
    float GetProxyFloat(int32 PeerId, const FString& VarName, float Default = 0.f) const;

    UFUNCTION(BlueprintPure, Category="LamePigeon|Replication")
    bool GetProxyBool(int32 PeerId, const FString& VarName, bool Default = false) const;

    UFUNCTION(BlueprintPure, Category="LamePigeon|Replication")
    int32 GetProxyInt(int32 PeerId, const FString& VarName, int32 Default = 0) const;

    UPROPERTY(BlueprintAssignable, Category="LamePigeon|RPC")
    FOnRpcReceived OnRpcReceived;

    UFUNCTION(BlueprintCallable, Category="LamePigeon|RPC")
    void BroadcastRPC(const FString& FuncName, const TArray<float>& FloatArgs);

    UFUNCTION(BlueprintCallable, Category="LamePigeon|RPC")
    void SendRPCToPeer(int32 TargetPeerId, const FString& FuncName, const TArray<float>& FloatArgs);

    UFUNCTION(BlueprintCallable, Category="LamePigeon|Interaction")
    void SendInteractionPredict(int32 EventId, int32 OtherPeerId, float DeltaToContact,
                                const FString& MyTag, const FString& TheirTag);

    UPROPERTY(BlueprintAssignable, Category="LamePigeon|Interaction")
    FOnInteractionRejected OnInteractionRejected;

    UPROPERTY(BlueprintAssignable, Category="LamePigeon|Replication")
    FOnProxyReplicatedFloatUpdated OnProxyReplicatedFloatUpdated;

    void RegisterGameplayRpcComponent(ULamePigeonGameRpcListenerComponent* Component);
    void UnregisterGameplayRpcComponent(ULamePigeonGameRpcListenerComponent* Component);

    UFUNCTION(BlueprintCallable, Category="LamePigeon|RPC")
    void RegisterGameRpcHandler(UObject* Handler);

    UFUNCTION(BlueprintCallable, Category="LamePigeon|RPC")
    void UnregisterGameRpcHandler(UObject* Handler);

    UFUNCTION(BlueprintPure, Category="LamePigeon|Replication")
    bool TryGetPeerIdForProxyActor(ACharacter* ProxyCharacter, int32& OutPeerId) const;

    UFUNCTION(BlueprintPure, Category="LamePigeon|Replication")
    TArray<int32> GetProxyPeerIds() const;

    UFUNCTION(BlueprintPure, Category="LamePigeon|Replication")
    ACharacter* GetProxyCharacter(int32 PeerId) const;

    UFUNCTION(BlueprintPure, Category="LamePigeon|Replication")
    bool TryGetProxyInterpolatedKinematics(int32 PeerId, FVector& OutLocation, FVector& OutVelocity, float& OutYaw) const;

    UFUNCTION(BlueprintCallable, Category="LamePigeon|Replication", meta=(AdvancedDisplay="PredictEventId"))
    void NotifyVictimPredictedKnockbackContactAlign(int32 DasherPeerId, ACharacter* VictimCharacter,
                                                    const FVector& KnockbackHorizontalDirectionAwayFromDasher,
                                                    const FVector& InterpolatedDasherLocation,
                                                    const FVector& InterpolatedDasherVelocityXY, int32 PredictEventId = 0);

    UFUNCTION(BlueprintCallable, Category="LamePigeon|Replication", meta=(AdvancedDisplay="StartDelaySeconds,PredictEventId"))
    void NotifyPredictedProxyKnockback(int32 PeerId, FVector LaunchVelocity, float StartDelaySeconds = 0.f,
                                       int32 PredictEventId = 0);

    UFUNCTION(BlueprintCallable, Category="LamePigeon|Replication", meta=(AdvancedDisplay="PredictEventId"))
    void CancelPredictedProxyKnockback(int32 PeerId, int32 PredictEventId = 0);

private:
    void DispatchGameplayRpcListeners(int32 SenderPeerId, const FString& FuncName, const TArray<float>& Args);

    void SendLocalPlayerPosition();
    void EnqueueProxySpawn(int32 PeerId, float X, float Y, float Z, float Yaw, float Vx, float Vy, float Vz, bool bIsFalling);
    void ProcessPendingProxySpawns();
    void SpawnProxyActorNow(int32 PeerId, float X, float Y, float Z, float Yaw, float Vx, float Vy, float Vz, bool bIsFalling);
    void DespawnProxyActor(int32 PeerId);
    void ApplyInterpolatedStateToActor(int32 PeerId, ACharacter* ProxyCharacter, float X, float Y, float Z, float Yaw,
                                       float Pitch, float SmoothedVx, float SmoothedVy, float SmoothedVz, bool bIsFalling);
    void ApplyProxyVisualPrediction(int32 PeerId, ACharacter* ProxyCharacter, const FVector& InterpolatedLocation,
                                    const ULamePigeonSettings* Settings, FVector& InOutLocation);
    void ApplyContactSnapVisualPrediction(int32 PeerId, ACharacter* ProxyCharacter, const FVector& InterpolatedLocation,
                                          const ULamePigeonSettings* Settings, FVector& InOutLocation,
                                          bool& bOutHandledPrediction);
    void ApplyKnockbackVisualPrediction(int32 PeerId, ACharacter* ProxyCharacter, const FVector& InterpolatedLocation,
                                        const ULamePigeonSettings* Settings, FVector& InOutLocation,
                                        bool& bOutHandledPrediction);
    void UpdateProxyMovementState(ACharacter* ProxyCharacter, float SmoothedVx, float SmoothedVy, float SmoothedVz,
                                  bool bIsFalling);
    void UpdateProxyAnimationState(int32 PeerId, ACharacter* ProxyCharacter, const ULamePigeonSettings* Settings,
                                   float Yaw, float SmoothedVx, float SmoothedVy, float SmoothedVz, bool bIsFalling);

    void SyncCarrierInterpolationFromSettings();

    Carrier* CarrierInstance = nullptr;
    UPROPERTY()
    TMap<int32, ACharacter*> ProxyActors;
    float SendAccumulator = 0.f;
    uint32 LastCarrierPumpWorldFrameCount = MAX_uint32;

    struct FPendingProxySpawn
    {
        int32 PeerId = 0;
        float X = 0.f, Y = 0.f, Z = 0.f, Yaw = 0.f;
        float Vx = 0.f, Vy = 0.f, Vz = 0.f;
        bool bIsFalling = false;
    };
    TArray<FPendingProxySpawn> PendingProxySpawns;

    TArray<TWeakObjectPtr<ULamePigeonGameRpcListenerComponent>> GameplayRpcComponents;
    TArray<TWeakObjectPtr<UObject>>                             GameplayRpcHandlerObjects;

    uint32 ProxyAnimThrottleFrameCounter = 0;

    struct FProxyKnockbackContactVisualSnap
    {
        FVector SnapLocation           = FVector::ZeroVector;
        float   StartWorldTimeSeconds  = 0.f;
        float   MaxHoldSeconds         = 3.f;
        uint16  ConsecutiveCloseFrames = 0;
        uint32  PredictEventId         = 0;
        bool    bBlendOutActive        = false;
        float   BlendOutStartWorldSeconds = 0.f;
        FVector BlendOutFixedStartLocation = FVector::ZeroVector;
    };

    TMap<int32, FProxyKnockbackContactVisualSnap> KnockbackContactProxySnaps;

    struct FPredictedProxyKnockbackVisual
    {
        FVector LaunchVelocityCentimetersPerSecond = FVector::ZeroVector;
        float   StartWorldTimeSeconds = 0.f;
        uint32  PredictEventId = 0;
        FVector StartInterpolatedLocation = FVector::ZeroVector;
        bool    bHasStartInterpolatedLocation = false;
        FVector BlendStartResidual = FVector::ZeroVector;
        bool    bCapturedBlendStartResidual = false;
    };

    TMap<int32, FPredictedProxyKnockbackVisual> PredictedProxyKnockbacks;
};
