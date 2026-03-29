// Source file name: LamePigeonDemoHealthWidget.cpp
// Author: Igor Matiushin
// Brief description: Implements the local HUD health widget for the demo pawn.

#include "LamePigeonDemoHealthWidget.h"

void ULamePigeonDemoHealthWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// Screen / multi-line health HUD removed — use ULamePigeonDemoWorldHealthBillboardComponent over the pawn instead.
}
