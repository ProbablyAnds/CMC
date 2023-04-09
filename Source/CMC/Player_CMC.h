// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CMC.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player_CMC.generated.h"

UCLASS()
class CMC_API UPlayer_CMC : public UCharacterMovementComponent
{
	GENERATED_BODY()

	/*-------------------------------Description opf movement safe var - custom fSavedMove and flags----------------------------------------------

	OnTick will call perform move -> exec all movement logic -> util value of safe_bWantsToSprint
	Then it will create a saved move -> using SetMoveFor to read of the current value of Safe_bWantsToSprint saving it into Saved_bWantsToSprint
	then it calls canCombineWith to see if there are any duplicate pending moves waiting to be sent to the server -> combining indentical moves when nessecary
	then it calls getCompressedFlags -> reduces savedMove into small networkable 8bit uInt packet that can be sent to the server
	then when server rec move it calls UpdateFromCompressedFlags -> Updates state values such as Safe_bWantToSprint from client movement data 
	then the server will perform the move the client performed
	after this those two moves made (on server and client) should be identical
	movement safe var auto replicated from client to server - completely movement safe - will be insync iwht server and client updates - reducing rubber banding
	
	-----------------------------------------------------------------------------------------------------------------------------------------------*/

	class FSavedMove_Player : public FSavedMove_Character
	{
		typedef FSavedMove_Character Super;

		uint8 Saved_bWantsToSprint : 1;

		virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
		virtual void Clear() override;
		virtual uint8 GetCompressedFlags() const override;
		virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override;
		virtual void PrepMoveFor(ACharacter* C) override;
	};

	class FNetworkPredictionData_Client_Player : public FNetworkPredictionData_Client_Character
	{
	public:
		FNetworkPredictionData_Client_Player(const UPlayer_CMC& ClientMovement);

		typedef FNetworkPredictionData_Client_Character Super;

		virtual FSavedMovePtr AllocateNewMove() override;
	};

	bool Safe_bWantsToSprint;

public:
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

protected:
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;

public:
	UPlayer_CMC();
};


