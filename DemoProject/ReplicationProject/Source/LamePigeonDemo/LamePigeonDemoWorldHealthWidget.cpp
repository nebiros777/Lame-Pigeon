// Source file name: LamePigeonDemoWorldHealthWidget.cpp
// Author: Igor Matiushin
// Brief description: Implements the replicated world-space health widget for remote demo proxies.

#include "LamePigeonDemoWorldHealthWidget.h"
#include "Components/ProgressBar.h"

void ULamePigeonDemoWorldHealthWidget::SetHealthFillRatio(float Ratio01)
{
	const float Clamped = FMath::Clamp(Ratio01, 0.f, 1.f);
	if (HealthProgressBar)
		HealthProgressBar->SetPercent(Clamped);
}
