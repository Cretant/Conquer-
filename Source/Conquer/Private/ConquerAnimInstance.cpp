#include "ConquerAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"
#include "GameFramework/Pawn.h"

namespace
{
	const FName SlotName(TEXT("DefaultSlot"));

	void CopyPose(const FPoseContext& From, FPoseContext& To)
	{
		To.Pose = From.Pose;
		To.Curve = From.Curve;
		To.CustomAttributes = From.CustomAttributes;
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
		Walk = Instance->Walk;
		Jog = Instance->Jog;
		WalkSpeed = FMath::Max(Instance->WalkSpeed, 1.f);
		JogSpeed = FMath::Max(Instance->JogSpeed, WalkSpeed + 1.f);
		Speed = Instance->GetGroundSpeed();
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
		const float CycleLength = FMath::Lerp(Length(Walk), Length(Jog), JogAlpha);
		const float NaturalSpeed = FMath::Lerp(WalkSpeed, JogSpeed, JogAlpha);
		const float Rate = Speed > 1.f ? FMath::Clamp(Speed / NaturalSpeed, 0.5f, 2.f) : 1.f;
		if (CycleLength > UE_KINDA_SMALL_NUMBER)
		{
			MovePhase = FMath::Fmod(MovePhase + DeltaSeconds * Rate / CycleLength, 1.f);
		}

		GetSlotWeight(SlotName, SlotWeight, SourceWeight, TotalWeight);
		UpdateSlotNodeWeight(SlotName, SlotWeight, 1.f);
	}

	virtual bool Evaluate(FPoseContext& Output) override
	{
		FPoseContext Locomotion(this);
		EvaluateLocomotion(Locomotion);

		if (SlotWeight <= ZERO_ANIMWEIGHT_THRESH)
		{
			CopyPose(Locomotion, Output);
			return true;
		}

		const FAnimationPoseData SourceData(Locomotion);
		FAnimationPoseData OutputData(Output);
		SlotEvaluatePose(SlotName, SourceData, SourceWeight, OutputData, SlotWeight, TotalWeight);
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
		FAnimationPoseData Data(Out);
		Sequence->GetAnimationPose(Data, FAnimExtractContext(static_cast<double>(Time), false, {}, true));
	}

	void Blend(FPoseContext& One, FPoseContext& Two, float WeightOfOne, FPoseContext& Out)
	{
		FAnimationPoseData OutData(Out);
		FAnimationRuntime::BlendTwoPosesTogether(FAnimationPoseData(One), FAnimationPoseData(Two), WeightOfOne, OutData);
	}

	void EvaluateLocomotion(FPoseContext& Out)
	{
		FPoseContext IdlePose(this);
		Sample(Idle, IdleTime, IdlePose);
		if (MoveAlpha <= 0.f || !Walk)
		{
			CopyPose(IdlePose, Out);
			return;
		}

		FPoseContext Moving(this);
		Sample(Walk, MovePhase * Length(Walk), Moving);
		if (JogAlpha > 0.f && Jog)
		{
			FPoseContext WalkPose(this);
			CopyPose(Moving, WalkPose);
			FPoseContext JogPose(this);
			Sample(Jog, MovePhase * Length(Jog), JogPose);
			Blend(WalkPose, JogPose, 1.f - JogAlpha, Moving);
		}
		Blend(IdlePose, Moving, 1.f - MoveAlpha, Out);
	}

	TObjectPtr<UAnimSequence> Idle;
	TObjectPtr<UAnimSequence> Walk;
	TObjectPtr<UAnimSequence> Jog;
	float WalkSpeed = 160.f;
	float JogSpeed = 450.f;
	float Speed = 0.f;

	float MoveAlpha = 0.f;
	float JogAlpha = 0.f;
	float IdleTime = 0.f;
	float MovePhase = 0.f;

	float SlotWeight = 0.f;
	float SourceWeight = 1.f;
	float TotalWeight = 0.f;
};

void UConquerAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	const APawn* Pawn = TryGetPawnOwner();
	GroundSpeed = Pawn ? Pawn->GetVelocity().Size2D() : 0.f;
}

FAnimInstanceProxy* UConquerAnimInstance::CreateAnimInstanceProxy()
{
	return new FConquerAnimInstanceProxy(this);
}

void UConquerAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete InProxy;
}
