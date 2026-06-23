# DanzmannMovement

A modular movement layer built on Epic's Mover framework. A Movement Profile describes a Pawn's locomotion in data; a Mover Component consumes that profile, produces the per-frame Mover Input Command, and publishes its Orientation Mode and Movement Mode as loose Gameplay Tags so the rest of the game can react. Stance behaviors (sprint, aim, ...) are deliberately not hard-wired here -- they are driven externally through GAS.

Built to sit beside the sibling Danzmann plugins: input bindings ride `DanzmannInput`'s apply pipeline, speed and orientation talk to `DanzmannAbilities` Attributes/Tags, and a Movement Profile is normally pushed onto the Component from `DanzmannExperiences` Pawn Data. When the owner has no Ability System Component the Component falls back to neutral defaults so non-GAS Pawns (and AI) still move.

> Depends on the sibling Danzmann plugins `DanzmannAbilities` and `DanzmannInput`. For input to bind, the project must set `DefaultInputComponentClass` variable in `Config/DefaultInput.ini` as the following: 
> ```ini
> [/Script/Engine.InputSettings]
> DefaultInputComponentClass=/Script/DanzmannInput.DanzmannEnhancedInputComponent
> ```
> Also, the active Input Profile must pair `InputAction.Move` and `InputAction.Jump` with `UInputAction`s. The GAS linkage expects a `UDanzmannAbilitySystemComponent` (on the Pawn or its Player State) carrying the `MovementSpeedMultiplier` Attribute.

## Concepts

