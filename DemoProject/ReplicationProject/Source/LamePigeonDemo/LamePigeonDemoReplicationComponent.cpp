// Source file name: LamePigeonDemoReplicationComponent.cpp
// Author: Igor Matiushin
// Brief description: Implements local demo gameplay prediction, dash scanning, and knockback application.

#include "LamePigeonDemoReplicationComponent.h"
#include "LamePigeonDemoConstants.h"
#include "LamePigeonSettings.h"
#include "LamePigeonSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/GameInstance.h"
#include "CollisionQueryParams.h"
#include "Engine/OverlapResult.h"
#include "WorldCollision.h"
#include "LamePigeonDemo.h"

namespace
{
constexpr float DashDurationSeconds =
	LamePigeonDemoConstants::DashDistanceCentimeters / LamePigeonDemoConstants::DashSpeedCentimetersPerSecond;

FVector ComputeKnockbackFlatDirection(ACharacter* Dasher, ACharacter* Victim, const FVector& DashStep, UWorld* World)
{
	if (!Dasher || !Victim || !World)
		return FVector::ZeroVector;

	UCapsuleComponent* DasherCap = Dasher->GetCapsuleComponent();
	UCapsuleComponent* VictimCap = Victim->GetCapsuleComponent();
	if (!DasherCap || !VictimCap)
		return FVector::ZeroVector;

	const FVector Start    = Dasher->GetActorLocation();
	const FVector VictimLoc = Victim->GetActorLocation();
	FVector       AwayFromDasher2D(VictimLoc.X - Start.X, VictimLoc.Y - Start.Y, 0.f);
	if (!AwayFromDasher2D.Normalize())
		AwayFromDasher2D = FVector(DashStep.X, DashStep.Y, 0.f).GetSafeNormal();

	const FVector ToVictim = VictimLoc - Start;
	const float   ToVictimLen = ToVictim.Size2D();
	if (ToVictimLen > KINDA_SMALL_NUMBER)
	{
		const FVector TowardVictim2D = FVector(ToVictim.X, ToVictim.Y, 0.f).GetSafeNormal();
		const float   SweepDist      = FMath::Min(DashStep.Size2D(), ToVictimLen + DasherCap->GetScaledCapsuleRadius()
		                                                              + VictimCap->GetScaledCapsuleRadius() + 10.f);
		const FVector End            = Start + TowardVictim2D * SweepDist;

		FCollisionQueryParams Q(SCENE_QUERY_STAT(LamePigeonKnockVictimSweep), false, Dasher);
		FHitResult            Hit;
		const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(
			DasherCap->GetScaledCapsuleRadius(), DasherCap->GetScaledCapsuleHalfHeight());
		if (World->SweepSingleByChannel(Hit, Start, End, Dasher->GetActorQuat(), ECC_Pawn, CapsuleShape, Q)
		    && Hit.GetActor() == Victim)
		{
			FVector N(Hit.ImpactNormal.X, Hit.ImpactNormal.Y, 0.f);
			if (N.Normalize())
			{
				if (FVector::DotProduct(N, AwayFromDasher2D) < 0.f)
					N *= -1.f;
				if (FVector::DotProduct(N, AwayFromDasher2D) < LamePigeonDemoConstants::KnockbackNormalMinDotAway)
					return AwayFromDasher2D;
				return N;
			}
		}
	}

	return AwayFromDasher2D;
}

FVector ComputeKnockbackFlatDirectionVirtualDasher(ACharacter* Victim, const FVector& DasherWorldLocation,
                                                  const FVector& DashStepXY, UWorld* World, float DasherCapsuleRadius,
                                                  float DasherCapsuleHalfHeight)
{
	if (!Victim || !World)
		return FVector::ZeroVector;
	UCapsuleComponent* VictimCap = Victim->GetCapsuleComponent();
	if (!VictimCap)
		return FVector::ZeroVector;

	const FVector Start    = DasherWorldLocation;
	const FVector VictimLoc = Victim->GetActorLocation();
	FVector       AwayFromDasher2D(VictimLoc.X - Start.X, VictimLoc.Y - Start.Y, 0.f);
	if (!AwayFromDasher2D.Normalize())
		AwayFromDasher2D = FVector(DashStepXY.X, DashStepXY.Y, 0.f).GetSafeNormal();

	const FVector ToVictim    = VictimLoc - Start;
	const float   ToVictimLen = ToVictim.Size2D();
	if (ToVictimLen > KINDA_SMALL_NUMBER)
	{
		const FVector TowardVictim2D = FVector(ToVictim.X, ToVictim.Y, 0.f).GetSafeNormal();
		const float   SweepDist      = FMath::Max(10.f,
		                                          FMath::Min(DashStepXY.Size2D(), ToVictimLen + DasherCapsuleRadius
		                                                                                    + VictimCap->GetScaledCapsuleRadius()
		                                                                                    + 10.f));
		const FVector End = Start + TowardVictim2D * SweepDist;

		FCollisionQueryParams Q(SCENE_QUERY_STAT(LamePigeonKnockVictimVirtSweep), false, Victim);
		const FCollisionShape CapsuleShape =
			FCollisionShape::MakeCapsule(DasherCapsuleRadius, DasherCapsuleHalfHeight);
		FHitResult Hit;
		if (World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Pawn, CapsuleShape, Q)
		    && Hit.GetActor() == Victim)
		{
			FVector N(Hit.ImpactNormal.X, Hit.ImpactNormal.Y, 0.f);
			if (N.Normalize())
			{
				if (FVector::DotProduct(N, AwayFromDasher2D) < 0.f)
					N *= -1.f;
				if (FVector::DotProduct(N, AwayFromDasher2D) < LamePigeonDemoConstants::KnockbackNormalMinDotAway)
					return AwayFromDasher2D;
				return N;
			}
		}
	}

	return AwayFromDasher2D;
}

static float WireDeltaForGeometryTime(float GeometryTimeToHit)
{
	return FMath::Clamp(GeometryTimeToHit + LamePigeonDemoConstants::InteractionPredictWireSlackSeconds, 0.06f,
	                    LamePigeonDemoConstants::KnockbackPredictWireDeltaClampSeconds);
}
}

ULamePigeonDemoReplicationComponent::ULamePigeonDemoReplicationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	CurrentHealth                     = LamePigeonDemoConstants::InitialHealthPoints;
}

