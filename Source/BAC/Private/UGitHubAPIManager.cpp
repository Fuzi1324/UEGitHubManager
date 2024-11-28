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
    
    UE_LOG(LogTemp, Log, TEXT("Access Token has been set."));
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

void UGitHubAPIManager::FetchUserRepositories()
{
    if (AccessToken.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Access Token is empty. Cannot retrieve repositories."));
        return;
    }

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = CreateHttpRequest("https://api.github.com/user/repos", "GET");
    Request->OnProcessRequestComplete().BindUObject(this, &UGitHubAPIManager::HandleRepoListResponse);
    Request->ProcessRequest();
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

void UGitHubAPIManager::SendGraphQLQuery(const FString& Query, const TFunction<void(TSharedPtr<FJsonObject>)>& Callback)
{
    FString URL = "https://api.github.com/graphql";

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();
    Request->SetURL(URL);
    Request->SetVerb("POST");
    Request->SetHeader("Authorization", "Bearer " + AccessToken);
    Request->SetHeader("User-Agent", "UEGitHubManager");
    Request->SetHeader("Content-Type", "application/json");

    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
    JsonObject->SetStringField("query", Query);

    FString RequestBody;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

    Request->SetContentAsString(RequestBody);

    Request->OnProcessRequestComplete().BindLambda([this, Callback](FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bWasSuccessful)
        {
            if (bWasSuccessful && ResponsePtr->GetResponseCode() == 200)
            {
                TSharedPtr<FJsonObject> ResponseObject;
                TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponsePtr->GetContentAsString());

                if (FJsonSerializer::Deserialize(Reader, ResponseObject))
                {
                    if (ResponseObject->HasField("errors"))
                    {
                        const TArray<TSharedPtr<FJsonValue>>& Errors = ResponseObject->GetArrayField("errors");
                        for (const TSharedPtr<FJsonValue>& ErrorValue : Errors)
                        {
                            TSharedPtr<FJsonObject> ErrorObject = ErrorValue->AsObject();
                            FString Message = ErrorObject->GetStringField("message");
                            UE_LOG(LogTemp, Error, TEXT("GraphQL Fehler: %s"), *Message);
                        }
                        return;
                    }

                    Callback(ResponseObject);
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("Fehler beim Deserialisieren der JSON-Antwort."));
                }
            }
            else
            {
                LogHttpError(ResponsePtr);
            }
        });

    Request->ProcessRequest();
}

void UGitHubAPIManager::CreateNewProject(const FString& Owner, const FString& RepositoryName, const FString& ProjectName)
{
    FString Query = FString::Printf(
        TEXT("query { repository(owner:\"%s\", name:\"%s\") { owner { id } } }"),
        *Owner, *RepositoryName);

    SendGraphQLQuery(Query, [this, ProjectName](TSharedPtr<FJsonObject> ResponseObject)
        {
            if (!ResponseObject.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("Ungültige Antwort vom Server."));
                return;
            }

            TSharedPtr<FJsonObject> DataObject = ResponseObject->GetObjectField("data");
            if (!DataObject.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("Fehler beim Parsen des Datenobjekts."));
                return;
            }

            TSharedPtr<FJsonObject> RepositoryObject = DataObject->GetObjectField("repository");
            if (!RepositoryObject.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("Repository nicht gefunden."));
                return;
            }

            TSharedPtr<FJsonObject> OwnerObject = RepositoryObject->GetObjectField("owner");
            if (!OwnerObject.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("Eigentümer nicht gefunden."));
                return;
            }

            FString OwnerId = OwnerObject->GetStringField("id");

            FString Mutation = FString::Printf(
                TEXT("mutation { createProjectV2(input: {title: \"%s\", ownerId: \"%s\"}) { projectV2 { id title url } } }"),
                *ProjectName, *OwnerId);

            SendGraphQLQuery(Mutation, [this](TSharedPtr<FJsonObject> MutationResponse)
                {
                    if (!MutationResponse.IsValid())
                    {
                        UE_LOG(LogTemp, Error, TEXT("Ungültige Antwort vom Server."));
                        return;
                    }

                    TSharedPtr<FJsonObject> DataObject = MutationResponse->GetObjectField("data");
                    if (!DataObject.IsValid())
                    {
                        UE_LOG(LogTemp, Error, TEXT("Fehler beim Parsen des Datenobjekts."));
                        return;
                    }

                    TSharedPtr<FJsonObject> CreateProjectV2Object = DataObject->GetObjectField("createProjectV2");
                    if (!CreateProjectV2Object.IsValid())
                    {
                        UE_LOG(LogTemp, Error, TEXT("Fehler beim Erstellen des Projekts."));
                        return;
                    }

                    TSharedPtr<FJsonObject> ProjectData = CreateProjectV2Object->GetObjectField("projectV2");
                    if (!ProjectData.IsValid())
                    {
                        UE_LOG(LogTemp, Error, TEXT("Projektdaten nicht gefunden."));
                        return;
                    }

                    FString ProjectId = ProjectData->GetStringField("id");
                    FString ProjectTitle = ProjectData->GetStringField("title");
                    FString ProjectUrl = ProjectData->GetStringField("url");

                    AsyncTask(ENamedThreads::GameThread, [this, ProjectUrl]()
                        {
                            OnProjectCreated.Broadcast(ProjectUrl);
                        });
                });
        });
}

