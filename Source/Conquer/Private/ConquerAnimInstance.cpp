#include "ConquerAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"
#include "BonePose.h"
#include "TwoBoneIK.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
	const FName SlotName(TEXT("DefaultSlot"));

	void CopyPose(const FPoseContext& From, FPoseContext& To)
	{
		To.Pose = From.Pose;
		To.Curve = From.Curve;
		To.CustomAttributes = From.CustomAttributes;
	}

	bool AllSet(const TArray<TObjectPtr<UAnimSequence>>& Clips, int32 Count)
	{
		if (Clips.Num() < Count)
		{
			return false;
		}
		for (int32 i = 0; i < Count; ++i)
		{
			if (!Clips[i])
			{
				return false;
			}
		}
		return true;
	}

	/** Fades a one-shot in over its first moment and out over its last. */
	float FadeWeight(float Time, float Length, float FadeOutAt)
	{
		const float In = FMath::Clamp(Time / 0.12f, 0.f, 1.f);
		const float Out = FMath::Clamp((FMath::Min(Length, FadeOutAt) - Time) / 0.2f, 0.f, 1.f);
		return FMath::Min(In, Out);
	}
}

/** Does on the animation worker thread what a Blueprint graph would: sample, blend, then layer the montage slot. */
struct FConquerAnimInstanceProxy : public FAnimInstanceProxy
{
	explicit FConquerAnimInstanceProxy(UAnimInstance* InInstance) : FAnimInstanceProxy(InInstance) {}

	virtual void Initialize(UAnimInstance* InInstance) override
	{
		FAnimInstanceProxy::Initialize(InInstance);
		RegisterSlotNodeWithAnimInstance(SlotName);
	}

	virtual void PreUpdate(UAnimInstance* InInstance, float DeltaSeconds) override
	{
		FAnimInstanceProxy::PreUpdate(InInstance, DeltaSeconds);
		const UConquerAnimInstance* Instance = CastChecked<UConquerAnimInstance>(InInstance);
		Idle = Instance->Idle;

		// Eight ways round, clockwise from forward: F, FR, R, BR, B, BL, L, FL. Without the diagonals, four.
		bEightWay = AllSet(Instance->WalkDiagonals, 4) && AllSet(Instance->JogDiagonals, 4);
		if (bEightWay)
		{
			Walk = { Instance->Walk, Instance->WalkDiagonals[0], Instance->WalkRight, Instance->WalkDiagonals[1],
				Instance->WalkBack, Instance->WalkDiagonals[2], Instance->WalkLeft, Instance->WalkDiagonals[3] };
			Jog = { Instance->Jog, Instance->JogDiagonals[0], Instance->JogRight, Instance->JogDiagonals[1],
				Instance->JogBack, Instance->JogDiagonals[2], Instance->JogLeft, Instance->JogDiagonals[3] };
		}
		else
		{
			Walk = { Instance->Walk, Instance->WalkRight, Instance->WalkBack, Instance->WalkLeft };
			Jog = { Instance->Jog, Instance->JogRight, Instance->JogBack, Instance->JogLeft };
		}
		WalkStart = Instance->WalkStart;
		JogStart = Instance->JogStart;
		WalkStops = AllSet(Instance->WalkStops, 8) ? Instance->WalkStops : TArray<TObjectPtr<UAnimSequence>>();
		JogStops = AllSet(Instance->JogStops, 8) ? Instance->JogStops : TArray<TObjectPtr<UAnimSequence>>();
		IdleBreaks = Instance->IdleBreaks;
		Turn = Instance->TurnElapsed >= 0.f && Instance->Turns.IsValidIndex(Instance->TurnClip) ? Instance->Turns[Instance->TurnClip].Get() : nullptr;
		TurnTime = Instance->TurnElapsed;

		WalkSpeed = FMath::Max(Instance->WalkSpeed, 1.f);
		JogSpeed = FMath::Max(Instance->JogSpeed, WalkSpeed + 1.f);
		Speed = Instance->GetGroundSpeed();
		Direction = Instance->GetMoveDirection();
		bAccelerating = Instance->bAccelerating;
		BodyYawOffset = Instance->BodyYawOffset;
		FootOffsetL = Instance->FootOffsetL;
		FootOffsetR = Instance->FootOffsetR;
	}

