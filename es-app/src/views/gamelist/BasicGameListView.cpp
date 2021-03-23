#include "views/gamelist/BasicGameListView.h"

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

BasicGameListView::BasicGameListView(Window* window, FolderData* root)
	: ISimpleGameListView(window, root), mList(window)
{
	mList.setSize(mSize.x(), mSize.y() * 0.8f);
	mList.setPosition(0, mSize.y() * 0.2f);
	mList.setDefaultZIndex(20);

	mList.setCursorChangedCallback([&](const CursorState& /*state*/) 
		{ 
			if (mRoot->getSystem()->isCollection())
				updateHelpPrompts();
		});

	addChild(&mList);

	populateList(root->getChildrenListToDisplay());
}

void BasicGameListView::onThemeChanged(const std::shared_ptr<ThemeData>& theme)
{
	ISimpleGameListView::onThemeChanged(theme);
	using namespace ThemeFlags;
	mList.applyTheme(theme, getName(), "gamelist", ALL);

	sortChildren();
}

void BasicGameListView::onFileChanged(FileData* file, FileChangeType change)
{
	if(change == FILE_METADATA_CHANGED)
	{
		// might switch to a detailed view
		ViewController::get()->reloadGameListView(this);
		return;
	}

	ISimpleGameListView::onFileChanged(file, change);
}

void BasicGameListView::populateList(const std::vector<FileData*>& files)
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
			mList.add(". .", placeholder, true);
		}

		GameNameFormatter formatter(mRoot->getSystem());

		bool favoritesFirst = mRoot->getSystem()->getShowFavoritesFirst();
		if (favoritesFirst)
		{			
			for (auto file : files)
			{
				if (!file->getFavorite())
					continue;
						
				mList.add(formatter.getDisplayName(file), file, file->getType() == FOLDER);
			}
		}

		for (auto file : files)		
		{
			if (favoritesFirst && file->getFavorite())
				continue;
				
			mList.add(formatter.getDisplayName(file), file, file->getType() == FOLDER);
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

FileData* BasicGameListView::getCursor()
{
	if (mList.size() == 0)
		return nullptr;

	return mList.getSelected();
}

void BasicGameListView::setCursor(FileData* cursor)
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

void BasicGameListView::addPlaceholder()
{
	// empty list - add a placeholder
	FileData* placeholder = new FileData(PLACEHOLDER, "<" + _("No Entries Found") + ">", mRoot->getSystem());	
	mList.add(placeholder->getName(), placeholder, true);
}

std::string BasicGameListView::getQuickSystemSelectRightButton()
{
	return "right";
}

std::string BasicGameListView::getQuickSystemSelectLeftButton()
{
	return "left";
}

void BasicGameListView::launch(FileData* game)
{
	ViewController::get()->launch(game);
}

void BasicGameListView::remove(FileData *game)
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

std::vector<HelpPrompt> BasicGameListView::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;

	if (Renderer::getScreenProportion() > 1.4)
	{
		if (mPopupSelfReference == nullptr && Settings::getInstance()->getBool("QuickSystemSelect"))
			prompts.push_back(HelpPrompt("left/right", _("SYSTEM"))); // batocera

		prompts.push_back(HelpPrompt("up/down", _("CHOOSE"))); // batocera
	}

	prompts.push_back(HelpPrompt(BUTTON_OK, _("LAUNCH")));
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK")));

	if(!UIModeController::getInstance()->isUIModeKid())
	  prompts.push_back(HelpPrompt("select", _("VIEW OPTIONS"))); // batocera

	if (UIModeController::getInstance()->isUIModeKid())
		prompts.push_back(HelpPrompt("x", _("GAME OPTIONS")));
	else
		prompts.push_back(HelpPrompt("x", _("GAME OPTIONS") + std::string(" / ") + _("FAVORITE")));

	prompts.push_back(HelpPrompt("y", _("RANDOM") + std::string(" / ") + _("SEARCH")));
	/*
	FileData* cursor = getCursor();
	if (cursor != nullptr && cursor->isNetplaySupported())
		prompts.push_back(HelpPrompt("x", _("NETPLAY"))); // batocera
	else
		prompts.push_back(HelpPrompt("x", _("RANDOM"))); // batocera

	if(mRoot->getSystem()->isGameSystem() && !UIModeController::getInstance()->isUIModeKid())
	{
		std::string prompt = CollectionSystemManager::get()->getEditingCollection();
		
		if (Utils::String::toLower(prompt) == "favorites")
			prompts.push_back(HelpPrompt("y", _("Favorites")));
		else
			prompts.push_back(HelpPrompt("y", _(prompt.c_str())));
	}*/

	return prompts;
}

// batocera
void BasicGameListView::setCursorIndex(int cursor){
	mList.setCursorIndex(cursor);
}

// batocera
int BasicGameListView::getCursorIndex(){
	return mList.getCursorIndex();
}

std::vector<FileData*> BasicGameListView::getFileDataEntries()
{
	return mList.getObjects();	
}
