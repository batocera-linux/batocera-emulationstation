#include "Splash.h"
#include "ThemeData.h"
#include "Window.h"
#include "resources/TextureResource.h"

#include <algorithm>
#include <SDL_events.h>

Splash::Splash(Window* window, const std::string image, bool fullScreenBackGround) :
	mBackground(window),
	mText(window),
	mInactiveProgressbar(window),
	mActiveProgressbar(window)	
{
	mBackgroundColor = 0x000000FF;
	mRoundCorners = 0.01;
	mPercent = -1;

	bool useOldSplashLayout = OLD_SPLASH_LAYOUT;

	// Theme
	auto theme = std::make_shared<ThemeData>();

	std::string themeFilePath = fullScreenBackGround ? ":/splash.xml" : ":/gamesplash.xml";
	themeFilePath = ResourceManager::getInstance()->getResourcePath(themeFilePath);

	std::map<std::string, ThemeSet> themeSets = ThemeData::getThemeSets();
	auto themeset = themeSets.find(Settings::getInstance()->getString("ThemeSet"));
	if (themeset != themeSets.cend())
	{
		std::string path = themeset->second.path + (fullScreenBackGround ? "/splash.xml" : "/gamesplash.xml");
		if (Utils::FileSystem::exists(path))
			themeFilePath = path;
	}

	if (Utils::FileSystem::exists(themeFilePath))
	{
		std::map<std::string, std::string> sysData;
		theme->loadFile("splash", sysData, themeFilePath);
		useOldSplashLayout = false;
	}

	// Background image
	std::string imagePath = image;

	auto backGroundImageTheme = theme->getElement("splash", "background", "image");
	if (backGroundImageTheme && backGroundImageTheme->has("path") && Utils::FileSystem::exists(backGroundImageTheme->get<std::string>("path")))
		imagePath = backGroundImageTheme->get<std::string>("path");

	bool linearSmooth = false;
	if (backGroundImageTheme && backGroundImageTheme->has("linearSmooth"))
		linearSmooth = backGroundImageTheme->get<bool>("linearSmooth");
	
	if (fullScreenBackGround && !useOldSplashLayout)
	{
		mBackground.setOrigin(0.5, 0.5);
		mBackground.setPosition(Renderer::getScreenWidth() / 2, Renderer::getScreenHeight() / 2);

		if (backGroundImageTheme)
			mBackground.setMaxSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());
		else
		{
			mBackground.setMinSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());
			linearSmooth = true;
		}
	}
	else
	{
		mBackground.setOrigin(0.5, 0.5);
		mBackground.setPosition(Renderer::getScreenWidth() * 0.5f, Renderer::getScreenHeight()  * 0.42f);

		if (useOldSplashLayout)
		{
			mBackground.setMaxSize(Renderer::getScreenWidth() * 0.80f, Renderer::getScreenHeight() * 0.60f);
			mBackground.setRoundCorners(0.02);
		}
		else
		{
			mBackground.setMaxSize(Renderer::getScreenWidth() * 0.71f, Renderer::getScreenHeight() * 0.55f);
			mBackground.setRoundCorners(0.02);
		}
	}

	// Label
	auto font = Font::get(FONT_SIZE_MEDIUM);

	if (useOldSplashLayout)
		mText.setColor(0xFFFFFFD0);
	else
		mText.setColor(0xFFFFFFFF);
#ifdef _ENABLEEMUELEC
		mText.setColor(0x51468700);
