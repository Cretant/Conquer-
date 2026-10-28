#include "ConquerHitNotify.h"
#include "Animation/AnimMontage.h"

float UConquerHitNotify::FindHitTime(const UAnimMontage* Montage)
{
	if (!Montage)
	{
		return -1.f;
	}
	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		if (Event.Notify && Event.Notify->IsA<UConquerHitNotify>())
		{
			return Event.GetTriggerTime();
		}
	}
	return -1.f;
}
