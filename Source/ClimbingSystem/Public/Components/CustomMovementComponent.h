// CloudHu:604746493@qq.com All Rights Reserved

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CustomMovementComponent.generated.h"

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

#pragma region ClimbTraces
	TArray<FHitResult> DoCapsuleTraceMultiForObjects(const FVector& Start, const FVector& End, bool bShowDebugShape = false, bool bDrawPersistantShapes = false) const;

	FHitResult DoLineTraceSingleForObject(const FVector& Start, const FVector& End, bool bShowDebugShape = false, bool bDrawPersistantShapes = false) const;

#pragma endregion

#pragma region ClimbCore
	bool TraceClimbableSurfaces();

	FHitResult TraceFromEyeHeight(float TraceDistance, float TraceStartOffset = 0.f) const;

	bool CanStartClimbing();

	void StartClimbing();

	void PhysClimb(float DeltaTime, int32 Iterations);

	void StopClimbing();

	void ProcessClimbableSurfaceInfo();

	bool CheckShouldStopClimbing() const;
	
	bool CheckHasReachedFloor() const;
	
	bool CheckHasReachedLedge() const;

	FQuat GetClimbRotation(float DeltaTime) const;

	void SnapMovementToClimbableSurfaces(float DeltaTime) const;

	void PlayClimbMontage(UAnimMontage* MontageToPlay) const;

	UFUNCTION()
	void OnClimMontageEnded(UAnimMontage* MontageToPlay,bool bInterrupted);
#pragma endregion

#pragma region ClimbCoreVars

	TArray<FHitResult> ClimbableSurfacesTracedResults;

	FVector CurrentClimbableSurfaceLocation;

	FVector CurrentClimbableSurfaceNormal;

	UPROPERTY()
	UAnimInstance* OwningPlayerAnimInstance;
	
#pragma endregion

#pragma region OverrideFuctions

protected:
	virtual void PhysCustom(float deltaTime, int32 Iterations) override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

	virtual float GetMaxSpeed() const override;

	virtual float GetMaxAcceleration() const override;

public:
	virtual void BeginPlay() override;

protected:
#pragma endregion

public:
	void ToggleClimbing(bool bEnableClimb);

	bool IsClimbing() const;

	FORCEINLINE FVector GetClimbableSurfaceNormal() const { return CurrentClimbableSurfaceNormal; }

	FVector GetUnrotatedClimbVelocity()const;
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
	UAnimMontage* IdleToClimbMontage;
	
#pragma endregion
};
