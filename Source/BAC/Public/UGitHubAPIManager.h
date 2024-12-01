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
struct FProjectItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	FString ItemId;

	UPROPERTY(BlueprintReadWrite)
	FString Title;

	UPROPERTY(BlueprintReadWrite)
	FString Url;

	UPROPERTY(BlueprintReadWrite)
	FString Type; // "Issue", "PullRequest", "DraftIssue"

	UPROPERTY(BlueprintReadWrite)
	FString State; // "OPEN", "CLOSED", "MERGED", "DRAFT", etc.

	UPROPERTY(BlueprintReadWrite)
	FString CreatedAt;

	UPROPERTY(BlueprintReadWrite)
	FString Body;

	UPROPERTY(BlueprintReadWrite)
	FString ColumnId;

	UPROPERTY(BlueprintReadWrite)
	FString ColumnName;
};

USTRUCT(BlueprintType)
struct FProjectInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	FString ProjectId;

	UPROPERTY(BlueprintReadWrite)
	FString ProjectTitle;

	UPROPERTY(BlueprintReadWrite)
	FString ProjectDescription;

	UPROPERTY(BlueprintReadWrite)
	FString ProjectURL;

	UPROPERTY(BlueprintReadWrite)
	TArray<FProjectItem> Items;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRepositoriesLoaded, const TArray<FRepositoryInfo>&, Repositories);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRepositoryDetailsLoaded, const FRepositoryInfo&, RepositoryInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnProjectCreated, const FString&, ProjectUrl);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUserProjectsLoaded, const TArray<FProjectInfo>&, Projects);
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
	void FetchUserRepositories();

	UFUNCTION(BlueprintCallable, Category = "GitHub API")
	TArray<FRepositoryInfo> GetRepositoryList();

	UFUNCTION(BlueprintCallable, Category = "GitHub API")
	void FetchRepositoryDetails(const FString& RepositoryName);

	UPROPERTY(BlueprintAssignable, Category = "GitHub API")
	FOnRepositoriesLoaded OnRepositoriesLoaded;

	UPROPERTY(BlueprintAssignable, Category = "GitHub API")
	FOnRepositoryDetailsLoaded OnRepositoryDetailsLoaded;

	UPROPERTY(BlueprintAssignable, Category = "GitHub API")
	FOnProjectCreated OnProjectCreated;

	UFUNCTION(BlueprintCallable, Category = "GitHub API")
	void CreateNewProject(const FString& Owner, const FString& RepositoryName, const FString& ProjectName);

	UFUNCTION(BlueprintCallable, Category = "GitHub API")
	void FetchUserProjects();

	UFUNCTION(BlueprintCallable, Category = "GitHub API")
	void FetchProjectDetails(const FString& ProjectName);

	UPROPERTY(BlueprintAssignable, Category = "GitHub API")
	FOnUserProjectsLoaded OnUserProjectsLoaded;

	UPROPERTY(BlueprintAssignable, Category = "GitHub API")
	FOnProjectDetailsLoaded OnProjectDetailsLoaded;

private:
	FHttpModule* Http;
	FString AccessToken;
	static UGitHubAPIManager* SingletonInstance;
	TMap<FString, FRepositoryInfo> RepositoryInfos;
	TMap<FString, FProjectInfo> UserProjects;
	FRepositoryInfo ActiveRepository;

	void LogHttpError(FHttpResponsePtr Response) const;
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> CreateHttpRequest(const FString& URL, const FString& Verb);

	void HandleRepoListResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void HandleRepoDetailsResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void HandleFetchUserProjectsResponse(TSharedPtr<FJsonObject> ResponseObject);
	void HandleFetchProjectDetailsResponse(TSharedPtr<FJsonObject> ResponseObject, const FString& ProjectName);

	void SendGraphQLQuery(const FString& Query, const TFunction<void(TSharedPtr<FJsonObject>)>& Callback);

	FString GetStringFieldSafe(TSharedPtr<FJsonObject> JsonObject, const FString& FieldName);
	TOptional<int32> GetIntegerFieldSafe(TSharedPtr<FJsonObject> JsonObject, const FString& FieldName);
};