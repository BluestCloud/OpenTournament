// Copyright (c) 2019 Open Tournament Project, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UR_PickupBase.h"
#include "UR_WeaponBase.generated.h"

/**
 * 
 */
UCLASS()
class OPENTOURNAMENT_API AUR_WeaponBase : public AUR_PickupBase
{
	GENERATED_BODY()

public:
	AUR_WeaponBase();

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<class AUR_Weapon> WeaponClass;

protected:
#if WITH_EDITOR
	virtual void CheckForErrors() override;
#endif
	virtual void BeginPlay() override;

public:
	virtual void GiveTo_Implementation(class AActor* Other) override;
	virtual FText GetItemName_Implementation() override;

	/**
	* BP Utility - get the weapon mesh associated with a weapon class.
	* TODO: We should probably use static mesh instead of skeletal.
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon")
	static class USkeletalMesh* GetMeshForWeapon(const TSubclassOf<class AUR_Weapon> InWeaponClass);

};
