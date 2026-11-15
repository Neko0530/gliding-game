#include "WingsuitFlyer.h"
#include "ReferenceSkeleton.h"

namespace
{
	template <typename F> void Grid(FWsFlyerGeo& G, int32 NI, int32 NJ, F Fn)
	{
		const int32 Base = G.V.Num();
		for (int32 i = 0; i <= NI; i++) for (int32 j = 0; j <= NJ; j++)
		{
			G.V.Add(Fn(double(i) / NI, double(j) / NJ));
			G.UV.Add(FVector2D(double(i) / NI, double(j) / NJ));
		}
		for (int32 i = 0; i < NI; i++) for (int32 j = 0; j < NJ; j++)
		{
			const int32 a = Base + i * (NJ + 1) + j, b = a + 1, c = a + (NJ + 1), d = c + 1;
			G.T.Append({ a, c, b, b, c, d });
		}
	}

	void Ellipsoid(FWsFlyerGeo& G, FVector C, FVector R)
	{
		const int32 NI = 10, NJ = 14, Base = G.V.Num();
		for (int32 i = 0; i <= NI; i++) for (int32 j = 0; j < NJ; j++)
		{
			const double Th = PI * i / NI, Ph = 2 * PI * j / NJ;
			G.V.Add(C + FVector(R.X * FMath::Cos(Th), R.Y * FMath::Sin(Th) * FMath::Cos(Ph), R.Z * FMath::Sin(Th) * FMath::Sin(Ph)));
			G.UV.Add(FVector2D(double(j) / NJ, double(i) / NI));
		}
		for (int32 i = 0; i < NI; i++) for (int32 j = 0; j < NJ; j++)
		{
			const int32 j2 = (j + 1) % NJ, a = Base + i * NJ + j, b = Base + i * NJ + j2, c = Base + (i + 1) * NJ + j, d = Base + (i + 1) * NJ + j2;
			G.T.Append({ a, c, b, b, c, d });
		}
	}
}

void FWsRig::Init(const FReferenceSkeleton& Ref)
{
	const int32 N = Ref.GetNum();
	Parent.SetNum(N); RefLocal.SetNum(N); RefRotS.SetNum(N); RefLocS.SetNum(N); Idx.Reset();
	Rc = FRotator(0, -90, 0).Quaternion();                      // mesh space (faces +Y) -> actor space (faces +X)
	TArray<FQuat> MR; TArray<FVector> ML; MR.SetNum(N); ML.SetNum(N);
	for (int32 i = 0; i < N; i++)
	{
		Parent[i] = Ref.GetParentIndex(i);
		RefLocal[i] = Ref.GetRefBonePose()[i];
		Idx.Add(Ref.GetBoneName(i), i);
		const FQuat PQ = Parent[i] < 0 ? FQuat::Identity : MR[Parent[i]];
		const FVector PL = Parent[i] < 0 ? FVector::ZeroVector : ML[Parent[i]];
		MR[i] = (PQ * RefLocal[i].GetRotation()).GetNormalized();
		ML[i] = PL + PQ.RotateVector(RefLocal[i].GetTranslation());
		RefRotS[i] = (Rc * MR[i]).GetNormalized();
		RefLocS[i] = Rc.RotateVector(ML[i]);
	}
	bReady = N > 0;
}

void FWsRig::Pose(const FWsPose& P, TArray<FTransform>& OutLocal, TArray<FVector>& OutPos) const
{
	const int32 N = Parent.Num();
	OutLocal = RefLocal; OutPos.SetNum(N);
	TArray<FQuat> Des; Des.SetNum(N);
	TMap<int32, FVector> Dir; TMap<int32, int32> Child;
	auto Drive = [&](const FString& B, const FString& C, FVector D)
	{
		const int32 b = Find(*B), c = Find(*C);
		if (b >= 0 && c >= 0) { Dir.Add(b, D.GetSafeNormal()); Child.Add(b, c); }
	};
	const double A = P.Arch, H = P.HeadPitch, Sw = P.Sweep;
	auto Sp = [](double s) { return FVector(1, 0, s); };
	// spine: lies along +X, arched by posture
	Drive("pelvis", "spine_01", Sp(0));
	Drive("spine_01", "spine_02", Sp(.10 * A));
	Drive("spine_02", "spine_03", Sp(.14 * A));
	Drive("spine_03", "spine_04", Sp(.18 * A));
	Drive("spine_04", "spine_05", Sp(.18 * A));
	Drive("spine_05", "neck_01", Sp(.20 * A + .10 * H));
	Drive("neck_01", "neck_02", Sp(.35 + .30 * H));
	Drive("neck_02", "head", Sp(.55 + .30 * H));
	for (int32 s = 0; s < 2; s++)
	{
		const FString L = s ? TEXT("r") : TEXT("l");
		const double Sg = s ? -1.0 : 1.0;      // pilot's left is +Y in S space
		Drive(TEXT("clavicle_") + L, TEXT("upperarm_") + L, FVector(.10, Sg, .05));
		Drive(TEXT("upperarm_") + L, TEXT("lowerarm_") + L, FVector(-FMath::Sin(Sw), Sg * FMath::Cos(Sw), P.Lift));
		Drive(TEXT("lowerarm_") + L, TEXT("hand_") + L, FVector(-FMath::Sin(Sw + .06), Sg * FMath::Cos(Sw + .06), P.Lift));
		Drive(TEXT("thigh_") + L, TEXT("calf_") + L, FVector(-1, Sg * FMath::Sin(P.LegSpread), FMath::Sin(P.Knee * .5)));
		Drive(TEXT("calf_") + L, TEXT("foot_") + L, FVector(-1, Sg * FMath::Sin(P.LegSpread) * .8, FMath::Sin(P.Knee)));
		Drive(TEXT("foot_") + L, TEXT("ball_") + L, FVector(-1, 0, .12));
	}
	for (int32 i = 0; i < N; i++)
	{
		const FQuat PR = Parent[i] < 0 ? Rc : Des[Parent[i]];
		const FVector PL = Parent[i] < 0 ? FVector::ZeroVector : OutPos[Parent[i]];
		FQuat D = PR * RefLocal[i].GetRotation();
		if (const FVector* d = Dir.Find(i))
		{
			const FVector RefDir = (RefLocS[Child[i]] - RefLocS[i]).GetSafeNormal();
			D = FQuat::FindBetweenNormals(RefDir, *d) * RefRotS[i];
		}
		Des[i] = D.GetNormalized();
		OutLocal[i].SetRotation((PR.Inverse() * Des[i]).GetNormalized());
		OutPos[i] = PL + PR.RotateVector(RefLocal[i].GetTranslation());
	}
}

