#pragma once

#include "GuiComponent.h"
#include "components/ImageGridComponent.h"
#include "components/CarouselComponent.h"
#include "SystemData.h"
#include <memory>

class SecondaryScreen
{
public:
    explicit SecondaryScreen(Window* window) : mWindow(window) {}
    ~SecondaryScreen();
    void renderFrame(int deltaTime);

private:
    void clear();
    Window* mWindow;
    std::shared_ptr<ThemeData> mTheme;
    std::string mView;
    Vector2f mSize;
    std::vector<std::unique_ptr<GuiComponent>> mExtras;
    std::unique_ptr<ImageGridComponent<SystemData*>> mSystemGrid;
    std::unique_ptr<CarouselComponent> mCarousel;
    std::vector<IBindable*> mEntries;
};
