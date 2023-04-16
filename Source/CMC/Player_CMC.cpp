// Fill out your copyright notice in the Description page of Project Settings.


#include "Player_CMC.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Net/UnrealNetwork.h"
#include "CMCCharacter.h"

UPlayer_CMC::UPlayer_CMC()
{
	NavAgentProps.bCanCrouch = true; //enables engine side crouch variable
}

void UPlayer_CMC::InitializeComponent()
{
	Super::InitializeComponent();

	PlayerCharacterOwner = Cast<ACMCCharacter>(GetOwner());
}

void UPlayer_CMC::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);

	//if in walking movement move then adjust the movement speed based on bWantsToSprint
	if (MovementMode == MOVE_Walking)
	{
		if (Safe_bWantsToSprint)
		{
			MaxWalkSpeed = Sprint_MaxWalkSpeed;
		}
		else
		{
			MaxWalkSpeed = Walk_MaxWalkSpeed;
		}
	}

	Safe_bPrevWantsToCrouch = bWantsToCrouch;
}

void UPlayer_CMC::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{	

	//Two cases - entering and exiting

	//ENTERING  -- case to enter a slide --- TODO: Need to update to use hold or tap
	//DOUBLE tap cntrl to crouch - second time bwantstocrouch would be false - then check if we can enter a slide
	if (MovementMode == MOVE_Walking && !bWantsToCrouch && Safe_bPrevWantsToCrouch) //entering slide + exit crouch
	{ // essentially monitors when crouch button is pressed for a second time

		FHitResult PotentialSlideSurface; //is sliding possible?
		if (Velocity.SizeSquared() > pow(Slide_MinSpeed, 2) && GetSlideSurface(PotentialSlideSurface))
		{
			EnterSlide(); //bWantsToCrouch will flip back to true - to stay in crouching state
		}
	}


	//EXITING - player presses crouch whilst sliding
	if (IsCustomMovementMode(CMOVE_Slide) && !bWantsToCrouch)
	{
		SetMovementMode(MOVE_Walking);
	}

	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
}


//handles the custom movement mode
void UPlayer_CMC::PhysCustom(float deltaTime, int32 Iterations)
{
	Super::PhysCustom(deltaTime, Iterations);

	switch (CustomMovementMode)
	{
	case CMOVE_Slide:
		PhysSlide(deltaTime, Iterations);
		break;
	default:
		UE_LOG(LogTemp, Fatal, TEXT("Invalid movement mode"))
	}
}

//checks new and current move to check if it can be combined to save bandwidth if they are identical
bool UPlayer_CMC::FSavedMove_Player::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	//cast new move to player move
	FSavedMove_Player* NewPlayerMove = static_cast<FSavedMove_Player*>(NewMove.Get());

	if (Saved_bWantsToSprint != NewPlayerMove->Saved_bWantsToSprint)
	{
		return false;
	}

	return FSavedMove_Character::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

//resets Saved move object to be empty
void UPlayer_CMC::FSavedMove_Player::Clear()
{
	FSavedMove_Character::Clear();

	//also reset our custom added flag
	Saved_bWantsToSprint = 0;
	Saved_bPrevWantsToCrouch = 0;

	Saved_bCMCPressedJump = 0;
	Saved_bHadAnimRootMotion = 0;
	Saved_bTransitionFinished = 0;
}


//compressed flags are the way client will rep movement data
//eight flags as it is an eight bit integer
uint8 UPlayer_CMC::FSavedMove_Player::GetCompressedFlags() const
{
	//running super
	uint8 Result = Super::GetCompressedFlags();

	//changing cust flag if bWantsToSprint is true
	if (Saved_bWantsToSprint) Result |= FLAG_Custom_0;
	if (Saved_bCMCPressedJump) Result |= FLAG_JumpPressed;

	return Result;
}