	virtual void Update(float DeltaSeconds) override
	{
		// Idle fades into walk up to walking speed, walk into jog up to jogging speed.
		MoveAlpha = FMath::Clamp(Speed / WalkSpeed, 0.f, 1.f);
		JogAlpha = FMath::Clamp((Speed - WalkSpeed) / (JogSpeed - WalkSpeed), 0.f, 1.f);

		if (Idle)
		{
			IdleTime = FMath::Fmod(IdleTime + DeltaSeconds, FMath::Max(Idle->GetPlayLength(), UE_KINDA_SMALL_NUMBER));
		}

		// Walk and jog share one phase so their footfalls stay in step while blending; the rate follows the real speed.
		const float CycleLength = FMath::Lerp(Length(Walk[0]), Length(Jog[0]), JogAlpha);
		const float NaturalSpeed = FMath::Lerp(WalkSpeed, JogSpeed, JogAlpha);
		const float Rate = Speed > 1.f ? FMath::Clamp(Speed / NaturalSpeed, 0.5f, 2.f) : 1.f;
		if (CycleLength > UE_KINDA_SMALL_NUMBER)
		{
			MovePhase = FMath::Fmod(MovePhase + DeltaSeconds * Rate / CycleLength, 1.f);
		}

		GetSlotWeight(SlotName, SlotWeight, SourceWeight, TotalWeight);
		UpdateSlotNodeWeight(SlotName, SlotWeight, 1.f);

		UpdateOneShot(DeltaSeconds);
		PrevSpeed = Speed;
		bWasAccelerating = bAccelerating;
	}

	virtual bool Evaluate(FPoseContext& Output) override
	{
		FPoseContext Locomotion(this);
		EvaluateLocomotion(Locomotion);

		// A start, stop or idle break over the loops.
		if (OneShot && OneShotWeight > ZERO_ANIMWEIGHT_THRESH)
		{
			FPoseContext Loops(this);
			CopyPose(Locomotion, Loops);
			FPoseContext Shot(this);
			Sample(OneShot, OneShotTime, Shot);
			Blend(Loops, Shot, 1.f - OneShotWeight, Locomotion);
		}

		// Stepping round on the spot.
		if (Turn && TurnTime >= 0.f)
		{
			const float Weight = FadeWeight(TurnTime, Turn->GetPlayLength(), Turn->GetPlayLength());
			if (Weight > ZERO_ANIMWEIGHT_THRESH)
			{
				FPoseContext Standing(this);
				CopyPose(Locomotion, Standing);
				FPoseContext Turning(this);
				Sample(Turn, TurnTime, Turning);
				Blend(Standing, Turning, 1.f - Weight, Locomotion);
			}
		}

		if (SlotWeight <= ZERO_ANIMWEIGHT_THRESH)
		{
			CopyPose(Locomotion, Output);
		}
		else
		{
			const FAnimationPoseData SourceData(Locomotion);
			FAnimationPoseData OutputData(Output);
			SlotEvaluatePose(SlotName, SourceData, SourceWeight, OutputData, SlotWeight, TotalWeight);
		}

		ApplyBodyYaw(Output);
		ApplyFootPlacement(Output);
		return true;
	}

private:
	static float Length(const UAnimSequence* Sequence)
	{
		return Sequence ? Sequence->GetPlayLength() : 0.f;
	}

	void Sample(const UAnimSequence* Sequence, float Time, FPoseContext& Out)
	{
		if (!Sequence)
		{
			Out.ResetToRefPose();
			return;
		}
		// Extracting root motion locks the root to the reference pose (the clips' root tracks wander), and the
		// characters ignore root motion anyway: movement is the capsule's.
		FAnimationPoseData Data(Out);
		Sequence->GetAnimationPose(Data, FAnimExtractContext(static_cast<double>(Time), true, {}, true));
	}

	void Blend(FPoseContext& One, FPoseContext& Two, float WeightOfOne, FPoseContext& Out)
	{
		FAnimationPoseData OutData(Out);
		FAnimationRuntime::BlendTwoPosesTogether(FAnimationPoseData(One), FAnimationPoseData(Two), WeightOfOne, OutData);
	}

	/** Samples the two directional clips either side of the movement direction and blends between them. */
	void SampleDirectional(const TArray<TObjectPtr<UAnimSequence>>& Clips, FPoseContext& Out)
	{
		const int32 Count = Clips.Num();
		const float Sector = FMath::Fmod(Direction + 360.f, 360.f) / (360.f / Count);
		const int32 First = FMath::FloorToInt(Sector) % Count;
		const int32 Second = (First + 1) % Count;
		const float Alpha = Sector - FMath::FloorToFloat(Sector);
		const UAnimSequence* A = Clips[First] ? Clips[First].Get() : Clips[0].Get();
		const UAnimSequence* B = Clips[Second] ? Clips[Second].Get() : Clips[0].Get();

		Sample(A, MovePhase * Length(A), Out);
		if (Alpha > KINDA_SMALL_NUMBER && A != B)
		{
			FPoseContext APose(this);
			CopyPose(Out, APose);
			FPoseContext BPose(this);
			Sample(B, MovePhase * Length(B), BPose);
			Blend(APose, BPose, 1.f - Alpha, Out);
		}
	}

