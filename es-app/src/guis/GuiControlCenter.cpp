#include "guis/GuiControlCenter.h"

#include "controlcenter/CcShell.h"
#include "controlcenter/GuiTextViewer.h"
#include "guis/GuiImageViewer.h"
#include "components/ImageComponent.h"
#include "components/WebImageComponent.h"
#include "guis/GuiMsgBox.h"
#include "resources/Font.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "InputConfig.h"
#include "LocaleES.h"
#include "Log.h"
#include "ThemeData.h"
#include "Window.h"

#include <pugixml/src/pugixml.hpp>
#include <qrcodegen/qrcodegen.hpp>
#include <cstdio>
#include <deque>
#include <fstream>
#include <algorithm>
#include <cmath>

// Math::clamp only applies the lower bound
static float clampf(float v, float lo, float hi)
{
	return v < lo ? lo : v > hi ? hi : v;
}



struct GuiControlCenter::Binding
{
	std::string tmpl;
	int refreshMs = 0;
	int ttlMs = 1000;
	int tab = -1;

	std::string value;
	bool resolved = false;
	bool inflight = false;
	int elapsed = 0;

	// results are ignored while > 0, an optimistic value is showing
	int holdMs = 0;

	bool stale = false;

	std::string idRef;
	bool negate = false;

	std::function<void(const std::string&)> onChanged;

	std::function<void(const std::string&)> onResult;
};

enum class CcKind { Slider, Tile, Cycler, Info, Image, Qr, Graph, Chip };

struct GuiControlCenter::Item
{
	CcKind kind;
	std::string label;
	std::string subLabel;

	std::shared_ptr<Binding> value;
	std::shared_ptr<Binding> labelValue;

	// enabled="${...}": empty shows the control disabled
	std::shared_ptr<Binding> enabled;
	std::string disabledLabel;
	std::vector<std::shared_ptr<Binding>> conds;

	std::string action, actionOn, actionOff, actionDec, actionInc, actionSet;
	std::string afterclick;
	std::vector<std::string> updateIds;
	bool confirm = false;
	bool toggle = false;
	bool progress = false;

	float min = 0, max = 100, step = 5;
	float target = -1;

	struct Choice
	{
		std::string label, action, afterclick;
		std::vector<std::string> updateIds;
	};
	std::vector<Choice> choices;
	std::string cmdLabels, cmdArgs, cmdAction, cmdAfterclick;
	std::vector<std::string> cmdUpdateIds;
	int cacheMs = 5000;
	int pendingIndex = -1;
	int applyTimer = 0;

	float imageHeight = 0;
	std::shared_ptr<WebImageComponent> image;
	std::string buttonText;

	Renderer::Rect rect;
	int row = -1;
	bool asRow = false;

	int running = 0;

	bool doc = false;
	bool loading = false;

	float qrSize = 0;
	unsigned int qrBg = 0xFFFFFFFF;
	bool qrCard = false;
	std::string qrText, qrFont;
	std::shared_ptr<ImageComponent> qrLogo;
	std::vector<bool> qrModules;
	int qrModulesSize = 0;

	std::deque<float> history;
	int historyMax = 60;
	std::string unit;
	float warn = -1;
	bool autoMax = false;
};

struct GuiControlCenter::Tab
{
	std::string label;
	std::string target;
	std::vector<std::shared_ptr<Binding>> conds;
	std::vector<Item*> items;
};

static const float SLIDE_IN_MS = 230.0f;
static const float SLIDE_OUT_MS = 170.0f;
static const int CYCLER_APPLY_MS = 900;

static std::string tr(const std::string& text)
{
	if (text.empty() || text.find("${") != std::string::npos)
		return text;

#ifdef HAVE_INTL
	return dgettext("controlcenter", text.c_str());
#else
	return text;
#endif
}

static unsigned int withAlpha(unsigned int color, unsigned int alpha)
{
	return (color & 0xFFFFFF00) | (alpha & 0xFF);
}

static bool isTrue(const std::string& value)
{
	std::string v = Utils::String::toLower(Utils::String::trim(value));
	return v == "1" || v == "true" || v == "on" || v == "yes" || v == "enabled";
}

static bool parseNumber(const std::string& s, float& out)
{
	for (size_t i = 0; i < s.size(); i++)
	{
		if (isdigit((unsigned char)s[i]) || (s[i] == '-' && i + 1 < s.size() && isdigit((unsigned char)s[i + 1])))
		{
			out = (float)atof(s.c_str() + i);
			return true;
		}
	}
	return false;
}

static float lastNumber(const std::string& s, float def)
{
	float ret = def;
	for (size_t i = 0; i < s.size(); i++)
	{
		if (isdigit((unsigned char)s[i]) && (i == 0 || !isdigit((unsigned char)s[i - 1])))
			ret = (float)atof(s.c_str() + i);
	}
	return ret > 0 ? ret : def;
}

static std::string shellQuote(const std::string& s)
{
	return "'" + Utils::String::replace(s, "'", "'\\''") + "'";
}

static std::vector<std::string> splitIds(const std::string& s)
{
	std::vector<std::string> ret;
	for (auto id : Utils::String::splitAny(s, " ,;"))
		if (!Utils::String::trim(id).empty())
			ret.push_back(Utils::String::trim(id));
	return ret;
}

static float refreshOf(const pugi::xml_node& node)
{
	return node.attribute("refresh") ? node.attribute("refresh").as_float() : 0.0f;
}

// "Vol -" and "Vol +" give "Vol"
static std::string commonLabel(const std::string& a, const std::string& b)
{
	size_t n = 0;
	while (n < a.size() && n < b.size() && a[n] == b[n])
		n++;

	return Utils::String::trim(a.substr(0, n));
}

GuiControlCenter::GuiControlCenter(Window* window, const std::string& xmlPath, float refHeight, bool fitContent)
	: GuiComponent(window), mRefHeight(refHeight), mFitContent(fitContent),
	mTab(0), mFocus(nullptr), mFocusTabs(false), mFocusHeader(-1), mShowFocus(true), mLayoutDirty(true), mDirty(true), mTabChosen(false),
	mTitleH(0), mHeaderH(0), mTabsH(0), mFooterH(0), mHelpH(0), mHandleH(0), mContentH(0), mScroll(0),
	mShown(0), mClosing(false), mClosed(false), mDragOffset(0),
	mDrag(Drag::None), mDownScroll(0), mDownSheetScroll(0), mDownItem(nullptr),
	mRepeatDir(0), mRepeatTime(0),
	mSheetItem(nullptr), mSheetCursor(0), mSheetScroll(0), mSheetLoading(false)
{
	mShell = new CcShell(window);
	mThemeWidth = 0;
	mThemeHeight = 0;

#ifdef HAVE_INTL
	bindtextdomain("controlcenter", "/usr/share/locale");
	bind_textdomain_codeset("controlcenter", "UTF-8");
#endif

	auto menuTheme = ThemeData::getMenuTheme();
	mFontTitle = menuTheme->Title.font;
	mFontText = menuTheme->Text.font;
	mFontSmall = menuTheme->TextSmall.font;
	mFontFooter = menuTheme->Footer.font;
	mFontSub = Font::get((int)(mFontText->getSize() * 0.72f), mFontText->getPath());

	// theme sizes made for a TV get too small on a handheld
	int subMin = (int)(mRefHeight * 0.024f);
	if (mFontSub->getSize() < subMin)
		mFontSub = Font::get(subMin, mFontText->getPath());

	int footerMin = (int)(mRefHeight * 0.022f);
	if (mFontFooter->getSize() < footerMin)
		mFontFooter = Font::get(footerMin, mFontFooter->getPath());

	mUnit = (float)mFontText->getSize();
	mRowH = mFontText->getHeight(1.5f);
	mPad = 0.45f * mUnit;

	// a translucent menu background is unreadable over a game
	mColorBg = menuTheme->Background.color;
	if ((mColorBg & 0xFF) < 0xF0)
		mColorBg = withAlpha(mColorBg, 0xF0);

	mColorText = menuTheme->Text.color;
	mColorTextDim = withAlpha(menuTheme->Text.color, (menuTheme->Text.color & 0xFF) * 2 / 3);
	mColorSelector = menuTheme->Text.selectorColor;
	mColorSelectorEnd = menuTheme->Text.selectorGradientColor;
	mSelectorHorizontal = menuTheme->Text.selectorGradientType;
	mColorSelected = menuTheme->Text.selectedColor;
	mColorTrack = withAlpha(menuTheme->Text.color, 0x20);
	mColorTitle = menuTheme->Title.color;
	mColorFooter = menuTheme->Footer.color;
	mColorSeparator = menuTheme->Grid.hasSeparatorColor && (menuTheme->Grid.separatorColor & 0xFF) ? menuTheme->Grid.separatorColor : mColorTrack;
	mColorScrollbar = (menuTheme->Background.scrollbarColor & 0xFF) ? menuTheme->Background.scrollbarColor : mColorTextDim;

	if (auto theme = ThemeData::getDefaultTheme())
	{
		if (auto elem = theme->getElement("controlcenter", "card", "container"))
		{
			if (elem->has("size"))
			{
				auto size = elem->get<Vector2f>("size");
				mThemeWidth = size.x();
				mThemeHeight = size.y();
			}
		}

		if (auto elem = theme->getElement("controlcenter", "card", "menuBackground"))
		{
			if (elem->has("color"))
				mColorBg = elem->get<unsigned int>("color");
		}
	}

	auto icon = [window](const std::string& path, bool flip = false)
	{
		auto image = std::make_shared<ImageComponent>(window);
		image->setImage(path);
		image->setFlipX(flip);
		return image;
	};

	mIconOn = icon(menuTheme->Icons.on);
	mIconOff = icon(menuTheme->Icons.off);
	mIconArrow = icon(menuTheme->Icons.arrow);
	mIconOptionArrow = icon(menuTheme->Icons.option_arrow);
	mIconOptionArrowLeft = icon(menuTheme->Icons.option_arrow, true);
	mIconKnob = icon(menuTheme->Icons.knob);

	setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

	mSavesHistory = false;
	mHistoryDirty = false;
	mHistoryMs = 0;
	mHistorySaveMs = 0;

	load(xmlPath);
	loadHistory();
}

GuiControlCenter::~GuiControlCenter()
{
	delete mShell;

	for (auto item : mItems)
		delete item;

	for (auto tab : mTabs)
		delete tab;
}

std::shared_ptr<GuiControlCenter::Binding> GuiControlCenter::bind(const std::string& tmpl, float refresh, int tab, int ttlMs)
{
	auto b = std::make_shared<Binding>();
	b->tmpl = tmpl;
	b->refreshMs = (int)(refresh * 1000);
	b->tab = tab;
	b->ttlMs = refresh > 0 ? std::min(ttlMs, b->refreshMs / 2) : ttlMs;

	if (!CcShell::hasCommands(tmpl))
	{
		b->value = tmpl;
		b->resolved = true;
	}

	mBindings.push_back(b);
	return b;
}

std::shared_ptr<GuiControlCenter::Binding> GuiControlCenter::bindCondition(const pugi::xml_node& node, float refresh, int tab)
{
	std::string expr = Utils::String::trim(node.attribute("if").as_string());
	if (expr.empty())
		return nullptr;

	bool negate = Utils::String::startsWith(expr, "!");
	std::string idExpr = negate ? expr.substr(1) : expr;
	if (Utils::String::startsWith(idExpr, "id(") && Utils::String::endsWith(idExpr, ")"))
	{
		auto b = std::make_shared<Binding>();
		b->idRef = idExpr.substr(3, idExpr.size() - 4);
		b->negate = negate;
		b->resolved = true;
		return b;
	}

	return bind(expr, refresh, tab);
}

void GuiControlCenter::registerId(const pugi::xml_node& node, Item* item)
{
	std::string id = node.attribute("id").as_string();
	if (!id.empty())
		mIds[id] = item;
}

