#pragma once
#include "CoreMinimal.h"

struct FReferenceSkeleton;

/** Posture driving the flyer (angles in radians). */
struct FWsPose
{
	double Arch = 0;        // + = chest up / back arched (flare), - = hunched (tuck)
	double HeadPitch = 0;   // + = head up
	double Knee = 0;        // leg trail angle (up)
	double Sweep = .6;      // arm sweep back from the span axis
	double Span = .8;
	double LegSpread = .15; // half-angle between legs
	double Lift = 0;        // arm elevation
	double Amp = .006;      // fabric flutter amplitude (m)
	double Time = 0;
};

struct FWsFlyerGeo
{
	TArray<FVector> V;
	TArray<int32> T;
	TArray<FVector2D> UV;
};

/**
 * Poses a UE mannequin skeleton into a wingsuit flight position. Everything is computed in "S space":
 * X forward, Y to the pilot's left, Z up (back of the pilot). Local bone transforms are returned for the poseable mesh.
 */
struct FWsRig
{
	bool bReady = false;
	TArray<int32> Parent;
	TArray<FTransform> RefLocal;
	TArray<FQuat> RefRotS;
	TArray<FVector> RefLocS;
	TMap<FName, int32> Idx;
	FQuat Rc;

	void Init(const FReferenceSkeleton& Ref);
	int32 Find(const TCHAR* Name) const { const int32* I = Idx.Find(FName(Name)); return I ? *I : -1; }
	void Pose(const FWsPose& P, TArray<FTransform>& OutLocal, TArray<FVector>& OutPos) const;
};

/** Sections: 0 white fabric, 1 orange trim, 2 helmet (black), 3 container (tan). Positions are metres relative to Center (cm). */
void WsBuildFabric(const FWsRig& Rig, const TArray<FVector>& Pos, const FVector& Center, const FWsPose& P, FWsFlyerGeo Out[4]);
