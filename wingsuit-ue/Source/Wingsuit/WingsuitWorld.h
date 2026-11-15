#pragma once
#include "CoreMinimal.h"

struct FWsDiff { const TCHAR* Name; double Lz, Wind, Turb, Tol, Gate, Mult; int32 Hud; double Pen; bool bHints; };

struct FWsTree { double X, H, Th, W, C; };
struct FWsWind { double X = 0, Y = 0, Turb = 0; };
struct FWsUpdraft { double X, R, S, Top; };

/** Pure data/maths for the course. All units are metres (X forward, Y up in these helpers). */
namespace WsWorld
{
	constexpr double LZ_X = 2950.0;
	constexpr double X0 = -800.0, X1 = 4800.0;

	const FWsDiff& Diff();
	int32 DiffIndex();
	void SetDiff(int32 Index);

	double Ground(double X);
	/** Height at depth Z metres behind the flight plane (scenery only). */
	double GroundAt(double X, double Depth);
	bool TreeHit(double X, double Y);
	const TArray<FWsTree>& Trees();
	const TArray<FWsUpdraft>& Updrafts();
	void Gates(TArray<FVector2D>& Out);
	double TurbAt(double X);
	FWsWind WindAt(double X, double Y, double T);
	double TurbPitchNoise(double T);
}
