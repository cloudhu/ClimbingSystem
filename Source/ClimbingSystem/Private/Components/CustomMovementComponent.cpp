// CloudHu:604746493@qq.com All Rights Reserved


#include "Components/CustomMovementComponent.h"

#include "ClimbingSystem/ClimbingSystemCharacter.h"
#include "ClimbingSystem/DebugHelper.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

TArray<FHitResult> UCustomMovementComponent::DoCapsuleTraceMultiForObjects(const FVector& Start, const FVector& End, const bool bShowDebugShape,
                                                                           bool bDrawPersistantShapes) const
{
	TArray<FHitResult> OutHitResults;

	EDrawDebugTrace::Type DebugType = EDrawDebugTrace::None;
	if (bShowDebugShape)
	{
		DebugType = EDrawDebugTrace::ForOneFrame;
		if (bDrawPersistantShapes)
		{
			DebugType = EDrawDebugTrace::Persistent;
		}
	}


	UKismetSystemLibrary::CapsuleTraceMultiForObjects(
		this, Start, End, ClimbCapsuleTraceRadius, ClimbCapsuleTraceHalfHeight,
		ClimbableSurfaceTraceTypes,
		false,
		TArray<AActor*>(),
		DebugType,
		OutHitResults,
		false
	);
	return OutHitResults;
}

FHitResult UCustomMovementComponent::DoLineTraceMultiForObject(const FVector& Start, const FVector& End, bool bShowDebugShape, bool bDrawPersistantShapes) const
{
	FHitResult OutHitResult;

	EDrawDebugTrace::Type DebugType = EDrawDebugTrace::None;
	if (bShowDebugShape)
	{
		DebugType = EDrawDebugTrace::ForOneFrame;
		if (bDrawPersistantShapes)
		{
			DebugType = EDrawDebugTrace::Persistent;
		}
	}

	UKismetSystemLibrary::LineTraceSingleForObjects(
		this, Start, End,
		ClimbableSurfaceTraceTypes,
		false,
		TArray<AActor*>(),
		DebugType,
		OutHitResult,
		false
	);
	return OutHitResult;
}

/**
 * 通过碰撞检测场景中是否检测到可攀爬
 * @return 如果有可攀爬的表面返回true，否则返回false
 */
bool UCustomMovementComponent::TraceClimbableSurfaces()
{
	const FVector StartOffset = UpdatedComponent->GetForwardVector() * 30.f;
	const FVector Start = UpdatedComponent->GetComponentLocation() + StartOffset;
	const FVector End = Start + UpdatedComponent->GetForwardVector();
	ClimbableSurfacesTracedResults = DoCapsuleTraceMultiForObjects(Start, End);
	// return !ClimbableSurfacesTracedResults.IsEmpty();
	return ClimbableSurfacesTracedResults.Num() > 0;
}

FHitResult UCustomMovementComponent::TraceFromEyeHeight(const float TraceDistance, const float TraceStartOffset) const
{
	const FVector ComponentLocation = UpdatedComponent->GetComponentLocation();
	const FVector EyeHeightOffset = UpdatedComponent->GetUpVector() * (CharacterOwner->BaseEyeHeight + TraceStartOffset);
	const FVector Start = ComponentLocation + EyeHeightOffset;
	const FVector End = Start + UpdatedComponent->GetForwardVector() * TraceDistance;
	return DoLineTraceMultiForObject(Start, End);
}

bool UCustomMovementComponent::CanStartClimbing()
{
	if (IsFalling())return false;
	if (!TraceClimbableSurfaces())return false;
	if (!TraceFromEyeHeight(100.f).bBlockingHit)return false;
	return true;
}

void UCustomMovementComponent::StartClimbing()
{
	SetMovementMode(MOVE_Custom, ECustomMovementMode::MOVE_Climb);
}

void UCustomMovementComponent::PhysClimb(const float DeltaTime, int32 Iterations)
{
	if (DeltaTime < MIN_TICK_TIME)
	{
		return;
	}
	//处理所有的可攀爬表面信息
	TraceClimbableSurfaces();

	ProcessClimbableSurfaceInfo();
	//检查是否应该停止攀爬
	if (CheckShouldStopClimbing())
	{
		StopClimbing();
	}

	RestorePreAdditiveRootMotionVelocity();

	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		//定义最大攀爬速度和加速度

		CalcVelocity(DeltaTime, 0.f, true, MaxBreakClimbDeceleration);
	}

	ApplyRootMotionToVelocity(DeltaTime);

	const FVector OldLocation = UpdatedComponent->GetComponentLocation();
	const FVector Adjusted = Velocity * DeltaTime;
	FHitResult Hit(1.f);

	//处理攀爬时的旋转
	SafeMoveUpdatedComponent(Adjusted, GetClimbRotation(DeltaTime), true, Hit);

	if (Hit.Time < 1.f)
	{
		HandleImpact(Hit, DeltaTime, Adjusted);
		SlideAlongSurface(Adjusted, (1.f - Hit.Time), Hit.Normal, Hit, true);
	}

	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaTime;
	}

	//使攀爬时的动作贴合可攀爬物体表面
	SnapMovementToClimbableSurfaces(DeltaTime);
}

void UCustomMovementComponent::PhysCustom(float deltaTime, int32 Iterations)
{
	if (IsClimbing())
	{
		PhysClimb(deltaTime, Iterations);
	}
	Super::PhysCustom(deltaTime, Iterations);
}

void UCustomMovementComponent::StopClimbing()
{
	SetMovementMode(MOVE_Falling);
}

