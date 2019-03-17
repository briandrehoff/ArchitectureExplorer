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
#include "Kismet/GameplayStatics.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"

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

	TeleportPath = CreateDefaultSubobject<USplineComponent>("TeleportPath");
	TeleportPath->SetupAttachment(RightController);

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

bool AVRCharacter::DidFindTeleportDestination(TArray<FVector> &OutPath, FVector &OutLocation)
{
	FVector Start = RightController->GetComponentLocation();
	FVector Look = RightController->GetForwardVector();

	FPredictProjectilePathParams Params(
		TeleportProjectileRadius,
		Start,
		Look * TeleportProjectileSpeed,
		TeleportSimulationTime,
		ECollisionChannel::ECC_Visibility,
		this
	);

	FPredictProjectilePathResult Result;
	bool DidHit = UGameplayStatics::PredictProjectilePath(GetWorld(), Params, Result);

	if (!DidHit) return false;

	for (FPredictProjectilePathPointData PointData : Result.PathData)
	{
		OutPath.Add(PointData.Location);
	}

	FNavLocation NavigationLocation;
	UNavigationSystemV1* NavigationSystem = Cast<UNavigationSystemV1>(GetWorld()->GetNavigationSystem());
	bool OnNavMesh;
	if (NavigationSystem != nullptr)
	{
		OnNavMesh = NavigationSystem->ProjectPointToNavigation(Result.HitResult.Location, NavigationLocation, TeleportProjectionExtent);
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
	TArray<FVector> Path;
	FVector Destination;
	if (DidFindTeleportDestination(Path, Destination))
	{
		DestinationMarker->SetVisibility(true);
		DestinationMarker->SetWorldLocation(Destination);
		DrawTeleportPath(Path);
	}
	else
	{
		DestinationMarker->SetVisibility(false);
		TArray<FVector> EmptyPath;
		DrawTeleportPath(EmptyPath);
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

void AVRCharacter::AddMeshToPool()
{
	USplineMeshComponent* SplineMesh = NewObject<USplineMeshComponent>(this);
	SplineMesh->SetMobility(EComponentMobility::Movable);
	SplineMesh->AttachToComponent(TeleportPath, FAttachmentTransformRules::KeepRelativeTransform);
	SplineMesh->SetStaticMesh(TeleportArchMesh);
	SplineMesh->SetMaterial(0, TeleportArchMaterial);
	SplineMesh->RegisterComponent();
	TeleportPathMeshPool.Add(SplineMesh);
}

void AVRCharacter::DrawTeleportPath(const TArray<FVector> &Path)
{
	UpdateSpline(Path);
	for (USplineMeshComponent* SplineMesh : TeleportPathMeshPool)
	{
		SplineMesh->SetVisibility(false);
	}
	int32 SegmentNum = Path.Num() - 1;
	for (int32 i = 0; i < SegmentNum; ++i)
	{
		if (TeleportPathMeshPool.Num() <= i)
		{
			AddMeshToPool();
		}

		USplineMeshComponent* SplineMesh = TeleportPathMeshPool[i];
		SplineMesh->SetVisibility(true);

		FVector StartPosition, StartTangent, EndPosition, EndTangent;
		TeleportPath->GetLocalLocationAndTangentAtSplinePoint(i, StartPosition, StartTangent);
		TeleportPath->GetLocalLocationAndTangentAtSplinePoint(i + 1, EndPosition, EndTangent);
		SplineMesh->SetStartAndEnd(
			StartPosition,
			StartTangent,
			EndPosition,
			EndTangent
		);
	}

}

void AVRCharacter::UpdateSpline(const TArray<FVector> &Path)
{
	TeleportPath->ClearSplinePoints(false);
	for (int32 i = 0; i < Path.Num(); ++i)
	{
		FVector LocalPosition = TeleportPath->GetComponentTransform().InverseTransformPosition(Path[i]);
		FSplinePoint Point = FSplinePoint(i, LocalPosition, ESplinePointType::Curve);
		TeleportPath->AddPoint(Point, false);
	}

	TeleportPath->UpdateSpline();
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