void ULamePigeonDemoReplicationComponent::BeginPlay()
{
	Super::BeginPlay();

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
		return;

	UGameInstance* GameInstance = OwnerPawn->GetGameInstance();
	if (!GameInstance)
		return;

	ULamePigeonSubsystem* Subsystem = GameInstance->GetSubsystem<ULamePigeonSubsystem>();
	if (!Subsystem)
		return;

	LamePigeonSubsystem = Subsystem;
	Subsystem->OnRpcReceived.AddDynamic(this, &ULamePigeonDemoReplicationComponent::HandleRpcReceived);
	Subsystem->OnInteractionRejected.AddDynamic(this, &ULamePigeonDemoReplicationComponent::HandleInteractionRejected);

}

void ULamePigeonDemoReplicationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ULamePigeonSubsystem* Subsystem = LamePigeonSubsystem.Get())
	{
		Subsystem->OnRpcReceived.RemoveDynamic(this, &ULamePigeonDemoReplicationComponent::HandleRpcReceived);
		Subsystem->OnInteractionRejected.RemoveDynamic(this, &ULamePigeonDemoReplicationComponent::HandleInteractionRejected);
	}

	Super::EndPlay(EndPlayReason);
}

void ULamePigeonDemoReplicationComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                        FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn && OwnerPawn->IsLocallyControlled())
	{
		if (UGameInstance* GameInstance = OwnerPawn->GetGameInstance())
			if (ULamePigeonSubsystem* SubsystemEarlyPump = GameInstance->GetSubsystem<ULamePigeonSubsystem>())
				SubsystemEarlyPump->PumpCarrierOncePerFrame(DeltaTime);
	}

	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
		return;

	TryPublishInitialHealth();
	if (ACharacter* Ch = Cast<ACharacter>(OwnerPawn))
		PublishInteractionContextIfNeeded(Ch);
	TickActiveDash(DeltaTime);
	TickContactPredictions30Hz(DeltaTime);
	TickApplyPendingKnockbacks(DeltaTime);
	TickResolveFinishedKnockbackPredictions(DeltaTime);
}

void ULamePigeonDemoReplicationComponent::TryPublishInitialHealth()
{
	if (bInitialHealthPublished)
		return;

	ULamePigeonSubsystem* Subsystem = LamePigeonSubsystem.Get();
	if (!Subsystem || !Subsystem->IsConnected() || Subsystem->GetCurrentRoomId() < 0)
		return;

	Subsystem->SendReplicatedFloat(FString(LamePigeonDemoConstants::HealthVariableName), CurrentHealth);
	bInitialHealthPublished = true;
}

int32 ULamePigeonDemoReplicationComponent::ComputeLocalMovementContext(ACharacter* LocalCharacter) const
{
	if (!LocalCharacter)
		return LamePigeonDemoConstants::InteractionContextWalk;
	if (bDashActive)
		return LamePigeonDemoConstants::InteractionContextDash;
	if (const UWorld* World = LocalCharacter->GetWorld())
	{
		if (static_cast<double>(World->GetTimeSeconds()) < DashContextHoldUntilWorldSeconds)
			return LamePigeonDemoConstants::InteractionContextDash;
	}
	return LamePigeonDemoConstants::InteractionContextWalk;
}

void ULamePigeonDemoReplicationComponent::PublishInteractionContextIfNeeded(ACharacter* LocalCharacter)
{
	ULamePigeonSubsystem* Subsystem = LamePigeonSubsystem.Get();
	if (!Subsystem || !Subsystem->IsConnected() || Subsystem->GetCurrentRoomId() < 0 || !LocalCharacter)
		return;

	const int32 LocalMovementContext = ComputeLocalMovementContext(LocalCharacter);
	if (LocalMovementContext == LastPublishedInteractionContext)
		return;

	Subsystem->SendReplicatedInt(FString(LamePigeonDemoConstants::InteractionContextVariableName), LocalMovementContext);
	LastPublishedInteractionContext = LocalMovementContext;

}

void ULamePigeonDemoReplicationComponent::RequestKnockbackDash(FName MyTag, FName TheirTag)
{
	if (bDashActive)
		return;

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
		return;

	ACharacter* Character = Cast<ACharacter>(OwnerPawn);
	if (!Character)
		return;

	DashDirectionWorld = Character->GetActorForwardVector();
	if (APlayerController* PlayerController = Cast<APlayerController>(Character->GetController()))
	{
		FRotator ControlRotation = PlayerController->GetControlRotation();
		ControlRotation.Pitch    = 0.f;
		ControlRotation.Roll     = 0.f;
		FVector FromCam            = ControlRotation.Vector();
		FromCam.Z                  = 0.f;
		if (FromCam.Normalize())
			DashDirectionWorld = FromCam;
	}

	DashTimeRemainingSeconds = DashDurationSeconds;
	bDashActive                = true;
	KnockbackSentPeerIdsThisDash.Reset();

	if (UWorld* World = Character->GetWorld())
	{
		DashContextHoldUntilWorldSeconds =
			static_cast<double>(World->GetTimeSeconds()) + static_cast<double>(DashDurationSeconds)
			+ static_cast<double>(LamePigeonDemoConstants::DashContextStickyHoldAfterEndSeconds);
	}

	FName UseMy    = MyTag;
	FName UseTheir = TheirTag;
	const bool bNoMy    = UseMy.IsNone();
	const bool bNoTheir = UseTheir.IsNone();
	if (bNoMy && bNoTheir)
	{
		if (!ULamePigeonSettings::GetPredictTagsForPairIndex(0, false, UseMy, UseTheir))
			return;
	}
	else if (bNoMy || bNoTheir)
		return;
	PendingDashPredictMyTag    = UseMy;
	PendingDashPredictTheirTag = UseTheir;
}

