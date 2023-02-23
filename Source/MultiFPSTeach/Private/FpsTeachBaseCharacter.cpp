// Fill out your copyright notice in the Description page of Project Settings.


#include "MultiFPSTeach/Public/FpsTeachBaseCharacter.h"

#include <charconv>
#include <ios>
#include <variant>

#include "WeaponBaseClient.h"
#include "Components/DecalComponent.h"
#include "Engine/MicroTransactionBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Net/UnrealNetwork.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Windows/AllowWindowsPlatformTypes.h"

#pragma region Engine
// Sets default values
AFpsTeachBaseCharacter::AFpsTeachBaseCharacter()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;


	PlayerCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("PlyaerCamera"));
	if (PlayerCamera)
	{
		PlayerCamera->SetupAttachment(RootComponent);
		PlayerCamera->bUsePawnControlRotation = true;
	}
	FPArmsMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FPArmsMesh"));
	if (FPArmsMesh)
	{
		FPArmsMesh->SetupAttachment(PlayerCamera);
		FPArmsMesh->SetOnlyOwnerSee(true); //仅自己可见
	}
	Mesh->SetOwnerNoSee(true); //自己不可见

	//设置碰撞
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly); //这是MESH,胶囊体去处理真实碰撞
	Mesh->SetCollisionObjectType(ECollisionChannel::ECC_Pawn);
}

void AFpsTeachBaseCharacter::DelayBeginplayCallBack()
{
	FPSPlayerController = Cast<AMultiFPSPlayerController>(GetController());
	if (FPSPlayerController)
	{
		FPSPlayerController->CreatePlayerUI();
	}
	else
	{
		FLatentActionInfo ActionInfo;
		ActionInfo.CallbackTarget = this;
		ActionInfo.ExecutionFunction = TEXT("DelayBeginplayCallBack");
		ActionInfo.UUID = FMath::Rand();
		ActionInfo.Linkage = 0 ; //不能传-1；
		UKismetSystemLibrary::Delay(this,0.5,ActionInfo);
	}
}

// Called when the game starts or when spawned
void AFpsTeachBaseCharacter::BeginPlay()
{
	Super::BeginPlay();
	Health = 100;
	OnTakePointDamage.AddDynamic(this, &AFpsTeachBaseCharacter::OnHit); //绑定回调函数 受到伤害
	// StartWithKindWeapon();
	ClientArmsAnimBP = FPArmsMesh->GetAnimInstance();
	ServerBodyAnmiBP = Mesh->GetAnimInstance();              //播放设计动画时才有？？
	FPSPlayerController = Cast<AMultiFPSPlayerController>(GetController());
	if (FPSPlayerController)
	{
		FPSPlayerController->CreatePlayerUI();
	}
	else
	{
		FLatentActionInfo ActionInfo;
		ActionInfo.CallbackTarget = this;
		ActionInfo.ExecutionFunction = TEXT("DelayBeginplayCallBack");
		ActionInfo.UUID = FMath::Rand();
		ActionInfo.Linkage = 0 ; //不能传-1；
		UKismetSystemLibrary::Delay(this,0.5,ActionInfo);
	}
	bIsFiring = false;
	bReloading = false;
	
	StartWithKindWeapon();
}


// Called every frame
void AFpsTeachBaseCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void AFpsTeachBaseCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	InputComponent->BindAxis(TEXT("MoveRight"), this, &AFpsTeachBaseCharacter::MoveRight);
	InputComponent->BindAxis(TEXT("MoveForward"), this, &AFpsTeachBaseCharacter::MoveForward);

	InputComponent->BindAxis(TEXT("Turn"), this, &AFpsTeachBaseCharacter::AddControllerYawInput);
	InputComponent->BindAxis(TEXT("LookUp"), this, &AFpsTeachBaseCharacter::AddControllerPitchInput);
	InputComponent->BindAction(TEXT("Jump"), IE_Pressed, this, &AFpsTeachBaseCharacter::JumpAction);
	InputComponent->BindAction(TEXT("StopJump"), IE_Released, this, &AFpsTeachBaseCharacter::StopJumpAction);
	InputComponent->BindAction(TEXT("LowSpeedWalk"), IE_Pressed, this, &AFpsTeachBaseCharacter::LowSpeedWalkAction);
	InputComponent->BindAction(TEXT("LowSpeedWalk"), IE_Released, this, &AFpsTeachBaseCharacter::NormalSpeedWalkAction);

	InputComponent->BindAction(TEXT("Fire"), IE_Pressed, this, &AFpsTeachBaseCharacter::InputFirePressed);
	InputComponent->BindAction(TEXT("Fire"), IE_Released, this, &AFpsTeachBaseCharacter::InputFireReleased);
	InputComponent->BindAction(TEXT("Reload"), IE_Released,this, &AFpsTeachBaseCharacter::InputReload);
}

#pragma endregion
#pragma region NetWorking
/* 服务器客户端 行走同步  */
void AFpsTeachBaseCharacter::ServerLowSpeedWalkAction_Implementation()
{
	CharacterMovement->MaxWalkSpeed = 300;
}

bool AFpsTeachBaseCharacter::ServerLowSpeedWalkAction_Validate()
{
	return true;
}

void AFpsTeachBaseCharacter::ServerNormalSpeedWalkAction_Implementation()
{
	CharacterMovement->MaxWalkSpeed = 600;
}

bool AFpsTeachBaseCharacter::ServerNormalSpeedWalkAction_Validate()
{
	return true;
}

