#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "ConquerHitNotify.generated.h"

class UAnimMontage;

/**
 * Marks an attack montage's contact frame: where the blade (or foot) connects. Tools/build_anims.py places it where
 * the striking bone moves fastest; the combat component times the montage so this frame lands as the blow resolves,
 * and draws the weapon trail around it. It does nothing when it fires.
 */
UCLASS(meta = (DisplayName = "Conquer Hit"))
class CONQUER_API UConquerHitNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	/** The montage's contact time in seconds, or a negative number if it has no Hit notify. */
	static float FindHitTime(const UAnimMontage* Montage);
};
