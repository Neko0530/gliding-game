#include "WingsuitPawn.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "WingsuitWorld.h"
#include "Camera/CameraComponent.h"
#include "ProceduralMeshComponent.h"
#include "KismetProceduralMeshLibrary.h"
#include "WingsuitFlyer.h"
#include "WsPoseable.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SceneComponent.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"

namespace
{
	constexpr double GRAV = 9.81, MASS = 80, CLA = 3.3, A0 = -0.04;
	constexpr double M2U = 100.0; // metres -> UE units
	double Smooth(double A, double B, double X) { const double T = FMath::Clamp((X - A) / (B - A), 0.0, 1.0); return T * T * (3 - 2 * T); }
	double WrapPi(double A) { A = FMath::Fmod(A + PI, 2 * PI); if (A < 0) A += 2 * PI; return A - PI; }
	double Lerp(double A, double B, double T) { return A + (B - A) * T; }
	FVector ToW(double X, double Y, double Depth = 0) { return FVector(X * M2U, -Depth * M2U, Y * M2U); }
	FString FmtTime(double T) { return FString::Printf(TEXT("%d:%02d"), int32(T / 60), int32(FMath::Fmod(T, 60.0))); }
}

AWingsuitPawn::AWingsuitPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Cam = CreateDefaultSubobject<UCameraComponent>(TEXT("Cam"));
	Cam->SetupAttachment(Root);
	Cam->SetFieldOfView(30.f);
	Cam->SetUsingAbsoluteLocation(true);
	Cam->SetUsingAbsoluteRotation(true);
	Cam->SetUsingAbsoluteScale(true);
	FlyerRoot = CreateDefaultSubobject<USceneComponent>(TEXT("FlyerRoot"));
	FlyerRoot->SetupAttachment(Root);
	Manny = CreateDefaultSubobject<UWsPoseable>(TEXT("Manny"));
	Manny->SetupAttachment(FlyerRoot);
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannyMesh(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	if (MannyMesh.Succeeded()) Manny->SetSkinnedAssetAndUpdate(MannyMesh.Object);
	Manny->SetRelativeRotation(FRotator(0, -90, 0));
	Manny->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FlyerMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Flyer"));
	FlyerMesh->SetupAttachment(FlyerRoot);
	FlyerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FlyerMesh->SetRelativeScale3D(FVector(100.0));
	FlyerMesh->bUseAsyncCooking = false;
	AutoPossessPlayer = EAutoReceiveInput::Player0;
}

