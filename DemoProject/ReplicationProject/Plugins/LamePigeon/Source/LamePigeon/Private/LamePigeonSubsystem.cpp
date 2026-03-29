// Source file name: LamePigeonSubsystem.cpp
// Author: Igor Matiushin
// Brief description: Implements the Unreal subsystem wrapper around Carrier, proxy sync, and prediction visuals.

#include "LamePigeonSubsystem.h"
#include "Carrier.h"
#include "LamePigeonGameRpcHandler.h"
#include "LamePigeonGameRpcListenerComponent.h"
#include "LamePigeonSettings.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/UnrealType.h"
#include "CoreGlobals.h"

DEFINE_LOG_CATEGORY(LogLamePigeon);

namespace
{
static FVector ComputePredictedProxyBallisticDisplacement(const FVector& LaunchVelocityCentimetersPerSecond,
                                                          float ElapsedSeconds, float BallisticSeconds, float GravityZ,
                                                          float HorizontalDecelerationCentimetersPerSecondSquared)
{
    if (ElapsedSeconds <= 0.f)
        return FVector::ZeroVector;

    const float MaxBallisticSeconds = FMath::Max(BallisticSeconds, KINDA_SMALL_NUMBER);
    const float ClampedElapsedSeconds = FMath::Min(ElapsedSeconds, MaxBallisticSeconds);
    const float VerticalDisplacement =
        LaunchVelocityCentimetersPerSecond.Z * ClampedElapsedSeconds
        + 0.5f * GravityZ * ClampedElapsedSeconds * ClampedElapsedSeconds;

    const FVector2D HorizontalVelocity(LaunchVelocityCentimetersPerSecond.X, LaunchVelocityCentimetersPerSecond.Y);
    const float HorizontalSpeed = HorizontalVelocity.Size();
    if (HorizontalSpeed < KINDA_SMALL_NUMBER || HorizontalDecelerationCentimetersPerSecondSquared <= 0.f)
    {
        return FVector(LaunchVelocityCentimetersPerSecond.X * ClampedElapsedSeconds,
                       LaunchVelocityCentimetersPerSecond.Y * ClampedElapsedSeconds, VerticalDisplacement);
    }

    const float StopTimeSeconds = HorizontalSpeed / HorizontalDecelerationCentimetersPerSecondSquared;
    const float HorizontalTimeSeconds = FMath::Min(ClampedElapsedSeconds, StopTimeSeconds);
    const float HorizontalDistance =
        HorizontalSpeed * HorizontalTimeSeconds
        - 0.5f * HorizontalDecelerationCentimetersPerSecondSquared * HorizontalTimeSeconds * HorizontalTimeSeconds;
    const FVector2D HorizontalDirection = HorizontalVelocity / HorizontalSpeed;
    return FVector(HorizontalDirection.X * HorizontalDistance, HorizontalDirection.Y * HorizontalDistance,
                   VerticalDisplacement);
}

static FVector ComputeSafePredictedProxyResidual(const FVector& PredictedDisplacement, const FVector& StreamDisplacement)
{
    FVector Residual = FVector::ZeroVector;
    for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
    {
        const float PredictedAxisValue = PredictedDisplacement.Component(AxisIndex);
        const float StreamAxisValue = StreamDisplacement.Component(AxisIndex);
        if (FMath::Abs(StreamAxisValue) < 1.f)
        {
            Residual.Component(AxisIndex) = PredictedAxisValue;
            continue;
        }

        const float RemainingLead = PredictedAxisValue - StreamAxisValue;
        Residual.Component(AxisIndex) =
            (PredictedAxisValue * StreamAxisValue > 0.f && RemainingLead * PredictedAxisValue > 0.f) ? RemainingLead : 0.f;
    }
    return Residual;
}
}

static void SetAnimBlueprintVariable(UAnimInstance* AnimInstance, const TCHAR* VariableName, double Value)
{
    if (FDoubleProperty* Property = CastField<FDoubleProperty>(AnimInstance->GetClass()->FindPropertyByName(VariableName)))
        Property->SetPropertyValue_InContainer(AnimInstance, Value);
}

static void SetAnimBlueprintVariable(UAnimInstance* AnimInstance, const TCHAR* VariableName, bool Value)
{
    if (FBoolProperty* Property = CastField<FBoolProperty>(AnimInstance->GetClass()->FindPropertyByName(VariableName)))
        Property->SetPropertyValue_InContainer(AnimInstance, Value);
}

static void SetProxyAcceleration(UCharacterMovementComponent* Mov, const FVector& Accel)
{
    if (!Mov)
        return;
    if (FStructProperty* Prop = CastField<FStructProperty>(Mov->GetClass()->FindPropertyByName(TEXT("Acceleration"))))
    {
        if (void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Mov))
        {
            *static_cast<FVector*>(ValuePtr) = Accel;
        }
    }
}

ULamePigeonSubsystem::ULamePigeonSubsystem() = default;

ULamePigeonSubsystem::~ULamePigeonSubsystem()
{
    if (CarrierInstance)
    {
        CarrierInstance->Disconnect();
        delete CarrierInstance;
        CarrierInstance = nullptr;
    }
}

void ULamePigeonSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    CarrierInstance = new Carrier();

    SyncCarrierInterpolationFromSettings();

    CarrierInstance->SetOnSpawnProxy([this](uint32_t peerId, float x, float y, float z, float yaw, float vx, float vy, float vz, bool isFalling)
    {
        EnqueueProxySpawn(static_cast<int32>(peerId), x, y, z, yaw, vx, vy, vz, isFalling);
    });
    CarrierInstance->SetOnDespawnProxy([this](uint32_t peerId)
    {
        DespawnProxyActor(static_cast<int32>(peerId));
    });
    CarrierInstance->SetOnProxyInterpolated([this](uint32_t peerId, const Carrier::InterpolatedState& state)
    {
        const int32 Id = static_cast<int32>(peerId);
        ACharacter** Found = ProxyActors.Find(Id);
        if (Found && IsValid(*Found))
            ApplyInterpolatedStateToActor(Id, *Found, state.x, state.y, state.z, state.yaw, state.pitch,
                                          state.smoothedVx, state.smoothedVy, state.smoothedVz, state.isFalling);
    });
    CarrierInstance->SetOnRpcReceived([this](uint32_t senderPeerId, const std::string& funcName, const std::vector<float>& floatArgs)
    {
        TArray<float> Args;
        for (float v : floatArgs) Args.Add(v);
        const FString Name(UTF8_TO_TCHAR(funcName.c_str()));
        const int32   Sender = static_cast<int32>(senderPeerId);
        OnRpcReceived.Broadcast(Sender, Name, Args);
        DispatchGameplayRpcListeners(Sender, Name, Args);
    });
    CarrierInstance->SetOnConnected([this]() { OnRelayConnected.Broadcast(); });
    CarrierInstance->SetOnDisconnected([this]() { OnRelayDisconnected.Broadcast(); });
    CarrierInstance->SetOnInteractionRejected([this](uint32_t YourEventId, uint8_t Reason) {
        OnInteractionRejected.Broadcast(static_cast<int32>(YourEventId), static_cast<int32>(Reason));
    });
    CarrierInstance->SetOnProxyFloatVarUpdated([this](uint32_t PeerId, const std::string& VarName) {
        OnProxyReplicatedFloatUpdated.Broadcast(static_cast<int32>(PeerId), FString(UTF8_TO_TCHAR(VarName.c_str())));
    });
}

void ULamePigeonSubsystem::Deinitialize()
{
    Disconnect();
    GameplayRpcComponents.Empty();
    GameplayRpcHandlerObjects.Empty();
    if (CarrierInstance)
    {
        delete CarrierInstance;
        CarrierInstance = nullptr;
    }
    Super::Deinitialize();
}

void ULamePigeonSubsystem::RegisterGameplayRpcComponent(ULamePigeonGameRpcListenerComponent* Component)
{
    if (!Component)
        return;
    GameplayRpcComponents.RemoveAll(
        [Component](const TWeakObjectPtr<ULamePigeonGameRpcListenerComponent>& P) { return P.Get() == Component; });
    GameplayRpcComponents.Add(Component);
}

void ULamePigeonSubsystem::UnregisterGameplayRpcComponent(ULamePigeonGameRpcListenerComponent* Component)
{
    if (!Component)
        return;
    GameplayRpcComponents.RemoveAll(
        [Component](const TWeakObjectPtr<ULamePigeonGameRpcListenerComponent>& P) { return P.Get() == Component; });
}

void ULamePigeonSubsystem::RegisterGameRpcHandler(UObject* Handler)
{
    if (!Handler || !Handler->GetClass()->ImplementsInterface(ULamePigeonGameRpcHandler::StaticClass()))
        return;
    UnregisterGameRpcHandler(Handler);
    GameplayRpcHandlerObjects.Add(Handler);
}

void ULamePigeonSubsystem::UnregisterGameRpcHandler(UObject* Handler)
{
    if (!Handler)
        return;
    GameplayRpcHandlerObjects.RemoveAll(
        [Handler](const TWeakObjectPtr<UObject>& P) { return P.Get() == Handler; });
}

void ULamePigeonSubsystem::DispatchGameplayRpcListeners(int32 SenderPeerId, const FString& FuncName,
                                                        const TArray<float>& Args)
{
    GameplayRpcComponents.RemoveAll([](const TWeakObjectPtr<ULamePigeonGameRpcListenerComponent>& P) { return !P.IsValid(); });
    for (TWeakObjectPtr<ULamePigeonGameRpcListenerComponent>& Weak : GameplayRpcComponents)
    {
        if (ULamePigeonGameRpcListenerComponent* C = Weak.Get())
            C->ReceiveGameRpc(SenderPeerId, FuncName, Args);
    }

    GameplayRpcHandlerObjects.RemoveAll([](const TWeakObjectPtr<UObject>& P) { return !P.IsValid(); });
    for (TWeakObjectPtr<UObject>& Weak : GameplayRpcHandlerObjects)
    {
        if (UObject* O = Weak.Get())
        {
            if (O->GetClass()->ImplementsInterface(ULamePigeonGameRpcHandler::StaticClass()))
                ILamePigeonGameRpcHandler::Execute_HandleLamePigeonGameRpc(O, SenderPeerId, FuncName, Args);
        }
    }
}

bool ULamePigeonSubsystem::Connect(const FString& ServerHost, int32 Port)
{
    if (!CarrierInstance) return false;
    return CarrierInstance->Connect(TCHAR_TO_ANSI(*ServerHost), static_cast<uint16_t>(Port));
}

void ULamePigeonSubsystem::Disconnect()
{
    if (CarrierInstance)
        CarrierInstance->Disconnect();
    for (auto& Pair : ProxyActors)
        if (IsValid(Pair.Value)) Pair.Value->Destroy();
    ProxyActors.Empty();
    PendingProxySpawns.Reset();
    KnockbackContactProxySnaps.Reset();
    PredictedProxyKnockbacks.Reset();
}