void GuiControlCenter::load(const std::string& xmlPath)
{
	pugi::xml_document doc;
	pugi::xml_parse_result res = doc.load_file(xmlPath.c_str());
	if (!res)
	{
		LOG(LogError) << "ControlCenter: unable to parse " << xmlPath << ": " << res.description();
		return;
	}

	pugi::xml_node root = doc.child("features");

	std::map<std::string, pugi::xml_node> panels;
	std::vector<pugi::xml_node> loose;

	for (pugi::xml_node node : root.children())
	{
		std::string name = node.name();
		std::string role = node.attribute("role").as_string();

		if (name == "vgroup" && role == "header")
			buildContainer(node, -1, mHeader, {});
		else if (name == "vgroup" && role == "footer")
			buildContainer(node, -1, mFooter, {});
		else if (name == "feature" && node.child("tab"))
		{
			for (pugi::xml_node t : node.children("tab"))
			{
				Tab* tab = new Tab();
				tab->label = tr(t.attribute("display").as_string());
				tab->target = t.attribute("target").as_string();
				auto cond = bindCondition(t, std::max(refreshOf(t), 3.0f), -1);
				if (cond)
					tab->conds.push_back(cond);

				mTabs.push_back(tab);
			}
		}
		else if (name == "hgroup" && node.attribute("name"))
			panels[node.attribute("name").as_string()] = node;
		else
			loose.push_back(node);
	}

	if (mTabs.empty())
	{
		Tab* tab = new Tab();
		mTabs.push_back(tab);

		for (auto& p : panels)
			loose.push_back(p.second);
	}

	for (int i = 0; i < (int)mTabs.size(); i++)
	{
		Tab* tab = mTabs[i];
		if (!tab->target.empty())
		{
			auto it = panels.find(tab->target);
			if (it != panels.cend())
				buildContainer(it->second, i, tab->items, {});
		}
		else
		{
			for (auto& node : loose)
				buildContainer(node, i, tab->items, {});
		}
	}

	mTab = 0;
}

void GuiControlCenter::buildContainer(const pugi::xml_node& node, int tab, std::vector<Item*>& target, std::vector<std::shared_ptr<Binding>> conds)
{
	auto cond = bindCondition(node, refreshOf(node), tab);
	if (cond)
		conds.push_back(cond);

	std::string name = node.name();
	if (name == "feature")
	{
		std::vector<pugi::xml_node> controls;
		for (pugi::xml_node c : node.children())
			controls.push_back(c);

		buildControls(tr(node.attribute("display").as_string()), controls, tab, target, conds);
		return;
	}

	std::vector<pugi::xml_node> controls;
	for (pugi::xml_node c : node.children())
	{
		std::string cn = c.name();
		if (cn == "feature" || cn == "hgroup" || cn == "vgroup")
		{
			if (!controls.empty())
			{
				buildControls("", controls, tab, target, conds);
				controls.clear();
			}

			buildContainer(c, tab, target, conds);
		}
		else if (c.type() == pugi::node_element)
			controls.push_back(c);
	}

	if (!controls.empty())
		buildControls("", controls, tab, target, conds);
}

void GuiControlCenter::buildControls(const std::string& label, const std::vector<pugi::xml_node>& controls, int tab, std::vector<Item*>& target, const std::vector<std::shared_ptr<Binding>>& conds)
{
	bool header = (&target == &mHeader || &target == &mFooter);

	std::vector<pugi::xml_node> buttons, progress, texts, switches, choices, images, qrcodes, docs, graphs;
	for (auto& c : controls)
	{
		std::string n = c.name();
		if (n == "button" || n == "button_confirm") buttons.push_back(c);
		else if (n == "progressbar") progress.push_back(c);
		else if (n == "text") texts.push_back(c);
		else if (n == "switch" || n == "toggle") switches.push_back(c);
		else if (n == "choice" || n == "choice_cmd") choices.push_back(c);
		else if (n == "img") images.push_back(c);
		else if (n == "qrcode") qrcodes.push_back(c);
		else if (n == "doc") docs.push_back(c);
		else if (n == "graph") graphs.push_back(c);
		else
			LOG(LogDebug) << "ControlCenter: <" << n << "> is not rendered yet";
	}

	auto newItem = [&](CcKind kind, const pugi::xml_node& node) -> Item*
	{
		Item* item = new Item();
		item->kind = kind;
		item->conds = conds;
		auto cond = bindCondition(node, refreshOf(node), tab);
		if (cond)
			item->conds.push_back(cond);

		item->afterclick = node.attribute("afterclick").as_string();
		item->updateIds = splitIds(node.attribute("updateids").as_string());

		std::string enabled = node.attribute("enabled").as_string();
		if (!enabled.empty())
			item->enabled = bind(enabled, std::max(refreshOf(node), 2.0f), tab);
		item->disabledLabel = tr(node.attribute("disabled_display").as_string());
		registerId(node, item);
		mItems.push_back(item);
		target.push_back(item);
		return item;
	};

	std::string firstLabel = label;

	if (!header && progress.size() == 1 && buttons.size() >= 2)
	{
		auto& p = progress[0];
		auto& dec = buttons.front();
		auto& inc = buttons.back();

		Item* item = newItem(CcKind::Slider, p);
		item->label = !label.empty() ? label : commonLabel(tr(dec.attribute("display").as_string()), tr(inc.attribute("display").as_string()));
		item->value = bind(p.attribute("display").as_string(), refreshOf(p), tab);
		item->min = p.attribute("min").as_float(0);
		item->max = p.attribute("max").as_float(100);
		item->actionDec = dec.attribute("action").as_string();
		item->actionInc = inc.attribute("action").as_string();
		item->actionSet = p.attribute("action_set").as_string();
		item->step = lastNumber(item->actionInc, 5);
		registerId(dec, item);
		registerId(inc, item);

		progress.clear();
		buttons.erase(buttons.begin());
		buttons.pop_back();
		firstLabel = "";
	}

	for (auto& s : switches)
	{
		Item* item = newItem(CcKind::Tile, s);
		item->toggle = true;
		item->label = !label.empty() ? label : tr(s.attribute("display").as_string());
		item->value = bind(s.attribute("value").as_string(), refreshOf(s), tab);
		item->actionOn = s.attribute("action_on").as_string();
		item->actionOff = s.attribute("action_off").as_string();
		firstLabel = "";
	}

	if (!choices.empty())
	{
		Item* item = newItem(CcKind::Cycler, choices[0]);
		item->label = label;

		if (!texts.empty())
		{
			item->value = bind(tr(texts[0].attribute("display").as_string()), refreshOf(texts[0]), tab);
			registerId(texts[0], item);
			texts.erase(texts.begin());
		}

		for (auto& c : choices)
		{
			int cache = (int)(c.attribute("cache").as_float(5) * 1000);
			item->cacheMs = std::max(item->cacheMs, cache);

			if (std::string(c.name()) == "choice_cmd")
			{
				item->cmdLabels = c.attribute("display").as_string();
				item->cmdArgs = c.attribute("action_arg").as_string();
				item->cmdAction = c.attribute("action").as_string();
				item->cmdAfterclick = c.attribute("afterclick").as_string();
				item->cmdUpdateIds = splitIds(c.attribute("updateids").as_string());
			}
			else
			{
				Item::Choice ch;
				ch.label = tr(c.attribute("display").as_string());
				ch.action = c.attribute("action").as_string();
				ch.afterclick = c.attribute("afterclick").as_string();
				ch.updateIds = splitIds(c.attribute("updateids").as_string());
				item->choices.push_back(ch);
			}
		}

		if (item->label.empty() && !buttons.empty())
			item->label = tr(buttons[0].attribute("display").as_string());

		firstLabel = "";
	}

	for (auto& b : buttons)
	{
		Item* item = newItem(header ? CcKind::Chip : CcKind::Tile, b);
		std::string display = tr(b.attribute("display").as_string());
		if (buttons.size() == 1 && !firstLabel.empty() && firstLabel != display)
		{
			item->label = firstLabel;
			item->subLabel = display;
		}
		else
			item->label = display;

		item->action = b.attribute("action").as_string();
		item->confirm = std::string(b.name()) == "button_confirm";
		firstLabel = "";

		if (header)
		{
			// the feature names the action, the button often just says "Toggle"
			std::string text = !label.empty() && buttons.size() == 1 ? label : display;
			item->buttonText = Utils::String::toUpper(text);
		}
	}

	for (auto& g : graphs)
	{
		Item* item = newItem(CcKind::Graph, g);
		item->label = label;
		item->value = bind(g.attribute("display").as_string(), refreshOf(g) > 0 ? refreshOf(g) : 1.0f, tab);
		item->min = g.attribute("min").as_float(0);
		item->max = g.attribute("max").as_float(100);
		item->historyMax = std::max(2, g.attribute("history").as_int(60));
		item->unit = g.attribute("unit").as_string("%");
		item->autoMax = std::string(g.attribute("max").as_string()) == "auto";
		item->warn = g.attribute("warn") ? g.attribute("warn").as_float() : (item->unit == "%" ? 85.0f : -1.0f);

		if (!texts.empty())
		{
			item->labelValue = bind(tr(texts[0].attribute("display").as_string()), refreshOf(texts[0]), tab);
			registerId(texts[0], item);
			texts.erase(texts.begin());
		}

		item->value->onResult = [this, item](const std::string& v)
		{
			float f = 0;
			if (!parseNumber(v, f))
				return;

			item->history.push_back(f);
			while ((int)item->history.size() > item->historyMax)
				item->history.pop_front();
			mHistoryDirty = true;
		};
		firstLabel = "";
	}

	for (auto& d : docs)
	{
		Item* item = newItem(header ? CcKind::Chip : CcKind::Tile, d);
		item->label = tr(d.attribute("display").as_string("View"));
		item->doc = true;
		item->value = bind(d.attribute("content").as_string(), refreshOf(d), tab);
		if (header)
			item->buttonText = Utils::String::toUpper(item->label);
		firstLabel = "";
	}

	for (auto& t : texts)
	{
		Item* item = newItem(header ? CcKind::Chip : CcKind::Info, t);
		item->label = firstLabel;
		item->value = bind(tr(t.attribute("display").as_string()), refreshOf(t), tab);
		firstLabel = "";
	}

	for (auto& p : progress)
	{
		Item* item = newItem(header ? CcKind::Chip : CcKind::Info, p);
		item->progress = true;
		item->label = firstLabel;
		item->value = bind(p.attribute("display").as_string(), refreshOf(p), tab);
		item->min = p.attribute("min").as_float(0);
		item->max = p.attribute("max").as_float(100);
		firstLabel = "";
	}

	for (auto& q : qrcodes)
	{
		Item* item = newItem(CcKind::Qr, q);
		item->value = bind(q.attribute("display").as_string(), refreshOf(q), tab);

		auto dimension = [this](const std::string& v) -> float
		{
			if (v.empty())
				return 0;
			if (Utils::String::endsWith(v, "%"))
				return (float)atof(v.c_str()) / 100.0f * mRefHeight;
			return (float)atof(v.c_str());
		};

		float w = dimension(q.attribute("width").as_string());
		float h = dimension(q.attribute("height").as_string());
		item->qrSize = w > 0 ? w : (h > 0 ? h : 160.0f);

		std::string bg = Utils::String::trim(q.attribute("bg").as_string());
		if (Utils::String::startsWith(bg, "#") && bg.size() >= 7)
			item->qrBg = (unsigned int)std::stoul(bg.substr(1, 6), nullptr, 16) << 8 | 0xFF;

		item->qrCard = std::string(q.attribute("style").as_string()) == "card";
		if (item->qrCard)
		{
			item->qrText = q.attribute("text").as_string();
			item->qrFont = q.attribute("font").as_string();

			std::string logo = q.attribute("logo").as_string();
			if (!logo.empty() && Utils::FileSystem::exists(logo))
			{
				item->qrLogo = std::make_shared<ImageComponent>(mWindow);
				item->qrLogo->setImage(logo);
			}
		}

		float total = item->qrSize;
		if (item->qrCard)
			total += item->qrSize * 0.1f + (item->qrLogo ? item->qrSize / 4 : 0) + (!item->qrText.empty() ? item->qrSize / 6 : 0);
		item->imageHeight = total / mRefHeight;

		auto encode = [item](const std::string& data)
		{
			item->qrModules.clear();
			item->qrModulesSize = 0;

			std::string text = Utils::String::trim(data);
			if (text.empty())
				return;

			try
			{
				auto qr = qrcodegen::QrCode::encodeText(text.c_str(), qrcodegen::QrCode::Ecc::LOW);
				item->qrModulesSize = qr.getSize();
				item->qrModules.resize(item->qrModulesSize * item->qrModulesSize);
				for (int y = 0; y < item->qrModulesSize; y++)
					for (int x = 0; x < item->qrModulesSize; x++)
						item->qrModules[y * item->qrModulesSize + x] = qr.getModule(x, y);
			}
			catch (std::exception& e)
			{
				LOG(LogWarning) << "ControlCenter: qrcode not generated: " << e.what();
			}
		};

		item->value->onChanged = encode;
		if (item->value->resolved)
			encode(item->value->value);
	}

	for (auto& i : images)
	{
		Item* item = newItem(CcKind::Image, i);
		item->value = bind(i.attribute("display").as_string(), refreshOf(i), tab);

		std::string h = i.attribute("height").as_string();
		item->imageHeight = Utils::String::endsWith(h, "%") ? (float)atof(h.c_str()) / 100.0f : (h.empty() ? 0.15f : (float)atof(h.c_str()) / mRefHeight);
		if (header)
			item->imageHeight = 0.03f;

		item->image = std::make_shared<WebImageComponent>(mWindow, 30);
		auto image = item->image;
		item->value->onChanged = [image](const std::string& path) { image->setImage(path); };
		if (item->value->resolved)
			image->setImage(item->value->value);
	}
}