//武器开火    服务器端
void AFpsTeachBaseCharacter::ServerFireRifleWeapon_Implementation(FVector CameraLocation, FRotator CameraRotation,
                                                                  bool IsMoving)
{
	if (ServerPrimaryWeapon)
	{
		//多播 所有玩家可以看(必须在服务器调用，谁调用谁多播,  枪口闪光声音等，  子弹
		ServerPrimaryWeapon->MultiShootingEffect();
		ServerPrimaryWeapon->ClipCurrentAmmo -= 1; //服务器的-1了，客户端的没有-1.
		ClientUpdateAmmo(ServerPrimaryWeapon->ClipCurrentAmmo, ServerPrimaryWeapon->GunCurrentAmmo);
		//多播 枪口胳膊动画
		MultiShooting();

		bIsFiring = true;
		//格式化输出下
		//UKismetSystemLibrary::PrintString(this,FString::Printf
		//	(TEXT("ServerPrimaryWeapon->ClipCurrentAmmo: = %d"),ServerPrimaryWeapon->ClipCurrentAmmo));
	}
	RifleLineTrace(CameraLocation, CameraRotation, IsMoving);
}

//服务器多播 -> 手臂动画
void AFpsTeachBaseCharacter::MultiShooting_Implementation()
{
	AWeaponBaseServer* CurrentServerWeapon = GetCurrentServerTpBodysArmsWeaponActor();
	if (ServerBodyAnmiBP)
	{
		if (CurrentServerWeapon)
		{
			ServerBodyAnmiBP->Montage_Play(CurrentServerWeapon->ServerTPBodysShootAnimMontage);
		}
	}
}

bool AFpsTeachBaseCharacter::MultiShooting_Validate()
{
	return true;
}
//换弹
void AFpsTeachBaseCharacter::MultiSReloadAnimation_Implementation()
{
	AWeaponBaseServer* CurrentServerWeapon = GetCurrentServerTpBodysArmsWeaponActor();
	if (ServerBodyAnmiBP)
	{
		if (ServerPrimaryWeapon)
		{
			ServerBodyAnmiBP->Montage_Play(CurrentServerWeapon->ServerTPBodysReloadAnimMontage);
		}
	}
}
bool AFpsTeachBaseCharacter::MultiSReloadAnimation_Validate()
{
	return true;
}

//多播 生成单孔
void AFpsTeachBaseCharacter::MultiSpawnBulletDecal_Implementation(FVector Location, FRotator Rotation)
{
	AWeaponBaseServer* CurrentServerWeapon = GetCurrentServerTpBodysArmsWeaponActor();
	if (CurrentServerWeapon)
	{
		//世界物体， 弹孔属性，大小，
		UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(
			GetWorld(), CurrentServerWeapon->BulletDecalMaterial, FVector(8, 8, 8),
			Location, Rotation, 10);
		if (Decal) //必须判断！！！！！！！！！！！！！！！！！！！！！！！！！！ server没获得Decal,而client获得
		{
			Decal->SetFadeScreenSize(0.001);
		}
		else
		{
		}

		//FadeScreenSize 可以控制贴图逐渐变暗
	}
}

void AFpsTeachBaseCharacter::ClientEquipFPArmsSecondary_Implementation()
{
	if (ServerSecondaryWeapon)
	{
		if (ClientSecondaryWeapon)
		{
		}
		else
		{
			FActorSpawnParameters SpawnInfo;
			SpawnInfo.Owner = this;
			SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ClientSecondaryWeapon = GetWorld()->SpawnActor<AWeaponBaseClient>(
				ServerSecondaryWeapon->ClientWeaponBaseBPClass,
				GetActorTransform(), SpawnInfo);

			//不同武器的插槽不同
			FName WeaponSocketName = TEXT("WeaponSocket");
			ClientSecondaryWeapon->K2_AttachToComponent(FPArmsMesh,WeaponSocketName, EAttachmentRule::SnapToTarget,
													  EAttachmentRule::SnapToTarget,
													  EAttachmentRule::SnapToTarget,
													  true);

			ClientUpdateAmmo(ServerSecondaryWeapon->ClipCurrentAmmo, ServerSecondaryWeapon->GunCurrentAmmo);

			//改变手臂动画
			if (ClientSecondaryWeapon)
			{
				UpdateFPArmsBlendPose(ClientSecondaryWeapon->FPArmsBlendPoss);
			}
		}
	}
}

bool AFpsTeachBaseCharacter::MultiSpawnBulletDecal_Validate(FVector Location, FRotator Rotation)
{
	return true;
}


//客户端更新UI 子弹
void AFpsTeachBaseCharacter::ClientUpdateAmmo_Implementation(int32 ClipCurrentAmmo, int32 GunCurrentAmmo)
{
	if (FPSPlayerController)
	{
		FPSPlayerController->UpdataAmmo(ClipCurrentAmmo, GunCurrentAmmo);
	}
}

bool AFpsTeachBaseCharacter::ServerFireRifleWeapon_Validate(FVector CameraLocation, FRotator CameraRotation,
                                                            bool IsMoving)
{
	return true;
}

void AFpsTeachBaseCharacter::ServerFirePistolWeapon_Implementation(FVector CameraLocation, FRotator CameraRotation,
	bool IsMoving)
{
	if (ServerSecondaryWeapon)
	{
		//延时 单发
		FLatentActionInfo ActionInfo;
		ActionInfo.CallbackTarget = this;
		ActionInfo.ExecutionFunction = TEXT("DelaySpreadWeaponShootCallBack");
		ActionInfo.UUID = FMath::Rand();
		ActionInfo.Linkage = 0 ; //不能传-1；
		UKismetSystemLibrary::Delay(this,ServerSecondaryWeapon->SpreadWeaponCallBackRate,ActionInfo);
		//多播 所有玩家可以看(必须在服务器调用，谁调用谁多播,  枪口闪光声音等，  子弹
		ServerSecondaryWeapon->MultiShootingEffect();
		ServerSecondaryWeapon->ClipCurrentAmmo -= 1; //服务器的-1了，客户端的没有-1.
		ClientUpdateAmmo(ServerSecondaryWeapon->ClipCurrentAmmo, ServerSecondaryWeapon->GunCurrentAmmo);
		//多播 枪口胳膊动画
		
		MultiShooting();

		bIsFiring = true;
		//格式化输出下
		//UKismetSystemLibrary::PrintString(this,FString::Printf
		//	(TEXT("ServerPrimaryWeapon->ClipCurrentAmmo: = %d"),ServerPrimaryWeapon->ClipCurrentAmmo));
	}
	PistolLineTrace(CameraLocation, CameraRotation, IsMoving);
}

