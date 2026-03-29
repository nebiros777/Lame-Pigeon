// Source file name: LamePigeonGameRpcHandler.h
// Author: Igor Matiushin
// Brief description: Declares the gameplay RPC handler interface consumed by the plugin subsystem.

#pragma once

#include "UObject/Interface.h"
#include "LamePigeonGameRpcHandler.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class ULamePigeonGameRpcHandler : public UInterface
{
	GENERATED_BODY()
};

class LAMEPIGEON_API ILamePigeonGameRpcHandler
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category = "LamePigeon|RPC")
	void HandleLamePigeonGameRpc(int32 SenderPeerId, const FString& FuncName, const TArray<float>& FloatArgs);
};
