// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CMCCharacter.h"
#include "CoreMinimal.h"
#include "CMC.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player_CMC.generated.h"


UENUM(BlueprintType)
enum ECustomMovementMode
{
	CMOVE_None		UMETA(Hidden),
	CMOVE_Slide		UMETA(DisplayName = "Slide"),
	CMOVE_MAX		UMETA(Hidden)
};

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
		//used to detect when bcrouch is flipped - recreates crouich event to repl on all clients
		uint8 Saved_bPrevWantsToCrouch : 1;
		uint8 Saved_bCMCPressedJump : 1;
		uint8 Saved_bHadAnimRootMotion : 1;
		uint8 Saved_bTransitionFinished : 1;

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

	//editable in BP - determines characters speed when walking and sprinting
	UPROPERTY(EditDefaultsOnly) float Sprint_MaxWalkSpeed;
	UPROPERTY(EditDefaultsOnly) float Walk_MaxWalkSpeed;

	//Referance to the character owner -  
	UPROPERTY(Transient) ACMCCharacter* PlayerCharacterOwner;

	//(Component : Owner)
	//(PawnMovementComponent : PawnOwner)
	//(CharacterMovementComponent : CharacterOwner)
	//(PlayerCharacterMovementComponent: PlayerCharacterOwner)

	// Slide

	UPROPERTY(EditDefaultsOnly) float Slide_MinSpeed = 400;			//min speed to slide
	UPROPERTY(EditDefaultsOnly) float Slide_EnterImpulse = 400;		//boost got from entering slide
	UPROPERTY(EditDefaultsOnly) float Slide_GravityForce = 200;	//force applied to player agaist ground
	UPROPERTY(EditDefaultsOnly) float Slide_Friction = .1;			//decelleration

	//	Mantle
		//Control Vars - controls how the mantle functions
		UPROPERTY(EditDefaultsOnly) float MantleMaxDistance = 200;
		UPROPERTY(EditDefaultsOnly) float MantleReachHeight = 50;
		UPROPERTY(EditDefaultsOnly) float MinMantleDepth = 30;
		UPROPERTY(EditDefaultsOnly) float MantleMinWallSteepnessAngle = 75;
		UPROPERTY(EditDefaultsOnly) float MantleMaxSurfaceAngle = 40;
		UPROPERTY(EditDefaultsOnly) float MantleMaxAlignmentAngle = 45;
		//mantle animations
		UPROPERTY(EditDefaultsOnly) UAnimMontage* TallMantleMontage;
		UPROPERTY(EditDefaultsOnly) UAnimMontage* TransitionTallMantleMontage;
		UPROPERTY(EditDefaultsOnly) UAnimMontage* ProxyTallMantleMontage;
		UPROPERTY(EditDefaultsOnly) UAnimMontage* ShortMantleMontage;
		UPROPERTY(EditDefaultsOnly) UAnimMontage* TransitionShortMantleMontage;
		UPROPERTY(EditDefaultsOnly) UAnimMontage* ProxyShortMantleMontage;

	bool Safe_bWantsToSprint;

	//stored in the saved var - used for logic and rep - need saved to recreate move that made the state
	bool Safe_bPrevWantsToCrouch; //working var - need to add to set and prep functions

	bool Safe_bHadAnimRootMotion;

	//Transitions - into the mantle
	bool Safe_bTransitionFinished;
	TSharedPtr<FRootMotionSource_MoveToForce> TransitionRMS;
	UPROPERTY(Transient) UAnimMontage* TransitionQueuedMontage;
	float TransitionQueiedMontageSpeed;
	int TransitionRMS_ID;

	// Replication - reped variables - controls the animimations of others on your screen
		UPROPERTY(ReplicatedUsing = OnRep_ShortMantle) bool Proxy_bShortMantle;
		UPROPERTY(ReplicatedUsing = OnRep_TallMantle) bool Proxy_bTallMantle; 

public:
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

	virtual bool IsMovingOnGround() const override;
	virtual bool CanCrouchInCurrentState() const override;

protected:
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;

	//automatically called at the end of every perform move call
	//allows you to write movement logic REGLARDLESS of what movement mode you are in 
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override; 

	//where we do the edge detection - where we enter slide movement
	//this is where crouch mechanic is updated - we need to perform slide update before crouch update
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;

	virtual void UpdateCharacterStateAfterMovement(float DeltaSeconds) override;

	virtual void PhysCustom(float deltaTime, int32 Iterations) override;

//Slide System
private:
	void EnterSlide();
	void ExitSlide();
	void PhysSlide(float deltaTime, int32 Iterations);	//slide mechanics
	bool GetSlideSurface(FHitResult& Hit) const;	//helper
	
//Mantle
private:
	bool TryMantle();
	FVector GetMantleStartLocation(FHitResult FrontHit, FHitResult SurfaceHit, bool bTallMantle) const;
//Helpers
private:
	bool IsServer() const;
	float CapR() const;
	float CapHH() const;

public:
	UPlayer_CMC();

protected:
	virtual void InitializeComponent() override;

public:
	UFUNCTION(BlueprintCallable) void SprintPressed();
	UFUNCTION(BlueprintCallable) void SprintReleased();

	//Called on client - not on the server
	UFUNCTION(BlueprintCallable) void CrouchPressed();
	UFUNCTION(BlueprintCallable) void CrouchReleased();

	//check to see if we are in a custom movement mode
	UFUNCTION(BlueprintPure) bool IsCustomMovementMode(ECustomMovementMode InCustomMovementMode) const;

	//Replication on the rpoxy 
private:
	UFUNCTION() void OnRep_ShortMante();
	UFUNCTION() void OnRep_TallMantle();
};


