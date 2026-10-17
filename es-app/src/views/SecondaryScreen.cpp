#include "SecondaryScreen.h"
#include "Window.h"
#include "ViewController.h"
#include "SystemView.h"
#include "gamelist/IGameListView.h"
#include "BindingManager.h"
#include "gamelist/GameNameFormatter.h"
#include "components/TextComponent.h"
#include "math/Transform4x4f.h"
#include <algorithm>

SecondaryScreen::~SecondaryScreen() { clear(); }

void SecondaryScreen::clear()
{
    for (auto& extra : mExtras)
        extra->onHide();
    mExtras.clear();
    if (mSystemGrid)
        mSystemGrid->onHide();
    mSystemGrid.reset();
    if (mCarousel)
        mCarousel->onHide();
    mCarousel.reset();
    mEntries.clear();
    mTheme.reset();
}

void SecondaryScreen::renderFrame(int deltaTime)
{
    const Vector2f size(Renderer::getScreenWidth(), Renderer::getScreenHeight());
    Renderer::setMatrix(Transform4x4f::Identity());
    if (mWindow->isScreenSaverRunning())
    {
        // Secondary components are outside Window's GUI stack. Stop their media
        // and present black before the main loop can enter its sleeping state.
        if (mTheme)
            clear();
        Renderer::drawRect(0, 0, size.x(), size.y(), 0x000000ff);
        return;
    }
    Renderer::drawRect(0, 0, size.x(), size.y(), 0x373737ff);
    auto controller = ViewController::get();
    const auto mode = controller->getViewMode();
    if (mode != ViewController::SYSTEM_SELECT && mode != ViewController::GAME_LIST)
        return;
    auto system = controller->getSelectedSystem();
    if (!system)
        return;
    auto theme = system->getTheme();
    IBindable* selection = system;
    std::string view = "secondary-system";
    std::vector<IBindable*> entries;
    if (mode == ViewController::GAME_LIST)
    {
        auto gameView = controller->getGameListView(system, false);
        if (gameView)
        {
            selection = gameView->getCursor();
            for (auto file : gameView->getFileDataEntries())
                entries.push_back(file);
            view = "secondary-" + std::string(gameView->getName());
        }
        if (!theme->hasView(view))
            view = "secondary-basic";
    }

    else
        for (auto item : controller->getSystemListView()->getObjects())
            entries.push_back(item);

    const bool systemMode = mode == ViewController::SYSTEM_SELECT;
    const char* carouselName = systemMode ? "systemcarousel" : "gamecarousel";
    const char* carouselType = systemMode ? "carousel" : "gamecarousel";
    if (theme != mTheme || view != mView || size != mSize || entries != mEntries)
    {
        // System selection changes the theme object. Preserve the carousel's
        // camera and animation when its entry list and screen are unchanged.
        std::unique_ptr<CarouselComponent> retainedCarousel;
        if (systemMode && view == mView && size == mSize && entries == mEntries &&
            theme->getElement(view, carouselName, carouselType) &&
            !theme->getElement(view, "imagegrid", "imagegrid"))
            retainedCarousel = std::move(mCarousel);
        // Keep shared textures alive until replacement components have acquired
        // them. Dropping the last reference here causes a reload/fade every scroll.
        auto previousExtras = std::move(mExtras);
        for (auto& extra : previousExtras)
            extra->onHide();
        clear();
        mCarousel = std::move(retainedCarousel);
        mTheme = theme;
        mView = view;
        mSize = size;
        mEntries = entries;
        for (auto extra : ThemeData::makeExtras(theme, view, mWindow, systemMode))
        {
            mExtras.emplace_back(extra);
            if (selection)
                BindingManager::updateBindings(extra, selection);
            extra->onShow();
        }
        std::stable_sort(mExtras.begin(), mExtras.end(), [](const std::unique_ptr<GuiComponent>& a, const std::unique_ptr<GuiComponent>& b) {
            return a->getZIndex() < b->getZIndex();
        });
        if (mode == ViewController::SYSTEM_SELECT && theme->getElement(view, "imagegrid", "imagegrid"))
        {
            mSystemGrid.reset(new ImageGridComponent<SystemData*>(mWindow));
            mSystemGrid->setThemeName("system");
            mSystemGrid->applyTheme(theme, view, "imagegrid", ThemeFlags::ALL);
            for (auto item : controller->getSystemListView()->getObjects())
                mSystemGrid->add(item->getFullName(), item->getProperty("image").toString(), item);
            mSystemGrid->setCursor(system);
            mSystemGrid->onShow();
        }
        if (!mSystemGrid && theme->getElement(view, carouselName, carouselType))
        {
            const bool created = !mCarousel;
            if (created)
                mCarousel.reset(new CarouselComponent(mWindow));
            if (systemMode)
                mCarousel->setThemedContext("logo", "logoText", "systemcarousel", "carousel", CarouselType::HORIZONTAL, CarouselImageSource::IMAGE);
            mCarousel->applyTheme(theme, view, carouselName, ThemeFlags::ALL);
            GameNameFormatter formatter(system);
            if (created)
                for (auto item : entries)
                    mCarousel->add(systemMode ? static_cast<SystemData*>(item)->getFullName() : formatter.getDisplayName(static_cast<FileData*>(item)), item);
            if (selection && mCarousel->size())
                mCarousel->setCursor(selection);
            if (created)
            {
                mCarousel->finishAnimation(0);
                mCarousel->onShow();
            }
        }
    }
    if (mSystemGrid && mSystemGrid->size() && mSystemGrid->getSelected() != system)
        mSystemGrid->setCursor(system);

    if (mCarousel && selection && mCarousel->size() && mCarousel->getSelected() != selection)
        mCarousel->setCursor(selection);

    GuiComponent* navigation = mSystemGrid ? static_cast<GuiComponent*>(mSystemGrid.get()) : mCarousel.get();
    auto drawGrid = [&]() {
        if (navigation)
        {
            navigation->update(deltaTime);
            navigation->render(Transform4x4f::Identity());
        }
    };
    bool gridDrawn = false;
    for (auto& extra : mExtras)
    {
        if (navigation && !gridDrawn && extra->getZIndex() > navigation->getZIndex())
        {
            drawGrid();
            gridDrawn = true;
        }
        if (selection)
            BindingManager::updateBindings(extra.get(), selection);
        extra->update(deltaTime);
        extra->render(Transform4x4f::Identity());
    }
    if (!gridDrawn)
        drawGrid();
}
