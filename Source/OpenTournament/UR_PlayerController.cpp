// Copyright 2019 Open Tournament Project, All Rights Reserved.

/////////////////////////////////////////////////////////////////////////////////////////////////

#include "UR_PlayerController.h"

#include "Engine/World.h"

//UMG
#include "SlateBasics.h"
#include "Runtime/UMG/Public/UMG.h"
#include "Runtime/UMG/Public/UMGStyle.h"
#include "Runtime/UMG/Public/Slate/SObjectWidget.h"
#include "Runtime/UMG/Public/IUMGModule.h"
#include "Runtime/UMG/Public/Blueprint/UserWidget.h"

#include "Components/AudioComponent.h"

#include "OpenTournament.h"
#include "UR_Character.h"
#include "UR_HUD.h"
#include "UR_PCInputDodgeComponent.h"
#include "UR_ChatComponent.h"
#include "UR_LocalPlayer.h"
#include "Widgets/UR_Widget_BaseMenu.h"
#include "UR_MessageHistory.h"

/////////////////////////////////////////////////////////////////////////////////////////////////

AUR_PlayerController::AUR_PlayerController(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    MusicVolumeScalar = 1.0;
    MusicComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("MusicComponent"));
    MusicComponent->SetupAttachment(RootComponent);

    BaseTurnRate = 45.f;
    BaseLookUpRate = 45.f;

    // @! TODO Should we add via BP instead ?
    InputDodgeComponent = CreateDefaultSubobject<UUR_PCInputDodgeComponent>(TEXT("InputDodgeComponent"));

    ChatComponent = CreateDefaultSubobject<UUR_ChatComponent>(TEXT("ChatComponent"));
    ChatComponent->FallbackOwnerName = TEXT("SOMEBODY");
    ChatComponent->AntiSpamDelay = 1.f;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void AUR_PlayerController::BeginPlay()
{
    Super::BeginPlay();
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void AUR_PlayerController::PlayMusic(USoundBase * Music, float FadeInDuration)
{
    if (MusicComponent)
    {
        MusicComponent->SetSound(Music);
        MusicComponent->FadeIn(FadeInDuration, MusicVolumeScalar);
    }
}

void AUR_PlayerController::StopMusic(float FadeOutDuration)
{
    if (MusicComponent)
    {
        MusicComponent->FadeOut(FadeOutDuration, 0.f);
    }
}

void AUR_PlayerController::SetMusicVolume(float MusicVolume)
{
    MusicVolumeScalar = MusicVolume;
    if (MusicComponent)
    {
        MusicComponent->AdjustVolume(0.f, MusicVolumeScalar);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void AUR_PlayerController::SetPlayer(UPlayer* InPlayer)
{
    Super::SetPlayer(InPlayer);

    UUR_LocalPlayer* LP = Cast<UUR_LocalPlayer>(GetLocalPlayer());
    if (LP && LP->MessageHistory)
    {
        // bind chat dispatcher to MessageHistory handler
        ChatComponent->OnReceiveChatMessage.AddUniqueDynamic(LP->MessageHistory, &UUR_MessageHistory::OnReceiveChatMessage);

        // bind system message dispatcher to MessageHistory handler
        OnReceiveSystemMessage.AddUniqueDynamic(LP->MessageHistory, &UUR_MessageHistory::OnReceiveSystemMessage);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("URPlayerController created but no URLocalPlayer available ?! %s"), *GetDebugName(this));
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void AUR_PlayerController::InitInputSystem()
{
    if (PlayerInput == nullptr)
    {
        // PlayerInput = NewObject<UUTPlayerInput>(this, UUTPlayerInput::StaticClass());
    }

    Super::InitInputSystem();
}

void AUR_PlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    // Turning with absolute delta (e.g. mouse)
    InputComponent->BindAxis("Turn", this, &APlayerController::AddYawInput);
    InputComponent->BindAxis("LookUp", this, &APlayerController::AddPitchInput);
    // Turning with analog device (e.g. joystick)
    InputComponent->BindAxis("TurnRate", this, &AUR_PlayerController::TurnAtRate);
    InputComponent->BindAxis("LookUpRate", this, &AUR_PlayerController::LookUpAtRate);

    InputComponent->BindAxis("MoveForward", this, &AUR_PlayerController::MoveForward);
    InputComponent->BindAxis("MoveBackward", this, &AUR_PlayerController::MoveBackward);
    InputComponent->BindAxis("MoveLeft", this, &AUR_PlayerController::MoveLeft);
    InputComponent->BindAxis("MoveRight", this, &AUR_PlayerController::MoveRight);
    InputComponent->BindAxis("MoveUp", this, &AUR_PlayerController::MoveUp);
    InputComponent->BindAxis("MoveDown", this, &AUR_PlayerController::MoveDown);
    InputComponent->BindAction("Jump", IE_Pressed, this, &AUR_PlayerController::Jump);

    InputComponent->BindAction("Crouch", IE_Pressed, this, &AUR_PlayerController::Crouch);
    InputComponent->BindAction("Crouch", IE_Released, this, &AUR_PlayerController::UnCrouch);

    // Forward to StartFire() provided by engine, handles things like spectator, request respawn...
    InputComponent->BindAction("Fire", IE_Pressed, this, &AUR_PlayerController::PressedFire);
    //NOTE: we cannot bind 'Pressed' in PC and 'Released' in Character that just doesn't work...
    InputComponent->BindAction("Fire", IE_Released, this, &AUR_PlayerController::ReleasedFire);
}

void AUR_PlayerController::ProcessPlayerInput(const float DeltaTime, const bool bGamePaused)
{
    if (InputEnabled())
    {
        Super::ProcessPlayerInput(DeltaTime, bGamePaused);

        if (InputDodgeComponent)
        {
            InputDodgeComponent->ProcessPlayerInput(DeltaTime, bGamePaused);
        }
    }
}

void AUR_PlayerController::SetPawn(APawn* InPawn)
{
    AController::SetPawn(InPawn);

    URCharacter = Cast<AUR_Character>(InPawn);

    if (InPawn != nullptr)
    {
        if (auto URHUD = Cast<AUR_HUD>(GetHUD()))
        {
            URHUD->OnHUDRestart();
        }
    }

    // Set Spectating Pawn
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void AUR_PlayerController::MoveForward(const float InValue)
{
    if (URCharacter != nullptr && InValue != 0.0f)
    {
        if (InputDodgeComponent)
        {
            InputDodgeComponent->MovementForwardAxis = InValue;
        }

        URCharacter->MoveForward(InValue);
    }
    else if (GetSpectatorPawn() != nullptr)
    {
        GetSpectatorPawn()->MoveForward(InValue);
    }
}

void AUR_PlayerController::MoveBackward(const float InValue)
{
    MoveForward(InValue * -1);
}

void AUR_PlayerController::MoveRight(const float InValue)
{
    if (URCharacter != nullptr && InValue != 0.0f)
    {
        if (InputDodgeComponent)
        {
            InputDodgeComponent->MovementStrafeAxis = InValue;
        }

        URCharacter->MoveRight(InValue);
    }
    else if (GetSpectatorPawn() != nullptr)
    {
        GetSpectatorPawn()->MoveRight(InValue);
    }
}

void AUR_PlayerController::MoveLeft(const float InValue)
{
    MoveRight(InValue * -1);
}

void AUR_PlayerController::MoveUp(const float InValue)
{
    if (URCharacter != nullptr && InValue != 0.0f)
    {
        URCharacter->MoveUp(InValue);
    }
    else if (GetSpectatorPawn() != nullptr)
    {
        GetSpectatorPawn()->MoveUp_World(InValue);
    }
}

void AUR_PlayerController::MoveDown(const float InValue)
{
    MoveUp(InValue * -1);
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void AUR_PlayerController::TurnAtRate(const float InRate)
{
    // calculate delta for this frame from the rate information
    AddYawInput(InRate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AUR_PlayerController::LookUpAtRate(const float InRate)
{
    // calculate delta for this frame from the rate information
    AddPitchInput(InRate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void AUR_PlayerController::Jump()
{
    if (URCharacter != nullptr && !IsMoveInputIgnored())
    {
        URCharacter->bPressedJump = true;
    }
}

void AUR_PlayerController::Crouch()
{
    if (!IsMoveInputIgnored())
    {
        if (URCharacter)
        {
            URCharacter->Crouch(false);
        }
    }
}

void AUR_PlayerController::UnCrouch()
{
    if (URCharacter)
    {
        URCharacter->UnCrouch(false);
    }
}

void AUR_PlayerController::ReleasedFire()
{
    if (URCharacter)
    {
        URCharacter->PawnStopFire(0);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void AUR_PlayerController::Say(const FString& Message)
{
    if (ChatComponent)
        ChatComponent->Send(Message, false);
}

void AUR_PlayerController::TeamSay(const FString& Message)
{
    if (ChatComponent)
        ChatComponent->Send(Message, true);
}

void AUR_PlayerController::ClientMessage_Implementation(const FString& S, FName Type, float MsgLifeTime)
{
    if (OnReceiveSystemMessage.IsBound())
        OnReceiveSystemMessage.Broadcast(S);
    else
        Super::ClientMessage_Implementation(S, Type, MsgLifeTime);
}


/////////////////////////////////////////////////////////////////////////////////////////////////

//TODO: delete this now that it is accessible from other menus.

void AUR_PlayerController::OpenControlBindingMenu()
{
    if (KeyBindingMenu == nullptr)
    {
        return;
    }

    ControlsMenu = CreateWidget<UUR_Widget_BaseMenu>(GetWorld(), KeyBindingMenu);
    if (ControlsMenu)
    {
        ControlsMenu->Show(nullptr);
    }
}
