// Copyright (C) 2026 Vicente Danzmann. All Rights Reserved.

#include "DanzmannMoverComponent.h"

#include "AbilitySystemGlobals.h"
#include "AIController.h"
#include "DanzmannAbilitySystemComponent.h"
#include "AttributeSets/DanzmannAttributeSet_Movement.h"
#include "DanzmannEnhancedInputComponent.h"
#include "DanzmannGameplayTags_InputActions.h"
#include "DanzmannInputProfile.h"
#include "DanzmannLogDanzmannMovement.h"
#include "DanzmannMovementGameplayTags.h"
#include "DanzmannMovementProfile.h"
#include "DefaultMovementSet/NavMoverComponent.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "MoveLibrary/BasedMovementUtils.h"
#include "MoverSimulationTypes.h"
#include "MoverTypes.h"

FName UDanzmannMoverComponent::ComponentName(TEXT("DanzmannMoverComponent"));

void UDanzmannMoverComponent::BeginPlay()
{
	Super::BeginPlay();

	ApplyMovementProfile();
	
	/*
	 * Mesh Vertical Smoothing
	 */

	// Drive vertical mesh smoothing from the OnPostFinalize callback rather than a Component tick,
	// so the visual offset is never a frame stale relative to the shape's vertical snap
	OnPostFinalize.AddDynamic(this, &ThisClass::HandlePostFinalizeForMeshVerticalSmoothing);

	/*
	 * Movement Mode publishing
	 */

	// Mirror the simulation's authoritative movement mode as a loose Tag. Seed from the current mode, then track changes
	CurrentMovementMode = GetMovementModeGameplayTagFromName(GetMovementModeName());
	OnMovementModeChanged.AddDynamic(this, &ThisClass::HandleMovementModeChanged);

	/* 
	 * Ability System Component
	 */
	
	AActor* Owner = GetOwner();
	
	ensureMsgf(Owner->IsA(APawn::StaticClass()) || Owner->IsA(APlayerState::StaticClass()), TEXT("UDanzmannMoverComponent should live on UPawn or UPlayerState -- or derived classes."));
	
	UDanzmannAbilitySystemComponent* DanzmannAbilitySystemComponent = nullptr;
	
	// Try to get Ability System Component from Pawn
	if (UAbilitySystemComponent* PawnAbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner); IsValid(PawnAbilitySystem))
	{
		AbilitySystemComponent = Cast<UDanzmannAbilitySystemComponent>(PawnAbilitySystem);
	}
	// Try to get Ability System Component from Player State
	else if (UAbilitySystemComponent* PlayerStateAbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Cast<APawn>(Owner)->GetPlayerState()); IsValid(PlayerStateAbilitySystem))
	{
		AbilitySystemComponent = Cast<UDanzmannAbilitySystemComponent>(PlayerStateAbilitySystem);
	}
	else
	{
		UE_LOG(LogDanzmannMovement, Warning, TEXT("[%hs] No Ability System Component found on Pawn or Player State. Linkage with GAS will be disabled."), __FUNCTION__);
		return;
	}
	
	AbilitySystemComponent->OnInitialization(FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::SetupAbilitySystemComponentLinkage));
}

void UDanzmannMoverComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	OnPostFinalize.RemoveDynamic(this, &ThisClass::HandlePostFinalizeForMeshVerticalSmoothing);
	OnMovementModeChanged.RemoveDynamic(this, &ThisClass::HandleMovementModeChanged);

	TeardownAbilitySystemComponentLinkage();

	Super::EndPlay(EndPlayReason);
}

