// Fill out your copyright notice in the Description page of Project Settings.


#include "WeaponBaseClient.h"

#include "Kismet/GameplayStatics.h"

// Sets default values
AWeaponBaseClient::AWeaponBaseClient()
{
	PrimaryActorTick.bCanEverTick = true;
	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	RootComponent = WeaponMesh;
	WeaponMesh->SetOnlyOwnerSee(true);
}

void AWeaponBaseClient::BeginPlay()
{
	Super::BeginPlay();
	
}

void AWeaponBaseClient::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AWeaponBaseClient::DisplayWeaponEffect()
{
	//播放声音
	UGameplayStatics::PlaySound2D(GetWorld(),FireSound);//和距离无关
	//播放粒子特效
	UGameplayStatics::SpawnEmitterAttached(MuzzleFlash, WeaponMesh,	TEXT("Fire_FX_Slot"),
		 FVector::ZeroVector,FRotator::ZeroRotator,FVector::OneVector,EAttachLocation::KeepRelativeOffset,
		 true,EPSCPoolMethod::None, true);
}