void ULamePigeonDemoReplicationComponent::TickActiveDash(float DeltaTime)
{
	if (!bDashActive)
		return;

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		bDashActive = false;
		return;
	}

	UWorld* const World = GetWorld();
	if (!World)
		return;

	const float   StepDistance = LamePigeonDemoConstants::DashSpeedCentimetersPerSecond * DeltaTime;
	const FVector DashStep     = DashDirectionWorld * StepDistance;

	const FVector TraceStart = Character->GetActorLocation();
	const FVector TraceEnd = TraceStart + DashStep;
	const ULamePigeonSettings* Settings = GetDefault<ULamePigeonSettings>();
	const float ExtraRadius = Settings ? Settings->DashOverlapExtraRadiusCm : 4.f;
	const float CapsuleRadius = Character->GetCapsuleComponent()
		                            ? Character->GetCapsuleComponent()->GetScaledCapsuleRadius() + ExtraRadius
		                            : 80.f;
	const float CapsuleHalfHeight = Character->GetCapsuleComponent()
		                                ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
		                                : 88.f;

	TArray<FHitResult> Hits;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(LamePigeonDashOverlap), false, Character);
	if (World->SweepMultiByChannel(Hits, TraceStart, TraceEnd, Character->GetActorQuat(), ECC_Pawn,
	                               FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight), QueryParams))
	{
		const float SendTime = World->GetTimeSeconds();
		for (const FHitResult& Hit : Hits)
		{
			ACharacter* OtherCharacter = Cast<ACharacter>(Hit.GetActor());
			if (!OtherCharacter || OtherCharacter == Character)
				continue;
			SendKnockbackToVictimIfNeeded(Character, OtherCharacter, DashStep, SendTime);
		}
	}

	Character->AddActorWorldOffset(DashDirectionWorld * StepDistance, true);

	DashTimeRemainingSeconds -= DeltaTime;
	if (DashTimeRemainingSeconds <= 0.f)
	{
		bDashActive = false;
	}
}

void ULamePigeonDemoReplicationComponent::SendKnockbackToVictimIfNeeded(ACharacter* DasherCharacter,
                                                                       ACharacter* VictimCharacter,
                                                                       const FVector& DashStepWorld,
                                                                       float SendTimeSeconds)
{
	ULamePigeonSubsystem* Subsystem = LamePigeonSubsystem.Get();
	if (!Subsystem)
		return;

	if (IsInteractionPredictionEnabled())
		return;

	int32 VictimPeerId = INDEX_NONE;
	if (!Subsystem->TryGetPeerIdForProxyActor(VictimCharacter, VictimPeerId))
		return;
	if (KnockbackSentPeerIdsThisDash.Contains(VictimPeerId))
		return;

	KnockbackSentPeerIdsThisDash.Add(VictimPeerId);

	FVector HorizontalDirection =
		ComputeKnockbackFlatDirection(DasherCharacter, VictimCharacter, DashStepWorld, GetWorld());
	if (HorizontalDirection.IsNearlyZero())
	{
		HorizontalDirection = FVector(DashStepWorld.X, DashStepWorld.Y, 0.f).GetSafeNormal();
	}

	TArray<float> Args;
	Args.Add(HorizontalDirection.X);
	Args.Add(HorizontalDirection.Y);
	Args.Add(HorizontalDirection.Z);
	Args.Add(LamePigeonDemoConstants::KnockbackHorizontalSpeed);
	Args.Add(SendTimeSeconds);

	Subsystem->SendRPCToPeer(VictimPeerId, FString(LamePigeonDemoConstants::ApplyKnockbackRpcName), Args);
	StartDasherProxyKnockbackVisual(VictimPeerId, DasherCharacter, DashStepWorld, 0.f, 0u);
}

void ULamePigeonDemoReplicationComponent::RequestBroadcastJumpRpc()
{
	if (ULamePigeonSubsystem* Subsystem = LamePigeonSubsystem.Get())
		Subsystem->BroadcastRPC(FString(LamePigeonDemoConstants::ReplicateJumpRpcName), TArray<float>());
}

void ULamePigeonDemoReplicationComponent::HandleRpcReceived(int32 SenderPeerId, FString FuncName,
                                                            const TArray<float>& FloatArgs)
{
	if (FuncName == LamePigeonDemoConstants::ReplicateJumpRpcName)
	{
		ApplyProxyJumpVisual(SenderPeerId);
		return;
	}
	if (FuncName == LamePigeonDemoConstants::ApplyKnockbackRpcName)
	{
		ApplyLocalKnockbackFromRpc(FloatArgs);
	}
}

void ULamePigeonDemoReplicationComponent::ApplyProxyJumpVisual(int32 SenderPeerId)
{
	ULamePigeonSubsystem* Subsystem = LamePigeonSubsystem.Get();
	if (!Subsystem)
		return;

	ACharacter* ProxyCharacter = Subsystem->GetProxyCharacter(SenderPeerId);
	if (!IsValid(ProxyCharacter))
		return;

	const FVector Bump(0.f, 0.f, LamePigeonDemoConstants::JumpBumpCentimeters);
	ProxyCharacter->AddActorWorldOffset(Bump, false, nullptr, ETeleportType::None);
}

void ULamePigeonDemoReplicationComponent::ApplyLocalKnockbackFromRpc(const TArray<float>& FloatArgs)
{
	if (FloatArgs.Num() < 4)
		return;

	if (IsInteractionPredictionEnabled())
		return;

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
		return;

	ACharacter* Character = Cast<ACharacter>(OwnerPawn);
	if (!Character)
		return;

	if (CurrentHealth <= 0.f)
		return;

	CurrentHealth = FMath::Max(0.f, CurrentHealth - LamePigeonDemoConstants::KnockbackDamagePoints);
	if (ULamePigeonSubsystem* Subsystem = LamePigeonSubsystem.Get())
		Subsystem->SendReplicatedFloat(FString(LamePigeonDemoConstants::HealthVariableName), CurrentHealth);

	UWorld* World = GetWorld();
	if (World && FloatArgs.Num() >= 5)
	{
		const float Now   = World->GetTimeSeconds();
		const float Delta = Now - FloatArgs[4];
	}

	const FVector Horizontal(FloatArgs[0], FloatArgs[1], FloatArgs[2]);
	const float   Speed = FloatArgs[3];
	FVector       LaunchVelocity = Horizontal * Speed;
	LaunchVelocity.Z += LamePigeonDemoConstants::KnockbackUpwardComponent;

	Character->LaunchCharacter(LaunchVelocity, true, true);
}

