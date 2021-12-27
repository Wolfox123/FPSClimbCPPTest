// Fill out your copyright notice in the Description page of Project Settings.


#include "EngiPC.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/ArrowComponent.h"
#include "DrawDebugHelpers.h" 
#include "Runtime/Engine/Classes/Kismet/KismetSystemLibrary.h"
#include "Runtime/Engine/Classes/Kismet/KismetMathLibrary.h"

// Sets default values
AEngiPC::AEngiPC()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(25.0f, 55.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	ToggleSprint = false;
	WalkingSpeed = 260.0f;
	SprintBuff = 30.0f; //When Sprinting, Increase Speed by X Amount
	PlayerWeighted = false;
	nonWeightedBuff = 20.0f; //If holding nothing, Increase Speed by X Amount
	JumpVelocity = 260.0f;

	// Set our movement speeds
	GetCharacterMovement()->MaxWalkSpeed = (PlayerWeighted) ? WalkingSpeed : (WalkingSpeed * (1 + (nonWeightedBuff / 100.0f)));
	GetCharacterMovement()->MaxWalkSpeedCrouched = (WalkingSpeed / 2);
	GetCharacterMovement()->JumpZVelocity = JumpVelocity;
	GetCharacterMovement()->MaxStepHeight = 46;
	GetCharacterMovement()->MaxFlySpeed = (WalkingSpeed * (1 + (nonWeightedBuff / 100.0f))) / 2;
	GetCharacterMovement()->BrakingDecelerationFlying = 2048;


	// Create a CameraComponent	
	FPCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FPCamera"));
	FPCameraComponent->SetupAttachment(GetCapsuleComponent());
	FPCameraComponent->SetRelativeLocation(FVector(0, 0, 24.f)); // Position the camera
	FPCameraComponent->bUsePawnControlRotation = true;

	holdingComponent = CreateDefaultSubobject<UArrowComponent>(TEXT("holdingObject"));
	if (holdingComponent)
	{
		holdingComponent->ArrowColor = FColor(0, 200, 0);
		holdingComponent->bTreatAsASprite = true;
		holdingComponent->SetupAttachment(FPCameraComponent);
		holdingComponent->bIsScreenSizeScaled = true;
		holdingComponent->SetRelativeLocation(FVector(100, 0, -40));
	}
}

