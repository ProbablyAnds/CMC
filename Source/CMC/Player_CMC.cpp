// Fill out your copyright notice in the Description page of Project Settings.


#include "Player_CMC.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Net/UnrealNetwork.h"

UPlayer_CMC::UPlayer_CMC()
{
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
}


//compressed flags are the way client will rep movement data
//eight flags as it is an eight bit integer
uint8 UPlayer_CMC::FSavedMove_Player::GetCompressedFlags() const
{
	//running super
	uint8 Result = Super::GetCompressedFlags();

	//changing cust flag if bWantsToSprint is true
	if (Saved_bWantsToSprint) Result |= FLAG_Custom_0;

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
}


//this is the exact opposite as above
//takes the data in the saved move and apply it to the current state of the cmc 
void UPlayer_CMC::FSavedMove_Player::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);

	UPlayer_CMC* PlayerMovement = Cast<UPlayer_CMC>(C->GetCharacterMovement());

	PlayerMovement->Safe_bWantsToSprint = Saved_bWantsToSprint;
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
