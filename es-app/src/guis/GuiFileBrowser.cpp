#include "guis/GuiFileBrowser.h"

#include "ApiSystem.h"
#include "components/OptionListComponent.h"
#include "guis/GuiSettings.h"
#include "views/ViewController.h"
#include "components/ComponentGrid.h"
#include "SystemData.h"
#include "LocaleES.h"
#include "components/MultiLineMenuEntry.h"
#include "GuiLoading.h"
#include "guis/GuiMsgBox.h"
#include <cstring>
#include "SystemConf.h"

#if WIN32
#include "Win32ApiSystem.h"
#endif

#define WINDOW_WIDTH (float)Math::max((int)Renderer::getScreenHeight(), (int)(Renderer::getScreenWidth() * 0.65f))

#define DRIVE_ICON		_U("\uF0A0 ")
#define FOLDER_ICON		_U("\uF07C ")
#define IMAGE_ICON		_U("\uF03E ")
#define VIDEO_ICON		_U("\uF03D ")
#define DOCUMENT_ICON	_U("\uF02D ")

GuiFileBrowser::GuiFileBrowser(Window* window, const std::string startPath, const std::string selectedFile, FileTypes types, const std::function<void(const std::string&)>& okCallback, const std::string& title)
	: GuiComponent(window), mMenu(window, title.empty() ? _("FILE BROWSER") : title)
{
	setTag("popup");

	mTypes = types;
	mSelectedFile = Utils::FileSystem::getCanonicalPath(selectedFile);
	mOkCallback = okCallback;

	addChild(&mMenu);

	if (mOkCallback != nullptr)
	{
		mMenu.addButton(_("RESET"), "back", [&]
		{
			mOkCallback("");
			delete this;
		});
	}

    mMenu.addButton(_("BACK"), "back", [&] { delete this; });

	if (startPath.empty() || !Utils::FileSystem::exists(startPath))
	{
#if WIN32
		navigateTo(Win32ApiSystem::getEmulatorLauncherPath("screenshots"));
#else
		navigateTo("/userdata/screenshots");
#endif
	}
	else
		navigateTo(startPath);
}

void GuiFileBrowser::navigateTo(const std::string path)
{
	mCurrentPath = path;

	auto theme = ThemeData::getMenuTheme();

	mMenu.clear();
	mMenu.setSubTitle(mCurrentPath);

	auto files = Utils::FileSystem::getDirectoryFiles(mCurrentPath);

	if (mCurrentPath != "\\" && mCurrentPath != "/" && !mCurrentPath.empty())
	{
		mMenu.addEntry(FOLDER_ICON + std::string(".."), false, [this]()
		{
			navigateTo(Utils::FileSystem::getParent(mCurrentPath));
		});
	}

	for (auto file : files)
	{
		if (!file.directory || file.hidden)
			continue;

		std::string icon = FOLDER_ICON;
		if (Utils::String::endsWith(file.path, ":"))
			icon = DRIVE_ICON;

		bool isSelected = false;

		if (mTypes == FileTypes::DIRECTORY)
			isSelected = (mSelectedFile == file.path);

		mMenu.addEntry(icon + Utils::FileSystem::getFileName(file.path), false, [this, file]()
		{
			navigateTo(Utils::FileSystem::combine(mCurrentPath, Utils::FileSystem::getFileName(file.path)));
		}, "", isSelected, true, false, file.path, false);
	}

	if (mTypes != FileTypes::DIRECTORY)
	{
		for (auto file : files)
		{
			if (file.directory || file.hidden)
				continue;

			std::string ext = Utils::FileSystem::getExtension(file.path);

			std::string icon;

			if ((mTypes & FileTypes::IMAGES) == FileTypes::IMAGES)
				if (ext == ".jpg" || ext == ".png" || ext == ".gif" || ext == ".svg")
					icon = IMAGE_ICON;

			if ((mTypes & FileTypes::MANUALS) == FileTypes::MANUALS)
				if (ext == ".pdf" || ext == ".cbz")
					icon = DOCUMENT_ICON;

			if ((mTypes & FileTypes::VIDEO) == FileTypes::VIDEO)
				if (ext == ".mp4")
					icon = VIDEO_ICON;

			if (icon.empty())
				continue;

			bool isSelected = (mSelectedFile == file.path);

			mMenu.addEntry(icon + Utils::FileSystem::getFileName(file.path), false, [this, file]()
			{
				if (mOkCallback)
					mOkCallback(file.path);

				delete this;
			}, "", isSelected, true, false, file.path, false);
		}
	}

	centerWindow();	
}

void GuiFileBrowser::centerWindow()
{
	if (Renderer::isSmallScreen())
		mMenu.setSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());
	else
	{
		mMenu.setSize(mMenu.getSize().x(), Renderer::getScreenHeight() * 0.875f);
		mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, (Renderer::getScreenHeight() - mMenu.getSize().y()) / 2);
	}
}

bool GuiFileBrowser::input(InputConfig* config, Input input)
{
	if (GuiComponent::input(config, input))
		return true;

	if (config->isMappedTo("y", input) && input.value)
	{
		if (mCurrentPath != "\\" && mCurrentPath != "/" && !mCurrentPath.empty())
			navigateTo(Utils::FileSystem::getParent(mCurrentPath));

		return true;
	}

	if (input.value != 0 && (config->isMappedLike("left", input) || config->isMappedLike("right", input)))
	{
		mMenu.setCursorToButtons();
		return true;
	}

	if (input.value != 0 && config->isMappedTo(BUTTON_BACK, input))
	{
		delete this;
		return true;
	}

	if (config->isMappedTo("start", input) && input.value != 0)
	{		
		if (mMenu.size() && mOkCallback != nullptr)
		{
			auto path = mMenu.getSelected();

			if (mTypes == FileTypes::DIRECTORY && path.empty())
			{
				mOkCallback(mCurrentPath);
				delete this;
			}
			else if (!path.empty() && (mTypes == FileTypes::DIRECTORY || !Utils::FileSystem::isDirectory(path)))
			{
				mOkCallback(path);
				delete this;
			}
		}

		return true;		
	}

	if (config->isMappedTo("x", input) && input.value && mOkCallback != nullptr)
	{
		mOkCallback("");
		delete this;
		return true;
	}
	
	if (config->isMappedTo("select", input))
	{

#if WIN32
		navigateTo(Win32ApiSystem::getEmulatorLauncherPath("screenshots"));
#else
		navigateTo("/userdata/screenshots");
#endif
		return true;
	}

	return false;
}

std::vector<HelpPrompt> GuiFileBrowser::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = mMenu.getHelpPrompts();
	
	if (mOkCallback != nullptr)
		prompts.push_back(HelpPrompt("x", _("RESET")));

	prompts.push_back(HelpPrompt(BUTTON_BACK, _("CLOSE")));
	prompts.push_back(HelpPrompt("y", _("PARENT FOLDER")));
	prompts.push_back(HelpPrompt("select", _("SCREENSHOTS FOLDER")));
	prompts.push_back(HelpPrompt("start", _("SELECT")));

	return prompts;
}
