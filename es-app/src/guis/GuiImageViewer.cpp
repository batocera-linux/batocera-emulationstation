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

#include "utils/ZipFile.h"
#include "components/HelpComponent.h"

class ZoomableImageComponent : public ImageComponent
{
public:
	ZoomableImageComponent(Window* window, const std::string& image) : ImageComponent(window, true, false)
	{
		mLocked = false;
		mFirstShow = true;
		mZooming = 0;
		mMoving = Vector2f::Zero();

		mCheckClipping = false;

		setIsLinear(true);
		setImage(image, false, MaxSizeInfo(8192, 8192));

		setPosition(Renderer::getScreenWidth() * 0.5, Renderer::getScreenHeight() * 0.5);
		setOrigin(0.5, 0.5);
		setMaxSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());	
	}

	void runAnimation()
	{
		auto sz = getSize();
		if (sz.x() > 0 && sz.y() > 0)
		{
			auto fromOrigin = getOrigin();
			auto fromScale = getScale();
			auto toScale = Renderer::getScreenWidth() / sz.x();
			auto toOrigin = fromOrigin;

			auto imgHeight = sz.y() * toScale;
			if (imgHeight > Renderer::getScreenHeight())
				toOrigin = Vector2f(0.5, Renderer::getScreenHeight() / imgHeight / 2.0);
			else
				toScale = Renderer::getScreenHeight() / sz.y();

			Animation* infoFadeIn = new LambdaAnimation([this, fromScale, toScale, fromOrigin, toOrigin](float t)
			{
				auto cubic = Math::easeOutCubic(t);
				auto scale = fromScale * (1.0f - cubic) + toScale * cubic;

				auto originX = fromOrigin.x() * (1.0f - cubic) + toOrigin.x() * cubic;
				auto originY = fromOrigin.y() * (1.0f - cubic) + toOrigin.y() * cubic;

				setScale(scale);
				setOrigin(originX, originY);
			}, 350);

			setAnimation(infoFadeIn);
		}
	}

	std::vector<HelpPrompt> getHelpPrompts()
	{
		std::vector<HelpPrompt> prompts;
		prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK"), [&] { delete this; }));
		prompts.push_back(HelpPrompt("l", _("ZOOM OUT"), [&] { onMouseWheel(-1); }));
		prompts.push_back(HelpPrompt("r", _("ZOOM IN"), [&] { onMouseWheel(1); }));
		prompts.push_back(HelpPrompt("up/down/left/right", _("MOVE")));

		return prompts;
	}

	void update(int deltaTime)
	{
		ImageComponent::update(deltaTime);

		if (mFirstShow)
		{
			mFirstShow = false;
			runAnimation();
		}

		if (mZooming != 0)
		{
			float zoomSpeed = deltaTime * 0.0015f;

			auto scale = getScale();
			scale = scale + mZooming * zoomSpeed;
			if (scale < 0.01)
				scale = 0.01;

			setScale(scale);
		}

		if (mMoving.x() != 0 || mMoving.y() != 0)
		{
			float moveSpeed = deltaTime * 0.0003f;

			auto org = getOrigin();
			org.x() = org.x() + mMoving.x() * moveSpeed;
			org.y() = org.y() + mMoving.y() * moveSpeed;
			setOrigin(org);
		}
	}

	bool input(InputConfig* config, Input input)
	{
		if (input.value != 0)
		{
			if (!mLocked && (config->isMappedTo(BUTTON_BACK, input) || config->isMappedTo(BUTTON_OK, input)))
			{
				delete this;
				return true;
			}

			if (config->isMappedLike("pageup", input))
			{				
				mZooming = -1;
				return true;
			}
			
			if (config->isMappedLike("pagedown", input))
			{
				mZooming = 1;
				return true;
			}
						
			if (config->isMappedLike("down", input))
			{
				mMoving.y() = 1;
				return true;
			}
			
			if (config->isMappedLike("up", input))
			{
				mMoving.y() = -1;
				return true;
			}
			
			if (config->isMappedLike("left", input))
			{
				mMoving.x() = -1;
				return true;
			}
			
			if (config->isMappedLike("right", input))
			{
				mMoving.x() = 1;
				return true;
			}
		}	
		else
		{
			if (input.type == InputType::TYPE_HAT)
			{
				mMoving = Vector2f::Zero();
				return true;
			}

			if (config->isMappedLike("left", input) || config->isMappedLike("right", input))
			{
				mMoving.x() = 0;
				return true;
			}
			
			if (config->isMappedLike("down", input) || config->isMappedLike("up", input))
			{
				mMoving.y() = 0;
				return true;
			}
			
			if (config->isMappedLike("pagedown", input) || config->isMappedLike("pageup", input))
			{
				mZooming = 0;
				return true;
			}
		}
		
		return ImageComponent::input(config, input);
	}

	void lock(bool state)
	{
		mLocked = state;
	}

	bool hitTest(int x, int y, Transform4x4f& parentTransform, std::vector<GuiComponent*>* pResult)
	{
		if (pResult != nullptr)
		{
			for (auto cp : *pResult)
				if (cp->isKindOf<HelpComponent>())
					return false;

			pResult->push_back(this);
		}

		return true;
	}

	bool onMouseWheel(int delta)
	{
		auto scale = getScale();
		float zoomSpeed = 0.1f;

		scale = scale + delta * zoomSpeed;
		if (scale < 0.01)
			scale = 0.01;

		setScale(scale);
		return true;
	}

	void onMouseMove(int x, int y)
	{
		if (mPressedPoint.x() != -1 && mPressedPoint.y() != -1 && mWindow->hasMouseCapture(this))
		{
			auto org = getOrigin();

			auto scale = getScale();
			auto size = getSize();

			if (size.x() != 0 && size.y() != 0)
			{
				float dx = (mPressedPoint.x() - x) / (float)(size.x() * scale);
				float dy = (mPressedPoint.y() - y) / (float)(size.y() * scale);

				org.x() = org.x() + dx;
				org.y() = org.y() + dy;
				setOrigin(org);
			}

			mPressedPoint = Vector2i(x, y);
		}
	}

	bool onMouseClick(int button, bool pressed, int x, int y)
	{
		if (button == 1)
		{
			if (pressed)
			{
				mPressedPoint = Vector2i(x, y);
				mWindow->setMouseCapture(this);
			}
			else if (mWindow->hasMouseCapture(this))
				mWindow->releaseMouseCapture();

			return true;
		}

		return false;
	}

