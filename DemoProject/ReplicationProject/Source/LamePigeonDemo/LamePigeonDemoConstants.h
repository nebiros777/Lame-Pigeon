// Source file name: LamePigeonDemoConstants.h
// Author: Igor Matiushin
// Brief description: Defines demo gameplay constants for movement, prediction timing, and replication variable names.

#pragma once

#include "CoreMinimal.h"

namespace LamePigeonDemoConstants
{
inline const TCHAR* HealthVariableName       = TEXT("Health");
inline const TCHAR* ReplicateJumpRpcName     = TEXT("ReplicateJump");
inline const TCHAR* ApplyKnockbackRpcName    = TEXT("ApplyKnockback");
inline const TCHAR* InteractionContextVariableName = TEXT("LamePigeonInteractionCtx");

inline constexpr int32 InteractionContextWalk = 0;
inline constexpr int32 InteractionContextDash = 1;

inline constexpr float InitialHealthPoints           = 99.f;
inline constexpr float KnockbackDamagePoints         = 2.f;
inline constexpr float DashDistanceCentimeters       = 600.f;
inline constexpr float DashSpeedCentimetersPerSecond = 2400.f;
inline constexpr float JumpBumpCentimeters         = 120.f;
inline constexpr float KnockbackHorizontalSpeed =
	(2.f * DashSpeedCentimetersPerSecond) / 1.5f;
inline constexpr float KnockbackUpwardComponent = 220.f / 1.5f;

inline constexpr float KnockbackPredictWireDeltaClampSeconds = 0.35f;

inline constexpr float InteractionPredictScanHz = 30.f;
inline constexpr float ContactPredictionScanRadiusCm = 2200.f;
inline constexpr float InteractionPredictMinResendIntervalSeconds = 0.12f;
inline constexpr float InteractionPredictWireSlackSeconds = 0.08f;
inline constexpr float KnockbackPredictResolveAfterAppliedSeconds = 0.55f;
inline constexpr float VictimKnockbackPerDasherCooldownSeconds = 1.2f;

inline constexpr float KnockbackPredictConfirmTimeoutSeconds = 9999.f;

inline constexpr float DashContextStickyHoldAfterEndSeconds = 0.28f;

inline constexpr float KnockbackNormalMinDotAway = 0.45f;
}
