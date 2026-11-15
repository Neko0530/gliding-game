#include "WingsuitWorld.h"

namespace
{
	double Clamp(double V, double A, double B) { return FMath::Clamp(V, A, B); }
	double Smooth(double A, double B, double X) { const double T = Clamp((X - A) / (B - A), 0, 1); return T * T * (3 - 2 * T); }

	struct FRng
	{
		uint32 S;
		explicit FRng(uint32 Seed) : S(Seed) {}
		double Next()
		{
			S += 0x6D2B79F5u;
			uint32 T = (S ^ (S >> 15)) * (1u | S);
			T = (T + ((T ^ (T >> 7)) * (61u | T))) ^ T;
			return double(T ^ (T >> 14)) / 4294967296.0;
		}
	};

	struct FNoise
	{
		float P[256];
		explicit FNoise(uint32 Seed) { FRng R(Seed); for (float& V : P) V = float(R.Next() * 2 - 1); }
		double Get(double X) const
		{
			const int32 I = FMath::FloorToInt(X); const double F = X - I;
			const double A = P[I & 255], B = P[(I + 1) & 255], T = F * F * (3 - 2 * F);
			return A + (B - A) * T;
		}
	};

	const double Knots[][2] = { {-700,1500},{0,1500},{45,880},{300,820},{560,900},{820,540},{1150,740},{1450,470},{1800,500},
		{2100,320},{2400,250},{2650,90},{2800,0},{3100,0},{3300,40},{3600,200},{4000,600},{4800,900} };
	const double Spires[][3] = { {1180,50,18},{1830,60,22},{2430,45,16},{700,80,18},{980,110,20},{1450,100,20},{2050,120,24},{2260,95,18},{2720,75,16} };

	double KnotH(double X)
	{
		const int32 N = UE_ARRAY_COUNT(Knots);
		if (X <= Knots[0][0]) return Knots[0][1];
		for (int32 I = 1; I < N; I++)
		{
			if (X <= Knots[I][0])
			{
				const double T = (X - Knots[I - 1][0]) / (Knots[I][0] - Knots[I - 1][0]);
				const double S = (1 - FMath::Cos(PI * T)) / 2;
				return Knots[I - 1][1] + (Knots[I][1] - Knots[I - 1][1]) * S;
			}
		}
		return Knots[N - 1][1];
	}

	const FNoise NT1(11), NT2(23), NGust(101), NTb1(202), NTb2(303), NTb3(404);
	constexpr double DX = 2.0;
	const int32 NS = int32((WsWorld::X1 - WsWorld::X0) / DX) + 1;
	TArray<float> Hs;
	TArray<FWsTree> TreeList;
	int32 DiffIdx = 1;

	const FWsDiff Diffs[4] = {
		{ TEXT("Easy"),   220, .45, .35, 1.3, 70, .8,  3, .7, true },
		{ TEXT("Normal"), 160, 1,   1,   1,   55, 1,   3, 1,  false },
		{ TEXT("Hard"),   110, 1.5, 1.5, .9,  40, 1.25, 2, 1.3, false },
		{ TEXT("Expert"), 70,  2,   2,   .8,  30, 1.6, 1, 1.6, false } };

	const FWsUpdraft Ups[3] = { {1160,75,5.5,260}, {1810,80,5,260}, {2420,70,4.5,220} };

