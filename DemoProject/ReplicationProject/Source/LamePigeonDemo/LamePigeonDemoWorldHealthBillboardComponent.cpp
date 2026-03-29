// Source file name: LamePigeonDemoWorldHealthBillboardComponent.cpp
// Author: Igor Matiushin
// Brief description: Implements billboard placement for remote health widgets in the demo.

#include "LamePigeonDemoWorldHealthBillboardComponent.h"
#include "LamePigeonDemoConstants.h"
#include "LamePigeonDemoReplicationComponent.h"
#include "LamePigeonDemoWorldHealthWidget.h"
#include "LamePigeonSubsystem.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

ULamePigeonDemoWorldHealthBillboardComponent::ULamePigeonDemoWorldHealthBillboardComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	SetWidgetSpace(EWidgetSpace::World);
	SetDrawSize(FVector2D(100.f, 10.f));
	SetPivot(FVector2D(0.5f, 0.5f));
	SetTwoSided(true);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);

	MaxHealthForBar = LamePigeonDemoConstants::InitialHealthPoints;
}

void ULamePigeonDemoWorldHealthBillboardComponent::BeginPlay()
{
	const TSubclassOf<UUserWidget> WidgetClassPtr =
		HealthWidgetOverride ? HealthWidgetOverride
		                     : TSubclassOf<UUserWidget>(ULamePigeonDemoWorldHealthWidget::StaticClass());
	SetWidgetClass(WidgetClassPtr);
	SetDrawSize(FVector2D(100.f, 10.f));

	Super::BeginPlay();

	SetRelativeLocation(BarOffsetFromCapsule);

	if (AActor* Owner = GetOwner())
	{
		if (UGameInstance* GI = Owner->GetGameInstance())
		{
			if (ULamePigeonSubsystem* Sub = GI->GetSubsystem<ULamePigeonSubsystem>())
			{
				CachedSubsystem = Sub;
				Sub->OnProxyReplicatedFloatUpdated.AddDynamic(
					this, &ULamePigeonDemoWorldHealthBillboardComponent::HandleProxyFloatUpdated);
			}
		}
	}
}

void ULamePigeonDemoWorldHealthBillboardComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ULamePigeonSubsystem* Sub = CachedSubsystem.Get())
		Sub->OnProxyReplicatedFloatUpdated.RemoveDynamic(
			this, &ULamePigeonDemoWorldHealthBillboardComponent::HandleProxyFloatUpdated);
	CachedSubsystem.Reset();
	Super::EndPlay(EndPlayReason);
}

void ULamePigeonDemoWorldHealthBillboardComponent::HandleProxyFloatUpdated(int32 PeerId, FString VarName)
{
	if (VarName != FString(LamePigeonDemoConstants::HealthVariableName))
		return;

	AActor* Owner = GetOwner();
	if (!Owner)
		return;

	APawn* Pawn = Cast<APawn>(Owner);
	if (!Pawn || Pawn->IsLocallyControlled())
		return;

	ACharacter* Ch = Cast<ACharacter>(Pawn);
	if (!Ch)
		return;

	if (UGameInstance* GI = Owner->GetGameInstance())
	{
		if (ULamePigeonSubsystem* Sub = GI->GetSubsystem<ULamePigeonSubsystem>())
		{
			int32 MyPeerId = INDEX_NONE;
			if (Sub->TryGetPeerIdForProxyActor(Ch, MyPeerId) && MyPeerId == PeerId)
				RefreshHealthVisual();
		}
	}
}

void ULamePigeonDemoWorldHealthBillboardComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                                 FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	FaceLocalViewCamera();
	RefreshHealthVisual();
}

void ULamePigeonDemoWorldHealthBillboardComponent::FaceLocalViewCamera()
{
	UWorld* World = GetWorld();
	if (!World)
		return;

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager)
		return;

	const FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
	const FVector Here           = GetComponentLocation();
	FVector       ToCamera       = CameraLocation - Here;
	if (!ToCamera.Normalize())
		return;
	SetWorldRotation(ToCamera.Rotation());
}

void ULamePigeonDemoWorldHealthBillboardComponent::RefreshHealthVisual()
{
	UUserWidget* UserW = GetWidget();
	auto*        HW    = Cast<ULamePigeonDemoWorldHealthWidget>(UserW);
	if (!HW)
		return;

	AActor* Owner = GetOwner();
	if (!Owner)
		return;

	const float MaxH = FMath::Max(1.f, MaxHealthForBar);
	float       CurrentHealth = LamePigeonDemoConstants::InitialHealthPoints;

	if (APawn* Pawn = Cast<APawn>(Owner))
	{
		if (Pawn->IsLocallyControlled())
		{
			if (ULamePigeonDemoReplicationComponent* Rep =
			        Pawn->FindComponentByClass<ULamePigeonDemoReplicationComponent>())
				CurrentHealth = Rep->GetCurrentHealth();
		}
		else if (ACharacter* Ch = Cast<ACharacter>(Pawn))
		{
			if (UGameInstance* GI = Pawn->GetGameInstance())
			{
				if (ULamePigeonSubsystem* Sub = GI->GetSubsystem<ULamePigeonSubsystem>())
				{
					int32 PeerId = INDEX_NONE;
					if (Sub->TryGetPeerIdForProxyActor(Ch, PeerId))
						CurrentHealth = Sub->GetProxyFloat(PeerId, FString(LamePigeonDemoConstants::HealthVariableName),
						                                   MaxH);
				}
			}
		}
	}

	HW->SetHealthFillRatio(CurrentHealth / MaxH);
}
