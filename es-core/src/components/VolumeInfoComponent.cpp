#include "VolumeInfoComponent.h"
#include "ThemeData.h"
#include "PowerSaver.h"
#include "components/ComponentGrid.h"
#include "components/NinePatchComponent.h"
#include "components/TextComponent.h"
#include "LocaleES.h"
#include "VolumeControl.h"
#include "Window.h"

#define PADDING_PX			(Renderer::getScreenWidth()*0.006)
#define VISIBLE_TIME		3000
#define FADE_TIME			350
#define BASEOPACITY			200
#define CHECKVOLUMEDELAY	40

VolumeInfoComponent::VolumeInfoComponent(Window* window, bool actionLine)
	: GuiComponent(window)
{
	mDisplayTime = -1;
	mVolume = -1;
	mCheckTime = 0;

	auto theme = ThemeData::getMenuTheme();

	Vector2f fullSize(
		2 * PADDING_PX + theme->TextSmall.font->sizeText("100%").x(),
		2 * PADDING_PX + Renderer::getScreenHeight() * 0.20f);

	setSize(fullSize);

	mFrame = new NinePatchComponent(window);
	mFrame->setImagePath(theme->Background.path);
	mFrame->setEdgeColor(theme->Background.color);
	mFrame->setCenterColor(theme->Background.centerColor);
	mFrame->setCornerSize(theme->Background.cornerSize);
	mFrame->fitTo(mSize, Vector3f::Zero(), Vector2f(-32, -32));
	addChild(mFrame);

	mLabel = new TextComponent(mWindow, "", theme->TextSmall.font, theme->Text.color, ALIGN_CENTER);
	
	int h = theme->TextSmall.font->sizeText("100%").y() + PADDING_PX;
	mLabel->setPosition(0, fullSize.y() - h);
	mLabel->setSize(fullSize.x(), h);
	addChild(mLabel);


	// FCA TopLeft
	float posX = Renderer::getScreenWidth() * 0.02f;
	float posY = Renderer::getScreenHeight() * 0.04f;

	setPosition(posX, posY, 0);
	setOpacity(BASEOPACITY);
}

VolumeInfoComponent::~VolumeInfoComponent()
{
	delete mLabel;
	delete mFrame;
}

void VolumeInfoComponent::update(int deltaTime)
{
	GuiComponent::update(deltaTime);

	mCheckTime += deltaTime;
	if (mCheckTime < CHECKVOLUMEDELAY)
		return;

	mCheckTime = 0;

	if (mDisplayTime >= 0)
	{
		mDisplayTime += deltaTime;
		if (mDisplayTime > VISIBLE_TIME + FADE_TIME)
		{
			mDisplayTime = -1;
			setVisible(false);
		}
	}

	int volume = VolumeControl::getInstance()->getVolume();
	if (volume != mVolume)
	{
		bool firstTime = (mVolume < 0);

		mVolume = volume;

		if (mVolume == 0)
			mLabel->setText("X");
		else
			mLabel->setText(std::to_string(mVolume) + "%");

		if (!firstTime)
		{
			mDisplayTime = 0;
			setVisible(true);
		}
	}
}

void VolumeInfoComponent::render(const Transform4x4f& parentTrans)
{
	if (!mVisible || mDisplayTime < 0)
		return;

	int opacity = BASEOPACITY - Math::max(0, (mDisplayTime - VISIBLE_TIME) * BASEOPACITY / FADE_TIME);
	setOpacity(opacity);

	GuiComponent::render(parentTrans);

	Transform4x4f trans = parentTrans * getTransform();
	Renderer::setMatrix(trans);

	float x = PADDING_PX * 2;
	float y = PADDING_PX * 2;
	float w = getSize().x() - 4 * PADDING_PX;
	float h = getSize().y() - mLabel->getSize().y() - PADDING_PX - PADDING_PX;
	
	auto theme = ThemeData::getMenuTheme();

	Renderer::drawRect(x, y, w, h, (theme->Text.color & 0xFFFFFF00) | (opacity / 2));

	float px = (h*mVolume) / 100;
	Renderer::drawRect(x, y + h - px, w, px, (theme->TextSmall.color & 0xFFFFFF00) | opacity);
}