bool AFpsTeachBaseCharacter::ServerFirePistolWeapon_Validate(FVector CameraLocation, FRotator CameraRotation,
	bool IsMoving)
{
	return true;
}

void AFpsTeachBaseCharacter::ServerStopFiring_Implementation()
{
	bIsFiring = false;
}

bool AFpsTeachBaseCharacter::ServerStopFiring_Validate()
{
	return true;
}

void AFpsTeachBaseCharacter::ClientUpdateHealUI_Implementation(float NewHealth)
{
	if (FPSPlayerController)
	{
		FPSPlayerController->UpdataHealthUI(NewHealth);
	}
}

void AFpsTeachBaseCharacter::ClientEquipFPArmsPrimary_Implementation()
{
	if (ServerPrimaryWeapon)
	{
		if (ClientPrimaryWeapon)
		{
		}
		else
		{
			FActorSpawnParameters SpawnInfo;
			SpawnInfo.Owner = this;
			SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ClientPrimaryWeapon = GetWorld()->SpawnActor<AWeaponBaseClient>(
				ServerPrimaryWeapon->ClientWeaponBaseBPClass,
				GetActorTransform(), SpawnInfo);

			//不同武器的插槽不同
			FName WeaponSocketName = TEXT("WeaponSocket");
			if(ActiveWeapon == EWeaponType::M4A1)
			{
				WeaponSocketName = TEXT("M4A1_Socket");
			}
			ClientPrimaryWeapon->K2_AttachToComponent(FPArmsMesh,WeaponSocketName, EAttachmentRule::SnapToTarget,
			                                          EAttachmentRule::SnapToTarget,
			                                          EAttachmentRule::SnapToTarget,
			                                          true);

			ClientUpdateAmmo(ServerPrimaryWeapon->ClipCurrentAmmo, ServerPrimaryWeapon->GunCurrentAmmo);

			//改变手臂动画
			if (ClientPrimaryWeapon)
			{
				UpdateFPArmsBlendPose(ClientPrimaryWeapon->FPArmsBlendPoss);
			}
		}
	}

}


void AFpsTeachBaseCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	// 父类的方法要去实现下：！！！BUG
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AFpsTeachBaseCharacter,bIsFiring,COND_None);// None  永远同步
	DOREPLIFETIME_CONDITION(AFpsTeachBaseCharacter,bReloading,COND_None);
	DOREPLIFETIME_CONDITION(AFpsTeachBaseCharacter,ActiveWeapon,COND_None);
}
#pragma  endregion

#pragma region InputEvent


void AFpsTeachBaseCharacter::ClientRecoil_Implementation()
{
	UCurveFloat* VerticalRecoilCurve = nullptr;
	UCurveFloat* HorizontalRecoilCurve = nullptr;
	if (ServerPrimaryWeapon)
	{
		VerticalRecoilCurve = ServerPrimaryWeapon->VerticalRecoilCurve;
		HorizontalRecoilCurve = ServerPrimaryWeapon->HorizontalRecoilCurve;
	}
	RecoilXCoordPerShoot += 0.1; //不用初始化为0 ？？？ 用： stop里实现
	if (VerticalRecoilCurve)
	{
		NewVerticalRecoilAmount = VerticalRecoilCurve->GetFloatValue(RecoilXCoordPerShoot);
	}
	if (HorizontalRecoilCurve)
	{
		NewHorizontalRecoilAmount = HorizontalRecoilCurve->GetFloatValue(RecoilXCoordPerShoot);
	}
	VerticalRecoilAmount = NewVerticalRecoilAmount-OldVerticalRecoilAmount;
	HorizontalRecoilAmount= NewHorizontalRecoilAmount-OldHorizontalRecoilAmount;
	/*    */
	
	if (FPSPlayerController)
	{
		FRotator ControllerRotator = FPSPlayerController->GetControlRotation();
		FPSPlayerController->SetControlRotation(FRotator(ControllerRotator.Pitch + VerticalRecoilAmount,
		                                                 ControllerRotator.Yaw+HorizontalRecoilAmount, ControllerRotator.Roll));
	}

	OldVerticalRecoilAmount = NewVerticalRecoilAmount;
	OldHorizontalRecoilAmount = NewHorizontalRecoilAmount;
}

void AFpsTeachBaseCharacter::ClientReload_Implementation()
{
	
	AWeaponBaseClient* CurrentClientWeapon = GetCurrentClientFPArmsWeaponActor();
	// 客户端播放
	if(CurrentClientWeapon)
	{
		UAnimMontage* ClientArmsReloadMontage = CurrentClientWeapon->ClientArmsReloadMontage;
		ClientArmsAnimBP->Montage_Play(ClientArmsReloadMontage);
		CurrentClientWeapon->PlayReloadAnimation();
	}
}

void AFpsTeachBaseCharacter::ClientDeathMatchDeath_Implementation()
{
	AWeaponBaseClient* CurrentClientWeapon = GetCurrentClientFPArmsWeaponActor();

	if(CurrentClientWeapon)
	{
		CurrentClientWeapon->Destroy();
	}	
}

