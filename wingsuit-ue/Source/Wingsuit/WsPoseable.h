#pragma once
#include "CoreMinimal.h"
#include "Components/PoseableMeshComponent.h"
#include "WsPoseable.generated.h"

/** Poseable skeletal mesh that lets code write local bone transforms directly (the base class keeps them protected). */
UCLASS()
class UWsPoseable : public UPoseableMeshComponent
{
	GENERATED_BODY()
public:
	void ApplyLocal(const TArray<FTransform>& Local)
	{
		BoneSpaceTransforms = Local;
		MarkRefreshTransformDirty();
	}
};