//captures the state data of the character movement component
//will look through all of the all vars in cmc and set their saved variables
//-setting saved move for the current snapshot of the cmc
void UPlayer_CMC::FSavedMove_Player::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	FSavedMove_Character::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	UPlayer_CMC* PlayerMovement = Cast<UPlayer_CMC>(C->GetCharacterMovement());

	Saved_bWantsToSprint = PlayerMovement->Safe_bWantsToSprint;
	Saved_bPrevWantsToCrouch = PlayerMovement->Safe_bPrevWantsToCrouch;

	Saved_bCMCPressedJump = PlayerMovement->PlayerCharacterOwner->bCMCPressedJump;
	Saved_bHadAnimRootMotion = PlayerMovement->Safe_bHadAnimRootMotion;
	Saved_bTransitionFinished = PlayerMovement->Safe_bTransitionFinished;
}


//this is the exact opposite as above
//takes the data in the saved move and apply it to the current state of the cmc 
void UPlayer_CMC::FSavedMove_Player::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);

	UPlayer_CMC* PlayerMovement = Cast<UPlayer_CMC>(C->GetCharacterMovement());

	PlayerMovement->Safe_bWantsToSprint = Saved_bWantsToSprint;
	PlayerMovement->Safe_bPrevWantsToCrouch = Saved_bPrevWantsToCrouch;
}

FSavedMovePtr UPlayer_CMC::FNetworkPredictionData_Client_Player::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_Player());
}

UPlayer_CMC::FNetworkPredictionData_Client_Player::FNetworkPredictionData_Client_Player(const UPlayer_CMC& ClientMovement)
	: Super(ClientMovement)
{
}

//will only create a new predData if we haven't already created it else we return regardless
FNetworkPredictionData_Client* UPlayer_CMC::GetPredictionData_Client() const 
{
	check(PawnOwner != nullptr)

	//if the client prediction data is null
	if(ClientPredictionData == nullptr)
	{
		//then we create a new one
		UPlayer_CMC* MutableThis = const_cast<UPlayer_CMC*>(this); //annoing workaround

		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Player(*this);
		MutableThis->ClientPredictionData->MaxSmoothNetUpdateDist = 92.f;
		MutableThis->ClientPredictionData->NoSmoothNetUpdateDist = 140.f;
	}

	//and return then result
	return ClientPredictionData;
}

void UPlayer_CMC::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	
	Safe_bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
}

bool UPlayer_CMC::IsMovingOnGround() const
{
	return Super::IsMovingOnGround() || IsCustomMovementMode(CMOVE_Slide);
}

bool UPlayer_CMC::CanCrouchInCurrentState() const
{
	return Super::CanCrouchInCurrentState() && IsMovingOnGround();
}


//changes bWantsToSprint flag
void UPlayer_CMC::SprintPressed()
{
	Safe_bWantsToSprint = true;
}

void UPlayer_CMC::SprintReleased()
{
	Safe_bWantsToSprint = false;
}
void UPlayer_CMC::CrouchPressed()
{
	bWantsToCrouch = !bWantsToCrouch;
	//bWantsToCrouch = true;
}
void UPlayer_CMC::CrouchReleased()
{
	//bWantsToCrouch = false;
}

/*	^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
You can alter movement safe vars in non movement safe functions from the client
You can never use NON movement safe vars in a movement safe function 
cant call NON movement safe functions that alter movement safe variables on the server*/

bool UPlayer_CMC::IsCustomMovementMode(ECustomMovementMode InCustomMovementMode) const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == InCustomMovementMode;
}

void UPlayer_CMC::EnterSlide()
{
	bWantsToCrouch = true; //want to stay crouching 

	Velocity += Velocity.GetSafeNormal2D() * Slide_EnterImpulse; //apply enter boost
	SetMovementMode(MOVE_Custom, CMOVE_Slide); //update movement mode
}

