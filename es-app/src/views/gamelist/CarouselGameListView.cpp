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
	
	FileData* file = (mList.size() == 0 || mList.isScrolling()) ? NULL : mList.getSelected();
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
	SystemData* system = mCursorStack.size() && mRoot->getSystem()->isGroupSystem() ? mCursorStack.top()->getSystem() : mRoot->getSystem();

	auto groupTheme = system->getTheme();
	if (groupTheme && mHeaderImage.hasImage())
	{
		const ThemeData::ThemeElement* logoElem = groupTheme->getElement(getName(), "logo", "image");
		if (logoElem && logoElem->has("path") && Utils::FileSystem::exists(logoElem->get<std::string>("path")))
			mHeaderImage.setImage(logoElem->get<std::string>("path"), false, mHeaderImage.getMaxSizeInfo());
	}

	mHeaderText.setText(system->getFullName());

	mList.clear();

	if (files.size() > 0)
	{
		bool showParentFolder = mRoot->getSystem()->getShowParentFolder();
		if (showParentFolder && mCursorStack.size())
		{
			FileData* placeholder = new FileData(PLACEHOLDER, "..", this->mRoot->getSystem());
			mList.add(". .", placeholder);
		}

		GameNameFormatter formatter(mRoot->getSystem());

		bool favoritesFirst = mRoot->getSystem()->getShowFavoritesFirst();		
		if (favoritesFirst)
		{
			for (auto file : files)
			{
				if (!file->getFavorite())
					continue;
				
				mList.add(formatter.getDisplayName(file), file);
			}
		}

		for (auto file : files)		
		{
			if (file->getFavorite() && favoritesFirst)
				continue;

			mList.add(formatter.getDisplayName(file), file);
		}

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

	return mList.getSelected();
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
	FileData* placeholder = new FileData(PLACEHOLDER, "<" + _("No Entries Found") + ">", mRoot->getSystem());	
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
	FolderData* parent = game->getParent();
	if (getCursor() == game)                     // Select next element in list, or prev if none
	{
		std::vector<FileData*> siblings = mList.getObjects();

		int gamePos = getCursorIndex();
		if ((gamePos + 1) < (int)siblings.size())
			setCursor(siblings.at(gamePos + 1));
		else if ((gamePos - 1) > 0)
			setCursor(siblings.at(gamePos - 1));
	}

	mList.remove(game);
	if(mList.size() == 0)
		addPlaceholder();

	mRoot->removeFromVirtualFolders(game);
	delete game;                                 // remove before repopulating (removes from parent)
	onFileChanged(parent, FILE_REMOVED);           // update the view, with game removed
}

// batocera
void CarouselGameListView::setCursorIndex(int cursor){
	mList.setCursorIndex(cursor);
}

// batocera
int CarouselGameListView::getCursorIndex(){
	return mList.getCursorIndex();
}

std::vector<FileData*> CarouselGameListView::getFileDataEntries()
{
	return mList.getObjects();	
}

void CarouselGameListView::update(int deltaTime)
{
	mDetails.update(deltaTime);
	ISimpleGameListView::update(deltaTime);
}
