// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Http.h"
#include "UGitHubAPIManager.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct FRepositoryInfo
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadWrite)
	FString RepositoryName;

	UPROPERTY(BlueprintReadWrite)
	FString Owner;

	UPROPERTY(BlueprintReadWrite)
	FString Description;

	UPROPERTY(BlueprintReadWrite)
	FString CreatedAt;

	UPROPERTY(BlueprintReadWrite)
	int32 Stars = 0;

	UPROPERTY(BlueprintReadWrite)
	int32 Forks = 0;
};

USTRUCT(BlueprintType)
struct FProjectInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	int32 Id;

	UPROPERTY(BlueprintReadWrite)
	FString Name;

	UPROPERTY(BlueprintReadWrite)
	FString Body;

	UPROPERTY(BlueprintReadWrite)
	FString State;

	UPROPERTY(BlueprintReadWrite)
	FString HtmlUrl;
}; 

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRepositoriesLoaded, const TArray<FRepositoryInfo>&, Repositories);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRepositoryDetailsLoaded, const FRepositoryInfo&, RepositoryInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnProjectCreated, const FString&, ProjectUrl);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnProjectDetailsLoaded, const FProjectInfo&, ProjectInfo);

UCLASS(Blueprintable)
class BAC_API UGitHubAPIManager : public UObject
{
	GENERATED_BODY()

public:
	UGitHubAPIManager();

	UFUNCTION(BlueprintCallable, Category = "GitHub API")
	void SetAccessToken(const FString& AuthToken);

	UFUNCTION(BlueprintCallable, Category = "GitHub API")
	void InitializeIntegration(const FString& UserAccessToken);

	UFUNCTION(BlueprintCallable, Category = "GitHub API")
	static UGitHubAPIManager* GetInstance();

	UFUNCTION(BlueprintCallable, Category = "GitHub API")
	TArray<FRepositoryInfo> GetRepositoryList();

	UFUNCTION(BlueprintCallable, Category = "GitHub API")
	void FetchRepositoryDetails(const FString& RepositoryName);

	UPROPERTY(BlueprintAssignable, Category = "GitHub API")
	FOnRepositoriesLoaded OnRepositoriesLoaded;

	UPROPERTY(BlueprintAssignable, Category = "GitHub API")
	FOnRepositoryDetailsLoaded OnRepositoryDetailsLoaded;

	UFUNCTION(BlueprintCallable, Category = "GitHub API")
	void CreateRepositoryProject(const FString& RepositoryName, const FString& ProjectName, const FString& ProjectBody);

	UPROPERTY(BlueprintAssignable, Category = "GitHub API")
	FOnProjectCreated OnProjectCreated;

	UFUNCTION(BlueprintCallable, Category = "GitHub API")
	void FetchProjectDetails(int32 ProjectId);

	UPROPERTY(BlueprintAssignable, Category = "GitHub API")
	FOnProjectDetailsLoaded OnProjectDetailsLoaded;

private:
	FHttpModule* Http;
	FString AccessToken;
	static UGitHubAPIManager* SingletonInstance;
	TMap<FString, FRepositoryInfo> RepositoryInfos;
	FRepositoryInfo ActiveRepository;

	void LogHttpError(FHttpResponsePtr Response) const;
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> CreateHttpRequest(const FString& URL, const FString& Verb);

	void HandleRepoListResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void HandleRepoDetailsResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void HandleCreateProjectResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void HandleFetchProjectDetailsResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	FString GetStringFieldSafe(TSharedPtr<FJsonObject> JsonObject, const FString& FieldName);
	TOptional<int32> GetIntegerFieldSafe(TSharedPtr<FJsonObject> JsonObject, const FString& FieldName);
};