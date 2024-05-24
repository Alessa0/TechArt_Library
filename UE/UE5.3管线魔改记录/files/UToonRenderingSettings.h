#pragma once
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPath.h"

#include "Engine/DeveloperSettings.h"
#include "UToonRenderingSettings.generated.h"

/**
 *
 */
 UCLASS(config = Engine, defaultconfig, meta = (DisplayName = "ToonRamp"))
class ENGINE_API UToonRenderingSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()
public:
	UPROPERTY(config, EditAnywhere, Category="PreIntegrated-RampTexture", meta=(AllowedClasses="/Script/CroeUObject.Class'/Script/Engine.CurveLinearColorAtlas'", DisplayName="Curve LinearColor Atlas"))
	FSoftObjectPath ToonRampTextureName;
public:
	FTextureRHIRef GetTextureRHI() const;
	virtual void PostInitProperties() override;
#if WITH_EDITOR
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	DECLARE_EVENT(UToonRenderingSettings, FToonRenderingSettingsChanged)
	FToonRenderingSettingsChanged ToonRenderingSettingsChanged;
#endif//WITH_EDITOR
#if WITH_EDITOR
	FToonRenderingSettingsChanged& OnToonRenderingSettingsChanged() 
	{
		return ToonRenderingSettingsChanged;
	}
#endif//WITH_EDITOR
private:
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> ToonRampTexture;
public:
	void LoadDefaultObjects();
#if WITH_EDITOR
	FSoftObjectPath CachedToonRampTextureNameClass;
#endif//WITH_EDITOR
};
