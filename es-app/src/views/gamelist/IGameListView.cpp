#include "views/gamelist/IGameListView.h"

#include "guis/GuiGamelistOptions.h"
#include "views/UIModeController.h"
#include "views/ViewController.h"
#include "Sound.h"
#include "Window.h"

void IGameListView::setTheme(const std::shared_ptr<ThemeData>& theme)
{
	mTheme = theme;
	onThemeChanged(theme);
}

HelpStyle IGameListView::getHelpStyle()
{
	HelpStyle style;
	style.applyTheme(mTheme, getName());
	return style;
}

void IGameListView::render(const Transform4x4f& parentTrans)
{
	Transform4x4f trans = parentTrans * getTransform();

	auto rect = Renderer::getScreenRect(trans, mSize);
	if (!Renderer::isVisibleOnScreen(rect))
		return;

	Renderer::pushClipRect(rect);
	renderChildren(trans);
	Renderer::popClipRect();
}

void IGameListView::setThemeName(std::string name)
{
	mCustomThemeName = name;
}

bool IGameListView::hasFileDataEntry(FileData* file)
{
	std::string path = file->getPath();

	for (auto item : getFileDataEntries())
		if (item->getPath() == path)
			return true;

	return false;
}