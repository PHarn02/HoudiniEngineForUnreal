/*
 * PROPRIETARY INFORMATION.  This software is proprietary to
 * Side Effects Software Inc., and is not to be reproduced,
 * transmitted, or disclosed in any way without written permission.
 *
 * Produced by:
 *      Mykola Konyk
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 */

#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniAssetInstanceInputField.h"


bool
FHoudiniAssetInstanceInputFieldSortPredicate::operator()(const UHoudiniAssetInstanceInputField& A, 
	const UHoudiniAssetInstanceInputField& B) const
{
	FHoudiniGeoPartObjectSortPredicate HoudiniGeoPartObjectSortPredicate;
	return HoudiniGeoPartObjectSortPredicate(A.GetHoudiniGeoPartObject(), B.GetHoudiniGeoPartObject());
}


UHoudiniAssetInstanceInputField::UHoudiniAssetInstanceInputField(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	OriginalStaticMesh(nullptr),		
	HoudiniAssetComponent(nullptr),
	InstancePathName(TEXT("")),		
	HoudiniAssetInstanceInputFieldFlagsPacked(0)
{
	RotationOffsets.Add(FRotator(0, 0, 0));
	ScaleOffsets.Add(FVector(1, 1, 1));
	bScaleOffsetsLinearlyArray.Add(true);
}


UHoudiniAssetInstanceInputField::~UHoudiniAssetInstanceInputField()
{

}


UHoudiniAssetInstanceInputField*
UHoudiniAssetInstanceInputField::Create(UHoudiniAssetComponent* HoudiniAssetComponent, 
	const FHoudiniGeoPartObject& HoudiniGeoPartObject, const FString& InstancePathName)
{
	UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField
		= NewObject<UHoudiniAssetInstanceInputField>(HoudiniAssetComponent, 
			UHoudiniAssetInstanceInputField::StaticClass(), NAME_None, RF_Public | RF_Transactional);

	HoudiniAssetInstanceInputField->HoudiniGeoPartObject = HoudiniGeoPartObject;
	HoudiniAssetInstanceInputField->HoudiniAssetComponent = HoudiniAssetComponent;
	HoudiniAssetInstanceInputField->InstancePathName = InstancePathName;

	return HoudiniAssetInstanceInputField;
}


UHoudiniAssetInstanceInputField*
UHoudiniAssetInstanceInputField::Create(UHoudiniAssetComponent* InHoudiniAssetComponent, 
	UHoudiniAssetInstanceInputField* OtherInputField)
{
	UHoudiniAssetInstanceInputField* InputField
		= NewObject<UHoudiniAssetInstanceInputField>(InHoudiniAssetComponent, 
			UHoudiniAssetInstanceInputField::StaticClass(), NAME_None, RF_Public | RF_Transactional);

	InputField->HoudiniGeoPartObject = OtherInputField->HoudiniGeoPartObject;
	InputField->HoudiniAssetComponent = InHoudiniAssetComponent;
	InputField->InstancePathName = OtherInputField->InstancePathName;
	InputField->RotationOffsets = OtherInputField->RotationOffsets;
	InputField->ScaleOffsets = OtherInputField->ScaleOffsets;
	InputField->bScaleOffsetsLinearlyArray = OtherInputField->bScaleOffsetsLinearlyArray;
	InputField->InstancedTransforms = OtherInputField->InstancedTransforms;
	InputField->StaticMeshes = OtherInputField->StaticMeshes;
	InputField->OriginalStaticMesh = OtherInputField->OriginalStaticMesh;

	return InputField;
}


void
UHoudiniAssetInstanceInputField::Serialize(FArchive& Ar)
{
	// Call base implementation first.
	Super::Serialize(Ar);

	Ar << HoudiniAssetInstanceInputFieldFlagsPacked;
	HoudiniGeoPartObject.Serialize(Ar);

	Ar << InstancePathName;
	Ar << RotationOffsets;
	Ar << ScaleOffsets;
	Ar << bScaleOffsetsLinearlyArray;

	Ar << InstancedTransforms;
	Ar << InstancedStaticMeshComponents;	
	Ar << StaticMeshes;    
	Ar << OriginalStaticMesh;
}


void
UHoudiniAssetInstanceInputField::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UHoudiniAssetInstanceInputField* HoudiniAssetInstanceInputField = Cast<UHoudiniAssetInstanceInputField>(InThis);
	if(HoudiniAssetInstanceInputField)// && !HoudiniAssetInstanceInputField->IsPendingKill())
	{
		if(HoudiniAssetInstanceInputField->OriginalStaticMesh)
		{
			Collector.AddReferencedObject(HoudiniAssetInstanceInputField->OriginalStaticMesh, InThis);
		}

		for (int32 Idx = 0; Idx < HoudiniAssetInstanceInputField->StaticMeshes.Num(); ++Idx)
		{
			UStaticMesh* StaticMesh = HoudiniAssetInstanceInputField->StaticMeshes[Idx];
			Collector.AddReferencedObject(StaticMesh, InThis);
		}
		
		for (int32 Idx = 0; Idx < HoudiniAssetInstanceInputField->InstancedStaticMeshComponents.Num(); ++Idx)
		{
			UInstancedStaticMeshComponent* InstancedStaticMeshComponent = HoudiniAssetInstanceInputField->InstancedStaticMeshComponents[Idx];            
			Collector.AddReferencedObject(InstancedStaticMeshComponent, InThis);
		}
		
	}

	// Call base implementation.
	Super::AddReferencedObjects(InThis, Collector);
}


