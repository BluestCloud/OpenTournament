// Copyright (c) 2019-2020 Open Tournament Project, All Rights Reserved.

/////////////////////////////////////////////////////////////////////////////////////////////////

#include "UR_Character.h"

#include "Net/UnrealNetwork.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/GameState.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"

#include "OpenTournament.h"
#include "UR_InventoryComponent.h"
#include "UR_CharacterMovementComponent.h"
#include "UR_AttributeSet.h"
#include "UR_AbilitySystemComponent.h"
#include "UR_GameplayAbility.h"
#include "UR_PlayerController.h"
#include "UR_GameMode.h"
#include "UR_Weapon.h"
#include "UR_Projectile.h"


/////////////////////////////////////////////////////////////////////////////////////////////////

AUR_Character::AUR_Character(const FObjectInitializer& ObjectInitializer) :
    Super(ObjectInitializer.SetDefaultSubobjectClass<UUR_CharacterMovementComponent>(ACharacter::CharacterMovementComponentName)),
    FootstepTimestamp(0.f),
    FootstepTimeIntervalBase(0.300f),
    FallDamageSpeedThreshold(2675.f),
    CrouchTransitionSpeed(12.f)
{
    // Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;

    bReplicates = true;

    // Unreal & UT99 Values (Scaling Factor 2.5)
    GetCapsuleComponent()->InitCapsuleSize(42.5, 97.5);

    URMovementComponent = Cast<UUR_CharacterMovementComponent>(GetCharacterMovement());
    URMovementComponent->bUseFlatBaseForFloorChecks = true;

    InventoryComponent = Cast<UUR_InventoryComponent>(CreateDefaultSubobject<UUR_InventoryComponent>(TEXT("InventoryComponent")));

    // Create a CameraComponent	
    CharacterCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    CharacterCameraComponent->SetupAttachment(GetCapsuleComponent());
    CharacterCameraComponent->SetRelativeLocation(FVector(-39.56f, 1.75f, BaseEyeHeight)); // Position the camera
    CharacterCameraComponent->bUsePawnControlRotation = true;
    BaseEyeHeight = 64.f;
    CrouchedEyeHeight = 64.f;

    // Create a mesh component that will be used when being viewed from a '1st person' view (when controlling this pawn)
    MeshFirstPerson = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshFirstPerson"));
    MeshFirstPerson->SetOnlyOwnerSee(true);
    MeshFirstPerson->SetupAttachment(CharacterCameraComponent);
    MeshFirstPerson->bCastDynamicShadow = false;
    MeshFirstPerson->CastShadow = false;
    MeshFirstPerson->SetRelativeRotation(FRotator(1.9f, -19.19f, 5.2f));
    MeshFirstPerson->SetRelativeLocation(FVector(-0.5f, -4.4f, -155.7f));

    WeaponAttachPoint = FName(TEXT("GripPoint"));

    // Mesh third person
    GetMesh()->bOwnerNoSee = true;

    // Third person camera
    ThirdPersonArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("ThirdPersonArm"));
    ThirdPersonArm->SetupAttachment(GetCapsuleComponent());
    ThirdPersonArm->TargetArmLength = 400.f;
    ThirdPersonArm->TargetOffset.Set(0.f, 0.f, 100.f);
    ThirdPersonArm->bUsePawnControlRotation = true;

    ThirdPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ThirdPersonCamera"));
    ThirdPersonCamera->SetupAttachment(ThirdPersonArm);
    
    // Create the attribute set, this replicates by default
    AttributeSet = CreateDefaultSubobject<UUR_AttributeSet>(TEXT("AttributeSet"));

    // Create the ASC
    AbilitySystemComponent = CreateDefaultSubobject<UUR_AbilitySystemComponent>("AbilitySystemComponent");
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void AUR_Character::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(AUR_Character, DodgeDirection);
    DOREPLIFETIME(AUR_Character, InventoryComponent);
    DOREPLIFETIME(AUR_Character, AbilitySystemComponent);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void AUR_Character::BeginPlay()
{
    Super::BeginPlay();

    AttributeSet->SetHealth(100.f);
    AttributeSet->SetHealthMax(100.f);
    AttributeSet->SetArmor(100.f);
    AttributeSet->SetArmorMax(100.f);
    AttributeSet->SetShieldMax(100.f);
}

