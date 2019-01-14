#pragma once

#include "GuiComponent.h"

class ScrollableContainer : public GuiComponent
{
public:
	ScrollableContainer(Window* window);

	Vector2f getScrollPos() const;
	void setScrollPos(const Vector2f& pos);
	void setAutoScroll(bool autoScroll);
	void reset();

	void update(int deltaTime) override;
	void render(const Transform4x4f& parentTrans) override;
	int mAutoScrollDelay; // ms to wait before starting to autoscroll
	int mAutoScrollSpeed; // ms to wait before scrolling down by mScrollDir
	int mAutoScrollResetAccumulator;
	int mAutoScrollAccumulator;


private:
	Vector2f getContentSize();

	Vector2f mScrollPos;
	Vector2f mScrollDir;
	bool mAtEnd;
};