void UGitHubAPIManager::FetchUserProjects()
{
    FString Query = TEXT("query { viewer { projectsV2(first: 100) { nodes { id title url } } } }");

    SendGraphQLQuery(Query, [this](TSharedPtr<FJsonObject> ResponseObject)
        {
            HandleFetchUserProjectsResponse(ResponseObject);
        });
}

void UGitHubAPIManager::HandleFetchUserProjectsResponse(TSharedPtr<FJsonObject> ResponseObject)
{
    if (!ResponseObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Ungültige Antwort vom Server."));
        return;
    }

    TSharedPtr<FJsonObject> DataObject = ResponseObject->GetObjectField("data");
    if (!DataObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Fehler beim Parsen des Datenobjekts."));
        return;
    }

    TSharedPtr<FJsonObject> ViewerObject = DataObject->GetObjectField("viewer");
    if (!ViewerObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Viewer-Daten nicht gefunden."));
        return;
    }

    TSharedPtr<FJsonObject> ProjectsObject = ViewerObject->GetObjectField("projectsV2");
    if (!ProjectsObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("ProjectsV2-Daten nicht gefunden."));
        return;
    }

    const TArray<TSharedPtr<FJsonValue>>* ProjectsArray;
    if (ProjectsObject->TryGetArrayField("nodes", ProjectsArray))
    {
        TArray<FProjectInfo> ProjectsList;
        UserProjects.Empty();

        for (const TSharedPtr<FJsonValue>& ProjectValue : *ProjectsArray)
        {
            TSharedPtr<FJsonObject> ProjectObject = ProjectValue->AsObject();

            FProjectInfo ProjectInfo;
            ProjectInfo.ProjectId = GetStringFieldSafe(ProjectObject, "id");
            ProjectInfo.ProjectTitle = GetStringFieldSafe(ProjectObject, "title");
            ProjectInfo.ProjectURL = GetStringFieldSafe(ProjectObject, "url");

            ProjectsList.Add(ProjectInfo);
            UserProjects.Add(ProjectInfo.ProjectTitle, ProjectInfo);
        }

        AsyncTask(ENamedThreads::GameThread, [this, ProjectsList]()
            {
                OnUserProjectsLoaded.Broadcast(ProjectsList);
            });
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Keine Projekte gefunden."));
    }
}