private:
	Vector2i mPressedPoint;
	Vector2f mMoving;
	float	 mZooming;
	bool	 mFirstShow;
	bool	 mLocked;
};

static bool g_isGuiImageViewerRunning = false;

GuiImageViewer::GuiImageViewer(Window* window, bool linearSmooth) :
	GuiComponent(window), mGrid(window), mPdfThreads(nullptr)
{
	g_isGuiImageViewerRunning = true;

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

void GuiImageViewer::loadPdf(const std::string& imagePath)
{
	Window* window = mWindow;

	int pages = ApiSystem::getInstance()->getPdfPageCount(imagePath);
	if (pages == 0)
	{
		delete this;
		return;
	}

#define INITIALPAGES	1
#define PAGESPERTHREAD  1

	mPdf = imagePath;
	
	for (int i = 0; i < pages; i++)
		mGrid.add("", ":/blank.png", std::to_string(i + 1));
	
	if (pages > INITIALPAGES)
	{
		mPdfThreads = new Utils::ThreadPool(1);

		for (int i = INITIALPAGES; i < pages; i += PAGESPERTHREAD)
		{
			mPdfThreads->queueWorkItem([this, imagePath, window, i]
			{
				auto fl = ApiSystem::getInstance()->extractPdfImages(imagePath, i + 1, PAGESPERTHREAD);
				if (fl.size() == 0 || !g_isGuiImageViewerRunning)
					return;

				window->postToUiThread([this, i, fl]()
				{
					if (!g_isGuiImageViewerRunning)
						return;

					for (int f = 0; f < fl.size(); f++)
					{
						auto img = fl[f];
						ImageIO::removeImageCache(img);
						mGrid.setImage(img, std::to_string(i + 1 + f));
					}
				});
			});
		}

		mPdfThreads->start();
	}
	
	window->pushGui(new GuiLoading<std::vector<std::string>>(window, _("Loading..."),
		[window, imagePath](auto gui)
		{		
			return ApiSystem::getInstance()->extractPdfImages(imagePath, 1, INITIALPAGES);
		},
			[this, window, imagePath, pages](std::vector<std::string> fileList)
		{
			if (fileList.size() == 0)
				return;
		
			for (int i = 0; i < fileList.size(); i++)
			{
				ImageIO::removeImageCache(fileList[i]);
				mGrid.setImage(fileList[i], std::to_string(i + 1));
			}

			window->pushGui(this);
		}));
}

static std::string _extractZipFile(const std::string& zipFileName, const std::string& fileToExtract)
{
	auto pdfFolder = Utils::FileSystem::getPdfTempPath();

	Utils::Zip::ZipFile zipFile;
	if (zipFile.load(zipFileName))
	{
		std::string fullPath = Utils::FileSystem::combine(pdfFolder, fileToExtract);
		std::string folder = Utils::FileSystem::getParent(fullPath);
		if (folder != pdfFolder)
			Utils::FileSystem::createDirectory(folder);

		if (zipFile.extract(fileToExtract, fullPath, true))
			if (Utils::FileSystem::exists(fullPath))
				return fullPath;
	}

	return "";
}

void GuiImageViewer::loadImages(std::vector<std::string>& images)
{
	if (images.size() == 0)
	{
		delete this;
		return;
	}

	Window* window = mWindow;

	mWindow->postToUiThread([images, window, this]
	{
		for (int i = 0; i < images.size(); i++)
			mGrid.add("", images[i], std::to_string(i + 1));

		window->pushGui(this);
	});
}

void GuiImageViewer::loadCbz(const std::string& imagePath)
{
	auto pdfFolder = Utils::FileSystem::getPdfTempPath();
	Utils::FileSystem::createDirectory(pdfFolder);
	Utils::FileSystem::deleteDirectoryFiles(pdfFolder);

	Window* window = mWindow;

	std::vector<std::string> files;
	std::vector<std::wstring> filesW;
	

	try
	{
		Utils::Zip::ZipFile zipFile;
		if (zipFile.load(imagePath))
		{
			for (auto file : zipFile.namelist())
			{
				auto ext = Utils::String::toLower(Utils::FileSystem::getExtension(file));
				if (ext != ".jpg")
					continue;

				if (Utils::String::startsWith(file, "__"))
					continue;

				files.push_back(file);
			}

			std::sort(files.begin(), files.end(), [](std::string a, std::string b) { return Utils::String::toLower(a) < Utils::String::toLower(b); });
		}
	}
	catch (...)
	{
		// Bad zip file
		delete this;
		return;
	}

	int pages = files.size();
	if (pages == 0)
	{
		delete this;
		return;
	}

#define INITIALPAGES	1

	mPdf = imagePath;

	for (int i = 0; i < pages; i++)
		mGrid.add("", ":/blank.png", std::to_string(i + 1));

	if (pages > INITIALPAGES)
	{
		mPdfThreads = new Utils::ThreadPool(1);

		for (int i = INITIALPAGES; i < pages; i += PAGESPERTHREAD)
		{
			auto fileToExtract = files[i];
			mPdfThreads->queueWorkItem([this, imagePath, fileToExtract, window, i]
			{
				auto localFile = _extractZipFile(imagePath, fileToExtract);
				if (localFile.empty() || !g_isGuiImageViewerRunning)
					return;

				window->postToUiThread([this, i, localFile]()
				{
					if (!g_isGuiImageViewerRunning)
						return;

					ImageIO::removeImageCache(localFile);
					mGrid.setImage(localFile, std::to_string(i + 1));
				});
			});
		}

		mPdfThreads->start();
	}

	window->pushGui(new GuiLoading<std::vector<std::string>>(window, _("Loading..."),
		[window, imagePath, files](auto gui)
		{
			std::vector<std::string> ret;

			for (size_t i = 0; i < INITIALPAGES && i < files.size(); i++)
			{
				auto fileToExtract = files[i];

				auto localFile = _extractZipFile(imagePath, fileToExtract);
				if (!localFile.empty())
					ret.push_back(localFile);
			}

			return ret;
		},
		[this, window, imagePath, pages](std::vector<std::string> fileList)
		{
			if (fileList.size() == 0)
				return;

			for (size_t i = 0; i < fileList.size(); i++)
			{
				ImageIO::removeImageCache(fileList[i]);
				mGrid.setImage(fileList[i], std::to_string(i + 1));
			}

			window->pushGui(this);
		}
	));
}


GuiImageViewer::~GuiImageViewer()
{
	g_isGuiImageViewerRunning = false;

	if (mPdfThreads != nullptr)
	{
		mPdfThreads->stop();
		delete mPdfThreads;
	}

	auto pdfFolder = Utils::FileSystem::getPdfTempPath();
	Utils::FileSystem::deleteDirectoryFiles(pdfFolder, true);
}

bool GuiImageViewer::input(InputConfig* config, Input input)
{
	if (input.value != 0 && config->isMappedTo(BUTTON_OK, input))
	{		
		std::string path = mGrid.getSelected();
		if (!path.empty())
		{			
			if (!mPdf.empty())
			{
				if (Utils::String::toLower(Utils::FileSystem::getExtension(mPdf)) != ".pdf")
				{
					path = mGrid.getImage(path);
					mWindow->pushGui(new ZoomableImageComponent(mWindow, path));
				}
				else
				{
					// path = mGrid.getImage(path);
					int page = mGrid.getCursorIndex() + 1;

					Window* window = mWindow;
					window->pushGui(new GuiLoading<std::string>(window, _("Loading..."),
						[this, window, path, page](auto gui)
						{
							auto files = ApiSystem::getInstance()->extractPdfImages(mPdf, page, 1, 300);
							if (files.size() == 1)
								return files[0];

							return path;
						},
						[window](std::string file)
						{
							window->pushGui(new ZoomableImageComponent(window, file));
						})
					);
				}
			}
			else
				mWindow->pushGui(new ZoomableImageComponent(mWindow, path));
		}

		return true;
	}

	if (input.value != 0 && config->isMappedTo(BUTTON_BACK, input))
	{		
		delete this;
		return true;
	}

	return GuiComponent::input(config, input);
}

std::vector<HelpPrompt> GuiImageViewer::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("CLOSE"), [&] { delete this; }));
	
	if (!mPdf.empty())
		prompts.push_back(HelpPrompt(BUTTON_OK, _("ZOOM")));

	return prompts;
}

