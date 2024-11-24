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

void UGitHubAPIManager::InitializeIntegration(const FString& UserAccessToken)
{
    UE_LOG(LogTemp, Log, TEXT("User Access Token: %s"), *UserAccessToken);
    SetAccessToken(UserAccessToken);
}

void UGitHubAPIManager::SetAccessToken(const FString& AuthToken)
{
    AccessToken = AuthToken;

    if (AccessToken.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Access Token is empty. Cannot retrieve repositories."));
        return;
    }

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = CreateHttpRequest("https://api.github.com/user/repos", "GET");
    Request->OnProcessRequestComplete().BindUObject(this, &UGitHubAPIManager::HandleRepoListResponse);
    Request->ProcessRequest();
}

void UGitHubAPIManager::LogHttpError(FHttpResponsePtr Response) const
{
    if (Response.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Error %d: %s"), Response->GetResponseCode(), *Response->GetContentAsString());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Request failed without a valid response."));
    }
}

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> UGitHubAPIManager::CreateHttpRequest(const FString& URL, const FString& Verb)
{
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();
    Request->SetURL(URL);
    Request->SetVerb(Verb);
    Request->SetHeader("Authorization", "Bearer " + AccessToken);
    Request->SetHeader("User-Agent", "UEGitHubManager");

    if (Verb == "POST" || Verb == "PATCH")
    {
        Request->SetHeader("Content-Type", "application/json");
    }

    return Request;
}

void UGitHubAPIManager::HandleRepoListResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (bWasSuccessful && Response->GetResponseCode() == 200)
    {
        TArray<TSharedPtr<FJsonValue>> JsonArray;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());

        if (FJsonSerializer::Deserialize(Reader, JsonArray))
        {
            RepositoryInfos.Empty();

            for (const TSharedPtr<FJsonValue>& Value : JsonArray)
            {
                TSharedPtr<FJsonObject> Obj = Value->AsObject();
                FString RepoName = GetStringFieldSafe(Obj, "name");
                FString OwnerName = GetStringFieldSafe(Obj->GetObjectField("owner"), "login");

                FRepositoryInfo RepoInfo;
                RepoInfo.RepositoryName = RepoName;
                RepoInfo.Owner = OwnerName;

                RepositoryInfos.Add(RepoName, RepoInfo);
            }

            TArray<FRepositoryInfo> Values;
            RepositoryInfos.GenerateValueArray(Values);

            AsyncTask(ENamedThreads::GameThread, [this, Values]()
                {
                    OnRepositoriesLoaded.Broadcast(Values);
                });
        }
    }
    else
    {
        LogHttpError(Response);
    }
}

TArray<FRepositoryInfo> UGitHubAPIManager::GetRepositoryList()
{
    TArray<FRepositoryInfo> Values;
    RepositoryInfos.GenerateValueArray(Values);
    return Values;
}

void UGitHubAPIManager::FetchRepositoryDetails(const FString& RepositoryName)
{
    if (RepositoryInfos.Contains(RepositoryName))
    {
        FRepositoryInfo SelectedRepo = RepositoryInfos[RepositoryName];
        FString URL = FString::Printf(TEXT("https://api.github.com/repos/%s/%s"), *SelectedRepo.Owner, *SelectedRepo.RepositoryName);

        TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = CreateHttpRequest(URL, "GET");
        Request->OnProcessRequestComplete().BindUObject(this, &UGitHubAPIManager::HandleRepoDetailsResponse);
        Request->ProcessRequest();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Repository %s not found!"), *RepositoryName);
    }
}

void UGitHubAPIManager::HandleRepoDetailsResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (bWasSuccessful && Response->GetResponseCode() == 200)
    {
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());

        if (FJsonSerializer::Deserialize(Reader, JsonObject))
        {
            ActiveRepository.RepositoryName = GetStringFieldSafe(JsonObject, "name");
            ActiveRepository.Owner = GetStringFieldSafe(JsonObject->GetObjectField("owner"), "login");
            ActiveRepository.Description = GetStringFieldSafe(JsonObject, "description");
            ActiveRepository.CreatedAt = GetStringFieldSafe(JsonObject, "created_at");

            ActiveRepository.Stars = JsonObject->HasField("stargazers_count") ? JsonObject->GetIntegerField("stargazers_count") : 0;
            ActiveRepository.Forks = JsonObject->HasField("forks_count") ? JsonObject->GetIntegerField("forks_count") : 0;

        }

        FRepositoryInfo RepositoryCopy = ActiveRepository;

        AsyncTask(ENamedThreads::GameThread, [this, RepositoryCopy]()
            {
                OnRepositoryDetailsLoaded.Broadcast(RepositoryCopy);
            });
    }
    else
    {
        LogHttpError(Response);
    }
}