void ULamePigeonSubsystem::JoinRoom(int32 RoomId)
{
    if (!CarrierInstance) return;
    const ULamePigeonSettings* Settings = GetDefault<ULamePigeonSettings>();
    if (RoomId == 0)
    {
        RoomId = Settings->DefaultRoomId;
    }
    if (!CarrierInstance->IsConnected())
    {
        if (!Connect(Settings->ServerAddress, Settings->ServerPort))
            return;
        for (int32 i = 0; i < 50; ++i)
        {
            CarrierInstance->Pump(0.01f);
            if (CarrierInstance->IsConnected()) break;
        }
        if (!CarrierInstance->IsConnected())
            return;
    }
    if (CarrierInstance->GetCurrentRoomId() >= 0) LeaveRoom();
    CarrierInstance->JoinRoom(static_cast<uint32_t>(RoomId));
}

void ULamePigeonSubsystem::LeaveRoom()
{
    if (!CarrierInstance) return;
    for (auto& Pair : ProxyActors)
        if (IsValid(Pair.Value)) Pair.Value->Destroy();
    ProxyActors.Empty();
    PendingProxySpawns.Reset();
    KnockbackContactProxySnaps.Reset();
    PredictedProxyKnockbacks.Reset();
    CarrierInstance->LeaveRoom();
}

void ULamePigeonSubsystem::PumpCarrierOncePerFrame(float DeltaTime)
{
    if (!CarrierInstance)
        return;
    const uint32 Frame = GFrameCounter;
    if (Frame == LastCarrierPumpWorldFrameCount)
        return;
    LastCarrierPumpWorldFrameCount = Frame;
    CarrierInstance->Pump(DeltaTime);
}

void ULamePigeonSubsystem::Tick(float DeltaTime)
{
    if (!CarrierInstance) return;
    SyncCarrierInterpolationFromSettings();
    PumpCarrierOncePerFrame(DeltaTime);

    if (!CarrierInstance->IsConnected() || CarrierInstance->GetCurrentRoomId() < 0) return;

    ProcessPendingProxySpawns();

    const ULamePigeonSettings* Settings = GetDefault<ULamePigeonSettings>();

    SendAccumulator += DeltaTime;
    if (SendAccumulator >= 1.f / FMath::Max(1, Settings->SendRateHz))
    {
        SendAccumulator = 0.f;
        SendLocalPlayerPosition();
    }
}

TStatId ULamePigeonSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(ULamePigeonSubsystem, STATGROUP_Tickables);
}

void ULamePigeonSubsystem::SendLocalPlayerPosition()
{
    if (!GetWorld()) return;
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC) return;
    ACharacter* Ch = Cast<ACharacter>(PC->GetPawn());
    if (!Ch) return;

    FVector Loc = Ch->GetActorLocation();
    FRotator Rot = Ch->GetActorRotation();
    FVector Vel = Ch->GetVelocity();
    bool bFalling = false;
    if (UCharacterMovementComponent* Mov = Ch->GetCharacterMovement())
        bFalling = Mov->IsFalling();

    if (CarrierInstance)
        CarrierInstance->SendPositionUpdate(Loc.X, Loc.Y, Loc.Z, Rot.Yaw, Rot.Pitch,
                                           Vel.X, Vel.Y, Vel.Z, bFalling);
}

void ULamePigeonSubsystem::SendPositionUpdate(float X, float Y, float Z, float Yaw, float Pitch,
                                              float VelocityX, float VelocityY, float VelocityZ, bool bIsFalling)
{
    if (CarrierInstance)
        CarrierInstance->SendPositionUpdate(X, Y, Z, Yaw, Pitch, VelocityX, VelocityY, VelocityZ, bIsFalling);
}

bool ULamePigeonSubsystem::IsConnected() const
{
    return CarrierInstance && CarrierInstance->IsConnected();
}

int32 ULamePigeonSubsystem::GetCurrentRoomId() const
{
    return CarrierInstance ? static_cast<int32>(CarrierInstance->GetCurrentRoomId()) : -1;
}

float ULamePigeonSubsystem::GetPingMs() const
{
    return CarrierInstance ? CarrierInstance->GetPingMs() : 0.f;
}

void ULamePigeonSubsystem::SendReplicatedFloat(const FString& VarName, float Value)
{
    if (CarrierInstance)
        CarrierInstance->SendReplicatedFloat(std::string(TCHAR_TO_UTF8(*VarName)), Value);
}

void ULamePigeonSubsystem::SendReplicatedBool(const FString& VarName, bool Value)
{
    if (CarrierInstance)
        CarrierInstance->SendReplicatedBool(std::string(TCHAR_TO_UTF8(*VarName)), Value);
}

void ULamePigeonSubsystem::SendReplicatedInt(const FString& VarName, int32 Value)
{
    if (CarrierInstance)
        CarrierInstance->SendReplicatedInt(std::string(TCHAR_TO_UTF8(*VarName)), Value);
}

float ULamePigeonSubsystem::GetProxyFloat(int32 PeerId, const FString& VarName, float Default) const
{
    return CarrierInstance ? CarrierInstance->GetProxyFloat(static_cast<uint32_t>(PeerId), std::string(TCHAR_TO_UTF8(*VarName)), Default) : Default;
}

bool ULamePigeonSubsystem::GetProxyBool(int32 PeerId, const FString& VarName, bool Default) const
{
    return CarrierInstance ? CarrierInstance->GetProxyBool(static_cast<uint32_t>(PeerId), std::string(TCHAR_TO_UTF8(*VarName)), Default) : Default;
}

