// Source file name: LamePigeonRpcLibrary.h
// Author: Igor Matiushin
// Brief description: Declares Blueprint-callable RPC helpers for the LamePigeon plugin.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "LamePigeonRpcLibrary.generated.h"

UCLASS()
class LAMEPIGEON_API ULamePigeonRpcLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "LamePigeon|RPC")
	static FString RpcNamespaceChat();

	UFUNCTION(BlueprintPure, Category = "LamePigeon|RPC")
	static FString RpcNamespaceEmote();

	UFUNCTION(BlueprintPure, Category = "LamePigeon|RPC")
	static FString RpcNamespaceGameEvent();

	UFUNCTION(BlueprintPure, Category = "LamePigeon|RPC")
	static FString RpcNamespaceRole();

	UFUNCTION(BlueprintPure, Category = "LamePigeon|RPC")
	static FString RpcBuildName(const FString& NamespacePrefix, const FString& Suffix);

	UFUNCTION(BlueprintPure, Category = "LamePigeon|RPC")
	static TArray<float> MakeRpcFloatArgs();

	UFUNCTION(BlueprintPure, Category = "LamePigeon|RPC")
	static TArray<float> MakeRpcFloatArgs1(float A);

	UFUNCTION(BlueprintPure, Category = "LamePigeon|RPC")
	static TArray<float> MakeRpcFloatArgs2(float A, float B);

	UFUNCTION(BlueprintPure, Category = "LamePigeon|RPC")
	static TArray<float> MakeRpcFloatArgs3(float A, float B, float C);

	UFUNCTION(BlueprintPure, Category = "LamePigeon|RPC")
	static TArray<float> MakeRpcFloatArgs4(float A, float B, float C, float D);

	UFUNCTION(BlueprintPure, Category = "LamePigeon|RPC")
	static TArray<float> MakeRpcFloatArgs5(float A, float B, float C, float D, float E);
};