#endif
	mText.setHorizontalAlignment(ALIGN_CENTER);
	mText.setFont(font);

	if (fullScreenBackGround)
		mText.setPosition(0, Renderer::getScreenHeight() * 0.78f);
	else
		mText.setPosition(0, Renderer::getScreenHeight() * 0.835f);

	mText.setSize(Renderer::getScreenWidth(), font->getLetterHeight());

	Vector2f mProgressPosition;
	Vector2f mProgressSize;

	if (backGroundImageTheme)
		mBackground.applyTheme(theme, "splash", "background", ThemeFlags::ALL ^ (ThemeFlags::PATH));

	auto maxSize = mBackground.getMaxSizeInfo();
	mTexture = TextureResource::get(imagePath, false, linearSmooth, true, false, false, &maxSize);

	if (!fullScreenBackGround)
		ResourceManager::getInstance()->removeReloadable(mTexture);

	mBackground.setImage(mTexture);

	if (theme->getElement("splash", "label", "text"))
		mText.applyTheme(theme, "splash", "label", ThemeFlags::ALL ^ (ThemeFlags::TEXT));
	else if (fullScreenBackGround)
	{
		
#ifdef _ENABLEEMUELEC
		mText.setGlowColor(0x00000010);
		mText.setGlowSize(1);	
#else	
		mText.setGlowColor(0x00000020);	
		mText.setGlowSize(2);
#endif
		mText.setGlowOffset(1, 1);
	}

	// Splash background
	auto elem = theme->getElement("splash", "splash", "splash");
	if (elem && elem->has("backgroundColor"))
		mBackgroundColor = elem->get<unsigned int>("backgroundColor");

	// Progressbar
	float baseHeight = 0.036f;

	float w = Renderer::getScreenWidth() / 2.0f;
	float h = Renderer::getScreenHeight() * baseHeight;
	float x = Renderer::getScreenWidth() / 2.0f - w / 2.0f;
	float y = Renderer::getScreenHeight() - (Renderer::getScreenHeight() * 3 * baseHeight);

	auto blankTexture = TextureResource::get("", false, true, true, false, false);

	mInactiveProgressbar.setImage(blankTexture);
	mActiveProgressbar.setImage(blankTexture);

	mInactiveProgressbar.setPosition(x, y);
	mInactiveProgressbar.setSize(w, h);
	mInactiveProgressbar.setResize(w, h);
	mActiveProgressbar.setPosition(x, y);
	mActiveProgressbar.setSize(w, h);
	mActiveProgressbar.setResize(w, h);

	mInactiveProgressbar.setColorShift(0x6060607F);
	mInactiveProgressbar.setColorShiftEnd(0x9090907F);
	mInactiveProgressbar.setColorGradientHorizontal(true);

	elem = theme->getElement("splash", "progressbar", "image");
	if (elem)
	{
		mInactiveProgressbar.applyTheme(theme, "splash", "progressbar", ThemeFlags::ALL);
		mActiveProgressbar.applyTheme(theme, "splash", "progressbar", ThemeFlags::ALL);

		mRoundCorners = mInactiveProgressbar.getRoundCorners();
	}

	if (useOldSplashLayout)
	{
		mActiveProgressbar.setColorShift(0x006C9EFF);
		mActiveProgressbar.setColorShiftEnd(0x003E5CFF);
	}
	else
	{
		mActiveProgressbar.setColorShift(0xDF1010FF);
		mActiveProgressbar.setColorShiftEnd(0x4F0000FF);
#ifdef _ENABLEEMUELEC
		mActiveProgressbar.setColorShift(0xA8A2D0FF);
		mActiveProgressbar.setColorShiftEnd(0x514687FF);
#endif
	}

	mActiveProgressbar.setColorGradientHorizontal(true);

	elem = theme->getElement("splash", "progressbar:active", "image");
	if (elem)
		mActiveProgressbar.applyTheme(theme, "splash", "progressbar:active", ThemeFlags::ALL);

	mInactiveProgressbar.setRoundCorners(0);
	mActiveProgressbar.setRoundCorners(0);

	mExtras = ThemeData::makeExtras(theme, "splash", window, true);

	if (!fullScreenBackGround)
	{
		for (auto im : mExtras)
			if (im->isKindOf<ImageComponent>())
				ResourceManager::getInstance()->removeReloadable(((ImageComponent*)im)->getTexture());
	}

	std::stable_sort(mExtras.begin(), mExtras.end(), [](GuiComponent* a, GuiComponent* b) { return b->getZIndex() > a->getZIndex(); });

	// Don't waste time in waiting for vsync
	SDL_GL_SetSwapInterval(0);
}

Splash::~Splash()
{
	Renderer::setSwapInterval();

	for (auto extra : mExtras)
		delete extra;

	mExtras.clear();
}

void Splash::update(std::string text, float percent)
{
	mText.setText(text);
	mPercent = percent;
}

void Splash::render(float opacity, bool swapBuffers)
{
	if (opacity == 0)
		return;

	unsigned char alpha = (unsigned char)(opacity * 255);

	mText.setOpacity(alpha);

	Transform4x4f trans = Transform4x4f::Identity();
	Renderer::setMatrix(trans);
	Renderer::drawRect(0, 0, Renderer::getScreenWidth(), Renderer::getScreenHeight(), (mBackgroundColor & 0xFFFFFF00) | alpha);

	for (auto extra : mExtras)
		extra->setOpacity(alpha);

	for (auto extra : mExtras)
		if (extra->getZIndex() < 5)
			extra->render(trans);

	mBackground.setOpacity(alpha);
	mBackground.render(trans);

	for (auto extra : mExtras)
		if (extra->getZIndex() >= 5 && extra->getZIndex() < 10)
			extra->render(trans);

	if (mPercent >= 0)
	{
		Renderer::setMatrix(trans);

		auto pos = mInactiveProgressbar.getPosition();
		auto sz = mInactiveProgressbar.getSize();

		mInactiveProgressbar.setOpacity(alpha);
		mActiveProgressbar.setOpacity(alpha);
		mActiveProgressbar.setSize(sz.x()  * mPercent, sz.y());

		float radius = Math::max(sz.x(), sz.y()) * mRoundCorners;

		if (radius > 1)
			Renderer::enableRoundCornerStencil(pos.x(), pos.y(), sz.x(), sz.y(), radius);

		mInactiveProgressbar.render(trans);
		mActiveProgressbar.render(trans);

		if (radius > 1)
			Renderer::disableStencil();
	}

	if (!mText.getText().empty())
	{
		// Ensure font is loaded
		auto font = mText.getFont();
		if (font != nullptr)
			font->reload();
		else
			Font::get(FONT_SIZE_MEDIUM)->reload();

		mText.render(trans);
	}

	for (auto extra : mExtras)
		if (extra->getZIndex() >= 10)
			extra->render(trans);

	if (swapBuffers)
	{
		Renderer::swapBuffers();

#if defined(_WIN32)
		// Avoid Window Freezing on Windows
		SDL_Event event;
		while (SDL_PollEvent(&event));
#endif
	}
}