// Called to bind functionality to input
void AEngiPC::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// set up gameplay key bindings
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	check(PlayerInputComponent);

	// Bind jump events
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &AEngiPC::GrabWall);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	// Bind Interaction Events
	DECLARE_DELEGATE_OneParam(FSprintController, const int);
	PlayerInputComponent->BindAction<FSprintController>("SpecialAction", IE_Pressed, this, &AEngiPC::Sprint, 0);
	PlayerInputComponent->BindAction<FSprintController>("SpecialAction", IE_Released, this, &AEngiPC::Sprint, -1);

	// Bind movement events
	PlayerInputComponent->BindAxis("MoveForward", this, &AEngiPC::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AEngiPC::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "LookRight/LookUp" handles devices that provide an absolute delta, such as a mouse.
	// "Look/Turn" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("LookRight", this, &AEngiPC::TurnAtRate);
	PlayerInputComponent->BindAxis("Look", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUp", this, &AEngiPC::LookUpAtRate);
}

// Called when the game starts or when spawned
void AEngiPC::BeginPlay()
{
	Super::BeginPlay();

}

void AEngiPC::GrabWall()
{
	// When you jump, see if you can attach to a wall
	if (GetCharacterMovement()->MovementMode != EMovementMode::MOVE_Flying)
	{
		//Trace from Body
		FVector TraceStartPointMidBody = GetActorLocation() + (GetActorForwardVector());
		FVector TraceEndPointMidBody = GetActorLocation() + (GetActorForwardVector() * 75.0f);

		//Trace from Head
		FVector TraceStartPointHead = GetFirstPersonCameraComponent()->GetComponentLocation() + (GetActorForwardVector());
		FVector TraceEndPointHead = GetFirstPersonCameraComponent()->GetComponentLocation() + (GetActorForwardVector() * 75.0f);

		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(this);
		QueryParams.bTraceComplex = true;

		FHitResult SweepResultMid;
		FHitResult SweepResultHead;

		//DrawDebugLine(GetWorld(), TraceStartPointMidBody, TraceEndPointMidBody, FColor::Red, false, 1, 0, 1);
		//DrawDebugLine(GetWorld(), TraceStartPointHead, TraceEndPointHead, FColor::Red, false, 1, 0, 1);

		//If two points of contact are met (At Body and Head), attach player to wall
		if (GetWorld()->LineTraceSingleByChannel(SweepResultMid, TraceStartPointMidBody, TraceEndPointMidBody, ECollisionChannel::ECC_Visibility, QueryParams)
			&& GetWorld()->LineTraceSingleByChannel(SweepResultHead, TraceStartPointHead, TraceEndPointHead, ECollisionChannel::ECC_Visibility, QueryParams))
		{
			//Swap player to "Flying" movement mode to void gravity easily and handle different speeds
			GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Flying);
			GetCharacterMovement()->StopMovementImmediately();

			//Get the Normal of the Impact and make a Rotation from it off the X Axis
			FRotator NormalImpact = UKismetMathLibrary::MakeRotFromX(SweepResultHead.Normal * -1);

			FLatentActionInfo LatentInfo;
			LatentInfo.CallbackTarget = this;
			UKismetSystemLibrary::MoveComponentTo(GetCapsuleComponent(), (SweepResultHead.Location + (SweepResultHead.Normal * 25.0f)),
				FRotator(0, NormalImpact.Yaw * -1, 0), false, false, 0.2f, false, EMoveComponentAction::Move, LatentInfo);
			//Move the Actor to the location

			//Bind a timed Delegate to the movement and rotation to give a feel of flow to attaching to the wall instead of a flat teleport
			FTimerDelegate TimerDelegate;
			TimerDelegate.BindLambda([this, NormalImpact]
				{
					float Temp = GetControlRotation().Pitch;

					GetController()->SetControlRotation(FRotator(GetControlRotation().Pitch, NormalImpact.Yaw, GetControlRotation().Roll));

					GetFirstPersonCameraComponent()->bUsePawnControlRotation = false;
					GetFirstPersonCameraComponent()->SetRelativeRotation(FRotator(Temp, 0, 0));

					Latched = true;
				});

			FTimerHandle TimerHandle;
			GetWorld()->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, 0.3f, false);
		}
	}
	else //Handle Release of the wall/Jumping
	{
		bool jumpTrigger = false;
		FVector LaunchDir = FVector(0, 0, 0);

		//If the player is looking away from the wall, Trigger a bool
		if (GetFirstPersonCameraComponent()->GetRelativeRotation().Yaw > 55 || GetFirstPersonCameraComponent()->GetRelativeRotation().Yaw < -55)
		{
			jumpTrigger = true;
			LaunchDir = GetFirstPersonCameraComponent()->GetForwardVector() * 500;
		}

		//Handle Release
		ReleaseWall();

		//If able to jump, launch player character
		if (jumpTrigger)
		{
			ACharacter::LaunchCharacter(FVector(LaunchDir.X, LaunchDir.Y, 350), false, true);
		}
	}
}

void AEngiPC::ReleaseWall()
{
	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Falling);

	//Release the character from the wall and return them to the original control scheme
	FRotator CorrectRotation = FRotator(GetFirstPersonCameraComponent()->GetRelativeRotation().Pitch, GetFirstPersonCameraComponent()->GetRelativeRotation().Yaw + GetControlRotation().Yaw, 0);
	GetFirstPersonCameraComponent()->bUsePawnControlRotation = true;
	GetController()->SetControlRotation(CorrectRotation);
	GetCapsuleComponent()->SetRelativeRotation(FRotator(0, 0, 0));

	Latched = false;
}

