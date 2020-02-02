#include "AsyncNotificationComponent.h"
#include "ThemeData.h"
#include "PowerSaver.h"
#include "components/ComponentGrid.h"
#include "components/NinePatchComponent.h"
#include "components/TextComponent.h"
#include "LocaleES.h"

#define PADDING_PX  (Renderer::getScreenWidth()*0.01)

AsyncNotificationComponent::AsyncNotificationComponent(Window* window, bool actionLine)
	: GuiComponent(window)
{
	mPercent = -1;

	auto theme = ThemeData::getMenuTheme();

	// Note : Don't localize this text -> It is only used to guess width calculation for the component.
	float width = theme->TextSmall.font->sizeText("TEXT FOR SIZE CALCULATION").x(); // Renderer::getScreenWidth() * 0.14f;											

	mTitle = std::make_shared<TextComponent>(mWindow, "", theme->TextSmall.font, theme->TextSmall.color, ALIGN_LEFT);
	mGameName = std::make_shared<TextComponent>(mWindow, "", theme->TextSmall.font, theme->Text.color, ALIGN_LEFT);

	if (actionLine)
		mAction = std::make_shared<TextComponent>(mWindow, "", theme->TextSmall.font, theme->Text.color, ALIGN_LEFT);

	Vector2f fullSize(width + 2 * PADDING_PX, 2 * PADDING_PX + mTitle->getSize().y() + mGameName->getSize().y() + (mAction == nullptr ? 0 : mAction->getSize().y()));
	Vector2f gridSize(width, mTitle->getSize().y() + mGameName->getSize().y() + (mAction == nullptr ? 0 : mAction->getSize().y()));

	setSize(fullSize);

	mFrame = new NinePatchComponent(window);
	mFrame->setImagePath(theme->Background.path);
	mFrame->setEdgeColor(theme->Background.color);
	mFrame->setCenterColor(theme->Background.centerColor);
	mFrame->setCornerSize(theme->Background.cornerSize);
	mFrame->fitTo(mSize, Vector3f::Zero(), Vector2f(-32, -32));
	addChild(mFrame);

	mGrid = new ComponentGrid(window, Vector2i(1, mAction == nullptr ? 2 : 3));
	mGrid->setPosition((fullSize.x() - gridSize.x()) / 2.0, (fullSize.y() - gridSize.y()) / 2.0);
	mGrid->setSize(gridSize);
	mGrid->setEntry(mTitle, Vector2i(0, 0), false, true);
	mGrid->setEntry(mGameName, Vector2i(0, 1), false, true);

	if (mAction != nullptr)
		mGrid->setEntry(mAction, Vector2i(0, 2), false, true);

	addChild(mGrid);

	float posX = Renderer::getScreenWidth()*0.5f - mSize.x()*0.5f;
	float posY = Renderer::getScreenHeight() * 0.02f;

	// FCA TopRight
	posX = Renderer::getScreenWidth()*0.99f - mSize.x();
	posY = Renderer::getScreenHeight() * 0.02f;

	setPosition(posX, posY, 0);
	setOpacity(200);

	PowerSaver::pause();
}

AsyncNotificationComponent::~AsyncNotificationComponent()
{
	delete mFrame;
	delete mGrid;

	PowerSaver::resume();
}

void AsyncNotificationComponent::updateText(const std::string text, const std::string action)
{
	std::unique_lock<std::mutex> lock(mMutex);

	mNextGameName = text;
	mNextAction = action;
}

void AsyncNotificationComponent::updatePercent(int percent)
{
	std::unique_lock<std::mutex> lock(mMutex);

	mPercent = percent;
}

void AsyncNotificationComponent::updateTitle(const std::string text)
{
	std::unique_lock<std::mutex> lock(mMutex);

	mNextTitle = text;
}

void AsyncNotificationComponent::render(const Transform4x4f& parentTrans)
{
	std::unique_lock<std::mutex> lock(mMutex);

	Transform4x4f trans = parentTrans * getTransform();

	if (mGameName != nullptr && mNextGameName != mGameName->getText())
		mGameName->setText(mNextGameName);

	if (mAction != nullptr && mNextAction != mAction->getText())
		mAction->setText(mNextAction);

	if (mTitle != nullptr && mNextTitle != mTitle->getText())
		mTitle->setText(mNextTitle);

	mFrame->render(trans);

	auto lastControl = mGameName;
	if (mAction != nullptr)
		lastControl = mAction;

	float x = mGrid->getPosition().x() + lastControl->getPosition().x();
	float y = mGrid->getPosition().y() + lastControl->getPosition().y();
	float w = lastControl->getSize().x();
	float h = lastControl->getSize().y();

	h /= 10.0;
	y += lastControl->getSize().y();

	Renderer::setMatrix(trans);

	if (mPercent >= 0)
	{
		float percent = mPercent / 100.0;
		if (percent < 0)
			percent = 0;
		if (percent > 100)
			percent = 100;

		auto theme = ThemeData::getMenuTheme();
		auto color = theme->Text.color & 0xFFFFFF00 | (unsigned char)((theme->Text.color & 0xFF) * (mOpacity / 255.0));
		Renderer::drawRect(x, y, (w*percent), h, color);
	}

	mGrid->render(trans);
}
