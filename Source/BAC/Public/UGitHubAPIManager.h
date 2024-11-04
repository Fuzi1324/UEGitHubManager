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
	FString RepositoryName;

	UPROPERTY(BlueprintReadWrite)
	FString Owner;

	// Blueprint-kompatible Felder für optionale Daten
	UPROPERTY(BlueprintReadWrite)
	FString Description;

	UPROPERTY(BlueprintReadWrite)
	FString CreatedAt;

	UPROPERTY(BlueprintReadWrite)
	int32 Stars = 0;

	UPROPERTY(BlueprintReadWrite)
	int32 Forks = 0;
};

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

	UFUNCTION(BlueprintCallable, Category = "GitHub API")
	FRepositoryInfo GetActiveRepositoryInfo();

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

	FString GetStringFieldSafe(TSharedPtr<FJsonObject> JsonObject, const FString& FieldName);
	TOptional<int32> GetIntegerFieldSafe(TSharedPtr<FJsonObject> JsonObject, const FString& FieldName);
};