void WsBuildFabric(const FWsRig& Rig, const TArray<FVector>& Pos, const FVector& Center, const FWsPose& P, FWsFlyerGeo Out[4])
{
	for (int32 i = 0; i < 4; i++) { Out[i].V.Reset(); Out[i].T.Reset(); Out[i].UV.Reset(); }
	FWsFlyerGeo& Fab = Out[0]; FWsFlyerGeo& Trim = Out[1]; FWsFlyerGeo& Gear = Out[2]; FWsFlyerGeo& Pack = Out[3];
	auto Pm = [&](const FString& N) -> FVector { const int32 i = Rig.Find(*N); return i >= 0 ? (Pos[i] - Center) * .01 : FVector::ZeroVector; };
	FVector Ankle[2];
	const double Amp = P.Amp;
	for (int32 s = 0; s < 2; s++)
	{
		const FString L = s ? TEXT("r") : TEXT("l");
		const double Sg = s ? -1.0 : 1.0;
		const FVector Sh = Pm(TEXT("upperarm_") + L) + FVector(0, 0, .01), Wrist = Pm(TEXT("hand_") + L) + FVector(0, 0, .01);
		const FVector HipP = Pm(TEXT("thigh_") + L) + FVector(0, 0, .02);
		Ankle[s] = Pm(TEXT("foot_") + L) + FVector(0, 0, .02);
		const FVector WristBack = Wrist + FVector(-.38 + .10 * P.Sweep, 0, 0);
		auto ArmPt = [&](double S2, double C) {
			const FVector LE = Sh + (Wrist - Sh) * S2;
			const FVector TE = HipP + (WristBack - HipP) * S2;
			FVector Pt = LE + (TE - LE) * C;
			Pt.Z += FMath::Sin(PI * FMath::Min(C * 1.4, 1.0)) * .055 * (1 - .55 * S2);
			Pt.Z += Amp * FMath::Sin(P.Time * 17 + S2 * 6 + C * 3 + Sg) * FMath::Pow(C, 1.6) * (.4 + S2);
			return Pt; };
		Grid(Fab, 14, 8, [&](double S2, double C) { return ArmPt(S2, .05 + C * .90); });
		Grid(Trim, 14, 1, [&](double S2, double C) { return ArmPt(S2, C * .05); });
		Grid(Trim, 14, 1, [&](double S2, double C) { return ArmPt(S2, .95 + C * .05); });
		Grid(Trim, 1, 8, [&](double S2, double C) { return ArmPt(.95 + S2 * .05, C); });
	}
	// tail wing between the legs
	{
		const FVector Crotch = Pm(TEXT("pelvis")) + FVector(-.05, 0, .02);
		Grid(Fab, 10, 12, [&](double A, double B) {
			const FVector E = FMath::Lerp(Ankle[0], Ankle[1], B);
			FVector Pt = Crotch + (E - Crotch) * A;
			Pt.Z += FMath::Sin(PI * B) * .02 * A + Amp * FMath::Sin(P.Time * 19 + A * 5 + B * 4) * A * A;
			return Pt; });
	}
	// helmet and container
	const FVector Head = Pm(TEXT("head"));
	Ellipsoid(Gear, Head + FVector(.07, 0, .035), FVector(.13, .105, .115));
	Ellipsoid(Pack, Pm(TEXT("spine_03")) + FVector(-.02, 0, .15), FVector(.24, .17, .08));
}