void AEngiPC::Sprint(int Action)
{
	float variableSpeed = (PlayerWeighted) ? WalkingSpeed : (WalkingSpeed * (1 + (nonWeightedBuff / 100)));

	if (ToggleSprint || Action == 0) //Toggle between Stats when pressed
	{
		if (GetCharacterMovement()->GetMaxSpeed() != variableSpeed)
		{
			GetCharacterMovement()->MaxWalkSpeed = variableSpeed;
		}
		else
		{
			GetCharacterMovement()->MaxWalkSpeed = (PlayerWeighted) ? (WalkingSpeed * (1 + (SprintBuff / 100.0f))) : (WalkingSpeed * (1 + ((nonWeightedBuff + SprintBuff) / 100.0f)));
		}
	}
	else if (!ToggleSprint && Action == -1) //If Toggle Sprint is not active and this is the release action, flip character back to walk speed
	{
		if (GetCharacterMovement()->GetMaxSpeed() != variableSpeed)
		{
			GetCharacterMovement()->MaxWalkSpeed = variableSpeed;
		}
	}
}

void AEngiPC::MoveForward(float Val)
{
	//Switch if on wall or on the ground
	if (GetCharacterMovement()->MovementMode != EMovementMode::MOVE_Flying)
	{
		if (Val != 0.0f)
		{
			// add movement in that direction
			AddMovementInput(GetActorForwardVector(), Val);
		}
	}
	else if (GetCharacterMovement()->MovementMode == EMovementMode::MOVE_Flying && Latched)
	{
		if (Val != 0.0f)
		{
			//Trace from Body to see if there is room to move
			FVector TraceStartPoint = GetActorLocation() + (GetActorForwardVector() * (-10.0f));
			FVector TraceEndPoint = GetActorLocation() + (GetActorForwardVector() * (-10.0f)) + (GetActorUpVector() * (Val * 15.0f));

			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(this);
			QueryParams.bTraceComplex = true;

			FCollisionShape Shape = FCollisionShape::MakeCapsule(12, 27);
			FHitResult SweepResult;
			FVector HitLocation;

			//DrawDebugBox(GetWorld(), TraceEndPoint, FVector(24, 24, 24), FQuat::Identity, FColor::Green, true, -1.0f, 0, 1.0f);

			//Check the direction above and below of the Actor to see if they can transition that direction
			if (GetWorld()->SweepSingleByChannel(SweepResult, TraceStartPoint, TraceEndPoint, FQuat::Identity, ECollisionChannel::ECC_Visibility, Shape, QueryParams))
			{
				// If Location hit, move to the max distance allowed based off the hit
				HitLocation = SweepResult.Location;
			}
			else
			{
				// No Location hit, move to at max distance
				HitLocation = TraceEndPoint;
			}

			//Set the offset of the traces
			FVector TargetOffset = HitLocation - GetActorLocation();

			//Trace from Body with Offset
			FVector TraceStartPointMidBody = GetActorLocation() + TargetOffset + (GetActorForwardVector() * -10.0f);
			FVector TraceEndPointMidBody = GetActorLocation() + TargetOffset + (GetActorForwardVector() * 100.0f);

			//Trace from Head with Offset
			FVector TraceStartPointHead = GetFirstPersonCameraComponent()->GetComponentLocation() + TargetOffset + (GetActorForwardVector() * -10.0f);
			FVector TraceEndPointHead = GetFirstPersonCameraComponent()->GetComponentLocation() + TargetOffset + (GetActorForwardVector() * 100.0f);

			FHitResult SweepResultMid;
			FHitResult SweepResultHead;

			//DrawDebugLine(GetWorld(), TraceStartPointMidBody, TraceEndPointMidBody, FColor::Red, false, 1, 0, 1);
			//DrawDebugLine(GetWorld(), TraceStartPointHead, TraceEndPointHead, FColor::Red, false, 1, 0, 1);

			//Are you legally able to move there? If there is a hit move normally
			if (GetWorld()->LineTraceSingleByChannel(SweepResultMid, TraceStartPointMidBody, TraceEndPointMidBody, ECollisionChannel::ECC_Visibility, QueryParams)
				&& GetWorld()->LineTraceSingleByChannel(SweepResultHead, TraceStartPointHead, TraceEndPointHead, ECollisionChannel::ECC_Visibility, QueryParams))
			{
				FRotator NormalImpact = UKismetMathLibrary::MakeRotFromX(SweepResultMid.Normal * -1);

				FVector TempV = SweepResultMid.Location + (SweepResultMid.Normal * 25.0f);
				FVector Direction = TempV - GetActorLocation();

				AddMovementInput(FVector(Direction.X, Direction.Y, Direction.Z), FMath::Abs(Val / 3));

				FRotator NormalComputed = FMath::RInterpTo(GetCapsuleComponent()->GetRelativeRotation(), NormalImpact, GetWorld()->GetDeltaSeconds(), 4);
				GetController()->SetControlRotation(NormalComputed);
				GetCapsuleComponent()->SetRelativeRotation(NormalComputed);
			}
			else //If no wall is found to move up on AKA Move up Edge at a tilt
			{
				//Trace from Body
				FVector TraceStartPointClimb = GetActorLocation() + (GetActorForwardVector() * (-10.0f));
				TraceStartPointClimb.Z += GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
				FVector TraceEndPointClimb = GetActorLocation() + (GetActorForwardVector() * (-10.0f)) + (GetActorUpVector() * (50.0f));
				TraceEndPointClimb.Z += GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();

				//Trace to make sure climb is able to be performed on Ledge
				FVector FreeLedgeEndPoint = TraceEndPointClimb + (GetActorForwardVector() * (-10.0f)) + (GetActorForwardVector() * (50.0f));

				FCollisionShape ShapeClimb = FCollisionShape::MakeCapsule(12, 27);
				FHitResult SweepResultClimb;
				FHitResult FreeLedgeSweep;

				//DrawDebugBox(GetWorld(), TraceEndPointClimb, FVector(24, 24, 24), FQuat::Identity, FColor::Green, true, -1.0f, 0, 1.0f);

				if (GetWorld()->SweepSingleByChannel(SweepResultClimb, TraceStartPointClimb, TraceEndPointClimb, FQuat::Identity, ECollisionChannel::ECC_Visibility, ShapeClimb, QueryParams)
					|| GetWorld()->SweepSingleByChannel(FreeLedgeSweep, TraceEndPointClimb, FreeLedgeEndPoint, FQuat::Identity, ECollisionChannel::ECC_Visibility, ShapeClimb, QueryParams))
				{
					//Object Hit, not a good climb
				}
				else
				{
					ReleaseWall();
					// No Location hit, move to where grabbed
					FLatentActionInfo LatentInfo;
					LatentInfo.CallbackTarget = this;
					UKismetSystemLibrary::MoveComponentTo(GetCapsuleComponent(), (FreeLedgeEndPoint),
						GetCapsuleComponent()->GetRelativeRotation(), false, false, 0.75f, false, EMoveComponentAction::Move, LatentInfo);
				}
			}
		}
	}
}

