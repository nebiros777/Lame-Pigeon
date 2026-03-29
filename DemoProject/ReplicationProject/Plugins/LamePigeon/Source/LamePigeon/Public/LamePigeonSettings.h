// Source file name: LamePigeonSettings.h
// Author: Igor Matiushin
// Brief description: Declares Unreal project settings for replication, interaction prediction, and proxy visuals.

#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "LamePigeonSettings.generated.h"

USTRUCT(BlueprintType)
struct FLamePigeonInteractionRolePair
{
    GENERATED_BODY()

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon")
    FName InteractionProfileId;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon")
    FName RoleA;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon")
    FName RoleB;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon")
    bool bSymmetric = false;
};

UENUM(BlueprintType)
enum class ELamePigeonProxyAnimDriver : uint8
{
    CharacterMovement UMETA(DisplayName="CharacterMovement"),
    AnimBlueprintVariables UMETA(DisplayName="AnimBlueprintVariables"),
};

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="LamePigeon"))
class LAMEPIGEON_API ULamePigeonSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    ULamePigeonSettings();
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon",
              meta=(AllowAbstract=false))
    TSoftClassPtr<ACharacter> ProxyCharacterClass;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon")
    FString ServerAddress = TEXT("127.0.0.1");

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon")
    int32 ServerPort = 7777;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon")
    int32 DefaultRoomId = 42;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon",
              meta=(ClampMin=1, ClampMax=60))
    int32 SendRateHz = 20;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon|Interpolation",
              meta=(ClampMin=0.0, ClampMax=2.0))
    float InterpolationDelaySeconds = 0.1f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon|Interpolation",
              meta=(ClampMin=0.0, ClampMax=2.0))
    float MaxExtrapolationSeconds = 0.25f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon|Interpolation")
    bool bScaleInterpolationTuningByFrequencies = true;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon|Interpolation",
              meta=(ClampMin=1.0, ClampMax=120.0, EditCondition="bScaleInterpolationTuningByFrequencies"))
    float InterpolationReferenceSendRateHz = 20.f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon|Interpolation",
              meta=(ClampMin=1.0, ClampMax=120.0, EditCondition="bScaleInterpolationTuningByFrequencies"))
    float InterpolationReferencePredictScanHz = 30.f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon|Interpolation",
              meta=(ClampMin=1.0, ClampMax=120.0))
    float ProxyVelocitySmoothBaseHz = 10.f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon|Performance",
              meta=(ClampMin=1, ClampMax=64))
    int32 MaxProxySpawnsPerFrame = 3;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon|Performance")
    EVisibilityBasedAnimTickOption ProxyVisibilityBasedAnimTick = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon|Proxy|Animation")
    ELamePigeonProxyAnimDriver ProxyAnimDriver = ELamePigeonProxyAnimDriver::CharacterMovement;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon|Performance")
    bool bProxyMeshEnableUpdateRateOptimizations = true;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon|Performance",
              meta=(ClampMin=1, ClampMax=256))
    int32 ProxyAnimThrottleMinPeerCount = 16;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon|Performance",
              meta=(ClampMin=1, ClampMax=8))
    int32 ProxyAnimVarsUpdateStride = 2;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon|Interaction")
    TArray<FLamePigeonInteractionRolePair> InteractionRolePairs;

    UFUNCTION(BlueprintCallable, Category="LamePigeon|Interaction")
    static bool GetDefaultDasherPredictTags(FName& OutMyTag, FName& OutTheirTag);

    UFUNCTION(BlueprintCallable, Category="LamePigeon|Interaction")
    static bool GetDefaultVictimPredictTags(FName& OutMyTag, FName& OutTheirTag);

    UFUNCTION(BlueprintCallable, Category="LamePigeon|Interaction")
    static bool GetPredictTagsForPairIndex(int32 PairIndex, bool bVictimSide, FName& OutMyTag, FName& OutTheirTag);

    UFUNCTION(BlueprintCallable, Category="LamePigeon|Interaction")
    static int32 ResolveInteractionPairIndex(int32 LocalMovementContext, int32 RemoteMovementContext);

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon|Interaction",
              meta=(ClampMin=0.05, ClampMax=5.0))
    float InteractionEventTtlSeconds = 1.f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon|Interaction",
              meta=(ClampMin=0.01, ClampMax=2.0))
    float InteractionConfirmWindowSeconds = 1.0f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon|Interaction",
              meta=(ClampMin=0.05, ClampMax=5.0))
    float InteractionRejectAfterSeconds = 0.85f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon|Interaction")
    bool bEnableInteractionPrediction = true;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon|Interaction",
              meta=(ClampMin=1.0, ClampMax=120.0))
    float InteractionPredictScanHz = 30.f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon|Interaction|Tuning",
              meta=(ClampMin=0.0, ClampMax=200.0))
    float DashOverlapExtraRadiusCm = 4.f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon|Interaction|Tuning",
              meta=(ClampMin=0.0, ClampMax=200.0))
    float VictimCloseContactSlackCm = 55.f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon|Interaction|Tuning",
              meta=(ClampMin=100.0, ClampMax=3000.0))
    float VictimDashHeuristicMaxDistXY = 900.f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon|Interaction|Tuning",
              meta=(ClampMin=-1.0, ClampMax=1.0))
    float VictimTowardDotMin = 0.2f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon|Interaction|Tuning",
              meta=(ClampMin=-1.0, ClampMax=1.0))
    float VictimAimDotMin = 0.35f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon|Interaction|Tuning",
              meta=(ClampMin=100.0, ClampMax=5000.0))
    float VictimFastPathMaxDistXY = 1500.f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon|Interaction|Tuning",
              meta=(ClampMin=0.0, ClampMax=800.0))
    float VictimFastPathMaxSeparatedCm = 400.f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon|Interaction|Tuning",
              meta=(ClampMin=0.1, ClampMax=5.0))
    float PredictedContactProxyHoldMaxSeconds = 3.f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon|Interaction|Tuning",
              meta=(ClampMin=5.0, ClampMax=200.0))
    float PredictedContactProxyReleaseCatchupCm = 42.f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon|Interaction|Tuning",
              meta=(ClampMin=1, ClampMax=60))
    int32 PredictedContactProxyReleaseStableFrames = 8;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon|Interaction|Tuning",
              meta=(ClampMin=1.05, ClampMax=4.0))
    float PredictedContactProxyReleaseHysteresisMultiplier = 2.25f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon|Interaction|Tuning",
              meta=(ClampMin=0.0, ClampMax=0.5))
    float PredictedContactProxyMinHoldSeconds = 0.08f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon|Interaction|Tuning",
              meta=(ClampMin=0.0, ClampMax=100.0))
    float PredictedContactProxyGapCm = 8.f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon|Interaction|Tuning",
              meta=(ClampMin=0.0, ClampMax=0.5))
    float PredictedContactProxyBlendOutSeconds = 0.12f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon|Interaction|Tuning",
              meta=(ClampMin=0.0, ClampMax=1.5))
    float PredictedProxyKnockbackBallisticSeconds = 0.18f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon|Interaction|Tuning",
              meta=(ClampMin=0.0, ClampMax=1.0))
    float PredictedProxyKnockbackBlendSeconds = 0.14f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="LamePigeon|Interaction|Tuning",
              meta=(ClampMin=0.0, ClampMax=30000.0))
    float PredictedProxyKnockbackHorizontalDecelCmPerSecSq = 7000.f;
};