void UPlayer_CMC::ExitSlide()
{
	bWantsToCrouch = false; //leave crouch

	//corrects the rotation - makes caspule verticle when leaving slide
	FQuat NewRotation = FRotationMatrix::MakeFromXZ(UpdatedComponent->GetForwardVector().GetSafeNormal2D(),
		FVector::UpVector).ToQuat(); 
	FHitResult Hit;
	SafeMoveUpdatedComponent(FVector::ZeroVector, NewRotation, true, Hit);

	SetMovementMode(MOVE_Walking);
}

void UPlayer_CMC::PhysSlide(float deltaTime, int32 Iterations)
{	
	//boilerplate
	if (deltaTime < MIN_TICK_TIME)
	{
		return;	
	}

	//REMOVE IF USING ROOT MOTION
	RestorePreAdditiveRootMotionVelocity();

	//checking to see if slide state is valid
	//also check if we have enough speed for the slide
	FHitResult SurfaceHit;
	if (!GetSlideSurface(SurfaceHit) || Velocity.SizeSquared() < pow(Slide_MinSpeed, 2))
	{
		ExitSlide();
		StartNewPhysics(deltaTime, Iterations); //Start new iteration in the same frame in a new physics function because we are sliding
		return;//exit out of the slide function
	}


	//	Apply Surface Gravity to our velocity
	Velocity += Slide_GravityForce * FVector::DownVector * deltaTime;	// V += A * dt

	//Strafe 
	 //only allows left and right movement, no forward or backwards movement
	//thresholding the values
	if (FMath::Abs(FVector::DotProduct(Acceleration.GetSafeNormal(), UpdatedComponent->GetRightVector())) > .5)
	{
		//only allow left and right
		Acceleration = Acceleration.ProjectOnTo(UpdatedComponent->GetRightVector());
	}
	else
	{
		//else set to zero if it is not above the threshold 
		Acceleration = FVector::ZeroVector;
	}

	// Calc Velocity 
	//if not root motion or rm override 
	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		//helper - consider friction and braking - updates velocity
		CalcVelocity(deltaTime, Slide_Friction, true, GetMaxBrakingDeceleration());
	}
	ApplyRootMotionToVelocity(deltaTime); //apply rootmotion velocity

	// Perform Move
	Iterations++;
	bJustTeleported = false; //boiler plate


	FVector OldLocation = UpdatedComponent->GetComponentLocation();
	FQuat OldRotation = UpdatedComponent->GetComponentRotation().Quaternion();
	FHitResult Hit(1.f);
	FVector Adjusted = Velocity * deltaTime; //x = v * dt
	//conform the capsule rotation to the surface it is on
	FVector VelPlaneDir = FVector::VectorPlaneProject(Velocity, SurfaceHit.Normal).GetSafeNormal();
	FQuat NewRotation = FRotationMatrix::MakeFromXZ(VelPlaneDir, SurfaceHit.Normal).ToQuat();
	//Everything happens here --- this moves the line ------ NB
	SafeMoveUpdatedComponent(Adjusted, NewRotation, true, Hit); 
	//(delta, rot, capsule cast (not teleport, prevents noclip), hitresult if hit something)


	//	did we hit something along the way during this update?
	if (Hit.Time < 1.f)
	{
		HandleImpact(Hit, deltaTime, Adjusted);
		SlideAlongSurface(Adjusted, (1.f - Hit.Time), Hit.Normal, Hit, true);
	}


	//cehck slide conditions
	FHitResult NewSurfaceHit;
	if (!GetSlideSurface(NewSurfaceHit) || Velocity.SizeSquared() < pow(Slide_MinSpeed, 2))
	{
		ExitSlide();
	}
	
	//update outgoing velocity and acceleration
	if (!bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / deltaTime;
	}
}

bool UPlayer_CMC::GetSlideSurface(FHitResult& Hit) const
{
	//Basic Line Trace
	FVector Start = UpdatedComponent->GetComponentLocation();
	FVector End = Start + CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 2.f * FVector::DownVector;
	FName ProfileName = TEXT("BlockAll");
	return GetWorld()->LineTraceSingleByProfile(Hit, Start, End, ProfileName, PlayerCharacterOwner->GetIgnoreCharacterParams());
}