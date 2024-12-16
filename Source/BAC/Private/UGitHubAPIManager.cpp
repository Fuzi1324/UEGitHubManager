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

void UGitHubAPIManager::FetchCurrentUser()
{
    if (AccessToken.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Access Token is empty. Cannot fetch user info."));
        return;
    }

    FString Query = TEXT("query { viewer { login } }");

    SendGraphQLQuery(Query, [this](TSharedPtr<FJsonObject> ResponseObject)
        {
            if (!ResponseObject.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("Invalid response from server."));
                return;
            }

            TSharedPtr<FJsonObject> DataObject = ResponseObject->GetObjectField("data");
            if (!DataObject.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("Error parsing data object."));
                return;
            }

            TSharedPtr<FJsonObject> ViewerObject = DataObject->GetObjectField("viewer");
            if (!ViewerObject.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("Viewer data not found."));
                return;
            }

            FString UserName = GetStringFieldSafe(ViewerObject, "login");

            AsyncTask(ENamedThreads::GameThread, [this, UserName]()
                {
                    OnUserNameReceived.Broadcast(UserName);
                });
        });
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

void UGitHubAPIManager::SendGraphQLMutation(const FString& Mutation, const TFunction<void(TSharedPtr<FJsonObject>)>& Callback)
{
    FString URL = "https://api.github.com/graphql";
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();
    Request->SetURL(URL);
    Request->SetVerb("POST");
    Request->SetHeader("Authorization", "Bearer " + AccessToken);
    Request->SetHeader("User-Agent", "UEGitHubManager");
    Request->SetHeader("Content-Type", "application/json");

    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
    JsonObject->SetStringField("query", Mutation);

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
                        AsyncTask(ENamedThreads::GameThread, [this]() { OnMutationCompleted.Broadcast(false); });
                        return;
                    }
                    Callback(ResponseObject);
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("Fehler beim Deserialisieren der JSON-Antwort."));
                    AsyncTask(ENamedThreads::GameThread, [this]() { OnMutationCompleted.Broadcast(false); });
                }
            }
            else
            {
                LogHttpError(ResponsePtr);
                AsyncTask(ENamedThreads::GameThread, [this]() { OnMutationCompleted.Broadcast(false); });
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

            FString OwnerId = DataObject->GetObjectField("repository")->GetObjectField("owner")->GetStringField("id");

            FString Mutation = FString::Printf(
                TEXT("mutation { createProjectV2(input: {title: \"%s\", ownerId: \"%s\"}) { projectV2 { id title url } } }"),
                *ProjectName, *OwnerId);

            SendGraphQLMutation(Mutation, [this](TSharedPtr<FJsonObject> MutationResponse)
                {
                    if (!MutationResponse.IsValid())
                    {
                        UE_LOG(LogTemp, Error, TEXT("Ungültige Antwort vom Server."));
                        AsyncTask(ENamedThreads::GameThread, [this]() { OnMutationCompleted.Broadcast(false); });
                        return;
                    }

                    TSharedPtr<FJsonObject> ProjectData = MutationResponse->GetObjectField("data")
                        ->GetObjectField("createProjectV2")
                        ->GetObjectField("projectV2");

                    FString ProjectUrl = ProjectData->GetStringField("url");

                    AsyncTask(ENamedThreads::GameThread, [this, ProjectUrl]()
                        {
                            OnProjectCreated.Broadcast(ProjectUrl);
                            OnMutationCompleted.Broadcast(true);
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
        UE_LOG(LogTemp, Error, TEXT("Project with name '%s' not found."), *ProjectName);
        return;
    }

    FString ProjectId = UserProjects[ProjectName].ProjectId;

    FString Query = FString::Printf(TEXT(
        "query { "
        "  node(id: \"%s\") { "
        "    ... on ProjectV2 { "
        "      id "
        "      title "
        "      url "
        "      fields(first: 20) { "
        "        nodes { "
        "          ... on ProjectV2Field { "
        "            id "
        "            name "
        "            dataType "
        "          } "
        "          ... on ProjectV2SingleSelectField { "
        "            id "
        "            name "
        "            options { "
        "              id "
        "              name "
        "            } "
        "          } "
        "        } "
        "      } "
        "      items(first: 100) { "
        "        nodes { "
        "          id "
        "          fieldValues(first: 8) { "
        "            nodes { "
        "              ... on ProjectV2ItemFieldDateValue { "
        "                id "
        "                date "
        "                field { "
        "                  ... on ProjectV2Field { "
        "                    id "
        "                    name "
        "                    dataType "
        "                  } "
        "                } "
        "              } "
        "              ... on ProjectV2ItemFieldSingleSelectValue { "
        "                id "
        "                name "
        "                optionId "
        "                field { "
        "                  ... on ProjectV2SingleSelectField { "
        "                    id "
        "                    name "
        "                  } "
        "                } "
        "              } "
        "            } "
        "          } "
        "          content { "
        "            __typename "
        "            ... on Issue { "
        "              id "
        "              title "
        "              url "
        "              issueState: state "
        "              createdAt "
        "              body "
        "            } "
        "            ... on PullRequest { "
        "              id "
        "              title "
        "              url "
        "              pullRequestState: state "
        "              createdAt "
        "              body "
        "            } "
        "            ... on DraftIssue { "
        "              id "
        "              title "
        "              body "
        "              createdAt "
        "            } "
        "          } "
        "        } "
        "      } "
        "    } "
        "  } "
        "}"), *ProjectId);

    SendGraphQLQuery(Query, [this, ProjectName](TSharedPtr<FJsonObject> ResponseObject)
        {
            HandleFetchProjectDetailsResponse(ResponseObject, ProjectName);
        });
}



void UGitHubAPIManager::HandleFetchProjectDetailsResponse(TSharedPtr<FJsonObject> ResponseObject, const FString& ProjectName)
{
    if (!ResponseObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid response from server."));
        return;
    }

    TSharedPtr<FJsonObject> DataObject = ResponseObject->GetObjectField("data");
    if (!DataObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Error parsing data object."));
        return;
    }

    TSharedPtr<FJsonObject> NodeObject = DataObject->GetObjectField("node");
    if (!NodeObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Project node not found."));
        return;
    }

    FProjectInfo ProjectInfo;
    ProjectInfo.ProjectId = GetStringFieldSafe(NodeObject, "id");
    ProjectInfo.ProjectTitle = GetStringFieldSafe(NodeObject, "title");
    ProjectInfo.ProjectURL = GetStringFieldSafe(NodeObject, "url");

    TSharedPtr<FJsonObject> ItemsObject = NodeObject->GetObjectField("items");
    if (!ItemsObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Items object is invalid."));
        AsyncTask(ENamedThreads::GameThread, [this, ProjectInfo]()
            {
                OnProjectDetailsLoaded.Broadcast(ProjectInfo);
            });
        return;
    }

    const TArray<TSharedPtr<FJsonValue>>* ItemsArray;
    if (!ItemsObject->TryGetArrayField("nodes", ItemsArray))
    {
        UE_LOG(LogTemp, Error, TEXT("Error retrieving items array."));
        AsyncTask(ENamedThreads::GameThread, [this, ProjectInfo]()
            {
                OnProjectDetailsLoaded.Broadcast(ProjectInfo);
            });
        return;
    }

    for (const TSharedPtr<FJsonValue>& ItemValue : *ItemsArray)
    {
        TSharedPtr<FJsonObject> ItemObject = ItemValue->AsObject();
        if (!ItemObject.IsValid())
        {
            continue;
        }

        FProjectItem ProjectItem;
        ProjectItem.ItemId = GetStringFieldSafe(ItemObject, "id");

        if (ItemObject->HasTypedField<EJson::Object>("fieldValues"))
        {
            TSharedPtr<FJsonObject> FieldValuesObject = ItemObject->GetObjectField("fieldValues");
            const TArray<TSharedPtr<FJsonValue>>* FieldValuesNodes;
            if (FieldValuesObject->TryGetArrayField("nodes", FieldValuesNodes))
            {
                for (const TSharedPtr<FJsonValue>& FieldValue : *FieldValuesNodes)
                {
                    TSharedPtr<FJsonObject> FieldValueObject = FieldValue->AsObject();
                    if (!FieldValueObject.IsValid())
                    {
                        continue;
                    }

                    if (FieldValueObject->HasField("name") && FieldValueObject->HasField("field"))
                    {
                        FString Name = GetStringFieldSafe(FieldValueObject, "name");
                        TSharedPtr<FJsonObject> FieldObject = FieldValueObject->GetObjectField("field");
                        if (FieldObject.IsValid())
                        {
                            FString FieldName = GetStringFieldSafe(FieldObject, "name");
                            FString FieldId = GetStringFieldSafe(FieldObject, "id");

                            if (FieldName == "Status")
                            {
                                ProjectItem.ColumnName = Name;
                                FString OptionId = GetStringFieldSafe(FieldValueObject, "optionId");
                                ProjectItem.ColumnId = OptionId;

                                if (ProjectInfo.ColumnFieldId.IsEmpty())
                                {
                                    ProjectInfo.ColumnFieldId = FieldId;
                                }
                            }
                        }
                    }

                    if (FieldValueObject->HasField("date") && FieldValueObject->HasField("field"))
                    {
                        FString DateValue = GetStringFieldSafe(FieldValueObject, "date");
                        TSharedPtr<FJsonObject> FieldObject = FieldValueObject->GetObjectField("field");
                        if (FieldObject.IsValid())
                        {
                            FString FieldName = GetStringFieldSafe(FieldObject, "name");
                            FString FieldId = GetStringFieldSafe(FieldObject, "id");

                            if (FieldName == "StartDate")
                            {
                                ProjectItem.StartDate = DateValue;
                                ProjectItem.StartDateFieldId = FieldId;
                            }
                            else if (FieldName == "EndDate")
                            {
                                ProjectItem.EndDate = DateValue;
                                ProjectItem.EndDateFieldId = FieldId;
                            }
                        }
                    }
                }
            }
        }

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
                    ProjectItem.Body = GetStringFieldSafe(ContentObject, "body");
                }
                else if (TypeName == "PullRequest")
                {
                    ProjectItem.Title = GetStringFieldSafe(ContentObject, "title");
                    ProjectItem.Url = GetStringFieldSafe(ContentObject, "url");
                    ProjectItem.CreatedAt = GetStringFieldSafe(ContentObject, "createdAt");
                    ProjectItem.State = GetStringFieldSafe(ContentObject, "pullRequestState");
                    ProjectItem.Body = GetStringFieldSafe(ContentObject, "body");
                }
                else if (TypeName == "DraftIssue")
                {
                    ProjectItem.Title = GetStringFieldSafe(ContentObject, "title");
                    ProjectItem.CreatedAt = GetStringFieldSafe(ContentObject, "createdAt");
                    ProjectItem.Body = GetStringFieldSafe(ContentObject, "body");
                    ProjectItem.State = "DRAFT";
                    ProjectItem.Url = "";
                }
            }
        }

        ProjectInfo.Items.Add(ProjectItem);
    }

    AsyncTask(ENamedThreads::GameThread, [this, ProjectInfo]()
        {
            OnProjectDetailsLoaded.Broadcast(ProjectInfo);
        });
}