bool ULamePigeonDemoReplicationComponent::TryBuildDashKnockbackLaunchVelocity(int32 VictimPeerId,
                                                                              ACharacter* DasherCharacter,
                                                                              const FVector& DashStepWorld,
                                                                              FVector& OutLaunchVelocity) const
{
	OutLaunchVelocity = FVector::ZeroVector;

	ULamePigeonSubsystem* Subsystem = LamePigeonSubsystem.Get();
	UWorld* World = GetWorld();
	if (!Subsystem || !World || !DasherCharacter)
		return false;

	ACharacter* VictimProxyCharacter = Subsystem->GetProxyCharacter(VictimPeerId);
	FVector VictimProxyLocation = FVector::ZeroVector;
	FVector VictimProxyVelocity = FVector::ZeroVector;
	float VictimProxyYawDegrees = 0.f;
	const bool bHasInterpolatedKinematics =
		Subsystem->TryGetProxyInterpolatedKinematics(VictimPeerId, VictimProxyLocation, VictimProxyVelocity, VictimProxyYawDegrees);
	if (!VictimProxyCharacter && !bHasInterpolatedKinematics)
		return false;

	FVector HorizontalDirection = FVector::ZeroVector;
	if (VictimProxyCharacter)
	{
		HorizontalDirection = ComputeKnockbackFlatDirection(DasherCharacter, VictimProxyCharacter, DashStepWorld, World);
	}

	if (HorizontalDirection.IsNearlyZero())
	{
		const FVector DasherLocation = DasherCharacter->GetActorLocation();
		const FVector VictimLocation = VictimProxyCharacter ? VictimProxyCharacter->GetActorLocation() : VictimProxyLocation;
		HorizontalDirection =
			FVector(VictimLocation.X - DasherLocation.X, VictimLocation.Y - DasherLocation.Y, 0.f).GetSafeNormal();
	}

	if (HorizontalDirection.IsNearlyZero())
		HorizontalDirection = FVector(DashStepWorld.X, DashStepWorld.Y, 0.f).GetSafeNormal();

	if (HorizontalDirection.IsNearlyZero())
		return false;

	OutLaunchVelocity = HorizontalDirection * LamePigeonDemoConstants::KnockbackHorizontalSpeed;
	OutLaunchVelocity.Z += LamePigeonDemoConstants::KnockbackUpwardComponent;
	return true;
}

void ULamePigeonDemoReplicationComponent::StartDasherProxyKnockbackVisual(int32 VictimPeerId, ACharacter* DasherCharacter,
                                                                          const FVector& DashStepWorld,
                                                                          float StartDelaySeconds, uint32 PredictEventId)
{
	ULamePigeonSubsystem* Subsystem = LamePigeonSubsystem.Get();
	if (!Subsystem)
		return;

	FVector LaunchVelocity;
	if (!TryBuildDashKnockbackLaunchVelocity(VictimPeerId, DasherCharacter, DashStepWorld, LaunchVelocity))
		return;

	if (PredictEventId > 0u)
	{
		for (auto PendingBounceIterator = PendingDasherProxyBouncePeerIds.CreateIterator(); PendingBounceIterator;
		     ++PendingBounceIterator)
		{
			if (PendingBounceIterator.Value() == VictimPeerId)
				PendingBounceIterator.RemoveCurrent();
		}
		PendingDasherProxyBouncePeerIds.Add(PredictEventId, VictimPeerId);
	}

	Subsystem->NotifyPredictedProxyKnockback(VictimPeerId, LaunchVelocity, StartDelaySeconds,
	                                         static_cast<int32>(PredictEventId));
}

bool ULamePigeonDemoReplicationComponent::IsInteractionPredictionEnabled() const
{
	const ULamePigeonSettings* Settings = GetDefault<ULamePigeonSettings>();
	return Settings && Settings->bEnableInteractionPrediction;
}

bool ULamePigeonDemoReplicationComponent::ComputeEarliestDashProxyHit(ACharacter* Dasher, const FVector& DashDir,
                                                                      float TotalDistanceCm, int32& OutVictimPeerId,
                                                                      float& OutTimeToHitSeconds,
                                                                      FVector* OptionalSweepEndWorld,
                                                                      FVector* OptionalHitPositionWorld,
                                                                      bool* OptionalBHitProxy) const
{
	OutVictimPeerId     = INDEX_NONE;
	OutTimeToHitSeconds = BIG_NUMBER;

	if (OptionalBHitProxy)
		*OptionalBHitProxy = false;

	if (!Dasher)
		return false;

	ULamePigeonSubsystem* Subsystem = LamePigeonSubsystem.Get();
	UWorld*               World     = GetWorld();
	if (!Subsystem || !World)
		return false;

	UCapsuleComponent* DasherCap = Dasher->GetCapsuleComponent();
	if (!DasherCap)
		return false;

	const FVector Dir2D(DashDir.X, DashDir.Y, 0.f);
	if (Dir2D.IsNearlyZero())
		return false;
	const FVector Dir = Dir2D.GetSafeNormal();
	const FVector Start = Dasher->GetActorLocation();
	const FVector End   = Start + Dir * TotalDistanceCm;
	if (OptionalSweepEndWorld)
		*OptionalSweepEndWorld = End;

	FCollisionQueryParams Q(SCENE_QUERY_STAT(LamePigeonDashProxySweep), false, Dasher);
	const ULamePigeonSettings* Settings = GetDefault<ULamePigeonSettings>();
	const float ExtraRadius = Settings ? Settings->DashOverlapExtraRadiusCm : 4.f;
	const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(
		DasherCap->GetScaledCapsuleRadius() + ExtraRadius, DasherCap->GetScaledCapsuleHalfHeight());

	TArray<FHitResult> Hits;
	const bool bSweepHit = World->SweepMultiByChannel(Hits, Start, End, Dasher->GetActorQuat(), ECC_Pawn, CapsuleShape, Q);

	float          BestTime  = BIG_NUMBER;
	int32          BestPeer  = INDEX_NONE;
	FVector        BestHitPos = FVector::ZeroVector;
	if (bSweepHit)
	{
		for (const FHitResult& Hit : Hits)
		{
			ACharacter* Other = Cast<ACharacter>(Hit.GetActor());
			if (!Other || Other == Dasher)
				continue;
			int32 PeerId = INDEX_NONE;
			if (!Subsystem->TryGetPeerIdForProxyActor(Other, PeerId))
				continue;
			const float T = FMath::Clamp(Hit.Time, 0.f, 1.f);
			if (T < BestTime)
			{
				BestTime  = T;
				BestPeer  = PeerId;
				BestHitPos = Hit.ImpactPoint.IsNearlyZero() ? Hit.Location : Hit.ImpactPoint;
			}
		}
	}

	if (BestPeer == INDEX_NONE)
	{
		TArray<FOverlapResult> Overlaps;
		if (World->OverlapMultiByChannel(Overlaps, Start, Dasher->GetActorQuat(), ECC_Pawn, CapsuleShape, Q))
		{
			float BestDistSq = 1.e30f;
			for (const FOverlapResult& O : Overlaps)
			{
				ACharacter* Other = Cast<ACharacter>(O.GetActor());
				if (!Other || Other == Dasher)
					continue;
				int32 PeerId = INDEX_NONE;
				if (!Subsystem->TryGetPeerIdForProxyActor(Other, PeerId))
					continue;
				const float Dsq = FVector::DistSquared2D(Start, Other->GetActorLocation());
				if (Dsq < BestDistSq)
				{
					BestDistSq = Dsq;
					BestPeer   = PeerId;
					BestHitPos = Other->GetActorLocation();
				}
			}
		}
		if (BestPeer != INDEX_NONE)
		{
			OutVictimPeerId       = BestPeer;
			OutTimeToHitSeconds   = 0.f;
			if (OptionalHitPositionWorld)
				*OptionalHitPositionWorld = BestHitPos;
			if (OptionalBHitProxy)
				*OptionalBHitProxy = true;
			return true;
		}
		return false;
	}

	OutVictimPeerId = BestPeer;
	OutTimeToHitSeconds =
		BestTime * (TotalDistanceCm / LamePigeonDemoConstants::DashSpeedCentimetersPerSecond);
	if (OptionalHitPositionWorld)
		*OptionalHitPositionWorld = BestHitPos;
	if (OptionalBHitProxy)
		*OptionalBHitProxy = true;
	return OutTimeToHitSeconds >= 0.f;
}

