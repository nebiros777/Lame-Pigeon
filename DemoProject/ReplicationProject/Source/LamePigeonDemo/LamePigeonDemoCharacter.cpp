// Source file name: LamePigeonDemoCharacter.cpp
// Author: Igor Matiushin
// Brief description: Implements the playable demo character and local input-driven movement actions.

#include "LamePigeonDemoCharacter.h"
#include "LamePigeonDemoReplicationComponent.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "LamePigeonDemo.h"

ALamePigeonDemoCharacter::ALamePigeonDemoCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	LamePigeonDemoReplicationComponent = CreateDefaultSubobject<ULamePigeonDemoReplicationComponent>(TEXT("LamePigeonDemoReplicationComponent"));

	WorldHealthBillboard = CreateDefaultSubobject<ULamePigeonDemoWorldHealthBillboardComponent>(TEXT("WorldHealthBillboard"));
	WorldHealthBillboard->SetupAttachment(RootComponent);
}

void ALamePigeonDemoCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this,
		                                   &ALamePigeonDemoCharacter::JumpWithNetworkReplicate);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this,
		                                   &ALamePigeonDemoCharacter::HandleMoveInput);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this,
		                                   &ALamePigeonDemoCharacter::HandleLookInput);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this,
		                                   &ALamePigeonDemoCharacter::HandleLookInput);
	}
}

void ALamePigeonDemoCharacter::JumpWithNetworkReplicate(const FInputActionValue& Value)
{
	Jump();
	if (ULamePigeonDemoReplicationComponent* DemoComponent =
	        FindComponentByClass<ULamePigeonDemoReplicationComponent>())
	{
		DemoComponent->RequestBroadcastJumpRpc();
	}
}

void ALamePigeonDemoCharacter::HandleMoveInput(const FInputActionValue& Value)
{
	Move(Value);
}

void ALamePigeonDemoCharacter::HandleLookInput(const FInputActionValue& Value)
{
	Look(Value);
}

void ALamePigeonDemoCharacter::Move(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();
	DoMove(MovementVector.X, MovementVector.Y);
}

void ALamePigeonDemoCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void ALamePigeonDemoCharacter::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void ALamePigeonDemoCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void ALamePigeonDemoCharacter::DoJumpStart()
{
	Jump();
	if (ULamePigeonDemoReplicationComponent* Demo = FindComponentByClass<ULamePigeonDemoReplicationComponent>())
		Demo->RequestBroadcastJumpRpc();
}

void ALamePigeonDemoCharacter::DoJumpEnd()
{
	StopJumping();
}