//primary reloading
void AFpsTeachBaseCharacter::ServerReloadPrimary_Implementation()
{

	//换弹逻辑条件  枪体无子弹， 弹夹子弹满的
	if (ServerPrimaryWeapon)
	{
		if (ServerPrimaryWeapon->GunCurrentAmmo > 0 && ServerPrimaryWeapon->ClipCurrentAmmo < ServerPrimaryWeapon->MaxClipAmmo)
		{
			//Client arms animation , serverMesh Mutiplay animation
			// update data,  UI

			bReloading = true;
			ClientReload();
			MultiSReloadAnimation();
			
			//动画播放完之后更改数据，所以增加延迟
			if(ClientPrimaryWeapon)
			{
				FLatentActionInfo ActionInfo;
				ActionInfo.CallbackTarget = this;
				ActionInfo.ExecutionFunction = TEXT("DelayPlayArmReloadCallBack");
				ActionInfo.UUID = FMath::Rand();
				ActionInfo.Linkage = 0 ; //不能传-1；
				UKismetSystemLibrary::Delay(this,ClientPrimaryWeapon->ClientArmsReloadMontage->GetPlayLength(),
											ActionInfo);
			}
		}
		
	}

}

bool AFpsTeachBaseCharacter::ServerReloadPrimary_Validate()
{
	return true;
}

void AFpsTeachBaseCharacter::ServerReloadSeconday_Implementation()
{
	//换弹逻辑条件  枪体无子弹， 弹夹子弹满的
	if (ServerSecondaryWeapon)
	{
		if (ServerSecondaryWeapon->GunCurrentAmmo > 0 && ServerSecondaryWeapon->ClipCurrentAmmo < ServerSecondaryWeapon->MaxClipAmmo)
		{
			//Client arms animation , serverMesh Mutiplay animation
			// update data,  UI

			bReloading = true;
			ClientReload();
			MultiSReloadAnimation();
			
			//动画播放完之后更改数据，所以增加延迟
			if(ServerSecondaryWeapon)
			{
				FLatentActionInfo ActionInfo;
				ActionInfo.CallbackTarget = this;
				ActionInfo.ExecutionFunction = TEXT("DelayPlayArmReloadCallBack");
				ActionInfo.UUID = FMath::Rand();
				ActionInfo.Linkage = 0 ; //不能传-1；
				UKismetSystemLibrary::Delay(this,ClientSecondaryWeapon->ClientArmsReloadMontage->GetPlayLength(),
											ActionInfo);
			}
		}
		
	}
}

bool AFpsTeachBaseCharacter::ServerReloadSeconday_Validate()
{
	return true;
}

void AFpsTeachBaseCharacter::MoveRight(float AxisValue)
{
	AddMovementInput(GetActorRightVector(), AxisValue, false);
}

void AFpsTeachBaseCharacter::MoveForward(float AxisValue)
{
	AddMovementInput(GetActorForwardVector(), AxisValue, false);
}

void AFpsTeachBaseCharacter::JumpAction()
{
	Jump();
}

void AFpsTeachBaseCharacter::StopJumpAction()
{
	StopJumping();
}

void AFpsTeachBaseCharacter::LowSpeedWalkAction()
{
	CharacterMovement->MaxWalkSpeed = 300;
	ServerLowSpeedWalkAction();
}

void AFpsTeachBaseCharacter::NormalSpeedWalkAction()
{
	CharacterMovement->MaxWalkSpeed = 600;
	ServerNormalSpeedWalkAction();
}

void AFpsTeachBaseCharacter::InputFirePressed()
{
	//根据武器类型 选择设计方法
	switch (ActiveWeapon)
	{
		case EWeaponType::Ak47:
		{
			FirePrimary();
		}
		break;
		case EWeaponType::M4A1:
		{
			FirePrimary();
		}
		break;
		case EWeaponType::MP7:
		{
			FirePrimary();
		}
		break;
		case EWeaponType::DesertEagle:
		{
			FireSecondary();
		}
	}
}

void AFpsTeachBaseCharacter::InputFireReleased()
{
	//根据武器类型 选择设计方法
	switch (ActiveWeapon)
	{
		case EWeaponType::Ak47:
		{
			StopFirePrimary();
		}
		break;
		case EWeaponType::M4A1:
		{
			StopFirePrimary();
		}
		break;
		case EWeaponType::MP7:
		{
			StopFirePrimary();
		}
		break;
		case EWeaponType::DesertEagle:
		{
			StopFireSecondary();
		}
	}
}



void AFpsTeachBaseCharacter::InputReload()
{
	// Reloading   Firing  --->  false
	if(!bReloading)
	{
		if(!bIsFiring)
		{
			// accoding to class of weapon  to chossice funtion's load
			switch (ActiveWeapon)
			{
				case EWeaponType::Ak47:
				{
					ServerReloadPrimary();
				}
				case EWeaponType::M4A1:
				{
					ServerReloadPrimary();
				}
				case EWeaponType::MP7:
				{
					ServerReloadPrimary();
				}
				case EWeaponType::DesertEagle:
				{
					ServerReloadSeconday();
				}
			}
		}
	}
}
#pragma endregion

#pragma region weapon

//将武器加到第三人称视角上面
void AFpsTeachBaseCharacter::EquipPrimary(AWeaponBaseServer* WeaponBaseServer)
{
	if (ServerPrimaryWeapon)
	{
	}
	else
	{
		ServerPrimaryWeapon = WeaponBaseServer;
		ServerPrimaryWeapon->SetOwner(this);
		ServerPrimaryWeapon->K2_AttachToComponent(Mesh,TEXT("Weapon_Rifle"), EAttachmentRule::SnapToTarget,
		                                          EAttachmentRule::SnapToTarget,
		                                          EAttachmentRule::SnapToTarget,
		                                          true);
		ActiveWeapon = ServerPrimaryWeapon->KindOfWeapon;// second!!!也要改！！！
		ClientEquipFPArmsPrimary();
	}
}

