// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "CustomPlayerCameraManager.generated.h"

UCLASS()
class CMC_API ACustomPlayerCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()
private:
	UPROPERTY(EditDefaultsOnly) float CrouchBlendDuration = .5f;
	float CrouchBlendTime;

public:
	ACustomPlayerCameraManager();

	virtual void UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime) override;
};