void UGitHubAPIManager::CreateRepositoryProject(const FString& RepositoryName, const FString& ProjectName, const FString& ProjectBody)
{
    if (RepositoryInfos.Contains(RepositoryName))
    {
        FRepositoryInfo SelectedRepo = RepositoryInfos[RepositoryName];
        FString URL = FString::Printf(TEXT("https://api.github.com/repos/%s/%s/projects"), *SelectedRepo.Owner, *SelectedRepo.RepositoryName);

        TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = CreateHttpRequest(URL, "POST");

        Request->SetHeader("Accept", "application/vnd.github.inertia-preview+json");

        TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
        JsonObject->SetStringField("name", ProjectName);
        JsonObject->SetStringField("body", ProjectBody);

        FString RequestBody;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
        FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

        Request->SetContentAsString(RequestBody);

        Request->OnProcessRequestComplete().BindUObject(this, &UGitHubAPIManager::HandleCreateProjectResponse);
        Request->ProcessRequest();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Repository %s not found!"), *RepositoryName);
    }
}

void UGitHubAPIManager::HandleCreateProjectResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (bWasSuccessful && (Response->GetResponseCode() == 201 || Response->GetResponseCode() == 200))
    {
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());

        if (FJsonSerializer::Deserialize(Reader, JsonObject))
        {
            FString ProjectUrl = GetStringFieldSafe(JsonObject, "html_url");
            int32 ProjectId = JsonObject->GetIntegerField("id");

            FetchProjectDetails(ProjectId);

            AsyncTask(ENamedThreads::GameThread, [this, ProjectUrl]()
                {
                    OnProjectCreated.Broadcast(ProjectUrl);
                });
        }
    }
    else
    {
        LogHttpError(Response);
    }
}

void UGitHubAPIManager::FetchProjectDetails(int32 ProjectId)
{
    FString URL = FString::Printf(TEXT("https://api.github.com/projects/%d"), ProjectId);

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = CreateHttpRequest(URL, "GET");

    Request->SetHeader("Accept", "application/vnd.github.inertia-preview+json");

    Request->OnProcessRequestComplete().BindUObject(this, &UGitHubAPIManager::HandleFetchProjectDetailsResponse);
    Request->ProcessRequest();
}

void UGitHubAPIManager::HandleFetchProjectDetailsResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (bWasSuccessful && Response->GetResponseCode() == 200)
    {
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());

        if (FJsonSerializer::Deserialize(Reader, JsonObject))
        {
            FProjectInfo ProjectInfo;
            ProjectInfo.Id = JsonObject->GetIntegerField("id");
            ProjectInfo.Name = GetStringFieldSafe(JsonObject, "name");
            ProjectInfo.Body = GetStringFieldSafe(JsonObject, "body");
            ProjectInfo.State = GetStringFieldSafe(JsonObject, "state");
            ProjectInfo.HtmlUrl = GetStringFieldSafe(JsonObject, "html_url");

            AsyncTask(ENamedThreads::GameThread, [this, ProjectInfo]()
                {
                    OnProjectDetailsLoaded.Broadcast(ProjectInfo);
                });
        }
    }
    else
    {
        LogHttpError(Response);
    }
}


FString UGitHubAPIManager::GetStringFieldSafe(TSharedPtr<FJsonObject> JsonObject, const FString& FieldName)
{
    return JsonObject->HasField(FieldName) ? JsonObject->GetStringField(FieldName) : TEXT("");
}

TOptional<int32> UGitHubAPIManager::GetIntegerFieldSafe(TSharedPtr<FJsonObject> JsonObject, const FString& FieldName)
{
    return JsonObject->HasField(FieldName) ? TOptional<int32>(JsonObject->GetIntegerField(FieldName)) : TOptional<int32>();
}