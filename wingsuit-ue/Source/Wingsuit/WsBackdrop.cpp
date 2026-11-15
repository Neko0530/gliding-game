#include "WsBackdrop.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

namespace
{
	constexpr double DistM = 9000.0;      // metres behind the camera
	constexpr double Aspect = 1920.0 / 1120.0;
	constexpr double DriftPerSec = 0.004; // continuous scroll, fraction of the image width per second
	constexpr double ParallaxX = 0.00006; // extra scroll per metre of camera travel
	constexpr double ParallaxY = 0.00002;
}

AWsBackdrop::AWsBackdrop()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	Plate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Plate"));
	SetRootComponent(Plate);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (Plane.Succeeded()) Plate->SetStaticMesh(Plane.Object);
	Plate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Plate->SetCastShadow(false);
	Plate->SetMobility(EComponentMobility::Movable);
}

void AWsBackdrop::BeginPlay()
{
	Super::BeginPlay();
	if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Backdrop.M_Backdrop")))
	{
		Mat = UMaterialInstanceDynamic::Create(M, this);
		Plate->SetMaterial(0, Mat);
	}
	// sized to fill a 30 degree view with margin
	const double W = .536 * DistM * 1.15, Hh = W / Aspect;
	SetActorScale3D(FVector(W, -Hh, 1.0));      // negative Y flips the image upright
	SetActorRotation(FRotator(0, 0, -90));      // plane normal toward the camera (+Y)
}

void AWsBackdrop::Tick(float Dt)
{
	Super::Tick(Dt);
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->PlayerCameraManager) return;
	const FVector C = PC->PlayerCameraManager->GetCameraLocation();
	SetActorLocation(FVector(C.X, C.Y - DistM * 100.0, C.Z));
	if (Mat)
	{
		const double T = GetWorld()->GetTimeSeconds();
		Mat->SetVectorParameterValue(TEXT("Scroll"), FLinearColor(float(T * DriftPerSec + C.X / 100.0 * ParallaxX), float(-C.Z / 100.0 * ParallaxY), 0, 0));
	}
}