int32 ULamePigeonSubsystem::GetProxyInt(int32 PeerId, const FString& VarName, int32 Default) const
{
    return CarrierInstance ? CarrierInstance->GetProxyInt(static_cast<uint32_t>(PeerId), std::string(TCHAR_TO_UTF8(*VarName)), Default) : Default;
}

void ULamePigeonSubsystem::BroadcastRPC(const FString& FuncName, const TArray<float>& FloatArgs)
{
    if (!CarrierInstance) return;
    std::vector<float> args;
    for (float v : FloatArgs) args.push_back(v);
    CarrierInstance->BroadcastRPC(std::string(TCHAR_TO_UTF8(*FuncName)), args);
}

void ULamePigeonSubsystem::SendRPCToPeer(int32 TargetPeerId, const FString& FuncName, const TArray<float>& FloatArgs)
{
    if (!CarrierInstance) return;
    std::vector<float> args;
    for (float v : FloatArgs) args.push_back(v);
    CarrierInstance->SendRPCToPeer(static_cast<uint32_t>(TargetPeerId), std::string(TCHAR_TO_UTF8(*FuncName)), args);
}

void ULamePigeonSubsystem::SendInteractionPredict(int32 EventId, int32 OtherPeerId, float DeltaToContact,
                                                  const FString& MyTag, const FString& TheirTag)
{
    if (!CarrierInstance)
        return;
    CarrierInstance->SendInteractionPredict(static_cast<uint32_t>(EventId), static_cast<uint32_t>(OtherPeerId), DeltaToContact,
                                            std::string(TCHAR_TO_UTF8(*MyTag)), std::string(TCHAR_TO_UTF8(*TheirTag)));
}

bool ULamePigeonSubsystem::TryGetProxyInterpolatedKinematics(int32 PeerId, FVector& OutLocation, FVector& OutVelocity,
                                                             float& OutYaw) const
{
    if (!CarrierInstance)
        return false;
    Carrier::InterpolatedState St;
    if (!CarrierInstance->GetInterpolatedState(static_cast<uint32_t>(PeerId), St))
        return false;
    OutLocation = FVector(St.x, St.y, St.z);
    OutVelocity = FVector(St.vx, St.vy, St.vz);
    OutYaw      = St.yaw;
    return true;
}

bool ULamePigeonSubsystem::TryGetPeerIdForProxyActor(ACharacter* ProxyCharacter, int32& OutPeerId) const
{
    if (!ProxyCharacter)
        return false;
    for (const TPair<int32, ACharacter*>& Pair : ProxyActors)
    {
        if (Pair.Value == ProxyCharacter)
        {
            OutPeerId = Pair.Key;
            return true;
        }
    }
    return false;
}

TArray<int32> ULamePigeonSubsystem::GetProxyPeerIds() const
{
    TArray<int32> Result;
    ProxyActors.GetKeys(Result);
    return Result;
}

ACharacter* ULamePigeonSubsystem::GetProxyCharacter(int32 PeerId) const
{
    for (const TPair<int32, ACharacter*>& Pair : ProxyActors)
        if (Pair.Key == PeerId)
            return Pair.Value;
    return nullptr;
}

void ULamePigeonSubsystem::EnqueueProxySpawn(int32 PeerId, float X, float Y, float Z, float Yaw,
                                             float Vx, float Vy, float Vz, bool bIsFalling)
{
    if (ProxyActors.Contains(PeerId))
        return;
    PendingProxySpawns.RemoveAll([PeerId](const FPendingProxySpawn& E) { return E.PeerId == PeerId; });
    FPendingProxySpawn Entry;
    Entry.PeerId     = PeerId;
    Entry.X = X;
    Entry.Y = Y;
    Entry.Z = Z;
    Entry.Yaw        = Yaw;
    Entry.Vx = Vx;
    Entry.Vy = Vy;
    Entry.Vz = Vz;
    Entry.bIsFalling = bIsFalling;
    PendingProxySpawns.Add(Entry);
}

void ULamePigeonSubsystem::ProcessPendingProxySpawns()
{
    if (!GetWorld() || PendingProxySpawns.Num() == 0)
        return;

    const ULamePigeonSettings* Settings = GetDefault<ULamePigeonSettings>();
    const int32                Budget   = FMath::Max(1, Settings->MaxProxySpawnsPerFrame);
    int32                        Used   = 0;
    while (PendingProxySpawns.Num() > 0 && Used < Budget)
    {
        const FPendingProxySpawn Next = PendingProxySpawns[0];
        PendingProxySpawns.RemoveAt(0);
        if (ProxyActors.Contains(Next.PeerId))
            continue;
        SpawnProxyActorNow(Next.PeerId, Next.X, Next.Y, Next.Z, Next.Yaw, Next.Vx, Next.Vy, Next.Vz, Next.bIsFalling);
        ++Used;
    }
}

