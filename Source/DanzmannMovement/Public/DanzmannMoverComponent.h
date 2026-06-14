// Copyright (C) 2026 Vicente Danzmann. All Rights Reserved.

#pragma once

#include "AbilitySystemComponent.h"
#include "CoreMinimal.h"
#include "DanzmannInputBinderInterface.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "GameplayTagContainer.h"

#include "DanzmannMoverComponent.generated.h"

class UDanzmannAbilitySystemComponent;
class UDanzmannEnhancedInputComponent;
class UDanzmannInputProfile;
class UDanzmannMovementProfile;
class UNavMoverComponent;
struct FInputActionValue;

/**
 * Mover with input production, profile-driven settings, and  orient-to-movement/orient-to-control rotation. Stance 
 * behaviors (such as sprint, aim, ...) are driven externally by GAS. When the owner has no ASC the component falls 
 * back to neutral defaults so non-GAS pawns still function.
 *
 * Orientation is a single value this Component owns and publishes. The effective orientation is the active override
 * if one is set, otherwise the Movement Profile's default. The Component keeps exactly one Movement.Orientation.* loose
 * Gameplay Tag on the owner's Ability System Component reflecting it (so other systems can query it) and is the sole
 * writer of those Tags. Overrides are applied imperatively via SetOrientationOverride()/ClearOrientationOverride()
 * (e.g., an aim Ability sets OrientToControl on activate and clears it on end) -- never by granting the Tag through a
 * Gameplay Effect, which would break the single-tag invariant.
 *
 * Gameplay Effect-applied Attribute changes:
 *   - MovementSpeedMultiplier scales the live UCommonLegacyMovementSettings::MaxSpeed each time it changes.
 * 
 * @note To use a UDanzmannMoverComponent subclass on a derived Pawn, follow the same pattern Epic uses 
 * for CMC in ACharacter.
 */
UCLASS(ClassGroup = "Danzmann", HideCategories = ("Activation", "AssetUserData", "ComponentTick", "ComponentReplication", "Cooking", "Navigation", "Replication"), Meta = (BlueprintSpawnableComponent))
class DANZMANNMOVEMENT_API UDanzmannMoverComponent : public UCharacterMoverComponent, public IDanzmannInputBinderInterface
{
	GENERATED_BODY()

	public:
		/**
		 * @see more info in ActorComponent.h.
		 */
		virtual void BeginPlay() override;
		
		/**
		 * @see more info in ActorComponent.h.
		 */
		virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
		
		/**
		 * Build the Mover Input Command for the next simulation frame. Must be called from the owning Pawn's
		 * IMoverInputProducerInterface::ProduceInput_Implementation() override.
		 * @param DeltaMs Delta in milliseconds.
		 * @param Out_MoverInputCommandContext Mover Input Command to populate.
		 */
		void BuildInputCmd(float DeltaMs, FMoverInputCmdContext& Out_MoverInputCommandContext);
		
		/**
		 * FName the Component is created under. Pass to FObjectInitializer::SetDefaultSubobjectClass<>()
		 * to swap in a subclass on derived Pawn classes.
		 */
		static FName ComponentName;
	
	protected:
		/**
		 * Cached owning-Actor NavMoverComponent. 
		 * When present, BuildInputCmd() drains its requested path-following
		 * velocity each frame and uses it as the Mover's MoveInput (instead of the EnhancedInput cache).
		 * @note Null on Pawns controlled by players.
		 */
		UPROPERTY()
		TObjectPtr<UNavMoverComponent> NavMoverComponent = nullptr;
		
	#pragma region IDanzmannInputBinderInterface

		public:
			/**
			 * Bind inputs for Mover.
			 * @see more info in DanzmannInputBinderInterface.h.
			 */
			virtual void BindInputActions(UDanzmannEnhancedInputComponent* EnhancedInputComponent, const UDanzmannInputProfile* InputProfile) override;

	#pragma endregion IDanzmannInputBinderInterface
		
	#pragma region AbilitSystemComponentLinkage
		
		protected:
			/**
			 * Resolve owner's Ability System Component (Pawn or Player State) and registers delegates
			 * for communication with Ability System Component.
			 */
			virtual void SetupAbilitySystemComponentLinkage();

