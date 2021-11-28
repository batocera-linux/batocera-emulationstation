#include "views/gamelist/GridGameListView.h"

#include "animations/LambdaAnimation.h"
#include "views/UIModeController.h"
#include "views/ViewController.h"
#include "CollectionSystemManager.h"
#include "Settings.h"
#include "SystemData.h"
#include "LocaleES.h"
#include "Window.h"
#include "SystemConf.h"
#include "guis/GuiGamelistOptions.h"
#include "GameNameFormatter.h"
#include "utils/Randomizer.h"

GridGameListView::GridGameListView(Window* window, FolderData* root, const std::shared_ptr<ThemeData>& theme, std::string themeName, Vector2f gridSize) :
	ISimpleGameListView(window, root),
	mGrid(window),
	mDetails(this, &mGrid, mWindow, DetailedContainer::GridView)
{
	// Let DetailedContainer handle extras with activation scripts
	mExtraMode = ThemeData::ExtraImportType::WITHOUT_ACTIVATESTORYBOARD;

	setTag("grid");

	const float padding = 0.01f;

	mGrid.setGridSizeOverride(gridSize);
	mGrid.setPosition(mSize.x() * 0.1f, mSize.y() * 0.1f);
	mGrid.setDefaultZIndex(20);
	mGrid.setCursorChangedCallback([&](const CursorState& /*state*/) { updateInfoPanel(); });
	addChild(&mGrid);
	
	if (!themeName.empty())
		setThemeName(themeName);

	setTheme(theme);

	populateList(mRoot->getChildrenListToDisplay());
	updateInfoPanel();
}

void GridGameListView::onShow()
{
	ISimpleGameListView::onShow();
	updateInfoPanel();
}

void GridGameListView::setThemeName(std::string name)
{
	ISimpleGameListView::setThemeName(name);
	mGrid.setThemeName(getName());
}

FileData* GridGameListView::getCursor()
{
	if (mGrid.size() == 0)
		return nullptr;

	return mGrid.getSelected();
}

void GridGameListView::resetLastCursor()
{
	mGrid.resetLastCursor();
}

void GridGameListView::setCursor(FileData* cursor)
{
	if (cursor && !mGrid.setCursor(cursor) && !cursor->isPlaceHolder())
	{
		std::stack<FileData*> stack;
		auto childrenToDisplay = mRoot->findChildrenListToDisplayAtCursor(cursor, stack);
		if (childrenToDisplay != nullptr)
		{
			mCursorStack = stack;
			populateList(*childrenToDisplay.get());
			mGrid.setCursor(cursor);
		}
	}
}

std::string GridGameListView::getQuickSystemSelectRightButton()
{
	return "r2"; //rightshoulder
}

std::string GridGameListView::getQuickSystemSelectLeftButton()
{
	return "l2"; //leftshoulder
}

bool GridGameListView::input(InputConfig* config, Input input)
{
	/*
	if (!UIModeController::getInstance()->isUIModeKid() && config->isMappedTo("select", input) && input.value)
	{
		auto idx = mRoot->getSystem()->getIndex(false);
		if (idx != nullptr && idx->hasRelevency())
			return true;

		Sound::getFromTheme(mTheme, getName(), "menuOpen")->play();
		mWindow->pushGui(new GuiGamelistOptions(mWindow, this, this->mRoot->getSystem(), true));
		return true;

		// Ctrl-R to reload a view when debugging
	}
	*/
	if(config->isMappedLike("left", input) || config->isMappedLike("right", input))
		return GuiComponent::input(config, input);

	return ISimpleGameListView::input(config, input);
}