void GuiImageViewer::add(const std::string imagePath)
{
	if (!Utils::FileSystem::exists(imagePath))
		return;
	/*
	std::string img;
	std::string vid;

	if (Utils::FileSystem::isAudio(imagePath))
	{
		vid = imagePath;
		img = imagePath;

		if (ResourceManager::getInstance()->fileExists(":/mp3.jpg"))
			img = ":/mp3.jpg";
	}
	else if (Utils::FileSystem::isVideo(imagePath))
		vid = imagePath;
	else
		img = imagePath;
	*/
	mGrid.add("", imagePath, imagePath); // vid, "", false, false, false, false, 
}

void GuiImageViewer::setCursor(const std::string imagePath)
{
	mGrid.resetLastCursor();
	mGrid.setCursor(imagePath);
	mGrid.onShow();
}

void GuiImageViewer::showImage(Window* window, const std::string imagePath, bool zoomSingleFile)
{
	if (!Utils::FileSystem::exists(imagePath))
		return;

	if (Utils::String::toLower(Utils::FileSystem::getExtension(imagePath)) == ".cbz")
	{
		showCbz(window, imagePath);
		return;
	}

	if (Utils::String::toLower(Utils::FileSystem::getExtension(imagePath)) == ".pdf") 
	{
		showPdf(window, imagePath);
		return;
	}

	if (zoomSingleFile)
	{
		window->pushGui(new ZoomableImageComponent(window, imagePath));
		return;
	}

	auto imgViewer = new GuiImageViewer(window, false);
	imgViewer->add(imagePath);
	imgViewer->setCursor(imagePath);
	window->pushGui(imgViewer);
}

