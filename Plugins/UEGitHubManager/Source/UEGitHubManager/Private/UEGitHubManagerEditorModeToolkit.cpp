// Copyright Epic Games, Inc. All Rights Reserved.

#include "UEGitHubManagerEditorModeToolkit.h"
#include "UEGitHubManagerEditorMode.h"
#include "Engine/Selection.h"

#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "EditorModeManager.h"

#define LOCTEXT_NAMESPACE "UEGitHubManagerEditorModeToolkit"

FUEGitHubManagerEditorModeToolkit::FUEGitHubManagerEditorModeToolkit()
{
}

void FUEGitHubManagerEditorModeToolkit::Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode)
{
	FModeToolkit::Init(InitToolkitHost, InOwningMode);
}

void FUEGitHubManagerEditorModeToolkit::GetToolPaletteNames(TArray<FName>& PaletteNames) const
{
	PaletteNames.Add(NAME_Default);
}


FName FUEGitHubManagerEditorModeToolkit::GetToolkitFName() const
{
	return FName("UEGitHubManagerEditorMode");
}

FText FUEGitHubManagerEditorModeToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("DisplayName", "UEGitHubManagerEditorMode Toolkit");
}

#undef LOCTEXT_NAMESPACE
