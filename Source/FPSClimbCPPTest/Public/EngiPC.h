// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EngiPC.generated.h"

//Foward Declaration
class UCameraComponent;
class UArrowComponent;

UCLASS()
class FPSCLIMBCPPTEST_API AEngiPC : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AEngiPC();

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
		UCameraComponent* FPCameraComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
		UArrowComponent* holdingComponent;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:

	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Camera)
		float BaseTurnRate;

	/** Base look up/down rate, in deg/sec. Other scaling may affect final rate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Camera)
		float BaseLookUpRate;

	////////////////////////////////////////////////////////////////////////////////
	// Standard Movement Tech (Sprinting)

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		bool ToggleSprint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		float WalkingSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		float SprintBuff;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		bool PlayerWeighted;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		float nonWeightedBuff;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		float JumpVelocity;

	////////////////////////////////////////////////////////////////////////////////
	// Wall Climbing Tech

	void GrabWall();
	void ReleaseWall();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		bool Latched;

	////////////////////////////////////////////////////////////////////////////////
	// 

	/** Handles Sprinting */
	void Sprint(int Action);

	/** Handles moving forward/backward */
	void MoveForward(float Val);

	/** Handles stafing movement, left and right */
	void MoveRight(float Val);

	//Handles Turn Rate of Camera
	void TurnAtRate(float Rate);

	//Handles Look Rate of Camera
	void LookUpAtRate(float Rate);

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** Returns FirstPersonCameraComponent subobject **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FPCameraComponent; }
};