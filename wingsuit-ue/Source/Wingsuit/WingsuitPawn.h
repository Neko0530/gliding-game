#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "WingsuitFlyer.h"
#include "WingsuitPawn.generated.h"

class UCameraComponent;
class UProceduralMeshComponent;
class UWsPoseable;
class UAudioComponent;
class USoundBase;
class USceneComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;

UENUM()
enum class EWsState : uint8 { Ready, Fly, Paused, Crash, Slide, Result };

struct FWsGate { double X, Y; int32 Res = 0; /*0 pending,1 pass,2 miss*/ int32 Pts = 0; };
struct FWsToast { FString Text; FColor Col; double T = 0; };
struct FWsPart { FVector2D P, V; double Life; FColor C; };

UCLASS()
class AWingsuitPawn : public APawn
{
	GENERATED_BODY()
public:
	AWingsuitPawn();
	virtual void Tick(float Dt) override;
	virtual void BeginPlay() override;
	void DebugJump() { if (State == EWsState::Ready) StartJump(); }

	// ---- read by the HUD ----
	EWsState State = EWsState::Ready;
	double PX = 0, PY = 0, VX = 0, VY = 0, Th = 0, Om = 0, Al = 0, V = 0, Agl = 0;
	bool bStalled = false;
	double T = 0, Tuck = 0, Ext = 0, Flare = 0, GlideSm = 2.5;
	double Wx = 0, Wy = 0, WTurb = 0;
	bool bFreeMode = false;
	TArray<FWsGate> Gates;
	TArray<FWsToast> Toasts;
	TArray<FString> ResultLines;
	FString ResultTitle, ResultSub, ResultBest;
	FString ResultGrade;
	int32 ResultTotal = 0;
	double WarnTtc = -1; // seconds to terrain impact, <0 none
	int32 BestScore = 0;

private:
	void ResetRun();
	void StartJump();
	void ReadInput();
	void FlightStep(double Dt);
	void SlideStep(double Dt);
	void Contact(bool bTree);
	void FinishRun();
	void UpdateCamera(double Dt);
	void UpdateVisual();
	void DrawWorldFx(double Dt);
	void Toast(const FString& S, FColor C);
	bool Down(FKey K) const;
	bool Pressed(FKey K);

	UPROPERTY() TObjectPtr<UCameraComponent> Cam;
	UPROPERTY() TObjectPtr<USceneComponent> FlyerRoot;
	UPROPERTY() TObjectPtr<UWsPoseable> Manny;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> FlyerMesh;
	FWsRig Rig;
	UPROPERTY() TObjectPtr<UAudioComponent> WindAC;
	UPROPERTY() TObjectPtr<UAudioComponent> MusicAC;
	UPROPERTY() TMap<FName, TObjectPtr<USoundBase>> Sounds;
	double BeepT = 0;
	bool bFlareSnd = false;
	void Sfx(const TCHAR* N, float Vol = 1.f, float Pitch = 1.f);
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> FlyerMats[4];
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> MannyMat;
	bool bFlyerBuilt = false;
	double FlyTime = 0, PoseArch = 0, PoseHead = 0, PoseSpan = .8, PoseSweep = .6, PoseSpread = .15, PoseKnee = 0, PoseLift = 0, Wobble = 0;

	TMap<FKey, bool> Prev;
	double PitchSm = 0, FineSm = 0, Trim = .2, AS = .4;
	double CamX = 0, CamY = 1500, CamS = 3;
	double MaxX = 0, CloseT = 0, CloseCool = 0, StallT = 0, EndT = 0, SlideT = 0, SlideSp = 0;
	int32 CloseCount = 0, Overs = 0, LastSign = 0;
	double LastSignT = -9;
	int32 Outcome = 0; // 0 none,1 perfect,2 clean,3 rough,4 crash
	double ImpVn = 0, ImpVt = 0;
	bool bInZone = false;
	TArray<FVector2D> Trail; double TrailT = 0;
	TArray<FWsPart> Parts;
	double Accum = 0;
	struct FInputState { double Pitch = 0, Fine = 0; bool bTuck = false, bStab = false, bFlare = false; } In;
};