void AUR_Character::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    TickFootsteps(DeltaTime);
    TickEyePosition(DeltaTime);
}

UAbilitySystemComponent* AUR_Character::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

void AUR_Character::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    PlayerInputComponent->BindAction("Pickup", IE_Pressed, this, &AUR_Character::BeginPickup);
    PlayerInputComponent->BindAction("Pickup", IE_Released, this, &AUR_Character::EndPickup);

    PlayerInputComponent->BindAction("ShowInventory", IE_Pressed, this, &AUR_Character::ShowInventory);

    PlayerInputComponent->BindAction("ARifle", IE_Pressed, this, &AUR_Character::SelectWeapon1);
    PlayerInputComponent->BindAction("Shotgun", IE_Pressed, this, &AUR_Character::SelectWeapon2);
    PlayerInputComponent->BindAction("RLauncher", IE_Pressed, this, &AUR_Character::SelectWeapon3);
    PlayerInputComponent->BindAction("GLauncher", IE_Pressed, this, &AUR_Character::SelectWeapon4);
    PlayerInputComponent->BindAction("SRifle", IE_Pressed, this, &AUR_Character::SelectWeapon5);
    PlayerInputComponent->BindAction("Pistol", IE_Pressed, this, &AUR_Character::SelectWeapon0);

    // Select Weapon Bind
    // Throw Weapon

    // Voice
    // Ping
    // Emote

    PlayerInputComponent->BindAction("NextWeapon", IE_Pressed, this, &AUR_Character::NextWeapon);
    PlayerInputComponent->BindAction("PrevWeapon", IE_Pressed, this, &AUR_Character::PrevWeapon);
}

void AUR_Character::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
    UCameraComponent* Camera = PickCamera();
    if (Camera)
    {
        const bool bThirdPerson = IsThirdPersonCamera(Camera);
        if (bThirdPerson != bViewingThirdPerson)
        {
            bViewingThirdPerson = bThirdPerson;
            CameraViewChanged();
        }
        Camera->GetCameraView(DeltaTime, OutResult);
    }
    else
    {
        // Fallback to Super
        Super::CalcCamera(DeltaTime, OutResult);
    }
}

UCameraComponent* AUR_Character::PickCamera_Implementation()
{
    // End game = always 3p
    AGameState* GS = GetWorld()->GetGameState<AGameState>();
    if (GS && GS->HasMatchEnded() && ThirdPersonCamera)
    {
        return ThirdPersonCamera;
    }

    // Alive = 1p
    //TODO: specs should be able to switch at will.
    // That would be a variable in PlayerController.
    // Hopefully this is client-only, so we can get local PC to check this.
    if (IsAlive() && CharacterCameraComponent && CharacterCameraComponent->IsActive())
    {
        return CharacterCameraComponent;
    }

    // Fallback to 3p
    if (ThirdPersonCamera && ThirdPersonCamera->IsActive())
    {
        return ThirdPersonCamera;
    }

    return nullptr;
}

bool AUR_Character::IsThirdPersonCamera_Implementation(UCameraComponent* Camera)
{
    return Camera && Camera == ThirdPersonCamera;
}

