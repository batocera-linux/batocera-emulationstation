#include "views/gamelist/VideoGameListView.h"

#include "animations/LambdaAnimation.h"
#include "utils/FileSystemUtil.h"
#include "FileData.h"
#include "SystemData.h"
#include "views/ViewController.h"
#include "LangParser.h"

VideoGameListView::VideoGameListView(Window* window, FolderData* root) :
	BasicGameListView(window, root),
	mDetails(this, &mList, mWindow, DetailedContainer::VideoView)
{
	const float padding = 0.01f;

	mList.setPosition(mSize.x() * (0.50f + padding), mList.getPosition().y());
	mList.setSize(mSize.x() * (0.50f - padding), mList.getSize().y());
	mList.setAlignment(TextListComponent<FileData*>::ALIGN_LEFT);
	mList.setCursorChangedCallback([&](const CursorState& /*state*/) { updateInfoPanel(); });	
}

void VideoGameListView::onThemeChanged(const std::shared_ptr<ThemeData>& theme)
{
	BasicGameListView::onThemeChanged(theme);
	
	mDetails.onThemeChanged(theme);
	updateInfoPanel();
}

void VideoGameListView::updateInfoPanel()
{
	if (!mShowing)
		return;

	if (mRoot->getSystem()->isCollection())
		updateHelpPrompts();

	FileData* file = (mList.size() == 0 || mList.isScrolling()) ? NULL : mList.getSelected();
	bool isClearing = mList.getObjects().size() == 0 && mList.getCursorIndex() == 0 && mList.getScrollingVelocity() == 0;
	mDetails.updateControls(file, isClearing);	
}

void VideoGameListView::launch(FileData* game)
{
	float screenWidth = (float) Renderer::getScreenWidth();
	float screenHeight = (float) Renderer::getScreenHeight();

	Vector3f target = mDetails.getLaunchTarget();
	ViewController::get()->launch(game, target);
}

void VideoGameListView::update(int deltaTime)
{
	BasicGameListView::update(deltaTime);
	mDetails.update(deltaTime);
}

void VideoGameListView::onShow()
{
	GuiComponent::onShow();
	updateInfoPanel();
}
