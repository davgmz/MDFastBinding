﻿#pragma once

#include "CoreMinimal.h"
#include "MDFastBindingFunctionWrapper.h"
#include "MDFastBindingValueBase.h"
#include "MDFastBindingValue_Function.generated.h"

/**
 * Call a function and retrieve its return value
 */
UCLASS(meta = (DisplayName = "Call a Function"))
class MDFASTBINDING_API UMDFastBindingValue_Function : public UMDFastBindingValueBase
{
	GENERATED_BODY()

public:
	virtual TTuple<const FProperty*, void*> GetValue(UObject* SourceObject) override;
	virtual const FProperty* GetOutputProperty() override;
	virtual bool DoesBindingItemDefaultToSelf(const FName& InItemName) const override;

#if WITH_EDITORONLY_DATA
	virtual FText GetDisplayName() override;
#endif
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(TArray<FText>& ValidationErrors) override;
#endif

protected:
	virtual UObject* GetFunctionOwner(UObject* SourceObject);
	virtual UClass* GetFunctionOwnerClass();
	virtual void PopulateFunctionParam(UObject* SourceObject, const FProperty* Param, void* ValuePtr);
	virtual bool IsFunctionValid(UFunction* Func, const FProperty* ReturnValue, const TArray<const FProperty*>& Params) const;
	
	virtual void SetupBindingItems() override;

	virtual void PostInitProperties() override;
	
	UPROPERTY(EditDefaultsOnly, Category = "Binding")
	FMDFastBindingFunctionWrapper Function;

	UPROPERTY(Transient)
	UObject* ObjectProperty = nullptr;

	UPROPERTY(Transient)
	bool bAddPathRootBindingItem = true;
};