void
UHoudiniAssetInstanceInputField::BeginDestroy()
{
	UInstancedStaticMeshComponent* InstancedStaticMeshComponent
		= InstancedStaticMeshComponents.Num() ? InstancedStaticMeshComponents[0] : NULL;
	if(InstancedStaticMeshComponent)
	{
		InstancedStaticMeshComponent->UnregisterComponent();
		InstancedStaticMeshComponent->DetachFromParent();
		InstancedStaticMeshComponent->DestroyComponent();

		if(HoudiniAssetComponent)
		{
			HoudiniAssetComponent->AttachChildren.Remove(InstancedStaticMeshComponent);
		}
	}

	Super::BeginDestroy();
}


void
UHoudiniAssetInstanceInputField::PostEditUndo()
{
	Super::PostEditUndo();

	UInstancedStaticMeshComponent* InstancedStaticMeshComponent
		= InstancedStaticMeshComponents.Num() ? InstancedStaticMeshComponents[0] : NULL;

	if(InstancedStaticMeshComponent)
	{		
		UStaticMesh* StaticMesh = StaticMeshes.Num() > 0 ? StaticMeshes[0] : NULL;
		InstancedStaticMeshComponent->SetStaticMesh(StaticMesh);
	}

	SetRotationOffset(RotationOffsets[0]);
	SetScaleOffset(ScaleOffsets[0]);

	UpdateInstanceTransforms();

	if(HoudiniAssetComponent)
	{
		HoudiniAssetComponent->UpdateEditorProperties(false);
	}
}


void
UHoudiniAssetInstanceInputField::CreateInstancedComponent()
{
	check(StaticMeshes.Num() > 0);
	
	UStaticMesh* StaticMesh = StaticMeshes[0];
	check(HoudiniAssetComponent);

	if(InstancedStaticMeshComponents.Num() <= 0)
	{
		UInstancedStaticMeshComponent* InstancedStaticMeshComponent = NewObject<UInstancedStaticMeshComponent>(HoudiniAssetComponent->GetOwner(),
			UInstancedStaticMeshComponent::StaticClass(), NAME_None);
		InstancedStaticMeshComponents.Add(InstancedStaticMeshComponent);
	}

	// Assign static mesh to this instanced component.
	
	InstancedStaticMeshComponents[0]->SetStaticMesh(StaticMesh);
	InstancedStaticMeshComponents[0]->AttachTo(HoudiniAssetComponent);
	InstancedStaticMeshComponents[0]->RegisterComponent();
	InstancedStaticMeshComponents[0]->GetBodyInstance()->bAutoWeld = false;

	// We want to make this invisible if it's a collision instancer.
	InstancedStaticMeshComponents[0]->SetVisibility(!HoudiniGeoPartObject.bIsCollidable);
}


void
UHoudiniAssetInstanceInputField::SetInstanceTransforms(const TArray<FTransform>& ObjectTransforms)
{
	InstancedTransforms = ObjectTransforms;
	UpdateInstanceTransforms();
}


void
UHoudiniAssetInstanceInputField::UpdateInstanceTransforms()
{
	UInstancedStaticMeshComponent* InstancedStaticMeshComponent = 
		InstancedStaticMeshComponents.Num() > 0 ? InstancedStaticMeshComponents[0] : NULL;
	FHoudiniEngineUtils::UpdateInstancedStaticMeshComponentInstances(InstancedStaticMeshComponent, InstancedTransforms,
		RotationOffsets[0], ScaleOffsets[0]);
}


void
UHoudiniAssetInstanceInputField::UpdateRelativeTransform()
{
	check(InstancedStaticMeshComponents.Num() > 0);
	InstancedStaticMeshComponents[0]->SetRelativeTransform(HoudiniGeoPartObject.TransformMatrix);
}


const FHoudiniGeoPartObject&
UHoudiniAssetInstanceInputField::GetHoudiniGeoPartObject() const
{
	return HoudiniGeoPartObject;
}


UStaticMesh*
UHoudiniAssetInstanceInputField::GetOriginalStaticMesh() const
{
	return OriginalStaticMesh;
}


