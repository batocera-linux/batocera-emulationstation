#include "components/ControllerActivityComponent.h"

#include "resources/TextureResource.h"
#include "utils/StringUtil.h"
#include "ThemeData.h"
#include "InputManager.h"
#include "Settings.h"
#include "platform.h"

// #define DEVTEST

#define PLAYER_PAD_TIME_MS		 150
#define UPDATE_NETWORK_DELAY	2000
#define UPDATE_BATTERY_DELAY	5000

ControllerActivityComponent::ControllerActivityComponent(Window* window) : GuiComponent(window)
{
	init();
}

void ControllerActivityComponent::init()
{
	mBatteryFont = nullptr;
	mBatteryText = nullptr;
	mBatteryTextX = -999;

	mView = CONTROLLERS;
	
	mBatteryInfo = BatteryInformation();
	mBatteryCheckTime = UPDATE_BATTERY_DELAY;

	mNetworkCheckTime = UPDATE_NETWORK_DELAY;

	mColorShift = 0xFFFFFF99;
	mActivityColor = 0xFF000066;
	mHotkeyColor = 0x0000FF66;

	mPadTexture = nullptr;
	mHorizontalAlignment = ALIGN_LEFT;
	mSpacing = (int)(Renderer::getScreenHeight() / 200.0f);

	float itemSize = Renderer::getScreenHeight() / 100.0f;
	mSize = Vector2f(itemSize * MAX_PLAYERS + mSpacing * (MAX_PLAYERS - 1), itemSize);

	float margin = (int)(Renderer::getScreenHeight() / 280.0f);
	mPosition = Vector3f(margin, Renderer::getScreenHeight() - mSize.y() - margin, 0.0f);

	for (int i = 0; i < MAX_PLAYERS; i++)
		mPads[i].reset();

	updateNetworkInfo();
	updateBatteryInfo();
}

void ControllerActivityComponent::setColorShift(unsigned int color)
{
	mColorShift = color;
}

void ControllerActivityComponent::onPositionChanged()
{
	GuiComponent::onPositionChanged();

	mBatteryText = nullptr;
	mBatteryFont = nullptr;

	for (int idx = 0; idx < MAX_PLAYERS; idx++)
		mPads[idx].batteryText = nullptr;
}

void ControllerActivityComponent::onSizeChanged()
{	
	GuiComponent::onSizeChanged();

	mBatteryText = nullptr;
	mBatteryFont = nullptr;

	for (int idx = 0; idx < MAX_PLAYERS; idx++)
		mPads[idx].batteryText = nullptr;

	if (mSize.y() > 0 && mPadTexture)
	{
		size_t heightPx = (size_t)Math::round(mSize.y());
		mPadTexture->rasterizeAt(heightPx, heightPx);
	}	
}

bool ControllerActivityComponent::input(InputConfig* config, Input input)
{
	if (config->getDeviceIndex() != -1 && (input.type == TYPE_BUTTON || input.type == TYPE_HAT))
	{
		int idx = config->getDeviceIndex();
		if (idx >= 0 && idx < MAX_PLAYERS)
		{
			for (int i = 0; i < MAX_PLAYERS; i++)
			{
				if (mPads[i].index == idx)
				{
					mPads[i].keyState = config->isMappedTo("hotkey", input) ? 2 : 1;
					mPads[i].timeOut = PLAYER_PAD_TIME_MS;
					break;
				}
			}
		}
	}

	return false;
}

void ControllerActivityComponent::update(int deltaTime)
{
	GuiComponent::update(deltaTime);

	if (mView & BATTERY)
	{
		mBatteryCheckTime += deltaTime;
		if (mBatteryCheckTime >= UPDATE_BATTERY_DELAY)
		{
			updateBatteryInfo();
			mBatteryCheckTime = 0;
		}
	}
	
	if (mView & NETWORK)
	{
		mNetworkCheckTime += deltaTime;
		if (mNetworkCheckTime >= UPDATE_NETWORK_DELAY)
		{
			updateNetworkInfo();
			mNetworkCheckTime = 0;			
		}
	}

	if (mView & CONTROLLERS)
	{
		for (int i = 0; i < MAX_PLAYERS; i++)
		{
			PlayerPad& pad = mPads[i];
			if (pad.timeOut == 0)
				continue;

			pad.timeOut -= deltaTime;
			if (pad.timeOut <= 0)
			{
				pad.timeOut = 0;
				pad.keyState = 0;
			}
		}
	}
}

