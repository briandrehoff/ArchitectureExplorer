// Fill out your copyright notice in the Description page of Project Settings.

#include "VRCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "Components/CapsuleComponent.h"
#include "NavigationSystem.h"
#include "Components/PostProcessComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MotionControllerComponent.h"

// Sets default values
AVRCharacter::AVRCharacter()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	VRRoot = CreateDefaultSubobject<USceneComponent>("VRRoot");
	VRRoot->SetupAttachment(GetRootComponent());

	Camera = CreateDefaultSubobject<UCameraComponent>("Camera");
	Camera->SetupAttachment(VRRoot);

	LeftController = CreateDefaultSubobject<UMotionControllerComponent>("LeftController");
	LeftController->SetupAttachment(VRRoot);
	LeftController->SetTrackingSource(EControllerHand::Left);

	RightController = CreateDefaultSubobject<UMotionControllerComponent>("RightController");
	RightController->SetupAttachment(VRRoot);
	RightController->SetTrackingSource(EControllerHand::Right);

	DestinationMarker = CreateDefaultSubobject<UStaticMeshComponent>("DestinationMarker");
	DestinationMarker->SetupAttachment(GetRootComponent());

	PostProcessingComponent = CreateDefaultSubobject<UPostProcessComponent>("PostProcessingComponent");
	PostProcessingComponent->SetupAttachment(GetRootComponent());
}

// Called when the game starts or when spawned
void AVRCharacter::BeginPlay()
{
	Super::BeginPlay();

	DestinationMarker->SetVisibility(false);

	if (BlinkerMaterialBase != nullptr)
	{
		BlinkerMaterialInstance = UMaterialInstanceDynamic::Create(BlinkerMaterialBase, this);
		PostProcessingComponent->AddOrUpdateBlendable(BlinkerMaterialInstance);
		BlinkerMaterialInstance->SetScalarParameterValue("Radius", 0.2);
	}
	
}

// Called every frame
void AVRCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FVector NewCameraOffset = Camera->GetComponentLocation() - GetActorLocation();
	//NewCameraOffset.Z = 0;
	AddActorWorldOffset(NewCameraOffset);
	VRRoot->AddWorldOffset(-NewCameraOffset);
	UpdateDestinationMarker();
	UpdateBlinkers();
}

bool AVRCharacter::DidFindTeleportDestination(FVector &OutLocation)
{
	FVector Start = RightController->GetComponentLocation();
	FVector Look = RightController->GetForwardVector();
	Look = Look.RotateAngleAxis(30, RightController->GetRightVector());
	FVector End = Start + Look * MaxTeleportDistance;

	FHitResult HitResult;
	bool DidHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECollisionChannel::ECC_Visibility);

	if (!DidHit) return false;

	FNavLocation NavigationLocation;
	UNavigationSystemV1* NavigationSystem = Cast<UNavigationSystemV1>(GetWorld()->GetNavigationSystem());
	bool OnNavMesh;
	if (NavigationSystem != nullptr)
	{
		OnNavMesh = NavigationSystem->ProjectPointToNavigation(HitResult.Location, NavigationLocation, TeleportProjectionExtent);
	}
	else
	{
		OnNavMesh = false;
	}

	if (!OnNavMesh) return false;

	OutLocation = NavigationLocation.Location;
	return true;
}

void AVRCharacter::UpdateDestinationMarker()
{
	FVector Destination;
	if (DidFindTeleportDestination(Destination))
	{
		DestinationMarker->SetVisibility(true);
		DestinationMarker->SetWorldLocation(Destination);
	}
	else
	{
		DestinationMarker->SetVisibility(false);
	}
}

void AVRCharacter::UpdateBlinkers()
{
	if (RadiusVsVelocity == nullptr) return;

	float Speed = GetVelocity().Size();
	float Radius = RadiusVsVelocity->GetFloatValue(Speed);

	FVector2D Center = GetBlinkerCenter();
	BlinkerMaterialInstance->SetScalarParameterValue("Radius", Radius);
	BlinkerMaterialInstance->SetVectorParameterValue("Center", FLinearColor(Center.X, Center.Y, 0));
}

FVector2D AVRCharacter::GetBlinkerCenter()
{
	FVector MovementDirection = GetVelocity().GetSafeNormal();
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (MovementDirection.IsNearlyZero() || PlayerController == nullptr)
	{
		return FVector2D(0.5, 0.5);
	}

	FVector WorldStationaryLocation;
	if (FVector::DotProduct(Camera->GetForwardVector(), MovementDirection) > 0)
	{
		WorldStationaryLocation = Camera->GetComponentLocation() + MovementDirection * 1000;
	}
	else
	{
		WorldStationaryLocation = Camera->GetComponentLocation() - MovementDirection * 1000;
	}

	FVector2D ScreenStationaryLocation;
	PlayerController->ProjectWorldLocationToScreen(WorldStationaryLocation, ScreenStationaryLocation);

	int32 SizeX, SizeY;
	PlayerController->GetViewportSize(SizeX, SizeY);
	ScreenStationaryLocation.X /= SizeX;
	ScreenStationaryLocation.Y /= SizeY;

	return ScreenStationaryLocation;
}

// Called to bind functionality to input
void AVRCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis("Forward", this, &AVRCharacter::MoveForward);
	PlayerInputComponent->BindAxis("Right", this, &AVRCharacter::MoveRight);
	PlayerInputComponent->BindAction("Teleport", IE_Released, this, &AVRCharacter::BeginTeleport);
}

void AVRCharacter::MoveForward(float throttle)
{
	AddMovementInput(throttle * Camera->GetForwardVector());
}

void AVRCharacter::MoveRight(float throttle)
{
	AddMovementInput(throttle * Camera->GetRightVector());
}

void AVRCharacter::BeginTeleport()
{
	StartFade(0, 1);
	FTimerHandle TimerHandle;
	GetWorldTimerManager().SetTimer(TimerHandle, this, &AVRCharacter::FinishTeleport, TeleportFadeTime);
}

void AVRCharacter::FinishTeleport()
{
	FVector Destination = DestinationMarker->GetComponentLocation();
	Destination.Z += GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	SetActorLocation(Destination);
	StartFade(1, 0);
}

void AVRCharacter::StartFade(float FromAlpha, float ToAlpha)
{
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (PlayerController != nullptr)
	{
		PlayerController->PlayerCameraManager->StartCameraFade(FromAlpha, ToAlpha, TeleportFadeTime, FLinearColor::Black, false, true);
	}
}