bool GuiControlCenter::conditionsPass(const std::vector<std::shared_ptr<Binding>>& conds, int depth)
{
	for (auto& c : conds)
	{
		if (!c->idRef.empty())
		{
			auto it = mIds.find(c->idRef);
			bool rendered = it != mIds.cend() && depth < 4 && isVisible(it->second, depth + 1);
			if (rendered == c->negate)
				return false;
		}
		else if (!c->resolved || Utils::String::trim(c->value).empty())
			return false;
	}

	return true;
}

bool GuiControlCenter::isVisible(Item* item, int depth)
{
	if (!conditionsPass(item->conds, depth))
		return false;

	if (item->kind == CcKind::Image || item->kind == CcKind::Qr || item->kind == CcKind::Info || item->doc || (item->kind == CcKind::Chip && item->value))
		return item->value && item->value->resolved && !Utils::String::trim(item->value->value).empty();

	return true;
}

static bool isEnabled(GuiControlCenter::Item* item);

bool GuiControlCenter::isVisible(Tab* tab)
{
	return conditionsPass(tab->conds);
}

std::vector<GuiControlCenter::Tab*> GuiControlCenter::visibleTabs()
{
	std::vector<Tab*> ret;
	for (auto t : mTabs)
		if (isVisible(t))
			ret.push_back(t);

	return ret;
}

void GuiControlCenter::selectTabIndex(int index)
{
	if (index < 0 || index >= (int)mTabs.size())
		return;

	mTabChosen = true;
	if (index == mTab)
		return;

	mTab = index;
	mScroll = 0;
	mFocus = nullptr;
	mLayoutDirty = true;
	closeSheet();
}

void GuiControlCenter::selectTab(int delta)
{
	auto tabs = visibleTabs();
	if (tabs.size() < 2)
		return;

	int cur = 0;
	for (int i = 0; i < (int)tabs.size(); i++)
		if (tabs[i] == mTabs[mTab])
			cur = i;

	int next = (cur + delta + (int)tabs.size()) % (int)tabs.size();
	auto it = std::find(mTabs.begin(), mTabs.end(), tabs[next]);
	selectTabIndex((int)(it - mTabs.begin()));
}

void GuiControlCenter::onSizeChanged()
{
	mLayoutDirty = true;
}

void GuiControlCenter::layout()
{
	mLayoutDirty = false;
	mDirty = true;

	const float u = mUnit;
	const float W = mSize.x();
	const float H = mSize.y();

	float cardW = mFitContent ? (mThemeWidth > 0 ? W * mThemeWidth : std::min(H, W * 0.90f)) : W;

	// tab conditions resolve late: follow the first visible tab until one is picked
	if (mTab >= (int)mTabs.size() || !isVisible(mTabs[mTab]) || !mTabChosen)
	{
		auto tabs = visibleTabs();
		int first = tabs.empty() ? mTab : (int)(std::find(mTabs.begin(), mTabs.end(), tabs[0]) - mTabs.begin());
		if (first != mTab)
		{
			mTab = first;
			mScroll = 0;
			mFocus = nullptr;
		}
	}

	const float gap = 0.3f * u;
	const float groupGap = 0.4f * mRowH;
	const float rowH = 1.25f * mRowH;
	float y = groupGap;
	int row = 0;
	int lastGroup = -1;

	std::vector<Item*> items;
	if (mTab < (int)mTabs.size())
	{
		for (auto item : mTabs[mTab]->items)
		{
			if (isVisible(item))
				items.push_back(item);
			else
				item->row = -1;
		}
	}

	for (size_t i = 0; i < items.size(); )
	{
		Item* item = items[i];

		auto media = [](Item* it) { return it->kind == CcKind::Image || it->kind == CcKind::Qr; };
		size_t end = i;
		while (end < items.size() && (items[end]->kind == item->kind || (media(item) && media(items[end]))))
			end++;

		item->asRow = item->kind == CcKind::Tile && end - i == 1;

		int group = item->kind == CcKind::Slider || item->kind == CcKind::Cycler || item->asRow ? 0 : media(item) ? 1 + (int)CcKind::Image : 1 + (int)item->kind;
		if (lastGroup >= 0 && lastGroup != group)
			y += groupGap;
		lastGroup = group;

		if ((item->kind == CcKind::Tile && !item->asRow) || media(item))
		{
			CcKind kind = item->kind;
			int count = (int)(end - i);
			int cols = kind == CcKind::Tile ? clampf((int)((cardW + gap) / (7.0f * u + gap)), 2, 4) : count;
			cols = std::min(cols, count);

			if (kind == CcKind::Tile)
			{
				int rows = (count + cols - 1) / cols;
				cols = (count + rows - 1) / rows;
			}
			float cellW = (cardW - gap * (cols - 1)) / cols;

			float cellH = 2.2f * mRowH;
			if (kind != CcKind::Tile)
			{
				cellH = 0;
				for (size_t k = i; k < end; k++)
					cellH = std::max(cellH, clampf(items[k]->imageHeight * mRefHeight, 2.0f * u, 8.0f * u));
			}

			for (int k = 0; k < count; k++)
			{
				Item* it = items[i + k];
				int c = k % cols;
				int r = k / cols;
				it->rect = Renderer::Rect((int)(c * (cellW + gap)), (int)(y + r * (cellH + gap)), (int)cellW, (int)cellH);
				it->row = kind == CcKind::Tile ? row + r : -1;
				it->rect.h = (int)cellH;

				if (it->image)
					it->image->setMaxSize(cellW, cellH);
			}

			int rows = (count + cols - 1) / cols;
			y += rows * (cellH + gap) - gap;
			if (kind == CcKind::Tile)
				row += rows;

			i = end;
			continue;
		}

		float h = item->kind == CcKind::Info ? 1.05f * mRowH : item->kind == CcKind::Graph ? 2.4f * mRowH : rowH;
		item->rect = Renderer::Rect(0, (int)y, (int)cardW, (int)h);
		item->row = item->kind == CcKind::Info || item->kind == CcKind::Graph ? -1 : row++;
		y += h;
		i++;
	}

	mContentH = y + groupGap;

	bool hasHeader = false;
	for (auto item : mHeader)
		hasHeader |= isVisible(item);

	bool hasFooter = false;
	for (auto item : mFooter)
		hasFooter |= isVisible(item);

	mTitleH = mFontTitle->getHeight(1.9f);
	mTabsH = mTitleH + (visibleTabs().size() > 1 ? mFontText->getHeight(1.9f) : 0.3f * u);
	mHeaderH = hasHeader ? mFontFooter->getHeight(2.0f) : 0;
	if (!headerButtons().empty())
		mHeaderH = std::max(mHeaderH, mFontSub->getHeight(1.9f) + 0.5f * u);
	mFooterH = hasFooter ? mFontFooter->getHeight(2.2f) : 0;
	mHelpH = getHelpStyle().font->getHeight(1.7f);
	mHandleH = 0.7f * u;

	float chrome = mTabsH + mHeaderH + mFooterH + mHelpH + mHandleH;
	float maxH = H * (mThemeHeight > 0 ? mThemeHeight : 1.0f);

	// shorten the graphs rather than scroll them
	int graphs = 0;
	for (auto item : items)
		graphs += item->kind == CcKind::Graph;

	float excess = chrome + mContentH - maxH;
	if (mFitContent && graphs > 0 && excess > 0)
	{
		int shrink = (int)std::ceil(std::min(excess / graphs, 0.7f * mRowH));
		int shift = 0;
		for (auto item : items)
		{
			item->rect.y -= shift;
			if (item->kind == CcKind::Graph)
			{
				item->rect.h -= shrink;
				shift += shrink;
			}
		}
		mContentH -= shift;
	}

	int cardH = (int)std::ceil(mFitContent ? std::min(maxH, chrome + mContentH) : H);

	bool moved = mCard.w != (int)cardW || mCard.h != cardH;
	mCard = Renderer::Rect((int)((W - cardW) / 2), 0, (int)cardW, cardH);
	if (moved)
		updateHelpPrompts();
	mScroll = clampf(mScroll, 0.0f, std::max(0.0f, mContentH - viewHeight()));

	auto focus = focusables();
	if (mFocus && std::find(focus.begin(), focus.end(), mFocus) == focus.end())
		mFocus = nullptr;
	if (!mFocus && !mFocusTabs && !focus.empty())
		mFocus = focus.front();
}

float GuiControlCenter::viewHeight()
{
	return mCard.h - (mTabsH + mHeaderH + mFooterH + mHelpH + mHandleH);
}

void GuiControlCenter::sliderTrack(Item* item, float& x, float& w)
{
	float labelW = std::min(item->rect.w * 0.32f, 7.0f * mUnit);
	float valueW = mFontText->sizeText("100%", 1.0f).x() + mPad;
	x = item->rect.x + labelW + mPad;
	w = item->rect.w - labelW - valueW - 3 * mPad;
}

static bool isEnabled(GuiControlCenter::Item* item)
{
	return !item->enabled || (item->enabled->resolved && !Utils::String::trim(item->enabled->value).empty());
}

std::vector<GuiControlCenter::Item*> GuiControlCenter::focusables()
{
	std::vector<Item*> ret;
	if (mTab < (int)mTabs.size())
		for (auto item : mTabs[mTab]->items)
			if (item->row >= 0 && isVisible(item) && isEnabled(item))
				ret.push_back(item);

	return ret;
}

std::vector<GuiControlCenter::Item*> GuiControlCenter::headerButtons()
{
	std::vector<Item*> ret;
	for (auto item : mHeader)
		if (!item->buttonText.empty() && isVisible(item))
			ret.push_back(item);

	return ret;
}