void AEngiPC::MoveRight(float Val)
{
	//Switch if on wall or on the ground
	if (GetCharacterMovement()->MovementMode != EMovementMode::MOVE_Flying)
	{
		if (Val != 0.0f)
		{
			// add movement in that direction
			AddMovementInput(GetActorRightVector(), Val);
		}
	}
	else if (GetCharacterMovement()->MovementMode == EMovementMode::MOVE_Flying && Latched)
	{
		if (Val != 0.0f)
		{
			/*
			//////////////////
			This is split between two parts compared to the up and down code base for movement

			PART 1:
			Check to see if there is a wall to the right or left of the player and if the player can attach to it

			PART 2: IF NOT WALL IS FOUND RIGHT OR LEFT THAT IS SUITABLE
			Check to see if you can shimmy
			//////////////////
			*/

			//Params for Wall hits
			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(this);
			QueryParams.bTraceComplex = true;

			//PART 1 - Looking to see if a wall is directly to my left or Right
			//Trace from Body with Offset
			FVector P1_TraceStartPointMidBody = GetActorLocation() + (GetActorForwardVector() * (-10.0f));
			FVector P1_TraceEndPointMidBody = GetActorLocation() + (GetActorForwardVector() * (-10.0f)) + (GetActorRightVector() * (Val * 35.0f)); //Distance of Each shimmy

			//Trace from Head with Offset
			FVector P1_TraceStartPointHead = GetFirstPersonCameraComponent()->GetComponentLocation() + (GetActorForwardVector() * (-10.0f));
			FVector P1_TraceEndPointHead = GetFirstPersonCameraComponent()->GetComponentLocation() + (GetActorForwardVector() * (-10.0f)) + (GetActorRightVector() * (Val * 35.0f)); //Distance of Each shimmy

			FHitResult P1_SweepResultMid;
			FHitResult P1_SweepResultHead;

			//DrawDebugLine(GetWorld(), P1_TraceStartPointMidBody, P1_TraceEndPointMidBody, FColor::Red, false, 1, 0, 1);
			//DrawDebugLine(GetWorld(), P1_TraceStartPointHead, P1_TraceEndPointHead, FColor::Red, false, 1, 0, 1);

			if (GetWorld()->LineTraceSingleByChannel(P1_SweepResultMid, P1_TraceStartPointMidBody, P1_TraceEndPointMidBody, ECollisionChannel::ECC_Visibility, QueryParams)
				&& GetWorld()->LineTraceSingleByChannel(P1_SweepResultHead, P1_TraceStartPointHead, P1_TraceEndPointHead, ECollisionChannel::ECC_Visibility, QueryParams))
			{
				//AddMovementInput(GetActorRightVector(), FMath::Abs(Val / 3));

				FRotator NormalComputed = FMath::RInterpTo(GetCapsuleComponent()->GetRelativeRotation(), GetCapsuleComponent()->GetRelativeRotation() + FRotator(0, 15 * Val, 0), GetWorld()->GetDeltaSeconds(), 16);
				GetController()->SetControlRotation(NormalComputed);
				GetCapsuleComponent()->SetRelativeRotation(NormalComputed);
			}
			else
			{
				//PART 2 - No wall has been found on my left or right, keep going with shimmy
				// 
				//Trace from Body
				FVector TraceStartPoint = GetActorLocation() + (GetActorForwardVector() * (-10.0f));
				FVector TraceEndPoint = GetActorLocation() + (GetActorForwardVector() * (-10.0f)) + (GetActorRightVector() * (Val * 15.0f)); //Distance of Each shimmy

				FCollisionShape Shape = FCollisionShape::MakeCapsule(12, 54);
				FHitResult SweepResult;
				FVector HitLocation;

				//DrawDebugBox(GetWorld(), TraceEndPoint, FVector(24, 24, 24), FQuat::Identity, FColor::Green, true, -1.0f, 0, 1.0f);

				if (GetWorld()->SweepSingleByChannel(SweepResult, TraceStartPoint, TraceEndPoint, FQuat::Identity, ECollisionChannel::ECC_Visibility, Shape, QueryParams))
				{
					// If Location hit, move to the max distance
					HitLocation = SweepResult.Location;
				}
				else
				{
					// No Location hit, move to the right
					HitLocation = TraceEndPoint;
				}

				FVector TargetOffset = HitLocation - GetActorLocation();

				//Trace from Body with Offset
				FVector TraceStartPointMidBody = GetActorLocation() + TargetOffset + (GetActorForwardVector() * -10.0f);
				FVector TraceEndPointMidBody = GetActorLocation() + TargetOffset + (GetActorForwardVector() * 60.0f);

				//Trace from Head with Offset
				FVector TraceStartPointHead = GetFirstPersonCameraComponent()->GetComponentLocation() + TargetOffset + (GetActorForwardVector() * -10.0f);
				FVector TraceEndPointHead = GetFirstPersonCameraComponent()->GetComponentLocation() + TargetOffset + (GetActorForwardVector() * 60.0f);

				FHitResult SweepResultMid;
				FHitResult SweepResultHead;

				//DrawDebugLine(GetWorld(), TraceStartPointMidBody, TraceEndPointMidBody, FColor::Red, false, 1, 0, 1);
				//DrawDebugLine(GetWorld(), TraceStartPointHead, TraceEndPointHead, FColor::Red, false, 1, 0, 1);

				if (GetWorld()->LineTraceSingleByChannel(SweepResultMid, TraceStartPointMidBody, TraceEndPointMidBody, ECollisionChannel::ECC_Visibility, QueryParams)
					&& GetWorld()->LineTraceSingleByChannel(SweepResultHead, TraceStartPointHead, TraceEndPointHead, ECollisionChannel::ECC_Visibility, QueryParams))
				{
					FRotator NormalImpact = UKismetMathLibrary::MakeRotFromX(SweepResultMid.Normal * -1);

					FVector TempV = SweepResultMid.Location + (SweepResultMid.Normal * 25.0f);
					FVector Direction = TempV - GetActorLocation();

					AddMovementInput(FVector(Direction.X, Direction.Y, 0), FMath::Abs(Val / 3));

					FRotator NormalComputed = FMath::RInterpTo(GetCapsuleComponent()->GetRelativeRotation(), NormalImpact, GetWorld()->GetDeltaSeconds(), 4);
					GetController()->SetControlRotation(NormalComputed);
					GetCapsuleComponent()->SetRelativeRotation(NormalComputed);
				}
				else
				{
					//Trace from Head with Offset
					FVector TraceStartPointFeet = -GetFirstPersonCameraComponent()->GetComponentLocation() + TargetOffset + (GetActorForwardVector() * -10.0f);
					FVector TraceEndPointFeet = -GetFirstPersonCameraComponent()->GetComponentLocation() + TargetOffset + (GetActorForwardVector() * 60.0f);

					FHitResult SweepResultLow;

					if (!GetWorld()->LineTraceSingleByChannel(SweepResultLow, TraceStartPointFeet, TraceEndPointFeet, ECollisionChannel::ECC_Visibility, QueryParams))
					{
						AddMovementInput(GetActorRightVector(), FMath::Abs(Val / 3));

						FRotator NormalComputed = FMath::RInterpTo(GetCapsuleComponent()->GetRelativeRotation(), GetCapsuleComponent()->GetRelativeRotation() + FRotator(0, -10 * Val, 0), GetWorld()->GetDeltaSeconds(), 8);
						GetController()->SetControlRotation(NormalComputed);
						GetCapsuleComponent()->SetRelativeRotation(NormalComputed);
					}
				}
			}
		}
	}
}