void UGitHubAPIManager::UpdateProjectItemDateValue(const FString& ProjectId, const FString& ItemId, const FString& FieldId, const FString& NewDateValue)
{
    FString FormattedDate = NewDateValue;
    if (!NewDateValue.Contains("T"))
    {
        FormattedDate = FString::Printf(TEXT("%sT00:00:00.000Z"), *NewDateValue);
    }

    FString Mutation = FString::Printf(
        TEXT("mutation { "
            "updateProjectV2ItemFieldValue( "
            "input: { "
            "projectId: \"%s\" "
            "itemId: \"%s\" "
            "fieldId: \"%s\" "
            "value: { "
            "date: \"%s\" "
            "} "
            "} "
            ") { "
            "projectV2Item { "
            "id "
            "fieldValues(first: 8) { "
            "nodes { "
            "... on ProjectV2ItemFieldDateValue { "
            "date "
            "field { "
            "... on ProjectV2Field { "
            "name "
            "} "
            "} "
            "} "
            "} "
            "} "
            "} "
            "} "
            "}"),
        *ProjectId, *ItemId, *FieldId, *FormattedDate);

    //UE_LOG(LogTemp, Log, TEXT("Sending mutation: %s"), *Mutation);

    SendGraphQLMutation(Mutation, [this](TSharedPtr<FJsonObject> ResponseObject)
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

            FString ResponseString;
            TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResponseString);
            FJsonSerializer::Serialize(ResponseObject.ToSharedRef(), Writer);
            UE_LOG(LogTemp, Log, TEXT("Response received: %s"), *ResponseString);

            AsyncTask(ENamedThreads::GameThread, [this]()
                {
                    OnMutationCompleted.Broadcast(true);
                });
        });
}