void ULamePigeonSubsystem::SpawnProxyActorNow(int32 PeerId, float X, float Y, float Z, float Yaw,
                                              float Vx, float Vy, float Vz, bool bIsFalling)
{
    if (ProxyActors.Contains(PeerId)) return;

    const ULamePigeonSettings* Settings = GetDefault<ULamePigeonSettings>();
    TSubclassOf<ACharacter> CharacterClass = Settings->ProxyCharacterClass.LoadSynchronous();
    if (!CharacterClass)
        return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ACharacter* Ch = GetWorld()->SpawnActor<ACharacter>(
        CharacterClass, FTransform(FRotator(0.f, Yaw, 0.f), FVector(X, Y, Z)), SpawnParams);
    if (!Ch) return;

    if (UCharacterMovementComponent* Mov = Ch->GetCharacterMovement())
    {
        Mov->SetComponentTickEnabled(false);
        Mov->DisableMovement();
    }

    if (USkeletalMeshComponent* Mesh = Ch->GetMesh())
    {
        const ULamePigeonSettings* LamePigeonSettings = GetDefault<ULamePigeonSettings>();
        Mesh->VisibilityBasedAnimTickOption       = LamePigeonSettings ? LamePigeonSettings->ProxyVisibilityBasedAnimTick
                                                             : EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
        Mesh->bEnableUpdateRateOptimizations =
            LamePigeonSettings ? LamePigeonSettings->bProxyMeshEnableUpdateRateOptimizations : true;
        if (UAnimInstance* Anim = Mesh->GetAnimInstance())
        {
            Anim->InitializeAnimation();
            if (LamePigeonSettings && LamePigeonSettings->ProxyAnimDriver == ELamePigeonProxyAnimDriver::AnimBlueprintVariables)
            {
                if (FObjectProperty* Prop = CastField<FObjectProperty>(Anim->GetClass()->FindPropertyByName(TEXT("Character"))))
                    Prop->SetObjectPropertyValue_InContainer(Anim, nullptr);
            }
        }
    }

    ProxyActors.Add(PeerId, Ch);
}

void ULamePigeonSubsystem::DespawnProxyActor(int32 PeerId)
{
    PendingProxySpawns.RemoveAll([PeerId](const FPendingProxySpawn& E) { return E.PeerId == PeerId; });
    KnockbackContactProxySnaps.Remove(PeerId);
    PredictedProxyKnockbacks.Remove(PeerId);
    ACharacter** Found = ProxyActors.Find(PeerId);
    if (Found && IsValid(*Found)) (*Found)->Destroy();
    ProxyActors.Remove(PeerId);
}

void ULamePigeonSubsystem::NotifyVictimPredictedKnockbackContactAlign(int32 DasherPeerId, ACharacter* VictimCharacter,
    const FVector& KnockbackHorizontalDirectionAwayFromDasher, const FVector& InterpolatedDasherLocation,
    const FVector& InterpolatedDasherVelocityXY, int32 PredictEventId)
{
    (void)InterpolatedDasherVelocityXY;

    if (DasherPeerId < 0 || !IsValid(VictimCharacter))
        return;

    ACharacter* Proxy = GetProxyCharacter(DasherPeerId);
    if (!IsValid(Proxy))
        return;

    FVector Dir2D(KnockbackHorizontalDirectionAwayFromDasher.X, KnockbackHorizontalDirectionAwayFromDasher.Y, 0.f);
    if (!Dir2D.Normalize())
        return;

    UCapsuleComponent* const VCap = VictimCharacter->GetCapsuleComponent();
    UCapsuleComponent* const DCap = Proxy->GetCapsuleComponent();
    const float VictimR = VCap ? VCap->GetScaledCapsuleRadius() : 42.f;
    const float DasherR = DCap ? DCap->GetScaledCapsuleRadius() : 42.f;

    const ULamePigeonSettings* const Settings = GetDefault<ULamePigeonSettings>();
    const float Gap = Settings ? Settings->PredictedContactProxyGapCm : 5.f;
    const float Separ = VictimR + DasherR + Gap;

    const FVector VLoc = VictimCharacter->GetActorLocation();
    FVector Snap = VLoc - Dir2D * Separ;
    Snap.Z = InterpolatedDasherLocation.Z;

    FProxyKnockbackContactVisualSnap& Entry = KnockbackContactProxySnaps.FindOrAdd(DasherPeerId);
    Entry.SnapLocation              = Snap;
    Entry.ConsecutiveCloseFrames      = 0;
    Entry.bBlendOutActive             = false;
    Entry.BlendOutStartWorldSeconds   = 0.f;
    Entry.BlendOutFixedStartLocation  = FVector::ZeroVector;
    Entry.PredictEventId =
        PredictEventId >= 0 ? static_cast<uint32>(PredictEventId) : 0u;
    UWorld* const World = VictimCharacter->GetWorld();
    Entry.StartWorldTimeSeconds = World ? World->GetTimeSeconds() : 0.f;
    Entry.MaxHoldSeconds        = Settings ? Settings->PredictedContactProxyHoldMaxSeconds : 3.f;
}

