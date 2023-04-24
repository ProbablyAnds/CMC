#include "Player_CMC.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Net/UnrealNetwork.h"
#include "CMCCharacter.h"
#include "DrawDebugHelpers.h"

// Helper Macros
#if 1
float MacroDuration = 2.f;
//logs to screen
#define SLOG(x) GEngine->AddOnScreenDebugMessage(-1, MacroDuration ? MacroDuration : -1.f, FColor::Yellow, x);
//point debug
#define POINT(x, c) DrawDebugPoint(GetWorld(), x, 10, c, !MacroDuration, MacroDuration);
//line debug
#define LINE(x1, x2, c) DrawDebugLine(GetWorld(), x1, x2, c, !MacroDuration, MacroDuration);
//capsule debug
#define CAPSULE(x, c) DrawDebugCapsule(GetWorld(), x, CapHH(), CapR(), FQuat::Identity, c, !MacroDuration, MacroDuration);
#else
#define SLOG(x)
#define POINT(x, c)
#define LINE(x1, x2, c)
#define CAPSULE(x, c)
#endif

#pragma region ActorComponent

//===============Component

UPlayer_CMC::UPlayer_CMC()
{
	NavAgentProps.bCanCrouch = true; //enables engine side crouch variable
}

void UPlayer_CMC::InitializeComponent()
{
	Super::InitializeComponent();

	PlayerCharacterOwner = Cast<ACMCCharacter>(GetOwner());
}

//===============Network

void UPlayer_CMC::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	Safe_bWantsToSprint = (Flags & FSavedMove_Player::FLAG_Sprint) != 0;
}

//will only create a new predData if we haven't already created it else we return regardless
FNetworkPredictionData_Client* UPlayer_CMC::GetPredictionData_Client() const
{
	check(PawnOwner != nullptr)

		//if the client prediction data is null
		if (ClientPredictionData == nullptr)
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

//==============Getters Setters Helpers

bool UPlayer_CMC::IsMovingOnGround() const
{
	return Super::IsMovingOnGround() || IsCustomMovementMode(CMOVE_Slide);
}

bool UPlayer_CMC::CanCrouchInCurrentState() const
{
	return Super::CanCrouchInCurrentState() && IsMovingOnGround();
}

float UPlayer_CMC::CapR() const //capsule radius
{
	return CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius();
}

float UPlayer_CMC::CapHH() const //capsule half height
{
	return CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
}

//===============Movement Pipeline

void UPlayer_CMC::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);

	//if (IsMovementMode(MOVE_Flying) && !HasRootMotionSources() && Safe_bHadAnimRootMotion)
	//{
	//	SetMovementMode(MOVE_Walking);
	//}


	////if in walking movement move then adjust the movement speed based on bWantsToSprint
	//if (MovementMode == MOVE_Walking)
	//{
	//	if (Safe_bWantsToSprint)
	//	{
	//		MaxWalkSpeed = Sprint_MaxWalkSpeed;
	//	}
	//	else
	//	{
	//		MaxWalkSpeed = Walk_MaxWalkSpeed;
	//	}
	//}

	//Safe_bHadAnimRootMotion = HasRootMotionSources();
	Safe_bPrevWantsToCrouch = bWantsToCrouch;
}

void UPlayer_CMC::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	if (PreviousMovementMode == MOVE_Custom && PreviousCustomMode == CMOVE_Slide) ExitSlide();
	
	if (IsCustomMovementMode(CMOVE_Slide)) EnterSlide();
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

	// Try Mantle
	if (PlayerCharacterOwner->bCMCPressedJump) //if jump is true
	{	//then we try to mantle
		if (TryMantle())
		{
			PlayerCharacterOwner->StopJumping();
		}
		else
		{
			//failed to mantle  - reverting to normal jump
			PlayerCharacterOwner->bCMCPressedJump = false;
			CharacterOwner->bPressedJump = true;
			CharacterOwner->CheckJumpInput(DeltaSeconds);
		}
	}


	//==================================Transition Mantle
	if (Safe_bTransitionFinished)
	{
SLOG("Transition Finished")
		UE_LOG(LogTemp, Warning, TEXT("FINISHED ROOTMOTION"))
		if (IsValid(TransitionQueuedMontage))
		{
			SetMovementMode(MOVE_Flying);
			CharacterOwner->PlayAnimMontage(TransitionQueuedMontage, TransitionQueuedMontageSpeed);
			TransitionQueuedMontageSpeed = 0.f;
			TransitionQueuedMontage = nullptr;
		}
		else
		{
			SetMovementMode(MOVE_Walking);
		}
		Safe_bTransitionFinished = false;
	}

	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
}

