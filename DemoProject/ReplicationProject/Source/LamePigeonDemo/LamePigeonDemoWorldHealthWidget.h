// Source file name: LamePigeonDemoWorldHealthWidget.h
// Author: Igor Matiushin
// Brief description: Declares the world-space health widget for replicated demo characters.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ProgressBar.h"
#include "LamePigeonDemoWorldHealthWidget.generated.h"

/**
 * Base world-space health bar. Create a Blueprint child, add a named Progress Bar widget
 * "HealthProgressBar" (optional — if missing, SetHealthFillRatio is a no-op for the bar).
 */
UCLASS()
class LAMEPIGEONDEMO_API ULamePigeonDemoWorldHealthWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 0..1 fill for the progress bar. */
	UFUNCTION(BlueprintCallable, Category="LamePigeonDemo|UI")
	void SetHealthFillRatio(float Ratio01);

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	UProgressBar* HealthProgressBar = nullptr;
};