void AFpsTeachBaseCharacter::EquipSecondary(AWeaponBaseServer* WeaponBaseServer)
{
	if (ServerSecondaryWeapon)
	{
	}
	else
	{
		ServerSecondaryWeapon = WeaponBaseServer;
		ServerSecondaryWeapon->SetOwner(this);
		ServerSecondaryWeapon->K2_AttachToComponent(Mesh,TEXT("Weapon_Rifle"), EAttachmentRule::SnapToTarget,
												  EAttachmentRule::SnapToTarget,
												  EAttachmentRule::SnapToTarget,
												  true);
		ActiveWeapon = ServerSecondaryWeapon->KindOfWeapon;// second!!!也要改！！！
		ClientEquipFPArmsSecondary();
	}
}

//second也要改！！！！！！！！！！！！！

void AFpsTeachBaseCharacter::StartWithKindWeapon()
{
	if (HasAuthority()) //是否有主控权， 则在服务器上   独立服务器有主动权，虽然不是服务器，但同样返回TRUE
	{
		PurchaseWeapon(static_cast<EWeaponType>(UKismetMathLibrary::RandomInteger64InRange(0,static_cast<int8>(EWeaponType::EEND)-1)));
		//PurchaseWeapon(TestStartWeapon);
	
	}
}

void AFpsTeachBaseCharacter::PurchaseWeapon(EWeaponType WeaponType)
{
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Owner = this;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	switch (WeaponType)
	{
		case EWeaponType::Ak47:
		{
			//动态拿到AK47Server 类
			UClass* BlueprintVar = StaticLoadClass(AWeaponBaseServer::StaticClass(), nullptr,
			                                       TEXT(
				                                       "Blueprint'/Game/Blueprint/Weapon/AK47/ServerBP_AK47.ServerBP_AK47_C'"));
			//记得加上_C!!!
			AWeaponBaseServer* ServerWeapon = GetWorld()->SpawnActor<AWeaponBaseServer>(
				BlueprintVar, GetActorTransform(), SpawnInfo);
			ServerWeapon->EquipWeapon();
			//ActiveWeapon = EWeaponType::MP7;
			EquipPrimary(ServerWeapon);
		}
		break;
		case EWeaponType::MP7:
		{
			//动态拿到AK47Server 类
			UClass* BlueprintVar = StaticLoadClass(AWeaponBaseServer::StaticClass(), nullptr,
												   TEXT(
													   "Blueprint'/Game/Blueprint/Weapon/MP7/SeverBP_MP7.SeverBP_MP7_C'"));
			//记得加上_C!!!
			AWeaponBaseServer* ServerWeapon = GetWorld()->SpawnActor<AWeaponBaseServer>(
				BlueprintVar, GetActorTransform(), SpawnInfo);
			ServerWeapon->EquipWeapon();
			//ActiveWeapon = EWeaponType::MP7;
			EquipPrimary(ServerWeapon); 
		}
		break;
		case EWeaponType::M4A1:
		{
			//动态拿到Server 类
			UClass* BlueprintVar = StaticLoadClass(AWeaponBaseServer::StaticClass(), nullptr,
												   TEXT(
													   "Blueprint'/Game/Blueprint/Weapon/M4A1/ServerBP_M4A1.ServerBP_M4A1_C'"));
			//记得加上_C!!!
			AWeaponBaseServer* ServerWeapon = GetWorld()->SpawnActor<AWeaponBaseServer>(
				BlueprintVar, GetActorTransform(), SpawnInfo);
			ServerWeapon->EquipWeapon();
			//ActiveWeapon = EWeaponType::M4A1;
			EquipPrimary(ServerWeapon);
		}
		break;
		case EWeaponType::DesertEagle:
		{
			//动态拿到AServer 类  
			UClass* BlueprintVar = StaticLoadClass(AWeaponBaseServer::StaticClass(), nullptr,
												   TEXT(
													   "Blueprint'/Game/Blueprint/Weapon/DesertEagle/ServerBP_DesertEagle.ServerBP_DesertEagle_C'"));
			//记得加上_C!!!
			AWeaponBaseServer* ServerWeapon = GetWorld()->SpawnActor<AWeaponBaseServer>(
				BlueprintVar, GetActorTransform(), SpawnInfo);
			ServerWeapon->EquipWeapon();
			//ActiveWeapon = EWeaponType::M4A1;
			EquipSecondary(ServerWeapon);
		}
		break;
	default:
		{
		}
	}
}

AWeaponBaseClient* AFpsTeachBaseCharacter::GetCurrentClientFPArmsWeaponActor()
{
	switch (ActiveWeapon)
	{
		case EWeaponType::Ak47:
		{
			return ClientPrimaryWeapon;
		}
		case EWeaponType::M4A1:
		{
			return ClientPrimaryWeapon;
		}
		case EWeaponType::MP7:
		{
			return ClientPrimaryWeapon;
		}
		case EWeaponType::DesertEagle:
		{
			return ClientSecondaryWeapon;
		}
	}
	return nullptr;
}

AWeaponBaseServer* AFpsTeachBaseCharacter::GetCurrentServerTpBodysArmsWeaponActor()
{
	switch (ActiveWeapon)
	{
	case EWeaponType::Ak47:
		{
			return ServerPrimaryWeapon;
		}
		
	case EWeaponType::M4A1:
		{
			return ServerPrimaryWeapon;
		}
	case EWeaponType::MP7:
		{
			return ServerPrimaryWeapon;
		}
	case EWeaponType::DesertEagle:
		{
			return ServerSecondaryWeapon;
		}
	}
	return nullptr;
}
#pragma endregion

