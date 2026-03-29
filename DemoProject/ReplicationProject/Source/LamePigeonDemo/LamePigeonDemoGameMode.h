// Source file name: LamePigeonDemoGameMode.h
// Author: Igor Matiushin
// Brief description: Declares the demo game mode class used by the sample project.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LamePigeonDemoGameMode.generated.h"

UCLASS(abstract)
class ALamePigeonDemoGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ALamePigeonDemoGameMode();
};