void UDanzmannMoverComponent::BuildInputCmd(float DeltaMs, FMoverInputCmdContext& Out_MoverInputCommandContext)
{
	/*
	 * Generate user commands. Called right before the movement simulation will tick (for a locally controlled Pawn).
	 * The code here happens outside the movement simulation meaning that it does not run on the server (and non-controlling clients)
	 * and is not rerun during reconcile/resimulates.
	 */
	
	static const FCharacterDefaultInputs DoNothingInput;
	
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	FCharacterDefaultInputs& CharacterDefaultInputs = Out_MoverInputCommandContext.InputCollection.FindOrAddMutableDataByType<FCharacterDefaultInputs>();
	
	/*
	 * Check validity of owner's Controller before proceeding.
	 */
	
	if (!IsValid(OwnerPawn->GetController()))
	{
		if (OwnerPawn->GetLocalRole() == ROLE_Authority && OwnerPawn->GetRemoteRole() == ROLE_SimulatedProxy)
		{
			// Pawn isn't currently possessed, so provide default do-nothing input
			CharacterDefaultInputs = DoNothingInput;
		}

		// We don't have a local Controller so we can't run the code below. This is ok.
		// Simulated proxies will just use previous input when extrapolating
		return;
	}
	
	/*
	 * Check if movement is disabled.
	 */
	
	if (bMovementDisabled)
	{
		CharacterDefaultInputs = DoNothingInput;
		return;
	}
	
	/*
	 * Get Control Rotation.
	 */
	
	CharacterDefaultInputs.ControlRotation = FRotator::ZeroRotator;

	// AI-controlled Pawn  
	if (AController* OwnerController = OwnerPawn->GetController(); OwnerController->IsA(AAIController::StaticClass()))
	{
		// Synthesize a Control Rotation that points at the focal point so aim/orient-to-control
		// behaves as if the Pawn were "looking at" its target.
		if (const FVector FocalPoint =  Cast<AAIController>(OwnerController)->GetFocalPoint(); !FocalPoint.IsNearlyZero())
		{
			const FVector EyeLocation = OwnerPawn->GetPawnViewLocation();
			CharacterDefaultInputs.ControlRotation = (FocalPoint - EyeLocation).Rotation();
		}
	}
	// Player-controlled Pawn
	else
	{
		CharacterDefaultInputs.ControlRotation = Cast<APlayerController>(OwnerController)->GetControlRotation();
	}
	
	/*
	 * Try to resolve NavMoverComponent.
	 */

	// Resolve the optional NavMoverComponent -- present on AI Pawns, absent on players.
	if (!IsValid(NavMoverComponent))
	{
		NavMoverComponent = OwnerPawn->FindComponentByClass<UNavMoverComponent>();
	}
	
	/*
	 * Set move input.
	 */
	
	// AI move
	if (IsValid(NavMoverComponent))
	{
		FVector NavMoveInputIntent = FVector::ZeroVector;
		FVector NavMoveInputVelocity = FVector::ZeroVector;
		
		if (NavMoverComponent->ConsumeNavMovementData(NavMoveInputIntent, NavMoveInputVelocity))
		{
			// Path Following Component drove this frame -- prefer its velocity, fall back to intent
			if (!NavMoveInputVelocity.IsZero())
			{
				CharacterDefaultInputs.SetMoveInput(EMoveInputType::Velocity, NavMoveInputVelocity);
			}
			else
			{
				CharacterDefaultInputs.SetMoveInput(EMoveInputType::DirectionalIntent, NavMoveInputIntent);
			}
		}
	}
	// Player move
	else
	{
		if (bAutoMove)
		{
			MoveInputIntent.X = 1.0f;
		}
		
		// Favor velocity input
		if (MoveInputVelocity.IsZero())
		{
			const FVector MoveInputDirectionalIntent = CharacterDefaultInputs.ControlRotation.RotateVector(MoveInputIntent);
			CharacterDefaultInputs.SetMoveInput(EMoveInputType::DirectionalIntent, MoveInputDirectionalIntent);
		}
		else
		{
			CharacterDefaultInputs.SetMoveInput(EMoveInputType::Velocity, MoveInputVelocity);
		}
	}
	
	/*
	 * Set orientation intent.
	 */

	static constexpr float RotationMagnitudeMin = 1e-3f;
	const bool bHasAffirmativeMoveInput = CharacterDefaultInputs.GetMoveInput().Size() >= RotationMagnitudeMin;

	// Figure out intended orientation
	CharacterDefaultInputs.OrientationIntent = FVector::ZeroVector;

	if (bHasAffirmativeMoveInput)
	{
		if (CurrentOrientationMode == Danzmann::GameplayTags::Movement_Orientation_OrientToControl)
		{
			CharacterDefaultInputs.OrientationIntent = CharacterDefaultInputs.ControlRotation.Vector().GetSafeNormal();
		}
		else
		{
			CharacterDefaultInputs.OrientationIntent = CharacterDefaultInputs.GetMoveInput().GetSafeNormal();
		}
	}
	
	/*
	 * Set Jump inputs.
	 */

	CharacterDefaultInputs.bIsJumpPressed = bIsJumpPressed;
	CharacterDefaultInputs.bIsJumpJustPressed = bIsJumpJustPressed;
	
	/*
	 * Set Movement Mode.
	 */

	// Only Flying is a deliberate suggestion; ground/air/water modes are left for the simulation to resolve.
	CharacterDefaultInputs.SuggestedMovementMode = (RequestedMovementMode == Danzmann::GameplayTags::Movement_Mode_Flying)
		? DefaultModeNames::Flying
		: NAME_None;
	
	/*
	 * Convert inputs to be relative to standing base -- if necessary.
	 */
	
	CharacterDefaultInputs.bUsingMovementBase = false;

	if (bMovementRelativeToStandingBase)
	{
		if (UPrimitiveComponent* MovementBase = GetMovementBase(); IsValid(MovementBase))
		{
			const FName MovementBaseBoneName = GetMovementBaseBoneName();

			FVector RelativeMoveInput = FVector::ZeroVector;
			FVector RelativeOrientDirection = FVector::ZeroVector;

			UBasedMovementUtils::TransformWorldDirectionToBased(MovementBase, MovementBaseBoneName, CharacterDefaultInputs.GetMoveInput(), RelativeMoveInput);
			UBasedMovementUtils::TransformWorldDirectionToBased(MovementBase, MovementBaseBoneName, CharacterDefaultInputs.OrientationIntent, RelativeOrientDirection);

			CharacterDefaultInputs.SetMoveInput(CharacterDefaultInputs.GetMoveInputType(), RelativeMoveInput);
			CharacterDefaultInputs.OrientationIntent = RelativeOrientDirection;

			CharacterDefaultInputs.bUsingMovementBase = true;
			CharacterDefaultInputs.MovementBase = MovementBase;
			CharacterDefaultInputs.MovementBaseBoneName = MovementBaseBoneName;
		}
	}
	
	/*
	 * Clear/consume temporal movement inputs. Other cached inputs persists across frames so that they carry
	 * over when the game world ticks at a lower rate than the Mover simulation.
	 */
	
	bIsJumpJustPressed = false;
}

