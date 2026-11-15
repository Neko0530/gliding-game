#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "WingsuitHUD.generated.h"

UCLASS()
class AWingsuitHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;
private:
	void Text(const FString& S, float X, float Y, float Scale, FLinearColor C, bool bCenter = false, bool bRight = false);
};
