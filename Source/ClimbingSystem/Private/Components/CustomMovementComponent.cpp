// CloudHu:604746493@qq.com All Rights Reserved


#include "Components/CustomMovementComponent.h"

#include "ClimbingSystem/ClimbingSystemCharacter.h"
#include "Kismet/KismetSystemLibrary.h"

TArray<FHitResult> UCustomMovementComponent::DoCapsuleTraceMultiForObjects(const FVector& Start, const FVector& End, const bool bShowDebugShape) const
{
	TArray<FHitResult> OutHitResults;
	UKismetSystemLibrary::CapsuleTraceMultiForObjects(
		this, Start, End, ClimbCapsuleTraceRadius, ClimbCapsuleTraceHalfHeight,
		ClimbableSurfaceTraceTypes,
		false,
		TArray<AActor*>(),
		bShowDebugShape ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None,
		OutHitResults,
		false
	);
	return OutHitResults;
}

FHitResult UCustomMovementComponent::DoLineTraceMultiForObject(const FVector& Start, const FVector& End, bool bShowDebugShape) const
{
	FHitResult OutHitResult;
	UKismetSystemLibrary::LineTraceSingleForObjects(
		this, Start, End,
		ClimbableSurfaceTraceTypes,
		false,
		TArray<AActor*>(),
		bShowDebugShape ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None,
		OutHitResult,
		false
	);
	return OutHitResult;
}

void UCustomMovementComponent::TraceClimbableSurfaces() const
{
	const FVector StartOffset = UpdatedComponent->GetForwardVector() * 30.f;
	const FVector Start = UpdatedComponent->GetComponentLocation() + StartOffset;
	const FVector End = Start + UpdatedComponent->GetForwardVector();
	const TArray<FHitResult> HitResults = DoCapsuleTraceMultiForObjects(Start, End, true);
}

void UCustomMovementComponent::TraceFromEyeHeight(const float TraceDistance, const float TraceStartOffset) const
{
	const FVector ComponentLocation = UpdatedComponent->GetComponentLocation();
	const FVector EyeHeightOffset = UpdatedComponent->GetUpVector() * (CharacterOwner->BaseEyeHeight + TraceStartOffset);
	const FVector Start = ComponentLocation + EyeHeightOffset;
	const FVector End = Start + UpdatedComponent->GetForwardVector() * TraceDistance;
	const FHitResult HitResult = DoLineTraceMultiForObject(Start, End, true);
}

void UCustomMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	TraceClimbableSurfaces();
	TraceFromEyeHeight(100.f);
}