#pragma region  fire
void AFpsTeachBaseCharacter::ClientFire_Implementation()
{
	//得到枪, 枪体播放开火动画
	AWeaponBaseClient* CurrentClientWeapon = GetCurrentClientFPArmsWeaponActor();
	if (CurrentClientWeapon)
	{
		//播放枪支动画
		GetCurrentClientFPArmsWeaponActor()->PlayShootAnimation();

		//手臂播放动画MONTAGE
		UAnimMontage* ClientArmsFireMontage = CurrentClientWeapon->ClientArmsFireAnimMontage;
		ClientArmsAnimBP->Montage_SetPlayRate(ClientArmsFireMontage, 1);
		ClientArmsAnimBP->Montage_Play(ClientArmsFireMontage);

		//播放射击声音 和 火花
		CurrentClientWeapon->DisplayWeaponEffect();

		//播放窗口抖动
		AMultiFPSPlayerController* MultiFPSPlayerController = Cast<AMultiFPSPlayerController>(GetController());
		if (MultiFPSPlayerController)
		{
			FPSPlayerController->PlayerCameraShake(CurrentClientWeapon->CameraShakeClass);
			//U十字线抖动
			FPSPlayerController->DoCrosshairRecoil();
		}
	}
}

void AFpsTeachBaseCharacter::ResetRecoil()
{
	NewVerticalRecoilAmount = 0;
	OldVerticalRecoilAmount = 0;
	VerticalRecoilAmount = 0;
	RecoilXCoordPerShoot = 0;
}

void AFpsTeachBaseCharacter::DelayPlayArmReloadCallBack()
{
	AWeaponBaseServer* CurrentClientWeapon = GetCurrentServerTpBodysArmsWeaponActor();
	if(CurrentClientWeapon)
	{
		int32 GunCurrentAmmo = CurrentClientWeapon->GunCurrentAmmo;//  枪体还剩多少子弹
		int32 ClipCurrentAmmo = CurrentClientWeapon->ClipCurrentAmmo;//弹夹还有多少子弹
		int32 const MaxClipAmmo = CurrentClientWeapon->MaxClipAmmo; //弹夹容量\
		//是否装填全部子弹
		bReloading = false;
		if (MaxClipAmmo-ClipCurrentAmmo >= GunCurrentAmmo)//无剩余子弹
			{
			ClipCurrentAmmo += GunCurrentAmmo;
			GunCurrentAmmo = 0;
			}
		else  //有剩余子弹
			{
			GunCurrentAmmo -= MaxClipAmmo - ClipCurrentAmmo;
			ClipCurrentAmmo = MaxClipAmmo;
			}

		CurrentClientWeapon->GunCurrentAmmo = GunCurrentAmmo;
		CurrentClientWeapon->ClipCurrentAmmo = ClipCurrentAmmo ;

		ClientUpdateAmmo(ClipCurrentAmmo,GunCurrentAmmo);
	}
	
}

void AFpsTeachBaseCharacter::DelaySpreadWeaponShootCallBack()
{
	PistolSpreadMax = PistolSpreadMin = 0;
}

void AFpsTeachBaseCharacter::Automacitfire()
{
	if (ServerPrimaryWeapon->ClipCurrentAmmo > 0) //这个是客户端里面的ServerPrimayWeapon ，暂时不会保持更新
	{
		//服务端	（播放设计声音(done)，枪口闪光(done)，减少弹药，射线检测（三种 步枪 手枪 狙击枪），伤害应用，弹孔生成）
		//射线检测参数： 相机的状态， 是否在移动
		if(UKismetMathLibrary::VSize(GetVelocity())>0.1)
		{
			ServerFireRifleWeapon(PlayerCamera->GetComponentLocation(), PlayerCamera->GetComponentRotation(),true);
		}
		else
		{
			ServerFireRifleWeapon(PlayerCamera->GetComponentLocation(), PlayerCamera->GetComponentRotation(),false);
		}

		//客户端（枪体播放动画（done)，手臂播放动画(done)，播放设计声音(done)，屏幕抖动(done)，后坐力，枪口闪光(done)）
		//客户端十字线瞄准UI(done), 初始化(done) ， 点击时扩散(done)）
		ClientFire();
		ClientRecoil();
	}
	else
	{
		StopFirePrimary();
	}
}

void AFpsTeachBaseCharacter::FirePrimary()
{
	//判断子弹是否足够
	if (ServerPrimaryWeapon->ClipCurrentAmmo > 0 && !bReloading) //这个是客户端里面的ServerPrimayWeapon ，暂时不会保持更新
	{
		//服务端	（播放设计声音(done)，枪口闪光(done)，减少弹药，射线检测（三种 步枪 手枪 狙击枪），伤害应用，弹孔生成）
		//射线检测参数： 相机的状态， 是否在移动
		if(UKismetMathLibrary::VSize(GetVelocity())>0.1)
		{
			ServerFireRifleWeapon(PlayerCamera->GetComponentLocation(), PlayerCamera->GetComponentRotation(),true);
		}
		else
		{
			ServerFireRifleWeapon(PlayerCamera->GetComponentLocation(), PlayerCamera->GetComponentRotation(),false);
		}
		//客户端（枪体播放动画（done)，手臂播放动画(done)，播放设计声音(done)，屏幕抖动(done)，后坐力，枪口闪光(done)）
		//客户端十字线瞄准UI(done), 初始化(done) ， 点击时扩散(done)）
		ClientFire();
		ClientRecoil();
		// 后坐力   摄像机使用了角色控制器的旋转  RPC 只在客户端选装

		if (ServerPrimaryWeapon->bIsAuto)
		{
			//开启计时器，每隔固定时间重新射击
			//连击系统
			// 计时器句柄，方法 ,时间， 循环， 延迟(不需要)
			GetWorldTimerManager().SetTimer(AutomaticFireTimerHandle, this,
			                                &AFpsTeachBaseCharacter::Automacitfire,
			                                ServerPrimaryWeapon->AutomaticFireRate, true);
		}
	}
	else
	{
	}
}

