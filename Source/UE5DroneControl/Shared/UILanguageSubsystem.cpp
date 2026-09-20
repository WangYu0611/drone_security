#include "Shared/UILanguageSubsystem.h"
#include "Internationalization/Culture.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Internationalization/Internationalization.h"
void UUILanguageSubsystem::ApplyConfirmed(const FString& Language) {
    if(Language!=TEXT("en") && Language!=TEXT("zh-Hans"))return;
    if(ConfirmedLanguage==Language && FInternationalization::Get().GetCurrentLanguage()->GetName()==Language)return;
    FInternationalization::Get().SetCurrentLanguage(Language);
#if WITH_EDITOR
    if(GIsEditor)FTextLocalizationManager::Get().EnableGameLocalizationPreview(Language);
#endif
    FTextLocalizationManager::Get().WaitForAsyncTasks();
    ConfirmedLanguage=Language;OnLanguageChanged.Broadcast();
}