void UGitHubAPIManager::FetchProjectDetails(const FString& ProjectName)
{
    if (!UserProjects.Contains(ProjectName))
    {
        UE_LOG(LogTemp, Error, TEXT("Projekt mit dem Namen '%s' nicht gefunden."), *ProjectName);
        return;
    }

    FString ProjectId = UserProjects[ProjectName].ProjectId;

    // Korrekte Formatierung der Query mit erforderlichen Leerzeichen und ohne überflüssige Leerzeichen um die Doppelpunkte
    FString Query = FString::Printf(
        TEXT("query { "
            "node(id: \"%s\") { "
            "... on ProjectV2 { "
            "id "
            "title "
            "url "
            "items(first: 100) { "
            "nodes { "
            "id "
            "content { "
            "__typename "
            "... on Issue { "
            "id "
            "title "
            "url "
            "issueState:state "
            "createdAt "
            "} "
            "... on PullRequest { "
            "id "
            "title "
            "url "
            "pullRequestState:state "
            "createdAt "
            "} "
            "... on DraftIssue { "
            "id "
            "title "
            "body "
            "createdAt "
            "} "
            "} "
            "} "
            "} "
            "} "
            "} "
            "}"),
        *ProjectId);

    UE_LOG(LogTemp, Log, TEXT("GraphQL Query: %s"), *Query);

    SendGraphQLQuery(Query, [this, ProjectName](TSharedPtr<FJsonObject> ResponseObject)
        {
            HandleFetchProjectDetailsResponse(ResponseObject, ProjectName);
        });
}


void UGitHubAPIManager::HandleFetchProjectDetailsResponse(TSharedPtr<FJsonObject> ResponseObject, const FString& ProjectName)
{
    if (!ResponseObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Ungültige Antwort vom Server."));
        return;
    }

    TSharedPtr<FJsonObject> DataObject = ResponseObject->GetObjectField("data");
    if (!DataObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Fehler beim Parsen des Datenobjekts."));
        return;
    }

    TSharedPtr<FJsonObject> NodeObject = DataObject->GetObjectField("node");
    if (!NodeObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Projektknoten nicht gefunden."));
        return;
    }

    FProjectInfo ProjectInfo;
    ProjectInfo.ProjectId = GetStringFieldSafe(NodeObject, "id");
    ProjectInfo.ProjectTitle = GetStringFieldSafe(NodeObject, "title");
    ProjectInfo.ProjectURL = GetStringFieldSafe(NodeObject, "url");

    TSharedPtr<FJsonObject> ItemsObject = NodeObject->GetObjectField("items");
    if (ItemsObject.IsValid())
    {
        const TArray<TSharedPtr<FJsonValue>>* ItemsArray;
        if (ItemsObject->TryGetArrayField("nodes", ItemsArray))
        {
            for (const TSharedPtr<FJsonValue>& ItemValue : *ItemsArray)
            {
                TSharedPtr<FJsonObject> ItemObject = ItemValue->AsObject();
                if (!ItemObject.IsValid())
                {
                    UE_LOG(LogTemp, Warning, TEXT("Ungültiges Item-Objekt."));
                    continue;
                }

                FProjectItem ProjectItem;
                ProjectItem.ItemId = GetStringFieldSafe(ItemObject, "id");

                if (ItemObject->HasTypedField<EJson::Object>("content"))
                {
                    TSharedPtr<FJsonObject> ContentObject = ItemObject->GetObjectField("content");
                    if (ContentObject.IsValid())
                    {
                        FString TypeName = GetStringFieldSafe(ContentObject, "__typename");
                        ProjectItem.Type = TypeName;

                        if (TypeName == "Issue")
                        {
                            ProjectItem.Title = GetStringFieldSafe(ContentObject, "title");
                            ProjectItem.Url = GetStringFieldSafe(ContentObject, "url");
                            ProjectItem.CreatedAt = GetStringFieldSafe(ContentObject, "createdAt");
                            ProjectItem.State = GetStringFieldSafe(ContentObject, "issueState");
                        }
                        else if (TypeName == "PullRequest")
                        {
                            ProjectItem.Title = GetStringFieldSafe(ContentObject, "title");
                            ProjectItem.Url = GetStringFieldSafe(ContentObject, "url");
                            ProjectItem.CreatedAt = GetStringFieldSafe(ContentObject, "createdAt");
                            ProjectItem.State = GetStringFieldSafe(ContentObject, "pullRequestState");
                        }
                        else if (TypeName == "DraftIssue")
                        {
                            ProjectItem.Title = GetStringFieldSafe(ContentObject, "title");
                            ProjectItem.CreatedAt = GetStringFieldSafe(ContentObject, "createdAt");
                            ProjectItem.State = "DRAFT";
                            ProjectItem.Url = "";
                        }
                        else
                        {
                            UE_LOG(LogTemp, Warning, TEXT("Unhandled content type: %s"), *TypeName);
                            continue;
                        }

                        ProjectInfo.Items.Add(ProjectItem);
                    }
                    else
                    {
                        UE_LOG(LogTemp, Warning, TEXT("ContentObject ist ungültig für Item mit ID: %s"), *ProjectItem.ItemId);
                    }
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("Item mit ID %s hat kein 'content'-Feld."), *ProjectItem.ItemId);
                }
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Fehler beim Abrufen des Items-Arrays."));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Items-Objekt ist ungültig."));
    }

    AsyncTask(ENamedThreads::GameThread, [this, ProjectInfo]()
        {
            OnProjectDetailsLoaded.Broadcast(ProjectInfo);
        });
}

