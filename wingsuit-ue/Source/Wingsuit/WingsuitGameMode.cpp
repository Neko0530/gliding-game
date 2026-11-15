#include "WingsuitGameMode.h"
#include "WingsuitPawn.h"
#include "WingsuitHUD.h"
#include "WingsuitTerrain.h"
#include "WsBackdrop.h"
#include "Engine/World.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Atmosphere/AtmosphericFog.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Actor.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"

AWingsuitGameMode::AWingsuitGameMode()
{
	DefaultPawnClass = AWingsuitPawn::StaticClass();
	HUDClass = AWingsuitHUD::StaticClass();
}

void AWingsuitGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
}

AActor* AWingsuitGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return GetWorld()->SpawnActor<APlayerStart>(FVector(0, 0, 150000), FRotator::ZeroRotator, P);
}

void AWingsuitGameMode::StartPlay()
{
	BuildScene();
	Super::StartPlay();
	if (FParse::Param(FCommandLine::Get(), TEXT("autoshot")))
	{
		FTimerHandle H1, H2, H3, H4, H5;
		auto Cmd = [this](const TCHAR* C) { if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) PC->ConsoleCommand(C); };
		GetWorldTimerManager().SetTimer(H1, [Cmd]() { Cmd(TEXT("HighResShot 1280x720")); }, 15.f, false);
		GetWorldTimerManager().SetTimer(H2, [this]() { if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) if (AWingsuitPawn* P = Cast<AWingsuitPawn>(PC->GetPawn())) P->DebugJump(); }, 17.f, false);
		GetWorldTimerManager().SetTimer(H3, [Cmd]() { Cmd(TEXT("HighResShot 1280x720")); }, 32.f, false);
		GetWorldTimerManager().SetTimer(H4, [Cmd]() { Cmd(TEXT("HighResShot 1280x720")); }, 52.f, false);
		GetWorldTimerManager().SetTimer(H5, [Cmd]() { Cmd(TEXT("quit")); }, 56.f, false);
	}
}

void AWingsuitGameMode::BuildScene()
{
	UWorld* W = GetWorld();
	FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	W->SpawnActor<AWingsuitTerrain>(FVector::ZeroVector, FRotator::ZeroRotator, P);

	W->SpawnActor<AWsBackdrop>(FVector::ZeroVector, FRotator::ZeroRotator, P);

	// Physically based sky: atmosphere + sun + real-time-captured sky light + height fog + volumetric clouds.
	AActor* Sky = W->SpawnActor<AActor>(P);
	USceneComponent* Root = NewObject<USceneComponent>(Sky, TEXT("SkyRoot")); Sky->SetRootComponent(Root); Root->RegisterComponent();
	USkyAtmosphereComponent* Atmo = NewObject<USkyAtmosphereComponent>(Sky, TEXT("Atmosphere"));
	Atmo->SetGroundAlbedo(FColor(70, 110, 60)); Atmo->SetAerialPespectiveViewDistanceScale(.03f); Atmo->SetupAttachment(Root); Atmo->RegisterComponent();

	ADirectionalLight* Sun = W->SpawnActor<ADirectionalLight>(FVector(0, 0, 200000), FRotator(-32, -122, 0), P);
	if (Sun) { UDirectionalLightComponent* L = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()); L->SetIntensity(10.f); L->SetLightColor(FColor(255, 238, 210)); L->SetMobility(EComponentMobility::Movable); L->SetAtmosphereSunLight(true); L->SetCastShadows(true); }

	ADirectionalLight* Fill = W->SpawnActor<ADirectionalLight>(FVector(0, 0, 200000), FRotator(-20, -58, 0), P);
	if (Fill) { UDirectionalLightComponent* L = Cast<UDirectionalLightComponent>(Fill->GetLightComponent()); L->SetMobility(EComponentMobility::Movable); L->SetIntensity(3.5f); L->SetAtmosphereSunLight(false); L->SetCastShadows(false); L->SetLightColor(FColor(190, 210, 255)); }

	ASkyLight* SL = W->SpawnActor<ASkyLight>(FVector(0, 0, 200000), FRotator::ZeroRotator, P);
	if (SL) { USkyLightComponent* C = SL->GetLightComponent(); C->SetMobility(EComponentMobility::Movable); C->bRealTimeCapture = true; C->SetIntensity(3.f); C->RecaptureSky(); }

	APostProcessVolume* PP = W->SpawnActor<APostProcessVolume>(FVector::ZeroVector, FRotator::ZeroRotator, P);
	if (PP)
	{
		PP->bUnbound = true;
		FPostProcessSettings& S = PP->Settings;
		S.bOverride_AutoExposureMethod = true; S.AutoExposureMethod = EAutoExposureMethod::AEM_Histogram;
		S.bOverride_AutoExposureBias = true; S.AutoExposureBias = 0.5f;
		S.bOverride_VignetteIntensity = true; S.VignetteIntensity = .45f;
		S.bOverride_MotionBlurAmount = true; S.MotionBlurAmount = .35f;
		S.bOverride_BloomIntensity = true; S.BloomIntensity = .7f;
	}
}
