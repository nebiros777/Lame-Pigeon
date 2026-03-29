// Source file name: LamePigeonSettings.cpp
// Author: Igor Matiushin
// Brief description: Implements default Unreal settings and interaction-pair lookup helpers for the plugin.

#include "LamePigeonSettings.h"

ULamePigeonSettings::ULamePigeonSettings()
{
    FLamePigeonInteractionRolePair Dash;
    Dash.InteractionProfileId = FName(TEXT("DashCollision"));
    Dash.RoleA                = FName(TEXT("DASHSOURCE"));
    Dash.RoleB                = FName(TEXT("DASHACCEPT"));
    Dash.bSymmetric           = false;
    InteractionRolePairs.Add(Dash);
}

bool ULamePigeonSettings::GetDefaultDasherPredictTags(FName& OutMyTag, FName& OutTheirTag)
{
    return GetPredictTagsForPairIndex(0, false, OutMyTag, OutTheirTag);
}

bool ULamePigeonSettings::GetDefaultVictimPredictTags(FName& OutMyTag, FName& OutTheirTag)
{
    return GetPredictTagsForPairIndex(0, true, OutMyTag, OutTheirTag);
}

bool ULamePigeonSettings::GetPredictTagsForPairIndex(int32 PairIndex, bool bVictimSide, FName& OutMyTag,
                                                     FName& OutTheirTag)
{
    const ULamePigeonSettings* S = GetDefault<ULamePigeonSettings>();
    if (!S || !S->InteractionRolePairs.IsValidIndex(PairIndex))
        return false;
    const FLamePigeonInteractionRolePair& P = S->InteractionRolePairs[PairIndex];
    // Row 0 (dash): always mirror for victim so we never send the same (SOURCE,ACCEPT) as the dasher.
    if (PairIndex == 0 && bVictimSide)
    {
        OutMyTag    = P.RoleB;
        OutTheirTag = P.RoleA;
        return !OutMyTag.IsNone() && !OutTheirTag.IsNone();
    }
    if (P.bSymmetric || !bVictimSide)
    {
        OutMyTag    = P.RoleA;
        OutTheirTag = P.RoleB;
    }
    else
    {
        OutMyTag    = P.RoleB;
        OutTheirTag = P.RoleA;
    }
    return !OutMyTag.IsNone() && !OutTheirTag.IsNone();
}

int32 ULamePigeonSettings::ResolveInteractionPairIndex(int32 LocalMovementContext, int32 RemoteMovementContext)
{
    // 0=Walk, 1=Dash (replicated int). Legacy/other values are treated as Walk for pair resolution.
    constexpr int32 CtxWalk = 0;
    constexpr int32 CtxDash = 1;
    const bool bLocalDash  = (LocalMovementContext == CtxDash);
    const bool bRemoteDash = (RemoteMovementContext == CtxDash);
    if (bLocalDash || bRemoteDash)
        return 0;
    return INDEX_NONE;
}