UStaticMesh*
UHoudiniAssetInstanceInputField::GetStaticMesh() const
{
	if (StaticMeshes.Num() <= 0)
		return NULL;
	
	UStaticMesh* StaticMesh = StaticMeshes[0];
	return StaticMesh;
}


void
UHoudiniAssetInstanceInputField::SetStaticMesh(UStaticMesh* InStaticMesh)
{
	check(InStaticMesh);
	if (StaticMeshes.Num() > 0)
		StaticMeshes[0] = InStaticMesh;
	else
		StaticMeshes.Add(InStaticMesh);

	check(InstancedStaticMeshComponents.Num() > 0);

	InstancedStaticMeshComponents[0]->SetStaticMesh(InStaticMesh);
}

void 
UHoudiniAssetInstanceInputField::AddInstanceVariation(UStaticMesh * InStaticMesh)
{
	check(InStaticMesh);
	
	StaticMeshes.Add(InStaticMesh);
	// Create instanced component.
	CreateInstancedComponent();
}

void
UHoudiniAssetInstanceInputField::ReplaceInstanceVariation(
								UStaticMesh * InStaticMesh,
								int Index)
{
	check(InStaticMesh);
	check(Index >= 0 && Index < StaticMeshes.Num());	
	check(InstancedStaticMeshComponents.Num() == StaticMeshes.Num());

	StaticMeshes[Index] = InStaticMesh;
	InstancedStaticMeshComponents[Index]->SetStaticMesh(InStaticMesh);
}

void 
UHoudiniAssetInstanceInputField::FindStaticMeshIndices(
							UStaticMesh * InStaticMesh, 
							TArray<int> & Indices)
{
	for (int32 Idx = 0; Idx < StaticMeshes.Num(); ++Idx)
	{
		UStaticMesh* StaticMesh = StaticMeshes[Idx];
		if (StaticMesh == InStaticMesh)
		{
			Indices.Add(Idx);
		}
	}	
}

void
UHoudiniAssetInstanceInputField::AssignThumbnailBorder(TSharedPtr<SBorder> InThumbnailBorder)
{
	ThumbnailBorder = InThumbnailBorder;
}


TSharedPtr<SBorder>
UHoudiniAssetInstanceInputField::GetThumbnailBorder() const
{
	return ThumbnailBorder;
}


void
UHoudiniAssetInstanceInputField::AssignComboButton(TSharedPtr<SComboButton> InComboButton)
{
	StaticMeshComboButton = InComboButton;
}


TSharedPtr<SComboButton>
UHoudiniAssetInstanceInputField::GetComboButton() const
{
	return StaticMeshComboButton;
}


const FRotator&
UHoudiniAssetInstanceInputField::GetRotationOffset() const
{
	return RotationOffsets[0];
}


void
UHoudiniAssetInstanceInputField::SetRotationOffset(const FRotator& Rotator)
{
	RotationOffsets[0] = Rotator;
}


const FVector&
UHoudiniAssetInstanceInputField::GetScaleOffset() const
{
	return ScaleOffsets[0];
}


void
UHoudiniAssetInstanceInputField::SetScaleOffset(const FVector& InScale)
{
	ScaleOffsets[0] = InScale;
}


bool
UHoudiniAssetInstanceInputField::AreOffsetsScaledLinearly() const
{
	return bScaleOffsetsLinearlyArray[0];
}


void
UHoudiniAssetInstanceInputField::SetLinearOffsetScale(bool bEnabled)
{
	bScaleOffsetsLinearlyArray[0] = bEnabled;
}


bool
UHoudiniAssetInstanceInputField::IsOriginalStaticMeshUsed() const
{
	UStaticMesh* StaticMesh = StaticMeshes.Num() > 0 ? StaticMeshes[0] : NULL;
	return OriginalStaticMesh == StaticMesh;
}


UInstancedStaticMeshComponent*
UHoudiniAssetInstanceInputField::GetInstancedStaticMeshComponent() const
{
	if (InstancedStaticMeshComponents.Num() <= 0)
		return NULL;
	return InstancedStaticMeshComponents[0];
}


const TArray<FTransform>&
UHoudiniAssetInstanceInputField::GetInstancedTransforms() const
{
	return InstancedTransforms;
}


void
UHoudiniAssetInstanceInputField::RecreateRenderState()
{
	if(InstancedStaticMeshComponents.Num() > 0)
	{
		InstancedStaticMeshComponents[0]->RecreateRenderState_Concurrent();
	}
}


void
UHoudiniAssetInstanceInputField::RecreatePhysicsState()
{
	if(InstancedStaticMeshComponents.Num() > 0)
	{
		InstancedStaticMeshComponents[0]->RecreatePhysicsState();
	}
}