void UPlayer_CMC::UpdateCharacterStateAfterMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateAfterMovement(DeltaSeconds);
	if (!HasAnimRootMotion() && Safe_bHadAnimRootMotion && IsMovementMode(MOVE_Flying))
	{
		UE_LOG(LogTemp, Warning, TEXT("Ending Anim Root Motion"))
		SetMovementMode(MOVE_Walking);
	}
	if (GetRootMotionSourceByID(TransitionRMS_ID) && GetRootMotionSourceByID(TransitionRMS_ID)->Status.HasFlag(ERootMotionSourceStatusFlags::Finished))
	{
		RemoveRootMotionSourceByID(TransitionRMS_ID);
		Safe_bTransitionFinished = true;
	}

	Safe_bHadAnimRootMotion = HasAnimRootMotion();
}

#pragma endregion

#pragma region Saved Move

UPlayer_CMC::FSavedMove_Player::FSavedMove_Player() 
{
	Saved_bWantsToSprint = 0;
	Saved_bPrevWantsToCrouch = 0;
}

//checks new and current move to check if it can be combined to save bandwidth if they are identical
bool UPlayer_CMC::FSavedMove_Player::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	//cast new move to player move
	const FSavedMove_Player* NewPlayerMove = static_cast<FSavedMove_Player*>(NewMove.Get());

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
	uint8 Result = FSavedMove_Character::GetCompressedFlags();

	//changing cust flag if bWantsToSprint is true
	if (Saved_bWantsToSprint) Result |= FLAG_Sprint;
	if (Saved_bCMCPressedJump) Result |= FLAG_JumpPressed;

	return Result;
}

//captures the state data of the character movement component
//will look through all of the all vars in cmc and set their saved variables
//-setting saved move for the current snapshot of the cmc
void UPlayer_CMC::FSavedMove_Player::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	FSavedMove_Character::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	const UPlayer_CMC* PlayerMovement = Cast<UPlayer_CMC>(C->GetCharacterMovement());

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
	FSavedMove_Character::PrepMoveFor(C);

	UPlayer_CMC* PlayerMovement = Cast<UPlayer_CMC>(C->GetCharacterMovement());

	PlayerMovement->Safe_bWantsToSprint = Saved_bWantsToSprint;
	PlayerMovement->Safe_bPrevWantsToCrouch = Saved_bPrevWantsToCrouch;
	PlayerMovement->PlayerCharacterOwner->bCMCPressedJump = Saved_bCMCPressedJump;

	PlayerMovement->Safe_bHadAnimRootMotion = Saved_bHadAnimRootMotion;
	PlayerMovement->Safe_bTransitionFinished = Saved_bTransitionFinished;
}



#pragma endregion

#pragma region Client Network Prediction Data

UPlayer_CMC::FNetworkPredictionData_Client_Player::FNetworkPredictionData_Client_Player(const UPlayer_CMC& ClientMovement)
	: Super(ClientMovement)
{
}

FSavedMovePtr UPlayer_CMC::FNetworkPredictionData_Client_Player::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_Player());
}

#pragma endregion

#pragma region Slide

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

#pragma endregion

#pragma region Mantle

