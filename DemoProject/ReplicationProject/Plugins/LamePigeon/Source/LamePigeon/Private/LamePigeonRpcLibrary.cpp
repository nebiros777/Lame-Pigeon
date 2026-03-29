// Source file name: LamePigeonRpcLibrary.cpp
// Author: Igor Matiushin
// Brief description: Implements Blueprint-callable RPC forwarding helpers for the plugin subsystem.

#include "LamePigeonRpcLibrary.h"

FString ULamePigeonRpcLibrary::RpcNamespaceChat()
{
	return TEXT("Chat.");
}

FString ULamePigeonRpcLibrary::RpcNamespaceEmote()
{
	return TEXT("Emote.");
}

FString ULamePigeonRpcLibrary::RpcNamespaceGameEvent()
{
	return TEXT("GameEvent.");
}

FString ULamePigeonRpcLibrary::RpcNamespaceRole()
{
	return TEXT("Role.");
}

FString ULamePigeonRpcLibrary::RpcBuildName(const FString& NamespacePrefix, const FString& Suffix)
{
	return NamespacePrefix + Suffix;
}

TArray<float> ULamePigeonRpcLibrary::MakeRpcFloatArgs()
{
	return TArray<float>();
}

TArray<float> ULamePigeonRpcLibrary::MakeRpcFloatArgs1(float A)
{
	return TArray<float>{A};
}

TArray<float> ULamePigeonRpcLibrary::MakeRpcFloatArgs2(float A, float B)
{
	return TArray<float>{A, B};
}

TArray<float> ULamePigeonRpcLibrary::MakeRpcFloatArgs3(float A, float B, float C)
{
	return TArray<float>{A, B, C};
}

TArray<float> ULamePigeonRpcLibrary::MakeRpcFloatArgs4(float A, float B, float C, float D)
{
	return TArray<float>{A, B, C, D};
}

TArray<float> ULamePigeonRpcLibrary::MakeRpcFloatArgs5(float A, float B, float C, float D, float E)
{
	return TArray<float>{A, B, C, D, E};
}