	void EvaluateLocomotion(FPoseContext& Out)
	{
		FPoseContext IdlePose(this);
		Sample(Idle, IdleTime, IdlePose);
		if (MoveAlpha <= 0.f || !Walk[0])
		{
			CopyPose(IdlePose, Out);
			return;
		}

		FPoseContext Moving(this);
		SampleDirectional(Walk, Moving);
		if (JogAlpha > 0.f && Jog[0])
		{
			FPoseContext WalkPose(this);
			CopyPose(Moving, WalkPose);
			FPoseContext JogPose(this);
			SampleDirectional(Jog, JogPose);
			Blend(WalkPose, JogPose, 1.f - JogAlpha, Moving);
		}
		Blend(IdlePose, Moving, 1.f - MoveAlpha, Out);
	}

	/** Starts, stops and idle breaks: picked here from how the movement changes, played once over the loops. */
	void UpdateOneShot(float DeltaSeconds)
	{
		const bool bMontage = SlotWeight > 0.05f;
		if (OneShot)
		{
			OneShotTime += DeltaSeconds;
			// A start gives way as soon as she turns or lets go; a stop as soon as she moves off again.
			if ((bOneShotIsStart && (!bAccelerating || FMath::Abs(Direction) > 60.f)) || (bOneShotIsStop && bAccelerating) || bMontage)
			{
				FadeOutAt = FMath::Min(FadeOutAt, OneShotTime + 0.2f);
			}
			OneShotWeight = FadeWeight(OneShotTime, Length(OneShot), FadeOutAt);
			if (OneShotTime >= FMath::Min(Length(OneShot), FadeOutAt))
			{
				OneShot = nullptr;
			}
			return;
		}
		if (bMontage)
		{
			IdleFor = 0.f;
			return;
		}

		// Setting off forwards from a standstill.
		const UAnimSequence* Start = JogStart ? JogStart.Get() : WalkStart.Get();
		if (Start && PrevSpeed < 20.f && Speed >= 20.f && bAccelerating && FMath::Abs(Direction) < 50.f)
		{
			Play(Start, true, false, Length(Start) * 0.85f);
			return;
		}

		// Letting go while moving: stop on the foot that's planted, the way she was going.
		if (bWasAccelerating && !bAccelerating && PrevSpeed > 150.f)
		{
			const TArray<TObjectPtr<UAnimSequence>>& Stops = PrevSpeed > (WalkSpeed + JogSpeed) * 0.5f && JogStops.Num() == 8 ? JogStops : WalkStops;
			if (Stops.Num() == 8)
			{
				const float D = FMath::UnwindDegrees(Direction);
				const int32 Way = FMath::Abs(D) <= 45.f ? 0 : (FMath::Abs(D) >= 135.f ? 2 : (D > 0.f ? 1 : 3));
				const int32 Foot = MovePhase < 0.5f ? 0 : 1;
				Play(Stops[Way * 2 + Foot], false, true, Length(Stops[Way * 2 + Foot]));
				return;
			}
		}

		// Standing about: now and then an idle break.
		IdleFor = Speed < 5.f && TurnTime < 0.f ? IdleFor + DeltaSeconds : 0.f;
		if (IdleFor > 9.f && IdleBreaks.Num() > 0)
		{
			IdleFor = 0.f;
			const UAnimSequence* Break = IdleBreaks[FMath::RandRange(0, IdleBreaks.Num() - 1)];
			if (Break)
			{
				Play(Break, false, false, Length(Break));
			}
		}
	}

	void Play(const UAnimSequence* Clip, bool bStart, bool bStop, float EndAt)
	{
		OneShot = Clip;
		OneShotTime = 0.f;
		OneShotWeight = 0.f;
		FadeOutAt = EndAt;
		bOneShotIsStart = bStart;
		bOneShotIsStop = bStop;
	}

