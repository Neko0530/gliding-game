#include "WingsuitHUD.h"
#include "WingsuitPawn.h"
#include "WingsuitWorld.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

void AWingsuitHUD::Text(const FString& S, float X, float Y, float Scale, FLinearColor C, bool bCenter, bool bRight)
{
	UFont* F = GEngine->GetMediumFont();
	float W = 0, H = 0; GetTextSize(S, W, H, F, Scale);
	if (bCenter) X -= W * .5f; else if (bRight) X -= W;
	DrawText(S, FLinearColor(0, 0, 0, C.A * .6f), X + 1.5f, Y + 1.5f, F, Scale);
	DrawText(S, C, X, Y, F, Scale);
}

void AWingsuitHUD::DrawHUD()
{
	Super::DrawHUD();
	const AWingsuitPawn* P = Cast<AWingsuitPawn>(GetOwningPawn());
	if (!P || !Canvas) return;
	const FWsDiff& D = WsWorld::Diff();
	const float W = Canvas->SizeX, H = Canvas->SizeY, U = FMath::Max(1.f, H / 900.f);
	const FLinearColor White(1, 1, 1, .92f), Dim(1, 1, 1, .55f);
	const double Now = GetWorld()->GetTimeSeconds();

	if (P->State == EWsState::Ready)
	{
		Text(TEXT("WINGSUIT"), W * .5f, H * .12f, 3.f * U, White, true);
		Text(TEXT("Press SPACE to jump"), W * .5f, H * .22f, 1.6f * U, White, true);
		Text(FString::Printf(TEXT("Difficulty: %s   (keys 1-4)     Mode: %s   (Tab)     Landing zone %d m ahead"), D.Name, P->bFreeMode ? TEXT("Free flight") : TEXT("Scored mission"), int32(WsWorld::LZ_X - P->PX)), W * .5f, H * .22f + 42 * U, 1.f * U, Dim, true);
		Text(TEXT("W/S pitch   A/D trim   Shift tuck   Space stabilise   Ctrl/F flare   R restart   Esc pause"), W * .5f, H * .22f + 66 * U, 1.f * U, Dim, true);
		return;
	}
	if (P->State == EWsState::Paused) { Text(TEXT("PAUSED  -  Esc to resume, R to restart"), W * .5f, H * .4f, 1.8f * U, White, true); return; }
	if (P->State == EWsState::Result)
	{
		const float Cw = FMath::Min(W - 40.f, 720.f * U), X0 = (W - Cw) * .5f, Y0 = H * .14f;
		const float Ch = (170 + P->ResultLines.Num() * 30) * U;
		DrawRect(FLinearColor(.03f, .05f, .09f, .85f), X0, Y0, Cw, Ch);
		Text(P->ResultTitle, X0 + 24 * U, Y0 + 18 * U, 1.8f * U, White);
		Text(P->ResultSub, X0 + 24 * U, Y0 + 52 * U, 1.f * U, Dim);
		if (!P->ResultGrade.IsEmpty()) Text(P->ResultGrade, X0 + Cw - 24 * U, Y0 + 14 * U, 3.2f * U, FLinearColor(1.f, .54f, .24f), false, true);
		float Y = Y0 + 90 * U;
		for (const FString& L : P->ResultLines) { Text(L, X0 + 24 * U, Y, 1.05f * U, White); Y += 30 * U; }
		if (!P->bFreeMode) Text(FString::Printf(TEXT("%d   x%.2f %s"), P->ResultTotal, D.Mult, D.Name), X0 + 24 * U, Y + 6 * U, 1.7f * U, FLinearColor(1.f, .54f, .24f));
		Text(P->ResultBest, X0 + 24 * U, Y + 40 * U, 1.f * U, Dim);
		Text(TEXT("R / Enter: retry"), X0 + 24 * U, Y0 + Ch - 36 * U, 1.f * U, Dim);
		return;
	}

	const double Agl = FMath::Max(0.0, P->Agl), Spd = P->V * 3.6;
	if (D.Hud >= 1) { Text(TEXT("ALT"), 24 * U, 20 * U, .8f * U, Dim); Text(FString::Printf(TEXT("%d m"), int32(Agl)), 24 * U, 38 * U, (D.Hud == 1 ? 1.6f : 2.2f) * U, White); }
	if (D.Hud >= 2)
	{
		Text(TEXT("SPEED"), 24 * U, 92 * U, .8f * U, Dim); Text(FString::Printf(TEXT("%d km/h"), int32(Spd)), 24 * U, 108 * U, 1.5f * U, White);
		const double Mag = FMath::Sqrt(P->Wx * P->Wx + P->Wy * P->Wy);
		const FVector2D C(W - 70 * U, 62 * U); const double Ang = FMath::Atan2(P->Wy, P->Wx);
		const FVector2D Tip = C + FVector2D(FMath::Cos(Ang), -FMath::Sin(Ang)) * 22 * U, Tail = C - FVector2D(FMath::Cos(Ang), -FMath::Sin(Ang)) * 22 * U;
		DrawLine(Tail.X, Tail.Y, Tip.X, Tip.Y, FLinearColor::White, 3.f);
		DrawLine(Tip.X, Tip.Y, Tip.X - 10 * U * FMath::Cos(Ang - .5) , Tip.Y + 10 * U * FMath::Sin(Ang - .5), FLinearColor::White, 3.f);
		DrawLine(Tip.X, Tip.Y, Tip.X - 10 * U * FMath::Cos(Ang + .5) , Tip.Y + 10 * U * FMath::Sin(Ang + .5), FLinearColor::White, 3.f);
		Text(FString::Printf(TEXT("WIND %.1f m/s"), Mag), C.X, C.Y + 34 * U, .9f * U, White, true);
		Text(FString::Printf(TEXT("%d:%02d"), int32(P->T / 60), int32(FMath::Fmod(P->T, 60.0))), W * .5f, 20 * U, 1.1f * U, Dim, true);
		const TCHAR* Pos = P->Flare > .5 ? TEXT("FLARE") : P->Tuck > .5 ? TEXT("TUCKED") : P->Ext > .5 ? TEXT("EXTENDED") : TEXT("NEUTRAL");
		Text(Pos, W * .5f, H - 64 * U, .8f * U, Dim, true);
	}
	if (D.Hud >= 3)
	{
		Text(TEXT("GLIDE"), 24 * U, 146 * U, .8f * U, Dim);
		Text(FString::Printf(TEXT("%.1f : 1   %d deg"), P->GlideSm, int32(FMath::RadiansToDegrees(FMath::Atan2(-P->VY, FMath::Max(P->VX, .01))))), 24 * U, 160 * U, 1.1f * U, White);
		Text(TEXT("LANDING ZONE"), W - 24 * U, 120 * U, .8f * U, Dim, false, true);
		Text(FString::Printf(TEXT("%d m"), FMath::Max(0, int32(WsWorld::LZ_X - P->PX))), W - 24 * U, 136 * U, 1.6f * U, White, false, true);
		const float Bw = FMath::Min(420.f * U, W - 60.f), Bx = W * .5f - Bw * .5f, By = H - 28 * U;
		auto F = [&](double X) { return Bx + float(FMath::Clamp(X / (WsWorld::LZ_X + 300), 0.0, 1.0)) * Bw; };
		DrawLine(Bx, By, Bx + Bw, By, FLinearColor(1, 1, 1, .3f), 2);
		DrawRect(FLinearColor(.84f, .81f, .45f), F(WsWorld::LZ_X - D.Lz / 2), By - 4 * U, FMath::Max(3.f, F(WsWorld::LZ_X + D.Lz / 2) - F(WsWorld::LZ_X - D.Lz / 2)), 8 * U);
		for (const FWsGate& G : P->Gates) DrawRect(G.Res == 1 ? FLinearColor(.49f, 1.f, .6f) : G.Res == 2 ? FLinearColor(1.f, .56f, .56f) : FLinearColor(1, 1, 1, .7f), F(G.X) - 1, By - 5 * U, 2, 10 * U);
		DrawRect(FLinearColor::White, F(P->PX) - 4 * U, By - 4 * U, 8 * U, 8 * U);
	}
	// warnings
	const TCHAR* Warn = nullptr; FLinearColor WC(1.f, .42f, .35f);
	if (P->State == EWsState::Fly)
	{
		if (P->WarnTtc >= 0)
		{
			Warn = P->WarnTtc < 1.8 ? TEXT("PULL UP") : TEXT("TERRAIN");
			if (int32(Now * (P->WarnTtc < 1.8 ? 8 : 4)) % 2 == 0) DrawRect(FLinearColor(1.f, .15f, .1f, P->WarnTtc < 1.8 ? .18f : .08f), 0, 0, W, H);
		}
		else if (P->bStalled) Warn = TEXT("STALL");
		else if (P->WTurb > .6) { Warn = TEXT("TURBULENCE"); WC = FLinearColor(1.f, .89f, .48f); }
	}
	if (Warn) Text(Warn, W * .5f, H * .27f, 2.4f * U, WC, true);
	int32 Idx = 0; for (int32 I = P->Toasts.Num() - 1; I >= 0; I--, Idx++)
	{
		const FWsToast& T = P->Toasts[I]; const float A = FMath::Clamp(2.2f - float(T.T), 0.f, 1.f);
		Text(T.Text, W * .5f, H * .38f + Idx * 28 * U - float(T.T) * 10, 1.4f * U, FLinearColor(T.Col.R / 255.f, T.Col.G / 255.f, T.Col.B / 255.f, A), true);
	}
	if (D.bHints && P->State == EWsState::Fly)
	{
		const TCHAR* Hint = nullptr;
		if (P->T < 6) Hint = TEXT("Freefall: let speed build, the suit will pitch into a glide. W / S pitch.");
		else if (P->T < 16) Hint = TEXT("Hold a steady angle. Too much pitch-up (W) stalls the wing.");
		else if (P->Agl < 250 && P->PX > WsWorld::LZ_X - 500) Hint = TEXT("Approach: build a stable glide, then FLARE (Ctrl / F) just above the ground.");
		else if (P->T < 40) Hint = TEXT("Fly through the gates and use updrafts over ridges.");
		if (Hint) Text(Hint, W * .5f, H - 100 * U, 1.f * U, FLinearColor(1, 1, 1, .85f), true);
	}
}