void ULamePigeonDemoReplicationComponent::TickContactPredictions30Hz(float DeltaTime)
{
	if (!IsInteractionPredictionEnabled())
		return;
	InteractionPredict30HzAccum += DeltaTime;
	const ULamePigeonSettings* ScanSettings = GetDefault<ULamePigeonSettings>();
	const float               ScanHz =
		ScanSettings ? FMath::Max(1.f, ScanSettings->InteractionPredictScanHz) : LamePigeonDemoConstants::InteractionPredictScanHz;
	const float Step = 1.f / ScanHz;
	while (InteractionPredict30HzAccum >= Step)
	{
		InteractionPredict30HzAccum -= Step;
		RunContactPredictScanStep();
	}
}

bool ULamePigeonDemoReplicationComponent::TrySendDasherPredictionFromScan(ACharacter* LocalCharacter,
	ULamePigeonSubsystem* Subsystem, float CurrentWorldTimeSeconds)
{
	int32 HitPeerId = INDEX_NONE;
	float TimeToHitSeconds = 0.f;
	const float DashDistanceCentimeters = LamePigeonDemoConstants::DashDistanceCentimeters;
	if (!ComputeEarliestDashProxyHit(LocalCharacter, DashDirectionWorld, DashDistanceCentimeters, HitPeerId,
	                                 TimeToHitSeconds) || HitPeerId == INDEX_NONE)
	{
		return false;
	}

	FName PredictedMyTag = PendingDashPredictMyTag;
	FName PredictedTheirTag = PendingDashPredictTheirTag;
	const bool bMissingMyTag = PredictedMyTag.IsNone();
	const bool bMissingTheirTag = PredictedTheirTag.IsNone();
	bool bTagsResolved = true;
	if (bMissingMyTag && bMissingTheirTag)
	{
		bTagsResolved = ULamePigeonSettings::GetPredictTagsForPairIndex(0, false, PredictedMyTag, PredictedTheirTag);
	}
	else if (bMissingMyTag || bMissingTheirTag)
	{
		bTagsResolved = false;
	}

	bool bRateAllowed = true;
	if (const float* LastPredictWorldTimeSeconds = LastDasherPredictWorldTime.Find(HitPeerId))
	{
		if (CurrentWorldTimeSeconds - *LastPredictWorldTimeSeconds
		    < LamePigeonDemoConstants::InteractionPredictMinResendIntervalSeconds)
		{
			bRateAllowed = false;
		}
	}

	const FString MyTagString = PredictedMyTag.ToString();
	const FString TheirTagString = PredictedTheirTag.ToString();
	if (!bTagsResolved || !bRateAllowed || MyTagString.IsEmpty() || TheirTagString.IsEmpty())
		return false;

	const float WireDeltaSeconds = WireDeltaForGeometryTime(TimeToHitSeconds);
	const uint32 PredictEventId = NextInteractionEventId++;
	Subsystem->SendInteractionPredict(static_cast<int32>(PredictEventId), HitPeerId, WireDeltaSeconds, MyTagString,
	                                  TheirTagString);
	StartDasherProxyKnockbackVisual(HitPeerId, LocalCharacter, DashDirectionWorld * 100.f, TimeToHitSeconds,
	                                PredictEventId);
	LastDasherPredictWorldTime.Add(HitPeerId, CurrentWorldTimeSeconds);
	return true;
}

void ULamePigeonDemoReplicationComponent::RemoveOlderPendingVictimPredictions(int32 DasherPeerId)
{
	uint32 NewestOpenEventId = 0;
	for (const TPair<uint32, FPendingKnockbackPrediction>& PendingPair : PendingKnockbacks)
	{
		const FPendingKnockbackPrediction& PendingPrediction = PendingPair.Value;
		if (PendingPrediction.OtherPeerId != DasherPeerId || PendingPrediction.bRejected || PendingPrediction.bApplied)
			continue;
		NewestOpenEventId = FMath::Max(NewestOpenEventId, PendingPair.Key);
	}

	if (NewestOpenEventId == 0)
		return;

	for (auto PendingIterator = PendingKnockbacks.CreateIterator(); PendingIterator; ++PendingIterator)
	{
		FPendingKnockbackPrediction& PendingPrediction = PendingIterator.Value();
		if (PendingPrediction.OtherPeerId != DasherPeerId || PendingPrediction.bRejected || PendingPrediction.bApplied)
			continue;
		if (PendingIterator.Key() != NewestOpenEventId)
			PendingIterator.RemoveCurrent();
	}
}