			/**
			 * Tear down delegates registered against the cached Ability System Component.
			 */
			virtual void TeardownAbilitySystemComponentLinkage();
		
			/**
			 * Cached owning-Actor Ability System Component.
			 */
			UPROPERTY()
			TObjectPtr<UDanzmannAbilitySystemComponent> AbilitySystemComponent = nullptr;
	
	#pragma endregion AbilitSystemComponentLinkage
		
	#pragma region AutoMove
		
		public:
			/**
			 * Get whether auto move is enabled or not.
			 * @return Is auto move enabled?
			 */
			UFUNCTION(BlueprintPure, Category = "Danzmann|Movement", Meta = (BlueprintThreadSafe))
			bool IsAutoMoveEnabled() const
			{
				return bAutoMove;
			}

			/**
			 * Set new value for bAutoMove.
			 * @param bNewValue New value for bAutoMove.
			 */
			UFUNCTION(BlueprintCallable, Category = "Danzmann|Movement")
			void SetAutoMove(const bool bNewValue)
			{
				bAutoMove = bNewValue;
			}
			
		private:
			/**
			 * Whether auto move is enabled or not.
			 */
			bool bAutoMove = false;
		
	#pragma endregion AutoMove

	#pragma region Input

		protected:
			/**
			 * Callback for Input Action "Move" for Triggered event.
			 * @param Value Input Action value.
			 */
			virtual void Input_MoveTriggered(const FInputActionValue& Value);

			/**
			 * Callback for Input Action "Move" for Completed event.
			 * @param Value Input Action value.
			 */
			virtual void Input_MoveCompleted(const FInputActionValue& Value);

			/**
			 * Callback for Input Action "Jump" for Started event.
			 * @param Value Input Action value.
			 */
			virtual void Input_JumpStarted(const FInputActionValue& Value);

			/**
			 * Callback for Input Action "Jump" for Completed event.
			 * @param Value Input Action value.
			 */
			virtual void Input_JumpCompleted(const FInputActionValue& Value);

			/**
			 * Callback for Input Action "Crouch" for Started event.
			 * @param Value Input Action value.
			 */
			virtual void Input_CrouchStarted(const FInputActionValue& Value);

			/**
			 * Callback for Input Action "Crouch" for Completed event.
			 * @param Value Input Action value.
			 */
			virtual void Input_CrouchCompleted(const FInputActionValue& Value);
		
			/**
			 * Cached velocity-based move input. When non-zero, BuildInputCmd() feeds it to the Mover as
			 * EMoveInputType::Velocity, taking precedence over MoveInputIntent. Persists across frames until changed.
			 */
			FVector MoveInputVelocity = FVector::ZeroVector;

			/**
			 * Cached directional move intent from the "Move" Input Action (per-axis, in the range [-1, 1]). When no
			 * velocity input is present, BuildInputCmd() rotates it by the Control Rotation and feeds it as
			 * EMoveInputType::DirectionalIntent. Set on Move Triggered, zeroed on Move Completed.
			 */
			FVector MoveInputIntent = FVector::ZeroVector;

			/**
			 * Whether the "Jump" Input Action is currently held. Mirrored into FCharacterDefaultInputs::bIsJumpPressed
			 * each frame by BuildInputCmd().
			 */
			bool bIsJumpPressed = false;

			/**
			 * Whether "Jump" was first pressed this frame (rising edge). Mirrored into
			 * FCharacterDefaultInputs::bIsJumpJustPressed, then consumed (cleared) at the end of each BuildInputCmd().
			 */
			bool bIsJumpJustPressed = false;

	#pragma endregion Input
		
	#pragma region MeshVerticalSmoothing

		protected:
			/**
			 * Bound to UMoverComponent::OnPostFinalize -- fires on the game thread right after each simulation frame is
			 * finalized (the root has been moved to the new state and the visual component's relative transform reset).
			 * This is the correct point to apply our smoothing: reading/writing the visual offset here can't be a frame
			 * stale relative to the capsule snap, which a separate Component tick would be.
			 * @param SyncState Finalized sync state for the frame.
			 * @param AuxStateContext Finalized aux state for the frame.
			 */
			UFUNCTION()
			void HandlePostFinalizeForMeshVerticalSmoothing(const FMoverSyncState& SyncState, const FMoverAuxStateContext& AuxStateContext);

