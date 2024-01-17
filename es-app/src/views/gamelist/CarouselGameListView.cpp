#include "views/gamelist/CarouselGameListView.h"

#include "utils/FileSystemUtil.h"
#include "views/UIModeController.h"
#include "views/ViewController.h"
#include "CollectionSystemManager.h"
#include "Settings.h"
#include "SystemData.h"
#include "SystemConf.h"
#include "FileData.h"
#include "LocaleES.h"
#include "GameNameFormatter.h"

CarouselGameListView::CarouselGameListView(Window* window, FolderData* root)
	: ISimpleGameListView(window, root),
	mList(window), mDetails(this, &mList, mWindow, DetailedContainer::DetailedView)
{
	// Let DetailedContainer handle extras with activation scripts
	mExtraMode = ThemeData::ExtraImportType::WITHOUT_ACTIVATESTORYBOARD;

	mList.setSize(mSize.x(), mSize.y() * 0.8f);
	mList.setPosition(0, mSize.y() * 0.2f);
	mList.setDefaultZIndex(20);	
	mList.setCursorChangedCallback([&](const CursorState& /*state*/) { updateInfoPanel(); });

	updateInfoPanel();
		
	addChild(&mList);

	populateList(root->getChildrenListToDisplay());
}

void CarouselGameListView::onThemeChanged(const std::shared_ptr<ThemeData>& theme)
{
	ISimpleGameListView::onThemeChanged(theme);

	mList.applyTheme(theme, getName(), "gamecarousel", ThemeFlags::ALL);
	mDetails.onThemeChanged(theme);

	sortChildren();
	updateInfoPanel();
}

void CarouselGameListView::updateInfoPanel()
{
	if (mRoot->getSystem()->isCollection())
		updateHelpPrompts();
	
	updateThemeExtrasBindings();

	FileData* file = (mList.size() == 0 || mList.isScrolling()) ? NULL : getCursor();
	bool isClearing = mList.getObjects().size() == 0 && mList.getCursorIndex() == 0 && mList.getScrollingVelocity() == 0;
	mDetails.updateControls(file, isClearing, mList.getCursorIndex() - mList.getLastCursor());
}

void CarouselGameListView::onFileChanged(FileData* file, FileChangeType change)
{
	if(change == FILE_METADATA_CHANGED)
	{
		// might switch to a detailed view
		ViewController::get()->reloadGameListView(this);
		return;
	}

	ISimpleGameListView::onFileChanged(file, change);
}

void CarouselGameListView::populateList(const std::vector<FileData*>& files)
{
	updateHeaderLogoAndText();

	mList.clear();

	if (files.size() > 0)
	{
		bool showParentFolder = mRoot->getSystem()->getShowParentFolder();
		if (showParentFolder && mCursorStack.size())
			mList.add(". .", createParentFolderData());

		GameNameFormatter formatter(mRoot->getSystem());

		for (auto file : files)		
			mList.add(formatter.getDisplayName(file), file);

		// if we have the ".." PLACEHOLDER, then select the first game instead of the placeholder
		if (showParentFolder && mCursorStack.size() && mList.size() > 1 && mList.getCursorIndex() == 0)
			mList.setCursorIndex(1);
	}
	else
	{
		addPlaceholder();
	}

	updateFolderPath();

	if (mShowing)
		onShow();
}

FileData* CarouselGameListView::getCursor()
{
	if (mList.size() == 0)
		return nullptr;

	return dynamic_cast<FileData*>(mList.getSelected());	
}

void CarouselGameListView::resetLastCursor()
{
	mList.resetLastCursor();
}

void CarouselGameListView::setCursor(FileData* cursor)
{
	if (cursor && !mList.setCursor(cursor) && !cursor->isPlaceHolder())
	{
		std::stack<FileData*> stack;
		auto childrenToDisplay = mRoot->findChildrenListToDisplayAtCursor(cursor, stack);
		if (childrenToDisplay != nullptr)
		{
			mCursorStack = stack;
			populateList(*childrenToDisplay.get());
			mList.setCursor(cursor);
		}
	}
}

void CarouselGameListView::addPlaceholder()
{
	// empty list - add a placeholder
	FileData* placeholder = createNoEntriesPlaceholder();
	mList.add(placeholder->getName(), placeholder);
}

std::string CarouselGameListView::getQuickSystemSelectRightButton()
{
	if (mList.isHorizontalCarousel())
		return "r2";

	return "right";
}

std::string CarouselGameListView::getQuickSystemSelectLeftButton()
{
	if (mList.isHorizontalCarousel())
		return "l2";

	return "left";
}

void CarouselGameListView::launch(FileData* game)
{
	ViewController::get()->launch(game);
}

void CarouselGameListView::remove(FileData *game)
{
	mList.remove(game);
	mRoot->removeFromVirtualFolders(game);
	delete game;

	if (mList.size() == 0)
		addPlaceholder();

	ViewController::get()->reloadGameListView(this);
}

void CarouselGameListView::setCursorIndex(int cursor)
{
	mList.setCursorIndex(cursor);
}

int CarouselGameListView::getCursorIndex()
{
	return mList.getCursorIndex();
}

std::vector<FileData*> CarouselGameListView::getFileDataEntries()
{
	std::vector<FileData*> ret;

	for (auto item : mList.getObjects())
	{
		FileData* data = dynamic_cast<FileData*>(item);
		if (data != nullptr)
			ret.push_back(data);
	}

	return ret;
}

void CarouselGameListView::update(int deltaTime)
{
	mDetails.update(deltaTime);
	ISimpleGameListView::update(deltaTime);
}

bool CarouselGameListView::onMouseWheel(int delta)
{
	return mList.onMouseWheel(delta);
}