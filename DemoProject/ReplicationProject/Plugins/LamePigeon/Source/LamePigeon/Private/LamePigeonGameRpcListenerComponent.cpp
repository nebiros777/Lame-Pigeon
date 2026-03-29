// Source file name: LamePigeonGameRpcListenerComponent.cpp
// Author: Igor Matiushin
// Brief description: Implements component-based gameplay RPC listeners for the plugin subsystem.

#include "LamePigeonGameRpcListenerComponent.h"
#include "LamePigeonSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

ULamePigeonGameRpcListenerComponent::ULamePigeonGameRpcListenerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void ULamePigeonGameRpcListenerComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (ULamePigeonSubsystem* Subsys = GI->GetSubsystem<ULamePigeonSubsystem>())
				Subsys->RegisterGameplayRpcComponent(this);
		}
	}
}

void ULamePigeonGameRpcListenerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (ULamePigeonSubsystem* Subsys = GI->GetSubsystem<ULamePigeonSubsystem>())
				Subsys->UnregisterGameplayRpcComponent(this);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void ULamePigeonGameRpcListenerComponent::ReceiveGameRpc_Implementation(int32 SenderPeerId, const FString& FuncName,
                                                                        const TArray<float>& FloatArgs)
{
}
