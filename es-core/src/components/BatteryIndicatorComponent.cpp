#include "components/BatteryIndicatorComponent.h"

#include "resources/TextureResource.h"
#include "ThemeData.h"
#include "InputManager.h"
#include "Settings.h"
#include <SDL_power.h>
#include "utils/StringUtil.h"

#ifdef WIN32
#include <Windows.h>
#endif

#define UPDATEBATTERYDELAY	5000

BatteryIndicatorComponent::BatteryIndicatorComponent(Window* window) : GuiComponent(window), mImage(window, true)
{
	mHasBattery = false;
	mBatteryLevel = 0;
	mIsCharging = false;

	addChild(&mImage);
	init();
}

void BatteryIndicatorComponent::init()
{
	mImage.setColorShift(0xFFFFFFA0);
	mImage.setOrigin(0.5, 0.5);
	mBatteryLevel = -1;	
	mCheckTime = UPDATEBATTERYDELAY;
	
	if (Renderer::isSmallScreen())
	{		
		setPosition(Renderer::getScreenWidth() * 0.915, Renderer::getScreenHeight() * 0.013);
		setSize(Renderer::getScreenWidth() * 0.07, Renderer::getScreenHeight() * 0.07);
	}
	else
	{
		setPosition(Renderer::getScreenWidth() * 0.955, Renderer::getScreenHeight() *0.0125);
		setSize(Renderer::getScreenWidth() * 0.033, Renderer::getScreenHeight() *0.033);
	}
	
	if (ResourceManager::getInstance()->fileExists(":/battery/incharge.svg"))
		mIncharge = ResourceManager::getInstance()->getResourcePath(":/battery/incharge.svg");

	if (ResourceManager::getInstance()->fileExists(":/battery/full.svg"))
		mFull = ResourceManager::getInstance()->getResourcePath(":/battery/full.svg");

	if (ResourceManager::getInstance()->fileExists(":/battery/75.svg"))
		mAt75 = ResourceManager::getInstance()->getResourcePath(":/battery/75.svg");

	if (ResourceManager::getInstance()->fileExists(":/battery/50.svg"))
		mAt50 = ResourceManager::getInstance()->getResourcePath(":/battery/50.svg");

	if (ResourceManager::getInstance()->fileExists(":/battery/25.svg"))
		mAt25 = ResourceManager::getInstance()->getResourcePath(":/battery/25.svg");

	if (ResourceManager::getInstance()->fileExists(":/battery/empty.svg"))
		mEmpty = ResourceManager::getInstance()->getResourcePath(":/battery/empty.svg");
	

	updateBatteryInfo();
}

void BatteryIndicatorComponent::setColorShift(unsigned int color)
{
	mImage.setColorShift(color);
}

void BatteryIndicatorComponent::render(const Transform4x4f& parentTrans)
{
	if (!isVisible())
		return;

	Transform4x4f trans = parentTrans * getTransform();
	if (!Renderer::isVisibleOnScreen(trans.translation().x(), trans.translation().y(), mSize.x(), mSize.y()))
		return;

	if (!Settings::getInstance()->getBool("ShowBatteryIndicator"))
		return;

	renderChildren(trans);
}

void BatteryIndicatorComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{	
	init();

	GuiComponent::applyTheme(theme, view, element, properties);

	using namespace ThemeFlags;

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, "batteryIndicator");
	if (elem == nullptr)
		return;

	if (properties & PATH)
	{		
		if (elem->has("incharge") && ResourceManager::getInstance()->fileExists(elem->get<std::string>("incharge")))
			mIncharge = elem->get<std::string>("incharge");
		
		if (elem->has("full") && ResourceManager::getInstance()->fileExists(elem->get<std::string>("full")))
			mFull = elem->get<std::string>("full");

		if (elem->has("at75") && ResourceManager::getInstance()->fileExists(elem->get<std::string>("at75")))
			mAt75 = elem->get<std::string>("at75");

		if (elem->has("at50") && ResourceManager::getInstance()->fileExists(elem->get<std::string>("at50")))
			mAt50 = elem->get<std::string>("at50");

		if (elem->has("at25") && ResourceManager::getInstance()->fileExists(elem->get<std::string>("at25")))
			mAt25 = elem->get<std::string>("at25");

		if (elem->has("empty") && ResourceManager::getInstance()->fileExists(elem->get<std::string>("empty")))
			mEmpty = elem->get<std::string>("empty");
	}

	if (properties & COLOR && elem->has("color"))			
		setColorShift(elem->get<unsigned int>("color"));	
	
	onSizeChanged();
}

