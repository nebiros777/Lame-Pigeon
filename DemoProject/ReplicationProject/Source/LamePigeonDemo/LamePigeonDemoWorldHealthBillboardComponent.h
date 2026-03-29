// Source file name: LamePigeonDemoWorldHealthBillboardComponent.h
// Author: Igor Matiushin
// Brief description: Declares billboard support for remote health widgets in the demo.

#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "LamePigeonDemoWorldHealthBillboardComponent.generated.h"

class ULamePigeonSubsystem;

/**
 * World-space widget that faces the local player's camera each tick (billboard).
 * Reads health from ULamePigeonDemoReplicationComponent (local pawn) or ULamePigeonSubsystem proxy floats (remote squabs).
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LAMEPIGEONDEMO_API ULamePigeonDemoWorldHealthBillboardComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	ULamePigeonDemoWorldHealthBillboardComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	/** If set, used instead of the default C++ widget (subclass ULamePigeonDemoWorldHealthWidget for SetHealthFillRatio). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LamePigeonDemo|UI")
	TSubclassOf<class UUserWidget> HealthWidgetOverride;

	/** Denominator for the progress bar (should match gameplay max health). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LamePigeonDemo|UI", meta = (ClampMin = "1.0"))
	float MaxHealthForBar = 99.f;

protected:
	void FaceLocalViewCamera();
	void RefreshHealthVisual();

	UFUNCTION()
	void HandleProxyFloatUpdated(int32 PeerId, FString VarName);

	UPROPERTY(EditAnywhere, Category = "LamePigeonDemo|UI")
	FVector BarOffsetFromCapsule = FVector(0.f, 0.f, 110.f);

	TWeakObjectPtr<ULamePigeonSubsystem> CachedSubsystem;
};