#pragma region IDanzmannInputBinderInterface

	void UDanzmannMoverComponent::BindInputActions(UDanzmannEnhancedInputComponent* EnhancedInputComponent, const UDanzmannInputProfile* InputProfile)
	{
		if (!IsValid(EnhancedInputComponent) || !IsValid(InputProfile))
		{
			return;
		}

		// Move
		EnhancedInputComponent->BindNativeInputAction(InputProfile, Danzmann::GameplayTags::InputAction_Move, ETriggerEvent::Triggered, this, &ThisClass::Input_MoveTriggered);
		EnhancedInputComponent->BindNativeInputAction(InputProfile, Danzmann::GameplayTags::InputAction_Move, ETriggerEvent::Completed, this, &ThisClass::Input_MoveCompleted);
		// Jump
		EnhancedInputComponent->BindNativeInputAction(InputProfile, Danzmann::GameplayTags::InputAction_Jump, ETriggerEvent::Started, this, &ThisClass::Input_JumpStarted);
		EnhancedInputComponent->BindNativeInputAction(InputProfile, Danzmann::GameplayTags::InputAction_Jump, ETriggerEvent::Completed, this, &ThisClass::Input_JumpCompleted);
	}

#pragma endregion IDanzmannInputBinderInterface

#pragma region AbilitSystemComponentLinkage

	void UDanzmannMoverComponent::SetupAbilitySystemComponentLinkage()
	{
		/*
		 * Set up Movement Speed Multiplier
		 */
		
		// Apply the current MovementSpeedMultiplier once so MaxSpeed is in sync before the first sim tick
		if (UCommonLegacyMovementSettings* CommonLegacyMovementSettings = FindSharedSettings_Mutable<UCommonLegacyMovementSettings>(); IsValid(CommonLegacyMovementSettings))
		{
			const float CurrentMovementSpeedMultiplier = AbilitySystemComponent->GetNumericAttribute(UDanzmannAttributeSet_Movement::GetMovementSpeedMultiplierAttribute());
			CommonLegacyMovementSettings->MaxSpeed = BaseMaxSpeed * CurrentMovementSpeedMultiplier;
		}

		// Subscribe to MovementSpeedMultiplier changes so MaxSpeed tracks the Attribute
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UDanzmannAttributeSet_Movement::GetMovementSpeedMultiplierAttribute()).AddUObject(this, &ThisClass::OnMovementSpeedMultiplierUpdated);

		/*
		 * Publish current Orientation Mode
		 */

		// Mirror the current (already-seeded) effective orientation onto the Ability System Component now that it's linked
		SyncOrientationModeGameplayTag();

		/*
		 * Publish current Movement Mode
		 */

		// Mirror the current (already-seeded) Movement Mode onto the Ability System Component now that it's linked
		SyncMovementModeGameplayTag();
	}

	void UDanzmannMoverComponent::TeardownAbilitySystemComponentLinkage()
	{
		if (!IsValid(AbilitySystemComponent))
		{
			return;
		}
		
		// Clear subscription for MovementSpeedMultiplier Attribute changes
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UDanzmannAttributeSet_Movement::GetMovementSpeedMultiplierAttribute()).RemoveAll(this);

		// Remove the published orientation loose Tag we added
		if (AppliedOrientationModeGameplayTag.IsValid())
		{
			AbilitySystemComponent->RemoveLooseGameplayTag(AppliedOrientationModeGameplayTag);
			AppliedOrientationModeGameplayTag = FGameplayTag();
		}

		// Remove the published current Movement Mode loose Tag we added
		if (AppliedMovementMode.IsValid())
		{
			AbilitySystemComponent->RemoveLooseGameplayTag(AppliedMovementMode);
			AppliedMovementMode = FGameplayTag();
		}

		AbilitySystemComponent = nullptr;
	}