void UCustomMovementComponent::ProcessClimbableSurfaceInfo()
{
	CurrentClimbableSurfaceLocation = FVector::ZeroVector;
	CurrentClimbableSurfaceNormal = FVector::ZeroVector;

	if (ClimbableSurfacesTracedResults.IsEmpty())return;

	for (const FHitResult& HitResult : ClimbableSurfacesTracedResults)
	{
		CurrentClimbableSurfaceLocation += HitResult.ImpactPoint;
		CurrentClimbableSurfaceNormal += HitResult.ImpactNormal;
	}

	CurrentClimbableSurfaceLocation /= ClimbableSurfacesTracedResults.Num();
	CurrentClimbableSurfaceNormal = CurrentClimbableSurfaceNormal.GetSafeNormal();
}

bool UCustomMovementComponent::CheckShouldStopClimbing() const
{
	if (ClimbableSurfacesTracedResults.IsEmpty())return true;

	//计算可攀爬表面与向上方向的点乘，从而判断角色和水平面的相对夹角，当角色和水平面平行时角度为0，当角色垂直时角度为90
	const float DotResult = FVector::DotProduct(CurrentClimbableSurfaceNormal, FVector::UpVector);
	const float DegreeDiff = FMath::RadiansToDegrees(FMath::Acos(DotResult));
	if (DegreeDiff <= 60.f)return true;
	// Debug::Print("Climb, DegreeDiff: %f", DegreeDiff, FColor::Green, 1);
	return false;
}

FQuat UCustomMovementComponent::GetClimbRotation(const float DeltaTime) const
{
	const FQuat CurrentQuat = UpdatedComponent->GetComponentQuat();
	if (HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocity())
	{
		return CurrentQuat;
	}
	const FQuat TargetQuat = FRotationMatrix::MakeFromX(-CurrentClimbableSurfaceNormal).ToQuat();

	return FMath::QInterpTo(CurrentQuat, TargetQuat, DeltaTime, 5.f);
}

void UCustomMovementComponent::SnapMovementToClimbableSurfaces(const float DeltaTime) const
{
	const FVector ComponentForward = UpdatedComponent->GetForwardVector();
	const FVector ComponentLocation = UpdatedComponent->GetComponentLocation();
	const FVector ProjectedCharacterToSurface = (CurrentClimbableSurfaceLocation - ComponentLocation).ProjectOnTo(ComponentForward);
	const FVector SnapVector = -CurrentClimbableSurfaceNormal * ProjectedCharacterToSurface.Length();
	UpdatedComponent->MoveComponent(SnapVector * DeltaTime * MaxClimbSpeed, UpdatedComponent->GetComponentQuat(), true);
}

void UCustomMovementComponent::PlayClimbMontage(UAnimMontage* MontageToPlay) const
{
	if (!MontageToPlay || !OwningPlayerAnimInstance)return;
	if (OwningPlayerAnimInstance->IsAnyMontagePlaying())return;

	OwningPlayerAnimInstance->Montage_Play(MontageToPlay);
}

void UCustomMovementComponent::OnClimMontageEnded(UAnimMontage* MontageToPlay, bool bInterrupted)
{
	// Debug::Print("Climb Montage ended");
	if (MontageToPlay == IdleToClimbMontage)
	{
		StartClimbing();
	}
}

void UCustomMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	if (IsClimbing())
	{
		bOrientRotationToMovement = false;
		CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(48.f);
	}

	if (PreviousMovementMode == MOVE_Custom && PreviousCustomMode == ECustomMovementMode::MOVE_Climb)
	{
		bOrientRotationToMovement = true;
		CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(96.f);

		const FRotator DirtyRotation = UpdatedComponent->GetComponentRotation();
		const FRotator CleanStandRotation = FRotator(0.f, DirtyRotation.Yaw, 0.f);
		UpdatedComponent->SetRelativeRotation(CleanStandRotation);

		StopMovementImmediately();
	}

	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
}

float UCustomMovementComponent::GetMaxSpeed() const
{
	if (IsClimbing())
	{
		return MaxClimbSpeed;
	}
	return Super::GetMaxSpeed();
}

float UCustomMovementComponent::GetMaxAcceleration() const
{
	if (IsClimbing())return MaxClimbMaxAcceleration;
	return Super::GetMaxAcceleration();
}

void UCustomMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	OwningPlayerAnimInstance = CharacterOwner->GetMesh()->GetAnimInstance();
	if (OwningPlayerAnimInstance)
	{
		OwningPlayerAnimInstance->OnMontageEnded.AddDynamic(this, &UCustomMovementComponent::OnClimMontageEnded);
		OwningPlayerAnimInstance->OnMontageBlendingOut.AddDynamic(this, &UCustomMovementComponent::OnClimMontageEnded);
	}
}

void UCustomMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	// TraceClimbableSurfaces();
}

void UCustomMovementComponent::ToggleClimbing(bool bEnableClimb)
{
	if (bEnableClimb)
	{
		if (CanStartClimbing())
		{
			//Enter the climb state
			// Debug::Print("Can start climb");
			PlayClimbMontage(IdleToClimbMontage);
		}
		// else
		// {
		// 	Debug::Print("Can't start climb");
		// }
	}
	else
	{
		// Exit the climb state
		StopClimbing();
	}
}

bool UCustomMovementComponent::IsClimbing() const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == ECustomMovementMode::MOVE_Climb;
}

FVector UCustomMovementComponent::GetUnrotatedClimbVelocity() const
{
	return UKismetMathLibrary::Quat_UnrotateVector(UpdatedComponent->GetComponentQuat(), Velocity);
}
