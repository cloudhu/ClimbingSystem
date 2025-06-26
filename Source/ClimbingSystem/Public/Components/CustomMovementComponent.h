// CloudHu:604746493@qq.com All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CustomMovementComponent.generated.h"

DECLARE_DELEGATE(FOnEnterClimbState)
DECLARE_DELEGATE(FOnExitClimbState)

class AClimbingSystemCharacter;
// class UAnimMontage;
UENUM()
namespace ECustomMovementMode
{
	enum Type
	{
		MOVE_Climb UMETA(DisplayName="Climb Mode")
	};
}

/**
 * 
 */
UCLASS()
class CLIMBINGSYSTEM_API UCustomMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	FOnEnterClimbState OnEnterClimbStateDelegate;
	FOnExitClimbState OnExitClimbStateDelegate;

private:
#pragma region ClimbTraces
	TArray<FHitResult> DoCapsuleTraceMultiForObjects(const FVector& Start, const FVector& End, bool bShowDebugShape = false, bool bDrawPersistantShapes = false) const;

	FHitResult DoLineTraceSingleForObject(const FVector& Start, const FVector& End, bool bShowDebugShape = false, bool bDrawPersistantShapes = false) const;

#pragma endregion

#pragma region ClimbCore
	bool TraceClimbableSurfaces();

	FHitResult TraceFromEyeHeight(float TraceDistance, float TraceStartOffset = 0.f, bool bShowDebugShape = false, bool bDrawPersistantShapes = false) const;

	bool CanStartClimbing();

	bool CanClimbDownLedge() const;

	void StartClimbing();

	void PhysClimb(float DeltaTime, int32 Iterations);

	void StopClimbing();

	void ProcessClimbableSurfaceInfo();

	bool CheckShouldStopClimbing() const;

	bool CheckHasReachedFloor() const;

	bool CheckHasReachedLedge() const;

	FQuat GetClimbRotation(float DeltaTime) const;

	void SnapMovementToClimbableSurfaces(float DeltaTime) const;

	void TryStartVaulting();

	bool CanStartVaulting(FVector& OutVaultStartPosition, FVector& OutVaultLandPosition) const;

	void PlayClimbMontage(UAnimMontage* MontageToPlay) const;

	UFUNCTION()
	void OnClimMontageEnded(UAnimMontage* MontageToPlay, bool bInterrupted);

	void SetMotionWarpingTarget(const FName& InWarpingTargetName, const FVector& InTargetPosition) const;

	void HandleHopUp() const;

	bool CheckCanHopUp(FVector& OutTargetPosition) const;

	void HandleHopDown() const;

	bool CheckCanHopDown(FVector& OutTargetPosition) const;

#pragma endregion

#pragma region ClimbCoreVars

	TArray<FHitResult> ClimbableSurfacesTracedResults;

	FVector CurrentClimbableSurfaceLocation;

	FVector CurrentClimbableSurfaceNormal;

	UPROPERTY()
	UAnimInstance* OwningPlayerAnimInstance;

	UPROPERTY()
	AClimbingSystemCharacter* OwningPlayerCharacter;
#pragma endregion

#pragma region OverrideFuctions

public:
	virtual FVector ConstrainAnimRootMotionVelocity(const FVector& RootMotionVelocity, const FVector& CurrentVelocity) const override;

protected:
	virtual void PhysCustom(float deltaTime, int32 Iterations) override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

	virtual float GetMaxSpeed() const override;

	virtual float GetMaxAcceleration() const override;

	virtual void BeginPlay() override;

#pragma endregion

public:
	void ToggleClimbing(bool bEnableClimb);

	void RequestHopping();

	bool IsClimbing() const;

	FORCEINLINE FVector GetClimbableSurfaceNormal() const { return CurrentClimbableSurfaceNormal; }

	FVector GetUnrotatedClimbVelocity() const;

#pragma region BlueprintVars

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CharacterMovement:Climbing", meta=(AllowPrivateAccess=true))
	TArray<TEnumAsByte<EObjectTypeQuery>> ClimbableSurfaceTraceTypes;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CharacterMovement:Climbing", meta=(AllowPrivateAccess=true))
	float ClimbCapsuleTraceRadius = 50.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CharacterMovement:Climbing", meta=(AllowPrivateAccess=true))
	float ClimbCapsuleTraceHalfHeight = 72.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CharacterMovement:Climbing", meta=(AllowPrivateAccess=true))
	float MaxBreakClimbDeceleration = 400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CharacterMovement:Climbing", meta=(AllowPrivateAccess=true))
	float MaxClimbSpeed = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CharacterMovement:Climbing", meta=(AllowPrivateAccess=true))
	float MaxClimbMaxAcceleration = 300.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CharacterMovement:Climbing", meta=(AllowPrivateAccess=true))
	float ClimbDownWalkableSurfaceTraceOffset = 50.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CharacterMovement:Climbing", meta=(AllowPrivateAccess=true))
	float ClimbDownLedgeTraceOffset = 50.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CharacterMovement:Climbing", meta=(AllowPrivateAccess=true))
	UAnimMontage* IdleToClimbMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CharacterMovement:Climbing", meta=(AllowPrivateAccess=true))
	UAnimMontage* ClimbToTopMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CharacterMovement:Climbing", meta=(AllowPrivateAccess=true))
	UAnimMontage* ClimbDownLedgeMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CharacterMovement:Climbing", meta=(AllowPrivateAccess=true))
	UAnimMontage* VaultingMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CharacterMovement:Climbing", meta=(AllowPrivateAccess=true))
	UAnimMontage* HopUpMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CharacterMovement:Climbing", meta=(AllowPrivateAccess=true))
	UAnimMontage* HopDownMontage;


#pragma endregion
};
