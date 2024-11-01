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

	UFUNCTION(BlueprintCallable, Category = "GitHub API")
	void GetRepositories();

	UFUNCTION(BlueprintCallable, Category = "GitHub API")
	TArray<FString> GetRepositoryList();

private:
	FHttpModule* Http;
	FString AccessToken;
	static UGitHubAPIManager* SingletonInstance;
	TArray<FString> RepositoryNames;

	void OnAuthResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void OnReposResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
};
