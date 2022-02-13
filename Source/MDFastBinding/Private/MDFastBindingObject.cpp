﻿#include "MDFastBindingObject.h"

#include "MDFastBinding.h"
#include "MDFastBindingContainer.h"
#include "BindingValues/MDFastBindingValueBase.h"

#define LOCTEXT_NAMESPACE "MDFastBindingObject"

FMDFastBindingItem::~FMDFastBindingItem()
{
	if (AllocatedDefaultValue != nullptr)
	{
		FMemory::Free(AllocatedDefaultValue);
	}
}

TTuple<const FProperty*, void*> FMDFastBindingItem::GetValue(UObject* SourceObject)
{
	const FProperty* ItemProp = ItemProperty.Get();
	if (ItemProp == nullptr)
	{
		return {};
	}
	
	if (Value != nullptr)
	{
		return Value->GetValue(SourceObject);
	}
	else if (AllocatedDefaultValue != nullptr)
	{
		return TTuple<const FProperty*, void*>{ ItemProp, AllocatedDefaultValue };
	}
	else if (ItemProp->IsA<FObjectPropertyBase>())
	{
		return TTuple<const FProperty*, void*>{ ItemProp, &DefaultObject };
	}
	else if (ItemProp->IsA<FTextProperty>())
	{
		return TTuple<const FProperty*, void*>{ ItemProp, &DefaultText };
	}
	else if (ItemProp->IsA<FStrProperty>())
	{
		return TTuple<const FProperty*, void*>{ ItemProp, &DefaultString };
	}
	else if (!DefaultString.IsEmpty())
	{
		AllocatedDefaultValue = FMemory::Malloc(ItemProp->GetSize(), ItemProp->GetMinAlignment());
		ItemProp->InitializeValue(AllocatedDefaultValue);
		ItemProp->ImportText(*DefaultString, AllocatedDefaultValue, PPF_None, nullptr);
		return TTuple<const FProperty*, void*>{ ItemProp, AllocatedDefaultValue };
	}

	return {};
}

UClass* UMDFastBindingObject::GetBindingOuterClass() const
{
#if !WITH_EDITOR
	if (BindingOuterClass.IsValid())
	{
		return BindingOuterClass.Get();
	}
#endif

	const UObject* Object = this;
	while (Object != nullptr)
	{
		if (Object->IsA<UMDFastBindingContainer>() && Object->GetOuter() != nullptr)
		{
			UClass* Class = Object->GetOuter()->GetClass();
			BindingOuterClass = Class;
			return Class;
		}

		Object = Object->GetOuter();
	}

	return nullptr;
}

void UMDFastBindingObject::PostLoad()
{
	Super::PostLoad();

	SetupBindingItems();
}

void UMDFastBindingObject::PostInitProperties()
{
	Super::PostInitProperties();

	SetupBindingItems();
}

void UMDFastBindingObject::EnsureBindingItemExists(const FName& ItemName, const FProperty* ItemProperty, const FText& ItemDescription, bool bIsOptional)
{
	FMDFastBindingItem* BindingItem = BindingItems.FindByKey(ItemName);
	if (BindingItem == nullptr)
	{
		FMDFastBindingItem Item;
		Item.ItemName = ItemName;
		BindingItems.Add(MoveTemp(Item));
		BindingItem = BindingItems.FindByKey(ItemName);
	}

	BindingItem->bAllowNullValue = bIsOptional;
	BindingItem->ItemProperty = ItemProperty;
	BindingItem->ToolTip = ItemDescription;
}

const FProperty* UMDFastBindingObject::GetBindingItemValueProperty(const FName& Name) const
{
	if (const FMDFastBindingItem* Item = BindingItems.FindByKey(Name))
	{
		if (Item->Value != nullptr)
		{
			return Item->Value->GetOutputProperty();
		}
	}

	return nullptr;
}

TTuple<const FProperty*, void*> UMDFastBindingObject::GetBindingItemValue(UObject* SourceObject,
                                                                          const FName& Name)
{
	if (FMDFastBindingItem* Item = BindingItems.FindByKey(Name))
	{
		return Item->GetValue(SourceObject);
	}
	
	return {};
}

