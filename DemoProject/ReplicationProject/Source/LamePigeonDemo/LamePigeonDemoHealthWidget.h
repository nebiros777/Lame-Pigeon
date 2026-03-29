// Source file name: LamePigeonDemoHealthWidget.h
// Author: Igor Matiushin
// Brief description: Declares the local HUD health widget used by the demo pawn.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "LamePigeonDemoHealthWidget.generated.h"

UCLASS()
class LAMEPIGEONDEMO_API ULamePigeonDemoHealthWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
};
