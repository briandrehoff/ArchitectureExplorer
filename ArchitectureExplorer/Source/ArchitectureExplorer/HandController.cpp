// Fill out your copyright notice in the Description page of Project Settings.

#include "HandController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

// Sets default values
AHandController::AHandController()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	MotionController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("MotionController"));
	SetRootComponent(MotionController);
}

// Called when the game starts or when spawned
void AHandController::BeginPlay()
{
	Super::BeginPlay();
	
	OnActorBeginOverlap.AddDynamic(this, &AHandController::ActorBeginOverlap);
	OnActorEndOverlap.AddDynamic(this, &AHandController::ActorEndOverlap);
}

// Called every frame
void AHandController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	MoveCharacterIfClimbing();
}

void AHandController::ActorBeginOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	bool NewCanClimb = IsOverlappingClimbableActor();
	if (!CanClimb && NewCanClimb)
	{
		APawn* Pawn = Cast<APawn>(GetAttachParentActor());
		if (Pawn != nullptr)
		{
			APlayerController* Controller = Cast<APlayerController>(Pawn->GetController());
			if (Controller != nullptr)
			{
				Controller->PlayHapticEffect(HapticEffect, MotionController->GetTrackingSource());
			}
		}
	}

	CanClimb = NewCanClimb;
}

void AHandController::ActorEndOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	CanClimb = IsOverlappingClimbableActor();
}

bool AHandController::IsOverlappingClimbableActor() const
{
	TArray<AActor*> OverlappingActors;
	GetOverlappingActors(OverlappingActors);
	for (AActor* OverlappingActor : OverlappingActors)
	{
		if (OverlappingActor->ActorHasTag("Climbable")) return true;
	}

	return false;
}

void AHandController::MoveCharacterIfClimbing()
{
	if (!IsClimbing) return;

	FVector HandControllerDelta = GetActorLocation() - ClimbingStartLocation;
	GetAttachParentActor()->AddActorWorldOffset(-HandControllerDelta);
}

void AHandController::PairController(AHandController* Controller)
{
	OtherController = Controller;
	OtherController->OtherController = this;
}

void AHandController::Grip()
{
	if (!CanClimb) return;

	if (!IsClimbing)
	{
		OtherController->IsClimbing = false;
		IsClimbing = true;
		ClimbingStartLocation = GetActorLocation();

		ACharacter* Character = Cast<ACharacter>(GetAttachParentActor());
		if (Character != nullptr)
		{
			Character->GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Flying);
		}
	}
}

void AHandController::Release()
{
	if (IsClimbing)
	{
		IsClimbing = false;

		ACharacter* Character = Cast<ACharacter>(GetAttachParentActor());
		if (Character != nullptr)
		{
			Character->GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Falling);
		}
	}
}