void GuiControlCenter::setHeaderFocus(int index)
{
	auto buttons = headerButtons();
	mFocusHeader = index < (int)buttons.size() ? index : -1;
	if (mFocusHeader >= 0)
		mFocusTabs = false;
}

void GuiControlCenter::moveFocusVertical(int dir)
{
	auto items = focusables();
	bool hasButtons = !headerButtons().empty();
	bool hasTabs = visibleTabs().size() > 1;

	if (mFocusTabs)
	{
		if (dir > 0 && hasButtons)
			setHeaderFocus(0);
		else if (dir > 0 && !items.empty())
		{
			mFocusTabs = false;
			mFocus = items.front();
		}
		return;
	}

	if (mFocusHeader >= 0)
	{
		if (dir < 0 && hasTabs)
		{
			setHeaderFocus(-1);
			mFocusTabs = true;
		}
		else if (dir > 0 && !items.empty())
		{
			setHeaderFocus(-1);
			mFocus = items.front();
			ensureFocusVisible();
		}
		return;
	}

	if (!mFocus)
	{
		if (!items.empty())
			mFocus = items.front();
		return;
	}

	int targetRow = mFocus->row + dir;

	bool fullWidth = mFocus->kind != CcKind::Tile;
	float cx = fullWidth ? 0.0f : mFocus->rect.x + mFocus->rect.w / 2.0f;

	Item* best = nullptr;
	float bestDist = 0;
	for (auto item : items)
	{
		if (item->row != targetRow)
			continue;

		float d = std::abs(item->rect.x + item->rect.w / 2.0f - cx);
		if (!best || d < bestDist)
		{
			best = item;
			bestDist = d;
		}
	}

	if (best)
		mFocus = best;
	else if (dir < 0 && hasButtons)
	{
		setHeaderFocus(0);
		mScroll = 0;
	}
	else if (dir < 0 && hasTabs)
	{
		mFocusTabs = true;
		mScroll = 0;
	}

	ensureFocusVisible();
}

void GuiControlCenter::moveFocusHorizontal(int dir)
{
	if (!mFocus)
		return;

	Item* best = nullptr;
	for (auto item : focusables())
	{
		if (item->row != mFocus->row || item == mFocus)
			continue;

		bool side = dir > 0 ? item->rect.x > mFocus->rect.x : item->rect.x < mFocus->rect.x;
		if (side && (!best || std::abs(item->rect.x - mFocus->rect.x) < std::abs(best->rect.x - mFocus->rect.x)))
			best = item;
	}

	if (best)
		mFocus = best;
}

void GuiControlCenter::ensureFocusVisible()
{
	if (!mFocus)
		return;

	float viewH = viewHeight();
	float top = (float)mFocus->rect.y;
	float bottom = (float)(mFocus->rect.y + mFocus->rect.h);

	if (top < mScroll)
		mScroll = top;
	else if (bottom > mScroll + viewH)
		mScroll = bottom - viewH;

	mScroll = clampf(mScroll, 0.0f, std::max(0.0f, mContentH - viewH));
}

void GuiControlCenter::open()
{
	mShown = 0;
	mClosing = false;
	mClosed = false;
	mDragOffset = 0;
	mDrag = Drag::None;
	mScroll = 0;
	mFocus = nullptr;
	mFocusTabs = false;
	mFocusHeader = -1;
	mTabChosen = false;
	mSheetItem = nullptr;
	mRepeatDir = 0;
	mShowFocus = true;
	mLayoutDirty = true;
	mDirty = true;

	refreshAll();
	updateHelpPrompts();
}

bool GuiControlCenter::consumeDirty()
{
	bool animating = mShown < 1.0f || mClosing || mDragOffset < 0 || mDrag != Drag::None || mRepeatDir != 0 || mSheetLoading;
	for (auto item : mItems)
		animating |= item->applyTimer > 0 || item->loading;

	bool dirty = mDirty || animating;
	mDirty = false;
	return dirty;
}

std::string GuiControlCenter::defaultXmlPath()
{
	for (auto path : { "/userdata/system/configs/controlcenter/controlcenter.xml", "/usr/share/batocera/controlcenter/controlcenter.xml" })
		if (Utils::FileSystem::exists(path))
			return path;

	return "";
}

void GuiControlCenter::close()
{
	if (mClosing)
		return;

	mClosing = true;
	mSheetItem = nullptr;
	updateHelpPrompts();
}

void GuiControlCenter::request(const std::shared_ptr<Binding>& b)
{
	b->inflight = true;
	b->elapsed = 0;

	mShell->expand(b->tmpl, b->ttlMs, [this, b](const std::string& value)
	{
		b->inflight = false;
		if (b->stale)
		{
			b->stale = false;
			mShell->invalidate(b->tmpl);
			request(b);
			return;
		}

		if (b->holdMs > 0 && b->resolved)
			return;

		bool wasEmpty = !b->resolved || Utils::String::trim(b->value).empty();
		bool changed = !b->resolved || value != b->value;

		b->value = value;
		b->resolved = true;
		mDirty = true;

		if (wasEmpty != Utils::String::trim(value).empty())
			mLayoutDirty = true;

		if (changed && b->onChanged)
			b->onChanged(value);

		if (b->onResult)
			b->onResult(value);
	});
}

static const int HIDDEN_SAMPLE_MS = 5000;

static std::string historyPath()
{
	return "/tmp/.batocera-resources/controlcenter-history";
}

// Graphs off screen keep their history rolling, at a slower rate
void GuiControlCenter::sampleHiddenGraphs(int deltaTime)
{
	mHistoryMs += deltaTime;
	if (mHistoryMs >= HIDDEN_SAMPLE_MS)
	{
		mHistoryMs = 0;
		for (auto item : mItems)
		{
			auto& b = item->value;
			if (item->kind != CcKind::Graph || !b || b->inflight)
				continue;

			if (mClosed || (b->tab != -1 && b->tab != mTab))
			{
				mShell->invalidate(b->tmpl);
				request(b);
			}
		}
	}

	mHistorySaveMs += deltaTime;
	if (mSavesHistory && mHistoryDirty && mHistorySaveMs >= 1000)
	{
		mHistorySaveMs = 0;
		mHistoryDirty = false;
		saveHistory();
	}
}

// one line per graph: its command, a tab, then the values
void GuiControlCenter::saveHistory()
{
	std::string tmp = historyPath() + ".new";
	std::ofstream out(tmp);
	if (!out)
		return;

	for (auto item : mItems)
	{
		if (item->kind != CcKind::Graph || !item->value || item->history.empty())
			continue;

		out << item->value->tmpl << '\t';
		for (auto v : item->history)
			out << v << ' ';
		out << '\n';
	}
	out.close();
	std::rename(tmp.c_str(), historyPath().c_str());
}

void GuiControlCenter::loadHistory()
{
	std::ifstream in(historyPath());
	std::string line;
	while (std::getline(in, line))
	{
		auto tab = line.find('\t');
		if (tab == std::string::npos)
			continue;

		std::string cmd = line.substr(0, tab);
		for (auto item : mItems)
		{
			if (item->kind != CcKind::Graph || !item->value || item->value->tmpl != cmd)
				continue;

			item->history.clear();
			for (auto& v : Utils::String::split(line.substr(tab + 1), ' ', true))
			{
				float f;
				if (parseNumber(v, f))
					item->history.push_back(f);
			}
			while ((int)item->history.size() > item->historyMax)
				item->history.pop_front();
		}
	}
}

void GuiControlCenter::refreshItem(Item* item)
{
	for (auto& b : { item->value, item->labelValue, item->enabled })
	{
		if (!b || !CcShell::hasCommands(b->tmpl))
			continue;

		b->holdMs = 0;
		mShell->invalidate(b->tmpl);
		if (b->inflight)
			b->stale = true;
		else
			request(b);
	}
}

void GuiControlCenter::refreshAll()
{
	for (auto& b : mBindings)
	{
		mShell->invalidate(b->tmpl);
		b->holdMs = 0;
		if (!b->inflight && CcShell::hasCommands(b->tmpl))
			request(b);
	}
}

void GuiControlCenter::runAction(Item* item, const std::string& cmd, const std::string& afterclick, const std::vector<std::string>& updateIds)
{
	if (cmd.empty())
		return;

	if (afterclick == "bcc_close")
	{
		mCloseActions.push_back(cmd);
		close();
		return;
	}

	item->running++;
	mShell->run(cmd, [this, item, afterclick, updateIds]()
	{
		item->running--;
		afterAction(item, afterclick, updateIds);
	});
}

std::string GuiControlCenter::takeCloseActions()
{
	std::string cmds = Utils::String::join(mCloseActions, "; ");
	mCloseActions.clear();
	return cmds;
}

void GuiControlCenter::afterAction(Item* item, const std::string& afterclick, const std::vector<std::string>& updateIds)
{
	// reading back while later presses are queued makes the control bounce
	if (item->running > 0)
		return;

	refreshItem(item);

	for (auto& id : updateIds)
	{
		auto it = mIds.find(id);
		if (it != mIds.cend())
			refreshItem(it->second);
	}

	std::string ac = Utils::String::trim(afterclick);
	if (ac.empty() || ac == "bcc_close")
		return;

	if (ac == "bcc_refresh")
		refreshAll();
	else if (ac == "bcc_rescreen")
	{
		if (mOnRescreen)
			mOnRescreen();
	}
	else if (Utils::String::startsWith(ac, "${") && Utils::String::endsWith(ac, "}"))
		mShell->run(ac.substr(2, ac.size() - 3));
	else
		mShell->run(ac);
}

// Shows a value before the command has applied it. A read already running predates
// the change, so it is dropped; the next read replaces the value.
static void showPending(GuiControlCenter::Binding* b, const std::string& value)
{
	b->value = value;
	b->holdMs = 2000;
	if (b->inflight)
		b->stale = true;
}

float GuiControlCenter::sliderFraction(Item* item)
{
	float v = item->min;
	if (mDrag == Drag::Slider && mDownItem == item)
		v = item->target;
	else if (item->value && item->value->resolved)
		parseNumber(item->value->value, v);

	if (item->max <= item->min)
		return 0;

	return clampf((v - item->min) / (item->max - item->min), 0.0f, 1.0f);
}

void GuiControlCenter::setSliderTarget(Item* item, float value)
{
	value = clampf(value, item->min, item->max);

	float current = item->min;
	if (item->value->resolved)
		parseNumber(item->value->value, current);

	std::string cmd;
	if (!item->actionSet.empty())
		cmd = Utils::String::replace(item->actionSet, "{value}", std::to_string((int)std::round(value)));
	else
	{
		int n = (int)std::round((value - current) / item->step);
		if (n == 0)
			return;

		value = clampf(current + n * item->step, item->min, item->max);
		const std::string& step = n > 0 ? item->actionInc : item->actionDec;
		cmd = std::abs(n) == 1 ? step : "i=0; while [ $i -lt " + std::to_string(std::abs(n)) + " ]; do " + step + "; i=$((i+1)); done";
	}

	item->target = value;
	showPending(item->value.get(), std::to_string((int)std::round(value)));
	runAction(item, cmd, item->afterclick, item->updateIds);
}

void GuiControlCenter::adjust(Item* item, int dir)
{
	if (item->kind == CcKind::Slider)
	{
		float current = item->min;
		if (item->value->resolved)
			parseNumber(item->value->value, current);

		setSliderTarget(item, current + dir * item->step);
	}
	else if (item->kind == CcKind::Cycler)
	{
		bool dynamic = !item->cmdLabels.empty();
		for (auto& c : item->choices)
			dynamic |= CcShell::hasCommands(c.label);

		if (dynamic || item->choices.empty())
		{
			openSheet(item);
			return;
		}

		int n = (int)item->choices.size();
		if (item->pendingIndex < 0)
		{
			std::string cur = Utils::String::toLower(item->value ? item->value->value : "");
			item->pendingIndex = dir > 0 ? -1 : 0;
			for (int i = 0; i < n; i++)
				if (!cur.empty() && Utils::String::toLower(item->choices[i].label) == cur)
					item->pendingIndex = i;
		}

		item->pendingIndex = (item->pendingIndex + dir + n) % n;
		item->applyTimer = CYCLER_APPLY_MS;
	}
	else if (item->kind == CcKind::Tile)
		moveFocusHorizontal(dir);
}

