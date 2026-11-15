#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WingsuitTerrain.generated.h"

class UProceduralMeshComponent;
class UInstancedStaticMeshComponent;

/** Builds the mountain-valley course as a lit procedural mesh (grass / rock / snow sections) and scatters trees. */
UCLASS()
class AWingsuitTerrain : public AActor
{
	GENERATED_BODY()
public:
	AWingsuitTerrain();
	virtual void OnConstruction(const FTransform& Xf) override;
	virtual void BeginPlay() override;
private:
	void Build();
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Mesh;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> TreesA;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> TreesB;
	bool bBuilt = false;
};
