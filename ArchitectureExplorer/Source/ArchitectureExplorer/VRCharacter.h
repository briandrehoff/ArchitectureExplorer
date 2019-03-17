// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "VRCharacter.generated.h"

UCLASS()
class ARCHITECTUREEXPLORER_API AVRCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AVRCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

private:

	bool DidFindTeleportDestination(FVector &OutLocation);
	void UpdateDestinationMarker();

	void MoveForward(float throttle);
	void MoveRight(float throttle);
	void UpdateBlinkers();
	FVector2D GetBlinkerCenter();

	void BeginTeleport();
	void FinishTeleport();

	void StartFade(float FromAlpha, float ToAlpha);

private:

	UPROPERTY(VisibleAnywhere)
	class UCameraComponent* Camera;
	UPROPERTY(EditAnywhere)
	class UMotionControllerComponent* LeftController;
	UPROPERTY(EditAnywhere)
	class UMotionControllerComponent* RightController;
	UPROPERTY(VisibleAnywhere)
	class USceneComponent* VRRoot;
	UPROPERTY(VisibleAnywhere)
	class UStaticMeshComponent* DestinationMarker;
	UPROPERTY(VisibleAnywhere)
	class UPostProcessComponent* PostProcessingComponent;
	UPROPERTY(VisibleAnywhere)
	class UMaterialInstanceDynamic* BlinkerMaterialInstance;

private:

	UPROPERTY(EditAnywhere)
	float MaxTeleportDistance = 1000;
	UPROPERTY(EditAnywhere)
	float TeleportFadeTime = 1;
	UPROPERTY(EditAnywhere)
	FVector TeleportProjectionExtent = FVector(100, 100, 100);
	UPROPERTY(EditAnywhere)
	class UMaterialInterface* BlinkerMaterialBase;
	UPROPERTY(EditAnywhere)
	class UCurveFloat* RadiusVsVelocity;

};