void AEngiPC::TurnAtRate(float Rate)
{
	if (GetCharacterMovement()->MovementMode != EMovementMode::MOVE_Flying)
	{
		AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
	}
	else if (GetCharacterMovement()->MovementMode == EMovementMode::MOVE_Flying)
	{
		GetFirstPersonCameraComponent()->AddRelativeRotation(FRotator(0, Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds(), 0));
	}
}

void AEngiPC::LookUpAtRate(float Rate)
{
	if (GetCharacterMovement()->MovementMode != EMovementMode::MOVE_Flying)
	{
		AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
	}
	else if (GetCharacterMovement()->MovementMode == EMovementMode::MOVE_Flying)
	{
		float Addative = Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds() * -1;
		float CurrentRotationPitch = GetFirstPersonCameraComponent()->GetRelativeRotation().Pitch;
		float Calc = Addative + CurrentRotationPitch;

		if (Calc <= 88.99f && Calc >= -88.99f)
		{
			GetFirstPersonCameraComponent()->AddRelativeRotation(FRotator(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds() * -1, 0, 0));
		}
	}
}

// Called every frame
void AEngiPC::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FString Temp = FString::SanitizeFloat(GetCharacterMovement()->GetMaxSpeed());
	//GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, Temp);

	if (Latched) //If player is latched and starts to over extend an angle, release them
	{
		float Tilt = GetCapsuleComponent()->GetRelativeRotation().Pitch;

		if (Tilt >= 35.0f || Tilt <= -35.0f)
		{
			ReleaseWall();
		}
	}
}