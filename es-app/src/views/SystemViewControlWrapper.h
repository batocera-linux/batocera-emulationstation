#pragma once

#ifndef ES_APP_VIEWS_SYSTEM_VIEW_WRAPPER_H
#define ES_APP_VIEWS_SYSTEM_VIEW_WRAPPER_H

#include <memory>
#include <functional>
#include <map>
#include <unordered_set>

#include "components/CarouselComponent.h"
#include "components/TextListComponent.h"
#include "components/ImageGridComponent.h"
#include "SystemData.h"

class ControlWrapper
{
public:
	ControlWrapper()
	{
		mComp = nullptr; mText = nullptr; mGrid = nullptr; mCarousel = nullptr;
	}

	void attach(GuiComponent* comp)
	{
		if (mComp != nullptr && mComp != comp)
			delete mComp;

		mComp = comp;
		mText = dynamic_cast<TextListComponent<SystemData*>*>(mComp);
		mGrid = dynamic_cast<ImageGridComponent<SystemData*>*>(mComp);
		mCarousel = dynamic_cast<CarouselComponent*>(mComp);
	}

	bool isCarousel() { return mCarousel != nullptr; }
	bool isGrid()     { return mGrid != nullptr; }
	bool isTextList() { return mText != nullptr; }

	CarouselComponent* asCarousel()
	{
		return mCarousel;
	}

	ImageGridComponent<SystemData*>* asGrid()
	{
		return mGrid;
	}

	void setCursorChangedCallback(const std::function<void(CursorState state)>& func)
	{
		if (mText) mText->setCursorChangedCallback(func);
		else if (mGrid) mGrid->setCursorChangedCallback(func);
		else if (mCarousel) mCarousel->setCursorChangedCallback(func);
	}

	// Specific methods 
	void add(const std::string& name, SystemData* obj, bool preloadLogo)
	{
		if (mText)
		{
			mText->add(name, obj, 0);
			return;
		}

		if (mGrid)
		{
			mGrid->add(name, obj->getProperty("image").toString(), obj);
			return;
		}

		if (mCarousel)
		{
			mCarousel->add(name, obj, preloadLogo);
		}
	}

	int size() const 
	{ 
		if (mText) return mText->size();
		if (mGrid) return mGrid->size();
		return mCarousel->size();
	}

	// GuiComponent wrapped methods
	std::vector<HelpPrompt> getHelpPrompts() { return mComp->getHelpPrompts(); }
	float getZIndex() const { return mComp->getZIndex(); }
	bool input(InputConfig* config, Input input) { return mComp->input(config, input); }
	void update(int deltaTime) { mComp->update(deltaTime); }
	void render(const Transform4x4f& parentTrans) { mComp->render(parentTrans); }
	void onShow() { mComp->onShow(); }
	void onHide() { mComp->onHide(); }
	bool onMouseClick(int button, bool pressed, int x, int y) { return mComp->onMouseClick(button, pressed, x, y); }
	void onMouseMove(int x, int y) { mComp->onMouseMove(x, y); }
	bool onMouseWheel(int delta) { return mComp->onMouseWheel(delta); }
	bool hitTest(int x, int y, Transform4x4f& parentTransform, std::vector<GuiComponent*>* pResult) { return mComp->hitTest(x, y, parentTransform, pResult); }
	void applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties) { mComp->applyTheme(theme, view, element, properties); }
	bool finishAnimation(unsigned char slot) { return mComp->finishAnimation(slot); }
	void setZIndex(float z) { return mComp->setZIndex(z); }
	void setDefaultZIndex(float z) { return mComp->setDefaultZIndex(z); }

	void setSize(float w, float h) { mComp->setSize(w, h); }
	void setPosition(float x, float y, float z = 0.0f) { mComp->setPosition(x, y, z); }
	Vector3f getPosition() const { return mComp->getPosition(); }
	Vector2f getSize() const { return mComp->getSize(); }

	void clear()
	{
		if (mCarousel) mCarousel->clear();
		else if (mText) mText->clear();
		else if (mGrid) mGrid->clear();
	}

	// IList wrapped methods
	int getCursorIndex()
	{
		if (mText) return mText->getCursorIndex();
		if (mGrid) return mGrid->getCursorIndex();
		return mCarousel->getCursorIndex();
	}

	void setCursor(SystemData* system)
	{
		if (mCarousel) mCarousel->setCursor(system);
		else if (mText) mText->setCursor(system);
		else if (mGrid) mGrid->setCursor(system);
	}

	int getScrollingVelocity()
	{
		if (mText) return mText->getScrollingVelocity();
		if (mGrid) return mGrid->getScrollingVelocity();
		return mCarousel->getScrollingVelocity();
	}

	void stopScrolling()
	{
		if (mCarousel) mCarousel->stopScrolling();
		else if (mText) mText->stopScrolling();
		else if (mGrid) mGrid->stopScrolling();
	}

	SystemData* getSelected()
	{
		if (mText) return mText->getSelected();
		if (mGrid) return mGrid->getSelected();
		return dynamic_cast<SystemData*>(mCarousel->getActiveObject());
	}

	void moveSelectionBy(int count)
	{
		if (mText) mText->moveSelectionBy(count);
		else if (mGrid) mGrid->moveSelectionBy(count);
		else mCarousel->moveSelectionBy(count);
	}

	std::vector<SystemData*> getObjects()
	{
		if (mText) return mText->getObjects();
		if (mGrid) return mGrid->getObjects();

		std::vector<SystemData*> ret;

		if (mCarousel)
		{
			for (auto item : mCarousel->getObjects())
			{
				SystemData* data = dynamic_cast<SystemData*>(item);
				if (data != nullptr)
					ret.push_back(data);
			}
		}

		return ret;		
	}

private:
	GuiComponent* mComp;

	CarouselComponent* mCarousel;
	ImageGridComponent<SystemData*>* mGrid;
	TextListComponent<SystemData*>* mText;
};

#endif // ES_APP_VIEWS_SYSTEM_VIEW_WRAPPER_H
