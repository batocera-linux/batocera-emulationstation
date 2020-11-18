#include "GuiImageViewer.h"
#include "ThemeData.h"

#include "ApiSystem.h"
#include "GuiLoading.h"
#include "ImageIO.h"

#ifdef WIN32
#include <Windows.h>
#include <direct.h>
#else
#include <unistd.h>
#endif

GuiImageViewer::GuiImageViewer(Window* window, bool linearSmooth) :
	GuiComponent(window), mGrid(window)
{
	setPosition(0, 0);
	setSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());
		
	mGrid.setPosition(0, 0);
	mGrid.setSize(mSize);
	mGrid.setDefaultZIndex(20);
	addChild(&mGrid);

	std::string xml =
		"<theme defaultView=\"Tiles\">"
		"<formatVersion>7</formatVersion>"
		"<view name = \"grid\">"
		"<imagegrid name=\"gamegrid\">"
		"  <margin>0 0</margin>"
		"  <padding>0 0</padding>"
		"  <pos>0 0</pos>"
		"  <size>1 1</size>"
		"  <scrollDirection>horizontal</scrollDirection>"		
		"  <autoLayout>1 1</autoLayout>"
		"  <autoLayoutSelectedZoom>1</autoLayoutSelectedZoom>"
		"  <animateSelection>false</animateSelection>"
		"  <centerSelection>true</centerSelection>"
		"  <scrollLoop>true</scrollLoop>"
		"  <showVideoAtDelay>10</showVideoAtDelay>"
		"</imagegrid>"
		"<gridtile name=\"default\">"
		"  <backgroundColor>FFFFFF00</backgroundColor>"		
		"  <selectionMode>image</selectionMode>"
		"  <padding>4 4</padding>"
		"  <imageColor>FFFFFFFF</imageColor>"
		"</gridtile>"
		"<gridtile name=\"selected\">"
		"  <backgroundColor>FFFFFF00</backgroundColor>"
		"</gridtile>"
		"<image name=\"gridtile.image\">"
		"  <linearSmooth>true</linearSmooth>"
		"</image>"
		"</view>"
		"</theme>";

	if (!linearSmooth)
		xml = Utils::String::replace(xml, "<linearSmooth>true</linearSmooth>", "<linearSmooth>false</linearSmooth>");

	mTheme = std::shared_ptr<ThemeData>(new ThemeData());
	std::map<std::string, std::string> emptyMap;
	mTheme->loadFile("imageviewer", emptyMap, xml, false);

	mGrid.applyTheme(mTheme, "grid", "gamegrid", 0);

	animateTo(Vector2f(0, Renderer::getScreenHeight()), Vector2f(0, 0));
}

GuiImageViewer::~GuiImageViewer()
{
	auto pdfFolder = Utils::FileSystem::getGenericPath(Utils::FileSystem::getEsConfigPath() + "/pdftmp/");
	Utils::FileSystem::deleteDirectoryFiles(pdfFolder);
	rmdir(pdfFolder.c_str());
}

bool GuiImageViewer::input(InputConfig* config, Input input)
{
	if (input.value != 0 && (config->isMappedTo(BUTTON_BACK, input) || config->isMappedTo(BUTTON_OK, input)))
	{		
		delete this;
		return true;
	}

	return GuiComponent::input(config, input);
}

std::vector<HelpPrompt> GuiImageViewer::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("CLOSE")));
	return prompts;
}

void GuiImageViewer::add(const std::string imagePath)
{
	if (!Utils::FileSystem::exists(imagePath))
		return;

	std::string img;
	std::string vid;

	auto ext = Utils::FileSystem::getExtension(imagePath);
	if (ext == ".mp4")
		vid = imagePath;
	else
		img = imagePath;
	
	mGrid.add("", img, vid, "", false, false, false, imagePath);
}

void GuiImageViewer::setCursor(const std::string imagePath)
{
	mGrid.resetLastCursor();
	mGrid.setCursor(imagePath);
	mGrid.onShow();
}

void GuiImageViewer::showImage(Window* window, const std::string imagePath)
{
	if (!Utils::FileSystem::exists(imagePath))
		return;

	if (Utils::String::toLower(Utils::FileSystem::getExtension(imagePath)) == ".pdf") 
	{
		showPdf(window, imagePath);
		return;
	}

	auto imgViewer = new GuiImageViewer(window, false);
	imgViewer->add(imagePath);
	imgViewer->setCursor(imagePath);
	window->pushGui(imgViewer);
}

void GuiImageViewer::showPdf(Window* window, const std::string imagePath)
{
	window->pushGui(new GuiLoading<std::vector<std::string>>(window, _("Loading..."),
		[window, imagePath]
		{
			return ApiSystem::getInstance()->extractPdfImages(imagePath);
		},
		[window](std::vector<std::string> fileList)
		{			
			if (fileList.size() == 0)
				return;

			auto imgViewer = new GuiImageViewer(window, true);

			for (auto file : fileList)
			{
				ImageIO::removeImageCache(file);
				imgViewer->add(file);
			}

			imgViewer->setCursor(fileList[0]);
			window->pushGui(imgViewer);
		}));
}


#ifdef _RPI_
#include "Settings.h"
#include "components/VideoPlayerComponent.h"
#endif
#include "components/VideoVlcComponent.h"

void GuiVideoViewer::playVideo(Window* window, const std::string videoPath)
{
	if (!Utils::FileSystem::exists(videoPath))
		return;

	window->pushGui(new GuiVideoViewer(window, videoPath));
}

GuiVideoViewer::GuiVideoViewer(Window* window, const std::string& path) : GuiComponent(window)
{
	setPosition(0, 0);
	setSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());

#ifdef _RPI_
	if (Settings::getInstance()->getBool("VideoOmxPlayer"))
		mVideo = new VideoPlayerComponent(mWindow, "");
	else
#endif
	{
		mVideo = new VideoVlcComponent(mWindow, "");

		((VideoVlcComponent*)mVideo)->setLinearSmooth();
		((VideoVlcComponent*)mVideo)->setEffect(VideoVlcFlags::NONE);
	}
	
	mVideo->setOrigin(0.5f, 0.5f);
	mVideo->setPosition(Renderer::getScreenWidth() / 2.0f, Renderer::getScreenHeight() / 2.0f);
	mVideo->setMaxSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());

	mVideo->setOnVideoEnded([&]()
	{		
		mWindow->postToUiThread([&](Window* w) { delete this; });
		return false;
	});

	addChild(mVideo);
		
	mVideo->setStartDelay(25);
	mVideo->setVideo(path);
}

GuiVideoViewer::~GuiVideoViewer()
{
	delete mVideo;
}

bool GuiVideoViewer::input(InputConfig* config, Input input)
{
	if (input.value != 0 && (config->isMappedTo(BUTTON_BACK, input) || config->isMappedTo(BUTTON_OK, input)))
	{
		delete this;
		return true;
	}

	return GuiComponent::input(config, input);
}