void UGitHubAPIManager::CreateProjectItem(const FString& ProjectId, const FString& Title, const FString& FieldId, const FString& ColumnId)
{
    if (AccessToken.IsEmpty() || ProjectId.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Access Token or Project ID is empty."));
        return;
    }

    FString Mutation = FString::Printf(TEXT(
        "mutation {"
        "  addProjectV2DraftIssue(input: {"
        "    projectId: \"%s\""
        "    title: \"%s\""
        "  }) {"
        "    projectItem {"
        "      id"
        "    }"
        "  }"
        "}"), *ProjectId, *Title);

    UE_LOG(LogTemp, Log, TEXT("Creating new project item with title: %s"), *Title);

    SendGraphQLMutation(Mutation, [this, ProjectId, FieldId, ColumnId](TSharedPtr<FJsonObject> ResponseObject)
        {
            if (!ResponseObject.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("Invalid response from server."));
                return;
            }
            TSharedPtr<FJsonObject> DataObject = ResponseObject->GetObjectField("data");
            if (!DataObject.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("Error parsing data object."));
                return;
            }

            TSharedPtr<FJsonObject> CreateObject = DataObject->GetObjectField("addProjectV2DraftIssue");
            if (!CreateObject.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("Failed to create project item."));
                return;
            }

            TSharedPtr<FJsonObject> ProjectItem = CreateObject->GetObjectField("projectItem");
            if (!ProjectItem.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("Project item not found in response."));
                return;
            }

            FString ItemId = ProjectItem->GetStringField("id");

            FString UpdateMutation = FString::Printf(TEXT(
                "mutation {"
                "  updateProjectV2ItemFieldValue("
                "    input: {"
                "      projectId: \"%s\""
                "      itemId: \"%s\""
                "      fieldId: \"%s\""
                "      value: { singleSelectOptionId: \"%s\" }"
                "    }"
                "  ) {"
                "    projectV2Item {"
                "      id"
                "    }"
                "  }"
                "}"), *ProjectId, *ItemId, *FieldId, *ColumnId);

            SendGraphQLMutation(UpdateMutation, [this, ProjectId](TSharedPtr<FJsonObject> UpdateResponse)
                {
                    AsyncTask(ENamedThreads::GameThread, [this, ProjectId]()
                        {
                            OnItemCreated.Broadcast();
                            for (const TPair<FString, FProjectInfo>& Pair : UserProjects)
                            {
                                if (Pair.Value.ProjectId == ProjectId)
                                {
                                    FetchProjectDetails(Pair.Value.ProjectTitle);
                                    break;
                                }
                            }
                        });
                });
        });
}


FString UGitHubAPIManager::GetStringFieldSafe(TSharedPtr<FJsonObject> JsonObject, const FString& FieldName)
{
    return JsonObject->HasField(FieldName) ? JsonObject->GetStringField(FieldName) : TEXT("");
}

TOptional<int32> UGitHubAPIManager::GetIntegerFieldSafe(TSharedPtr<FJsonObject> JsonObject, const FString& FieldName)
{
    return JsonObject->HasField(FieldName) ? TOptional<int32>(JsonObject->GetIntegerField(FieldName)) : TOptional<int32>();
}