void GuiControlCenter::commitCycler(Item* item)
{
	if (item->pendingIndex < 0 || item->pendingIndex >= (int)item->choices.size())
		return;

	auto& c = item->choices[item->pendingIndex];
	if (item->value)
	{
		showPending(item->value.get(), c.label);
	}

	item->pendingIndex = -1;
	runAction(item, c.action, c.afterclick, c.updateIds);
}

static std::string docType(const std::string& path, const std::string& name)
{
	std::string ext = Utils::String::toLower(Utils::FileSystem::getExtension(name));
	if (ext == ".pdf") return "pdf";
	if (ext == ".cbz") return "cbz";
	for (auto e : { ".jpg", ".jpeg", ".png", ".gif", ".bmp", ".webp" })
		if (ext == e) return "image";
	for (auto e : { ".txt", ".log", ".md", ".conf", ".cfg", ".ini", ".json", ".xml", ".yaml", ".yml" })
		if (ext == e) return "text";

	char header[16] = { 0 };
	std::ifstream file(path, std::ios::binary);
	file.read(header, sizeof(header));
	std::string h(header, (size_t)file.gcount());

	if (Utils::String::startsWith(h, "%PDF")) return "pdf";
	if (Utils::String::startsWith(h, "PK\x03\x04") || Utils::String::startsWith(h, "PK\x05\x06")) return "cbz";
	if (Utils::String::startsWith(h, "\x89PNG") || Utils::String::startsWith(h, "\xff\xd8\xff") || Utils::String::startsWith(h, "GIF8") ||
		Utils::String::startsWith(h, "BM") || (h.size() >= 12 && Utils::String::startsWith(h, "RIFF") && h.substr(8, 4) == "WEBP"))
		return "image";

	return h.empty() ? "" : "text";
}

void GuiControlCenter::openDoc(Item* item)
{
	std::string content = Utils::String::trim(item->value ? item->value->value : "");
	if (content.empty() || item->loading)
		return;

	if (!Utils::String::startsWith(content, "http://") && !Utils::String::startsWith(content, "https://"))
	{
		showDoc(item, content);
		return;
	}

	// the viewers pick their decoder from the extension
	std::string name = content.substr(0, content.find('?'));
	std::string ext = Utils::FileSystem::getExtension(name);
	std::string target = "/tmp/es-controlcenter-doc" + (ext.size() <= 5 ? ext : "");

	item->loading = true;
	std::string cmd = "${curl -sfL --max-time 30 -o " + shellQuote(target) + " " + shellQuote(content) + " && echo " + shellQuote(target) + "}";
	mShell->expand(cmd, 0, [this, item, content](const std::string& path)
	{
		item->loading = false;
		if (path.empty())
		{
			mWindow->pushGui(new GuiMsgBox(mWindow, _("Unable to download") + "\n" + content, _("OK")));
			return;
		}
		showDoc(item, path);
	});
}

void GuiControlCenter::showDoc(Item* item, const std::string& path)
{
	if (!Utils::FileSystem::exists(path))
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, _("File not found") + "\n" + path, _("OK")));
		return;
	}

	std::string type = docType(path, path);
	std::string file = path;

	if (Utils::FileSystem::getExtension(path).empty() && (type == "pdf" || type == "cbz" || type == "image"))
	{
		file = path + (type == "pdf" ? ".pdf" : type == "cbz" ? ".cbz" : ".png");
		std::rename(path.c_str(), file.c_str());
	}

	if (type == "pdf")
		GuiImageViewer::showPdf(mWindow, file);
	else if (type == "cbz")
		GuiImageViewer::showCbz(mWindow, file);
	else if (type == "image")
		GuiImageViewer::showImage(mWindow, file, true);
	else if (type == "text")
		mWindow->pushGui(new GuiTextViewer(mWindow, item->label, file));
	else
		mWindow->pushGui(new GuiMsgBox(mWindow, _("Unsupported document") + "\n" + path, _("OK")));
}

void GuiControlCenter::activate(Item* item)
{
	if (item == nullptr || !isEnabled(item))
		return;

	switch (item->kind)
	{
	case CcKind::Tile:
	case CcKind::Chip:
		if (item->doc)
			openDoc(item);
		else if (item->toggle)
		{
			bool on = !isTrue(item->value->value);
			showPending(item->value.get(), on ? "true" : "");
			runAction(item, on ? item->actionOn : item->actionOff, item->afterclick, item->updateIds);
		}
		else if (item->confirm)
		{
			std::string question = Utils::String::endsWith(item->label, "?") ? item->label : item->label + "?";
			auto msgBox = new GuiMsgBox(mWindow, question,
				_("YES"), [this, item] { runAction(item, item->action, item->afterclick, item->updateIds); },
				_("NO"), nullptr);

			msgBox->setDefaultButton(1);
			mWindow->pushGui(msgBox);
		}
		else
			runAction(item, item->action, item->afterclick, item->updateIds);
		break;

	case CcKind::Cycler:
		if (item->pendingIndex >= 0)
		{
			item->applyTimer = 0;
			commitCycler(item);
		}
		else
			openSheet(item);
		break;

	default:
		break;
	}
}

void GuiControlCenter::openSheet(Item* item)
{
	mSheetItem = item;
	mSheetOptions.clear();
	mSheetCursor = 0;
	mSheetScroll = 0;
	mSheetLoading = true;
	item->pendingIndex = -1;

	struct Pending
	{
		std::vector<std::string> labels;
		int left;
	};

	auto pending = std::make_shared<Pending>();
	pending->labels.resize(item->choices.size());
	pending->left = (int)item->choices.size();

	auto finish = [this, item]()
	{
		if (mSheetItem != item)
			return;

		mSheetLoading = false;
		std::string cur = Utils::String::toLower(Utils::String::trim(item->value ? item->value->value : ""));
		for (int i = 0; i < (int)mSheetOptions.size(); i++)
			if (!cur.empty() && Utils::String::toLower(mSheetOptions[i].first) == cur)
				mSheetCursor = i;
	};

	auto addStatic = [this, item, pending, finish]()
	{
		for (int i = 0; i < (int)pending->labels.size(); i++)
			if (!Utils::String::trim(pending->labels[i]).empty())
				mSheetOptions.push_back({ pending->labels[i], std::to_string(i) });
	};

	if (!item->cmdLabels.empty())
	{
		auto labels = std::make_shared<std::string>();
		mShell->expand(item->cmdLabels, item->cacheMs, [this, item, labels, finish, addStatic, pending](const std::string& l)
		{
			*labels = l;
			auto withArgs = [this, item, labels, finish, addStatic, pending](const std::string& args)
			{
				if (mSheetItem != item)
					return;

				auto ls = Utils::String::split(*labels, '\n');
				auto as = Utils::String::split(args, '\n');
				for (size_t i = 0; i < ls.size(); i++)
				{
					std::string action = item->cmdAction;
					if (i < as.size() && !item->cmdArgs.empty())
						action += " " + shellQuote(Utils::String::trim(as[i]));

					if (!Utils::String::trim(ls[i]).empty())
						mSheetOptions.push_back({ Utils::String::trim(ls[i]), "cmd:" + action });
				}

				if (pending->left == 0)
				{
					addStatic();
					finish();
				}
			};

			if (item->cmdArgs.empty())
				withArgs("");
			else
				mShell->expand(item->cmdArgs, item->cacheMs, withArgs);
		});

		if (pending->left == 0)
			return;
	}

	for (int i = 0; i < (int)item->choices.size(); i++)
	{
		mShell->expand(item->choices[i].label, item->cacheMs, [this, item, pending, i, finish, addStatic](const std::string& label)
		{
			pending->labels[i] = label;
			if (--pending->left == 0 && mSheetItem == item && (item->cmdLabels.empty() || !mSheetOptions.empty()))
			{
				addStatic();
				finish();
			}
		});
	}
}

void GuiControlCenter::closeSheet()
{
	mSheetItem = nullptr;
	mSheetOptions.clear();
}

void GuiControlCenter::applySheet(int index)
{
	Item* item = mSheetItem;
	if (!item || index < 0 || index >= (int)mSheetOptions.size())
		return;

	auto option = mSheetOptions[index];
	closeSheet();

	if (item->value)
	{
		showPending(item->value.get(), option.first);
	}

	if (Utils::String::startsWith(option.second, "cmd:"))
		runAction(item, option.second.substr(4), item->cmdAfterclick, item->cmdUpdateIds);
	else
	{
		auto& c = item->choices[atoi(option.second.c_str())];
		runAction(item, c.action, c.afterclick, c.updateIds);
	}
}

bool GuiControlCenter::sheetInput(InputConfig* config, Input input)
{
	if (input.value == 0)
		return true;

	int n = (int)mSheetOptions.size();

	if (config->isMappedTo(BUTTON_BACK, input))
		closeSheet();
	else if (config->isMappedTo(BUTTON_OK, input))
		applySheet(mSheetCursor);
	else if (n > 0 && config->isMappedLike("up", input))
		mSheetCursor = (mSheetCursor - 1 + n) % n;
	else if (n > 0 && config->isMappedLike("down", input))
		mSheetCursor = (mSheetCursor + 1) % n;

	return true;
}

bool GuiControlCenter::input(InputConfig* config, Input input)
{
	mDirty = true;
	if (mClosing)
		return true;

	if (config->getDeviceId() != DEVICE_MOUSE)
		mShowFocus = true;

	if (mSheetItem)
		return sheetInput(config, input);

	bool horizontal = config->isMappedLike("left", input) || config->isMappedLike("right", input);
	if (horizontal && input.value == 0)
	{
		mRepeatDir = 0;
		return true;
	}

	if (input.value == 0)
		return true;

	if (config->isMappedTo(BUTTON_BACK, input) || config->isMappedTo("start", input))
	{
		close();
		return true;
	}

	if (config->isMappedTo("pageup", input) || config->isMappedTo("l2", input))
	{
		selectTab(-1);
		return true;
	}

	if (config->isMappedTo("pagedown", input) || config->isMappedTo("r2", input))
	{
		selectTab(1);
		return true;
	}

	if (config->isMappedLike("up", input))
	{
		moveFocusVertical(-1);
		return true;
	}

	if (config->isMappedLike("down", input))
	{
		moveFocusVertical(1);
		return true;
	}

	if (horizontal)
	{
		int dir = config->isMappedLike("left", input) ? -1 : 1;
		if (mFocusHeader >= 0)
			setHeaderFocus(clampf(mFocusHeader + dir, 0, (int)headerButtons().size() - 1));
		else if (mFocusTabs)
			selectTab(dir);
		else if (mFocus)
		{
			adjust(mFocus, dir);
			if (mFocus && mFocus->kind == CcKind::Slider)
			{
				mRepeatDir = dir;
				mRepeatTime = -380;
			}
		}
		return true;
	}

	if (config->isMappedTo(BUTTON_OK, input))
	{
		auto buttons = headerButtons();
		if (mFocusHeader >= 0 && mFocusHeader < (int)buttons.size())
			activate(buttons[mFocusHeader]);
		else if (mFocusTabs)
			moveFocusVertical(1);
		else
			activate(mFocus);
		return true;
	}

	return true;
}

