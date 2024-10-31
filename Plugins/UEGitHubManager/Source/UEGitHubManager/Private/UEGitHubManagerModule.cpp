// Copyright Epic Games, Inc. All Rights Reserved.

#include "UEGitHubManagerModule.h"
#include "UEGitHubManagerEditorModeCommands.h"

#define LOCTEXT_NAMESPACE "UEGitHubManagerModule"

void FUEGitHubManagerModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	FUEGitHubManagerEditorModeCommands::Register();
}

void FUEGitHubManagerModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	FUEGitHubManagerEditorModeCommands::Unregister();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUEGitHubManagerModule, UEGitHubManagerEditorMode)