void ULamePigeonDemoReplicationComponent::RunContactPredictScanStep()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
		return;

	ACharacter* LocalCharacter = Cast<ACharacter>(OwnerPawn);
	if (!LocalCharacter)
		return;

	ULamePigeonSubsystem* Subsystem = LamePigeonSubsystem.Get();
	UWorld*               World     = GetWorld();
	if (!Subsystem || !World || !Subsystem->IsConnected())
		return;

	const float Now = World->GetTimeSeconds();

	if (bDashActive)
		TrySendDasherPredictionFromScan(LocalCharacter, Subsystem, Now);

	if (bDashActive || static_cast<double>(Now) < DashContextHoldUntilWorldSeconds)
		return;

	ACharacter*               Victim            = LocalCharacter;
	const ULamePigeonSettings* Settings       = GetDefault<ULamePigeonSettings>();
	const int32                LocalCtx       = ComputeLocalMovementContext(Victim);
	UCapsuleComponent*         VictimCapsule  = Victim->GetCapsuleComponent();
	if (!VictimCapsule)
		return;

	const FVector VictimLocation      = Victim->GetActorLocation();
	const float   VictimCapsuleRadius = VictimCapsule->GetScaledCapsuleRadius();
	const FVector VictimVelocity      = Victim->GetVelocity();

	for (int32 DasherPeerId : Subsystem->GetProxyPeerIds())
	{
		FVector ProxyDasherPosition;
		FVector ProxyDasherVelocity;
		float   ProxyDasherYawDegrees = 0.f;
		if (!Subsystem->TryGetProxyInterpolatedKinematics(DasherPeerId, ProxyDasherPosition, ProxyDasherVelocity,
		                                                  ProxyDasherYawDegrees))
			continue;

		ACharacter*        ProxyDasherCharacter = Subsystem->GetProxyCharacter(DasherPeerId);
		UCapsuleComponent* DasherProxyCapsule =
			ProxyDasherCharacter ? ProxyDasherCharacter->GetCapsuleComponent() : nullptr;
		const float DasherProxyCapsuleRadius =
			DasherProxyCapsule ? DasherProxyCapsule->GetScaledCapsuleRadius() : 40.f;

		const FVector DeltaXY(ProxyDasherPosition.X - VictimLocation.X, ProxyDasherPosition.Y - VictimLocation.Y, 0.f);
		const float   DistXY = DeltaXY.Size();
		if (DistXY > LamePigeonDemoConstants::ContactPredictionScanRadiusCm)
			continue;

		const int32 RemoteCtx = Subsystem->GetProxyInt(
			DasherPeerId, FString(LamePigeonDemoConstants::InteractionContextVariableName),
			LamePigeonDemoConstants::InteractionContextWalk);
		const bool bRemoteDashing = (RemoteCtx == LamePigeonDemoConstants::InteractionContextDash);

		const int32 PairIdx = ULamePigeonSettings::ResolveInteractionPairIndex(LocalCtx, RemoteCtx);
		if (!Settings || !Settings->InteractionRolePairs.IsValidIndex(PairIdx))
			continue;

		const FName ProfileId = Settings->InteractionRolePairs[PairIdx].InteractionProfileId;

		const float Separated = DistXY - (DasherProxyCapsuleRadius + VictimCapsuleRadius);

		float TimeToHit = 0.f;
		bool  bGeomOk   = false;

		if (PairIdx == 0)
		{
			const float CloseContactSlackCm =
				Settings ? Settings->VictimCloseContactSlackCm : 55.f;
			const float DashHeurMaxDist =
				Settings ? Settings->VictimDashHeuristicMaxDistXY : 900.f;
			const float TowardDotMin = Settings ? Settings->VictimTowardDotMin : 0.2f;
			const float AimDotMin    = Settings ? Settings->VictimAimDotMin : 0.35f;
			const float FastPathDist = Settings ? Settings->VictimFastPathMaxDistXY : 1500.f;
			const float FastPathSep  = Settings ? Settings->VictimFastPathMaxSeparatedCm : 400.f;

			if (DistXY <= KINDA_SMALL_NUMBER)
			{
				if (bRemoteDashing)
				{
					TimeToHit = 0.02f;
					bGeomOk   = true;
				}
				else
					continue;
			}
			else if (bRemoteDashing && Separated <= CloseContactSlackCm)
			{
				TimeToHit = 0.02f;
				bGeomOk   = true;
			}
			else if (Separated <= KINDA_SMALL_NUMBER)
			{
				continue;
			}
			else
			{
				const FVector DirToVictim = DeltaXY / DistXY;
				const FVector ProxyDasherVelocityXY(ProxyDasherVelocity.X, ProxyDasherVelocity.Y, 0.f);
				const FVector VictimVelocityXY(VictimVelocity.X, VictimVelocity.Y, 0.f);
				const float   ClosingSpeed = FVector::DotProduct(ProxyDasherVelocityXY - VictimVelocityXY, DirToVictim);

				if (ClosingSpeed > 50.f)
				{
					TimeToHit = Separated / ClosingSpeed;
					bGeomOk   = TimeToHit > 0.f && TimeToHit <= 0.75f;
				}
				else if (bRemoteDashing)
				{
					const float ProxyDasherSpeedXY = ProxyDasherVelocityXY.Size();
					const float TowardMe =
						ProxyDasherSpeedXY > 10.f
							? FVector::DotProduct(ProxyDasherVelocityXY.GetSafeNormal(), DirToVictim)
							: 0.f;
					if (TowardMe > TowardDotMin && DistXY < DashHeurMaxDist)
					{
						TimeToHit = FMath::Clamp(Separated / FMath::Max(250.f, ProxyDasherSpeedXY * TowardMe), 0.08f, 0.65f);
						bGeomOk   = true;
					}
					if (!bGeomOk && DistXY < DashHeurMaxDist)
					{
						FVector Fwd2D = FRotator(0.f, ProxyDasherYawDegrees, 0.f).Vector();
						Fwd2D.Z       = 0.f;
						if (Fwd2D.Normalize())
						{
							const float AimTowardVictim = FVector::DotProduct(Fwd2D, DirToVictim);
							if (AimTowardVictim > AimDotMin)
							{
								TimeToHit = FMath::Clamp(
									Separated / LamePigeonDemoConstants::DashSpeedCentimetersPerSecond, 0.08f, 0.55f);
								bGeomOk = true;
							}
						}
					}
				}
			}
			if (!bGeomOk && bRemoteDashing && DistXY < FastPathDist && Separated > KINDA_SMALL_NUMBER
			    && Separated < FastPathSep)
			{
				TimeToHit = 0.02f;
				bGeomOk   = true;
			}
		}

		if (!bGeomOk)
			continue;

		FName VictimPredictMyTag;
		FName VictimPredictTheirTag;
		if (!ULamePigeonSettings::GetPredictTagsForPairIndex(PairIdx, true, VictimPredictMyTag, VictimPredictTheirTag))
			continue;
		const FString MyTag    = VictimPredictMyTag.ToString();
		const FString TheirTag = VictimPredictTheirTag.ToString();
		if (MyTag.IsEmpty() || TheirTag.IsEmpty())
			continue;

		RemoveOlderPendingVictimPredictions(DasherPeerId);

		if (const double* VictimCoolEnd = VictimKnockbackCooldownUntilWorldSeconds.Find(DasherPeerId))
		{
			if (static_cast<double>(Now) < *VictimCoolEnd)
				continue;
		}

		if (const float* LastVictimPredictWorldTimeSeconds = LastVictimPredictWorldTime.Find(DasherPeerId))
		{
			if (Now - *LastVictimPredictWorldTimeSeconds
			    < LamePigeonDemoConstants::InteractionPredictMinResendIntervalSeconds)
				continue;
		}

		const float  WireDelta = WireDeltaForGeometryTime(TimeToHit);
		const uint32 EventId = NextInteractionEventId++;
		Subsystem->SendInteractionPredict(static_cast<int32>(EventId), DasherPeerId, WireDelta, MyTag, TheirTag);

		FPendingKnockbackPrediction NewPendingKnockbackPrediction;
		NewPendingKnockbackPrediction.EventId               = EventId;
		NewPendingKnockbackPrediction.OtherPeerId           = DasherPeerId;
		NewPendingKnockbackPrediction.PairIndex             = PairIdx;
		NewPendingKnockbackPrediction.InteractionProfileId  = ProfileId;
		NewPendingKnockbackPrediction.ApplyAtWorldSeconds   = static_cast<double>(Now);
		PendingKnockbacks.Add(EventId, NewPendingKnockbackPrediction);
		LastVictimPredictWorldTime.Add(DasherPeerId, Now);
		OnInteractionPredictCommitted.Broadcast(static_cast<int32>(EventId), DasherPeerId, PairIdx);
	}
}

