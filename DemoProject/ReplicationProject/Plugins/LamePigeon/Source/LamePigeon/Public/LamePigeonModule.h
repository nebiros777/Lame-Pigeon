// Source file name: LamePigeonModule.h
// Author: Igor Matiushin
// Brief description: Declares the Unreal plugin module interface for LamePigeon.

#pragma once
#include "Modules/ModuleManager.h"

class FLamePigeonModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
