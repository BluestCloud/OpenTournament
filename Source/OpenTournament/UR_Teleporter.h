// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UR_Teleporter.generated.h"

class UArrowComponent;
class UAudioComponent;
class UCapsuleComponent;
class UStaticMeshComponent;
class UParticleSystemComponent;

UENUM()
enum class EExitRotation : uint8
{
    Relative,
    Fixed
};

UCLASS()
class OPENTOURNAMENT_API AUR_Teleporter : public AActor
{
    GENERATED_BODY()

public:
    // Sets default values for this actor's properties
    AUR_Teleporter(const FObjectInitializer& ObjectInitializer);

    /*
    * Static Mesh Component - Teleporter Base
    */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Teleporter", meta=(AllowPrivateAccess = "true"))
    UStaticMeshComponent* MeshComponent;

    /*
    * Capsule Component - Active Teleport Region
    */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Teleporter", meta=(AllowPrivateAccess = "true"))
    UCapsuleComponent* CapsuleComponent;

    /*
    * Audio Component
    */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Teleporter", meta=(AllowPrivateAccess = "true"))
    UAudioComponent* AudioComponent;

    /*
    * Arrow Component
    */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Teleporter", meta=(AllowPrivateAccess = "true"))
    UArrowComponent* ArrowComponent;

    /*
    * ParticleSystem Component
    */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Teleporter", meta=(AllowPrivateAccess = "true"))
    UParticleSystemComponent* ParticleSystemComponent;


    /*
    * Destination of Teleport - May be another Teleporter, TargetPoint, etc.
    */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Teleporter")
    AActor* DestinationActor;

    /*
    * Does our teleport force us into a new rotation, or is it relative to our DestinationActor's rotation?
    */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Teleporter")
    EExitRotation ExitRotationType = EExitRotation::Relative;

    /*
    * Do Actors teleported retain their velocity?
    */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Teleporter")
    bool bKeepMomentum = true;


protected:
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

public:
    // Called every frame
    virtual void Tick(float DeltaTime) override;

    bool PerformTeleport(AActor* TargetActor);

    UFUNCTION()
    void OnTriggerEnter(class UPrimitiveComponent* HitComp, class AActor* Other, class UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
