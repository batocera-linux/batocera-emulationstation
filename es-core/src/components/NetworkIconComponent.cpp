#include "components/NetworkIconComponent.h"
#include "Settings.h"
#include "utils/Platform.h"
#include "IExternalActivity.h"

#define UPDATE_NETWORK_DELAY	2000

NetworkIconComponent::NetworkIconComponent(Window* window) : ImageComponent(window)
{	
	mNetworkCheckTime = UPDATE_NETWORK_DELAY;
	setVisible(false);
}

void NetworkIconComponent::update(int deltaTime)
{
	ImageComponent::update(deltaTime);

	mNetworkCheckTime += deltaTime;
	if (mNetworkCheckTime >= UPDATE_NETWORK_DELAY)
	{		
		bool networkConnected = Settings::ShowNetworkIndicator() && !Utils::Platform::queryIPAdress().empty();
		bool planemodeEnabled = Settings::ShowNetworkIndicator() && IExternalActivity::Instance != nullptr && IExternalActivity::Instance->isPlaneMode();

		setVisible(networkConnected || planemodeEnabled);

		if (networkConnected || planemodeEnabled)
		{
			std::string txName = mNetworkIcon;

			if (planemodeEnabled && !mPlanemodeIcon.empty())
				txName = mPlanemodeIcon;

			setImage(txName);
		}

		mNetworkCheckTime = 0;
	}
}

void NetworkIconComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	ImageComponent::applyTheme(theme, view, element, properties);

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, getThemeTypeName());
	if (!elem)
		return;

	if (elem->has("networkIcon") && ResourceManager::getInstance()->fileExists(elem->get<std::string>("networkIcon")))
		mNetworkIcon = elem->get<std::string>("networkIcon");
	else if (elem->has("path") && ResourceManager::getInstance()->fileExists(elem->get<std::string>("path")))
		mNetworkIcon = elem->get<std::string>("path");

	if (elem->has("planemodeIcon") && ResourceManager::getInstance()->fileExists(elem->get<std::string>("planemodeIcon")))
		mPlanemodeIcon = elem->get<std::string>("planemodeIcon");
}

