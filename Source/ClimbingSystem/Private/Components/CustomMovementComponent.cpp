// CloudHu:604746493@qq.com All Rights Reserved


#include "Components/CustomMovementComponent.h"

#include "MotionWarpingComponent.h"
#include "ClimbingSystem/ClimbingSystemCharacter.h"
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

FHitResult UCustomMovementComponent::DoLineTraceSingleForObject(const FVector& Start, const FVector& End, bool bShowDebugShape, bool bDrawPersistantShapes) const
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

FHitResult UCustomMovementComponent::TraceFromEyeHeight(const float TraceDistance, const float TraceStartOffset, const bool bShowDebugShape,
                                                        const bool bDrawPersistantShapes) const
{
	const FVector ComponentLocation = UpdatedComponent->GetComponentLocation();
	const FVector EyeHeightOffset = UpdatedComponent->GetUpVector() * (CharacterOwner->BaseEyeHeight + TraceStartOffset);
	const FVector Start = ComponentLocation + EyeHeightOffset;
	const FVector End = Start + UpdatedComponent->GetForwardVector() * TraceDistance;
	return DoLineTraceSingleForObject(Start, End, bShowDebugShape, bDrawPersistantShapes);
}

bool UCustomMovementComponent::CanStartClimbing()
{
	if (IsFalling())return false;
	if (!TraceClimbableSurfaces())return false;
	if (!TraceFromEyeHeight(100.f).bBlockingHit)return false;
	return true;
}

bool UCustomMovementComponent::CanClimbDownLedge() const
{
	if (IsFalling())return false;
	const FVector ComponentLocation = UpdatedComponent->GetComponentLocation();
	const FVector DirectionForward = UpdatedComponent->GetForwardVector();
	const FVector DownVector = -UpdatedComponent->GetUpVector();
	const FVector WalkableSurfaceTraceStart = ComponentLocation + DirectionForward * ClimbDownWalkableSurfaceTraceOffset;
	const FVector WalkableSurfaceTraceEnd = WalkableSurfaceTraceStart + DownVector * 100.f;
	if (const FHitResult HitResult = DoLineTraceSingleForObject(WalkableSurfaceTraceStart, WalkableSurfaceTraceEnd); HitResult.bBlockingHit)
	{
		const FVector LedgeTraceStart = HitResult.ImpactPoint + DirectionForward * ClimbDownLedgeTraceOffset;
		const FVector LedgeTraceEnd = LedgeTraceStart + DownVector * 200.f;
		if (const FHitResult LedgeHit = DoLineTraceSingleForObject(LedgeTraceStart, LedgeTraceEnd); !LedgeHit.bBlockingHit)
		{
			return true;
		}
	}
	return false;
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
	if (CheckShouldStopClimbing() || CheckHasReachedFloor())
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

	if (CheckHasReachedLedge())
	{
		//Debug::Print("Ledge Reached", FColor::Green, 1);
		PlayClimbMontage(ClimbToTopMontage);
	}
	// else
	// {
	// 	Debug::Print("Ledge NOT Reached", FColor::Red, 1);
	// }
}

FVector UCustomMovementComponent::ConstrainAnimRootMotionVelocity(const FVector& RootMotionVelocity, const FVector& CurrentVelocity) const
{
	if (IsFalling() && OwningPlayerAnimInstance && OwningPlayerAnimInstance->IsAnyMontagePlaying())
	{
		return RootMotionVelocity;
	}
	return Super::ConstrainAnimRootMotionVelocity(RootMotionVelocity, CurrentVelocity);
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
	if (const float DegreeDiff = FMath::RadiansToDegrees(FMath::Acos(DotResult)); DegreeDiff <= 60.f)return true;
	// Debug::Print("Climb, DegreeDiff: %f", DegreeDiff, FColor::Green, 1);
	return false;
}

