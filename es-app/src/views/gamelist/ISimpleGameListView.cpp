#include "views/gamelist/ISimpleGameListView.h"

#include "views/UIModeController.h"
#include "views/ViewController.h"
#include "CollectionSystemManager.h"
#include "Settings.h"
#include "Sound.h"
#include "SystemData.h"
#include "SystemConf.h"
#include "guis/GuiMsgBox.h"
#include "Window.h"
#include "LocaleES.h"
#include "guis/GuiSettings.h"
#include <set>
#include "components/SwitchComponent.h"
#include "ApiSystem.h"
#include "animations/LambdaAnimation.h"

ISimpleGameListView::ISimpleGameListView(Window* window, FolderData* root, bool temporary) : IGameListView(window, root),
	mHeaderText(window), mHeaderImage(window), mBackground(window), mFolderPath(window), mOnExitPopup(nullptr)
{
	mHeaderText.setText("Logo Text");
	mHeaderText.setSize(mSize.x(), 0);
	mHeaderText.setPosition(0, 0);
	mHeaderText.setHorizontalAlignment(ALIGN_CENTER);
	mHeaderText.setDefaultZIndex(50);
	
	mHeaderImage.setResize(0, mSize.y() * 0.185f);
	mHeaderImage.setOrigin(0.5f, 0.0f);
	mHeaderImage.setPosition(mSize.x() / 2, 0);
	mHeaderImage.setDefaultZIndex(50);

	mBackground.setResize(mSize.x(), mSize.y());
	mBackground.setDefaultZIndex(0);

	mFolderPath.setHorizontalAlignment(ALIGN_CENTER);
	mFolderPath.setDefaultZIndex(55);
	mFolderPath.setVisible(false);

	addChild(&mHeaderText);
	addChild(&mBackground);
	addChild(&mFolderPath);
}

void ISimpleGameListView::onThemeChanged(const std::shared_ptr<ThemeData>& theme)
{
	using namespace ThemeFlags;
	mBackground.applyTheme(theme, getName(), "background", ALL);
	mHeaderImage.applyTheme(theme, getName(), "logo", ALL);
	mHeaderText.applyTheme(theme, getName(), "logoText", ALL);
	mFolderPath.applyTheme(theme, getName(), "folderpath", ALL);

	// Remove old theme extras
	for (auto extra : mThemeExtras)
	{
		removeChild(extra);
		delete extra;
	}
	mThemeExtras.clear();

	// Add new theme extras
	mThemeExtras = ThemeData::makeExtras(theme, getName(), mWindow);
	for (auto extra : mThemeExtras)
		addChild(extra);

	if(mHeaderImage.hasImage())
	{
		removeChild(&mHeaderText);
		addChild(&mHeaderImage);
	}
	else
	{
		addChild(&mHeaderText);
		removeChild(&mHeaderImage);
	}
}

void ISimpleGameListView::onFileChanged(FileData* /*file*/, FileChangeType /*change*/)
{
	// we could be tricky here to be efficient;
	// but this shouldn't happen very often so we'll just always repopulate
	FileData* cursor = getCursor();
	if (!cursor->isPlaceHolder()) 
	{
		populateList(cursor->getParent()->getChildrenListToDisplay());
		setCursor(cursor);
	}
	else
	{
		while (mCursorStack.size())
			mCursorStack.pop();

		populateList(mRoot->getChildrenListToDisplay());
		setCursor(cursor);
	}
}

void ISimpleGameListView::moveToFolder(FolderData* folder)
{
	if (folder == nullptr || folder->getChildren().size() == 0)
		return;
	
	mCursorStack.push(folder);
	populateList(folder->getChildrenListToDisplay());
	
	FileData* cursor = getCursor();
	if (cursor != nullptr)
		setCursor(cursor);	
}

FolderData*		ISimpleGameListView::getCurrentFolder()
{
	if (mCursorStack.size())
	{
		auto top = mCursorStack.top();
		if (top->getType() == FOLDER)
			return (FolderData*)top;
	}

	return nullptr;
}

bool ISimpleGameListView::input(InputConfig* config, Input input)
{
	if(input.value != 0)
	{
		if(config->isMappedTo(BUTTON_OK, input))
		{
			// Don't launch game if transition is still running
			if (ViewController::get()->isAnimationPlaying(0))
				return true;
			
			FileData* cursor = getCursor();
			FolderData* folder = NULL;

			if (mCursorStack.size() && cursor->getType() == PLACEHOLDER && cursor->getPath() == "..")
			{
				auto top = mCursorStack.top();
				mCursorStack.pop();

				FolderData* folder = top->getParent();
				if (folder == nullptr)
					folder = getCursor()->getSystem()->getParentGroupSystem()->getRootFolder();

				populateList(folder->getChildrenListToDisplay());
				setCursor(top);
				Sound::getFromTheme(getTheme(), getName(), "back")->play();
			}
			else
			{
				if (cursor->getType() == FOLDER)
					folder = (FolderData*)cursor;

				if (cursor->getType() == GAME)
				{
					Sound::getFromTheme(getTheme(), getName(), "launch")->play();
					launch(cursor);
				}
				else 
				{
					// it's a folder
					if (folder != nullptr)
						moveToFolder(folder);
				}
			}
			return true;
		}
		else if (config->isMappedTo(BUTTON_BACK, input))
		{
			if (mCursorStack.size())
			{
				auto top = mCursorStack.top();
				mCursorStack.pop();

				FolderData* folder = top->getParent();
				if (folder == nullptr && getCursor()->getSystem()->getParentGroupSystem() != nullptr)
					folder = getCursor()->getSystem()->getParentGroupSystem()->getRootFolder();

				if (folder == nullptr)
					return true;

				populateList(folder->getChildrenListToDisplay());
				setCursor(top);				
				Sound::getFromTheme(getTheme(), getName(), "back")->play();
			}
			else if (mPopupSelfReference)
			{				
				ViewController::get()->setActiveView(mPopupParentView);				
				closePopupContext();
				return true;
			}
			else
			{
				onFocusLost();
				SystemData* systemToView = getCursor()->getSystem();

				if (systemToView->isGroupChildSystem())
					systemToView = systemToView->getParentGroupSystem();
				else if (systemToView->isCollection())
					systemToView = CollectionSystemManager::get()->getSystemToView(systemToView);

				ViewController::get()->goToSystemView(systemToView);
			}

			return true;
		}else if(
#ifdef _ENABLEEMUELEC
		config->isMappedLike(getQuickSystemSelectRightButton(), input) || config->isMappedLike("rightshoulder", input))
#else
		config->isMappedLike(getQuickSystemSelectRightButton(), input) || config->isMappedLike("r2", input))
#endif
		{
			if (!mPopupSelfReference)
			{
				onFocusLost();
				ViewController::get()->goToNextGameList();
				return true;
			}
		}else if(
#ifdef _ENABLEEMUELEC
		config->isMappedLike(getQuickSystemSelectLeftButton(), input) || config->isMappedLike("leftshoulder", input))
#else
		config->isMappedLike(getQuickSystemSelectLeftButton(), input) || config->isMappedLike("l2", input))