void ULamePigeonSubsystem::NotifyPredictedProxyKnockback(int32 PeerId, FVector LaunchVelocity, float StartDelaySeconds,
                                                         int32 PredictEventId)
{
    if (PeerId < 0 || LaunchVelocity.IsNearlyZero(1.f) || !CarrierInstance)
        return;

    UWorld* const World = GetWorld();
    const float NowSeconds = World ? World->GetTimeSeconds() : 0.f;

    Carrier::InterpolatedState InterpolatedState;
    const bool bHasInterpolatedState = CarrierInstance->GetInterpolatedState(static_cast<uint32_t>(PeerId), InterpolatedState);
    ACharacter* const ProxyCharacter = GetProxyCharacter(PeerId);
    const FVector InterpolatedLocation =
        bHasInterpolatedState ? FVector(InterpolatedState.x, InterpolatedState.y, InterpolatedState.z) : FVector::ZeroVector;
    const FVector ProxyLocation = IsValid(ProxyCharacter) ? ProxyCharacter->GetActorLocation() : FVector::ZeroVector;
    const bool bHasUsableInterpolatedLocation = bHasInterpolatedState && !InterpolatedLocation.IsNearlyZero(50.f);
    const bool bHasUsableProxyLocation = IsValid(ProxyCharacter) && !ProxyLocation.IsNearlyZero(50.f);
    if (!bHasUsableInterpolatedLocation && !bHasUsableProxyLocation)
        return;

    FPredictedProxyKnockbackVisual& PredictedVisual = PredictedProxyKnockbacks.FindOrAdd(PeerId);
    PredictedVisual.LaunchVelocityCentimetersPerSecond = LaunchVelocity;
    PredictedVisual.StartWorldTimeSeconds = NowSeconds + FMath::Max(0.f, StartDelaySeconds);
    PredictedVisual.PredictEventId = PredictEventId >= 0 ? static_cast<uint32>(PredictEventId) : 0u;
    PredictedVisual.StartInterpolatedLocation =
        bHasUsableInterpolatedLocation ? InterpolatedLocation : ProxyLocation;
    PredictedVisual.bHasStartInterpolatedLocation = true;
    PredictedVisual.BlendStartResidual = FVector::ZeroVector;
    PredictedVisual.bCapturedBlendStartResidual = false;

    if (ProxyCharacter && bHasInterpolatedState)
    {
        ApplyInterpolatedStateToActor(PeerId, ProxyCharacter, InterpolatedState.x, InterpolatedState.y, InterpolatedState.z,
                                      InterpolatedState.yaw, InterpolatedState.pitch, InterpolatedState.smoothedVx,
                                      InterpolatedState.smoothedVy, InterpolatedState.smoothedVz,
                                      InterpolatedState.isFalling);
    }
}

void ULamePigeonSubsystem::CancelPredictedProxyKnockback(int32 PeerId, int32 PredictEventId)
{
    FPredictedProxyKnockbackVisual* PredictedVisual = PredictedProxyKnockbacks.Find(PeerId);
    if (!PredictedVisual)
        return;

    if (PredictEventId > 0 && PredictedVisual->PredictEventId != static_cast<uint32>(PredictEventId))
        return;

    PredictedProxyKnockbacks.Remove(PeerId);
}

void ULamePigeonSubsystem::SyncCarrierInterpolationFromSettings()
{
    if (!CarrierInstance)
        return;

    const ULamePigeonSettings* Settings = GetDefault<ULamePigeonSettings>();
    float                       Delay    = 0.1f;
    float                       MaxExt   = 0.25f;
    float                       SmoothHz = 10.f;

    if (Settings)
    {
        Delay    = Settings->InterpolationDelaySeconds;
        MaxExt   = Settings->MaxExtrapolationSeconds;
        SmoothHz = Settings->ProxyVelocitySmoothBaseHz;

        if (Settings->bScaleInterpolationTuningByFrequencies)
        {
            const float RefS = FMath::Max(1.f, Settings->InterpolationReferenceSendRateHz);
            const float RefP = FMath::Max(1.f, Settings->InterpolationReferencePredictScanHz);
            const float Send = FMath::Max(1.f, static_cast<float>(Settings->SendRateHz));
            const float Pred = FMath::Max(1.f, Settings->InteractionPredictScanHz);
            const float Scale = (RefS / Send) * (RefP / Pred);
            Delay *= Scale;
            MaxExt *= Scale;
            SmoothHz = Settings->ProxyVelocitySmoothBaseHz * (Send / RefS) * (Pred / RefP);
        }
    }

    CarrierInstance->SetInterpolationDelay(FMath::Clamp(Delay, 0.02f, 1.0f));
    CarrierInstance->SetMaxExtrapolationTime(FMath::Clamp(MaxExt, 0.05f, 2.0f));
    CarrierInstance->SetProxyVelocitySmoothHz(FMath::Clamp(SmoothHz, 2.f, 50.f));
}

void ULamePigeonSubsystem::ApplyProxyVisualPrediction(int32 PeerId, ACharacter* ProxyCharacter,
    const FVector& InterpolatedLocation, const ULamePigeonSettings* Settings, FVector& InOutLocation)
{
    bool bHandledPrediction = false;
    ApplyContactSnapVisualPrediction(PeerId, ProxyCharacter, InterpolatedLocation, Settings, InOutLocation,
                                     bHandledPrediction);
    if (!bHandledPrediction)
    {
        ApplyKnockbackVisualPrediction(PeerId, ProxyCharacter, InterpolatedLocation, Settings, InOutLocation,
                                       bHandledPrediction);
    }
}