			/**
			 * Ease the rendered mesh's vertical position toward the shape so it glides over the discrete up/down snaps
			 * the Mover simulation applies to the collision shape on uneven terrain and stairs.
			 * 
			 * The shape (root) still snaps for collision. We offset the primary visual component via
			 * UMoverComponent::SetBaseVisualComponentTransform() -- the sanctioned hook both FinalizeFrame and
			 * FinalizeSmoothingFrame read -- and also apply it to the mesh's relative transform immediately so it takes
			 * effect this frame. Smoothing is gated to grounded state; airborne vertical motion (jumps/falls) is intentional
			 * and passes through unsmoothed.
			 * @param DeltaTime Frame delta seconds.
			 */
			virtual void UpdateMeshVerticalSmoothing(const float DeltaTime);

			/**
			 * Whether vertical mesh smoothing is active. Sourced from the Movement Profile in ApplyMovementProfile().
			 */
			bool bEnableMeshVerticalSmoothing = true;

			/**
			 * How quickly the mesh catches up to the shape's vertical position (FMath::FInterpTo speed). Sourced from
			 * the Movement Profile.
			 */
			float MeshVerticalSmoothingInterpSpeed = 15.0f;

			/**
			 * Maximum distance, in cm, the mesh may lag behind the shape before the offset clamps (so big drops/teleports
			 * snap instead of smearing). Sourced from the Movement Profile.
			 */
			float MaxMeshVerticalSmoothingDistance = 50.0f;

		private:
			/**
			 * Smoothed (lagged) shape world Z that the mesh chases. Re-seeded to the live shape Z while airborne and on
			 * the first tick.
			 */
			double SmoothedShapeZ = 0.0;

			/**
			 * The Z offset this component last wrote into BaseVisualComponentTransform. Tracked so we can strip our own
			 * prior contribution and re-apply the new one without clobbering offsets written by other systems (e.g.
			 * crouch's UStanceModifier, which also adjusts BaseVisualComponentTransform).
			 */
			double AppliedMeshSmoothingZOffset = 0.0;

			/**
			 * Whether SmoothedShapeZ has been seeded from the shape yet.
			 */
			bool bMeshSmoothingInitialized = false;

	#pragma endregion MeshVerticalSmoothing
		
	#pragma region MovementDisabled
		
		public:
			/**
			 * Get whether movement is disabled or not.
			 * @return Is movement disabled?
			 */
			UFUNCTION(BlueprintPure, Category = "Danzmann|Movement", Meta = (BlueprintThreadSafe))
			bool IsMovementDisabled() const
			{
				return bMovementDisabled;
			}

			/**
			 * Set new value for bMovementDisabled.
			 * @param bNewValue New value for bMovementDisabled.
			 */
			UFUNCTION(BlueprintCallable, Category = "Danzmann|Movement")
			void SetMovementDisabled(const bool bNewValue)
			{
				bMovementDisabled = bNewValue;
			}
		
		private:
			/**
			 * Whether movement is disabled or not.
			 */
			bool bMovementDisabled = false;
	
	#pragma endregion MovementDisabled
		
	#pragma region MovementMode

		public:
			/**
			 * Get the current Movement Mode as a Movement.Mode.* Gameplay Tag, mirroring the Mover simulation's
			 * authoritative mode. This is the same value published as a loose Tag on the owner's Ability System Component.
			 * @return Current Movement Mode Tag (invalid for an unmapped/custom Mode).
			 */
			UFUNCTION(BlueprintPure, Category = "Danzmann|Movement", Meta = (BlueprintThreadSafe))
			FGameplayTag GetCurrentMovementMode() const
			{
				return CurrentMovementMode;
			}

			/**
			 * Request a Movement Mode. Fed to the Mover as the suggested Mode starting next frame. The simulation stays
			 * authoritative and may resolve a different actual Mode (reflected by GetCurrentMovementMode()).
			 * @param NewMovementMode One of the Movement.Mode.* Tags.
			 */
			UFUNCTION(BlueprintCallable, Category = "Danzmann|Movement")
			void RequestMovementMode(UPARAM(Meta = (Categories = "Movement.Mode")) const FGameplayTag NewMovementMode)
			{
				RequestedMovementMode = NewMovementMode;
			}

