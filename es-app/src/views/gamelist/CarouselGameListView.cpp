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
	mDetails.updateControls(file, isClearing);
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
	if(!mList.setCursor(cursor) && (!cursor->isPlaceHolder()))
	{
		auto children = mRoot->getChildrenListToDisplay();

		auto gameIter = std::find(children.cbegin(), children.cend(), cursor);
		if (gameIter == children.cend())
		{
			if (cursor->getParent() != nullptr)
				children = cursor->getParent()->getChildrenListToDisplay();

			// update our cursor stack in case our cursor just got set to some folder we weren't in before
			if (mCursorStack.empty() || mCursorStack.top() != cursor->getParent())
			{
				std::stack<FileData*> tmp;
				FileData* ptr = cursor->getParent();
				while (ptr && ptr != mRoot)
				{
					tmp.push(ptr);
					ptr = ptr->getParent();
				}

				// flip the stack and put it in mCursorStack
				mCursorStack = std::stack<FileData*>();
				while (!tmp.empty())
				{
					mCursorStack.push(tmp.top());
					tmp.pop();
				}
			}
		}
	
		populateList(children);
		mList.setCursor(cursor);
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
	return "right";
}

std::string CarouselGameListView::getQuickSystemSelectLeftButton()
{
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

std::vector<HelpPrompt> CarouselGameListView::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;

	if (mPopupSelfReference == nullptr && Settings::getInstance()->getBool("QuickSystemSelect"))
		prompts.push_back(HelpPrompt("left/right", _("SYSTEM"))); // batocera

	prompts.push_back(HelpPrompt("up/down", _("CHOOSE"))); // batocera
	prompts.push_back(HelpPrompt(BUTTON_OK, _("LAUNCH")));
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK")));
	if(!UIModeController::getInstance()->isUIModeKid())
	  prompts.push_back(HelpPrompt("select", _("OPTIONS"))); // batocera

	if (UIModeController::getInstance()->isUIModeKid())
		prompts.push_back(HelpPrompt("x", _("GAME OPTIONS")));
	else
		prompts.push_back(HelpPrompt("x", _("GAME OPTIONS") + std::string(" / ") + _("FAVORITE")));

	prompts.push_back(HelpPrompt("y", _("RANDOM") + std::string(" / ") + _("SEARCH")));

	return prompts;
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
