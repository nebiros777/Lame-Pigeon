// Source file name: LamePigeonDemoPlayerController.h
// Author: Igor Matiushin
// Brief description: Declares the demo player controller and local HUD setup hooks.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LamePigeonDemoPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;

UCLASS(abstract)
class ALamePigeonDemoPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	TObjectPtr<UUserWidget> MobileControlsWidget;

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
};