void ControllerActivityComponent::render(const Transform4x4f& parentTrans)
{
	if (!isVisible())
		return;

	Transform4x4f trans = parentTrans * getTransform();
	if (!Renderer::isVisibleOnScreen(trans.translation().x(), trans.translation().y(), mSize.x(), mSize.y()))
		return;

	Renderer::setMatrix(trans);

	if (Settings::DebugImage())
		Renderer::drawRect(0.0f, 0.0f, mSize.x(), mSize.y(), 0xFFFF0090, 0xFFFF0090);

	float x = 0;
	float szW = mSize.y();
	float szH = mSize.y();

	int itemsWidth = 0;
	float batteryTextOffset = 0;

	bool showControllerActivity = Settings::ShowControllerActivity();
	bool showControllerBattery = showControllerActivity && Settings::ShowControllerBattery();

	if ((mView & CONTROLLERS) && showControllerActivity)
	{	
		auto& playerJoysticks = InputManager::getInstance()->lastKnownPlayersDeviceIndexes();

		int padCount = 0;

		for (int player = 0; player < MAX_PLAYERS; player++)
		{
			PlayerPad& pad = mPads[player];
			pad.index = -1;

			auto it = playerJoysticks.find(player);
			if (it == playerJoysticks.cend() || it->second.index < 0 || it->second.index >= MAX_PLAYERS)
				continue;

			pad.index = it->second.index;
			pad.batteryLevel = it->second.batteryLevel;
		}

		for (int idx = 0; idx < MAX_PLAYERS; idx++)
		{
			PlayerPad& pad = mPads[idx];
			if (pad.index < 0)
				continue;

			itemsWidth += szW + mSpacing;

			if (showControllerBattery && pad.batteryLevel >= 0)
			{
				if (mBatteryFont == nullptr)
					mBatteryFont = Font::get(szH * (Renderer::isSmallScreen() ? 0.55f : 0.70f), FONT_PATH_REGULAR);

				std::string batteryTextValue = std::to_string(pad.batteryLevel) + "% ";

				auto sz = mBatteryFont->sizeText(batteryTextValue, 1.0);

				if (pad.batteryTextValue != batteryTextValue)
					pad.batteryText = nullptr;

				pad.batteryTextValue = batteryTextValue;
				pad.batteryTextSize = sz.x();

				itemsWidth += sz.x() + mSpacing;
				batteryTextOffset = mSize.y() / 2.0f - sz.y() / 2.0f;
			}
		}
	}

	if ((mView & NETWORK) && mNetworkConnected && mNetworkImage != nullptr)
		itemsWidth += szW + mSpacing; // getTextureSize(mNetworkImage).x()

	auto batteryText = std::to_string(mBatteryInfo.level) + "%";
	
	if ((mView & BATTERY) && mBatteryInfo.hasBattery && mBatteryImage != nullptr)
	{
		itemsWidth += szW + mSpacing;
		//itemsWidth += getTextureSize(mBatteryImage).x() + mSpacing;

		if (Settings::getInstance()->getString("ShowBattery") == "text")
		{
			if (mBatteryFont == nullptr)
				mBatteryFont = Font::get(szH * (Renderer::isSmallScreen() ? 0.55f : 0.70f), FONT_PATH_REGULAR);

			auto sz = mBatteryFont->sizeText(batteryText, 1.0);
			itemsWidth += sz.x() + mSpacing;
			batteryTextOffset = mSize.y() / 2.0f - sz.y() / 2.0f;
		}
	}

	if (mHorizontalAlignment == ALIGN_CENTER)
		x = mSize.x() / 2.0f - itemsWidth / 2.0f;	
	else if (mHorizontalAlignment == ALIGN_RIGHT)
		x = mSize.x() - itemsWidth;

	if ((mView & CONTROLLERS) && showControllerActivity)
	{
		for (int idx = 0; idx < MAX_PLAYERS; idx++)
		{
			auto pad = mPads[idx];
			if (pad.index < 0)
				continue;

			unsigned int padcolor = mColorShift;
			if (pad.keyState == 1)
				padcolor = mActivityColor;
			else if (pad.keyState == 2)
				padcolor = mHotkeyColor;

			if (mPadTexture && mPadTexture->bind())
				x += renderTexture(x, szW, mPadTexture, padcolor);
			else
			{
				Renderer::drawRect(x, 0.0f, szW, szH, padcolor);
				x += szW + mSpacing;
			}

			if (showControllerBattery && pad.batteryLevel >= 0 && mBatteryFont != nullptr)
			{
				if (mPads[idx].batteryText == nullptr)
					mPads[idx].batteryText = std::unique_ptr<TextCache>(mBatteryFont->buildTextCache(pad.batteryTextValue, Vector2f(x, batteryTextOffset), mColorShift, mSize.x(), Alignment::ALIGN_LEFT, 1.0f));

				mPads[idx].batteryText->setColor(padcolor);
				mBatteryFont->renderTextCache(mPads[idx].batteryText.get());
				x += pad.batteryTextSize + mSpacing;
			}
			else
				mPads[idx].batteryText = nullptr;
		}
	}
	
	if ((mView & NETWORK) && mNetworkConnected && mNetworkImage != nullptr)
		x += renderTexture(x, szW, mNetworkImage, mColorShift);

	if ((mView & BATTERY) && mBatteryInfo.hasBattery && mBatteryImage != nullptr)
	{
		x += renderTexture(x, szW, mBatteryImage, mColorShift);

		if (mBatteryFont != nullptr && Settings::getInstance()->getString("ShowBattery") == "text")
		{
			if (mBatteryText == nullptr || mBatteryTextX != x)
			{
				mBatteryTextX = x;
				mBatteryText = std::unique_ptr<TextCache>(mBatteryFont->buildTextCache(batteryText, Vector2f(x, batteryTextOffset), mColorShift, mSize.x(), Alignment::ALIGN_LEFT, 1.0f));
			}

			mBatteryFont->renderTextCache(mBatteryText.get());
		}
	}

	renderChildren(trans);
}

void ControllerActivityComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{	
	init();

	GuiComponent::applyTheme(theme, view, element, properties);

	using namespace ThemeFlags;

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, element);
	if (elem == nullptr)
		return;

	if (properties & PATH)
	{		
		// Controllers
		if (elem->has("imagePath") && ResourceManager::getInstance()->fileExists(elem->get<std::string>("imagePath")))
			mPadTexture = TextureResource::get(elem->get<std::string>("imagePath"), false, true);

		// Wifi
		if (elem->has("networkIcon"))
		{
			if (ResourceManager::getInstance()->fileExists(elem->get<std::string>("networkIcon")))
			{
				mView |= ActivityView::NETWORK;
				mNetworkImage = TextureResource::get(elem->get<std::string>("networkIcon"), false, true);
			}
			else
				mNetworkImage = nullptr;
		}

		// Battery
		if (elem->has("incharge") && ResourceManager::getInstance()->fileExists(elem->get<std::string>("incharge")))
		{
			mView |= ActivityView::BATTERY;
			mIncharge = elem->get<std::string>("incharge");
		}

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

	if (properties & COLOR)
	{
		if (elem->has("color"))
			setColorShift(elem->get<unsigned int>("color"));

		if (elem->has("activityColor"))
			setActivityColor(elem->get<unsigned int>("activityColor"));

		if (elem->has("hotkeyColor"))
			setHotkeyColor(elem->get<unsigned int>("hotkeyColor"));

		if (elem->has("itemSpacing"))
			setSpacing(elem->get<float>("itemSpacing") * Renderer::getScreenWidth());
	}

	if (properties & ALIGNMENT)
	{
		if (elem->has("horizontalAlignment"))
		{
			std::string str = elem->get<std::string>("horizontalAlignment");
			if (str == "left")
				setHorizontalAlignment(ALIGN_LEFT);
			else if (str == "right")
				setHorizontalAlignment(ALIGN_RIGHT);
			else
				setHorizontalAlignment(ALIGN_CENTER);
		}
	}

	onSizeChanged();

	// Force update battery images
	mBatteryInfo.level = -2;
	updateBatteryInfo();
}