void GuiImageViewer::showPdf(Window* window, const std::string imagePath)
{
	auto imgViewer = new GuiImageViewer(window, true);	

	std::string ext = Utils::FileSystem::getExtension(imagePath);
	if (ext == ".cbz")
		imgViewer->loadCbz(imagePath);
	else
		imgViewer->loadPdf(imagePath);	
}

void GuiImageViewer::showImages(Window* window, std::vector<std::string>& images)
{
	auto imgViewer = new GuiImageViewer(window, false);
	imgViewer->loadImages(images);
}

void GuiImageViewer::showCbz(Window* window, const std::string imagePath)
{
	auto imgViewer = new GuiImageViewer(window, true);
	imgViewer->loadCbz(imagePath);
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
		mVideo = new VideoVlcComponent(mWindow);

		((VideoVlcComponent*)mVideo)->setLinearSmooth();
		((VideoVlcComponent*)mVideo)->setEffect(VideoVlcFlags::NONE);
	}
	
	mVideo->setOrigin(0.5f, 0.5f);
	mVideo->setPosition(Renderer::getScreenWidth() / 2.0f, Renderer::getScreenHeight() / 2.0f);
	mVideo->setMaxSize(Renderer::getScreenWidth(), Renderer::getScreenHeight());

	mVideo->setOnVideoEnded([&]()
	{		
		mWindow->postToUiThread([&]() { delete this; });
		return false;
	});

	addChild(mVideo);
		
	mVideo->setStartDelay(25);
	mVideo->setVideo(path);

	topWindow(true);
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