#pragma endregion AbilitSystemComponentLinkage

#pragma region Input

	void UDanzmannMoverComponent::Input_MoveTriggered(const FInputActionValue& Value)
	{
		const FVector MoveVector = Value.Get<FVector>();
		MoveInputIntent.X = FMath::Clamp(MoveVector.Y, -1.0f, 1.0f);
		MoveInputIntent.Y = FMath::Clamp(MoveVector.X, -1.0f, 1.0f);
		MoveInputIntent.Z = FMath::Clamp(MoveVector.Z, -1.0f, 1.0f);
	}

	void UDanzmannMoverComponent::Input_MoveCompleted(const FInputActionValue& Value)
	{
		MoveInputIntent = FVector::ZeroVector;
	}

	void UDanzmannMoverComponent::Input_JumpStarted(const FInputActionValue& Value)
	{
		// Is this the first frame we want to jump?
		bIsJumpJustPressed = !bIsJumpPressed;
		bIsJumpPressed = true;
	}

	void UDanzmannMoverComponent::Input_JumpCompleted(const FInputActionValue& Value)
	{
		bIsJumpPressed = false;
		bIsJumpJustPressed = false;
	}

	void UDanzmannMoverComponent::Input_CrouchStarted(const FInputActionValue& Value)
	{
		Crouch();
	}

	void UDanzmannMoverComponent::Input_CrouchCompleted(const FInputActionValue& Value)
	{
		UnCrouch();
	}

#pragma endregion Input

#pragma region MeshVerticalSmoothing

	void UDanzmannMoverComponent::HandlePostFinalizeForMeshVerticalSmoothing(const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxStateContext)
	{
		const UWorld* World = GetWorld();

		if (!IsValid(World) || !bEnableMeshVerticalSmoothing)
		{
			return;
		}

		UpdateMeshVerticalSmoothing(World->GetDeltaSeconds());
	}

	void UDanzmannMoverComponent::UpdateMeshVerticalSmoothing(const float DeltaTime)
	{
		const USceneComponent* RootComponent = GetUpdatedComponent();
		USceneComponent* VisualComponent = GetPrimaryVisualComponent();

		// The offset moves the visual Component relative to the root, so it only makes sense when they're distinct
		if (!IsValid(RootComponent) || !IsValid(VisualComponent) || VisualComponent == RootComponent)
		{
			return;
		}

		const double ShapeZ = RootComponent->GetComponentLocation().Z;

		// Seed on the first tick, and snap (no lag) while airborne -- vertical motion from jumps/falls is intentional and should stay crisp
		if (!bMeshSmoothingInitialized || !IsOnGround())
		{
			SmoothedShapeZ = ShapeZ;
			bMeshSmoothingInitialized = true;
		}
		else
		{
			SmoothedShapeZ = FMath::FInterpTo(SmoothedShapeZ, ShapeZ, DeltaTime, MeshVerticalSmoothingInterpSpeed);
		}

		// How far the mesh trails behind the snapping capsule, clamped so big drops/teleports snap instead of smearing
		const double NewOffsetSmoothingZ = FMath::Clamp(SmoothedShapeZ - ShapeZ, -MaxMeshVerticalSmoothingDistance, MaxMeshVerticalSmoothingDistance);

		// Compose with any external visual offset rather than overwriting it. Other systems write BaseVisualComponentTransform
		// too -- notably crouch's UStanceModifier, which adds a Z offset to compensate for a capsule resize. We strip the
		// offset we applied last frame and re-apply the new one, so only our own contribution changes
		FTransform BaseVisualTransform = GetBaseVisualComponentTransform();
		FVector BaseVisualLocation = BaseVisualTransform.GetLocation();
		BaseVisualLocation.Z += NewOffsetSmoothingZ - AppliedMeshSmoothingZOffset;
		BaseVisualTransform.SetLocation(BaseVisualLocation);
		SetBaseVisualComponentTransform(BaseVisualTransform);

		// FinalizeFrame() already reset the mesh's relative transform to the pre-update BaseVisualComponentTransform.
		// Apply the fresh offset directly so the smoothed position is reflected this frame instead of next
		VisualComponent->SetRelativeTransform(BaseVisualTransform);

		AppliedMeshSmoothingZOffset = NewOffsetSmoothingZ;
	}

