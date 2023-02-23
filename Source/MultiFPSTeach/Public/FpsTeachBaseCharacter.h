// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MultiFPSPlayerController.h"
#include "WeaponBaseClient.h"
#include "WeaponBaseServer.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "FpsTeachBaseCharacter.generated.h"

UCLASS()
class MULTIFPSTEACH_API AFpsTeachBaseCharacter : public ACharacter
{
	GENERATED_BODY()
	
public:
	AFpsTeachBaseCharacter();
	UFUNCTION()
	void DelayBeginplayCallBack();
	
protected:
	virtual void BeginPlay() override;
private:	
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma region Component
private:
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category=Character,meta=(AllowPrivateAccess = "true")) //meta:private 可以被反射得到
	UCameraComponent* PlayerCamera;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category=Character,meta=(AllowPrivateAccess = "true")) //meta:private 可以被反射得到
	USkeletalMeshComponent* FPArmsMesh;
	UPROPERTY(BlueprintReadOnly,Category=Character,meta=(AllowPrivateAccess = "true")) //meta:private 可以被反射得到
	UAnimInstance* ClientArmsAnimBP;
	UPROPERTY(BlueprintReadOnly,Category=Character,meta=(AllowPrivateAccess = "true")) //meta:private 可以被反射得到
	UAnimInstance* ServerBodyAnmiBP;
	UPROPERTY(BlueprintReadOnly,meta=(AllowPrivateAccess = "true")) //meta:private 可以被反射得到
	AMultiFPSPlayerController* FPSPlayerController;
	float Health;



#pragma endregion 
//RPC网络同步通信  
#pragma region NetWorking
public:
	UFUNCTION(Server,Reliable,WithValidation)
	void ServerLowSpeedWalkAction();
	void ServerLowSpeedWalkAction_Implementation();
	bool ServerLowSpeedWalkAction_Validate();
	
	UFUNCTION(Server,Reliable,WithValidation)
	void ServerNormalSpeedWalkAction();
	void ServerNormalSpeedWalkAction_Implementation();
	bool ServerNormalSpeedWalkAction_Validate();

	//步枪武器射击
	UFUNCTION(Server,Reliable,WithValidation)
	void ServerFireRifleWeapon(FVector CameraLocation, FRotator CameraRotation,bool IsMoving);
	void ServerFireRifleWeapon_Implementation(FVector CameraLocation, FRotator CameraRotation,bool IsMoving);
	bool ServerFireRifleWeapon_Validate(FVector CameraLocation, FRotator CameraRotation,bool IsMoving);

	//步枪武器射击
	UFUNCTION(Server,Reliable,WithValidation)
	void ServerFirePistolWeapon(FVector CameraLocation, FRotator CameraRotation,bool IsMoving);
	void ServerFirePistolWeapon_Implementation(FVector CameraLocation, FRotator CameraRotation,bool IsMoving);
	bool ServerFirePistolWeapon_Validate(FVector CameraLocation, FRotator CameraRotation,bool IsMoving);
	
	// firing = false
	UFUNCTION(Server,Reliable,WithValidation)
	void ServerStopFiring();
	void ServerStopFiring_Implementation();
	bool ServerStopFiring_Validate();
	//多播 开火时手臂动画
	UFUNCTION(NetMulticast,Reliable,WithValidation)
	void MultiShooting();
	void MultiShooting_Implementation();
	bool MultiShooting_Validate();
	//多播 换弹动画
	UFUNCTION(NetMulticast,Reliable,WithValidation)
	void MultiSReloadAnimation();
	void MultiSReloadAnimation_Implementation();
	bool MultiSReloadAnimation_Validate();
	
	//多播 生成单孔  传参：位置参数
	UFUNCTION(NetMulticast,Reliable,WithValidation)
	void MultiSpawnBulletDecal(FVector Location,FRotator Rotation);
	void MultiSpawnBulletDecal_Implementation(FVector Location,FRotator Rotation);
	bool MultiSpawnBulletDecal_Validate(FVector Location,FRotator Rotation);
	//client RPC
	UFUNCTION(Client,Reliable)
	void ClientEquipFPArmsPrimary();
	UFUNCTION(Client,Reliable)
	void ClientEquipFPArmsSecondary();
	
	UFUNCTION(Client,Reliable)
	void ClientFire();
	UFUNCTION(Client,Reliable)
	void ClientUpdateAmmo(int32 ClipCurrentAmmo,int32 GunCurrentAmmo);
	UFUNCTION(Client,Reliable)
	void ClientUpdateHealUI(float NewHealth);
	//客户端旋转摄像头  ？ 服务器不转，？？
	UFUNCTION(Client,Reliable)
	void ClientRecoil( );
	// 客户端Reload
	UFUNCTION(Client,Reliable)
	void ClientReload();
	// 客户端死亡函数
	
	UFUNCTION(Client,Reliable)
	void ClientDeathMatchDeath();
	

	// ServerRpc  : Reload
	UFUNCTION(Server,Reliable,WithValidation)
	void ServerReloadPrimary();
	void ServerReloadPrimary_Implementation();
	bool SServerReloadPrimary_Validate();

	// ServerRpc  : Reload
	UFUNCTION(Server,Reliable,WithValidation)
	void ServerReloadSeconday();
	void ServerReloadSeconday_Implementation();
	bool ServerReloadSeconday_Validate();