void GuiControlCenter::update(int deltaTime)
{
	GuiComponent::update(deltaTime);

	if (mClosing)
	{
		mShown = std::max(0.0f, mShown - deltaTime / SLIDE_OUT_MS);
		if (mShown <= 0 && !mClosed)
		{
			mClosed = true;
			if (mOnClosed)
				mOnClosed();
			return;
		}
	}
	else if (mShown < 1.0f)
	{
		mShown = std::min(1.0f, mShown + deltaTime / SLIDE_IN_MS);
		if (mShown >= 1.0f)
			updateHelpPrompts();
	}

	if (mDrag != Drag::Dismiss && mDragOffset < 0 && !mClosing)
	{
		mDragOffset *= std::pow(0.7f, deltaTime / 16.0f);
		if (mDragOffset > -1)
			mDragOffset = 0;
	}

	sampleHiddenGraphs(deltaTime);

	for (auto& b : mBindings)
	{
		if (b->holdMs > 0)
			b->holdMs -= deltaTime;

		if (mClosed || !b->idRef.empty() || b->inflight || !CcShell::hasCommands(b->tmpl))
			continue;

		if (b->tab != -1 && b->tab != mTab)
			continue;

		b->elapsed += deltaTime;
		if (!b->resolved || (b->refreshMs > 0 && b->elapsed >= b->refreshMs))
			request(b);
	}

	for (auto item : mItems)
	{
		if (item->running > 0 && item->value && item->value->holdMs < 300)
			item->value->holdMs = 300;

		if (item->applyTimer > 0)
		{
			item->applyTimer -= deltaTime;
			if (item->applyTimer <= 0)
				commitCycler(item);
		}

		if (item->image)
			item->image->update(deltaTime);
	}

	if (mRepeatDir != 0 && mFocus && mFocus->kind == CcKind::Slider)
	{
		mRepeatTime += deltaTime;
		while (mRepeatTime >= 110)
		{
			mRepeatTime -= 110;
			adjust(mFocus, mRepeatDir);
		}
	}

	if (mLayoutDirty)
		layout();
}

// text caches are keyed on everything but the position
void GuiControlCenter::drawText(const std::shared_ptr<Font>& font, const std::string& text, float x, float y, float w, float h, unsigned int color, int align, const Transform4x4f& trans)
{
	if (text.empty() || w <= 0)
		return;

	std::string key = std::to_string((size_t)font.get()) + "|" + std::to_string(color) + "|" + std::to_string((int)w) + "|" + std::to_string(align) + "|" + text;

	auto it = mTextCache.find(key);
	if (it == mTextCache.cend())
	{
		if (mTextCache.size() > 400)
			mTextCache.clear();

		std::string t = text;
		if (font->sizeText(t, 1.0f).x() > w)
		{
			while (!t.empty() && font->sizeText(t + "...", 1.0f).x() > w)
				t.pop_back();
			t += "...";
		}

		auto cache = std::unique_ptr<TextCache>(font->buildTextCache(t, Vector2f(0, 0), color, w, (Alignment)align, 1.0f));
		it = mTextCache.insert(std::make_pair(key, std::move(cache))).first;
	}

	TextCache* cache = it->second.get();
	Transform4x4f t = trans;
	t.translate(Vector3f(x, y + (h - cache->metrics.size.y()) / 2.0f, 0));
	Renderer::setMatrix(t);
	font->renderTextCache(cache);
}

float GuiControlCenter::drawIcon(const std::shared_ptr<ImageComponent>& icon, float x, float y, float h, unsigned int color, const Transform4x4f& trans)
{
	if (icon->getSize().y() != h)
		icon->setResize(0, h);

	icon->setColorShift(color);
	icon->setPosition(x, y);
	icon->render(trans);
	return icon->getSize().x();
}

void GuiControlCenter::drawSelector(float x, float y, float w, float h, const Transform4x4f& trans)
{
	Renderer::setMatrix(trans);
	Renderer::drawRect(x, y, w, h, mColorSelector, mColorSelectorEnd, mSelectorHorizontal);
}

void GuiControlCenter::renderItem(Item* item, const Transform4x4f& trans, float ox, float oy)
{
	const float u = mUnit;
	float x = ox + item->rect.x;
	float y = oy + item->rect.y;
	float w = (float)item->rect.w;
	float h = (float)item->rect.h;
	bool disabled = !isEnabled(item);
	bool focused = !disabled && mShowFocus && !mFocusTabs && mFocusHeader < 0 && mFocus == item && !mSheetItem;
	unsigned int fg = focused ? mColorSelected : disabled ? withAlpha(mColorText, (mColorText & 0xFF) / 3) : mColorText;

	Renderer::setMatrix(trans);

	switch (item->kind)
	{
	case CcKind::Tile:
	{
		bool on = !disabled && item->toggle && isTrue(item->value->value);
		std::string state = disabled && !item->disabledLabel.empty() ? Utils::String::toUpper(item->disabledLabel) : (on ? _("ON") : _("OFF"));

		if (item->asRow)
		{
			if (focused)
				drawSelector(x, y, w, h, trans);

			float iconW = 0;
			if (item->toggle)
			{
				float ih = 0.9f * u;
				auto& icon = on ? mIconOn : mIconOff;
				if (icon->getSize().y() != ih)
					icon->setResize(0, ih);
				iconW = icon->getSize().x() + mPad;
				drawIcon(icon, x + w - mPad - icon->getSize().x(), y + (h - ih) / 2, ih, fg, trans);
			}

			std::string label = Utils::String::toUpper(item->label);
			if (!item->toggle && !item->subLabel.empty() && label != Utils::String::toUpper(item->subLabel))
			{
				drawText(mFontText, Utils::String::toUpper(item->subLabel), x + w * 0.45f, y, w * 0.55f - mPad - iconW, h, fg, ALIGN_RIGHT, trans);
				drawText(mFontText, label, x + mPad, y, w * 0.45f - mPad, h, fg, ALIGN_LEFT, trans);
			}
			else
				drawText(mFontText, label, x + mPad, y, w - 2 * mPad - iconW, h, fg, ALIGN_LEFT, trans);
			break;
		}

		if (focused)
			drawSelector(x, y, w, h, trans);
		else
		{
			Renderer::setMatrix(trans);
			Renderer::drawRect(x, y, w, h, disabled ? withAlpha(mColorTrack, (mColorTrack & 0xFF) / 2) : mColorTrack);
		}

		float iconW = 0;
		if (item->toggle)
		{
			float ih = 0.95f * u;
			auto& icon = on ? mIconOn : mIconOff;
			if (icon->getSize().y() != ih)
				icon->setResize(0, ih);
			iconW = icon->getSize().x() + mPad;
			drawIcon(icon, x + w - mPad - icon->getSize().x(), y + h * 0.52f + (h * 0.36f - ih) / 2, ih, fg, trans);
		}

		float tw = w - 2 * mPad;
		drawText(mFontText, Utils::String::toUpper(item->label), x + mPad, y + h * 0.12f, tw, h * 0.42f, fg, ALIGN_LEFT, trans);

		std::string sub = item->toggle || (disabled && !item->disabledLabel.empty()) ? state : item->loading ? "..." : Utils::String::toUpper(item->subLabel);
		drawText(mFontSub, sub, x + mPad, y + h * 0.52f, tw - iconW, h * 0.36f, focused ? mColorSelected : disabled ? fg : mColorTextDim, ALIGN_LEFT, trans);
		break;
	}

	case CcKind::Slider:
	{
		if (focused)
			drawSelector(x, y, w, h, trans);

		float tx, tw;
		sliderTrack(item, tx, tw);
		tx += ox;

		drawText(mFontText, Utils::String::toUpper(item->label), x + mPad, y, tx - x - 2 * mPad, h, fg, ALIGN_LEFT, trans);

		float f = sliderFraction(item);
		Renderer::setMatrix(trans);
		Renderer::drawRect(tx, y + h / 2 - 1, tw, 2, fg);

		float ks = 0.9f * u;
		if (mIconKnob->getSize().y() != ks)
			mIconKnob->setResize(0, ks);
		float kw = mIconKnob->getSize().x();
		drawIcon(mIconKnob, tx + tw * f - kw / 2, y + (h - ks) / 2, ks, fg, trans);

		int pct = (int)std::round(item->min + f * (item->max - item->min));
		float valueX = tx + tw + mPad;
		drawText(mFontText, std::to_string(pct) + "%", valueX, y, x + w - mPad - valueX + 1, h, fg, ALIGN_RIGHT, trans);
		break;
	}

	case CcKind::Cycler:
	{
		if (focused)
			drawSelector(x, y, w, h, trans);

		float half = w * 0.45f;
		drawText(mFontText, Utils::String::toUpper(item->label), x + mPad, y, half - mPad, h, fg, ALIGN_LEFT, trans);

		bool pending = item->pendingIndex >= 0 && item->pendingIndex < (int)item->choices.size();
		std::string value = pending ? item->choices[item->pendingIndex].label : (item->value ? item->value->value : "");

		bool inlineChoice = item->cmdLabels.empty() && !item->choices.empty();
		for (auto& c : item->choices)
			inlineChoice &= !CcShell::hasCommands(c.label);

		float ah = 0.7f * u;
		float right = x + w - mPad;
		if (inlineChoice)
		{
			float aw = drawIcon(mIconOptionArrow, right - ah, y + (h - ah) / 2, ah, fg, trans);
			float lx = x + half;
			drawIcon(mIconOptionArrowLeft, lx, y + (h - ah) / 2, ah, fg, trans);
			drawText(mFontText, value, lx + aw + mPad, y, right - aw - lx - aw - 2 * mPad, h, pending && !focused ? mColorSelector : fg, ALIGN_CENTER, trans);
		}
		else
		{
			float aw = drawIcon(mIconArrow, right - ah, y + (h - ah) / 2, ah, fg, trans);
			drawText(mFontText, value, x + half, y, right - aw - mPad - (x + half), h, fg, ALIGN_RIGHT, trans);
		}
		break;
	}

	case CcKind::Info:
	{
		std::string value = item->value ? item->value->value : "";
		if (item->label.empty())
		{
			drawText(mFontText, value, x + mPad, y, w - 2 * mPad, h, mColorText, ALIGN_LEFT, trans);
			break;
		}

		std::string label = Utils::String::toUpper(Utils::String::trim(item->label));
		if (!label.empty() && label.back() == ':')
			label.pop_back();
		drawText(mFontSub, label, x + mPad, y, w * 0.4f, h, mColorTextDim, ALIGN_LEFT, trans);
		if (item->progress)
		{
			float f = 0;
			parseNumber(value, f);
			f = item->max > item->min ? clampf((f - item->min) / (item->max - item->min), 0.0f, 1.0f) : 0;
			float bw = w * 0.45f, bh = 0.2f * u;
			Renderer::setMatrix(trans);
			Renderer::drawRect(x + w - bw - mPad, y + (h - bh) / 2, bw, bh, mColorTrack);
			Renderer::drawRect(x + w - bw - mPad, y + (h - bh) / 2, bw * f, bh, mColorSelector, mColorSelectorEnd, mSelectorHorizontal);
		}
		else
			drawText(mFontText, value, x + w * 0.4f, y, w * 0.6f - mPad, h, mColorText, ALIGN_RIGHT, trans);
		break;
	}

	case CcKind::Graph:
	{
		float headH = mRowH;
		float f = item->history.empty() ? 0 : item->history.back();
		std::string value = item->history.empty() ? "-" : std::to_string((int)std::round(f)) + item->unit;
		float vw = mFontText->sizeText("100" + item->unit, 1.0f).x();

		drawText(mFontText, Utils::String::toUpper(item->label), x + mPad, y, w * 0.4f, headH, mColorText, ALIGN_LEFT, trans);
		const unsigned int warnColor = 0xE5A23AFF;
		bool warning = item->warn >= 0 && !item->history.empty() && f >= item->warn;
		drawText(mFontText, value, x + w - mPad - vw, y, vw, headH, warning ? warnColor : mColorTitle, ALIGN_RIGHT, trans);
		if (item->labelValue)
			drawText(mFontSub, item->labelValue->value, x + w * 0.4f, y, w * 0.6f - 2 * mPad - vw, headH, mColorTextDim, ALIGN_RIGHT, trans);

		float cx = x + mPad;
		float cy = y + headH;
		float cw = w - 2 * mPad;
		float ch = h - headH - 0.3f * u;

		Renderer::setMatrix(trans);
		Renderer::drawRect(cx, cy, cw, ch, mColorTrack);
		for (int g = 1; g < 4; g++)
			Renderer::drawRect(cx, std::round(cy + ch * g / 4), cw, 1, mColorSeparator);

		float top = item->max;
		if (item->autoMax)
		{
			top = item->min + 1;
			for (auto v : item->history)
				top = std::max(top, v * 1.2f);
		}
		float range = top > item->min ? top - item->min : 100;
		float slot = cw / item->historyMax;
		float bar = std::max(1.0f, slot - std::max(1.0f, slot * 0.2f));
		int n = (int)item->history.size();
		for (int k = 0; k < n; k++)
		{
			float v = clampf((item->history[k] - item->min) / range, 0.0f, 1.0f);
			float bh = std::max(1.0f, v * ch);
			float bx = cx + cw - (n - k) * slot;
			if (item->warn >= 0 && item->history[k] >= item->warn)
				Renderer::drawRect(bx, cy + ch - bh, bar, bh, warnColor, withAlpha(warnColor, 0x60), false);
			else
				Renderer::drawRect(bx, cy + ch - bh, bar, bh, mColorSelector, withAlpha(mColorSelectorEnd, 0x60), false);
		}
		break;
	}

	case CcKind::Qr:
	{
		if (item->qrModulesSize == 0)
			break;

		float wanted = item->imageHeight * mRefHeight;
		float scale = std::min(1.0f, std::min(h / wanted, w / item->qrSize));
		float qs = item->qrSize * scale;

		float logoH = item->qrCard && item->qrLogo ? qs / 4 : 0;
		float footH = item->qrCard && !item->qrText.empty() ? qs / 6 : 0;
		float border = item->qrCard ? qs * 0.05f : 0;
		float cardW = qs + 2 * border;
		float cardH = qs + 2 * border + logoH + footH;
		float cx = x + (w - cardW) / 2;
		float cy = y + (h - cardH) / 2;

		Renderer::setMatrix(trans);
		if (item->qrCard)
			Renderer::drawRoundRect(cx, cy, cardW, cardH, cardW / 14, 0x000000FF);

		if (logoH > 0)
		{
			item->qrLogo->setMaxSize(qs, logoH * 0.9f);
			auto ls = item->qrLogo->getSize();
			item->qrLogo->setPosition(cx + (cardW - ls.x()) / 2, cy + border + (logoH - ls.y()) / 2);
			item->qrLogo->render(trans);
		}

		unsigned int bg = item->qrBg;
		float lum = 0.2126f * ((bg >> 24) & 0xFF) + 0.7152f * ((bg >> 16) & 0xFF) + 0.0722f * ((bg >> 8) & 0xFF);
		unsigned int fg = lum < 128 ? 0xFFFFFFFF : 0x000000FF;

		float qx = cx + border;
		float qy = cy + border + logoH;
		int n = item->qrModulesSize;
		float m = qs / n;

		Renderer::setMatrix(trans);
		Renderer::drawRect(qx, qy, qs, qs, bg);
		for (int row = 0; row < n; row++)
		{
			int start = -1;
			for (int col = 0; col <= n; col++)
			{
				bool dark = col < n && item->qrModules[row * n + col];
				if (dark && start < 0)
					start = col;
				else if (!dark && start >= 0)
				{
					Renderer::drawRect(std::floor(qx + start * m), std::floor(qy + row * m), std::ceil((col - start) * m), std::ceil(m), fg);
					start = -1;
				}
			}
		}

		if (footH > 0)
		{
			auto font = Font::get(std::max(8, (int)(footH * 0.6f)), Utils::FileSystem::exists(item->qrFont) ? item->qrFont : mFontText->getPath());
			float tw = font->sizeText(item->qrText, 1.0f).x() + footH * 0.6f;
			float py = qy + qs + border * 0.5f;
			Renderer::setMatrix(trans);
			Renderer::drawRoundRect(cx + (cardW - tw) / 2, py, tw, footH * 0.85f, footH * 0.3f, 0x000064FF);
			drawText(font, item->qrText, cx, py, cardW, footH * 0.85f, 0x9696C8FF, ALIGN_CENTER, trans);
		}
		break;
	}

	case CcKind::Image:
	{
		auto sz = item->image->getSize();
		item->image->setPosition(x + (w - sz.x()) / 2, y + (h - sz.y()) / 2);
		item->image->render(trans);
		break;
	}

	default:
		break;
	}
}


