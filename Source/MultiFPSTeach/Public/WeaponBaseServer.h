// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WeaponBaseClient.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Actor.h"
#include "WeaponBaseServer.generated.h"


UENUM()
enum class EWeaponType : uint8
{
	Ak47 UMETA(DisplayName = "AK47"),
	// DesertEagle UMETA(DisplayName = "DesertEagle"),
	MP7 UMETA(DisplayName = "MP7"),
	M4A1 UMETA(DisplayName = "M4A1"),
	DesertEagle UMETA(DisplayName="DesertEagle"),
	EEND    //结束拿到整体个数
};

UCLASS()
class MULTIFPSTEACH_API AWeaponBaseServer : public AActor
{
	GENERATED_BODY()
	
public:	
	AWeaponBaseServer();
protected:
	virtual void BeginPlay() override;
public:	
	virtual void Tick(float DeltaTime) override;

public:
	UFUNCTION()
	void OnOtherBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult & SweepResult);

	UFUNCTION()
	void EquipWeapon();

	//多播   改参1  
	UFUNCTION(NetMulticast,Reliable,WithValidation)
	void MultiShootingEffect();
	void MultiShootingEffect_Implementation();
	bool MultiShootingEffect_Validate();

	// 枪口声音和闪光
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	USoundBase* FireSound;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	UParticleSystem* MuzzleFlash;

	//子弹  Replicated  服务器改变了 那么客户端也要改变  实现方法去声明需要！ get life timer eplicatedprops
	UPROPERTY(EditAnywhere,Replicated)
	int32 GunCurrentAmmo;//  枪体还剩多少子弹
	UPROPERTY(EditAnywhere,Replicated)  
	int32 ClipCurrentAmmo;//弹夹还有多少子弹
	UPROPERTY(EditAnywhere,Replicated)
	int32 MaxClipAmmo; //弹夹容量
	
public:
	UPROPERTY(EditAnywhere)
	EWeaponType KindOfWeapon;
	UPROPERTY(EditAnywhere)
	USkeletalMeshComponent* WeaponMesh;
	UPROPERTY(EditAnywhere)
	USphereComponent* SphereCollision;
	UPROPERTY(EditAnywhere,BlueprintReadWrite)
	TSubclassOf<AWeaponBaseClient> ClientWeaponBaseBPClass;
	//动画属性  多播射击
	UPROPERTY(EditAnywhere)
	UAnimMontage* ServerTPBodysShootAnimMontage;
	//动画属性  多播换弹
	UPROPERTY(EditAnywhere)
	UAnimMontage* ServerTPBodysReloadAnimMontage;
	
	//射击距离
	UPROPERTY(EditAnywhere)
	float BulletDistancs;
	UPROPERTY(EditAnywhere)
	UMaterialInterface* BulletDecalMaterial;
	//基础伤害
	UPROPERTY(EditAnywhere)
	float BaseDamge;
	//设计间隔
	UPROPERTY(EditAnywhere)
	float AutomaticFireRate;
	//是否是自动步枪
	UPROPERTY(EditAnywhere)
	bool bIsAuto;
	//后坐力
	UPROPERTY(EditAnywhere)
	UCurveFloat* VerticalRecoilCurve;
	UPROPERTY(EditAnywhere)
	UCurveFloat* HorizontalRecoilCurve;
	UPROPERTY(EditAnywhere)
	float MovingFireRandomRange;
	// 手枪单发连续 后坐  时间回复
	UPROPERTY(EditAnywhere,Category="SpreadWeaponData")
	float SpreadWeaponCallBackRate;
	UPROPERTY(EditAnywhere,Category="SpreadWeaponData")
	float SpreadWeaponMinMaxIndex;
	
	
};
