// Fill out your copyright notice in the Description page of Project Settings.


#include "UGitHubAPIManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Blueprint/UserWidget.h"

UGitHubAPIManager* UGitHubAPIManager::SingletonInstance = nullptr;

UGitHubAPIManager* UGitHubAPIManager::GetInstance()
{
    if (!SingletonInstance)
    {
        SingletonInstance = NewObject<UGitHubAPIManager>();
        SingletonInstance->AddToRoot();
    }
    return SingletonInstance;
}

UGitHubAPIManager::UGitHubAPIManager()
{
    Http = &FHttpModule::Get();
}

void UGitHubAPIManager::StartGitHubIntegration(const FString& UserAccessToken)
{
    UE_LOG(LogTemp, Log, TEXT("User Access Token: %s"), *UserAccessToken);

    Authenticate(UserAccessToken);
}

void UGitHubAPIManager::Authenticate(const FString& AuthToken)
{
    // Speichere den AccessToken in der Instanzvariable
    AccessToken = AuthToken;

    if (AccessToken.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Access Token is empty. Cannot retrieve repositories."));
        return;
    }

    // Richte die HTTP-Anfrage für GitHub ein
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();
    //Request->OnProcessRequestComplete().BindUObject(this, &UGitHubAPIManager::OnAuthResponseReceived);
    //Request->SetURL("https://api.github.com/user");
    Request->OnProcessRequestComplete().BindUObject(this, &UGitHubAPIManager::OnReposResponseReceived);
    Request->SetURL("https://api.github.com/user/repos"); // GitHub API für Repositories des Benutzers
    Request->SetVerb("GET");

    // Authentifizierungs-Header
    Request->SetHeader("Authorization", "Bearer " + AccessToken);
    Request->SetHeader("User-Agent", "UEGitHubManager");

    // Anfrage abschicken
    Request->ProcessRequest();
}

void UGitHubAPIManager::OnReposResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (bWasSuccessful && Response->GetResponseCode() == 200)
    {
        UE_LOG(LogTemp, Log, TEXT("Repositories retrieved successfully!"));

        TArray<TSharedPtr<FJsonValue>> JsonArray;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());

        if (FJsonSerializer::Deserialize(Reader, JsonArray))
        {
            RepositoryInfos.Empty(); // Liste leeren, bevor sie neu gefüllt wird

            for (const TSharedPtr<FJsonValue>& Value : JsonArray)
            {
                TSharedPtr<FJsonObject> Obj = Value->AsObject();
                FString RepoName = Obj->GetStringField("name");
                FString OwnerName = Obj->GetObjectField("owner")->GetStringField("login");

                FRepositoryInfo RepoInfo;
                RepoInfo.RepoName = RepoName;
                RepoInfo.Owner = OwnerName;

                RepositoryInfos.Add(RepoInfo);
            }

            UE_LOG(LogTemp, Log, TEXT("Repositories count: %d"), RepositoryInfos.Num());
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to retrieve repositories: %s"), *Response->GetContentAsString());
    }
}


TArray<FRepositoryInfo> UGitHubAPIManager::GetRepositoryList()
{
    return RepositoryInfos;
}

void UGitHubAPIManager::ShowSelectedRepo(const FString& RepoName)
{
    for (const FRepositoryInfo& RepoInfo : UGitHubAPIManager::GetInstance()->RepositoryInfos)
    {
        if (RepoInfo.RepoName == RepoName)
        {
            UGitHubAPIManager::GetInstance()->GetRepositoryDetails(RepoInfo.Owner, RepoInfo.RepoName);
            return;
        }
    }

    UE_LOG(LogTemp, Error, TEXT("Repository %s not found!"), *RepoName);
}


void UGitHubAPIManager::GetRepositoryDetails(const FString& Owner, const FString& RepoName)
{
    if (AccessToken.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Access Token is empty. Cannot retrieve repository details."));
        return;
    }

    FString URL = FString::Printf(TEXT("https://api.github.com/repos/%s/%s"), *Owner, *RepoName);

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();
    Request->OnProcessRequestComplete().BindUObject(this, &UGitHubAPIManager::OnRepoDetailsResponseReceived);
    Request->SetURL(URL);
    Request->SetVerb("GET");

    Request->SetHeader("Authorization", "Bearer " + AccessToken);
    Request->SetHeader("User-Agent", "UEGitHubManager");

    Request->ProcessRequest();
}

void UGitHubAPIManager::OnRepoDetailsResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (bWasSuccessful && Response->GetResponseCode() == 200)
    {
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());

        if (FJsonSerializer::Deserialize(Reader, JsonObject))
        {
            FString RepoName = JsonObject->GetStringField("name");
            FString Description = JsonObject->GetStringField("description");
            int32 Stars = JsonObject->GetIntegerField("stargazers_count");
            int32 Forks = JsonObject->GetIntegerField("forks_count");
            FString CreatedAt = JsonObject->GetStringField("created_at");

            UE_LOG(LogTemp, Log, TEXT("Repository Name: %s"), *RepoName);
            UE_LOG(LogTemp, Log, TEXT("Description: %s"), *Description);
            UE_LOG(LogTemp, Log, TEXT("Stars: %d"), Stars);
            UE_LOG(LogTemp, Log, TEXT("Forks: %d"), Forks);
            UE_LOG(LogTemp, Log, TEXT("Created At: %s"), *CreatedAt);

            SelectedRepositoryInfo.RepoName = *RepoName;
            SelectedRepositoryInfo.RepoDescription = *Description;
            SelectedRepositoryInfo.RepoCreatedAt = *CreatedAt;

        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to retrieve repository details: %s"), *Response->GetContentAsString());
    }
}


FSelectedRepositoryInfo UGitHubAPIManager::GetSelectedRepositoryInfo()
{
    return SelectedRepositoryInfo;
}