Renderer::Rect GuiControlCenter::sheetRect(int& visibleRows)
{
	int n = std::max(1, (int)mSheetOptions.size());
	float titleH = mFontTitle->getHeight(1.8f);
	float w = std::min(mCard.w * 0.92f, std::max(mCard.w * 0.6f, 20.0f * mUnit));
	float maxRows = std::max(2.0f, std::floor((mCard.h * 0.9f - titleH) / mRowH));
	visibleRows = (int)std::min((float)n, maxRows);
	float h = titleH + visibleRows * mRowH + 0.3f * mUnit;
	return Renderer::Rect((int)((mCard.w - w) / 2), (int)std::max(0.0f, (mCard.h - h) / 2), (int)w, (int)h);
}

void GuiControlCenter::renderSheet(const Transform4x4f& trans)
{
	int visible;
	Renderer::Rect r = sheetRect(visible);
	float titleH = mFontTitle->getHeight(1.8f);

	if (mSheetCursor * mRowH < mSheetScroll)
		mSheetScroll = mSheetCursor * mRowH;
	else if ((mSheetCursor + 1) * mRowH > mSheetScroll + visible * mRowH)
		mSheetScroll = (mSheetCursor + 1) * mRowH - visible * mRowH;

	Renderer::setMatrix(trans);
	Renderer::drawRect(0, 0, (float)mCard.w, (float)mCard.h, 0x00000080);
	Renderer::drawRect((float)r.x - 1, (float)r.y - 1, (float)r.w + 2, (float)r.h + 2, mColorTrack);
	Renderer::drawRect((float)r.x, (float)r.y, (float)r.w, (float)r.h, withAlpha(mColorBg, 0xFF));

	drawText(mFontTitle, Utils::String::toUpper(mSheetItem->label), (float)r.x + mPad, (float)r.y, r.w - 2 * mPad, titleH, mColorTitle, ALIGN_CENTER, trans);

	if (mSheetLoading && mSheetOptions.empty())
	{
		drawText(mFontText, "...", (float)r.x, r.y + titleH, (float)r.w, mRowH, mColorTextDim, ALIGN_CENTER, trans);
		return;
	}

	Vector3f tr = trans.translation();
	Renderer::pushClipRect((int)(tr.x() + r.x), (int)(tr.y() + r.y + titleH), r.w, (int)(visible * mRowH));

	std::string cur = Utils::String::toLower(Utils::String::trim(mSheetItem->value ? mSheetItem->value->value : ""));
	float ih = 0.9f * mUnit;
	for (int i = 0; i < (int)mSheetOptions.size(); i++)
	{
		float ry = r.y + titleH + i * mRowH - mSheetScroll;
		bool selected = i == mSheetCursor && mShowFocus;
		if (selected)
			drawSelector((float)r.x, ry, (float)r.w, mRowH, trans);

		unsigned int fg = selected ? mColorSelected : mColorText;
		bool current = !cur.empty() && Utils::String::toLower(mSheetOptions[i].first) == cur;
		float iconW = 0;
		if (current)
		{
			if (mIconOn->getSize().y() != ih)
				mIconOn->setResize(0, ih);
			iconW = mIconOn->getSize().x() + mPad;
			drawIcon(mIconOn, r.x + r.w - mPad - mIconOn->getSize().x(), ry + (mRowH - ih) / 2, ih, fg, trans);
		}

		drawText(mFontText, mSheetOptions[i].first, r.x + mPad, ry, r.w - 2 * mPad - iconW, mRowH, fg, ALIGN_LEFT, trans);
	}

	Renderer::popClipRect();
}


