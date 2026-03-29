// Source file name: LamePigeonGameRpcListenerComponent.h
// Author: Igor Matiushin
// Brief description: Declares component-based gameplay RPC listeners for the LamePigeon plugin.

#pragma once

#include "Components/ActorComponent.h"
#include "LamePigeonGameRpcListenerComponent.generated.h"

class ULamePigeonSubsystem;

UCLASS(ClassGroup = (LamePigeon), meta = (BlueprintSpawnableComponent))
class LAMEPIGEON_API ULamePigeonGameRpcListenerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULamePigeonGameRpcListenerComponent();

	UFUNCTION(BlueprintNativeEvent, Category = "LamePigeon|RPC")
	void ReceiveGameRpc(int32 SenderPeerId, const FString& FuncName, const TArray<float>& FloatArgs);
	virtual void ReceiveGameRpc_Implementation(int32 SenderPeerId, const FString& FuncName,
	                                           const TArray<float>& FloatArgs);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
