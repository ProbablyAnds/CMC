#include "PlayerCMC.h"

UPlayerCMC::UPlayerCMC()
{
}

bool UPlayerCMC::FSavedMove_Player::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	return false;
}

void UPlayerCMC::FSavedMove_Player::Clear()
{
}

uint8 UPlayerCMC::FSavedMove_Player::GetCompressedFlags() const
{
	return uint8();
}

void UPlayerCMC::FSavedMove_Player::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
}

void UPlayerCMC::FSavedMove_Player::PrepMoveFor(ACharacter* C)
{
}