		protected:
			/**
			 * Bound to UMoverComponent::OnMovementModeChanged. Maps the new Mode FName to a Movement.Mode.* tag, caches it,
			 * and republishes it as the loose Tag on the Ability System Component.
			 * @param PreviousMovementModeName Movement Mode FName before the change.
			 * @param NewMovementModeName Movement Mode FName after the change.
			 */
			UFUNCTION()
			void HandleMovementModeChanged(const FName& PreviousMovementModeName, const FName& NewMovementModeName);

			/**
			 * Make the Ability System Component's loose Movement.Mode.* Tag match CurrentMovementModeTag (remove the
			 * previously-applied one, add the current). No-ops until the Ability System Component is linked. Idempotent.
			 */
			virtual void SyncMovementModeGameplayTag();
		
			/**
			 * Get the corresponding Gameplay Tag (Movement.Mode.*) given a Movement Mode name.
			 * @param MovementModeName Movement Mode name.
			 * @return Return matching Gameplay Tag to Movement Mode name. Return an empty Tag
			 * for an unmapped Mode.
			 */
			FGameplayTag GetMovementModeGameplayTagFromName(const FName MovementModeName);

			/**
			 * Requested/suggested Movement Mode fed into the Mover input. Seeded from the Movement Profile's default and
			 * updated by SetMovementMode(). The simulation remains authoritative over the actual Mode.
			 */
			FGameplayTag RequestedMovementMode;

			/**
			 * Current Movement Mode Tag, cached from UMoverComponent::OnMovementModeChanged so reads (including
			 * GetMovementModeTag() on worker threads) never touch the simulation or the Ability System Component.
			 */
			FGameplayTag CurrentMovementMode;

			/**
			 * The Movement.Mode.* loose Tag currently applied to the Ability System Component. Tracked so a Mode
			 * change removes the old Tag before adding the new one.
			 */
			FGameplayTag AppliedMovementMode;

	#pragma endregion MovementMode
		
	#pragma region MovementProfile
		
		public:
			/**
			 * Push a Movement Profile onto the Component.
			 * @param MovementProfileToApply The profile to apply.
			 */
			UFUNCTION(BlueprintCallable, Category = "Danzmann|Movement")
			void SetMovementProfile(const UDanzmannMovementProfile* MovementProfileToApply);
		
		protected:
			/**
			 * Apply the cached Movement Profile's values to UCommonLegacyMovementSettings (MaxSpeed,
			 * Acceleration, Deceleration, JumpUpwardsSpeed) and cache BaseMaxSpeed (pre-multiplier).
			 */
			virtual void ApplyMovementProfile();
		
			/**
			 * Movement Profile applied to this instance. Set by SetMovementProfile() (typically from PawnData).
			 */
			UPROPERTY()
			TObjectPtr<const UDanzmannMovementProfile> MovementProfile = nullptr;
		
			/**
			 * Pre-multiplier MaxSpeed cached from the Movement Profile (or from the legacy settings at startup).
			 * MoveSpeedMultiplier Attribute scales this value into the live CommonLegacySettings::MaxSpeed.
			 */
			float BaseMaxSpeed = 600.0f;
		
	#pragma endregion MovementProfile
		
	#pragma region MovementRelativeToStandingBase
		
		public:
			/**
			 * Get whether movement should be relative to base we're standing on.
			 */
			UFUNCTION(BlueprintPure, Category = "Danzmann|Movement")
			bool GetMovementRelativeToStandingBase() const
			{
				return bMovementRelativeToStandingBase;
			}

			/**
			 * Set a new value for whether movement should be relative to base we're standing on.
			 * @param bNewValue New value for bMovementRelativeToStandingBase.
			 */
			UFUNCTION(BlueprintCallable, Category = "Danzmann|Movement")
			void SetMovementRelativeToStandingBase(const bool bNewValue)
			{
				bMovementRelativeToStandingBase = bNewValue;
			}
			
