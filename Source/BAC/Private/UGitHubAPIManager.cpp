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

void UGitHubAPIManager::Authenticate(const FString& AuthToken)
{
    // Speichere den AccessToken in der Instanzvariable
    AccessToken = AuthToken;

    // Richte die HTTP-Anfrage für GitHub ein
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();
    Request->OnProcessRequestComplete().BindUObject(this, &UGitHubAPIManager::OnAuthResponseReceived);
    Request->SetURL("https://api.github.com/user");
    Request->SetVerb("GET");

    // Authentifizierungs-Header
    Request->SetHeader("Authorization", "Bearer " + AccessToken);
    Request->SetHeader("User-Agent", "UEGitHubManager");

    // Anfrage abschicken
    Request->ProcessRequest();
}


void UGitHubAPIManager::OnAuthResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (bWasSuccessful && Response->GetResponseCode() == 200)
    {
        UE_LOG(LogTemp, Log, TEXT("GitHub Authentication Successful!"));

    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("GitHub Authentication Failed: %s"), *Response->GetContentAsString());
    }
}

void UGitHubAPIManager::GetProjects() {
    UE_LOG(LogTemp, Log, TEXT("Getting Projects"))
}

void UGitHubAPIManager::StartGitHubIntegration(const FString& UserAccessToken)
{
    UE_LOG(LogTemp, Log, TEXT("User Access Token: %s"), *UserAccessToken);

    Authenticate(UserAccessToken);
}

void UGitHubAPIManager::GetRepositories()
{
    if (AccessToken.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Access Token is empty. Cannot retrieve repositories."));
        return;
    }

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();
    Request->OnProcessRequestComplete().BindUObject(this, &UGitHubAPIManager::OnReposResponseReceived);
    Request->SetURL("https://api.github.com/user/repos"); // GitHub API für Repositories des Benutzers
    Request->SetVerb("GET");
    Request->SetHeader("Authorization", "Bearer " + AccessToken);
    Request->SetHeader("User-Agent", "UEGitHubManager");
    Request->ProcessRequest();
}

void UGitHubAPIManager::OnReposResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (bWasSuccessful && Response->GetResponseCode() == 200)
    {
        UE_LOG(LogTemp, Log, TEXT("Repositories retrieved successfully!"));

        // JSON-Daten analysieren und Repositories extrahieren
        TArray<TSharedPtr<FJsonValue>> JsonArray;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());

        if (FJsonSerializer::Deserialize(Reader, JsonArray))
        {
            RepositoryNames.Empty(); // Liste leeren, bevor sie neu gefüllt wird

            for (const TSharedPtr<FJsonValue>& Value : JsonArray)
            {
                TSharedPtr<FJsonObject> Obj = Value->AsObject();
                FString RepoName = Obj->GetStringField("name");
                RepositoryNames.Add(RepoName);
            }

            UE_LOG(LogTemp, Log, TEXT("Repositories count: %d"), RepositoryNames.Num());
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to retrieve repositories: %s"), *Response->GetContentAsString());
    }
}

TArray<FString> UGitHubAPIManager::GetRepositoryList()
{
    return RepositoryNames;
}