bool UCustomMovementComponent::CheckHasReachedFloor() const
{
	//如果角色往上爬，则不需要检测地面
	if (GetUnrotatedClimbVelocity().Z > -10.f)return false;
	//获取角色朝下的向量（向上的向量取反）
	const FVector DownVector = -UpdatedComponent->GetUpVector();
	//起点偏移
	const FVector StartOffset = DownVector * 80.f;
	//计算起止点
	const FVector Start = UpdatedComponent->GetComponentLocation() + StartOffset;
	const FVector End = Start + DownVector;

	//进行碰撞检测，找到可能存在的地面
	TArray<FHitResult> PossibleFloorHits = DoCapsuleTraceMultiForObjects(Start, End);
	if (PossibleFloorHits.IsEmpty())return false;

	for (const FHitResult& HitResult : PossibleFloorHits)
	{
		if (FVector::Parallel(-HitResult.ImpactNormal, FVector::UpVector))
		{
			return true;
		}
	}
	return false;
}

bool UCustomMovementComponent::CheckHasReachedLedge() const
{
	//如果是往下爬，则不需要检测是否爬到顶部边缘
	// Debug::Print(TEXT("GetUnrotatedClimbVelocity.Z = %f"), GetUnrotatedClimbVelocity().Z, FColor::Blue, 2);
	if (GetUnrotatedClimbVelocity().Z < 10.f)return false;

	//先检测角色眼部往上偏移50高度往正前方100距离是否有碰撞
	if (const FHitResult LedgeHit = TraceFromEyeHeight(100.f, 50.f); !LedgeHit.bBlockingHit)
	{
		//如果上前方没有碰撞，则由末端再折下检测是否有可行走的地面
		const FVector WalkableSurfaceTraceStart = LedgeHit.TraceEnd;
		const FVector DownVector = -UpdatedComponent->GetUpVector();
		const FVector WalkableSurfaceTraceEnd = WalkableSurfaceTraceStart + DownVector * 100.f;

		if (const FHitResult WalkableSurfaceHit = DoLineTraceSingleForObject(WalkableSurfaceTraceStart, WalkableSurfaceTraceEnd); WalkableSurfaceHit.bBlockingHit)
		{
			return true;
		}
	}
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

void UCustomMovementComponent::TryStartVaulting()
{
	FVector VaultLandPosition;
	if (FVector VaultStartPosition; CanStartVaulting(VaultStartPosition, VaultLandPosition))
	{
		//Start Vaulting
		SetMotionWarpingTarget(FName("VaultStartPoint"), VaultStartPosition);
		SetMotionWarpingTarget(FName("VaultLandPoint"), VaultLandPosition);
		StartClimbing();
		PlayClimbMontage(VaultingMontage);
	}
}

bool UCustomMovementComponent::CanStartVaulting(FVector& OutVaultStartPosition, FVector& OutVaultLandPosition) const
{
	if (IsFalling())return false;

	OutVaultStartPosition = FVector::ZeroVector;
	OutVaultLandPosition = FVector::ZeroVector;

	const FVector ComponentForward = UpdatedComponent->GetForwardVector();
	const FVector ComponentUp = UpdatedComponent->GetUpVector();
	const FVector ComponentDown = -ComponentUp;
	const FVector ComponentLocation = UpdatedComponent->GetComponentLocation();

	for (int i = 0; i < 5; ++i)
	{
		const FVector Start = ComponentLocation + ComponentUp * 100.f + ComponentForward * 80.f * (i + 1);
		const FVector End = Start + ComponentDown * 100.f * (i + 1);

		const FHitResult TraceHit = DoLineTraceSingleForObject(Start, End);
		if (i == 0 && TraceHit.bBlockingHit)
		{
			OutVaultStartPosition = TraceHit.ImpactPoint;
		}

		if (i == 4 && TraceHit.bBlockingHit)
		{
			OutVaultLandPosition = TraceHit.ImpactPoint;
		}
	}
	if (!OutVaultStartPosition.IsZero() && !OutVaultLandPosition.IsZero())
	{
		return true;
	}
	return false;
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
	if (MontageToPlay == IdleToClimbMontage || MontageToPlay == ClimbDownLedgeMontage)
	{
		StartClimbing();
		StopMovementImmediately();
	}
	else if (MontageToPlay == ClimbToTopMontage || MontageToPlay == VaultingMontage)
	{
		SetMovementMode(MOVE_Walking);
	}
}

void UCustomMovementComponent::SetMotionWarpingTarget(const FName& InWarpingTargetName, const FVector& InTargetPosition) const
{
	if (!OwningPlayerCharacter)return;
	OwningPlayerCharacter->GetMotionWarpingComponent()->AddOrUpdateWarpTargetFromLocation(InWarpingTargetName, InTargetPosition);
}

void UCustomMovementComponent::HandleHopUp() const
{
	if (FVector TargetPosition; CheckCanHopUp(TargetPosition))
	{
		SetMotionWarpingTarget(FName("HopUpTargetPoint"), TargetPosition);
		PlayClimbMontage(HopUpMontage);
	}
}

bool UCustomMovementComponent::CheckCanHopUp(FVector& OutTargetPosition) const
{
	if (const FHitResult HitResult = TraceFromEyeHeight(100.f, -20.f); HitResult.bBlockingHit)
	{
		OutTargetPosition = HitResult.ImpactPoint;
		return true;
		// if (const FHitResult SafetyLedgeHitResult = TraceFromEyeHeight(100.f, 150.f); SafetyLedgeHitResult.bBlockingHit)
		// {
		// 	OutTargetPosition = HitResult.ImpactPoint;
		// 	return true;
		// }
	}

	return false;
}

void UCustomMovementComponent::HandleHopDown() const
{
	if (FVector TargetPosition; CheckCanHopDown(TargetPosition))
	{
		SetMotionWarpingTarget(FName("HopDownTargetPoint"), TargetPosition);
		PlayClimbMontage(HopDownMontage);
	}
}

bool UCustomMovementComponent::CheckCanHopDown(FVector& OutTargetPosition) const
{
	if (const FHitResult HitResult = TraceFromEyeHeight(100.f, -300.f); HitResult.bBlockingHit)
	{
		OutTargetPosition = HitResult.ImpactPoint;
		return true;
	}
	return false;
}

void UCustomMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	if (IsClimbing())
	{
		bOrientRotationToMovement = false;
		CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(48.f);

		OnEnterClimbStateDelegate.ExecuteIfBound();
	}

	if (PreviousMovementMode == MOVE_Custom && PreviousCustomMode == ECustomMovementMode::MOVE_Climb)
	{
		bOrientRotationToMovement = true;
		CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(96.f);

		const FRotator DirtyRotation = UpdatedComponent->GetComponentRotation();
		const FRotator CleanStandRotation = FRotator(0.f, DirtyRotation.Yaw, 0.f);
		UpdatedComponent->SetRelativeRotation(CleanStandRotation);

		StopMovementImmediately();

		OnExitClimbStateDelegate.ExecuteIfBound();
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

	OwningPlayerCharacter = Cast<AClimbingSystemCharacter>(CharacterOwner);
}

void UCustomMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	// TraceClimbableSurfaces();
	// CanClimbDownLedge();
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
		else if (CanClimbDownLedge())
		{
			//Debug::Print("Can start climb down!", FColor::Cyan, 1);
			PlayClimbMontage(ClimbDownLedgeMontage);
		}
		else
		{
			TryStartVaulting();
		}
	}
	else
	{
		// Exit the climb state
		StopClimbing();
	}
}

void UCustomMovementComponent::RequestHopping()
{
	const FVector UnrotatedLastInputVector = UKismetMathLibrary::Quat_UnrotateVector(UpdatedComponent->GetComponentQuat(), GetLastInputVector());

	if (const float DotResult = FVector::DotProduct(UnrotatedLastInputVector.GetSafeNormal(), FVector::UpVector); DotResult >= 0.9f)
	{
		//Hop Up
		HandleHopUp();
	}
	else if (DotResult <= -0.9)
	{
		//Hop Down
		HandleHopDown();
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
