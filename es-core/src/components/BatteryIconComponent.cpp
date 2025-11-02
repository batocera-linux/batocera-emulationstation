#include "components/BatteryIconComponent.h"
#include "Window.h"
#include "PowerSaver.h"
#include "Settings.h"
#include "watchers/BatteryLevelWatcher.h"
#include "resources/TextureResource.h"

BatteryIconComponent::BatteryIconComponent(Window* window) : ImageComponent(window), mDirty(true)
{	
    // Preload the default textures
    mTexIncharge = TextureResource::get(":/battery/incharge.svg", false, true);
    mTexFull = TextureResource::get(":/battery/full.svg", false, true);
    mTexAt75 = TextureResource::get(":/battery/75.svg", false, true);
    mTexAt50 = TextureResource::get(":/battery/50.svg", false, true);
    mTexAt25 = TextureResource::get(":/battery/25.svg", false, true);
    mTexEmpty = TextureResource::get(":/battery/empty.svg", false, true);

    mBatteryInfo = Utils::Platform::BatteryInformation();	

    WatchersManager::getInstance()->RegisterNotify(this);

    BatteryLevelWatcher* watcher = WatchersManager::GetComponent<BatteryLevelWatcher>();
    if (watcher != nullptr)
        mBatteryInfo = watcher->getBatteryInfo();
}

BatteryIconComponent::~BatteryIconComponent()
{
	WatchersManager::getInstance()->UnregisterNotify(this);
}

void BatteryIconComponent::OnWatcherChanged(IWatcher* component)
{
	BatteryLevelWatcher* watcher = dynamic_cast<BatteryLevelWatcher*>(component);
	if (watcher != nullptr)
	{
		mBatteryInfo = watcher->getBatteryInfo();
		mDirty = true;
		PowerSaver::pushRefreshEvent();
	}
}

void BatteryIconComponent::update(int deltaTime)
{
    ImageComponent::update(deltaTime);

    if (!mDirty)
        return;

    if (Settings::getInstance()->getString("ShowBattery").empty())
        mBatteryInfo.hasBattery = false;

    setVisible(mBatteryInfo.hasBattery);

    if (mBatteryInfo.hasBattery)
    {
        if (mBatteryInfo.isCharging)
            setImage(mTexIncharge);
        else if (mBatteryInfo.level > 75)
            setImage(mTexFull);
        else if (mBatteryInfo.level > 50)
            setImage(mTexAt75);
        else if (mBatteryInfo.level > 25)
            setImage(mTexAt50);
        else if (mBatteryInfo.level > 5)
            setImage(mTexAt25);
        else
            setImage(mTexEmpty);
    }

    mWindow->setNeedsRender(true);
    mDirty = false;
}

void BatteryIconComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
    ImageComponent::applyTheme(theme, view, element, properties);

    const ThemeData::ThemeElement* elem = theme->getElement(view, element, getThemeTypeName());
    if (!elem)
        return;

    if (elem->has("incharge") && ResourceManager::getInstance()->fileExists(elem->get<std::string>("incharge")))
        mTexIncharge = TextureResource::get(elem->get<std::string>("incharge"), false, true);
    if (elem->has("full") && ResourceManager::getInstance()->fileExists(elem->get<std::string>("full")))
        mTexFull = TextureResource::get(elem->get<std::string>("full"), false, true);
    // ... and so on for at75, at50, at25, empty ...
    if (elem->has("at75") && ResourceManager::getInstance()->fileExists(elem->get<std::string>("at75")))
        mTexAt75 = TextureResource::get(elem->get<std::string>("at75"), false, true);
    if (elem->has("at50") && ResourceManager::getInstance()->fileExists(elem->get<std::string>("at50")))
        mTexAt50 = TextureResource::get(elem->get<std::string>("at50"), false, true);
    if (elem->has("at25") && ResourceManager::getInstance()->fileExists(elem->get<std::string>("at25")))
        mTexAt25 = TextureResource::get(elem->get<std::string>("at25"), false, true);
    if (elem->has("empty") && ResourceManager::getInstance()->fileExists(elem->get<std::string>("empty")))
        mTexEmpty = TextureResource::get(elem->get<std::string>("empty"), false, true);
}

void BatteryIconComponent::onSizeChanged()
{
	ImageComponent::onSizeChanged();

	size_t heightPx = (size_t)Math::round(mSize.y());

	if (heightPx == 0)
		return;

	if (mTexIncharge) mTexIncharge->rasterizeAt(heightPx, heightPx);
	if (mTexFull) mTexFull->rasterizeAt(heightPx, heightPx);
	if (mTexAt75) mTexAt75->rasterizeAt(heightPx, heightPx);
	if (mTexAt50) mTexAt50->rasterizeAt(heightPx, heightPx);
	if (mTexAt25) mTexAt25->rasterizeAt(heightPx, heightPx);
	if (mTexEmpty) mTexEmpty->rasterizeAt(heightPx, heightPx);
}