void AFpsTeachBaseCharacter::StopFirePrimary()
{
	//UE_LOG(LogTemp,Warning,TEXT("AFpsTeachBaseCharacter::StopFirePrimary()"));

	//关闭计时器，步枪射击，逻辑重置
	GetWorldTimerManager().ClearTimer(AutomaticFireTimerHandle);
	//重置后坐力相关变量
	ResetRecoil();
	//stop !!:
	ServerStopFiring();
	// 连击系统句柄归零
}

//射线检测  需要考虑很多
void AFpsTeachBaseCharacter::RifleLineTrace(FVector CameraLocation, FRotator CameraRotation, bool IsMoving)
{
	//计算LOCATION
	FVector EndLocation;
	FVector CameraForwardVector = UKismetMathLibrary::GetForwardVector(CameraRotation);
	TArray<AActor*> IgnoreArray;
	IgnoreArray.Add(this);
	FHitResult HitResult;
	if (ServerPrimaryWeapon)
	{
		if (IsMoving)
		{
			//x y z 加上 偏移量  随机
			FVector Vector = CameraForwardVector * ServerPrimaryWeapon->BulletDistancs;
			float RandomX =UKismetMathLibrary::RandomFloatInRange(-ServerPrimaryWeapon->MovingFireRandomRange,ServerPrimaryWeapon->MovingFireRandomRange);
			float RandomY =UKismetMathLibrary::RandomFloatInRange(-ServerPrimaryWeapon->MovingFireRandomRange,ServerPrimaryWeapon->MovingFireRandomRange);
			float RandomZ =UKismetMathLibrary::RandomFloatInRange(-ServerPrimaryWeapon->MovingFireRandomRange,ServerPrimaryWeapon->MovingFireRandomRange);
			EndLocation = FVector(Vector.X+RandomX,Vector.Y+RandomY,Vector.Z+RandomZ);
		}
		else
		{
			EndLocation = CameraLocation + CameraForwardVector * ServerPrimaryWeapon->BulletDistancs;
		}
		//起始位置，终止位置，碰撞类型，是否复杂碰撞,忽略类型， 画Debug线,结果输出，是否忽略自己，射线颜色，击中颜色
		bool HitSuccess = UKismetSystemLibrary::LineTraceSingle(GetWorld(), CameraLocation, EndLocation,
		                                                        ETraceTypeQuery::TraceTypeQuery1,
		                                                        false, IgnoreArray, EDrawDebugTrace::None,
		                                                        HitResult, true, FLinearColor::Red, FLinearColor::Green,
		                                                        3.f);
		if (HitSuccess)
		{
			AFpsTeachBaseCharacter* FPSCharacter = Cast<AFpsTeachBaseCharacter>(HitResult.Actor);
			if (FPSCharacter)
			{
				//打到玩家 应用伤害

				DamagePlayer(HitResult.PhysMaterial.Get(), HitResult.Actor.Get(), CameraLocation, HitResult);
			}
			else //墙 生成单孔  广播形式
			{
				FRotator XRotater = UKismetMathLibrary::MakeRotFromX(HitResult.Normal); //!!!!!!!得到旋转，应用此旋转后，得到Normal方向
				MultiSpawnBulletDecal(HitResult.Location, XRotater);
			}
		}
	}
}

void AFpsTeachBaseCharacter::FireSecondary()
{
	//判断子弹是否足够
	if (ServerSecondaryWeapon->ClipCurrentAmmo > 0 && !bReloading) //这个是客户端里面的ServerPrimayWeapon ，暂时不会保持更新
		{
		//服务端	（播放设计声音(done)，枪口闪光(done)，减少弹药，射线检测（三种 步枪 手枪 狙击枪），伤害应用，弹孔生成）
		//射线检测参数： 相机的状态， 是否在移动
		if(UKismetMathLibrary::VSize(GetVelocity())>0.1)
		{
			ServerFirePistolWeapon(PlayerCamera->GetComponentLocation(), PlayerCamera->GetComponentRotation(),true);
		}
		else
		{
			ServerFirePistolWeapon(PlayerCamera->GetComponentLocation(), PlayerCamera->GetComponentRotation(),false);
		}

		
		//客户端（枪体播放动画（done)，手臂播放动画(done)，播放设计声音(done)，屏幕抖动(done)，后坐力，枪口闪光(done)）
		//客户端十字线瞄准UI(done), 初始化(done) ， 点击时扩散(done)）
		ClientFire();
		
		// 后坐力   摄像机使用了角色控制器的旋转  RPC 只在客户端选装
		}
	else
	{
	}
}

void AFpsTeachBaseCharacter::StopFireSecondary()
{
	ServerStopFiring();
}