void AUR_Character::CameraViewChanged_Implementation()
{
    GetMesh()->SetOwnerNoSee(!bViewingThirdPerson);
    MeshFirstPerson->SetVisibility(!bViewingThirdPerson, true);
    //TODO: weapon
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void AUR_Character::MoveForward(const float InValue)
{
    if (InValue != 0.0f)
    {
        const FRotator ControlRotation = GetControlRotation();
        FRotator ForwardRotation = ControlRotation;

        // If we aren't flying / swimming, our ForwardRotation should trim out pitch / roll values
        if (URMovementComponent != nullptr && !URMovementComponent->Is3DMovementMode())
        {
            ForwardRotation = FRotator(0, ControlRotation.Yaw, 0);
        }

        // Move in the direction calculated by our ForwardRotation
        AddMovementInput(FRotationMatrix(ForwardRotation).GetUnitAxis(EAxis::X), InValue);
    }
}

void AUR_Character::MoveRight(const float InValue)
{
    if (InValue != 0.0f)
    {
        // Strafing needs no Pitch, so just take our Control Rotation's Yaw
        const FRotator ControlRotation = GetControlRotation();
        const FRotator ForwardRotation(0, ControlRotation.Yaw, 0);

        // Add movement in the perpendicular Y axis
        AddMovementInput(FRotationMatrix(ForwardRotation).GetUnitAxis(EAxis::Y), InValue);
    }
}

void AUR_Character::MoveUp(const float InValue)
{
    if (InValue != 0.0f)
    {
        // add movement in up direction
        AddMovementInput(FVector(0.f, 0.f, 1.f), InValue);
    }
}

void AUR_Character::TickFootsteps(const float DeltaTime)
{
    const float VelocityMagnitude = GetCharacterMovement()->Velocity.Size();
    const float WalkingSpeedPercentage = GetCharacterMovement()->MaxWalkSpeed / VelocityMagnitude;
    const float TimeSeconds = GetWorld()->TimeSeconds;
    const float TimeSinceLastFootstep = TimeSeconds - FootstepTimestamp;

    if (GetCharacterMovement()->MovementMode == MOVE_Walking)
    {
        if (VelocityMagnitude > 0.0f && TimeSinceLastFootstep > FootstepTimeIntervalBase * WalkingSpeedPercentage)
        {
            PlayFootstepEffects(WalkingSpeedPercentage);
            FootstepTimestamp = TimeSeconds;
        }
    }
}

void AUR_Character::PlayFootstepEffects(const float WalkingSpeedPercentage) const
{
    const float FootstepVolume = FMath::Clamp<float>(0.2f, 1.f, WalkingSpeedPercentage);
    UGameplayStatics::PlaySound2D(GetWorld(), CharacterVoice.FootstepSound, FootstepVolume, 1.f);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void AUR_Character::RecalculateBaseEyeHeight()
{
    const float DefaultHalfHeight{ GetDefaultHalfHeight() };
    const float AbsoluteDifference = DefaultHalfHeight - ((GetCharacterMovement()->CrouchedHalfHeight / DefaultHalfHeight) * DefaultHalfHeight);
    EyeOffsetZ += bIsCrouched ? AbsoluteDifference : -1.f * AbsoluteDifference;
}

void AUR_Character::TickEyePosition(const float DeltaTime)
{
    const float StandingBonus{ bIsCrouched ? CrouchTransitionSpeed : 0.f };
    EyeOffsetZ =  FMath::FInterpTo(EyeOffsetZ, BaseEyeHeight, DeltaTime, CrouchTransitionSpeed + StandingBonus);

    CharacterCameraComponent->SetRelativeLocation(FVector(-39.56f, 1.75f, EyeOffsetZ));
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void AUR_Character::CheckJumpInput(float DeltaTime)
{
    if (URMovementComponent)
    {
        URMovementComponent->CheckJumpInput(DeltaTime);

        // If we reached this point and the flag indicating a jump is still set, then perform the jump effects
        if (bPressedJump)
        {
            if (JumpCurrentCount == 0)
            {
                JumpCurrentCount++;

                // Play effects for Jump
                OnJumped();
            }
        }
    }
}

void AUR_Character::ClearJumpInput(float DeltaTime)
{
    Super::ClearJumpInput(DeltaTime);
    if (URMovementComponent)
    {
        URMovementComponent->ClearDodgeInput();
    }
}

void AUR_Character::Landed(const FHitResult& Hit)
{
    TakeFallingDamage(Hit, GetCharacterMovement()->Velocity.Z);

    Super::Landed(Hit);
}

void AUR_Character::TakeFallingDamage(const FHitResult& Hit, float FallingSpeed)
{
    if (AbilitySystemComponent == nullptr || URMovementComponent == nullptr)
    {
        return;
    }

    if (FallingSpeed * -1.f > FallDamageSpeedThreshold)
    {
        const float FallingDamage = -0.15f * (FallDamageSpeedThreshold + FallingSpeed);
        //GAME_PRINT(10.f, FColor::Red, "Fall Damage (%f)", FallingDamage);

        if (FallingDamage >= 1.0f)
        {
            FDamageEvent DamageEvent; // @! TODO Real DamageTypes
            TakeDamage(FallingDamage, DamageEvent, Controller, this);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

bool AUR_Character::IsDodgeCapable_Implementation() const
{
    return (URMovementComponent != nullptr);
}

bool AUR_Character::IsDodgePermitted_Implementation() const
{
    bool IsPermitted = false;

    if (URMovementComponent != nullptr)
    {
        IsPermitted = !URMovementComponent->IsFlying();
        IsPermitted = IsPermitted && !URMovementComponent->bIsDodging;
        IsPermitted = IsPermitted && GetWorld()->TimeSeconds > URMovementComponent->DodgeResetTime;
    }

    return IsPermitted;
}

bool AUR_Character::CanDodge() const
{
    return IsDodgeCapable() && IsDodgePermitted();
}

void AUR_Character::Dodge(FVector DodgeDir, FVector DodgeCross)
{
    if (CanDodge())
    {
        if (DodgeOverride(DodgeDir, DodgeCross))
        {
            return;
        }

        URMovementComponent->PerformDodge(DodgeDir, DodgeCross);
        return;
    }

    if (URMovementComponent)
    {
        URMovementComponent->ClearDodgeInput();
    }
}

bool AUR_Character::ServerSetDodgeDirection_Validate(const EDodgeDirection InDodgeDirection)
{
    return true;
}
void AUR_Character::ServerSetDodgeDirection_Implementation(const EDodgeDirection InDodgeDirection)
{
    DodgeDirection = InDodgeDirection;
}

void AUR_Character::OnDodge_Implementation(const FVector& DodgeLocation, const FVector& DodgeDir)
{
    // @! TODO Effects
    if (CharacterVoice.DodgeSound != nullptr)
    {
        if (GetLocalRole() == ROLE_Authority)
        {
            UGameplayStatics::PlaySoundAtLocation(this, CharacterVoice.DodgeSound, GetActorLocation(), GetActorRotation());
        }
    }
}

void AUR_Character::OnWallDodge_Implementation(const FVector& DodgeLocation, const FVector& DodgeDir)
{
    // @! TODO Effects
    if (CharacterVoice.DodgeSound != nullptr)
    {
        if (GetLocalRole() == ROLE_Authority)
        {
            UGameplayStatics::PlaySoundAtLocation(this, CharacterVoice.DodgeSound, GetActorLocation(), GetActorRotation());
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

float AUR_Character::TakeDamage(float Damage, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    GAME_LOG(Game, Log, "Damage Incoming (%f)", Damage);

    if (!ShouldTakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser))
    {
        return 0.f;
    }

    // Super() takes care of calculating proper splash damage values and imparting components physics.
    // Then it triggers events : OnTakePointDamage, OnTakeRadialDamage, OnTakeAnyDamage.
    Damage = FMath::FloorToFloat(Super::TakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser));
    //GAME_PRINT(10.f, FColor::Purple, "Damage Floor (%f)", DamageToArmor);
    GAME_LOG(Game, Log, "Damage Incoming Floor (%f)", Damage);

    if (AttributeSet)
    {
        if (AttributeSet->Health.GetCurrentValue() > 0.f)
        {
            const float CurrentShield = FMath::FloorToFloat(AttributeSet->Shield.GetCurrentValue());
            if (CurrentShield > 0.f)
            {
                //GAME_PRINT(10.f, FColor::Yellow, "Damage to Shield (%f)", FMath::Min(Damage, CurrentShield));
                GAME_LOG(Game, Log, "Damage to Shield (%f)", FMath::Min(Damage, CurrentShield));

                AttributeSet->SetShield(FMath::FloorToFloat(FMath::Max(CurrentShield - Damage, 0.f)));
                Damage = CurrentShield - Damage > 0.f ? 0.f : Damage - CurrentShield;
            }

            const float CurrentArmor = FMath::FloorToFloat(AttributeSet->Armor.GetCurrentValue());
            const float ArmorAbsorption = AttributeSet->ArmorAbsorptionPercent.GetCurrentValue();
            if (CurrentArmor > 0.f && Damage > 0.f)
            {
                const float DamageToArmor = FMath::FloorToFloat(FMath::Min(Damage * ArmorAbsorption, CurrentArmor));

                //GAME_PRINT(10.f, FColor::Emerald, "Damage to Armor (%f)", DamageToArmor);
                GAME_LOG(Game, Log, "Damage to Armor (%f)", DamageToArmor);

                AttributeSet->SetArmor(FMath::Max(CurrentArmor - DamageToArmor, 0.f));
                Damage = FMath::Max(Damage - DamageToArmor, 0.f);
            }

            const float CurrentHealth = AttributeSet->Health.GetCurrentValue();
            if (CurrentHealth > 0.f && Damage > 0.f)
            {
                //GAME_PRINT(10.f, FColor::Red, "Damage to Health (%f)", Damage);
                GAME_LOG(Game, Log, "Damage to Health (%f)", Damage);

                AttributeSet->SetHealth(FMath::FloorToFloat(FMath::Max(CurrentHealth - Damage, 0.f)));
            }
        }
    }

    //NOTE: it seems like we are lacking control of damage momentum (knockback) overall.
    // DamageType has DamageImpulse but it is part of CDO, which is a bit annoying.
    // We cannot adjust DamageImpulse on the fly with gamemode/mutators.
    // Also it only applies to physics-enabled stuff. Characters need custom knockback anyways.

    // We will probably need to find a way back to the projectile/weapon (DamageCauser?),
    // so we can use custom-defined knockback values there.

    // Then we can apply splash falloff using (ActualDamage/Damage) to the knockback power.
    // And finally, apply knockback manually with a custom impulse.

    // For now, let's try basic values
    float KnockbackPower = 1500.f * Damage;

    // Avoid very small knockbacks
    if (KnockbackPower / GetCharacterMovement()->Mass >= 100.f)
    {
        FVector KnockbackDir;
        if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
        {
            FPointDamageEvent* PointDamageEvent = (FPointDamageEvent*)&DamageEvent;

            // Always use shot direction for knockback
            KnockbackDir = PointDamageEvent->ShotDirection;
            GetCharacterMovement()->AddImpulse(KnockbackPower*KnockbackDir);
        }
        else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
        {
            FRadialDamageEvent* RadialDamageEvent = (FRadialDamageEvent*)&DamageEvent;

            // Use no falloff (constant) because we already scaled KnockbackPower
            GetCharacterMovement()->AddRadialImpulse(RadialDamageEvent->Origin, RadialDamageEvent->Params.GetMaxRadius(), KnockbackPower, ERadialImpulseFalloff::RIF_Constant, false);

            /*
            // Experimental: use vector (HitLocation->EyeLocation) as knockback direction to make it feel more natural

            KnockbackDir = (GetPawnViewLocation() - RadialDamageEvent->Origin);
            KnockbackDir.Normalize();
            GetCharacterMovement()->AddImpulse(KnockbackPower*KnockbackDir);
            */
        }
    }

    if (AttributeSet->Health.GetCurrentValue() <= 0)
    {
        Die(EventInstigator, DamageEvent, DamageCauser);
    }

    return Damage;
}

void AUR_Character::Die(AController* Killer, const FDamageEvent& DamageEvent, AActor* DamageCauser)
{
    // Already killed (might happen when multiple damage sources in 1 frame)
    if (GetTearOff() || IsPendingKillPending())
    {
        return;
    }

    AUR_GameMode* URGameMode = GetWorld()->GetAuthGameMode<AUR_GameMode>();
    if (URGameMode)
    {
        AController* Killed = GetController();

        if (URGameMode->PreventDeath(Killed, Killer, DamageEvent, DamageCauser))
        {
            // Make sure we don't stay with <=0 health or IsAlive() would return false.
            if (AttributeSet->Health.GetCurrentValue() <= 0)
            {
                AttributeSet->SetHealth(1);
            }
            return;
        }

        URGameMode->PlayerKilled(Killed, Killer, DamageEvent, DamageCauser);
    }

    if (AttributeSet)
    {
        AttributeSet->SetHealth(0);
    }

    // Cut the replication link
    TearOff();

    if (GetNetMode() == NM_DedicatedServer)
    {
        //Destroy();
        SetLifeSpan(0.200f);	// give it time to replicate the tear off ?
        SetActorEnableCollision(false);
    }
    else
    {
        PlayDeath();
    }
}

void AUR_Character::PlayDeath()
{
    APlayerController* OldController = nullptr;
    if (Controller && Controller->GetPawn() == this)
        OldController = Cast<APlayerController>(Controller);

    // Unpossess
    DetachFromControllerPendingDestroy();

    // Set view target back, should auto to 3p cam
    if (OldController)
        OldController->SetViewTarget(this);

    // Improve camera feel
    ThirdPersonArm->bInheritPitch = false;
    ThirdPersonArm->bInheritRoll = false;
    ThirdPersonArm->bEnableCameraLag = true;
    ThirdPersonArm->bEnableCameraRotationLag = true;

    GetCharacterMovement()->StopActiveMovement();

    // Disable capsule
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetCapsuleComponent()->DetachFromComponent(FDetachmentTransformRules(EDetachmentRule::KeepWorld, false));

    // Set mesh as root with physics (ragdoll)
    GetMesh()->DetachFromComponent(FDetachmentTransformRules(EDetachmentRule::KeepWorld, false));
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
    GetMesh()->SetSimulatePhysics(true);
    SetRootComponent(GetMesh());

    // Attach capsule back to mesh
    GetCapsuleComponent()->AttachToComponent(GetMesh(), FAttachmentTransformRules(EAttachmentRule::KeepWorld, false));

    SetLifeSpan(5.0f);
}

bool AUR_Character::IsAlive() const
{
    if (GetTearOff() || IsPendingKillPending() || AttributeSet == nullptr)
    {
        return false;	// server link has been cut, health might not replicate anymore
    }

    return AttributeSet->GetHealth() > 0;
}

void AUR_Character::ServerSuicide_Implementation()
{
    Die(nullptr, FDamageEvent(), nullptr);
}


/////////////////////////////////////////////////////////////////////////////////////////////////

void AUR_Character::BeginPickup()
{
    bIsPickingUp = true;
}

void AUR_Character::EndPickup()
{
    bIsPickingUp = false;
}

void AUR_Character::SelectWeapon1()
{
    FString name;
    name = "Assault Rifle";
    InventoryComponent->SelectWeapon(1);
}

void AUR_Character::SelectWeapon2()
{
    FString name;
    name = "Shotgun";
    InventoryComponent->SelectWeapon(2);
}

void AUR_Character::SelectWeapon3()
{
    FString name;
    name = "Rocket Launcher";
    InventoryComponent->SelectWeapon(3);
}

void AUR_Character::SelectWeapon4()
{
    FString name;
    name = "Grenade Launcher";
    InventoryComponent->SelectWeapon(4);
}

void AUR_Character::SelectWeapon5()
{
    FString name;
    name = "Sniper Rifle";
    InventoryComponent->SelectWeapon(5);
}

void AUR_Character::SelectWeapon0()
{
    FString name;
    name = "Pistol";
    InventoryComponent->SelectWeapon(0);
}

void AUR_Character::ShowInventory()
{
    InventoryComponent->ShowInventory();
}

FName AUR_Character::GetWeaponAttachPoint() const
{
    return WeaponAttachPoint;
}

USkeletalMeshComponent* AUR_Character::GetPawnMesh() const
{
    return MeshFirstPerson;
}

void AUR_Character::WeaponSelect(int32 InWeaponGroup)
{
    InventoryComponent->SelectWeapon(InWeaponGroup);
}

void AUR_Character::NextWeapon()
{
    if (InventoryComponent)
    {
        InventoryComponent->NextWeapon();
    }
}

void AUR_Character::PrevWeapon()
{
    if (InventoryComponent)
    {
        InventoryComponent->PrevWeapon();
    }
}

void AUR_Character::PawnStartFire(uint8 FireModeNum)
{
    bIsFiring = true;

    if (InventoryComponent && InventoryComponent->ActiveWeapon)
    {
        InventoryComponent->ActiveWeapon->LocalStartFire();
    }
    else
    {
        GAME_LOG(Game, Log, "No Weapon Selected");
    }
}

void AUR_Character::PawnStopFire(uint8 FireModeNum)
{
    bIsFiring = false;

    if (InventoryComponent && InventoryComponent->ActiveWeapon)
    {
        InventoryComponent->ActiveWeapon->LocalStopFire();
    }
}

//deprecated
void AUR_Character::Fire()
{
    if (bIsFiring)
    {
        if (InventoryComponent->ActiveWeapon)
        {
            if (InventoryComponent->ActiveWeapon->ProjectileClass != nullptr)
            {
                //GetActorEyesViewPoint(InventoryComponent->ActiveWeapon->GetActorLocation(), FRotator()); //InventoryComponent->ActiveWeapon->GetActorRotation()
                //FVector MuzzleLocation{ InventoryComponent->ActiveWeapon->GetActorLocation() + FTransform(FRotator()).TransformVector(MuzzleOffset) }; // InventoryComponent->ActiveWeapon->GetActorRotation()
                //FRotator MuzzleRotation{ InventoryComponent->ActiveWeapon->GetActorRotation() };

                InventoryComponent->ActiveWeapon->Fire();
                MeshFirstPerson->PlayAnimation(FireAnimation, false);
            }
        }
        else
        {
            GAME_LOG(Game, Log, "No Weapon Selected");
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

bool AUR_Character::Server_GiveAbility_Validate(TSubclassOf<UUR_GameplayAbility> InAbilityClass, const int32 InAbilityLevel) { return true; }
void AUR_Character::Server_GiveAbility_Implementation(TSubclassOf<UUR_GameplayAbility> InAbility, const int32 InAbilityLevel)
{
    if (InAbility == nullptr)
    {
        GAME_LOG(Game, Error, "InAbilityClass was Null!");
        return;
    }

    check(AbilitySystemComponent);

    if (AbilitySystemComponent)
    {
        // @! TODO : Unsure of the purpose of InputID, seems to be related to input/keybinding
        const int32 InputID{ 0 }; //static_cast<int32>(Cast<UUR_GameplayAbility>(InAbilityClass.GetDefaultObject())->Input);

        AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(InAbility, InAbilityLevel, InputID, this));
        GameplayAbilities.AddUnique(InAbility);
    }
}

bool AUR_Character::Server_RemoveAbility_Validate(TSubclassOf<UUR_GameplayAbility> InAbilityClass) { return true; }
void AUR_Character::Server_RemoveAbility_Implementation(TSubclassOf<UUR_GameplayAbility> InAbilityClass) const
{
    if (InAbilityClass == nullptr)
    {
        GAME_LOG(Game, Error, "InAbilityClass was Null!");
        return;
    }

    check(AbilitySystemComponent);

    if (AbilitySystemComponent)
    {
        const auto& Abilities = AbilitySystemComponent->GetActivatableAbilities();

        for (const auto& Ability : Abilities)
        {
            if (Ability.Ability->GetClass() == InAbilityClass)
            {
                AbilitySystemComponent->ClearAbility(Ability.Handle);
                break;
            }
        }
    }
}

int32 AUR_Character::GetAbilityLevel(TSubclassOf<UUR_GameplayAbility> InAbilityClass) const
{
    int32 OutLevel{ -1 };

    if (AbilitySystemComponent)
    {
        const auto& Abilities = AbilitySystemComponent->GetActivatableAbilities();

        for (const auto& Ability : Abilities)
        {
            if (Ability.Ability->GetClass() == InAbilityClass)
            {
                OutLevel = Ability.Level;
                break;
            }
        }
    }

    return OutLevel;
}

bool AUR_Character::Server_SetAbilityLevel_Validate(TSubclassOf<UUR_GameplayAbility> InAbilityClass, const int32 InAbilityLevel) { return true; }
void AUR_Character::Server_SetAbilityLevel_Implementation(TSubclassOf<UUR_GameplayAbility> InAbilityClass, const int32 InAbilityLevel)
{
    if (AbilitySystemComponent)
    {
        // Get a reference to our ASC ActiveatableAbilities. Reference so our modifications stick.
        auto& Abilities = AbilitySystemComponent->GetActivatableAbilities();

        for (auto& Ability : Abilities)
        {
            if (Ability.Ability->GetClass() == InAbilityClass)
            {
                Ability.Level = InAbilityLevel;
                AbilitySystemComponent->MarkAbilitySpecDirty(Ability);
                break;
            }
        }
    }
}