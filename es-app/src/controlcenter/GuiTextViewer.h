#pragma once
#ifndef ES_APP_CONTROLCENTER_GUITEXTVIEWER_H
#define ES_APP_CONTROLCENTER_GUITEXTVIEWER_H

#include "GuiComponent.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

class Font;
struct TextCache;

// Fullscreen viewer for the plain text documents a control center <doc> opens.
class GuiTextViewer : public GuiComponent
{
public:
	GuiTextViewer(Window* window, const std::string& title, const std::string& path);
	~GuiTextViewer();

	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;
	void render(const Transform4x4f& parentTrans) override;

	bool hitTest(int x, int y, Transform4x4f& parentTransform, std::vector<GuiComponent*>* pResult = nullptr) override;
	bool onMouseClick(int button, bool pressed, int x, int y) override;
	void onMouseMove(int x, int y) override;
	bool onMouseWheel(int delta) override;

	std::vector<HelpPrompt> getHelpPrompts() override;

private:
	void setFontSize(int size);
	void scrollBy(float delta);
	float maxScroll();

	std::string mTitle;
	std::vector<std::string> mParagraphs;
	std::vector<std::string> mLines;

	std::string mFontPath;
	int mFontSize;
	std::shared_ptr<Font> mFont;
	std::shared_ptr<Font> mTitleFont;

	float mPad;
	float mTitleH;
	float mScroll;

	int mRepeatDir;
	int mRepeatTime;

	bool mDragging;
	float mDownY;
	float mDownScroll;

	std::map<int, std::unique_ptr<TextCache>> mCache;
};

#endif