const std::string GridGameListView::getImagePath(FileData* file)
{
	ImageSource src = mGrid.getImageSource();

	if (src == ImageSource::IMAGE)
		return file->getImagePath();

	if (src == TITLESHOT && !file->getMetadata(MetaDataId::TitleShot).empty())
		return file->getMetadata(MetaDataId::TitleShot);
	else if (src == BOXART && !file->getMetadata(MetaDataId::BoxArt).empty())
		return file->getMetadata(MetaDataId::BoxArt);
	else if ((src == MARQUEE || src == ImageSource::MARQUEEORTEXT) && !file->getMarqueePath().empty())
		return file->getMarqueePath();
	else if ((src == IMAGE || src == TITLESHOT) && !file->getImagePath().empty())
		return file->getImagePath();
	else if (src == FANART && !file->getMetadata(MetaDataId::FanArt).empty())
		return file->getMetadata(MetaDataId::FanArt);
	else if (src == CARTRIDGE && !file->getMetadata(MetaDataId::Cartridge).empty())
		return file->getMetadata(MetaDataId::Cartridge);
	else if (src == MIX && !file->getMetadata(MetaDataId::Mix).empty())
		return file->getMetadata(MetaDataId::Mix);

	return file->getThumbnailPath();
}

const bool GridGameListView::isVirtualFolder(FileData* file)
{
	return file->getType() == FOLDER && ((FolderData*)file)->isVirtualFolderDisplay();
}

void GridGameListView::populateList(const std::vector<FileData*>& files)
{
	updateHeaderLogoAndText();

	mGrid.resetLastCursor();
	mGrid.clear(); 
	mGrid.resetLastCursor();

	if (files.size() > 0)
	{
		bool showParentFolder = mRoot->getSystem()->getShowParentFolder();

		if (mCursorStack.size())
		{
			auto top = mCursorStack.top();

			std::string imagePath;
			bool displayAsVirtualFolder = true;

			// Find logo image from original system
			if (mCursorStack.size() == 1 && top->getSystem()->isGroupChildSystem())
			{
				std::string startPath = top->getSystem()->getStartPath();
				
				auto parent = top->getSystem()->getParentGroupSystem();
				
				auto theme = parent->getTheme();
				if (theme)
				{
					const ThemeData::ThemeElement* logoElem = theme->getElement("system", "logo", "image");
					if (logoElem && logoElem->has("path"))
						imagePath = logoElem->get<std::string>("path");
				}

				if (imagePath.empty())
				{
					for (auto child : parent->getRootFolder()->getChildren())
					{
						if (child->getPath() == startPath)
						{
							if (child->getType() == FOLDER)
								displayAsVirtualFolder = ((FolderData*)child)->isVirtualFolderDisplayEnabled();

							imagePath = child->getMetadata(MetaDataId::Image);
							break;
						}
					}
				}				
			}

			if (showParentFolder)
			{
				FileData* placeholder = new FileData(PLACEHOLDER, "..", this->mRoot->getSystem());
				mGrid.add(". .", imagePath, "", "", false, true, false, displayAsVirtualFolder && !imagePath.empty(), placeholder);
			}
		}

		GameNameFormatter formatter(mRoot->getSystem());

		bool favoritesFirst = mRoot->getSystem()->getShowFavoritesFirst();
		if (favoritesFirst)
		{
			for (auto file : files)
			{
				if (!file->getFavorite())
					continue;

				mGrid.add(formatter.getDisplayName(file, file->getType() == FOLDER && Utils::FileSystem::exists(getImagePath(file))), getImagePath(file), file->getVideoPath(), file->getMarqueePath(), file->getFavorite(), file->hasCheevos(), file->getType() != GAME, isVirtualFolder(file), file);
			}
		}

		for (auto file : files)
		{
			if (file->getFavorite() && favoritesFirst)
				continue;

			mGrid.add(formatter.getDisplayName(file, file->getType() == FOLDER && Utils::FileSystem::exists(getImagePath(file))), getImagePath(file), file->getVideoPath(), file->getMarqueePath(), file->getFavorite(), file->hasCheevos(), file->getType() != GAME, isVirtualFolder(file), file);
		}

		// if we have the ".." PLACEHOLDER, then select the first game instead of the placeholder
		if (showParentFolder && mCursorStack.size() && mGrid.size() > 1 && mGrid.getCursorIndex() == 0)
			mGrid.setCursorIndex(1);
	}
	else
		addPlaceholder();

	updateFolderPath();

	if (mShowing)
		onShow();
}

