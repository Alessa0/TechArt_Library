// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "../../Source/Runtime/Engine/Classes/Engine/UToonRenderingSettings.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeUToonRenderingSettings() {}
// Cross Module References
	COREUOBJECT_API UScriptStruct* Z_Construct_UScriptStruct_FSoftObjectPath();
	DEVELOPERSETTINGS_API UClass* Z_Construct_UClass_UDeveloperSettings();
	ENGINE_API UClass* Z_Construct_UClass_UTexture2D_NoRegister();
	ENGINE_API UClass* Z_Construct_UClass_UToonRenderingSettings();
	ENGINE_API UClass* Z_Construct_UClass_UToonRenderingSettings_NoRegister();
	UPackage* Z_Construct_UPackage__Script_Engine();
// End Cross Module References
	void UToonRenderingSettings::StaticRegisterNativesUToonRenderingSettings()
	{
	}
	IMPLEMENT_CLASS_NO_AUTO_REGISTRATION(UToonRenderingSettings);
	UClass* Z_Construct_UClass_UToonRenderingSettings_NoRegister()
	{
		return UToonRenderingSettings::StaticClass();
	}
	struct Z_Construct_UClass_UToonRenderingSettings_Statics
	{
		static UObject* (*const DependentSingletons[])();
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[];
#endif
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam NewProp_ToonRampTextureName_MetaData[];
#endif
		static const UECodeGen_Private::FStructPropertyParams NewProp_ToonRampTextureName;
#if WITH_METADATA
		static const UECodeGen_Private::FMetaDataPairParam NewProp_ToonRampTexture_MetaData[];
#endif
		static const UECodeGen_Private::FObjectPtrPropertyParams NewProp_ToonRampTexture;
		static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
		static const FCppClassTypeInfoStatic StaticCppClassTypeInfo;
		static const UECodeGen_Private::FClassParams ClassParams;
	};
	UObject* (*const Z_Construct_UClass_UToonRenderingSettings_Statics::DependentSingletons[])() = {
		(UObject* (*)())Z_Construct_UClass_UDeveloperSettings,
		(UObject* (*)())Z_Construct_UPackage__Script_Engine,
	};
	static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UToonRenderingSettings_Statics::DependentSingletons) < 16);
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UClass_UToonRenderingSettings_Statics::Class_MetaDataParams[] = {
		{ "Comment", "/**\n *\n */" },
		{ "DisplayName", "ToonRamp" },
		{ "IncludePath", "Engine/UToonRenderingSettings.h" },
		{ "ModuleRelativePath", "Classes/Engine/UToonRenderingSettings.h" },
	};
#endif
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UClass_UToonRenderingSettings_Statics::NewProp_ToonRampTextureName_MetaData[] = {
		{ "AllowedClasses", "/Script/CroeUObject.Class'/Script/Engine.CurveLinearColorAtlas'" },
		{ "Category", "PreIntegrated-RampTexture" },
		{ "DisplayName", "Curve LinearColor Atlas" },
		{ "ModuleRelativePath", "Classes/Engine/UToonRenderingSettings.h" },
	};
#endif
	const UECodeGen_Private::FStructPropertyParams Z_Construct_UClass_UToonRenderingSettings_Statics::NewProp_ToonRampTextureName = { "ToonRampTextureName", nullptr, (EPropertyFlags)0x0010000000004001, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UToonRenderingSettings, ToonRampTextureName), Z_Construct_UScriptStruct_FSoftObjectPath, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UToonRenderingSettings_Statics::NewProp_ToonRampTextureName_MetaData), Z_Construct_UClass_UToonRenderingSettings_Statics::NewProp_ToonRampTextureName_MetaData) };
#if WITH_METADATA
	const UECodeGen_Private::FMetaDataPairParam Z_Construct_UClass_UToonRenderingSettings_Statics::NewProp_ToonRampTexture_MetaData[] = {
		{ "ModuleRelativePath", "Classes/Engine/UToonRenderingSettings.h" },
	};
