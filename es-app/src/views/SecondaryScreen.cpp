#include "SecondaryScreen.h"
#include "ViewController.h"
#include "SystemView.h"
#include "gamelist/IGameListView.h"
#include "BindingManager.h"
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
    mTheme.reset();
}

void SecondaryScreen::renderFrame(int deltaTime)
{
    const Vector2f size(Renderer::getScreenWidth(), Renderer::getScreenHeight());
    Renderer::setMatrix(Transform4x4f::Identity());
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
    if (mode == ViewController::GAME_LIST)
    {
        auto gameView = controller->getGameListView(system, false);
        if (gameView)
        {
            selection = gameView->getCursor();
            view = "secondary-" + std::string(gameView->getName());
        }
        if (!theme->hasView(view))
            view = "secondary-basic";
    }

    if (theme != mTheme || view != mView || size != mSize)
    {
        clear();
        mTheme = theme;
        mView = view;
        mSize = size;
        for (auto extra : ThemeData::makeExtras(theme, view, mWindow))
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
    }
    if (mSystemGrid && mSystemGrid->size() && mSystemGrid->getSelected() != system)
        mSystemGrid->setCursor(system);

    auto drawGrid = [&]() {
        if (mSystemGrid)
        {
            mSystemGrid->update(deltaTime);
            mSystemGrid->render(Transform4x4f::Identity());
        }
    };
    bool gridDrawn = false;
    for (auto& extra : mExtras)
    {
        if (mSystemGrid && !gridDrawn && extra->getZIndex() > mSystemGrid->getZIndex())
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
