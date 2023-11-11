#include "components/NetworkIconComponent.h"
#include "Settings.h"
#include "utils/Platform.h"
#include "IExternalActivity.h"

#include "watchers/NetworkStateWatcher.h"

NetworkIconComponent::NetworkIconComponent(Window* window) : ImageComponent(window), mConnected(false), mPlaneMode(false), mDirty(true)
{	
	setVisible(false);

	WatchersManager::getInstance()->RegisterNotify(this);

	NetworkStateWatcher* watcher = WatchersManager::GetComponent<NetworkStateWatcher>();
	if (watcher != nullptr)
	{
		mConnected = watcher->isConnected();
		mPlaneMode = watcher->isPlaneMode();
	}
}

NetworkIconComponent::~NetworkIconComponent()
{
	WatchersManager::getInstance()->UnregisterNotify(this);
}

void NetworkIconComponent::OnWatcherChanged(IWatcher* component)
{
	NetworkStateWatcher* watcher = dynamic_cast<NetworkStateWatcher*>(component);
	if (watcher != nullptr)
	{
		mConnected = watcher->isConnected();
		mPlaneMode = watcher->isPlaneMode();
		mDirty = true;
	}	
}

void NetworkIconComponent::update(int deltaTime)
{
	ImageComponent::update(deltaTime);

	if (mDirty)
	{		
		bool networkConnected = Settings::ShowNetworkIndicator() && mConnected;
		bool planemodeEnabled = Settings::ShowNetworkIndicator() && mPlaneMode;

		setVisible(networkConnected || planemodeEnabled);

		if (networkConnected || planemodeEnabled)
		{
			std::string txName = mNetworkIcon;

			if (planemodeEnabled && !mPlanemodeIcon.empty())
				txName = mPlanemodeIcon;

			setImage(txName);
		}

		mDirty = false;
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