void AWingsuitPawn::BeginPlay()
{
	Super::BeginPlay();
	UMaterialInterface* BaseM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Matte.M_Matte"));
	// white wings, orange trim, black helmet, tan container; teal suit on the mannequin
	const FLinearColor Cols[4] = { FLinearColor(.82f, .83f, .85f), FLinearColor(.85f, .22f, .02f), FLinearColor(.015f, .015f, .017f), FLinearColor(.30f, .23f, .13f) };
	for (int32 I = 0; I < 4; I++)
		if (BaseM) if (UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(BaseM, this)) { M->SetVectorParameterValue(TEXT("Color"), Cols[I]); FlyerMats[I] = M; }
	UMaterialInterface* SuitBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Suit.M_Suit"));
	if (SuitBase) if (UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(SuitBase, this))
	{
		M->SetVectorParameterValue(TEXT("Color"), FLinearColor(.008f, .26f, .24f)); MannyMat = M;
		for (int32 I = 0; I < Manny->GetNumMaterials(); I++) Manny->SetMaterial(I, M);
	}
	if (Manny->GetSkinnedAsset())
	{
		Rig.Init(Manny->GetSkinnedAsset()->GetRefSkeleton());
		FString Names; for (const TPair<FName, int32>& K : Rig.Idx) Names += K.Key.ToString() + TEXT(" ");
		UE_LOG(LogTemp, Display, TEXT("Wingsuit rig bones: %d : %s"), Rig.Idx.Num(), *Names);
	}
	// ---- audio: looping music and wind, one-shot effects ----
	for (const TCHAR* N : { TEXT("wind_loop"), TEXT("music_loop"), TEXT("gate_chime"), TEXT("gate_miss"), TEXT("close_pass"), TEXT("jump"), TEXT("beep"), TEXT("crash"), TEXT("land"), TEXT("flare") })
		if (USoundBase* S = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Audio/%s.%s"), N, N))) Sounds.Add(FName(N), S);
	if (auto* M = Sounds.Find(FName("music_loop"))) MusicAC = UGameplayStatics::SpawnSound2D(this, M->Get(), .40f, 1.f, 0.f, nullptr, false, false);
	if (auto* Wd = Sounds.Find(FName("wind_loop"))) WindAC = UGameplayStatics::SpawnSound2D(this, Wd->Get(), .05f, 1.f, 0.f, nullptr, false, false);
	if (APlayerController* PC = Cast<APlayerController>(GetController())) { PC->bShowMouseCursor = false; PC->SetInputMode(FInputModeGameOnly()); }
	ResetRun();
}

bool AWingsuitPawn::Down(FKey K) const { const APlayerController* PC = Cast<APlayerController>(GetController()); return PC && PC->IsInputKeyDown(K); }
bool AWingsuitPawn::Pressed(FKey K) { const bool D = Down(K); bool& P = Prev.FindOrAdd(K); const bool R = D && !P; P = D; return R; }

void AWingsuitPawn::Sfx(const TCHAR* N, float Vol, float Pitch)
{
	if (auto* S = Sounds.Find(FName(N))) UGameplayStatics::PlaySound2D(this, S->Get(), Vol, Pitch);
}

void AWingsuitPawn::Toast(const FString& S, FColor C) { Toasts.Add({ S, C, 0 }); if (Toasts.Num() > 4) Toasts.RemoveAt(0); }

void AWingsuitPawn::ResetRun()
{
	State = EWsState::Ready; T = 0; PitchSm = FineSm = Tuck = Ext = Flare = 0; MaxX = 0; CloseT = CloseCool = StallT = 0; CloseCount = Overs = 0;
	LastSign = 0; LastSignT = -9; Outcome = 0; GlideSm = 2.5; bStalled = false; Trail.Empty(); Parts.Empty(); Toasts.Empty(); ResultLines.Empty(); Accum = 0;
	TArray<FVector2D> G; WsWorld::Gates(G); Gates.Reset(); for (const FVector2D& P : G) Gates.Add({ P.X, P.Y });
	PX = -4; PY = WsWorld::Ground(PX) + 3.2; VX = VY = 0; Th = PI / 2; Om = 0; Al = 0; V = 0;
	CamX = PX + 120; CamY = PY - 60; CamS = 2.0;
}

void AWingsuitPawn::StartJump() { State = EWsState::Fly; VX = 14; VY = 1; Th = .1; PY = WsWorld::Ground(PX) + 4.5; Om = 0; Toast(TEXT("GO!"), FColor::White); Sfx(TEXT("jump"), .9f); }

void AWingsuitPawn::ReadInput()
{
	In.Pitch = (Down(EKeys::W) || Down(EKeys::Up) ? 1 : 0) - (Down(EKeys::S) || Down(EKeys::Down) ? 1 : 0);
	In.Fine = (Down(EKeys::A) || Down(EKeys::Left) ? 1 : 0) - (Down(EKeys::D) || Down(EKeys::Right) ? 1 : 0);
	In.bFlare = Down(EKeys::LeftControl) || Down(EKeys::RightControl) || Down(EKeys::F);
	In.bTuck = (Down(EKeys::LeftShift) || Down(EKeys::RightShift)) && !In.bFlare;
	In.bStab = Down(EKeys::SpaceBar) && !In.bTuck && !In.bFlare;
}

void AWingsuitPawn::FlightStep(double Dt)
{
	const FWsDiff& D = WsWorld::Diff(); ReadInput(); T += Dt;
	const bool Stab = In.bStab;
	PitchSm += ((In.Pitch * (Stab ? .35 : 1)) - PitchSm) * (1 - FMath::Exp(-Dt / .45));
	FineSm += ((In.Fine * (Stab ? .35 : 1)) - FineSm) * (1 - FMath::Exp(-Dt / .35));
	Tuck += ((In.bTuck ? 1 : 0) - Tuck) * (1 - FMath::Exp(-Dt / .30));
	Ext += ((Stab ? 1 : 0) - Ext) * (1 - FMath::Exp(-Dt / .40));
	Flare += ((In.bFlare ? 1 : 0) - Flare) * (1 - FMath::Exp(-Dt / .28));
	const double S = 2.2 - .6 * Tuck + .2 * Ext + .4 * Flare;
	const double CD0 = .12 - .03 * Tuck + .02 * Ext + .09 * Flare;
	const double aS = .40 - .04 * Tuck + .18 * Flare;
	const double kStab = 1 - .3 * Tuck + .3 * Ext;
	const double Zeta = .55 + .5 * Ext - .15 * Tuck;
	const double Trm = FMath::Clamp(.20 + .17 * PitchSm + .06 * FineSm - .13 * Tuck + .26 * Flare, -.02, .50);
	const FWsWind W = WsWorld::WindAt(PX, PY, T); Wx = W.X; Wy = W.Y; WTurb = W.Turb;
	const double Rx = VX - W.X, Ry = VY - W.Y, Vr = FMath::Sqrt(Rx * Rx + Ry * Ry) + 1e-6;
	const double Gam = FMath::Atan2(Ry, Rx), Alpha = WrapPi(Th - Gam), AA = FMath::Abs(Alpha);
	const double AglL = PY - WsWorld::Ground(PX), Ge = FMath::Clamp(1 - AglL / 18, 0.0, 1.0);
	const double Sx = Smooth(aS, aS + .12, AA);
	const double CL = Lerp(CLA * (Alpha - A0), .9 * FMath::Sin(2 * Alpha), Sx) * (1 + .3 * Ge);
	const double KInd = .25 * (1 - .45 * Ge);
	const double CD = CD0 + KInd * CL * CL + 1.1 * Sx * FMath::Square(FMath::Sin(Alpha));
	const double Rho = 1.2 * FMath::Exp(-PY / 9000), Q = .5 * Rho * Vr * Vr;
	const double Lf = Q * S * CL, Df = Q * S * CD, Ux = Rx / Vr, Uy = Ry / Vr;
	const double Ax = (Lf * -Uy - Df * Ux) / MASS, Ay = (Lf * Ux - Df * Uy) / MASS - GRAV;
	const double Wn2 = .003 * FMath::Max(Q, 150.0) * kStab;
	double Ang = -Wn2 * FMath::Sin(Alpha - Trm) - 2 * Zeta * FMath::Sqrt(Wn2) * Om;
	if (W.Turb > 0) Ang += WsWorld::TurbPitchNoise(T) * 2.2 * W.Turb;
	Om += Ang * Dt; Th = WrapPi(Th + Om * Dt);
	const double Px0 = PX;
	VX += Ax * Dt; VY += Ay * Dt; PX += VX * Dt; PY += VY * Dt;
	Al = Alpha; V = Vr; Agl = AglL; AS = aS; Trim = Trm;
	bStalled = AA > aS + .02 && Vr > 8; if (bStalled) StallT += Dt;
	MaxX = FMath::Max(MaxX, PX);
	GlideSm += (FMath::Clamp(VX / FMath::Max(-VY, .5), 0.0, 9.0) - GlideSm) * (1 - FMath::Exp(-Dt / 1.2));
	for (FWsGate& G : Gates)
		if (G.Res == 0 && Px0 < G.X && PX >= G.X)
		{
			const double Acc = 1 - FMath::Abs(PY - G.Y) / D.Gate;
			if (Acc > 0) { G.Pts = FMath::RoundToInt(250 * Acc); G.Res = 1; Sfx(TEXT("gate_chime"), .8f); Toast(FString::Printf(TEXT("GATE  +%d"), G.Pts), Acc > .6 ? FColor(125, 255, 154) : FColor(255, 226, 122)); }
			else { G.Res = 2; Sfx(TEXT("gate_miss"), .7f); Toast(TEXT("GATE MISSED"), FColor(255, 143, 143)); }
		}
	CloseCool = FMath::Max(0.0, CloseCool - Dt);
	if (AglL > 4 && AglL < 18 && Vr > 28 && PX < WsWorld::LZ_X - 250) { CloseT += Dt; if (CloseCool <= 0) { CloseCount++; Sfx(TEXT("close_pass"), .8f); Toast(TEXT("CLOSE PASS"), FColor(255, 226, 122)); } CloseCool = 2.5; }
	TrailT += Dt; if (TrailT > .04) { TrailT = 0; Trail.Add(FVector2D(PX, PY)); if (Trail.Num() > 90) Trail.RemoveAt(0); }
	const double L = 5.0, C = FMath::Cos(Th), Sn = FMath::Sin(Th);
	const double Pts[3][2] = { {PX, PY}, {PX + C * L, PY + Sn * L}, {PX - C * L, PY - Sn * L} };
	for (const auto& P : Pts)
	{
		if (P[1] <= WsWorld::Ground(P[0])) { Contact(false); return; }
		if (WsWorld::TreeHit(P[0], P[1])) { Contact(true); return; }
	}
}

void AWingsuitPawn::Contact(bool bTree)
{
	const FWsDiff& D = WsWorld::Diff();
	const double Sl = FMath::Atan2(WsWorld::Ground(PX + 2) - WsWorld::Ground(PX - 2), 4.0);
	const double Nx = -FMath::Sin(Sl), Ny = FMath::Cos(Sl);
	const double Vn = -(VX * Nx + VY * Ny), Vt = VX * FMath::Cos(Sl) + VY * FMath::Sin(Sl);
	const double Align = FMath::Abs(WrapPi(Th - Sl)), Tol = D.Tol;
	int32 Out;
	if (bTree) Out = 4;
	else if (Vn <= 3.5 * Tol && FMath::Abs(Vt) <= 26 * Tol && Align <= .30) Out = 1;
	else if (Vn <= 6.5 * Tol && FMath::Abs(Vt) <= 34 * Tol && Align <= .50) Out = 2;
	else if (Vn <= 10 * Tol && FMath::Abs(Vt) <= 48 * Tol && Align <= .75) Out = 3;
	else Out = 4;
	Outcome = Out; ImpVn = Vn; ImpVt = Vt;
	if (Out == 4)
	{
		State = EWsState::Crash; EndT = 0; Sfx(TEXT("crash"), 1.f);
		for (int32 I = 0; I < 46; I++) Parts.Add({ FVector2D(PX, PY), FVector2D(VX * .25 + FMath::FRandRange(-15.f, 15.f), FMath::FRandRange(0.f, 22.f)), FMath::FRandRange(1.f, 2.f), FMath::FRand() < .5f ? FColor(255, 154, 77) : FColor(201, 195, 184) });
	}
	else
	{
		State = EWsState::Slide; SlideT = 0; Sfx(TEXT("land"), Out == 3 ? 1.f : .7f); PY = WsWorld::Ground(PX) + .5;
		SlideSp = FMath::Max(0.0, Vt); VX = SlideSp * FMath::Cos(Sl); VY = SlideSp * FMath::Sin(Sl); Om = 0;
		for (int32 I = 0; I < 14; I++) Parts.Add({ FVector2D(PX, PY + .5), FVector2D(FMath::FRandRange(-2.f, 8.f), FMath::FRandRange(0.f, 5.f)), .8, FColor(205, 191, 154) });
	}
}

void AWingsuitPawn::SlideStep(double Dt)
{
	SlideT += Dt;
	const double Sl = FMath::Atan2(WsWorld::Ground(PX + 2) - WsWorld::Ground(PX - 2), 4.0);
	SlideSp = FMath::Max(0.0, SlideSp - (Outcome == 3 ? 14 : 9) * Dt);
	PX += SlideSp * FMath::Cos(Sl) * Dt; PY = WsWorld::Ground(PX) + .5;
	VX = SlideSp * FMath::Cos(Sl); VY = SlideSp * FMath::Sin(Sl);
	Th += WrapPi(Sl - Th) * FMath::Min(1.0, Dt * 6); MaxX = FMath::Max(MaxX, PX);
	if (FMath::FRand() < SlideSp * Dt * .8) Parts.Add({ FVector2D(PX, PY + .3), FVector2D(-SlideSp * .3 + FMath::FRand() * 3, FMath::FRand() * 3), .6, FColor(205, 191, 154) });
	if (SlideSp < .8 || SlideT > 3.2) FinishRun();
}

void AWingsuitPawn::FinishRun()
{
	const FWsDiff& D = WsWorld::Diff(); State = EWsState::Result; ResultLines.Reset();
	const bool bLanded = Outcome != 4 && Outcome != 0;
	bInZone = FMath::Abs(PX - WsWorld::LZ_X) <= D.Lz / 2;
	static const TCHAR* OutName[] = { TEXT(""), TEXT("perfect"), TEXT("clean"), TEXT("rough"), TEXT("crash") };
	if (bFreeMode)
	{
		ResultTitle = bLanded ? TEXT("Touchdown") : TEXT("Crash");
		ResultSub = bLanded ? FString::Printf(TEXT("%s landing%s"), OutName[Outcome], bInZone ? TEXT(" in the zone") : TEXT(" outside the zone")) : FString(TEXT("Free flight - no score"));
		ResultLines.Add(FString::Printf(TEXT("Flight time   %s"), *FmtTime(T)));
		ResultLines.Add(FString::Printf(TEXT("Sink rate at contact   %.1f m/s"), ImpVn));
		ResultLines.Add(FString::Printf(TEXT("Ground speed at contact   %.0f km/h"), FMath::Abs(ImpVt * 3.6)));
		ResultGrade = TEXT(""); ResultTotal = 0; ResultBest = TEXT(""); return;
	}
	const int32 Dist = FMath::RoundToInt(1000 * FMath::Clamp(MaxX / WsWorld::LZ_X, 0.0, 1.0));
	int32 GatePts = 0, Passed = 0; for (const FWsGate& G : Gates) { GatePts += G.Pts; Passed += G.Res == 1; }
	const int32 Close = FMath::RoundToInt(FMath::Min(600.0, CloseT * 80));
	ResultLines.Add(FString::Printf(TEXT("Distance flown (%d m)   %d"), FMath::RoundToInt(MaxX), Dist));
	ResultLines.Add(FString::Printf(TEXT("Checkpoints (%d/%d)   %d"), Passed, Gates.Num(), GatePts));
	ResultLines.Add(FString::Printf(TEXT("Close passes (%d)   %d"), CloseCount, Close));
	int32 Total, Land = 0, Acc = 0, Tm = 0, Sm = 0;
	if (bLanded)
	{
		Tm = FMath::RoundToInt(FMath::Clamp(600 * (1.7 - T / 100), 0.0, 600.0));
		Sm = FMath::RoundToInt(FMath::Max(0.0, 500 - Overs * 30 * D.Pen - StallT * 40 * D.Pen));
		if (bInZone) { Land = Outcome == 1 ? 1000 : Outcome == 2 ? 600 : 250; if (Outcome != 3) Acc = FMath::RoundToInt(300 * (1 - FMath::Abs(PX - WsWorld::LZ_X) / (D.Lz / 2))); }
		else Land = 100;
		ResultLines.Add(FString::Printf(TEXT("Time efficiency (%s)   %d"), *FmtTime(T), Tm));
		ResultLines.Add(FString::Printf(TEXT("Smooth control (%d overcorrections, %.1f s stalled)   %d"), Overs, StallT, Sm));
		ResultLines.Add(FString::Printf(TEXT("Landing: %s%s   %d"), OutName[Outcome], bInZone ? TEXT("") : TEXT(" (outside zone)"), Land));
		if (Acc) ResultLines.Add(FString::Printf(TEXT("Touchdown accuracy (%d m off centre)   %d"), FMath::RoundToInt(FMath::Abs(PX - WsWorld::LZ_X)), Acc));
		Total = FMath::RoundToInt((Dist + GatePts + Close + Tm + Sm + Land + Acc) * D.Mult);
	}
	else { ResultLines.Add(TEXT("Crash - mission failed: score reduced")); Total = FMath::RoundToInt((Dist + GatePts + Close) * .4 * D.Mult); }
	ResultGrade = TEXT("F");
	if (bLanded) { const double B = Total / D.Mult; ResultGrade = B >= 4000 ? TEXT("S") : B >= 3200 ? TEXT("A") : B >= 2400 ? TEXT("B") : B >= 1600 ? TEXT("C") : TEXT("D"); }
	ResultTitle = !bLanded ? FString(TEXT("Crash - mission failed")) : bInZone ? FString(TEXT("Mission complete")) : FString(TEXT("Landed outside the zone"));
	ResultSub = FString::Printf(TEXT("%s - %s"), D.Name, *FmtTime(T)); ResultTotal = Total;
	if (bLanded && bInZone && Total > BestScore) { BestScore = Total; ResultBest = TEXT("New best!"); }
	else ResultBest = BestScore ? FString::Printf(TEXT("Best this session: %d"), BestScore) : FString();
}

void AWingsuitPawn::UpdateCamera(double Dt)
{
	const double SW = 1600, SH = 900;
	APlayerController* PC = Cast<APlayerController>(GetController());
	double Mx = 0, My = 0; float Mxp = 0, Myp = 0; int32 Vw = 0, Vh = 0;
	if (PC) { PC->GetViewportSize(Vw, Vh); if (Vw > 0 && PC->GetMousePosition(Mxp, Myp)) { Mx = Mxp / Vw * 2 - 1; My = Myp / Vh * 2 - 1; } }
	if (State == EWsState::Ready) { CamS += (2.0 - CamS) * FMath::Min(1.0, Dt * 2); CamX += (PX + 120 - CamX) * FMath::Min(1.0, Dt * 2); CamY += (PY - 60 - CamY) * FMath::Min(1.0, Dt * 2); }
	else if (State != EWsState::Paused)
	{
		const double Agl2 = PY - WsWorld::Ground(PX), Spd = FMath::Sqrt(VX * VX + VY * VY);
		const double Tx = PX + FMath::Clamp(VX, -60.0, 80.0) * .66 + Mx * SW * .12 / CamS;
		const double Ty = PY + FMath::Clamp(VY, -60.0, 40.0) * .5 - FMath::Min(.55 * Agl2, 85.0) + My * SH * .10 / CamS;
		const double Ts = 2.0;   // fixed zoom: the flyer and the scenery keep one constant scale
		const double K = 1 - FMath::Exp(-Dt * 3);
		CamX += (Tx - CamX) * K; CamY += (Ty - CamY) * K; CamS += (Ts - CamS) * (1 - FMath::Exp(-Dt * 1.2));
	}
	const double H = SH / CamS, Dist = H / 2 / FMath::Tan(FMath::DegreesToRadians(Cam->FieldOfView / 2));
	const double Elev = FMath::DegreesToRadians(0.0);
	FVector CamPos = ToW(CamX, CamY, -Dist * FMath::Cos(Elev)); CamPos.Z += Dist * FMath::Sin(Elev) * M2U;
	Cam->SetWorldLocationAndRotation(CamPos, FRotator(0.0, -90, 0));
}

void AWingsuitPawn::UpdateVisual()
{
	const double Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : .016;
	FlyTime += Dt;
	SetActorLocationAndRotation(ToW(PX, PY), FRotator(FMath::RadiansToDegrees(Th), 0, 0));
	static const bool bCloseup = FParse::Param(FCommandLine::Get(), TEXT("closeup"));
	SetActorScale3D(FVector(bCloseup ? 40.0 : 14.0));

	// --- target posture from flight state and pilot input ---
	const double Spd01 = FMath::Clamp(V / 60.0, 0.0, 1.5);
	double Arch = .03 + .30 * PitchSm - .35 * Tuck + .15 * Ext + .75 * Flare;
	double Head = .25 * PitchSm - .30 * Tuck + .10 * Flare + .08 * FMath::Clamp(-VY / 30.0, -1.0, 1.0);
	double Span = .82 - .30 * Tuck + .22 * Ext + .30 * Flare;
	double Sweep = .55 + .35 * Tuck - .22 * Ext - .40 * Flare - .10 * PitchSm;
	double Spread = .12 + .10 * Ext + .22 * Flare - .08 * Tuck;
	double Knee = .04 + .30 * Flare - .08 * Tuck + .12 * FMath::Abs(FineSm);
	double Lift = .05 + .10 * Ext + .12 * Flare;
	const double K = 1 - FMath::Exp(-Dt / .18);   // weight shifts take time
	PoseArch += (Arch - PoseArch) * K; PoseHead += (Head - PoseHead) * K; PoseSpan += (Span - PoseSpan) * K;
	PoseSweep += (Sweep - PoseSweep) * K; PoseSpread += (Spread - PoseSpread) * K; PoseKnee += (Knee - PoseKnee) * K; PoseLift += (Lift - PoseLift) * K;
	Wobble += ((FMath::Sin(FlyTime * 3.1) + .6 * FMath::Sin(FlyTime * 7.3)) * WTurb - Wobble) * (1 - FMath::Exp(-Dt / .15));

	FWsPose Pose;
	Pose.Arch = PoseArch; Pose.HeadPitch = PoseHead; Pose.Span = PoseSpan; Pose.Sweep = PoseSweep; Pose.LegSpread = PoseSpread;
	Pose.Knee = PoseKnee; Pose.Lift = PoseLift; Pose.Time = FlyTime;
	Pose.Amp = .004 + .014 * Spd01 * Spd01 + .02 * WTurb + (bStalled ? .03 : 0.0);   // fabric buffets with speed, turbulence and stall

	if (Rig.bReady)
	{
		TArray<FTransform> Local; TArray<FVector> Pos;
		Rig.Pose(Pose, Local, Pos);
		const int32 Ci = Rig.Find(TEXT("spine_03"));
		const FVector Center = Ci >= 0 ? Pos[Ci] : FVector::ZeroVector;
		Manny->ApplyLocal(Local);
		Manny->SetRelativeLocation(-Center);
		FWsFlyerGeo G[4]; WsBuildFabric(Rig, Pos, Center, Pose, G);
		for (int32 S = 0; S < 4; S++)
		{
			TArray<FVector> N; TArray<FProcMeshTangent> Tg; TArray<FLinearColor> Col; Col.Init(FLinearColor::White, G[S].V.Num());
			UKismetProceduralMeshLibrary::CalculateTangentsForMesh(G[S].V, G[S].T, G[S].UV, N, Tg);
			if (!bFlyerBuilt) { FlyerMesh->CreateMeshSection_LinearColor(S, G[S].V, G[S].T, N, G[S].UV, Col, Tg, false); if (FlyerMats[S]) FlyerMesh->SetMaterial(S, FlyerMats[S].Get()); }
			else FlyerMesh->UpdateMeshSection_LinearColor(S, G[S].V, N, G[S].UV, Col, Tg);
		}
		bFlyerBuilt = true;
	}
	// view: turn the pilot toward the camera so the wing planform reads; lean with trim input and gusts
	FlyerRoot->SetRelativeRotation(FRotator(0, 40 + 2 * Wobble, 50.0 + FMath::RadiansToDegrees(.10 * FineSm + .04 * Wobble)));
	const bool bHide = (State == EWsState::Crash) || (State == EWsState::Result && Outcome == 4);
	FlyerRoot->SetVisibility(!bHide, true);
}

void AWingsuitPawn::DrawWorldFx(double Dt)
{
	UWorld* Wd = GetWorld(); if (!Wd) return;
	const FWsDiff& D = WsWorld::Diff();
	auto Px = [&](double Pix) { return float(Pix * 100.0 / CamS); };
	const double A = WsWorld::LZ_X - D.Lz / 2, B = WsWorld::LZ_X + D.Lz / 2;
	DrawDebugLine(Wd, ToW(A, .3), ToW(B, .3), FColor(214, 207, 116), false, -1, 0, Px(6));
	for (double X : { A, B }) { DrawDebugLine(Wd, ToW(X, 0), ToW(X, 12), FColor::White, false, -1, 0, Px(2)); DrawDebugLine(Wd, ToW(X, 12), ToW(X + 4, 10), FColor(255, 106, 42), false, -1, 0, Px(3)); }
	for (const FWsGate& G : Gates)
	{
		const FColor C = G.Res == 1 ? FColor(125, 255, 154) : G.Res == 2 ? FColor(255, 143, 143) : FColor::White;
		DrawDebugLine(Wd, ToW(G.X, G.Y - D.Gate), ToW(G.X, G.Y + D.Gate), C, false, -1, 0, Px(3));
		DrawDebugLine(Wd, ToW(G.X - 3, G.Y + D.Gate), ToW(G.X + 3, G.Y + D.Gate), C, false, -1, 0, Px(3));
		DrawDebugLine(Wd, ToW(G.X - 3, G.Y - D.Gate), ToW(G.X + 3, G.Y - D.Gate), C, false, -1, 0, Px(3));
	}
	for (int32 I = 1; I < Trail.Num(); I++)
	{
		const float F = float(I) / Trail.Num();
		DrawDebugLine(Wd, ToW(Trail[I - 1].X, Trail[I - 1].Y), ToW(Trail[I].X, Trail[I].Y), FColor(255, 255, 255, uint8(F * 200)), false, -1, 0, Px(1.5 + F * 3));
	}
	if (State == EWsState::Fly)
	{
		const double Hw = 1600 / CamS * .5, Hh = 900 / CamS * .5, Tn = Wd->GetTimeSeconds();
		for (int32 I = 0; I < 50; I++)
		{
			const double U = FMath::Frac(FMath::Sin(I * 12.9898) * 43758.5453), W2 = FMath::Frac(FMath::Sin(I * 78.233) * 12345.678);
			const double X = CamX + (FMath::Frac(U + Tn * -VX * .004) * 2 - 1) * Hw * 1.1, Y = CamY + (FMath::Frac(W2 + Tn * -VY * .004) * 2 - 1) * Hh * 1.1;
			const double Len = FMath::Clamp(V * .22, 1.0, 14.0);
			const double Ux = Wx - VX, Uy = Wy - VY, Ul = FMath::Sqrt(Ux * Ux + Uy * Uy) + 1e-4;
			DrawDebugLine(Wd, ToW(X, Y, 1), ToW(X + Ux / Ul * Len, Y + Uy / Ul * Len, 1), FColor(255, 255, 255, 70), false, -1, 0, Px(1));
		}
	}
	for (int32 I = Parts.Num() - 1; I >= 0; I--)
	{
		FWsPart& P = Parts[I]; P.Life -= Dt; if (P.Life <= 0) { Parts.RemoveAtSwap(I); continue; }
		P.V.Y -= 12 * Dt; P.P += P.V * Dt; DrawDebugPoint(Wd, ToW(P.P.X, P.P.Y), Px(5), P.C, false, -1);
	}
}

void AWingsuitPawn::Tick(float DtF)
{
	Super::Tick(DtF);
	const double Dt = FMath::Min(DtF, .05f);
	const bool bSpace = Pressed(EKeys::SpaceBar), bEnter = Pressed(EKeys::Enter), bR = Pressed(EKeys::R), bEsc = Pressed(EKeys::Escape), bTab = Pressed(EKeys::Tab);
	const bool bW = Pressed(EKeys::W) || Pressed(EKeys::Up), bS = Pressed(EKeys::S) || Pressed(EKeys::Down);
	static const FKey NumKeys[4] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four };
	for (int32 I = 0; I < 4; I++) if (Pressed(NumKeys[I]) && State == EWsState::Ready) WsWorld::SetDiff(I);
	if (bR) { ResetRun(); }
	else if (State == EWsState::Ready) { if (bSpace || bEnter) StartJump(); if (bTab) bFreeMode = !bFreeMode; }
	else if (State == EWsState::Result) { if (bEnter) ResetRun(); }
	else if (bEsc) { if (State == EWsState::Fly) State = EWsState::Paused; else if (State == EWsState::Paused) State = EWsState::Fly; }
	if (State == EWsState::Fly && (bW || bS))
	{
		const int32 Sg = bW ? 1 : -1;
		if (LastSign && Sg != LastSign && T - LastSignT < .7) Overs++;
		LastSign = Sg; LastSignT = T;
	}
	if (State == EWsState::Fly) { Accum += Dt; while (Accum >= 1.0 / 120) { FlightStep(1.0 / 120); Accum -= 1.0 / 120; if (State != EWsState::Fly) break; } }
	else if (State == EWsState::Slide) SlideStep(Dt);
	else if (State == EWsState::Crash) { EndT += Dt; if (EndT > 1.3) FinishRun(); }
	WarnTtc = -1;
	if (State == EWsState::Fly)
	{
		const double Spd = FMath::Sqrt(VX * VX + VY * VY);
		if (!(Agl < 30 && -VY < 7 && Spd < 32))
			for (double K = .2; K <= 3.5; K += .2) { const double X = PX + VX * K, Y = PY + VY * K; if (Y - 3 <= WsWorld::Ground(X) || WsWorld::TreeHit(X, Y)) { WarnTtc = K; break; } }
	}
	for (int32 I = Toasts.Num() - 1; I >= 0; I--) { Toasts[I].T += Dt; if (Toasts[I].T > 2.2) Toasts.RemoveAt(I); }
	if (WindAC)
	{
		const float S01 = float(FMath::Clamp(V / 70.0, 0.0, 1.4));
		WindAC->SetVolumeMultiplier(State == EWsState::Fly ? .08f + .85f * S01 : (State == EWsState::Paused ? 0.f : .04f));
		WindAC->SetPitchMultiplier(.8f + .55f * S01);
	}
	BeepT -= Dt;
	if (State == EWsState::Fly)
	{
		if (WarnTtc >= 0 && BeepT <= 0) { Sfx(TEXT("beep"), .5f); BeepT = WarnTtc < 1.8 ? .18 : .4; }
		else if (bStalled && BeepT <= 0) { Sfx(TEXT("beep"), .5f, .7f); BeepT = .7; }
		const bool bFl = Flare > .5;
		if (bFl && !bFlareSnd) Sfx(TEXT("flare"), .8f);
		bFlareSnd = bFl;
	}
	UpdateCamera(Dt); UpdateVisual(); DrawWorldFx(Dt);
}
