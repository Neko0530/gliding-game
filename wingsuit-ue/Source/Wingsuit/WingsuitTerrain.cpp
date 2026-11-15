#include "WingsuitTerrain.h"
#include "WingsuitWorld.h"
#include "ProceduralMeshComponent.h"
#include "KismetProceduralMeshLibrary.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"

namespace
{
	constexpr double M2U = 100.0;
	double Sm(double A, double B, double X) { const double T = FMath::Clamp((X - A) / (B - A), 0.0, 1.0); return T * T * (3 - 2 * T); }
	// cheap smooth value noise from a hashed lattice
	double Hash(int32 X, int32 Y) { uint32 H = uint32(X) * 374761393u + uint32(Y) * 668265263u; H = (H ^ (H >> 13)) * 1274126177u; return double((H ^ (H >> 16)) & 0xFFFF) / 32768.0 - 1.0; }
	double Noise2(double X, double Y)
	{
		const int32 Xi = FMath::FloorToInt(X), Yi = FMath::FloorToInt(Y); const double Fx = X - Xi, Fy = Y - Yi;
		const double Sx = Fx * Fx * (3 - 2 * Fx), Sy = Fy * Fy * (3 - 2 * Fy);
		return FMath::Lerp(FMath::Lerp(Hash(Xi, Yi), Hash(Xi + 1, Yi), Sx), FMath::Lerp(Hash(Xi, Yi + 1), Hash(Xi + 1, Yi + 1), Sx), Sy);
	}
	double Fbm(double X, double Y) { return Noise2(X, Y) * .55 + Noise2(X * 2.1, Y * 2.1) * .28 + Noise2(X * 4.3, Y * 4.3) * .17; }
	FLinearColor Mix(const FLinearColor& A, const FLinearColor& B, double T) { return FMath::Lerp(A, B, float(FMath::Clamp(T, 0.0, 1.0))); }

	const FLinearColor GrassA(.11f, .29f, .045f), GrassB(.19f, .35f, .06f), Dirt(.20f, .15f, .09f), RockA(.30f, .26f, .22f), RockB(.18f, .15f, .13f), Snow(.92f, .95f, 1.f);

	FLinearColor TopColor(double X, double H, double Slope, double D)
	{
		const double N = Fbm(X * .02, D * .02);
		FLinearColor C = Mix(GrassA, GrassB, N * .5 + .5);
		C = Mix(C, Dirt, Sm(300, 520, H) * (.35 + .4 * N));
		const double Rk = FMath::Max(Sm(.45, .85, Slope), Sm(420, 650, H + N * 60));
		C = Mix(C, Mix(RockA, RockB, N * .5 + .5), Rk);
		const double Sn = Sm(880, 980, H + N * 70) * (1 - Sm(.8, 1.3, Slope) * .6);
		return Mix(C, Snow, Sn);
	}
}