void ULamePigeonDemoReplicationComponent::TickResolveFinishedKnockbackPredictions(float DeltaTime)
{
	(void)DeltaTime;
	if (!IsInteractionPredictionEnabled())
		return;

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
		return;

	ACharacter* Victim = Cast<ACharacter>(OwnerPawn);
	if (!Victim)
		return;

	UWorld* World = GetWorld();
	if (!World)
		return;

	const double Now = static_cast<double>(World->GetTimeSeconds());

	UCharacterMovementComponent* Move = Victim->GetCharacterMovement();
	const bool                   bOnGround =
		Move && (Move->IsMovingOnGround() || Move->MovementMode == MOVE_Walking || Move->MovementMode == MOVE_NavWalking);
	const float SpeedXY = FVector(Victim->GetVelocity().X, Victim->GetVelocity().Y, 0.f).Size();

	TArray<uint32> ToRemove;
	for (const TPair<uint32, FPendingKnockbackPrediction>& Pair : PendingKnockbacks)
	{
		const FPendingKnockbackPrediction& Rec = Pair.Value;
		if (!Rec.bApplied || Rec.bRejected)
			continue;
		const double Elapsed = Now - Rec.AppliedAtWorldSeconds;
		const bool   bTimeDone =
			Elapsed >= static_cast<double>(LamePigeonDemoConstants::KnockbackPredictResolveAfterAppliedSeconds);
		const bool bVelDone = bOnGround && SpeedXY < 80.f && Elapsed > 0.12;
		if (bTimeDone || bVelDone)
			ToRemove.Add(Pair.Key);
	}

	for (uint32 Key : ToRemove)
	{
		int32 OtherPeer = INDEX_NONE;
		if (const FPendingKnockbackPrediction* Rec = PendingKnockbacks.Find(Key))
			OtherPeer = Rec->OtherPeerId;
		OnInteractionPredictResolved.Broadcast(static_cast<int32>(Key), OtherPeer);
		PendingKnockbacks.Remove(Key);
	}
}

void ULamePigeonDemoReplicationComponent::ApplyKnockbackWithDamage(const FVector& LaunchVelocity, bool bSubtractHealth)
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
		return;

	ACharacter* Character = Cast<ACharacter>(OwnerPawn);
	if (!Character)
		return;

	if (bSubtractHealth)
	{
		if (CurrentHealth <= 0.f)
			return;
		CurrentHealth = FMath::Max(0.f, CurrentHealth - LamePigeonDemoConstants::KnockbackDamagePoints);
		if (ULamePigeonSubsystem* Subsystem = LamePigeonSubsystem.Get())
			Subsystem->SendReplicatedFloat(FString(LamePigeonDemoConstants::HealthVariableName), CurrentHealth);
	}

	Character->LaunchCharacter(LaunchVelocity, true, true);
}

void ULamePigeonDemoReplicationComponent::RollbackKnockback(uint32 EventId)
{
	FPendingKnockbackPrediction* Rec = PendingKnockbacks.Find(EventId);
	if (!Rec || !Rec->bHasRollbackSnapshot)
		return;

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
		return;

	Character->SetActorLocation(Rec->RollbackLocation, false, nullptr, ETeleportType::TeleportPhysics);
	if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
		Move->Velocity = Rec->RollbackVelocity;

	CurrentHealth = Rec->RollbackHealth;
	if (ULamePigeonSubsystem* Subsystem = LamePigeonSubsystem.Get())
		Subsystem->SendReplicatedFloat(FString(LamePigeonDemoConstants::HealthVariableName), CurrentHealth);

	Rec->bHasRollbackSnapshot = false;
}

