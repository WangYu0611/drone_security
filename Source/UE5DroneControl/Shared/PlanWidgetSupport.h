#pragma once
#include "Shared/ProductText.h"
#include "Shared/OperationalContextSubsystem.h"
#include "Command/CommandActionButton.h"
#include "Command/CommandTheme.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/ScrollBox.h"
#include "Components/EditableTextBox.h"
#include "Components/ComboBoxString.h"
#include "Engine/GameInstance.h"
namespace PlanUI {
inline FString Field(const TSharedPtr<FJsonObject>& O,const TCHAR* K){FString S;if(O)O->TryGetStringField(K,S);return S;}
inline TSharedPtr<FJsonObject> Object(const TSharedPtr<FJsonObject>& O,const TCHAR* K){const TSharedPtr<FJsonObject>* P;return O && O->TryGetObjectField(K,P)?*P:nullptr;}
inline TSharedPtr<FJsonObject> Find(const TSharedPtr<FJsonObject>& O,const TCHAR* Group,const FString& ID){const auto G=Object(O,Group);const TSharedPtr<FJsonObject>* P;return G && G->TryGetObjectField(ID,P)?*P:nullptr;}
inline FString AssignedUAVs(const TSharedPtr<FJsonObject>& M){const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;if(M && M->TryGetArrayField(TEXT("assigned_uav_ids"),Values)){TArray<FString> IDs;for(const auto& V:*Values)IDs.Add(V->AsString());return FString::Join(IDs,TEXT(", "));}return Field(M,TEXT("assigned_uav_id"));}
inline double EllipsoidAltitude(const TSharedPtr<FJsonObject>& P){double Offset=0;const auto Ref=Field(P,TEXT("altitude_reference"));if(Ref==TEXT("AGL"))P->TryGetNumberField(TEXT("terrain_ellipsoid_m"),Offset);if(Ref==TEXT("MSL"))P->TryGetNumberField(TEXT("geoid_undulation_m"),Offset);return P->GetNumberField(TEXT("altitude"))+Offset;}
inline FText T(const TCHAR* K){return ProductText::Get(K);}
inline FText User(const FString& S){return FText::AsCultureInvariant(S);}
inline UTextBlock* Label(UWidgetTree* Tree,UPanelWidget* Parent,const FText& Text,int Size=13){auto* W=Tree->ConstructWidget<UTextBlock>();CommandTheme::Text(W,Size);W->SetAutoWrapText(true);W->SetText(Text);Parent->AddChild(W);return W;}
inline UCommandActionButton* Button(UWidgetTree* Tree,UPanelWidget* Parent,const TCHAR* Action,const TCHAR* Key,int Id=0){auto* B=Tree->ConstructWidget<UCommandActionButton>();B->Configure(Action,Id);CommandTheme::Button(B,false,true);auto* L=Tree->ConstructWidget<UTextBlock>();CommandTheme::Text(L,12);L->SetText(T(Key));B->SetContent(L);Parent->AddChild(B);return B;}
inline UVerticalBox* Root(UWidgetTree* Tree){auto* B=Tree->ConstructWidget<UBorder>();CommandTheme::Panel(B);B->SetPadding(FMargin(8));Tree->RootWidget=B;auto* Scroll=Tree->ConstructWidget<UScrollBox>();B->SetContent(Scroll);auto* V=Tree->ConstructWidget<UVerticalBox>();Scroll->AddChild(V);return V;}
inline FString Error(const TSharedPtr<FJsonObject>& Reply){if(!Reply)return TEXT("SYNC_OFFLINE");FString C=Field(Reply,TEXT("code"));if(C.IsEmpty() && Reply->HasField(TEXT("error")))C=TEXT("Request");return C;}
inline bool Reviewed(const TSharedPtr<FJsonObject>& P){const auto R=Object(P,TEXT("review"));return R && P && R->GetNumberField(TEXT("content_revision"))==P->GetNumberField(TEXT("content_revision"));}
}
