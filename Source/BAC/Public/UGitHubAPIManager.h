// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Http.h" // Unreal HTTP-Modul
#include "UGitHubAPIManager.generated.h"

/**
 * 
 */
UCLASS(Blueprintable)
class BAC_API UGitHubAPIManager : public UObject
{
	GENERATED_BODY()

public:
	UGitHubAPIManager();

	UFUNCTION(BlueprintCallable, Category="GitHub API")
	void Authenticate(const FString& AuthToken);

	UFUNCTION(BlueprintCallable, Category="GitHub API")
	void GetProjects();

	UFUNCTION(BlueprintCallable, Category = "GitHub API")
	void StartGitHubIntegration(const FString& UserAccessToken);

	UFUNCTION(BlueprintCallable, Category = "GitHub API")
	static UGitHubAPIManager* GetInstance();

private:
	FHttpModule* Http;
	FString AccessToken;
	static UGitHubAPIManager* SingletonInstance;

	void OnAuthResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
};