#pragma endregion MeshVerticalSmoothing

#pragma region MovementMode

	void UDanzmannMoverComponent::HandleMovementModeChanged(const FName& PreviousMovementModeName, const FName& NewMovementModeName)
	{
		CurrentMovementMode = GetMovementModeGameplayTagFromName(NewMovementModeName);
		SyncMovementModeGameplayTag();
	}

	void UDanzmannMoverComponent::SyncMovementModeGameplayTag()
	{
		if (!IsValid(AbilitySystemComponent) || CurrentMovementMode == AppliedMovementMode)
		{
			return;
		}

		// Swap the always-present current Mode loose Tag so the Ability System Component mirrors the simulation
		if (AppliedMovementMode.IsValid())
		{
			AbilitySystemComponent->RemoveLooseGameplayTag(AppliedMovementMode);
		}

		if (CurrentMovementMode.IsValid())
		{
			AbilitySystemComponent->AddLooseGameplayTag(CurrentMovementMode);
		}

		AppliedMovementMode = CurrentMovementMode;
	}

	FGameplayTag UDanzmannMoverComponent::GetMovementModeGameplayTagFromName(const FName MovementModeName)
	{
		if (MovementModeName == DefaultModeNames::Walking)
		{
			return Danzmann::GameplayTags::Movement_Mode_Walking;
		}
	
		if (MovementModeName == DefaultModeNames::Falling)
		{
			return Danzmann::GameplayTags::Movement_Mode_Falling;
		}
	
		if (MovementModeName == DefaultModeNames::Flying)
		{
			return Danzmann::GameplayTags::Movement_Mode_Flying;
		}
	
		if (MovementModeName == DefaultModeNames::Swimming)
		{
			return Danzmann::GameplayTags::Movement_Mode_Swimming;
		}

		return FGameplayTag();
	}

#pragma endregion MovementMode