	/** Turns the whole body about the vertical so it faces where the feet point while turning on the spot. */
	void ApplyBodyYaw(FPoseContext& Output)
	{
		if (FMath::IsNearlyZero(BodyYawOffset, 0.1f))
		{
			return;
		}
		const FCompactPoseBoneIndex Root(0);
		FTransform& RootTransform = Output.Pose[Root];
		RootTransform.SetRotation(FRotator(0.f, BodyYawOffset, 0.f).Quaternion() * RootTransform.GetRotation());
		RootTransform.SetLocation(FRotator(0.f, BodyYawOffset, 0.f).RotateVector(RootTransform.GetLocation()));
	}

	FCompactPoseBoneIndex Bone(const FPoseContext& Output, FName Name) const
	{
		const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
		const int32 MeshIndex = Bones.GetPoseBoneIndexForBoneName(Name);
		return MeshIndex == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
	}

	/** Pelvis down for the lower foot's ground, the other leg bent to meet its own. */
	void ApplyFootPlacement(FPoseContext& Output)
	{
		const float PelvisDrop = FMath::Min3(FootOffsetL, FootOffsetR, 0.f);
		const float LiftL = FootOffsetL - PelvisDrop;
		const float LiftR = FootOffsetR - PelvisDrop;
		if (FMath::Abs(PelvisDrop) < 0.5f && LiftL < 0.5f && LiftR < 0.5f)
		{
			return;
		}

		const FCompactPoseBoneIndex Pelvis = Bone(Output, TEXT("pelvis"));
		if (!Pelvis.IsValid())
		{
			return;
		}
		Output.Pose[Pelvis].AddToTranslation(FVector(0.f, 0.f, PelvisDrop));

		FCSPose<FCompactPose> ComponentPose;
		ComponentPose.InitPose(Output.Pose);
		TArray<FBoneTransform> Solved;
		auto SolveLeg = [&](FName Thigh, FName Calf, FName Foot, float Lift)
		{
			const FCompactPoseBoneIndex A = Bone(Output, Thigh), B = Bone(Output, Calf), C = Bone(Output, Foot);
			if (Lift < 0.5f || !A.IsValid() || !B.IsValid() || !C.IsValid())
			{
				return;
			}
			FTransform Hip = ComponentPose.GetComponentSpaceTransform(A);
			FTransform Knee = ComponentPose.GetComponentSpaceTransform(B);
			FTransform Ankle = ComponentPose.GetComponentSpaceTransform(C);
			// Keep the knee bending the way it already does.
			const FVector Mid = (Hip.GetLocation() + Ankle.GetLocation()) * 0.5f;
			const FVector Pole = Knee.GetLocation() + (Knee.GetLocation() - Mid).GetSafeNormal() * 50.f;
			AnimationCore::SolveTwoBoneIK(Hip, Knee, Ankle, Pole, Ankle.GetLocation() + FVector(0.f, 0.f, Lift), false, 1.0, 1.0);
			Solved.Add(FBoneTransform(A, Hip));
			Solved.Add(FBoneTransform(B, Knee));
			Solved.Add(FBoneTransform(C, Ankle));
		};
		SolveLeg(TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l"), LiftL);
		SolveLeg(TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r"), LiftR);
		if (Solved.Num() > 0)
		{
			Solved.Sort(FCompareBoneTransformIndex());
			ComponentPose.SafeSetCSBoneTransforms(Solved);
			FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(ComponentPose), Output.Pose);
		}
	}

	TObjectPtr<UAnimSequence> Idle;
	TArray<TObjectPtr<UAnimSequence>> Walk = { nullptr, nullptr, nullptr, nullptr };
	TArray<TObjectPtr<UAnimSequence>> Jog = { nullptr, nullptr, nullptr, nullptr };
	TObjectPtr<UAnimSequence> WalkStart;
	TObjectPtr<UAnimSequence> JogStart;
	TArray<TObjectPtr<UAnimSequence>> WalkStops;
	TArray<TObjectPtr<UAnimSequence>> JogStops;
	TArray<TObjectPtr<UAnimSequence>> IdleBreaks;
	const UAnimSequence* Turn = nullptr;
	float TurnTime = -1.f;
	bool bEightWay = false;

	float Direction = 0.f;
	float WalkSpeed = 160.f;
	float JogSpeed = 450.f;
	float Speed = 0.f;
	float PrevSpeed = 0.f;
	bool bAccelerating = false;
	bool bWasAccelerating = false;
	float BodyYawOffset = 0.f;
	float FootOffsetL = 0.f;
	float FootOffsetR = 0.f;

	float MoveAlpha = 0.f;
	float JogAlpha = 0.f;
	float IdleTime = 0.f;
	float MovePhase = 0.f;
	float IdleFor = 0.f;

	const UAnimSequence* OneShot = nullptr;
	float OneShotTime = 0.f;
	float OneShotWeight = 0.f;
	float FadeOutAt = 0.f;
	bool bOneShotIsStart = false;
	bool bOneShotIsStop = false;

	float SlotWeight = 0.f;
	float SourceWeight = 1.f;
	float TotalWeight = 0.f;
};

void UConquerAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	const APawn* Pawn = TryGetPawnOwner();
	if (!Pawn)
	{
		GroundSpeed = 0.f;
		return;
	}
	const FVector Velocity = Pawn->GetVelocity();
	GroundSpeed = Velocity.Size2D();
	if (GroundSpeed > 1.f)
	{
		const FVector Local = Pawn->GetActorRotation().UnrotateVector(Velocity);
		MoveDirection = FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X));
	}
	const ACharacter* Character = Cast<ACharacter>(Pawn);
	bAccelerating = Character && Character->GetCharacterMovement()->GetCurrentAcceleration().SizeSquared2D() > 1.f;

	UpdateTurnInPlace(Pawn->GetActorRotation().Yaw, DeltaSeconds);
	UpdateFootPlacement(DeltaSeconds);
}