- `UDanzmannMovementProfile` (`UDataAsset`): one immutable description of a Pawn's locomotion, authored in the Editor. Carries walking speed/acceleration/braking, jump velocity, turning rate/boost, the `DefaultMovementMode` and `DefaultOrientationMode` Gameplay Tags, and the vertical mesh smoothing toggles. Applied values are written into Mover's `UCommonLegacyMovementSettings`;
- `UDanzmannMoverComponent` (`UCharacterMoverComponent`, implements `IDanzmannInputBinderInterface`): the heart of the plugin. One Component does input production, profile application, GAS linkage, orientation, movement mirroring, and mesh smoothing;
  - `BuildInputCmd(DeltaMs, Out_InputCmd)`: builds the Mover Input Command for the next simulation frame. Resolves the Control Rotation (from the Player Controller, or synthesized from an `AIController`'s focal point), turns the cached move input into either a velocity or directional-intent command, derives the orientation intent from the effective Orientation Mode, mirrors jump state, and (optionally) rebases everything onto the standing movement base. Call it from the owning Pawn's `IMoverInputProducerInterface::ProduceInput_Implementation()`;
  - `BindInputActions(EIC, Profile)`: native-binds `InputAction.Move` (Triggered/Completed) and `InputAction.Jump` (Started/Completed). Discovered automatically by `DanzmannInput`'s apply scan -- the Pawn never references the Component to wire it up. Stance inputs (sprint/aim/crouch) are intentionally left to GAS Gameplay Ability Input Actions;
  - `SetMovementProfile(Profile)`: pushes a Movement Profile (typically from Pawn Data). Applies immediately if play has begun, otherwise on `BeginPlay()`;
  - GAS linkage: on `BeginPlay()` it resolves the owner's `UDanzmannAbilitySystemComponent` (Pawn first, then Player State) and, once initialized, subscribes to the `MovementSpeedMultiplier` Attribute -- each change rescales the live `MaxSpeed` from the cached pre-multiplier `BaseMaxSpeed`. No ASC means neutral defaults and no published Tags;
  - Runtime toggles: `SetAutoMove()`, `SetMovementDisabled()`, and `SetMovementRelativeToStandingBase()` (with matching getters).
- Orientation Mode -- a single value the Component owns and publishes:
  - The effective Orientation Mode is the active override if one is set, otherwise the Movement Profile's default (`Movement.Orientation.OrientToControl` when unset). `OrientToControl` faces the Control Rotation (camera) and strafes; `OrientToMovement` faces the movement direction. Read it with `GetOrientationMode()`, which returns the `EDanzmannOrientationMode { OrientToControl, OrientToMovement }` enum mirror of the published Tag (`BlueprintThreadSafe`, so Anim Blueprints can read it from the worker thread);
  - The Component keeps **exactly one** `Movement.Orientation.*` loose Gameplay Tag on the ASC reflecting it, and is the **sole writer** of those Tags. Drive it imperatively with `SetOrientationModeOverride(Tag)`/`ClearOrientationModeOverride()` (e.g., an aim Ability sets `OrientToControl` on activate, clears it on end) -- never by granting the Tag through a Gameplay Effect, which would break the single-Tag invariant.
- Movement Mode -- the Mover simulation owns the truth; the Component mirrors it:
  - On `OnMovementModeChanged` it maps the new mode to a `Movement.Mode.*` Tag (`Walking`/`Falling`/`Flying`/`Swimming`) and republishes exactly one such loose Tag on the ASC. Read it with `GetCurrentMovementMode()`;
  - `RequestMovementMode(Tag)` feeds a *suggested* mode into the next frame's input; the simulation stays authoritative and may resolve a different actual mode.
- Vertical Mesh Smoothing: the Mover snaps the collision shape up/down in discrete per-tick jumps over uneven terrain and stairs. Bound to `OnPostFinalize`, the Component eases the rendered mesh toward the shape height (clamped by a max-lag distance, gated to grounded state, composed with -- not clobbering -- other systems' visual offsets like crouch's `UStanceModifier`), so the character glides instead of inheriting every snap;
- `ADanzmannMoverPawnCapsule` (`APawn`, implements `IMoverInputProducerInterface`): a turnkey Mover Pawn pre-wired with a capsule root, a skeletal mesh, and a `UDanzmannMoverComponent`. `ProduceInput_Implementation()` forwards to the Component, and `SetupPlayerInputComponent()` scans for the first `IDanzmannInputProfileSourceInterface` implementer and applies its profile. Inherit from it for a ready-made Pawn, or replicate the five-step wiring (documented in its header) on any `APawn`;
- Gameplay Tags (`Danzmann::GameplayTags` namespace): `Status.Gait.{Walking, Running, Sprinting}`, `Movement.Orientation.{OrientToControl, OrientToMovement}`, and `Movement.Mode.{Walking, Falling, Flying, Swimming}`. `Status.Gait.*` is an exclusive axis (exactly one present at a time) driven by Gameplay Effects -- a baseline `GE_Walk` plus higher gaits that inhibit lower ones via Ongoing Tag Requirements (Sprint > Run > Walk). Query it with `UDanzmannMovementStatics::GetCurrentGait(ASC)`, which resolves that precedence into an `EDanzmannGait { Walk, Run, Sprint }` enum (falling back to `Walk` -- the baseline -- when the ASC is null or carries no gait Tag). The orientation and mode Tags are written exclusively by `UDanzmannMoverComponent` and are read-only for everyone else.

## Usage

The fast path is to subclass `ADanzmannMoverPawnCapsule` and assign a Movement Profile (usually via `DanzmannExperiences` Pawn Data). To wire the Component onto an existing Pawn instead:

```cpp
// 1. Implement IMoverInputProducerInterface and add the Component.
UCLASS()
class AMyPawn : public APawn, public IMoverInputProducerInterface
{
    GENERATED_BODY()

    AMyPawn(const FObjectInitializer& ObjectInitializer)
        : Super(ObjectInitializer)
    {
        MoverComponent = CreateDefaultSubobject<UDanzmannMoverComponent>(UDanzmannMoverComponent::ComponentName);
        SetReplicatingMovement(false); // Mover replicates movement itself
    }

    // 2. Feed the Mover simulation each frame
    virtual void ProduceInput_Implementation(int32 SimTimeMs, FMoverInputCmdContext& InputCmdResult) override
    {
        MoverComponent->BuildInputCmd(SimTimeMs, InputCmdResult);
    }

    UPROPERTY()
    TObjectPtr<UDanzmannMoverComponent> MoverComponent;
};

// 3. Push a Movement Profile (typically from Pawn Data)
MoverComponent->SetMovementProfile(MyMovementProfile);

// 4. Input binds automatically: the Component implements IDanzmannInputBinderInterface,
//    so DanzmannInput's apply scan calls BindInputActions() on possession. Move and Jump
//    must be mapped in the active UDanzmannInputProfile
```

Driving orientation from a Gameplay Ability (the intended pattern -- aim, for example):

```cpp
// On activate: strafe / face the camera.
MoverComponent->SetOrientationModeOverride(Danzmann::GameplayTags::Movement_Orientation_OrientToControl);

// On end: revert to the Movement Profile's default.
MoverComponent->ClearOrientationModeOverride();
```

To swap locomotion at runtime, call `SetMovementProfile(NewProfile)` again (or let `DanzmannExperiences` re-apply Pawn Data) -- speeds, turning, default orientation, and smoothing all re-seed from the new profile.

## Input-to-simulation flow at a glance

```
DanzmannInput apply scan        -> UDanzmannMoverComponent::BindInputActions()  [Move, Jump]
  └─ Input_MoveTriggered / Input_JumpStarted / ...  -> cache MoveInputIntent / jump flags
AMyPawn::ProduceInput_Implementation()  (per controlled frame)
  └─ UDanzmannMoverComponent::BuildInputCmd()
       ├─ resolve Control Rotation (PlayerController | AIController focal point)
       ├─ move input        -> velocity (NavMover / cached) or directional intent (rotated by Control Rotation)
       ├─ orientation intent -> from effective Orientation Mode (override > profile default)
       ├─ jump + suggested Movement Mode
       └─ rebase onto standing movement base (optional)
Mover simulation tick           -> authoritative Movement Mode + transform
  ├─ OnMovementModeChanged       -> publish Movement.Mode.* loose Tag on the ASC
  └─ OnPostFinalize              -> ease Skeletal Mesh toward shape height (vertical smoothing)
GAS MovementSpeedMultiplier change -> rescale CommonLegacySettings::MaxSpeed from BaseMaxSpeed
```

---

Part of the Danzmann plugin suite. See also [`DanzmannInput`](https://github.com/iVcente/DanzmannInput), [`DanzmannExperiences`](https://github.com/iVcente/DanzmannExperiences), and [`DanzmannAbilities`](https://github.com/iVcente/DanzmannAbilities).