	void Build()
	{
		if (Hs.Num()) return;
		Hs.SetNumUninitialized(NS);
		for (int32 I = 0; I < NS; I++)
		{
			const double X = WsWorld::X0 + I * DX; double H = KnotH(X);
			const double Calm = Smooth(170, 320, FMath::Abs(X - WsWorld::LZ_X)) * Smooth(-30, 60, X);
			H += Calm * (NT1.Get(X * 0.012) * (10 + 0.03 * H) + NT2.Get(X * 0.05) * 4);
			for (const auto& S : Spires) { const double D = FMath::Abs(X - S[0]) / S[2]; if (D < 1) H += S[1] * FMath::Pow(1 - D, 1.4); }
			Hs[I] = float(FMath::Max(0.0, H));
		}
		FRng R(5); double X = 250;
		while (X < 2800)
		{
			X += 6 + R.Next() * 10;
			const bool bZone = (X > 250 && X < 1500) || (X > 1900 && X < 2750);
			if (!bZone) continue;
			const double H = WsWorld::Ground(X);
			if (H < 25 || H > 760) continue;
			if (FMath::Abs(WsWorld::Ground(X + 6) - WsWorld::Ground(X - 6)) / 12 > 0.85) continue;
			if (FMath::Abs(X - WsWorld::LZ_X) < 260) continue;
			FWsTree T; T.X = X; T.H = H; T.Th = 12 + R.Next() * 14; T.W = 6 + R.Next() * 6; T.C = R.Next();
			TreeList.Add(T);
		}
	}
}

namespace WsWorld
{
	const FWsDiff& Diff() { return Diffs[DiffIdx]; }
	int32 DiffIndex() { return DiffIdx; }
	void SetDiff(int32 Index) { DiffIdx = FMath::Clamp(Index, 0, 3); }

	double Ground(double X)
	{
		Build();
		const double F = (X - X0) / DX;
		if (F <= 0) return Hs[0];
		if (F >= NS - 1) return Hs[NS - 1];
		const int32 I = int32(F); const double T = F - I;
		return Hs[I] + (Hs[I + 1] - Hs[I]) * T;
	}

	double GroundAt(double X, double Depth)
	{
		const double G = Ground(X);
		const double K = Smooth(4, 90, Depth) * Smooth(-100, 400, X) * (1 - Smooth(2600, 2850, X) * (1 - Smooth(60, 250, Depth)));
		return G + K * (NT2.Get(X * 0.006 + Depth * 0.0011) * 25 + 110 * (1 - FMath::Exp(-Depth / 1200)));
	}

	const TArray<FWsTree>& Trees() { Build(); return TreeList; }
	const TArray<FWsUpdraft>& Updrafts() { static TArray<FWsUpdraft> U(Ups, 3); return U; }
	void Gates(TArray<FVector2D>& Out) { Out = { {700,1050},{1300,810},{2000,560},{2550,330} }; }

	bool TreeHit(double X, double Y)
	{
		Build();
		int32 Lo = 0, Hi = TreeList.Num();
		while (Lo < Hi) { const int32 M = (Lo + Hi) >> 1; if (TreeList[M].X < X - 8) Lo = M + 1; else Hi = M; }
		for (int32 I = Lo; I < TreeList.Num() && TreeList[I].X < X + 8; I++)
		{
			const FWsTree& T = TreeList[I]; const double Dx = FMath::Abs(X - T.X), Hw = T.W / 2;
			if (Dx < Hw && Y <= T.H + T.Th * (1 - Dx / Hw)) return true;
		}
		return false;
	}

	double TurbAt(double X) { return Smooth(1550, 1750, X) * (1 - Smooth(2150, 2350, X)); }

	FWsWind WindAt(double X, double Y, double T)
	{
		const FWsDiff& D = Diff();
		FWsWind W; W.X = -2.5 * D.Wind + NGust.Get(T * 0.35 + X * 0.0015) * 4.5 * D.Wind;
		for (const FWsUpdraft& U : Ups)
		{
			const double Dx = (X - U.X) / U.R; if (FMath::Abs(Dx) > 2.5) continue;
			const double Hh = Y - Ground(U.X);
			if (Hh > -20 && Hh < U.Top) W.Y += U.S * D.Wind * FMath::Exp(-Dx * Dx) * Smooth(0, 40, Hh) * (1 - Smooth(U.Top * .6, U.Top, Hh));
		}
		const double Tz = TurbAt(X) * D.Turb;
		if (Tz > 0) { W.X += NTb1.Get(T * 2.2 + X * .01) * 5 * Tz; W.Y += NTb2.Get(T * 2.7 + X * .013) * 4.5 * Tz; }
		W.Turb = Tz; return W;
	}

	double TurbPitchNoise(double T) { return NTb3.Get(T * 3.1); }
}
