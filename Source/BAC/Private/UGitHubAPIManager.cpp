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
    // Speichere das AccessToken in der Instanzvariable
    AccessToken = AuthToken;

    // Richte die HTTP-Anfrage für GitHub ein
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();
    Request->OnProcessRequestComplete().BindUObject(this, &UGitHubAPIManager::OnAuthResponseReceived);
    Request->SetURL("https://api.github.com/user"); // GitHub API-URL für Benutzerdaten
    Request->SetVerb("GET");

    // Authentifizierungs-Header
    Request->SetHeader("Authorization", "Bearer " + AccessToken);
    Request->SetHeader("User-Agent", "UEGitHubManager"); // GitHub verlangt einen User-Agent

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