void AFpsTeachBaseCharacter::PistolLineTrace(FVector CameraLocation, FRotator CameraRotation, bool IsMoving)
{
	//计算LOCATION
	FVector EndLocation;
	FVector CameraForwardVector;
	TArray<AActor*> IgnoreArray;
	IgnoreArray.Add(this);
	FHitResult HitResult;
	if (ServerSecondaryWeapon)
	{
		if (IsMoving)
		{

			FRotator Rotator;
			Rotator.Roll = CameraRotation.Roll;
			Rotator.Pitch = CameraRotation.Pitch + UKismetMathLibrary::RandomFloatInRange(PistolSpreadMin,PistolSpreadMax);
			Rotator.Yaw = CameraRotation.Yaw + UKismetMathLibrary::RandomFloatInRange(PistolSpreadMin,PistolSpreadMax);
			CameraForwardVector = UKismetMathLibrary::GetForwardVector(Rotator);
			
			//x y z 加上 偏移量  随机
			FVector Vector = CameraForwardVector * ServerSecondaryWeapon->BulletDistancs;
			float RandomX =UKismetMathLibrary::RandomFloatInRange(-ServerSecondaryWeapon->MovingFireRandomRange,ServerSecondaryWeapon->MovingFireRandomRange);
			float RandomY =UKismetMathLibrary::RandomFloatInRange(-ServerSecondaryWeapon->MovingFireRandomRange,ServerSecondaryWeapon->MovingFireRandomRange);
			float RandomZ =UKismetMathLibrary::RandomFloatInRange(-ServerSecondaryWeapon->MovingFireRandomRange,ServerSecondaryWeapon->MovingFireRandomRange);
			EndLocation = FVector(Vector.X+RandomX,Vector.Y+RandomY,Vector.Z+RandomZ);
		}
		else
		{
			//旋转 加一个随机旋转偏移 根据连续射击的快慢

			FRotator Rotator;
			Rotator.Roll = CameraRotation.Roll;
			Rotator.Pitch = CameraRotation.Pitch + UKismetMathLibrary::RandomFloatInRange(PistolSpreadMin,PistolSpreadMax);
			Rotator.Yaw = CameraRotation.Yaw + UKismetMathLibrary::RandomFloatInRange(PistolSpreadMin,PistolSpreadMax);
			CameraForwardVector = UKismetMathLibrary::GetForwardVector(Rotator);
			
			EndLocation = CameraLocation + CameraForwardVector * ServerSecondaryWeapon->BulletDistancs;
		}
		//起始位置，终止位置，碰撞类型，是否复杂碰撞,忽略类型， 画Debug线,结果输出，是否忽略自己，射线颜色，击中颜色
		bool HitSuccess = UKismetSystemLibrary::LineTraceSingle(GetWorld(), CameraLocation, EndLocation,
																ETraceTypeQuery::TraceTypeQuery1,
																false, IgnoreArray, EDrawDebugTrace::None,
																HitResult, true, FLinearColor::Red, FLinearColor::Green,
																3.f);

		PistolSpreadMax += ServerSecondaryWeapon->SpreadWeaponMinMaxIndex;
		PistolSpreadMin -= ServerSecondaryWeapon->SpreadWeaponMinMaxIndex;
		if (HitSuccess)
		{
			AFpsTeachBaseCharacter* FPSCharacter = Cast<AFpsTeachBaseCharacter>(HitResult.Actor);
			if (FPSCharacter)
			{
				//打到玩家 应用伤害

				DamagePlayer(HitResult.PhysMaterial.Get(), HitResult.Actor.Get(), CameraLocation, HitResult);
			}
			else
			{
				FRotator XRotater = UKismetMathLibrary::MakeRotFromX(HitResult.Normal); //!!!!!!!得到旋转，应用此旋转后，得到Normal方向
				MultiSpawnBulletDecal(HitResult.Location, XRotater);
			}
			//墙 生成单孔  广播形式

		}
	}
}

//受到伤害 判断

void AFpsTeachBaseCharacter::DamagePlayer(UPhysicalMaterial* PhysicalMaterial, AActor* DamagedActor,
                                          FVector const& HitFromDirection, FHitResult const& HitInfo)
{
	float idx = 0;
	//身体5个位置应用不同伤害  设置物理表面， 设置物理材质  设置MESH 物理材质 --碰撞   hitresult里面有物理材质的
	switch (PhysicalMaterial->SurfaceType)
	{
	case EPhysicalSurface::SurfaceType1: // 头
		{
			idx = 4;
		}
		break;
	case EPhysicalSurface::SurfaceType2: //身体
		{
			idx = 1;
		}
		break;
	case EPhysicalSurface::SurfaceType3: //胳膊
		{
			idx = 0.8;
		}
		break;
	case EPhysicalSurface::SurfaceType4: //腿
		{
			idx = 0.7;
		}
		break;
	}
	AWeaponBaseServer* CurrentWeapon = GetCurrentServerTpBodysArmsWeaponActor();
	if (CurrentWeapon)
	{
		// 被击ACTOR,伤害值，攻击者位置,hitresult射线检测结果,controller,造成伤害物体
		//controller!要调用，不能直接拿~~~~~~~~~~~~~~~~？？？？
		UGameplayStatics::ApplyPointDamage(DamagedActor, CurrentWeapon->BaseDamge * idx, HitFromDirection,
		                                   HitInfo, GetController(),
		                                   this, UDamageType::StaticClass()); //攻击别人，发通知 观察者模式
		// OnTakePointDamage.Add();//受到攻击， 调用此方法 begin添加
	}
}

void AFpsTeachBaseCharacter::OnHit(AActor* DamagedActor, float Damage, AController* InstigatedBy,
                                   FVector HitLocation, UPrimitiveComponent* FHitComponent, FName BoneName,
                                   FVector ShotFromDirection,
                                   const UDamageType* DamageType, AActor* DamageCauser)
{
	Health -= Damage; //服务器上的方法
	//UKismetSystemLibrary::PrintString(this,FString::Printf(TEXT("PlayerName %s Health: = %f"),*GetName(),Health));

	//客户端RPC, 调用客户端playerController （蓝图）  ，  实现playerUI血量减少的接口。
	ClientUpdateHealUI(Health);

	//死亡
	if (Health <= 0)
	{
		DeathMatchDeath(DamageCauser);
		//dead;
	}
}

void AFpsTeachBaseCharacter::DeathMatchDeath(AActor* DamageActor)
{
	AWeaponBaseClient* CurrentClientWeapon = GetCurrentClientFPArmsWeaponActor();
	AWeaponBaseServer* CurrentServerWeapon = GetCurrentServerTpBodysArmsWeaponActor();

	if(CurrentClientWeapon)// 只在服务器上执行
	{
		CurrentClientWeapon->Destroy();
	}
	if(CurrentServerWeapon)  //客户端和服务器同步
	{
		CurrentServerWeapon->Destroy();
	}
	ClientDeathMatchDeath();
	
	AMultiFPSPlayerController*  MultiFPSPlayerController = Cast<AMultiFPSPlayerController>(GetController());
	if (MultiFPSPlayerController)
	{
		MultiFPSPlayerController->DeathMathDeath(DamageActor);
	}
}

#pragma endregion