//Mantle
bool UPlayer_CMC::TryMantle()
{
	//Is not crouch walking and is not falling
	if (!(IsMovementMode(MOVE_Walking) && !IsCrouching()) && !IsMovementMode(MOVE_Falling)) return false;

	//============= Helper Variables 

	FVector BaseLoc = UpdatedComponent->GetComponentLocation() + FVector::DownVector * CapHH();
	FVector Fwd = UpdatedComponent->GetForwardVector().GetSafeNormal2D();
	auto Params = PlayerCharacterOwner->GetIgnoreCharacterParams();
	float MaxHeight = CapHH() * 2 + MantleReachHeight;
	float CosMMWSA = FMath::Cos(FMath::DegreesToRadians(MantleMinWallSteepnessAngle));
	float CosMMSA = FMath::Cos(FMath::DegreesToRadians(MantleMaxSurfaceAngle));
	float CosMMAA = FMath::Cos(FMath::DegreesToRadians(MantleMaxAlignmentAngle));

SLOG("Starting Mantle Attempt")

	//===================Check Front Face

	FHitResult FrontHit;				//Velocity dot ForwardVector
	float CheckDistance = FMath::Clamp(Velocity | Fwd, CapR() + 30, MantleMaxDistance);
	FVector FrontStart = BaseLoc + FVector::UpVector * (MaxStepHeight - 1);	//Max step height is used because you will eithber step up or mantle over an obsticle 
	for (int i = 0; i < 6; i++)// cast multiple rays out of the charaacter to check for obsticle
	{
LINE(FrontStart, FrontStart + Fwd * CheckDistance, FColor::Red);
		if (GetWorld()->LineTraceSingleByProfile(FrontHit, FrontStart, FrontStart + Fwd * CheckDistance, "BlockAll", Params)) break;
		FrontStart += FVector::UpVector * (2.f * CapHH() - (MaxStepHeight - 1)) / 5;
	}
	if (!FrontHit.IsValidBlockingHit()) return false; // return false if nothing was hit
	//steepness angle is used to calculate if it is an overhang
	float CosWallSteepnessAngle = FrontHit.Normal | FVector::UpVector;	//coSine wall steepness angle =  Normal(Adjacent to surface) dotprod Up vector
	//MantleMinWallSteepnessAngle OR ForwardVect dotprod negative normal vector of wall
	if (FMath::Abs(CosWallSteepnessAngle) > CosMMWSA || (Fwd | -FrontHit.Normal) < CosMMAA) return false;
POINT(FrontHit.Location, FColor::Red);

	//=====================Check Height
	
	//Array of Hit Result = every height hit is stored here
	TArray<FHitResult> HeightHits;
	FHitResult SurfaceHit;
	FVector WallUp = FVector::VectorPlaneProject(FVector::UpVector, FrontHit.Normal).GetSafeNormal();
	float WallCos = FVector::UpVector | FrontHit.Normal;
	float WallSin = FMath::Sqrt(1 - WallCos * WallCos);
	FVector TraceStart = FrontHit.Location + Fwd + WallUp * (MaxHeight - (MaxStepHeight - 1)) / WallSin;
LINE(TraceStart, FrontHit.Location + Fwd, FColor::Orange);
		if (!GetWorld()->LineTraceMultiByProfile(HeightHits, TraceStart, FrontHit.Location + Fwd, "BlockAll", Params)) return false;
	for (const FHitResult& Hit : HeightHits)
	{
		if (Hit.IsValidBlockingHit())
		{
			SurfaceHit = Hit;
			break;
		}
	}
	if (!SurfaceHit.IsValidBlockingHit() || (SurfaceHit.Normal | FVector::UpVector) < CosMMSA) return false;
	float Height = (SurfaceHit.Location - BaseLoc) | FVector::UpVector;

SLOG(FString::Printf(TEXT("Height: %f"), Height))
POINT(SurfaceHit.Location, FColor::Blue);

	if (Height > MaxHeight) return false;

	//==========================Check Clearance
	 
	float SurfaceCos = FVector::UpVector | SurfaceHit.Normal;
	float SurfaceSin = FMath::Sqrt(1 - SurfaceCos * SurfaceCos);
	FVector ClearCapLoc = SurfaceHit.Location + Fwd * CapR() + FVector::UpVector * (CapHH() + 1 + CapR() * 2 * SurfaceSin);
	FCollisionShape CapShape = FCollisionShape::MakeCapsule(CapR(), CapHH());
	if (GetWorld()->OverlapAnyTestByProfile(ClearCapLoc, FQuat::Identity, "BlockAll", CapShape, Params))
	{
CAPSULE(ClearCapLoc, FColor::Red)
			return false;
	}
	else
	{
CAPSULE(ClearCapLoc, FColor::Green)
	}

	SLOG("Can Mantle")

	//==========Mantle Selection == 
	FVector ShortMantleTarget = GetMantleStartLocation(FrontHit, SurfaceHit, false);
	FVector TallMantleTarget = GetMantleStartLocation(FrontHit, SurfaceHit, true);

	//============Customise Mantle

	bool bTallMantle = false;
	if (IsMovementMode(MOVE_Walking) && Height > CapHH() * 2)
	{
		bTallMantle = true;
	}
	else if (IsMovementMode(MOVE_Falling) && (Velocity | FVector::UpVector) < 0)
	{
		if (!GetWorld()->OverlapAnyTestByProfile(TallMantleTarget, FQuat::Identity, "BlockAll", CapShape, Params))
		{
			bTallMantle = true;
		}
	}
	FVector TransitionTarget = bTallMantle ? TallMantleTarget : ShortMantleTarget;
CAPSULE(TransitionTarget, FColor::Yellow)	//tranisition to

	//============================================Perform Transition To Mantle

CAPSULE(UpdatedComponent->GetComponentLocation(), FColor::Red) //whyere we currently are

	float UpSpeed = Velocity | FVector::UpVector;
	float TransDistance = FVector::Dist(TransitionTarget, UpdatedComponent->GetComponentLocation());	//Upspeed usedto change mantle speed based on verticle speed

	TransitionQueuedMontageSpeed = FMath::GetMappedRangeValueClamped(FVector2D(-500, 750), FVector2D(.9f, 1.2f), UpSpeed); //
	TransitionRMS.Reset();
	TransitionRMS = MakeShared<FRootMotionSource_MoveToForce>();
	TransitionRMS->AccumulateMode = ERootMotionAccumulateMode::Override;

	TransitionRMS->Duration = FMath::Clamp(TransDistance / 500.f, .1f, .25f);
SLOG(FString::Printf(TEXT("Duration: %f"), TransitionRMS->Duration))
	TransitionRMS->StartLocation = UpdatedComponent->GetComponentLocation();
	TransitionRMS->TargetLocation = TransitionTarget;
	
	//Apply Tranisition Root Motion Source
	Velocity = FVector::ZeroVector;
	SetMovementMode(MOVE_Flying);
	TransitionRMS_ID = ApplyRootMotionSource(TransitionRMS);

	//=====================================Animations
	if (bTallMantle)
	{
		TransitionQueuedMontage = TallMantleMontage;
		CharacterOwner->PlayAnimMontage(TransitionTallMantleMontage, 1 / TransitionRMS->Duration);
		if (IsServer()) Proxy_bTallMantle = !Proxy_bTallMantle;
	}
	else
	{
		TransitionQueuedMontage = ShortMantleMontage;
		CharacterOwner->PlayAnimMontage(TransitionShortMantleMontage, 1 / TransitionRMS->Duration);
		if (IsServer()) Proxy_bShortMantle = !Proxy_bShortMantle;
	}
	return true;
}

