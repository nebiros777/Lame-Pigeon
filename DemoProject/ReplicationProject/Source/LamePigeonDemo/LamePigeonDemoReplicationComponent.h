// Source file name: LamePigeonDemoReplicationComponent.h
// Author: Igor Matiushin
// Brief description: Declares local demo gameplay prediction and knockback replication helpers.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LamePigeonDemoConstants.h"
#include "LamePigeonDemoReplicationComponent.generated.h"

class ULamePigeonSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnLamePigeonDemoPredictCommitted, int32, EventId, int32, OtherPeerId,
                                                 int32, PairIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnLamePigeonDemoPredictRejected, int32, EventId, int32, OtherPeerId,
                                                 int32, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLamePigeonDemoPredictResolved, int32, EventId, int32, OtherPeerId);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LAMEPIGEONDEMO_API ULamePigeonDemoReplicationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULamePigeonDemoReplicationComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category="LamePigeonDemo")
	void RequestBroadcastJumpRpc();

	UFUNCTION(BlueprintCallable, Category="LamePigeonDemo", meta=(AutoCreateRefTerm="MyTag,TheirTag"))
	void RequestKnockbackDash(FName MyTag = NAME_None, FName TheirTag = NAME_None);

	UFUNCTION(BlueprintPure, Category="LamePigeonDemo")
	float GetCurrentHealth() const { return CurrentHealth; }

	UPROPERTY(BlueprintAssignable, Category="LamePigeonDemo|Interaction")
	FOnLamePigeonDemoPredictCommitted OnInteractionPredictCommitted;

	UPROPERTY(BlueprintAssignable, Category="LamePigeonDemo|Interaction")
	FOnLamePigeonDemoPredictRejected OnInteractionPredictRejected;

	UPROPERTY(BlueprintAssignable, Category="LamePigeonDemo|Interaction")
	FOnLamePigeonDemoPredictResolved OnInteractionPredictResolved;

protected:
	UFUNCTION()
	void HandleRpcReceived(int32 SenderPeerId, FString FuncName, const TArray<float>& FloatArgs);

	UFUNCTION()
	void HandleInteractionRejected(int32 YourEventId, int32 Reason);

	void TryPublishInitialHealth();
	void TickActiveDash(float DeltaTime);
	void SendKnockbackToVictimIfNeeded(ACharacter* DasherCharacter, ACharacter* VictimCharacter,
	                                   const FVector& DashStepWorld, float SendTimeSeconds);

	void ApplyProxyJumpVisual(int32 SenderPeerId);
	void ApplyLocalKnockbackFromRpc(const TArray<float>& FloatArgs);
	bool TryBuildDashKnockbackLaunchVelocity(int32 VictimPeerId, ACharacter* DasherCharacter, const FVector& DashStepWorld,
	                                         FVector& OutLaunchVelocity) const;
	void StartDasherProxyKnockbackVisual(int32 VictimPeerId, ACharacter* DasherCharacter, const FVector& DashStepWorld,
	                                     float StartDelaySeconds, uint32 PredictEventId);

	bool IsInteractionPredictionEnabled() const;

	bool ComputeEarliestDashProxyHit(ACharacter* Dasher, const FVector& DashDir, float TotalDistanceCm,
	                                 int32& OutVictimPeerId, float& OutTimeToHitSeconds,
	                                 FVector* OptionalSweepEndWorld = nullptr,
	                                 FVector* OptionalHitPositionWorld = nullptr,
	                                 bool* OptionalBHitProxy = nullptr) const;

	void TickContactPredictions30Hz(float DeltaTime);
	void RunContactPredictScanStep();
	bool TrySendDasherPredictionFromScan(ACharacter* LocalCharacter, ULamePigeonSubsystem* Subsystem,
	                                     float CurrentWorldTimeSeconds);
	void RemoveOlderPendingVictimPredictions(int32 DasherPeerId);
	void TickResolveFinishedKnockbackPredictions(float DeltaTime);
	void TickApplyPendingKnockbacks(float DeltaTime);

	void PublishInteractionContextIfNeeded(ACharacter* LocalCharacter);

	int32 ComputeLocalMovementContext(ACharacter* LocalCharacter) const;

	void ApplyKnockbackWithDamage(const FVector& LaunchVelocity, bool bSubtractHealth = true);
	void RollbackKnockback(uint32 EventId);

	float CurrentHealth = LamePigeonDemoConstants::InitialHealthPoints;

private:
	struct FPendingKnockbackPrediction
	{
		uint32 EventId                 = 0;
		int32  OtherPeerId             = INDEX_NONE;
		int32  PairIndex               = 0;
		FName  InteractionProfileId    = NAME_None;
		double ApplyAtWorldSeconds     = 0.0;
		double AppliedAtWorldSeconds   = 0.0;
		bool   bApplied                = false;
		bool   bRejected               = false;
		FVector RollbackLocation       = FVector::ZeroVector;
		FVector RollbackVelocity       = FVector::ZeroVector;
		float   RollbackHealth         = 0.f;
		bool    bHasRollbackSnapshot   = false;
	};

	TWeakObjectPtr<ULamePigeonSubsystem> LamePigeonSubsystem;

	bool bInitialHealthPublished = false;
	bool bDashActive = false;
	float DashTimeRemainingSeconds = 0.f;
	FVector DashDirectionWorld = FVector::ForwardVector;
	TSet<int32> KnockbackSentPeerIdsThisDash;
	FName PendingDashPredictMyTag = NAME_None;
	FName PendingDashPredictTheirTag = NAME_None;
	double DashContextHoldUntilWorldSeconds = 0.0;

	uint32 NextInteractionEventId = 1;
	TMap<uint32, FPendingKnockbackPrediction> PendingKnockbacks;
	TMap<int32, float> LastVictimPredictWorldTime;
	TMap<int32, float> LastDasherPredictWorldTime;
	TMap<uint32, int32> PendingDasherProxyBouncePeerIds;
	TMap<int32, double> VictimKnockbackCooldownUntilWorldSeconds;

	float InteractionPredict30HzAccum = 0.f;

	int32 LastPublishedInteractionContext = -1;
};