#pragma  endregion
#pragma region InputEvent
	void MoveRight(float AxisValue);
	void MoveForward(float AxisValue);
	void JumpAction();
	void StopJumpAction();
	void LowSpeedWalkAction();
	void NormalSpeedWalkAction();

	void InputFirePressed();
	void InputFireReleased();
	void InputReload();
#pragma endregion

#pragma region Weapon
public:
	void EquipPrimary(AWeaponBaseServer* WeaponBaseServer);
	void EquipSecondary(AWeaponBaseServer* WeaponBaseServer);
	void StartWithKindWeapon();
	void PurchaseWeapon(EWeaponType WeaponType);
	AWeaponBaseClient* GetCurrentClientFPArmsWeaponActor();
	AWeaponBaseServer* GetCurrentServerTpBodysArmsWeaponActor();
	UFUNCTION(BlueprintImplementableEvent)
	void UpdateFPArmsBlendPose(int NewIdx);
	
private:
	UPROPERTY(meta=(AllowPrivateAccess = "true"),Replicated)
	EWeaponType ActiveWeapon;
	UPROPERTY(meta=(AllowPrivateAccess = "true"))
	AWeaponBaseServer* ServerPrimaryWeapon;
	UPROPERTY(meta=(AllowPrivateAccess = "true"))
	AWeaponBaseClient* ClientPrimaryWeapon;

	UPROPERTY(meta=(AllowPrivateAccess = "true"))
	AWeaponBaseServer* ServerSecondaryWeapon;
	UPROPERTY(meta=(AllowPrivateAccess = "true"))
	AWeaponBaseClient* ClientSecondaryWeapon;

#pragma endregion


#pragma region  fire
public:

	FTimerHandle AutomaticFireTimerHandle;
	//后坐力
	float NewVerticalRecoilAmount;
	float OldVerticalRecoilAmount;
	float VerticalRecoilAmount;
	float RecoilXCoordPerShoot;
	void ResetRecoil();
	float NewHorizontalRecoilAmount;
	float OldHorizontalRecoilAmount;
	float HorizontalRecoilAmount;
	//手枪后坐力
	float PistolSpreadMin=0;
	float PistolSpreadMax=0;
	UPROPERTY(Replicated)
	bool bIsFiring;
	UPROPERTY(Replicated)
	bool bReloading;

	UFUNCTION()
	void DelayPlayArmReloadCallBack();

	UFUNCTION()
	void DelaySpreadWeaponShootCallBack();
	
	//步枪射击
	void Automacitfire();
	void FirePrimary();
	void StopFirePrimary();
	//射线检测  需要考虑很多 
	void RifleLineTrace(FVector CameraLocation, FRotator CameraRotation,bool IsMoving);
	
	//狙击枪
	// void FirePrimary();
	// void StopFirePrimary();
	
	//手枪
	void FireSecondary();
	void StopFireSecondary();
	void PistolLineTrace(FVector CameraLocation, FRotator CameraRotation,bool IsMoving);
	

	//伤害玩家
	
	// 被击ACTOR,伤害值，攻击者位置,hitresult射线检测结果,controller,造成伤害物体
	void DamagePlayer(UPhysicalMaterial* PhysicalMaterial, AActor* DamagedActor,FVector const& HitFromDirection, FHitResult const& HitInfo);

	//回调函数！
	UFUNCTION()
	void OnHit(AActor* DamagedActor, float Damage, class AController* InstigatedBy, FVector HitLocation,
		class UPrimitiveComponent* FHitComponent, FName BoneName, FVector ShotFromDirection,
		const class UDamageType* DamageType, AActor* DamageCauser );

	
	//character  死亡竞赛中死亡
	void DeathMatchDeath(AActor* DamageActor);
	//Reload
#pragma endregion


	UPROPERTY(EditAnywhere)
	EWeaponType TestStartWeapon;
};