void UConquerAnimInstance::UpdateTurnInPlace(float ActorYaw, float DeltaSeconds)
{
	const bool bHasTurns = AllSet(Turns, 4);
	if (!bHasTurns || GroundSpeed > 10.f || IsAnyMontagePlaying())
	{
		BodyYaw = ActorYaw;
		BodyYawOffset = 0.f;
		TurnElapsed = -1.f;
		return;
	}

	if (TurnElapsed >= 0.f)
	{
		// Step round towards wherever the character faces now, eased over most of the clip.
		TurnElapsed += DeltaSeconds;
		const float Alpha = FMath::SmoothStep(0.f, TurnLength * 0.8f, TurnElapsed);
		BodyYaw = TurnFrom + FMath::FindDeltaAngleDegrees(TurnFrom, ActorYaw) * Alpha;
		if (TurnElapsed >= TurnLength)
		{
			TurnElapsed = -1.f;
		}
	}
	else
	{
		const float Behind = FMath::FindDeltaAngleDegrees(BodyYaw, ActorYaw);
		if (FMath::Abs(Behind) > TurnThreshold)
		{
			TurnClip = (FMath::Abs(Behind) > 135.f ? 2 : 0) + (Behind > 0.f ? 1 : 0);
			TurnFrom = BodyYaw;
			TurnElapsed = 0.f;
			TurnLength = Turns[TurnClip]->GetPlayLength();
		}
	}
	BodyYawOffset = FMath::FindDeltaAngleDegrees(ActorYaw, BodyYaw);
}

void UConquerAnimInstance::UpdateFootPlacement(float DeltaSeconds)
{
	const ACharacter* Character = Cast<ACharacter>(TryGetPawnOwner());
	USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	float TargetL = 0.f, TargetR = 0.f;
	if (bFootPlacement && Character && Mesh && Character->GetCharacterMovement()->IsMovingOnGround())
	{
		const float Floor = Character->GetActorLocation().Z - Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		FCollisionQueryParams Query(SCENE_QUERY_STAT(ConquerFootTrace), false, Character);
		auto Trace = [&](FName Foot)
		{
			if (Mesh->GetBoneIndex(Foot) == INDEX_NONE)
			{
				return 0.f;
			}
			const FVector At = Mesh->GetBoneLocation(Foot);
			FHitResult Hit;
			if (GetWorld()->LineTraceSingleByChannel(Hit, FVector(At.X, At.Y, Floor + 50.f), FVector(At.X, At.Y, Floor - 60.f), ECC_Visibility, Query))
			{
				return FMath::Clamp(static_cast<float>(Hit.ImpactPoint.Z - Floor), -45.f, 45.f);
			}
			return 0.f;
		};
		TargetL = Trace(TEXT("foot_l"));
		TargetR = Trace(TEXT("foot_r"));
	}
	FootOffsetL = FMath::FInterpTo(FootOffsetL, TargetL, DeltaSeconds, 12.f);
	FootOffsetR = FMath::FInterpTo(FootOffsetR, TargetR, DeltaSeconds, 12.f);
}

FAnimInstanceProxy* UConquerAnimInstance::CreateAnimInstanceProxy()
{
	return new FConquerAnimInstanceProxy(this);
}

void UConquerAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete InProxy;
}