AWingsuitTerrain::AWingsuitTerrain()
{
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->bUseAsyncCooking = false;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TreesA = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TreesA"));
	TreesB = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TreesB"));
	for (UInstancedStaticMeshComponent* C : { TreesA.Get(), TreesB.Get() }) { C->SetupAttachment(Mesh); C->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	TreesA->SetStaticMesh(Cone.Object); TreesB->SetStaticMesh(Cone.Object);
}

void AWingsuitTerrain::OnConstruction(const FTransform& Xf) { Super::OnConstruction(Xf); }
void AWingsuitTerrain::BeginPlay() { Super::BeginPlay(); if (!bBuilt) Build(); }

void AWingsuitTerrain::Build()
{
	bBuilt = true;
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_Matte.M_Matte"));
	auto Mid = [&](FLinearColor C) { UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, this); if (M) M->SetVectorParameterValue(TEXT("Color"), C); return M; };
	auto TexMid = [&](const TCHAR* Path) -> UMaterialInterface* {
		UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, Path);
		return M ? M : static_cast<UMaterialInterface*>(Mid(FLinearColor::White)); };
	UMaterialInterface* TopMat = TexMid(TEXT("/Game/Materials/M_TerrainTop.M_TerrainTop"));
	UMaterialInterface* CliffMat = TexMid(TEXT("/Game/Materials/M_TerrainCliff.M_TerrainCliff"));

	const double StepX = 4.0, XA = -3200.0, XB = 7200.0;
	const int32 NX = int32((XB - XA) / StepX) + 1;
	auto XOf = [&](int32 I) { return XA + I * StepX; };
	auto Slope = [&](double X) { return FMath::Abs(WsWorld::Ground(X + StepX) - WsWorld::Ground(X - StepX)) / (2 * StepX); };

	// ---- top surface: depth rows behind the flight plane ----
	static const double Depths[] = { 3, 10, 25, 50, 90, 150, 230, 340, 480, 650 };
	const int32 NZ = UE_ARRAY_COUNT(Depths);
	{
		TArray<FVector> V; TArray<FVector2D> UV; TArray<FLinearColor> Col; TArray<int32> Tri;
		V.Reserve(NX * NZ);
		for (int32 J = 0; J < NZ; J++) for (int32 I = 0; I < NX; I++)
		{
			const double X = XOf(I), D = Depths[J];
			// rolling foothills, then a distant range that closes the horizon
			const double Range = 0.0;
			const double H = WsWorld::GroundAt(X, D) + Range;
			V.Add(FVector(X * M2U, -D * M2U, H * M2U)); UV.Add(FVector2D(X * .01, D * .01));
			Col.Add(TopColor(X, H, Slope(X) * (1 - Sm(300, 1500, D)) + Sm(600, 3000, D) * .55 * (.5 + .5 * Fbm(X * .003, D * .003)), D));
		}
		for (int32 J = 0; J + 1 < NZ; J++) for (int32 I = 0; I + 1 < NX; I++)
		{
			const int32 A = J * NX + I, B = A + 1, C = A + NX, D = C + 1;
			Tri.Append({ A, B, C, B, D, C });
		}
		TArray<FVector> N; TArray<FProcMeshTangent> Tg;
		UKismetProceduralMeshLibrary::CalculateTangentsForMesh(V, Tri, UV, N, Tg);
		for (FVector& Nn : N) if (Nn.Z < 0) Nn = -Nn;
		Mesh->CreateMeshSection_LinearColor(0, V, Tri, N, UV, Col, Tg, false);
	}

	// ---- cliff face toward the camera: rough rock with strata and a turf / snow lip ----
	{
		const int32 NR = 18;
		TArray<FVector> V; TArray<FVector2D> UV; TArray<FLinearColor> Col; TArray<int32> Tri;
		V.Reserve(NX * NR);
		for (int32 I = 0; I < NX; I++)
		{
			const double X = XOf(I), H = WsWorld::Ground(X);
			for (int32 J = 0; J < NR; J++)
			{
				const double T = double(J) / (NR - 1), Z = H - FMath::Pow(T, 1.4) * (H + 80.0);
				const double Rough = (Fbm(X * .035, Z * .05) * 5.0 + Fbm(X * .12, Z * .10) * 1.6) * Sm(0, .12, T);
				V.Add(FVector(X * M2U, (-3.0 + Rough) * M2U, Z * M2U)); UV.Add(FVector2D(X * .01, Z * .01));
				const double Strata = .5 + .5 * FMath::Sin(Z * .42 + Fbm(X * .05, Z * .08) * 3.0);
				FLinearColor C = Mix(RockB, RockA, Strata * .7 + Fbm(X * .08, Z * .15) * .3);
				C = Mix(C, Dirt, .25 * Sm(.0, .5, Fbm(X * .03, Z * .03)));
				const double Cap = 1 - Sm(0, .06, T);
				const double Gr = 1 - Sm(380, 520, H), Sn = Sm(880, 980, H);
				C = Mix(C, Mix(GrassA, Snow, Sn), Cap * (Gr * .8 + Sn));
				Col.Add(C);
			}
		}
		for (int32 I = 0; I + 1 < NX; I++) for (int32 J = 0; J + 1 < NR; J++)
		{
			const int32 A = I * NR + J, B = A + 1, C = A + NR, D = C + 1;
			Tri.Append({ A, B, C, C, B, D });
		}
		TArray<FVector> N; TArray<FProcMeshTangent> Tg;
		UKismetProceduralMeshLibrary::CalculateTangentsForMesh(V, Tri, UV, N, Tg);
		for (FVector& Nn : N) if (Nn.Y < 0) Nn = -Nn;
		Mesh->CreateMeshSection_LinearColor(1, V, Tri, N, UV, Col, Tg, false);
	}
	Mesh->SetMaterial(0, TopMat); Mesh->SetMaterial(1, CliffMat);
	Mesh->SetCastShadow(true);

	// ---- conifers (trunk plus three tiers), boulders and bushes ----
	UStaticMesh* Sph = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UStaticMesh* Cyl = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	auto MakeISM = [&](UStaticMesh* M, UMaterialInterface* Mat, const TCHAR* Nm) -> UInstancedStaticMeshComponent*
	{
		UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(this, Nm);
		C->SetStaticMesh(M); C->SetMaterial(0, Mat); C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetupAttachment(Mesh); C->RegisterComponent(); return C;
	};
	UInstancedStaticMeshComponent* Trunks = MakeISM(Cyl, Mid(FLinearColor(.10f, .055f, .03f)), TEXT("Trunks"));
	UInstancedStaticMeshComponent* Rocks = MakeISM(Sph, Mid(FLinearColor(.26f, .24f, .22f)), TEXT("Boulders"));
	UInstancedStaticMeshComponent* BushA = MakeISM(Sph, Mid(FLinearColor(.10f, .28f, .05f)), TEXT("BushA"));
	UInstancedStaticMeshComponent* BushB = MakeISM(Sph, Mid(FLinearColor(.19f, .35f, .06f)), TEXT("BushB"));
	TreesA->SetMaterial(0, Mid(FLinearColor(.02f, .12f, .04f)));
	TreesB->SetMaterial(0, Mid(FLinearColor(.05f, .17f, .05f)));
	FRandomStream Rng(77);
	auto AddTree = [&](UInstancedStaticMeshComponent* C, double X, double D, double H, double W, double Th)
	{
		const double Y = -D * M2U;
		Trunks->AddInstance(FTransform(FRotator::ZeroRotator, FVector(X * M2U, Y, (H + Th * .12) * M2U), FVector(W * .16, W * .16, Th * .26)));
		C->AddInstance(FTransform(FRotator::ZeroRotator, FVector(X * M2U, Y, (H + Th * .14) * M2U), FVector(W, W, Th * .50)));
		C->AddInstance(FTransform(FRotator::ZeroRotator, FVector(X * M2U, Y, (H + Th * .42) * M2U), FVector(W * .72, W * .72, Th * .42)));
		C->AddInstance(FTransform(FRotator::ZeroRotator, FVector(X * M2U, Y, (H + Th * .66) * M2U), FVector(W * .48, W * .48, Th * .36)));
	};
	for (const FWsTree& T : WsWorld::Trees())
	{
		UInstancedStaticMeshComponent* C = T.C < .5 ? TreesA.Get() : TreesB.Get();
		AddTree(C, T.X, 4, T.H, T.W, T.Th);
		for (int32 K = 0; K < 9; K++)
		{
			const double D = 8 + Rng.FRand() * 220, X = T.X + (Rng.FRand() - .5) * 40, S = .8 + Rng.FRand() * .6;
			AddTree(Rng.FRand() < .5f ? TreesA.Get() : TreesB.Get(), X, D, WsWorld::GroundAt(X, D), T.W * S, T.Th * S);
		}
	}
	for (double X = -300; X < 4200; X += Rng.FRandRange(3.f, 9.f))
	{
		if (FMath::Abs(X - WsWorld::LZ_X) < 120) continue;
		const double D = Rng.FRand() < .45f ? Rng.FRandRange(3.f, 12.f) : Rng.FRandRange(12.f, 260.f), H = WsWorld::GroundAt(X, D);
		if (H < 4) continue;
		if (Rng.FRand() < .35f)
		{
			const double S = Rng.FRandRange(.8f, 4.5f);
			Rocks->AddInstance(FTransform(FRotator(0, Rng.FRand() * 360.f, 0), FVector(X * M2U, -D * M2U, (H + S * .22) * M2U), FVector(S * 1.2, S * .9, S * .7)));
		}
		else
		{
			const double S = Rng.FRandRange(.8f, 2.4f);
			(Rng.FRand() < .5f ? BushA : BushB)->AddInstance(FTransform(FRotator::ZeroRotator, FVector(X * M2U, -D * M2U, (H + S * .3) * M2U), FVector(S * 1.5, S * 1.3, S)));
		}
	}
}