#endif
		{
			if (!mPopupSelfReference)
			{
				onFocusLost();
				ViewController::get()->goToPrevGameList();
				return true;
			}
		}else if (config->isMappedTo("x", input))
		{
			FileData* cursor = getCursor();
			if(cursor != nullptr && cursor->isNetplaySupported())
			{
				Window* window = mWindow;
				
				GuiSettings* msgBox = new GuiSettings(mWindow, _("NETPLAY"), "", nullptr, true);
				msgBox->setSubTitle(cursor->getName());
				msgBox->setTag("popup");				
				msgBox->addGroup(_("START GAME"));

				msgBox->addEntry(_U("\uF144 ") + _("START NETPLAY HOST"), false, [this, msgBox, cursor]
				{
					if (ApiSystem::getInstance()->getIpAdress() == "NOT CONNECTED")
					{
						mWindow->pushGui(new GuiMsgBox(mWindow, _("YOU ARE NOT CONNECTED TO A NETWORK"), _("OK"), nullptr));
						return;
					}
				
					LaunchGameOptions options;
					options.netPlayMode = SERVER;
					ViewController::get()->launch(cursor, options);
					msgBox->close();					
				});
			
				msgBox->addGroup(_("OPTIONS"));
				msgBox->addInputTextRow(_("SET PLAYER PASSWORD"), "global.netplay.password", false);
				msgBox->addInputTextRow(_("SET VIEWER PASSWORD"), "global.netplay.spectatepassword", false);
				
				mWindow->pushGui(msgBox);

				return true;
			}				
			
			// go to random system game
			FileData* randomGame = getRandomGame();
			if (randomGame)
				setCursor(randomGame);

			return true;
		}
		else if (config->isMappedTo("y", input) && !UIModeController::getInstance()->isUIModeKid())
		{
			if (mRoot->getSystem()->isGameSystem() || mRoot->getSystem()->isGroupSystem())
				if (CollectionSystemManager::get()->toggleGameInCollection(getCursor()))
					return true;
		}
	}
	return IGameListView::input(config, input);
}

FileData* ISimpleGameListView::getRandomGame()
{
	auto list = getFileDataEntries();

	unsigned int total = (int)list.size();
	if (total == 0)
		return nullptr;

	int target = (int)Math::round((std::rand() / (float)RAND_MAX) * (total - 1));
	if (target >= 0 && target < total)
		return list.at(target);

	return nullptr;
}

std::vector<std::string> ISimpleGameListView::getEntriesLetters()
{	
	std::set<std::string> setOfLetters;

	for (auto file : getFileDataEntries()) 
		if (file->getType() == GAME)
			setOfLetters.insert(std::string(1, toupper(file->getName()[0])));

	std::vector<std::string> letters;

	for (const auto letter : setOfLetters)
		letters.push_back(letter);

	std::sort(letters.begin(), letters.end());
	return letters;
}

void ISimpleGameListView::updateFolderPath()
{
	if (mCursorStack.size())
	{
		auto top = mCursorStack.top();
		mFolderPath.setText(top->getBreadCrumbPath());
	}
	else 
		mFolderPath.setText("");
}

void ISimpleGameListView::repopulate()
{
	FolderData* folder = mRoot;

	if (mCursorStack.size())
	{
		auto top = mCursorStack.top();
		if (top->getType() == FOLDER)
			folder = (FolderData*)top;
	}

	populateList(folder->getChildrenListToDisplay());
}

void ISimpleGameListView::setPopupContext(std::shared_ptr<IGameListView> pThis, std::shared_ptr<GuiComponent> parentView, const std::string label, const std::function<void()>& onExitTemporary)
{ 
	mPopupSelfReference = pThis;
	mPopupParentView = parentView;
	mOnExitPopup = onExitTemporary;

	if (mHeaderImage.hasImage())
	{
		mHeaderText.setText(_("Games similar to") + " : " + label); // 

		mHeaderImage.setImage("");
		addChild(&mHeaderText);
		removeChild(&mHeaderImage);
	}
}

void ISimpleGameListView::closePopupContext()
{
	if (!mPopupSelfReference)
		return;

	auto exitPopup = mOnExitPopup;

	mPopupParentView.reset();	
	mPopupSelfReference.reset();

	if (exitPopup != nullptr)
		exitPopup();
}