#pragma region MovementProfile

	void UDanzmannMoverComponent::SetMovementProfile(const UDanzmannMovementProfile* MovementProfileToApply)
	{
		checkf(IsValid(MovementProfileToApply), TEXT("[%hs] Movement Profile to apply is invalid."), __FUNCTION__);
	
		MovementProfile = MovementProfileToApply;

		if (HasBegunPlay())
		{
			ApplyMovementProfile();
		}
	}

	void UDanzmannMoverComponent::ApplyMovementProfile()
	{
		if (!IsValid(MovementProfile))
		{
			UE_LOG(LogDanzmannMovement, Warning, TEXT("[%hs] No Movement Profile set. Using default movement values from UCommonLegacyMovementSettings."), __FUNCTION__);
			return;
		}
	
		// Get Common Legacy Movement Settings
		UCommonLegacyMovementSettings* CommonLegacyMovementSettings = FindSharedSettings_Mutable<UCommonLegacyMovementSettings>();
	
		if (!IsValid(CommonLegacyMovementSettings))
		{
			UE_LOG(LogDanzmannMovement, Warning, TEXT("[%hs] No valid UCommonLegacyMovementSettings found. Using engine defaults."), __FUNCTION__);
			return;
		}
	
		// Apply Movement Profile values
		CommonLegacyMovementSettings->MaxSpeed = MovementProfile->MaxWalkSpeed;
		CommonLegacyMovementSettings->Acceleration = MovementProfile->MaxAcceleration;
		CommonLegacyMovementSettings->Deceleration = MovementProfile->BrakingDeceleration;
		CommonLegacyMovementSettings->JumpUpwardsSpeed = MovementProfile->JumpVelocityZ;
		CommonLegacyMovementSettings->TurningRate = MovementProfile->TurningRate;
		CommonLegacyMovementSettings->TurningBoost = MovementProfile->TurningBoost;
		// Store the pre-multiplier base speed so the MovementSpeedMultiplier attribute can scale it later on
		BaseMaxSpeed = CommonLegacyMovementSettings->MaxSpeed;
		RequestedMovementMode = MovementProfile->DefaultMovementMode;

		// Seed the default orientation from the profile (OrientToControl when unset), then re-resolve the effective
		// orientation. An active override keeps winning; with none, this becomes the effective orientation.
		DefaultOrientationMode = MovementProfile->DefaultOrientationMode.IsValid()? MovementProfile->DefaultOrientationMode : Danzmann::GameplayTags::Movement_Orientation_OrientToControl;
		RefreshOrientationMode();

		// Vertical mesh smoothing
		bEnableMeshVerticalSmoothing = MovementProfile->bEnableMeshVerticalSmoothing;
		MeshVerticalSmoothingInterpSpeed = MovementProfile->MeshVerticalSmoothingInterpSpeed;
		MaxMeshVerticalSmoothingDistance = MovementProfile->MeshVerticalSmoothingMaxDistance;
	}

#pragma endregion MovementProfile

#pragma region MovementSpeedMultiplier

	float UDanzmannMoverComponent::GetMovementSpeedMultiplier() const
	{
		if (!IsValid(AbilitySystemComponent))
		{
			return 1.0f;
		}
	
		return AbilitySystemComponent->GetNumericAttribute(UDanzmannAttributeSet_Movement::GetMovementSpeedMultiplierAttribute());
	}

	void UDanzmannMoverComponent::OnMovementSpeedMultiplierUpdated(const FOnAttributeChangeData& AttributeData)
	{
		if (UCommonLegacyMovementSettings* LegacyMovementSettings = FindSharedSettings_Mutable<UCommonLegacyMovementSettings>(); IsValid(LegacyMovementSettings))
		{
			LegacyMovementSettings->MaxSpeed = BaseMaxSpeed * AttributeData.NewValue;
		}
	}

#pragma endregion MovementSpeedMultiplier

#pragma region OrientationMode

	EDanzmannOrientationMode UDanzmannMoverComponent::GetOrientationMode() const
	{
		// OrientToControl is the effective default whenever the cached Tag isn't explicitly OrientToMovement
		return CurrentOrientationMode == Danzmann::GameplayTags::Movement_Orientation_OrientToMovement ? EDanzmannOrientationMode::OrientToMovement : EDanzmannOrientationMode::OrientToControl;
	}

	void UDanzmannMoverComponent::SetOrientationModeOverride(const FGameplayTag NewOrientationModeOverride)
	{
		OrientationModeOverride = NewOrientationModeOverride;
		RefreshOrientationMode();
	}

	void UDanzmannMoverComponent::ClearOrientationModeOverride()
	{
		OrientationModeOverride = FGameplayTag();
		RefreshOrientationMode();
	}

	void UDanzmannMoverComponent::RefreshOrientationMode()
	{
		// Effective orientation: the override when set, otherwise the profile default
		CurrentOrientationMode = OrientationModeOverride.IsValid() ? OrientationModeOverride : DefaultOrientationMode;
		SyncOrientationModeGameplayTag();
	}

	void UDanzmannMoverComponent::SyncOrientationModeGameplayTag()
	{
		if (!IsValid(AbilitySystemComponent) || CurrentOrientationMode == AppliedOrientationModeGameplayTag)
		{
			return;
		}

		// Sole writer of Movement.Orientation.* -- keep exactly one such loose tag on the Ability System Component
		if (AppliedOrientationModeGameplayTag.IsValid())
		{
			AbilitySystemComponent->RemoveLooseGameplayTag(AppliedOrientationModeGameplayTag);
		}

		if (CurrentOrientationMode.IsValid())
		{
			AbilitySystemComponent->AddLooseGameplayTag(CurrentOrientationMode);
		}

		AppliedOrientationModeGameplayTag = CurrentOrientationMode;
	}

#pragma endregion OrientationMode

