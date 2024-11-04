// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Http.h" // Unreal HTTP-Modul
#include "UGitHubAPIManager.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct FRepositoryInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	FString RepoName;

	UPROPERTY(BlueprintReadWrite)
	FString Owner;
};

USTRUCT(BlueprintType)
struct FSelectedRepositoryInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString RepoName;

	UPROPERTY(BlueprintReadOnly)
	FString RepoDescription;

	UPROPERTY(BlueprintReadOnly)
	FString RepoCreatedAt;
};

UCLASS(Blueprintable)
class BAC_API UGitHubAPIManager : public UObject
{
	GENERATED_BODY()

public:
	UGitHubAPIManager();

	UFUNCTION(BlueprintCallable, Category="GitHub API")
	void Authenticate(const FString& AuthToken);

	UFUNCTION(BlueprintCallable, Category = "GitHub API")
	void StartGitHubIntegration(const FString& UserAccessToken);

	UFUNCTION(BlueprintCallable, Category = "GitHub API")
	static UGitHubAPIManager* GetInstance();

	UFUNCTION(BlueprintCallable, Category = "GitHub API")
	TArray<FRepositoryInfo> GetRepositoryList();

	UFUNCTION(BlueprintCallable, Category = "GitHub API")
	FSelectedRepositoryInfo GetSelectedRepositoryInfo();

	UFUNCTION(BlueprintCallable, Category="GitHub API")
	void GetRepositoryDetails(const FString& Owner, const FString& RepoName);

	UFUNCTION(BlueprintCallable, Category="GitHub API")
	void ShowSelectedRepo(const FString& RepoName);

private:
	FHttpModule* Http;
	FString AccessToken;
	static UGitHubAPIManager* SingletonInstance;
	TArray<FRepositoryInfo> RepositoryInfos;
	FSelectedRepositoryInfo SelectedRepositoryInfo;

	void OnReposResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void OnRepoDetailsResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
};
