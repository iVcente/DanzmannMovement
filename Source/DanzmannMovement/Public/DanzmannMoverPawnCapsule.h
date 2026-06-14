// Copyright (C) 2026 Vicente Danzmann. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "MoverSimulationTypes.h"

#include "DanzmannMoverPawnCapsule.generated.h"

class UCapsuleComponent;
class UDanzmannMoverComponent;

/**
 * Pawn that pre-wires Mover Component. Inherit from this when you want a turnkey
 * Mover Pawn -- it ships with a capsule (root), a skeletal mesh, a UDanzmannMoverComponent.
 * ProduceInput_Implementation() forwards to the Mover Component in a single line.
 *
 * Input pipeline split across two lifecycle events:
 *  - Initial possession: SetupPlayerInputComponent scans() this Pawn's Components for the first
 *    IDanzmannInputProfileSourceInterface implementer and applies its profile to the EIC. This is
 *    necessary because the Pawn's InputComponent is only created at SetupPlayerInputComponent time,
 *    after UDanzmannPawnDataComponent's possession-driven re-apply has already fired and silently
 *    no-op'd.
 *  - Runtime Pawn Data swap (already possessed): UDanzmannPawnDataComponent::ApplyPawnData()
 *    re-applies through the EIC directly.
 *
 * If you don't want to inherit from this class, you can replicate the wiring on any APawn:
 *  1. Implement IMoverInputProducerInterface on your Pawn class;
 *  2. Add UDanzmannMoverComponent;
 *  3. Forward ProduceInput_Implementation() to MoverComponent->BuildInputCmd();
 *  4. In SetupPlayerInputComponent(), scan for IDanzmannInputProfileSourceInterface and apply
 *     its profile to the UDanzmannEnhancedInputComponent;
 *  5. Call SetReplicatingMovement(false) in your constructor.
 */
UCLASS(Abstract)
class DANZMANNMOVEMENT_API ADanzmannMoverPawnCapsule : public APawn, public IMoverInputProducerInterface
{
	GENERATED_BODY()

	public:
		ADanzmannMoverPawnCapsule(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

		/**
		 * Get capsule collider.
		 * @return Capsule collider.
		 */
		UFUNCTION(BlueprintPure)
		UCapsuleComponent* GetCapsuleCollider() const
		{
			return CapsuleCollider;
		}

		/**
		 * Get main skeletal mesh.
		 * @return Main skeletal mesh.
		 */
		UFUNCTION(BlueprintPure)
		USkeletalMeshComponent* GetMainSkeletalMesh() const
		{
			return MainSkeletalMesh;
		}

		/**
		 * Get Mover Component.
		 * @return Mover Component.
		 */
		UFUNCTION(BlueprintPure)
		UDanzmannMoverComponent* GetMoverComponent() const
		{
			return MoverComponent;
		}

		/**
		 * @see more info in Pawn.h.
		 */
		virtual void PostInitializeComponents() override;

	protected:
		/**
		 * Scan for IDanzmannInputProfileSourceInterface and apply its profile to the UDanzmannEnhancedInputComponent.
		 * @see more info in Pawn.h.
		 */
		virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

		/**
		 * Mover requires the input producer interface on the Pawn. Forwards to DanzmannMoverComponent->BuildInputCmd().
		 * @see more info in MoverSimulationTypes.h.
		 */
		virtual void ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult) override;

		/**
		 * Root capsule collider.
		 */
		UPROPERTY(BlueprintReadOnly, EditAnywhere)
		TObjectPtr<UCapsuleComponent> CapsuleCollider = nullptr;

		/**
		 * Main skeletal mesh.
		 */
		UPROPERTY(BlueprintReadOnly, EditAnywhere)
		TObjectPtr<USkeletalMeshComponent> MainSkeletalMesh = nullptr;

		/**
		 * Mover Component.
		 */
		UPROPERTY(BlueprintReadOnly, EditAnywhere)
		TObjectPtr<UDanzmannMoverComponent> MoverComponent = nullptr;
};