void ULamePigeonSubsystem::ApplyContactSnapVisualPrediction(int32 PeerId, ACharacter* ProxyCharacter,
    const FVector& InterpolatedLocation, const ULamePigeonSettings* Settings, FVector& InOutLocation,
    bool& bOutHandledPrediction)
{
    FProxyKnockbackContactVisualSnap* Snap = KnockbackContactProxySnaps.Find(PeerId);
    if (!Snap)
        return;

    bOutHandledPrediction = true;
    UWorld* const World = ProxyCharacter->GetWorld();
    if (!World)
    {
        KnockbackContactProxySnaps.Remove(PeerId);
        return;
    }

    const float CurrentWorldTimeSeconds = World->GetTimeSeconds();
    const float ElapsedSeconds = CurrentWorldTimeSeconds - Snap->StartWorldTimeSeconds;
    const float CatchupDistance = Settings ? Settings->PredictedContactProxyReleaseCatchupCm : 45.f;
    const float HysteresisMultiplier =
        Settings ? Settings->PredictedContactProxyReleaseHysteresisMultiplier : 2.25f;
    const int32 RequiredStableFrames =
        Settings ? FMath::Max(1, Settings->PredictedContactProxyReleaseStableFrames) : 6;
    const float MinimumHoldSeconds = Settings ? Settings->PredictedContactProxyMinHoldSeconds : 0.06f;
    const float BlendDurationSeconds =
        Settings ? FMath::Max(0.f, Settings->PredictedContactProxyBlendOutSeconds) : 0.f;

    const FVector InterpolatedLocationXY(InterpolatedLocation.X, InterpolatedLocation.Y, 0.f);
    const FVector SnapLocationXY(Snap->SnapLocation.X, Snap->SnapLocation.Y, 0.f);
    const float DistanceToCatchup = FVector::Distance(InterpolatedLocationXY, SnapLocationXY);

    bool bRemoveSnap = false;

    if (Snap->bBlendOutActive)
    {
        if (BlendDurationSeconds <= KINDA_SMALL_NUMBER)
        {
            bRemoveSnap = true;
        }
        else
        {
            const float BlendProgress = (CurrentWorldTimeSeconds - Snap->BlendOutStartWorldSeconds) / BlendDurationSeconds;
            const float BlendAlpha = FMath::SmoothStep(0.f, 1.f, FMath::Clamp(BlendProgress, 0.f, 1.f));
            InOutLocation = FMath::Lerp(Snap->BlendOutFixedStartLocation, InterpolatedLocation, BlendAlpha);
            if (BlendProgress >= 1.f - KINDA_SMALL_NUMBER)
                bRemoveSnap = true;
        }
    }
    else
    {
        bool bStartBlendOut = false;
        if (ElapsedSeconds >= Snap->MaxHoldSeconds)
            bStartBlendOut = true;
        else if (DistanceToCatchup > CatchupDistance * HysteresisMultiplier)
            Snap->ConsecutiveCloseFrames = 0;
        else if (DistanceToCatchup <= CatchupDistance)
            Snap->ConsecutiveCloseFrames = static_cast<uint16>(
                FMath::Min(static_cast<int32>(Snap->ConsecutiveCloseFrames) + 1, 32000));

        if (!bStartBlendOut && ElapsedSeconds >= MinimumHoldSeconds
            && static_cast<int32>(Snap->ConsecutiveCloseFrames) >= RequiredStableFrames)
        {
            bStartBlendOut = true;
        }

        if (bStartBlendOut)
        {
            if (BlendDurationSeconds <= KINDA_SMALL_NUMBER)
            {
                bRemoveSnap = true;
            }
            else
            {
                Snap->bBlendOutActive = true;
                Snap->BlendOutStartWorldSeconds = CurrentWorldTimeSeconds;
                Snap->BlendOutFixedStartLocation = Snap->SnapLocation;
                InOutLocation = Snap->BlendOutFixedStartLocation;
            }
        }
        else
        {
            InOutLocation = Snap->SnapLocation;
        }
    }

    if (bRemoveSnap)
        KnockbackContactProxySnaps.Remove(PeerId);
}

void ULamePigeonSubsystem::ApplyKnockbackVisualPrediction(int32 PeerId, ACharacter* ProxyCharacter,
    const FVector& InterpolatedLocation, const ULamePigeonSettings* Settings, FVector& InOutLocation,
    bool& bOutHandledPrediction)
{
    FPredictedProxyKnockbackVisual* PredictedVisual = PredictedProxyKnockbacks.Find(PeerId);
    if (!PredictedVisual)
        return;

    bOutHandledPrediction = true;
    UWorld* const World = ProxyCharacter->GetWorld();
    if (!World)
    {
        PredictedProxyKnockbacks.Remove(PeerId);
        return;
    }

    const float CurrentWorldTimeSeconds = World->GetTimeSeconds();
    if (CurrentWorldTimeSeconds < PredictedVisual->StartWorldTimeSeconds)
        return;

    const float ElapsedSeconds = CurrentWorldTimeSeconds - PredictedVisual->StartWorldTimeSeconds;
    const float BallisticSeconds =
        Settings ? FMath::Max(0.f, Settings->PredictedProxyKnockbackBallisticSeconds) : 0.18f;
    const float BlendSeconds =
        Settings ? FMath::Max(0.f, Settings->PredictedProxyKnockbackBlendSeconds) : 0.14f;
    const float HorizontalDeceleration =
        Settings ? FMath::Max(0.f, Settings->PredictedProxyKnockbackHorizontalDecelCmPerSecSq) : 7000.f;
    const FVector StreamDisplacement =
        PredictedVisual->bHasStartInterpolatedLocation ? (InterpolatedLocation - PredictedVisual->StartInterpolatedLocation)
                                                       : FVector::ZeroVector;

    if (ElapsedSeconds >= BallisticSeconds + BlendSeconds - KINDA_SMALL_NUMBER)
    {
        PredictedProxyKnockbacks.Remove(PeerId);
        return;
    }

    FVector Residual = FVector::ZeroVector;
    if (ElapsedSeconds < BallisticSeconds)
    {
        PredictedVisual->bCapturedBlendStartResidual = false;
        const FVector PredictedDisplacement = ComputePredictedProxyBallisticDisplacement(
            PredictedVisual->LaunchVelocityCentimetersPerSecond, ElapsedSeconds, BallisticSeconds,
            World->GetGravityZ(), HorizontalDeceleration);
        Residual = ComputeSafePredictedProxyResidual(PredictedDisplacement, StreamDisplacement);
    }
    else
    {
        const FVector PredictedDisplacementAtBallisticEnd = ComputePredictedProxyBallisticDisplacement(
            PredictedVisual->LaunchVelocityCentimetersPerSecond, BallisticSeconds, BallisticSeconds,
            World->GetGravityZ(), HorizontalDeceleration);
        if (!PredictedVisual->bCapturedBlendStartResidual)
        {
            PredictedVisual->BlendStartResidual =
                ComputeSafePredictedProxyResidual(PredictedDisplacementAtBallisticEnd, StreamDisplacement);
            PredictedVisual->bCapturedBlendStartResidual = true;
        }

        const float BlendProgress =
            BlendSeconds > KINDA_SMALL_NUMBER ? FMath::Clamp((ElapsedSeconds - BallisticSeconds) / BlendSeconds, 0.f, 1.f)
                                              : 1.f;
        const float BlendAlpha = FMath::SmoothStep(0.f, 1.f, BlendProgress);
        Residual = FMath::Lerp(PredictedVisual->BlendStartResidual, FVector::ZeroVector, BlendAlpha);
    }

    InOutLocation = InterpolatedLocation + Residual;
}