		protected:
			/**
			 * Whether or not we author our movement inputs relative to whatever base we're standing on, or
			 * leave them in world space. Only applies if standing on a base of some sort.
			 */
			bool bMovementRelativeToStandingBase = true;
		
	#pragma endregion MovementRelativeToStandingBase
		
	#pragma region MovementSpeedMultiplier
		
		public:
			/**
			 * Get current movement speed multiplier Attribute value from
			 * the owner's Ability System Component, or 1.0f if none is present.
			 */
			UFUNCTION(BlueprintPure, Category = "Danzmann|Movement")
			float GetMovementSpeedMultiplier() const;
		
		protected:
			/**
			 * Update CommonLegacySettings->MaxSpeed = BaseMaxSpeed * NewMultiplier. 
			 * Bound to the Ability System Component's MovementSpeedMultiplier Attribute change delegate.
			 */
			virtual void OnMovementSpeedMultiplierUpdated(const FOnAttributeChangeData& AttributeData);
		
	#pragma endregion MovementSpeedMultiplier
		
	#pragma region OrientationMode
		
		public:
			/**
			 * Get the effective Orientation Mode Gameplay Tag the Pawn is currently orienting by -- either
			 * Movement.Orientation.OrientToControl or Movement.Orientation.OrientToMovement. This is the same value
			 * published as the single Movement.Orientation.* loose Tag on the owner's Ability System Component.
			 */
			UFUNCTION(BlueprintPure, Category = "Danzmann|Movement", Meta = (BlueprintThreadSafe))
			FGameplayTag GetOrientationMode() const
			{
				return CurrentOrientationMode;
			}

			/**
			 * Override the Orientation Mode, replacing any current override. Takes effect from next frame and is mirrored as
			 * the single Movement.Orientation.* loose Tag on the Ability System Component. A no-op if it doesn't change
			 * the effective orientation. Pair with ClearOrientationOverride() when the requesting feature ends.
			 * @param NewOrientationModeOverride One of the Movement.Orientation.* Tags (an invalid Tag clears the override).
			 */
			UFUNCTION(BlueprintCallable, Category = "Danzmann|Movement")
			void SetOrientationModeOverride(UPARAM(Meta = (Categories = "Movement.Orientation")) const FGameplayTag NewOrientationModeOverride);

			/**
			 * Clear any active Orientation Mode override, reverting the effective orientation to the Movement Profile's default.
			 */
			UFUNCTION(BlueprintCallable, Category = "Danzmann|Movement")
			void ClearOrientationModeOverride();

		protected:
			/**
			 * Recompute the effective orientation (override if set, otherwise the default) into CurrentOrientationTag and
			 * sync it onto the Ability System Component. Called after changing the override or the default.
			 */
			virtual void RefreshOrientationMode();

			/**
			 * Make the Ability System Component's single Movement.Orientation.* loose Tag match CurrentOrientationMode
			 * (remove the previously-applied one, add the current). This component is the sole writer of those Tags, so
			 * exactly one is present at a time. No-ops until the Ability System Component is linked. Idempotent.
			 */
			virtual void SyncOrientationModeGameplayTag();

			/**
			 * Default Orientation Mode, seeded from the Movement Profile's DefaultOrientationMode (Movement.Orientation.OrientToControl
			 * when unset) in ApplyMovementProfile(). Used as the effective orientation whenever no override is active.
			 */
			FGameplayTag DefaultOrientationMode;

			/**
			 * Active Orientation Mode override set via SetOrientationOverride(), or invalid when none. When valid it wins over
			 * DefaultOrientationMode.
			 */
			FGameplayTag OrientationModeOverride;

			/**
			 * Effective Orientation Mode = override if set, otherwise default. Cached so BuildInputCmd and
			 * GetOrientationMode() never query the Ability System Component on the simulation path.
			 */
			FGameplayTag CurrentOrientationMode;

			/**
			 * The Movement.Orientation.* loose Tag currently applied to the Ability System Component. Tracked so a change
			 * removes the old tag before adding the new one.
			 */
			FGameplayTag AppliedOrientationModeGameplayTag;

	#pragma endregion OrientationMode
};