TTuple<const FProperty*, void*> UMDFastBindingObject::GetBindingItemValue(UObject* SourceObject, int32 Index)
{
	if (BindingItems.IsValidIndex(Index))
	{
		return BindingItems[Index].GetValue(SourceObject);
	}
	
	return {};
}

#if WITH_EDITOR
EDataValidationResult UMDFastBindingObject::IsDataValid(TArray<FText>& ValidationErrors)
{
	for (const FMDFastBindingItem& BindingItem : BindingItems)
	{
		if (!BindingItem.ItemProperty.IsValid())
		{
			ValidationErrors.Add(FText::Format(LOCTEXT("BindingItemNullTypeError", "Pin '{0}' is missing an expected type"), FText::FromName(BindingItem.ItemName)));
			return EDataValidationResult::Invalid;
		}

		if (BindingItem.Value == nullptr && !BindingItem.bAllowNullValue)
		{
			const FProperty* ItemProp = BindingItem.ItemProperty.Get();
			if (!ItemProp->IsA<FObjectPropertyBase>() && !ItemProp->IsA<FTextProperty>() && !ItemProp->IsA<FStrProperty>() && BindingItem.DefaultString.IsEmpty())
			{
				ValidationErrors.Add(FText::Format(LOCTEXT("NullBindingItemValueError", "Pin '{0}' is empty"), FText::FromName(BindingItem.ItemName)));
				return EDataValidationResult::Invalid;
			}
		}
		else if (BindingItem.Value != nullptr)
		{
			const FProperty* OutputProp = BindingItem.Value->GetOutputProperty();
			if (OutputProp == nullptr)
			{
				ValidationErrors.Add(FText::Format(LOCTEXT("NullBindingItemValueTypeError", "Node '{0}' connected to Pin '{1}' has null output type"), BindingItem.Value->GetDisplayName(), FText::FromName(BindingItem.ItemName)));
				return EDataValidationResult::Invalid;
			}

			if (!FMDFastBindingModule::CanSetProperty(BindingItem.ItemProperty.Get(), OutputProp))
			{
				ValidationErrors.Add(FText::Format(LOCTEXT("BindingItemTypeMismatchError", "Pin '{0}' expects type '{1}' but Value '{2}' has type '{3}'")
					, FText::FromName(BindingItem.ItemName)
					, FText::FromString(BindingItem.ItemProperty->GetCPPType())
					, BindingItem.Value->GetDisplayName()
					, FText::FromString(OutputProp->GetCPPType())));
				return EDataValidationResult::Invalid;
			}
		}
	}

	return EDataValidationResult::Valid;
}

void UMDFastBindingObject::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	SetupBindingItems();
}
#endif

#if WITH_EDITORONLY_DATA
FText UMDFastBindingObject::GetDisplayName()
{
	return DevName.IsEmptyOrWhitespace() ? GetClass()->GetDisplayNameText() : DevName;
}

FText UMDFastBindingObject::GetToolTipText()
{
	return GetClass()->GetToolTipText();
}

const FMDFastBindingItem* UMDFastBindingObject::FindBindingItem(const FName& ItemName) const
{
	return BindingItems.FindByKey(ItemName);
}

FMDFastBindingItem* UMDFastBindingObject::FindBindingItem(const FName& ItemName)
{
	return BindingItems.FindByKey(ItemName);
}

UMDFastBindingValueBase* UMDFastBindingObject::SetBindingItem(const FName& ItemName,
                                                              TSubclassOf<UMDFastBindingValueBase> ValueClass)
{
	if (FMDFastBindingItem* BindingItem = BindingItems.FindByKey(ItemName))
	{
		if (UMDFastBindingValueBase* NewValue = NewObject<UMDFastBindingValueBase>(this, ValueClass, NAME_None, RF_Public | RF_Transactional))
		{
			BindingItem->Value = NewValue;
			BindingItem->ClearDefaultValues();
			return NewValue;
		}
	}

	return nullptr;
}

void UMDFastBindingObject::ClearBindingItemValue(const FName& ItemName)
{
	if (FMDFastBindingItem* BindingItem = BindingItems.FindByKey(ItemName))
	{
		BindingItem->Value = nullptr;
		BindingItem->ClearDefaultValues();
	}
}
#endif

#undef LOCTEXT_NAMESPACE