void ULamePigeonDemoReplicationComponent::TickApplyPendingKnockbacks(float DeltaTime)
{
	(void)DeltaTime;
	if (!IsInteractionPredictionEnabled())
		return;

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
		return;

	ACharacter* Victim = Cast<ACharacter>(OwnerPawn);
	if (!Victim)
		return;

	ULamePigeonSubsystem* Subsystem = LamePigeonSubsystem.Get();
	UWorld*               World     = GetWorld();
	if (!World || !Subsystem)
		return;

	const double               Now      = static_cast<double>(World->GetTimeSeconds());
	const ULamePigeonSettings* Settings = GetDefault<ULamePigeonSettings>();

	for (auto It = PendingKnockbacks.CreateIterator(); It; ++It)
	{
		FPendingKnockbackPrediction& Rec = It.Value();
		const uint32                 Key = It.Key();
		if (!Rec.bApplied && !Rec.bRejected && (Now > Rec.ApplyAtWorldSeconds + 3.0))
		{
			It.RemoveCurrent();
		}
	}

	for (auto It = PendingKnockbacks.CreateIterator(); It; ++It)
	{
		const uint32                 Key = It.Key();
		FPendingKnockbackPrediction& Rec = It.Value();
		if (Rec.bApplied || Rec.bRejected)
			continue;
		if (Now + 1e-4 < Rec.ApplyAtWorldSeconds)
			continue;

		if (const double* CoolEnd = VictimKnockbackCooldownUntilWorldSeconds.Find(Rec.OtherPeerId))
		{
			if (Now < *CoolEnd)
			{
				It.RemoveCurrent();
				continue;
			}
		}

		ACharacter* DasherProxy = Subsystem->GetProxyCharacter(Rec.OtherPeerId);
		FVector     ProxyLoc;
		FVector     ProxyVel;
		float       ProxyYawDegrees = 0.f;
		const bool  bKine = Subsystem->TryGetProxyInterpolatedKinematics(Rec.OtherPeerId, ProxyLoc, ProxyVel, ProxyYawDegrees);

		if (!DasherProxy && !bKine)
			continue;

		float       ProxyRadius = 42.f;
		float       ProxyHalfH  = 88.f;
		FVector     ProxyCenter = DasherProxy ? DasherProxy->GetActorLocation() : ProxyLoc;
		if (DasherProxy)
		{
			if (UCapsuleComponent* ProxyCap = DasherProxy->GetCapsuleComponent())
			{
				ProxyRadius = ProxyCap->GetScaledCapsuleRadius();
				ProxyHalfH  = ProxyCap->GetScaledCapsuleHalfHeight();
			}
		}
		else if (ACharacter* ProxyDims = Subsystem->GetProxyCharacter(Rec.OtherPeerId))
		{
			if (UCapsuleComponent* DimsCap = ProxyDims->GetCapsuleComponent())
			{
				ProxyRadius = DimsCap->GetScaledCapsuleRadius();
				ProxyHalfH  = DimsCap->GetScaledCapsuleHalfHeight();
			}
		}

		FVector LaunchVelocity = FVector::ZeroVector;
		{
			FVector HorizontalDirection = FVector::ZeroVector;
			if (DasherProxy)
			{
				const FVector DashStep = DasherProxy->GetVelocity() * (1.f / 60.f);
				HorizontalDirection    = ComputeKnockbackFlatDirection(DasherProxy, Victim, DashStep, World);
			}
			else
			{
				const FVector DashStepKine = FVector(ProxyVel.X, ProxyVel.Y, 0.f) * (1.f / 60.f);
				HorizontalDirection =
					ComputeKnockbackFlatDirectionVirtualDasher(Victim, ProxyLoc, DashStepKine, World, ProxyRadius, ProxyHalfH);
			}

			if (HorizontalDirection.IsNearlyZero())
			{
				const FVector DasherXY = DasherProxy ? DasherProxy->GetActorLocation() : ProxyLoc;
				const FVector Away(Victim->GetActorLocation() - DasherXY);
				HorizontalDirection = FVector(Away.X, Away.Y, 0.f).GetSafeNormal();
			}
			if (HorizontalDirection.IsNearlyZero())
			{
				const FVector FromYaw = FRotator(0.f, ProxyYawDegrees, 0.f).Vector();
				HorizontalDirection = FVector(FromYaw.X, FromYaw.Y, 0.f).GetSafeNormal();
			}

			LaunchVelocity =
				HorizontalDirection * LamePigeonDemoConstants::KnockbackHorizontalSpeed;
			LaunchVelocity.Z += LamePigeonDemoConstants::KnockbackUpwardComponent;
		}

		Rec.RollbackLocation     = Victim->GetActorLocation();
		Rec.RollbackVelocity     = Victim->GetVelocity();
		Rec.RollbackHealth       = CurrentHealth;
		Rec.bHasRollbackSnapshot = true;

		if (DasherProxy)
		{
			FVector KineLoc;
			FVector KineVel;
			float   KineYaw = 0.f;
			const FVector HorizForNotify = FVector(LaunchVelocity.X, LaunchVelocity.Y, 0.f).GetSafeNormal();
			if (Subsystem->TryGetProxyInterpolatedKinematics(Rec.OtherPeerId, KineLoc, KineVel, KineYaw))
			{
				Subsystem->NotifyVictimPredictedKnockbackContactAlign(Rec.OtherPeerId, Victim, HorizForNotify, KineLoc,
				                                                       FVector(KineVel.X, KineVel.Y, 0.f),
				                                                       static_cast<int32>(Key));
			}
			else
			{
				Subsystem->NotifyVictimPredictedKnockbackContactAlign(
					Rec.OtherPeerId, Victim, HorizForNotify, DasherProxy->GetActorLocation(),
					FVector(DasherProxy->GetVelocity().X, DasherProxy->GetVelocity().Y, 0.f),
					static_cast<int32>(Key));
			}
		}

		ApplyKnockbackWithDamage(LaunchVelocity, true);
		VictimKnockbackCooldownUntilWorldSeconds.Add(
			Rec.OtherPeerId,
			Now + static_cast<double>(LamePigeonDemoConstants::VictimKnockbackPerDasherCooldownSeconds));
		Rec.bApplied              = true;
		Rec.AppliedAtWorldSeconds = Now;

	}
}

void ULamePigeonDemoReplicationComponent::HandleInteractionRejected(int32 YourEventId, int32 Reason)
{
	const uint32 EId = static_cast<uint32>(YourEventId);
	FPendingKnockbackPrediction* Rec = PendingKnockbacks.Find(EId);
	if (!Rec)
	{
		if (int32* DasherProxyPeerId = PendingDasherProxyBouncePeerIds.Find(EId))
		{
			if (ULamePigeonSubsystem* Subsystem = LamePigeonSubsystem.Get())
				Subsystem->CancelPredictedProxyKnockback(*DasherProxyPeerId, static_cast<int32>(EId));
			PendingDasherProxyBouncePeerIds.Remove(EId);
		}

		return;
	}

	const int32 OtherPeer = Rec->OtherPeerId;

	if (Rec->bApplied)
	{
		PendingKnockbacks.Remove(EId);
		return;
	}

	OnInteractionPredictRejected.Broadcast(YourEventId, OtherPeer, Reason);
	PendingKnockbacks.Remove(EId);
}