void GridGameListView::onThemeChanged(const std::shared_ptr<ThemeData>& theme)
{
	ISimpleGameListView::onThemeChanged(theme);

	mGrid.applyTheme(theme, getName(), "gamegrid", ThemeFlags::ALL);
	mDetails.onThemeChanged(theme);
	updateInfoPanel();
}

void GridGameListView::updateInfoPanel()
{
	if (!mShowing)
		return;

	if (mRoot->getSystem()->isCollection())
		updateHelpPrompts();

	FileData* file = (mGrid.size() == 0 || mGrid.isScrolling()) ? NULL : mGrid.getSelected();
	bool isClearing = mGrid.getObjects().size() == 0 && mGrid.getCursorIndex() == 0 && mGrid.getScrollingVelocity() == 0;
	mDetails.updateControls(file, isClearing, mGrid.getCursorIndex() - mGrid.getLastCursor());
}

void GridGameListView::addPlaceholder()
{
	// empty grid - add a placeholder
	FileData* placeholder = new FileData(PLACEHOLDER, "<" + _("No Entries Found") + ">", mRoot->getSystem());
	mGrid.add(placeholder->getName(), "", "", "", false, false,false,false, placeholder);
}

void GridGameListView::launch(FileData* game)
{
	Vector3f target = mDetails.getLaunchTarget();
	if (target == Vector3f(Renderer::getScreenWidth() / 2.0f, Renderer::getScreenHeight() / 2.0f, 0))
	{
		auto tile = mGrid.getSelectedTile();
		if (tile != nullptr)
			target = mGrid.getPosition() + tile->getLaunchTarget();
	}

	ViewController::get()->launch(game, target);
}

void GridGameListView::remove(FileData *game)
{
	FolderData* parent = game->getParent();
	if (getCursor() == game)                     // Select next element in list, or prev if none
	{		
		std::vector<FileData*> siblings = mGrid.getObjects();

		int gamePos = getCursorIndex();
		if ((gamePos + 1) < (int)siblings.size())
			setCursor(siblings.at(gamePos + 1));
		else if ((gamePos - 1) > 0)
			setCursor(siblings.at(gamePos - 1));			
	}

	mGrid.remove(game);
	if(mGrid.size() == 0)
		addPlaceholder();

	mRoot->removeFromVirtualFolders(game);
	delete game;                                 // remove before repopulating (removes from parent)

	onFileChanged(parent, FILE_REMOVED);           // update the view, with game removed
}

void GridGameListView::onFileChanged(FileData* file, FileChangeType change)
{
	if (change == FILE_METADATA_CHANGED)
	{
		// might switch to a detailed view
		ViewController::get()->reloadGameListView(this);
		return;
	}

	ISimpleGameListView::onFileChanged(file, change);
}

void GridGameListView::setCursorIndex(int cursor)
{
	mGrid.setCursorIndex(cursor);
}

int GridGameListView::getCursorIndex()
{
	return mGrid.getCursorIndex();
}

std::vector<FileData*> GridGameListView::getFileDataEntries()
{
	return mGrid.getObjects();
}

void GridGameListView::update(int deltaTime)
{
	mDetails.update(deltaTime);
	ISimpleGameListView::update(deltaTime);
}

void GridGameListView::moveToRandomGame()
{
	auto list = getFileDataEntries();

	unsigned int total = (int)list.size();
	if (total == 0)
		return;

	int target = Randomizer::random(total);
	if (target >= 0 && target < total)
	{
		resetLastCursor();
		setCursor(list.at(target));
		if (isShowing())
			onShow();
	}
}