void ULamePigeonSubsystem::UpdateProxyMovementState(ACharacter* ProxyCharacter, float SmoothedVx, float SmoothedVy,
    float SmoothedVz, bool bIsFalling)
{
    if (UCharacterMovementComponent* MovementComponent = ProxyCharacter->GetCharacterMovement())
    {
        MovementComponent->Velocity = FVector(SmoothedVx, SmoothedVy, SmoothedVz);
        const FVector HorizontalVelocity(SmoothedVx, SmoothedVy, 0.f);
        SetProxyAcceleration(MovementComponent,
                             HorizontalVelocity.IsNearlyZero(0.1f) ? FVector::ZeroVector : HorizontalVelocity);
        const EMovementMode DesiredMovementMode = bIsFalling ? MOVE_Falling : MOVE_Walking;
        if (MovementComponent->MovementMode != DesiredMovementMode)
        {
            MovementComponent->SetMovementMode(DesiredMovementMode);
        }
    }
}

void ULamePigeonSubsystem::UpdateProxyAnimationState(int32 PeerId, ACharacter* ProxyCharacter,
    const ULamePigeonSettings* Settings, float Yaw, float SmoothedVx, float SmoothedVy, float SmoothedVz, bool bIsFalling)
{
    const ELamePigeonProxyAnimDriver AnimationDriver =
        Settings ? Settings->ProxyAnimDriver : ELamePigeonProxyAnimDriver::CharacterMovement;
    if (AnimationDriver != ELamePigeonProxyAnimDriver::AnimBlueprintVariables)
        return;

    if (Settings && ProxyActors.Num() >= Settings->ProxyAnimThrottleMinPeerCount && Settings->ProxyAnimVarsUpdateStride > 1)
    {
        const uint32 AnimationStride = static_cast<uint32>(Settings->ProxyAnimVarsUpdateStride);
        if (AnimationStride > 1u && ((ProxyAnimThrottleFrameCounter + static_cast<uint32>(PeerId)) % AnimationStride) != 0u)
            return;
    }

    USkeletalMeshComponent* Mesh = ProxyCharacter->GetMesh();
    UAnimInstance* AnimationInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
    if (!AnimationInstance)
        return;

    if (FObjectProperty* CharacterProperty =
        CastField<FObjectProperty>(AnimationInstance->GetClass()->FindPropertyByName(TEXT("Character"))))
    {
        CharacterProperty->SetObjectPropertyValue_InContainer(AnimationInstance, nullptr);
    }

    const double HorizontalSpeed = FVector(SmoothedVx, SmoothedVy, 0.0).Size();
    const bool bIsMoving = HorizontalSpeed > 5.0;

    double MovementDirection = 0.0;
    if (bIsMoving)
    {
        FVector HorizontalVelocity(SmoothedVx, SmoothedVy, 0.0);
        HorizontalVelocity.Normalize();
        MovementDirection = FMath::FindDeltaAngleDegrees(Yaw, HorizontalVelocity.Rotation().Yaw);
    }

    SetAnimBlueprintVariable(AnimationInstance, TEXT("GroundSpeed"), HorizontalSpeed);
    SetAnimBlueprintVariable(AnimationInstance, TEXT("ShouldMove"), bIsMoving);
    SetAnimBlueprintVariable(AnimationInstance, TEXT("Direction"), MovementDirection);
    SetAnimBlueprintVariable(AnimationInstance, TEXT("IsFalling"), bIsFalling);
}

void ULamePigeonSubsystem::ApplyInterpolatedStateToActor(int32 PeerId, ACharacter* ProxyCharacter,
    float X, float Y, float Z, float Yaw, float Pitch,
    float SmoothedVx, float SmoothedVy, float SmoothedVz, bool bIsFalling)
{
    const FVector InterpolatedLocation(X, Y, Z);
    FVector       TargetLocation = InterpolatedLocation;
    FRotator      TargetRotation(Pitch, Yaw, 0.f);

    const ULamePigeonSettings* Settings = GetDefault<ULamePigeonSettings>();
    ApplyProxyVisualPrediction(PeerId, ProxyCharacter, InterpolatedLocation, Settings, TargetLocation);

    ProxyCharacter->SetActorLocationAndRotation(TargetLocation, TargetRotation, false, nullptr, ETeleportType::TeleportPhysics);
    UpdateProxyMovementState(ProxyCharacter, SmoothedVx, SmoothedVy, SmoothedVz, bIsFalling);
    UpdateProxyAnimationState(PeerId, ProxyCharacter, Settings, Yaw, SmoothedVx, SmoothedVy, SmoothedVz, bIsFalling);
}