void GuiControlCenter::render(const Transform4x4f& parentTrans)
{
	Transform4x4f trans = parentTrans * getTransform();

	const float u = mUnit;
	float ease = 1.0f - std::pow(1.0f - mShown, 3.0f);

	if (mFitContent)
	{
		Renderer::setMatrix(trans);
		Renderer::drawRect(0, 0, mSize.x(), mSize.y(), (unsigned int)(0xA0 * ease));
	}

	float offY = -(mCard.y + mCard.h + u) * (1.0f - ease) + std::min(0.0f, mDragOffset);

	Transform4x4f card = trans;
	card.translate(Vector3f((float)mCard.x, mCard.y + offY, 0));

	Renderer::setMatrix(card);
	Renderer::drawRect(0, -2.0f, (float)mCard.w, mCard.h + 2.0f, mColorBg);

	drawText(mFontTitle, _("CONTROL CENTER"), mPad, 0, mCard.w - 2 * mPad, mTitleH, mColorTitle, ALIGN_CENTER, card);

	mTabRects.clear();
	auto tabs = visibleTabs();
	if (tabs.size() > 1)
	{
		std::vector<std::string> labels;
		std::vector<float> widths;
		float total = 0;
		for (auto t : tabs)
		{
			labels.push_back(Utils::String::toUpper(t->label));
			widths.push_back(mFontText->sizeText(labels.back(), 1.0f).x() + 1.6f * u);
			total += widths.back();
		}

		float x = std::max(0.0f, (mCard.w - total) / 2);
		float ty = mTitleH;
		float th = mTabsH - mTitleH;
		for (int i = 0; i < (int)tabs.size(); i++)
		{
			bool active = tabs[i] == mTabs[mTab];
			unsigned int color = active ? mColorTitle : mColorTextDim;

			if (active && mFocusTabs && mShowFocus)
			{
				drawSelector(x, ty, widths[i], th, card);
				color = mColorSelected;
			}
			else if (active)
				drawSelector(x + 0.5f * u, ty + th - 0.2f * u, widths[i] - u, std::max(2.0f, 0.1f * u), card);

			drawText(mFontText, labels[i], x, ty, widths[i], th, color, ALIGN_CENTER, card);
			mTabRects.push_back(Renderer::Rect((int)x, (int)ty, (int)widths[i], (int)th));
			x += widths[i];
		}
	}

	if (mHeaderH > 0)
	{
		const float spacing = 1.2f * u;
		float y = mTabsH;
		float right = mCard.w - mPad;

		// the theme's menuButton image can be too faint to read as a button
		auto buttons = headerButtons();
		for (int i = (int)buttons.size() - 1; i >= 0; i--)
		{
			Item* b = buttons[i];
			float bh = mFontSub->getHeight(1.9f);
			float bw = mFontSub->sizeText(b->buttonText, 1.0f).x() + 2.4f * mPad;
			float bx = right - bw;
			float by = y + (mHeaderH - bh) / 2;
			float r = 0.3f * u;
			bool focused = mShowFocus && mFocusHeader == i && !mSheetItem;

			Renderer::setMatrix(card);
			if (focused)
				Renderer::drawRoundRect(bx, by, bw, bh, r, mColorSelector);
			else
			{
				Renderer::drawRoundRect(bx, by, bw, bh, r, withAlpha(mColorText, 0x90));
				Renderer::drawRoundRect(bx + 1.5f, by + 1.5f, bw - 3, bh - 3, r, withAlpha(mColorBg, 0xFF));
				Renderer::drawRoundRect(bx + 1.5f, by + 1.5f, bw - 3, bh - 3, r, mColorTrack);
			}

			drawText(mFontSub, b->buttonText, bx, by, bw, bh, focused ? mColorSelected : mColorTitle, ALIGN_CENTER, card);
			b->rect = Renderer::Rect((int)bx, (int)y, (int)bw, (int)mHeaderH);
			right = bx - 0.4f * u;
		}

		std::vector<std::pair<Item*, std::string>> parts;
		float total = 0;
		for (auto item : mHeader)
		{
			if (!isVisible(item) || !item->buttonText.empty())
				continue;

			std::string text = item->kind == CcKind::Image ? "" : (item->label.empty() ? "" : item->label + " ") + (item->value ? item->value->value : "");
			float w = item->kind == CcKind::Image ? mFontFooter->getHeight(1.0f) : mFontFooter->sizeText(text, 1.0f).x();
			parts.push_back({ item, text });
			total += w + spacing;
		}

		float left = mPad;
		float x = std::max(left, left + (right - left - total + spacing) / 2);
		for (auto& part : parts)
		{
			Item* item = part.first;
			if (item->kind == CcKind::Image)
			{
				float s = mFontFooter->getHeight(1.0f);
				item->image->setMaxSize(s, s);
				item->image->setPosition(x, y + (mHeaderH - item->image->getSize().y()) / 2);
				item->image->render(card);
				x += s + spacing;
				continue;
			}

			float tw = std::min(mFontFooter->sizeText(part.second, 1.0f).x(), right - x);
			if (tw <= 0)
				break;

			drawText(mFontFooter, part.second, x, y, tw + 1, mHeaderH, mColorFooter, ALIGN_LEFT, card);
			x += tw + spacing;
		}
	}

	Renderer::setMatrix(card);
	Renderer::drawRect(0, mTabsH + mHeaderH - 1, (float)mCard.w, 1, mColorSeparator);

	float top = mTabsH + mHeaderH;
	float viewH = viewHeight();
	Vector3f tr = card.translation();

	int clipY = (int)(tr.y() + top);
	int clipH = (int)viewH;
	if (clipY < 0)
	{
		clipH += clipY;
		clipY = 0;
	}

	if (clipH > 0 && mTab < (int)mTabs.size())
	{
		Renderer::pushClipRect((int)tr.x(), clipY, mCard.w, clipH);
		for (auto item : mTabs[mTab]->items)
		{
			if (item->kind != CcKind::Image && item->kind != CcKind::Qr && item->kind != CcKind::Graph && item->row < 0 && item->kind != CcKind::Info)
				continue;
			if (!isVisible(item))
				continue;

			float y = top - mScroll + item->rect.y;
			if (y + item->rect.h < top || y > top + viewH)
				continue;

			renderItem(item, card, 0, top - mScroll);
		}
		Renderer::popClipRect();

		if (mContentH > viewH)
		{
			float barH = std::max(u, viewH * viewH / mContentH);
			float barY = top + (viewH - barH) * (mScroll / (mContentH - viewH));
			Renderer::setMatrix(card);
			Renderer::drawRect(mCard.w - 0.15f * u, barY, 0.1f * u, barH, mColorScrollbar);
		}
	}

	if (mFooterH > 0)
	{
		std::vector<std::string> texts;
		float total = 0;
		const float spacing = 1.2f * u;
		for (auto item : mFooter)
		{
			if (!isVisible(item))
				continue;

			std::string value = item->value ? item->value->value : "";
			if (item->progress)
				value += "%";

			texts.push_back((item->label.empty() ? "" : item->label + " ") + value);
			total += mFontFooter->sizeText(texts.back(), 1.0f).x() + spacing;
		}

		float x = std::max(mPad, (mCard.w - total + spacing) / 2);
		float y = mCard.h - mHandleH - mHelpH - mFooterH;
		for (auto& t : texts)
		{
			float tw = mFontFooter->sizeText(t, 1.0f).x();
			drawText(mFontFooter, t, x, y, tw + 1, mFooterH, mColorFooter, ALIGN_LEFT, card);
			x += tw + spacing;
		}
	}

	Renderer::setMatrix(card);
	float hw = 2.4f * u, hh = std::max(2.0f, 0.12f * u);
	Renderer::drawRect((mCard.w - hw) / 2, mCard.h - mHandleH / 2 - hh / 2, hw, hh, mColorTrack);

	if (mSheetItem)
		renderSheet(card);
}


Vector2f GuiControlCenter::toLocal(int x, int y)
{
	float ease = 1.0f - std::pow(1.0f - mShown, 3.0f);
	float offY = -(mCard.y + mCard.h + mUnit) * (1.0f - ease) + std::min(0.0f, mDragOffset);
	return Vector2f(x - getPosition().x() - mCard.x, y - getPosition().y() - mCard.y - offY);
}

GuiControlCenter::Item* GuiControlCenter::itemAt(float x, float y)
{
	float top = mTabsH + mHeaderH;

	if (y >= mTabsH && y < top)
	{
		for (auto item : mHeader)
			if (!item->action.empty() && isVisible(item) && x >= item->rect.x && x < item->rect.x + item->rect.w)
				return item;
		return nullptr;
	}

	if (y < top || y > top + viewHeight())
		return nullptr;

	float cy = y - top + mScroll;
	for (auto item : focusables())
		if (x >= item->rect.x && x < item->rect.x + item->rect.w && cy >= item->rect.y && cy < item->rect.y + item->rect.h)
			return item;

	return nullptr;
}


int GuiControlCenter::tabAt(float x, float y)
{
	auto tabs = visibleTabs();
	for (int i = 0; i < (int)mTabRects.size() && i < (int)tabs.size(); i++)
	{
		auto& r = mTabRects[i];
		if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h)
			return (int)(std::find(mTabs.begin(), mTabs.end(), tabs[i]) - mTabs.begin());
	}
	return -1;
}

int GuiControlCenter::sheetIndexAt(float x, float y)
{
	int visible;
	Renderer::Rect r = sheetRect(visible);
	float titleH = mFontTitle->getHeight(1.8f);

	if (x < r.x || x > r.x + r.w || y < r.y || y > r.y + r.h)
		return -2;

	float ly = y - r.y - titleH;
	if (ly < 0 || ly > visible * mRowH)
		return -1;

	int idx = (int)((ly + mSheetScroll) / mRowH);
	return idx < (int)mSheetOptions.size() ? idx : -1;
}


bool GuiControlCenter::hitTest(int x, int y, Transform4x4f& parentTransform, std::vector<GuiComponent*>* pResult)
{
	if (pResult)
		pResult->push_back(this);

	return true;
}

bool GuiControlCenter::onMouseClick(int button, bool pressed, int x, int y)
{
	mDirty = true;
	if (button != 1 || mClosing)
		return true;

	Vector2f p = toLocal(x, y);

	if (pressed)
	{
		mShowFocus = false;
		mDownPos = p;
		mDownScroll = mScroll;
		mDownSheetScroll = mSheetScroll;
		mDownItem = mSheetItem ? nullptr : itemAt(p.x(), p.y());
		mDrag = Drag::Pending;
		mWindow->setMouseCapture(this);
		return true;
	}

	mWindow->releaseMouseCapture();

	Drag drag = mDrag;
	mDrag = Drag::None;

	if (drag == Drag::Slider && mDownItem)
	{
		setSliderTarget(mDownItem, mDownItem->target);
		return true;
	}

	if (drag == Drag::Dismiss)
	{
		if (mDragOffset < -0.2f * mCard.h)
			close();
		return true;
	}

	if (drag != Drag::Pending)
		return true;

	if (mSheetItem)
	{
		int idx = sheetIndexAt(p.x(), p.y());
		if (idx >= 0)
			applySheet(idx);
		else if (idx == -2)
			closeSheet();
		return true;
	}

	if (p.x() < 0 || p.x() > mCard.w || p.y() > mCard.h)
	{
		close();
		return true;
	}

	int tab = tabAt(p.x(), p.y());
	if (tab >= 0)
	{
		selectTabIndex(tab);
		return true;
	}

	Item* item = itemAt(p.x(), p.y());
	if (item == nullptr)
		return true;

	mFocus = item;
	mFocusTabs = false;

	if (item->kind == CcKind::Slider)
	{
		float tx, tw;
		sliderTrack(item, tx, tw);
		float f = clampf((p.x() - tx) / tw, 0.0f, 1.0f);
		setSliderTarget(item, item->min + f * (item->max - item->min));
	}
	else if (item->kind == CcKind::Cycler)
		openSheet(item);
	else
		activate(item);

	return true;
}

void GuiControlCenter::onMouseMove(int x, int y)
{
	if (mDrag != Drag::None)
		mDirty = true;

	if (mDrag == Drag::None)
		return;

	Vector2f p = toLocal(x, y);
	if (mDrag == Drag::Dismiss)
		p.y() -= std::min(0.0f, mDragOffset);

	Vector2f d = p - mDownPos;
	const float u = mUnit;

	if (mDrag == Drag::Pending)
	{
		if (std::abs(d.x()) < 0.35f * u && std::abs(d.y()) < 0.35f * u)
			return;

		float top = mTabsH + mHeaderH;
		float viewH = viewHeight();
		bool onChrome = mDownPos.y() < top || mDownPos.y() > mCard.h - mHandleH - mFooterH;
		bool canScroll = mContentH > viewH;

		if (mSheetItem)
			mDrag = Drag::Scroll;
		else if (mDownItem && mDownItem->kind == CcKind::Slider && std::abs(d.x()) > std::abs(d.y()))
			mDrag = Drag::Slider;
		else if (d.y() < 0 && (onChrome || !canScroll))
			mDrag = Drag::Dismiss;
		else if (std::abs(d.x()) > 1.5f * std::abs(d.y()) && std::abs(d.x()) > 1.2f * u && !onChrome)
		{
			selectTab(d.x() < 0 ? 1 : -1);
			mDrag = Drag::Scroll;
			return;
		}
		else
			mDrag = Drag::Scroll;
	}

	if (mDrag == Drag::Scroll)
	{
		if (mSheetItem)
			mSheetScroll = std::max(0.0f, mDownSheetScroll - d.y());
		else
		{
			float viewH = viewHeight();
			mScroll = clampf(mDownScroll - d.y(), 0.0f, std::max(0.0f, mContentH - viewH));
		}
	}
	else if (mDrag == Drag::Slider && mDownItem)
	{
		Item* item = mDownItem;
		float tx, tw;
		sliderTrack(item, tx, tw);
		float f = clampf((p.x() - tx) / tw, 0.0f, 1.0f);
		item->target = item->min + f * (item->max - item->min);
	}
	else if (mDrag == Drag::Dismiss)
		mDragOffset = std::min(0.0f, d.y());
}

bool GuiControlCenter::onMouseWheel(int delta)
{
	mDirty = true;
	float viewH = viewHeight();
	mScroll = clampf(mScroll - delta * mRowH, 0.0f, std::max(0.0f, mContentH - viewH));
	return true;
}

HelpStyle GuiControlCenter::getHelpStyle()
{
	HelpStyle style = GuiComponent::getHelpStyle();

	// the help font follows the window size, which a compositor can oversize
	style.font = Font::get((int)(mFontText->getSize() * 0.9f), style.font->getPath());

	if (mFitContent)
	{
		float fh = style.font->getHeight(1.0f);
		style.position = Vector2f(mCard.x + mPad, mCard.y + mCard.h - mHandleH - mHelpH + (mHelpH - fh) / 2);
		style.origin = Vector2f(0, 0);
	}
	return style;
}

std::vector<HelpPrompt> GuiControlCenter::getHelpPrompts()
{
	// the window draws the help bar, keep it away while the card moves
	if (mShown < 1.0f || mClosing)
		return {};

	return { HelpPrompt("up/down/left/right", _("CHOOSE")), HelpPrompt(BUTTON_OK, _("SELECT")), HelpPrompt(BUTTON_BACK, _("CLOSE")) };
}
