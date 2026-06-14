// Copyright (C) 2026 Vicente Danzmann. All Rights Reserved.

#include "DanzmannMoverPawnCapsule.h"

#include "Components/CapsuleComponent.h"
#include "DanzmannEnhancedInputComponent.h"
#include "DanzmannInputProfileSourceInterface.h"
#include "DanzmannLogDanzmannMovement.h"
#include "DanzmannMoverComponent.h"
#include "Engine/CollisionProfile.h"

ADanzmannMoverPawnCapsule::ADanzmannMoverPawnCapsule(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// Don't rotate the Pawn when the Controller rotates -- Mover authors Pawn rotation each sim frame from OrientationIntent (set in BuildInputCmd())
	bUseControllerRotationRoll = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;

	// Mover replicates its own state -- disable Actor-level movement replication
	SetReplicatingMovement(false);

	// Set up CapsuleCollider
	CapsuleCollider = CreateDefaultSubobject<UCapsuleComponent>(FName(TEXT("CapsuleCollider")));
	CapsuleCollider->InitCapsuleSize(34.0f, 88.0f);
	CapsuleCollider->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	CapsuleCollider->CanCharacterStepUpOn = ECB_No;
	CapsuleCollider->SetShouldUpdatePhysicsVolume(true);
	CapsuleCollider->SetCanEverAffectNavigation(false);
	CapsuleCollider->bDynamicObstacle = true;
	SetRootComponent(CapsuleCollider);

	// Set up MainSkeletalMesh
	MainSkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(FName(TEXT("MainSkeletalMesh")));
	MainSkeletalMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	MainSkeletalMesh->SetupAttachment(GetRootComponent());

	// Set up MoverComponent
	MoverComponent = CreateDefaultSubobject<UDanzmannMoverComponent>(UDanzmannMoverComponent::ComponentName);
}

void ADanzmannMoverPawnCapsule::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Force animation tick after Mover updates so animations see the latest pose for this frame
	if (IsValid(MoverComponent) && IsValid(MainSkeletalMesh) && MainSkeletalMesh->PrimaryComponentTick.bCanEverTick)
	{
		MainSkeletalMesh->PrimaryComponentTick.AddPrerequisite(MoverComponent, MoverComponent->PrimaryComponentTick);
	}
}

void ADanzmannMoverPawnCapsule::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UDanzmannEnhancedInputComponent* EnhancedInputComponent = Cast<UDanzmannEnhancedInputComponent>(PlayerInputComponent);
	if (!IsValid(EnhancedInputComponent))
	{
		UE_LOG(LogDanzmannMovement, Warning, TEXT("[%hs] InputComponent is not a UDanzmannEnhancedInputComponent. Skipping input setup..."), __FUNCTION__);
		return;
	}

	// This scan is the only path that lands input bindings on first possession. 
	// Runtime PawnData swaps re-apply through UDanzmannPawnDataComponent::ApplyPawnData instead
	TArray<UActorComponent*> Components;
	GetComponents(Components);

	for (UActorComponent* Component : Components)
	{
		if (Component->Implements<UDanzmannInputProfileSourceInterface>())
		{
			const UDanzmannInputProfile* InputProfile = Cast<IDanzmannInputProfileSourceInterface>(Component)->GetInputProfile();
			EnhancedInputComponent->ApplyInputProfile(InputProfile);
			break;
		}
	}
}

void ADanzmannMoverPawnCapsule::ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult)
{
	if (IsValid(MoverComponent))
	{
		MoverComponent->BuildInputCmd(static_cast<float>(SimTimeMs), InputCmdResult);
	}
}
