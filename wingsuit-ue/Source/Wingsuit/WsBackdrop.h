#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WsBackdrop.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/** Illustrated backdrop: one camera-locked plate that drifts continuously and also scrolls with the camera for parallax. */
UCLASS()
class AWsBackdrop : public AActor
{
	GENERATED_BODY()
public:
	AWsBackdrop();
	virtual void Tick(float Dt) override;
	virtual void BeginPlay() override;
private:
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Plate;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Mat;
};
