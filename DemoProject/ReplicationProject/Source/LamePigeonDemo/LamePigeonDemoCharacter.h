// Source file name: LamePigeonDemoCharacter.h
// Author: Igor Matiushin
// Brief description: Declares the playable demo character and its input-bound gameplay hooks.

#pragma once

#include "CoreMinimal.h"
#include "LamePigeonDemoReplicationComponent.h"
#include "LamePigeonDemoWorldHealthBillboardComponent.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "LamePigeonDemoCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  Third-person character + LamePigeon demo replication component.
 */
UCLASS(abstract)
class ALamePigeonDemoCharacter : public ACharacter
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	ULamePigeonDemoReplicationComponent* LamePigeonDemoReplicationComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	ULamePigeonDemoWorldHealthBillboardComponent* WorldHealthBillboard;

protected:
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

public:
	ALamePigeonDemoCharacter();

protected:
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void JumpWithNetworkReplicate(const FInputActionValue& Value);
	void HandleMoveInput(const FInputActionValue& Value);
	void HandleLookInput(const FInputActionValue& Value);

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);

public:
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};
