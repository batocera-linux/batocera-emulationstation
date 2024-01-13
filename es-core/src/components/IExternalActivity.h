#pragma once
#ifndef ES_APP_COMPONENTS_EXTERNALACTIVITY_COMPONENT_H
#define ES_APP_COMPONENTS_EXTERNALACTIVITY_COMPONENT_H

class IExternalActivity
{
public:
	static IExternalActivity* Instance;

public:
	virtual bool isPlaneMode() = 0;
	virtual bool isReadPlaneModeSupported() = 0;
};
#endif // ES_APP_COMPONENTS_EXTERNALACTIVITY_COMPONENT_H
