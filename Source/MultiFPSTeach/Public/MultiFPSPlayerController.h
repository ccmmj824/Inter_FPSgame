// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MultiFPSPlayerController.generated.h"

/**
 * 
 */
UCLASS()
class MULTIFPSTEACH_API AMultiFPSPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	void PlayerCameraShake(TSubclassOf<UCameraShakeBase> CameraShake);
	//动态绑定UI所以蓝图？
	UFUNCTION(BlueprintImplementableEvent,Category="PlayerUI")
	void CreatePlayerUI();
	UFUNCTION(BlueprintImplementableEvent,Category="PlayerUI")
	void DoCrosshairRecoil();
	UFUNCTION(BlueprintImplementableEvent,Category="PlayerUI")
	void UpdataAmmo(int32 ClipCurrentAmmo, int32 GunCurrentAmmo);
	UFUNCTION(BlueprintImplementableEvent,Category="PlayerUI")
	void UpdataHealthUI(float NewHealth);

	UFUNCTION(BlueprintImplementableEvent,Category="Health")
	void DeathMathDeath(AActor* DamageActor);
};
