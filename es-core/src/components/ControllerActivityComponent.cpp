#include "components/ControllerActivityComponent.h"

#include "resources/TextureResource.h"
#include "ThemeData.h"
#include "InputManager.h"
#include "Settings.h"

#define PLAYER_PAD_TIME_MS 150

ControllerActivityComponent::ControllerActivityComponent(Window* window) : GuiComponent(window)
{
	init();
}

void ControllerActivityComponent::init()
{
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
	{
		mPads[i].keyState = 0;
		mPads[i].timeOut = 0;
	}
}

void ControllerActivityComponent::setColorShift(unsigned int color)
{
	mColorShift = color;
}

void ControllerActivityComponent::onSizeChanged()
{	
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
			mPads[idx].keyState = config->isMappedTo("hotkey", input) ? 2 : 1;
			mPads[idx].timeOut = PLAYER_PAD_TIME_MS;
		}
	}

	return false;
}

void ControllerActivityComponent::update(int deltaTime)
{
	GuiComponent::update(deltaTime);

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

void ControllerActivityComponent::render(const Transform4x4f& parentTrans)
{
	if (!isVisible())
		return;

	Transform4x4f trans = parentTrans * getTransform();
	if (!Renderer::isVisibleOnScreen(trans.translation().x(), trans.translation().y(), mSize.x(), mSize.y()))
		return;

	Renderer::setMatrix(trans);

	if (Settings::getInstance()->getBool("DebugImage"))
		Renderer::drawRect(0.0f, 0.0f, mSize.x(), mSize.y(), 0xFFFF0090, 0xFFFF0090);

	float padding = mSpacing;

	//float szW = (mSize.x() - (padding * (MAX_PLAYERS-1))) / MAX_PLAYERS;
	float szW = (mSize.x() - (padding * 4)) / 5;
	float szH = mSize.y();
	float x = 0;

	std::map<int, int> playerJoysticks = InputManager::getInstance()->lastKnownPlayersDeviceIndexes();

	std::vector<int> indexes;

	int padCount = 0;
	for (int player = 0; player < MAX_PLAYERS; player++)
	{
		if (playerJoysticks.count(player) != 1)
			continue;

		int idx = playerJoysticks[player];
		if (idx < 0 || idx >= MAX_PLAYERS)
			continue;
		
		indexes.push_back(idx);
	}

#if defined(WIN32) && defined(_DEBUG)
	indexes.push_back(0);
	indexes.push_back(1);
	indexes.push_back(2);
	indexes.push_back(3);
	indexes.push_back(4);
	indexes.push_back(5);
	indexes.push_back(6);
	indexes.push_back(7);

	mPads[1].keyState = 1;
	mPads[1].timeOut = PLAYER_PAD_TIME_MS;
	mPads[2].keyState = 2;
	mPads[2].timeOut = PLAYER_PAD_TIME_MS;
#endif

	if (mHorizontalAlignment == ALIGN_CENTER)
	{
		for (int i = indexes.size(); i < MAX_PLAYERS; i++)
			x += (szW + padding)/2;
	}

	if (mHorizontalAlignment == ALIGN_RIGHT)
	{
		for (int i = indexes.size() ; i < MAX_PLAYERS ; i++)
			x += szW + padding;
	}	

	for(int idx : indexes)
	{		
		unsigned int padcolor = mColorShift;
		if (mPads[idx].keyState == 1)
			padcolor = mActivityColor;
		else if (mPads[idx].keyState == 2)
			padcolor = mHotkeyColor;

		if (mPadTexture)
		{
			if (mPadTexture->bind())
			{
				const unsigned int color = Renderer::convertColor(padcolor);

				Renderer::Vertex vertices[4];

				vertices[0] = { { x, 0.0f }, { 0.0f, 1.0f }, color };
				vertices[1] = { { x, szH }, { 0.0f, 0.0f }, color };
				vertices[2] = { { x + szW,	0.0f },{ 1.0f, 1.0f }, color };
				vertices[3] = { { x + szW,	szH },{ 1.0f, 0.0f }, color };

				Renderer::drawTriangleStrips(&vertices[0], 4);
				Renderer::bindTexture(0);
			}
		}
		else
			Renderer::drawRect(x, 0.0f, szW, szH, padcolor);

		x += szW + padding;
	}

	renderChildren(trans);
}

void ControllerActivityComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{	
	init();

	GuiComponent::applyTheme(theme, view, element, properties);

	using namespace ThemeFlags;

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, "controllerActivity");
	if (elem == nullptr)
		return;

	if (properties & PATH)
	{		
		if (elem->has("imagePath") && ResourceManager::getInstance()->fileExists(elem->get<std::string>("imagePath")))
			mPadTexture = TextureResource::get(elem->get<std::string>("imagePath"));
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
}