void BatteryIndicatorComponent::update(int deltaTime)
{
	GuiComponent::update(deltaTime);

	if (!Settings::getInstance()->getBool("ShowBatteryIndicator"))
		return;

	mCheckTime += deltaTime;
	if (mCheckTime < UPDATEBATTERYDELAY)
		return;

	mCheckTime = 0;

	updateBatteryInfo();

	if (mHasBattery)
	{
		std::string txName = mIncharge;

		if (mIsCharging && !mIncharge.empty())
			txName = mIncharge;
		else if (mBatteryLevel > 75 && !mFull.empty())
			txName = mFull;
		else if (mBatteryLevel > 50 && !mAt75.empty())
			txName = mAt75;
		else if (mBatteryLevel > 25 && !mAt50.empty())
			txName = mAt50;
		else if (mBatteryLevel > 5 && !mAt25.empty())
			txName = mAt25;
		else
			txName = mEmpty;

		if (mTexturePath != txName && !txName.empty())
		{
			mTexturePath = txName;			
			mImage.setImage(txName);			
		}

		setVisible(!mTexturePath.empty());
	}
	else
		setVisible(false);
}

void BatteryIndicatorComponent::onSizeChanged()
{	
	mImage.setPosition(mSize.x() / 2.0f, mSize.y() / 2.0f);
	mImage.setMaxSize(mSize);
}

void BatteryIndicatorComponent::updateBatteryInfo()
{
#ifdef WIN32

	#ifdef _DEBUG
		mHasBattery = true;
		mIsCharging = false;
		mBatteryLevel = 33;
		return;
	#endif

	SYSTEM_POWER_STATUS systemPowerStatus;
	if (GetSystemPowerStatus(&systemPowerStatus))
	{
		if ((systemPowerStatus.BatteryFlag & 128) == 128)
			mHasBattery = false;
		else
		{
			mHasBattery = true;
			mIsCharging = (systemPowerStatus.BatteryFlag & 8) == 8;
			mBatteryLevel = systemPowerStatus.BatteryLifePercent;
		}
	}
	else
		mHasBattery = false;

	return;
#endif

	static std::string batteryStatusPath;
	static std::string batteryCapacityPath;

	// Find battery path - only at the first call
	if (batteryStatusPath.empty())
	{		
		std::string batteryRootPath;

		auto files = Utils::FileSystem::getDirContent("/sys/class/power_supply");
		for (auto file : files)
		{
			if (Utils::String::toLower(file).find("/bat") != std::string::npos)
			{
				batteryRootPath = file;
				break;
			}
		}

		if (batteryRootPath.empty())
			batteryStatusPath = ".";
		else
		{
			batteryStatusPath = batteryRootPath + "/status";
			batteryCapacityPath = batteryRootPath + "/capacity";
		}
	}
	
	if (batteryStatusPath.length() <= 1)
	{
		mHasBattery = false;
		mIsCharging = false;
	}
	else 
	{	
		mHasBattery = true;
		mIsCharging = (Utils::FileSystem::readAllText(batteryStatusPath) != "Discharging");
		mBatteryLevel = atoi(Utils::FileSystem::readAllText(batteryCapacityPath).c_str());
	}
}
