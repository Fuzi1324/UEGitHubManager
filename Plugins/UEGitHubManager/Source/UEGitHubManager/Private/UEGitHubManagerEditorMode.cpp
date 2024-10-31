// Copyright Epic Games, Inc. All Rights Reserved.

#include "UEGitHubManagerEditorMode.h"
#include "UEGitHubManagerEditorModeToolkit.h"
#include "EdModeInteractiveToolsContext.h"
#include "InteractiveToolManager.h"
#include "UEGitHubManagerEditorModeCommands.h"


//////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////// 
// AddYourTool Step 1 - include the header file for your Tools here
//////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////// 
#include "Tools/UEGitHubManagerSimpleTool.h"
#include "Tools/UEGitHubManagerInteractiveTool.h"

// step 2: register a ToolBuilder in FUEGitHubManagerEditorMode::Enter() below


#define LOCTEXT_NAMESPACE "UEGitHubManagerEditorMode"

const FEditorModeID UUEGitHubManagerEditorMode::EM_UEGitHubManagerEditorModeId = TEXT("EM_UEGitHubManagerEditorMode");

FString UUEGitHubManagerEditorMode::SimpleToolName = TEXT("UEGitHubManager_ActorInfoTool");
FString UUEGitHubManagerEditorMode::InteractiveToolName = TEXT("UEGitHubManager_MeasureDistanceTool");


UUEGitHubManagerEditorMode::UUEGitHubManagerEditorMode()
{
	FModuleManager::Get().LoadModule("EditorStyle");

	// appearance and icon in the editing mode ribbon can be customized here
	Info = FEditorModeInfo(UUEGitHubManagerEditorMode::EM_UEGitHubManagerEditorModeId,
		LOCTEXT("ModeName", "UEGitHubManager"),
		FSlateIcon(),
		true);
}


UUEGitHubManagerEditorMode::~UUEGitHubManagerEditorMode()
{
}


void UUEGitHubManagerEditorMode::ActorSelectionChangeNotify()
{
}

void UUEGitHubManagerEditorMode::Enter()
{
	UEdMode::Enter();

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	// AddYourTool Step 2 - register the ToolBuilders for your Tools here.
	// The string name you pass to the ToolManager is used to select/activate your ToolBuilder later.
	//////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////// 
	const FUEGitHubManagerEditorModeCommands& SampleToolCommands = FUEGitHubManagerEditorModeCommands::Get();

	RegisterTool(SampleToolCommands.SimpleTool, SimpleToolName, NewObject<UUEGitHubManagerSimpleToolBuilder>(this));
	RegisterTool(SampleToolCommands.InteractiveTool, InteractiveToolName, NewObject<UUEGitHubManagerInteractiveToolBuilder>(this));

	// active tool type is not relevant here, we just set to default
	GetToolManager()->SelectActiveToolType(EToolSide::Left, SimpleToolName);
}

void UUEGitHubManagerEditorMode::CreateToolkit()
{
	Toolkit = MakeShareable(new FUEGitHubManagerEditorModeToolkit);
}

TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> UUEGitHubManagerEditorMode::GetModeCommands() const
{
	return FUEGitHubManagerEditorModeCommands::Get().GetCommands();
}

#undef LOCTEXT_NAMESPACE