void ControllerActivityComponent::updateNetworkInfo()
{
	mNetworkConnected = Settings::ShowNetworkIndicator() && !queryIPAdress().empty();
}

void ControllerActivityComponent::updateBatteryInfo()
{
	if (Settings::getInstance()->getString("ShowBattery").empty() || (mView & BATTERY) == 0)
	{
		mBatteryInfo.hasBattery = false;
		return;
	}

	BatteryInformation info = queryBatteryInformation();

	if (info.hasBattery == mBatteryInfo.hasBattery && info.isCharging == mBatteryInfo.isCharging && info.level == mBatteryInfo.level)
		return;

	if (mBatteryInfo.level != info.level)
	{
		mBatteryFont = nullptr;
		mBatteryText = nullptr;
	}

	if (mBatteryInfo.hasBattery != info.hasBattery || mBatteryInfo.isCharging != info.isCharging)
	{
		mBatteryImage = nullptr;
		mCurrentBatteryTexture = "";
	}

	mBatteryInfo = info;

	if (mBatteryInfo.hasBattery)
	{
		std::string txName = mIncharge;

		if (mBatteryInfo.isCharging && !mIncharge.empty())
			txName = mIncharge;
		else if (mBatteryInfo.level > 75 && !mFull.empty())
			txName = mFull;
		else if (mBatteryInfo.level > 50 && !mAt75.empty())
			txName = mAt75;
		else if (mBatteryInfo.level > 25 && !mAt50.empty())
			txName = mAt50;
		else if (mBatteryInfo.level > 5 && !mAt25.empty())
			txName = mAt25;
		else
			txName = mEmpty;

		if (mCurrentBatteryTexture != txName)
		{
			mCurrentBatteryTexture = txName;

			if (mCurrentBatteryTexture.empty())
				mBatteryImage = nullptr;
			else
				mBatteryImage = TextureResource::get(mCurrentBatteryTexture, false, true);
		}
	}
}

Vector2f ControllerActivityComponent::getTextureSize(std::shared_ptr<TextureResource> texture)
{
	if (texture == nullptr)
		return Vector2f::Zero();

	auto imageSize = texture->getSourceImageSize();
	if (imageSize.x() == 0 || imageSize.y() == 0)
		return Vector2f::Zero();

	auto mTargetSize = mSize;
	auto textureSize = imageSize;

	Vector2f resizeScale((mTargetSize.x() / imageSize.x()), (mTargetSize.y() / imageSize.y()));
	if (resizeScale.x() < resizeScale.y())
	{
		imageSize[0] *= resizeScale.x();
		imageSize[1] = Math::min(Math::round(imageSize[1] *= resizeScale.x()), mTargetSize.y());
	}
	else
	{
		imageSize[1] = Math::round(imageSize[1] * resizeScale.y());
		imageSize[0] = Math::min((imageSize[1] / textureSize.y()) * textureSize.x(), mTargetSize.x());
	}

	return imageSize;
}

int ControllerActivityComponent::renderTexture(float x, float w, std::shared_ptr<TextureResource> texture, unsigned int color)
{
	if (!texture->bind())
		return 0;

	auto sz = getTextureSize(texture);
	if (sz.x() == 0 || sz.y() == 0)
		return 0;

	const unsigned int clr = Renderer::convertColor(color);

	float top = mSize.y() / 2.0f - sz.y() / 2.0f;
	float left = x + w / 2.0f - sz.x() / 2.0f;

	Renderer::Vertex vertices[4];

	vertices[0] = { { left, top },{ 0.0f, 1.0f }, clr };
	vertices[1] = { { left, sz.y() },{ 0.0f, 0.0f }, clr };
	vertices[2] = { { left + sz.x(), top },{ 1.0f, 1.0f }, clr };
	vertices[3] = { { left + sz.x(), sz.y() },{ 1.0f, 0.0f }, clr };

	Renderer::drawTriangleStrips(&vertices[0], 4);
	Renderer::bindTexture(0);

	return w + mSpacing;
}