void UGitHubAPIManager::FetchProjectColumns(const FString& ProjectName)
{
    if (!UserProjects.Contains(ProjectName))
    {
        UE_LOG(LogTemp, Error, TEXT("Projekt mit dem Namen '%s' nicht gefunden."), *ProjectName);
        return;
    }

    FString ProjectId = UserProjects[ProjectName].ProjectId;

    FString Query = FString::Printf(
        TEXT("query { "
            "node(id: \"%s\") { "
            "... on ProjectV2 { "
            "fields(first: 100) { "
            "nodes { "
            "__typename "
            "... on ProjectV2SingleSelectField { "
            "id "
            "name "
            "options { "
            "id "
            "name "
            "color "
            "} "
            "} "
            "} "
            "} "
            "} "
            "} "
            "}"),
        *ProjectId);

    UE_LOG(LogTemp, Log, TEXT("GraphQL Query for fetching project columns: %s"), *Query);

    SendGraphQLQuery(Query, [this, ProjectName](TSharedPtr<FJsonObject> ResponseObject)
        {
            HandleFetchProjectColumnsResponse(ResponseObject, ProjectName);
        });
}

void UGitHubAPIManager::HandleFetchProjectColumnsResponse(TSharedPtr<FJsonObject> ResponseObject, const FString& ProjectName)
{
    if (!ResponseObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Ungültige Antwort vom Server beim Abrufen der Projektspalten."));
        return;
    }

    TSharedPtr<FJsonObject> DataObject = ResponseObject->GetObjectField("data");
    TSharedPtr<FJsonObject> NodeObject = DataObject->GetObjectField("node");
    TSharedPtr<FJsonObject> ProjectObject = NodeObject->GetObjectField("fields");

    const TArray<TSharedPtr<FJsonValue>>* FieldsArray;
    if (ProjectObject->TryGetArrayField("nodes", FieldsArray))
    {
        TArray<FProjectColumn> Columns;

        for (const TSharedPtr<FJsonValue>& FieldValue : *FieldsArray)
        {
            TSharedPtr<FJsonObject> FieldObject = FieldValue->AsObject();
            FString TypeName = GetStringFieldSafe(FieldObject, "__typename");

            if (TypeName == "ProjectV2SingleSelectField")
            {
                FString FieldName = GetStringFieldSafe(FieldObject, "name");
                if (FieldName == "Status")
                {
                    const TArray<TSharedPtr<FJsonValue>>* OptionsArray;
                    if (FieldObject->TryGetArrayField("options", OptionsArray))
                    {
                        for (const TSharedPtr<FJsonValue>& OptionValue : *OptionsArray)
                        {
                            TSharedPtr<FJsonObject> OptionObject = OptionValue->AsObject();

                            FProjectColumn Column;
                            Column.ColumnId = GetStringFieldSafe(OptionObject, "id");
                            Column.ColumnName = GetStringFieldSafe(OptionObject, "name");
                            Column.Color = GetStringFieldSafe(OptionObject, "color");

                            Columns.Add(Column);
                        }
                    }
                }
            }
        }

        AsyncTask(ENamedThreads::GameThread, [this, ProjectName, Columns]()
            {
                OnProjectColumnsLoaded.Broadcast(ProjectName, Columns);
            });
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Keine Felder gefunden im Projekt."));
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