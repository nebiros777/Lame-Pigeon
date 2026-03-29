// Source file name: LamePigeonModule.cpp
// Author: Igor Matiushin
// Brief description: Implements the Unreal plugin module startup and shutdown entry points.

#include "LamePigeonModule.h"

#define LOCTEXT_NAMESPACE "FLamePigeonModule"

void FLamePigeonModule::StartupModule() {}
void FLamePigeonModule::ShutdownModule() {}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FLamePigeonModule, LamePigeon)
