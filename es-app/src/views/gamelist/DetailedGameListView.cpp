#include "views/gamelist/DetailedGameListView.h"

#include "animations/LambdaAnimation.h"
#include "views/ViewController.h"
#include "FileData.h"
#include "SystemData.h"
#include "LocaleES.h"

DetailedGameListView::DetailedGameListView(Window* window, FolderData* root) : 
	BasicGameListView(window, root), 
	mDetails(this, &mList, mWindow, DetailedContainer::DetailedView)
{
	const float padding = 0.01f;

	mList.setPosition(mSize.x() * (0.50f + padding), mList.getPosition().y());
	mList.setSize(mSize.x() * (0.50f - padding), mList.getSize().y());
	mList.setAlignment(TextListComponent<FileData*>::ALIGN_LEFT);
	mList.setCursorChangedCallback([&](const CursorState& /*state*/) { updateInfoPanel(); });

	updateInfoPanel();
}

void DetailedGameListView::onThemeChanged(const std::shared_ptr<ThemeData>& theme)
{
	BasicGameListView::onThemeChanged(theme);

	mDetails.onThemeChanged(theme);	
	updateInfoPanel();
}

void DetailedGameListView::updateInfoPanel()
{
	if (!mShowing)
		return;

	if (mRoot->getSystem()->isCollection())
		updateHelpPrompts();

	FileData* file = (mList.size() == 0 || mList.isScrolling()) ? NULL : mList.getSelected();	
	bool isClearing = mList.getObjects().size() == 0 && mList.getCursorIndex() == 0 && mList.getScrollingVelocity() == 0;
	mDetails.updateControls(file, isClearing);
}

void DetailedGameListView::launch(FileData* game)
{
	Vector3f target = mDetails.getLaunchTarget();
	
	ViewController::get()->launch(game, target);
}

void DetailedGameListView::onShow()
{
	BasicGameListView::onShow();
	updateInfoPanel();
}