FVector UPlayer_CMC::GetMantleStartLocation(FHitResult FrontHit, FHitResult SurfaceHit, bool bTallMantle) const
{
	float CosWallSteepnessAngle = FrontHit.Normal | FVector::UpVector;
	float DownDistance = bTallMantle ? CapHH() * 2.f : MaxStepHeight - 1;
	FVector EdgeTangent = FVector::CrossProduct(SurfaceHit.Normal, FrontHit.Normal).GetSafeNormal();
	FVector MantleStart = SurfaceHit.Location;
	MantleStart += FrontHit.Normal.GetSafeNormal2D() * (2.f + CapR());
	MantleStart += UpdatedComponent->GetForwardVector().GetSafeNormal2D().ProjectOnTo(EdgeTangent) * CapR() * .3f;
	MantleStart += FVector::UpVector * CapHH();
	MantleStart += FVector::DownVector * DownDistance;
	MantleStart += FrontHit.Normal.GetSafeNormal2D() * CosWallSteepnessAngle * DownDistance;
	return MantleStart;
}

#pragma endregion

#pragma region Interface

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
	bWantsToCrouch = ~bWantsToCrouch;
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

bool UPlayer_CMC::IsMovementMode(EMovementMode InMovementMode) const
{
	return InMovementMode == MovementMode;
}

#pragma endregion

#pragma region Replication

void UPlayer_CMC::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UPlayer_CMC, Proxy_bShortMantle, COND_SkipOwner)
	DOREPLIFETIME_CONDITION(UPlayer_CMC, Proxy_bTallMantle, COND_SkipOwner)
}

void UPlayer_CMC::OnRep_ShortMantle()
{
	CharacterOwner->PlayAnimMontage(ProxyShortMantleMontage);
}

void UPlayer_CMC::OnRep_TallMantle()
{
	CharacterOwner->PlayAnimMontage(ProxyTallMantleMontage);
}

bool UPlayer_CMC::IsServer() const
{
	return CharacterOwner->HasAuthority();
}

#pragma endregion

