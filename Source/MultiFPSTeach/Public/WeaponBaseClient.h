// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeaponBaseClient.generated.h"

UCLASS()
class MULTIFPSTEACH_API AWeaponBaseClient : public AActor
{
	GENERATED_BODY()
	
public:	
	AWeaponBaseClient();
protected:
	virtual void BeginPlay() override;
public:	
	virtual void Tick(float DeltaTime) override;


public:
	UFUNCTION(BlueprintImplementableEvent,Category = "FPGunAnimation")
	void PlayShootAnimation();

	UFUNCTION(BlueprintImplementableEvent,Category = "FPGunAnimation")
	void PlayReloadAnimation();
	
	void DisplayWeaponEffect();

public:
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	USkeletalMeshComponent* WeaponMesh;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	UAnimMontage* ClientArmsFireAnimMontage;
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	UAnimMontage* ClientArmsReloadMontage;

	//
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	USoundBase* FireSound;
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	UParticleSystem* MuzzleFlash;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TSubclassOf<UCameraShakeBase> CameraShakeClass;

	UPROPERTY(EditAnywhere)
	int FPArmsBlendPoss;
};