#endif
	const UECodeGen_Private::FObjectPtrPropertyParams Z_Construct_UClass_UToonRenderingSettings_Statics::NewProp_ToonRampTexture = { "ToonRampTexture", nullptr, (EPropertyFlags)0x0044000000002000, UECodeGen_Private::EPropertyGenFlags::Object | UECodeGen_Private::EPropertyGenFlags::ObjectPtr, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UToonRenderingSettings, ToonRampTexture), Z_Construct_UClass_UTexture2D_NoRegister, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UToonRenderingSettings_Statics::NewProp_ToonRampTexture_MetaData), Z_Construct_UClass_UToonRenderingSettings_Statics::NewProp_ToonRampTexture_MetaData) };
	const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_UToonRenderingSettings_Statics::PropPointers[] = {
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UToonRenderingSettings_Statics::NewProp_ToonRampTextureName,
		(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UToonRenderingSettings_Statics::NewProp_ToonRampTexture,
	};
	const FCppClassTypeInfoStatic Z_Construct_UClass_UToonRenderingSettings_Statics::StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UToonRenderingSettings>::IsAbstract,
	};
	const UECodeGen_Private::FClassParams Z_Construct_UClass_UToonRenderingSettings_Statics::ClassParams = {
		&UToonRenderingSettings::StaticClass,
		"Engine",
		&StaticCppClassTypeInfo,
		DependentSingletons,
		nullptr,
		Z_Construct_UClass_UToonRenderingSettings_Statics::PropPointers,
		nullptr,
		UE_ARRAY_COUNT(DependentSingletons),
		0,
		UE_ARRAY_COUNT(Z_Construct_UClass_UToonRenderingSettings_Statics::PropPointers),
		0,
		0x001000A6u,
		METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UToonRenderingSettings_Statics::Class_MetaDataParams), Z_Construct_UClass_UToonRenderingSettings_Statics::Class_MetaDataParams)
	};
	static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UToonRenderingSettings_Statics::PropPointers) < 2048);
	UClass* Z_Construct_UClass_UToonRenderingSettings()
	{
		if (!Z_Registration_Info_UClass_UToonRenderingSettings.OuterSingleton)
		{
			UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UToonRenderingSettings.OuterSingleton, Z_Construct_UClass_UToonRenderingSettings_Statics::ClassParams);
		}
		return Z_Registration_Info_UClass_UToonRenderingSettings.OuterSingleton;
	}
	template<> ENGINE_API UClass* StaticClass<UToonRenderingSettings>()
	{
		return UToonRenderingSettings::StaticClass();
	}
	DEFINE_VTABLE_PTR_HELPER_CTOR(UToonRenderingSettings);
	UToonRenderingSettings::~UToonRenderingSettings() {}
	struct Z_CompiledInDeferFile_FID_Engine_Source_Runtime_Engine_Classes_Engine_UToonRenderingSettings_h_Statics
	{
		static const FClassRegisterCompiledInInfo ClassInfo[];
	};
	const FClassRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Engine_Source_Runtime_Engine_Classes_Engine_UToonRenderingSettings_h_Statics::ClassInfo[] = {
		{ Z_Construct_UClass_UToonRenderingSettings, UToonRenderingSettings::StaticClass, TEXT("UToonRenderingSettings"), &Z_Registration_Info_UClass_UToonRenderingSettings, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UToonRenderingSettings), 1288485481U) },
	};
	static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_Engine_Source_Runtime_Engine_Classes_Engine_UToonRenderingSettings_h_2671136584(TEXT("/Script/Engine"),
		Z_CompiledInDeferFile_FID_Engine_Source_Runtime_Engine_Classes_Engine_UToonRenderingSettings_h_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_Engine_Source_Runtime_Engine_Classes_Engine_UToonRenderingSettings_h_Statics::ClassInfo),
		nullptr, 0,
		nullptr, 0);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
