#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "WingsuitGameMode.generated.h"

UCLASS()
class AWingsuitGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AWingsuitGameMode();
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
private:
	void BuildScene();
};
