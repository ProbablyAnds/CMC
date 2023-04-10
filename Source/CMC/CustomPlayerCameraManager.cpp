// Fill out your copyright notice in the Description page of Project Settings.

 
#include "CustomPlayerCameraManager.h"
#include "CMCCharacter.h"
#include "Player_CMC.h"
#include "Components/CapsuleComponent.h"

ACustomPlayerCameraManager::ACustomPlayerCameraManager()
{
}

void ACustomPlayerCameraManager::UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime)
{
	Super::UpdateViewTarget(OutVT, DeltaTime);
	ACMCCharacter* PlayerCharacter = Cast<ACMCCharacter>(GetOwningPlayerController()->GetPawn());
	//checking is player controller is posing a player character pre
	//prevents this function from running on a player controller posing a specator pawn
	if (PlayerCharacter)
	{
		UPlayer_CMC* P_CMC = PlayerCharacter->GetPlayerCustomMovementComponent();
		//target crouch offset -> z vector3 with the a axis of		
		//crouch half height minus characters scaled capsule half height

		FVector TargetCrouchOffset 
			= FVector(0, 0,
			P_CMC->GetCrouchedHalfHeight() - 
			PlayerCharacter->GetClass()->GetDefaultObject<ACharacter>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

		//Actual crouch offset -> is a vector with is linear interpolated between
		//no offset and the target offset with the alpha being
		//CBTime / CBDuration (between 0 and 1)
		FVector Offset = FMath::Lerp(FVector::ZeroVector, TargetCrouchOffset, 
			FMath::Clamp(CrouchBlendTime / CrouchBlendDuration, 0.f, 1.f));

		//need to adjust the blend times using DT before applying the offset

		if (P_CMC->IsCrouching())//going into a crouch > go from zero to blend duration
		{
			//if the player is crouching
			//blend time will be current blend time + DT clamped to the max CrouchBlendDuration
			CrouchBlendTime = FMath::Clamp(CrouchBlendTime + DeltaTime, 0.f, CrouchBlendDuration);
			//Decrement the actual offset by the target offset
			Offset -= TargetCrouchOffset;	//assists the lerp in making the transtion more smooth
		}
		else//leaving a crouch > go from blend duration to zero
		{
			//else if the player is not crouching then
			//blend time will be reduced by the DT clamped to 0
			CrouchBlendTime = FMath::Clamp(CrouchBlendDuration - DeltaTime, 0.f, CrouchBlendDuration);
		}
		
		//now we apply the actual offset
		if (P_CMC->IsMovingOnGround()) //reason - camaera wont change if char is mid jump
		{								//only want to change this if we are already on the ground
			OutVT.POV.Location += Offset;
		}
	}
}
