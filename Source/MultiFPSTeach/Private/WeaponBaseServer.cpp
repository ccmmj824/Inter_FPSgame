// Fill out your copyright notice in the Description page of Project Settings.


#include "WeaponBaseServer.h"

#include "FpsTeachBaseCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

AWeaponBaseServer::AWeaponBaseServer()
{
	PrimaryActorTick.bCanEverTick = true;
	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent> (TEXT("WeaponMesh"));
	RootComponent = WeaponMesh;
	SphereCollision = CreateDefaultSubobject<USphereComponent>(TEXT("SphereCollision"));
	SphereCollision->SetupAttachment(RootComponent);

	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	WeaponMesh->SetCollisionObjectType(ECollisionChannel::ECC_WorldStatic);

	SphereCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SphereCollision->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	WeaponMesh->SetOwnerNoSee(true);
	WeaponMesh->SetEnableGravity(true);
	WeaponMesh->SetSimulatePhysics(true);

	SphereCollision->OnComponentBeginOverlap.AddDynamic(this, &AWeaponBaseServer::OnOtherBeginOverlap);
	bReplicates = true;
	//SetReplicates(true); // ??? 有延迟
}

void AWeaponBaseServer::BeginPlay()
{
	Super::BeginPlay();
}

void AWeaponBaseServer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AWeaponBaseServer::OnOtherBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	AFpsTeachBaseCharacter* FPSCharacter = Cast<AFpsTeachBaseCharacter>(OtherActor);
	if (FPSCharacter)
	{
		EquipWeapon();

		if (KindOfWeapon == EWeaponType::DesertEagle)
		{
			
			FPSCharacter->EquipSecondary(this);
		}
		else
		{
			//玩家逻辑
			FPSCharacter->EquipPrimary(this);
		}
	}
}

void AWeaponBaseServer::EquipWeapon()
{
	WeaponMesh->SetEnableGravity(false);
	WeaponMesh->SetSimulatePhysics(false);
	SphereCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
 //服务端开火后业务
void AWeaponBaseServer::MultiShootingEffect_Implementation()
{
	//屏蔽调用进来的 即本人不做
	if(GetOwner()!=UGameplayStatics::GetPlayerPawn(GetWorld(),0))// 传0获得自己的pawn
	{
		FName MuzzleFlashSocketName = TEXT("Fire_FX_Slot");
		if(KindOfWeapon == EWeaponType::M4A1)
		{
			MuzzleFlashSocketName = TEXT("MuzzleSocket");
		}
		if(KindOfWeapon == EWeaponType::MP7)
		{
			MuzzleFlashSocketName = TEXT("MuzzleSocket");
		}
		//播放粒子效果
		UGameplayStatics::SpawnEmitterAttached(MuzzleFlash, WeaponMesh,	MuzzleFlashSocketName,
		 FVector::ZeroVector,FRotator::ZeroRotator,FVector::OneVector,EAttachLocation::KeepRelativeOffset,
		 true,EPSCPoolMethod::None, true);

		//播放声音  和距离有关
		UGameplayStatics::PlaySoundAtLocation(GetWorld(),FireSound,GetActorLocation());
	}
}

bool AWeaponBaseServer::MultiShootingEffect_Validate()
{
	return true;
}

void AWeaponBaseServer::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);//!!!
	DOREPLIFETIME_CONDITION(AWeaponBaseServer,ClipCurrentAmmo,COND_None)// None  永远同步
	DOREPLIFETIME_CONDITION(AWeaponBaseServer,GunCurrentAmmo,COND_None)
	DOREPLIFETIME_CONDITION(AWeaponBaseServer,MaxClipAmmo,COND_None)
}
