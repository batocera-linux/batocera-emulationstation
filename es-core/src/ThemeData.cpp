#include "ThemeData.h"

#include "components/ImageComponent.h"
#include "components/TextComponent.h"
#include "components/NinePatchComponent.h"
#include "components/VideoVlcComponent.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "Log.h"
#include "platform.h"
#include "Settings.h"
#include "SystemConf.h"
#include <algorithm>
#include "LocaleES.h"

std::vector<std::string> ThemeData::sSupportedViews { { "system" }, { "basic" }, { "detailed" }, { "grid" }, { "video" }, { "menu" }, { "screen" }, { "splash" } };
std::vector<std::string> ThemeData::sSupportedFeatures { { "video" }, { "carousel" }, { "z-index" }, { "visible" },{ "manufacturer" } };

std::map<std::string, std::map<std::string, ThemeData::ElementPropertyType>> ThemeData::sElementMap {

	{ "splash", {		
		{ "backgroundColor", COLOR } } },
	{ "image", {
		{ "pos", NORMALIZED_PAIR },
		{ "size", NORMALIZED_PAIR },
		{ "maxSize", NORMALIZED_PAIR },
		{ "minSize", NORMALIZED_PAIR },
		{ "origin", NORMALIZED_PAIR },
	 	{ "rotation", FLOAT },
		{ "rotationOrigin", NORMALIZED_PAIR },
		{ "path", PATH },
		{ "default", PATH },
		{ "tile", BOOLEAN },
		{ "color", COLOR },
		{ "colorEnd", COLOR },
		{ "gradientType", STRING },
		{ "visible", BOOLEAN },
		{ "reflexion", NORMALIZED_PAIR },
		{ "reflexionOnFrame", BOOLEAN },
		{ "horizontalAlignment", STRING },		
		{ "verticalAlignment", STRING },
		{ "roundCorners", FLOAT },
		{ "opacity", FLOAT },
		{ "flipX", BOOLEAN },
		{ "flipY", BOOLEAN },
		{ "linearSmooth", BOOLEAN },
		{ "zIndex", FLOAT } } },
	{ "imagegrid", {
		{ "pos", NORMALIZED_PAIR },
		{ "size", NORMALIZED_PAIR },
		{ "margin", NORMALIZED_PAIR },
		{ "padding", NORMALIZED_RECT },
		{ "autoLayout", NORMALIZED_PAIR },
		{ "autoLayoutSelectedZoom", FLOAT },
		{ "animateSelection", BOOLEAN },
		{ "imageSource", STRING }, // image, thumbnail, marquee
		{ "zIndex", FLOAT },
		{ "gameImage", PATH },
		{ "folderImage", PATH },
		{ "showVideoAtDelay", FLOAT },
		{ "scrollDirection", STRING },
		{ "scrollSound", PATH },
		{ "centerSelection", STRING },
		{ "scrollLoop", BOOLEAN } } },
	{ "gridtile", {
		{ "size", NORMALIZED_PAIR },
		{ "padding", NORMALIZED_RECT },
		{ "imageColor", COLOR },
		{ "backgroundImage", PATH },
		{ "backgroundCornerSize", NORMALIZED_PAIR },
		{ "backgroundColor", COLOR },
		{ "backgroundCenterColor", COLOR },
		{ "backgroundEdgeColor", COLOR },
		{ "selectionMode", STRING },
		{ "reflexion", NORMALIZED_PAIR },
		{ "imageSizeMode", STRING } } },
	{ "text", {
		{ "pos", NORMALIZED_PAIR },
		{ "size", NORMALIZED_PAIR },
		{ "origin", NORMALIZED_PAIR },
		{ "rotation", FLOAT },
		{ "rotationOrigin", NORMALIZED_PAIR },
		{ "text", STRING },
		{ "backgroundColor", COLOR },
		{ "fontPath", PATH },
		{ "fontSize", FLOAT },
		{ "color", COLOR },
		{ "alignment", STRING },
		{ "verticalAlignment", STRING },
		{ "forceUppercase", BOOLEAN },
		{ "lineSpacing", FLOAT },
		{ "value", STRING },
		{ "reflexion", NORMALIZED_PAIR },
		{ "reflexionOnFrame", BOOLEAN },
		{ "glowColor", COLOR },
		{ "glowSize", FLOAT },
		{ "glowOffset", NORMALIZED_PAIR },
		{ "singleLineScroll", BOOLEAN },
		{ "padding", NORMALIZED_RECT },
		{ "visible", BOOLEAN },
		{ "zIndex", FLOAT } } },
	{ "textlist", {
		{ "pos", NORMALIZED_PAIR },
		{ "size", NORMALIZED_PAIR },
		{ "origin", NORMALIZED_PAIR },
		{ "selectorHeight", FLOAT },
		{ "selectorOffsetY", FLOAT },
		{ "selectorColor", COLOR },
		{ "selectorColorEnd", COLOR },
		{ "selectorGradientType", STRING },
		{ "selectorImagePath", PATH },
		{ "selectorImageTile", BOOLEAN },
		{ "selectedColor", COLOR },
		{ "primaryColor", COLOR },
		{ "secondaryColor", COLOR },
		{ "fontPath", PATH },
		{ "fontSize", FLOAT },
		{ "scrollSound", PATH },
		{ "alignment", STRING },
		{ "horizontalMargin", FLOAT },
		{ "forceUppercase", BOOLEAN },
		{ "lineSpacing", FLOAT },
		{ "lines", FLOAT },
		{ "zIndex", FLOAT } } },
	{ "container", {
		{ "pos", NORMALIZED_PAIR },
		{ "size", NORMALIZED_PAIR },
	 	{ "origin", NORMALIZED_PAIR },
	 	{ "visible", BOOLEAN },
	 	{ "zIndex", FLOAT } } },
	{ "ninepatch", {
		{ "pos", NORMALIZED_PAIR },
		{ "size", NORMALIZED_PAIR },
		{ "path", PATH },
	 	{ "visible", BOOLEAN },
		{ "color", COLOR },
		{ "cornerSize", NORMALIZED_PAIR },
		{ "centerColor", COLOR },
		{ "edgeColor", COLOR },
		{ "animateColor", COLOR },
		{ "animateColorTime", FLOAT },
		{ "zIndex", FLOAT } } },
	{ "datetime", {
		{ "pos", NORMALIZED_PAIR },
		{ "size", NORMALIZED_PAIR },
		{ "origin", NORMALIZED_PAIR },
		{ "rotation", FLOAT },
		{ "rotationOrigin", NORMALIZED_PAIR },
		{ "backgroundColor", COLOR },
		{ "fontPath", PATH },
		{ "fontSize", FLOAT },
		{ "color", COLOR },
		{ "alignment", STRING },
		{ "forceUppercase", BOOLEAN },
		{ "lineSpacing", FLOAT },
		{ "value", STRING },
		{ "format", STRING },
		{ "displayRelative", BOOLEAN },
	 	{ "visible", BOOLEAN },
	 	{ "zIndex", FLOAT } } },
	{ "rating", {
		{ "pos", NORMALIZED_PAIR },
		{ "size", NORMALIZED_PAIR },
		{ "origin", NORMALIZED_PAIR },
		{ "rotation", FLOAT },
		{ "rotationOrigin", NORMALIZED_PAIR },
		{ "color", COLOR },
		{ "unfilledColor", COLOR },
		{ "filledPath", PATH },
		{ "unfilledPath", PATH },
		{ "visible", BOOLEAN },
		{ "zIndex", FLOAT } } },
	{ "sound", {
		{ "path", PATH } } },
	{ "controllerActivity", {
		{ "pos", NORMALIZED_PAIR },
		{ "size", NORMALIZED_PAIR },
		{ "itemSpacing", FLOAT },
		{ "horizontalAlignment", STRING },
		{ "imagePath", PATH },
		{ "color", COLOR },
		{ "activityColor", COLOR },
		{ "hotkeyColor", COLOR },
		{ "visible", BOOLEAN },
		{ "zIndex", FLOAT } } },
	{ "batteryIndicator", {
		{ "pos", NORMALIZED_PAIR },
		{ "size", NORMALIZED_PAIR },		
		{ "incharge", PATH },
		{ "full", PATH },
		{ "at75", PATH },
		{ "at50", PATH },
		{ "at25", PATH },
		{ "empty", PATH },
		{ "color", COLOR },
		{ "visible", BOOLEAN },
		{ "zIndex", FLOAT } } },
	{ "helpsystem", {
		{ "pos", NORMALIZED_PAIR },
		{ "origin", NORMALIZED_PAIR },
		{ "textColor", COLOR },
		{ "iconColor", COLOR },
		{ "fontPath", PATH },
		{ "fontSize", FLOAT },
		{ "iconUpDown", PATH },
		{ "iconLeftRight", PATH },
		{ "iconUpDownLeftRight", PATH },
		{ "iconA", PATH },
		{ "iconB", PATH },
		{ "iconX", PATH },
		{ "iconY", PATH },
		{ "iconL", PATH },
		{ "iconR", PATH },
		{ "iconStart", PATH },
		{ "iconSelect", PATH } } },
	{ "video", {
		{ "pos", NORMALIZED_PAIR },
		{ "size", NORMALIZED_PAIR },
		{ "maxSize", NORMALIZED_PAIR },
		{ "minSize", NORMALIZED_PAIR },
		{ "origin", NORMALIZED_PAIR },
		{ "rotation", FLOAT },
		{ "rotationOrigin", NORMALIZED_PAIR },
		{ "default", PATH },
		{ "path", PATH },
		{ "delay", FLOAT },
		{ "effect", STRING },
	 	{ "visible", BOOLEAN },
		{ "roundCorners", FLOAT },
		{ "color", COLOR },
	 	{ "zIndex", FLOAT },		
		{ "snapshotSource", STRING }, // image, thumbnail, marquee
		{ "loops", FLOAT }, // Number of loops to do -1 (default) is infinite 
		{ "showSnapshotNoVideo", BOOLEAN },
		{ "showSnapshotDelay", BOOLEAN } } },
	{ "carousel", {
		{ "type", STRING },
		{ "size", NORMALIZED_PAIR },
		{ "pos", NORMALIZED_PAIR },
		{ "origin", NORMALIZED_PAIR },
		{ "color", COLOR },
		{ "colorEnd", COLOR },
		{ "gradientType", STRING },
		{ "logoScale", FLOAT },
		{ "logoRotation", FLOAT },
		{ "logoRotationOrigin", NORMALIZED_PAIR },
		{ "logoSize", NORMALIZED_PAIR },
		{ "logoPos", NORMALIZED_PAIR },
		{ "logoAlignment", STRING },
		{ "maxLogoCount", FLOAT },
		{ "systemInfoDelay", FLOAT },	
		{ "defaultTransition", STRING },
		{ "scrollSound", PATH },
		{ "zIndex", FLOAT } } },
	{ "menuText", {
		{ "fontPath", PATH },
		{ "fontSize", FLOAT },
		{ "separatorColor", COLOR },
		{ "selectorColor", COLOR },
		{ "selectorColorEnd", COLOR },
		{ "selectorGradientType", STRING },
		{ "selectedColor", COLOR },
		{ "color", COLOR } } },
	{ "menuTextSmall", {
		{ "fontPath", PATH },
		{ "fontSize", FLOAT },
		{ "color", COLOR } } },
	{ "menuGroup", {
		{ "fontPath", PATH },
		{ "fontSize", FLOAT },		
		{ "lineSpacing", FLOAT },		
		{ "alignment", STRING },			
		{ "backgroundColor", COLOR },
		{ "separatorColor", COLOR },
		{ "visible", BOOLEAN },
		{ "color", COLOR } } },
	{ "menuBackground", {
		{ "path", PATH },
		{ "fadePath", PATH },
		{ "color", COLOR },
		{ "centerColor", COLOR },
		{ "cornerSize", NORMALIZED_PAIR } } },
	{ "menuIcons", { 		
		{ "iconSystem", PATH },
		{ "iconUpdates", PATH },
		{ "iconControllers", PATH },
		{ "iconGames", PATH },
		{ "iconUI", PATH },
		{ "iconSound", PATH },
		{ "iconNetwork", PATH },
		{ "iconScraper", PATH },
		{ "iconAdvanced", PATH },
		{ "iconQuit", PATH } } },
	{ "menuSwitch",{
		{ "pathOn", PATH },
		{ "pathOff", PATH } } },
	{ "menuTextEdit",{
		{ "active", PATH },
		{ "inactive", PATH } } },
	{ "menuSlider",{
		{ "path", PATH } } },
	{ "menuButton",{
		{ "path", PATH },
		{ "filledPath", PATH } } },
};

std::shared_ptr<ThemeData::ThemeMenu> ThemeData::mMenuTheme;
ThemeData* ThemeData::mDefaultTheme = nullptr;

#define MINIMUM_THEME_FORMAT_VERSION 3
#define CURRENT_THEME_FORMAT_VERSION 6

// helper
unsigned int getHexColor(const char* str)
{
//	ThemeException error;
	if (!str)
	{
		//throw error << "Empty color";
		LOG(LogWarning) << "Empty color";
		return 0;
	}

	size_t len = strlen(str);
	if(len != 6 && len != 8)
	{
		//throw error << "Invalid color (bad length, \"" << str << "\" - must be 6 or 8)";
		LOG(LogWarning) << "Invalid color (bad length, \"" << str << "\" - must be 6 or 8)";
		return 0;
	}

	unsigned int val;
	sscanf(str, "%x", &val);

	if(len == 6)
		val = (val << 8) | 0xFF;

	return val;
}

std::string ThemeData::resolvePlaceholders(const char* in)
{
	if (in == nullptr || in[0] == 0)
		return in;

	auto begin = strstr(in, "${");
	if (begin == nullptr)
		return in;

	auto end = strstr(begin, "}");
	if (end == nullptr)
		return in;

	std::string inStr(in);

	const size_t variableBegin = begin - in;
	const size_t variableEnd = end - in;

	std::string prefix  = inStr.substr(0, variableBegin);
	std::string replace = inStr.substr(variableBegin + 2, variableEnd - (variableBegin + 2));
	std::string suffix = resolvePlaceholders(end + 1);

	return prefix + mVariables[replace] + suffix;
}

ThemeData::ThemeData()
{	
	mColorset = Settings::getInstance()->getString("ThemeColorSet");
	mIconset = Settings::getInstance()->getString("ThemeIconSet");
	mMenu = Settings::getInstance()->getString("ThemeMenu");
	mSystemview = Settings::getInstance()->getString("ThemeSystemView");
	mGamelistview = Settings::getInstance()->getString("ThemeGamelistView");
	mRegion = Settings::getInstance()->getString("ThemeRegionName");
	if (mRegion.empty())
		mRegion = "eu";

	std::string language = SystemConf::getInstance()->get("system.language");
	if (!language.empty())
	{
		auto shortNameDivider = language.find("_");
		if (shortNameDivider != std::string::npos)
			language = Utils::String::toLower(language.substr(0, shortNameDivider));
	}

	if (language.empty())
		language = "en";

	mLanguage = Utils::String::toLower(language);
	mVersion = 0;
}

void ThemeData::loadFile(const std::string system, std::map<std::string, std::string> sysDataMap, const std::string& path, bool fromFile)
{
	mPaths.push_back(path);

	ThemeException error;
	error.setFiles(mPaths);

	if (fromFile && !Utils::FileSystem::exists(path))
		throw error << "File does not exist!";
	
	mVersion = 0;
	mViews.clear();

	mSystemThemeFolder = system;

	mVariables.clear();
	mVariables.insert(sysDataMap.cbegin(), sysDataMap.cend());
	mVariables["lang"] = mLanguage;

	pugi::xml_document doc;
	pugi::xml_parse_result res = fromFile ? doc.load_file(path.c_str()) : doc.load(path.c_str());
	if(!res)
		throw error << "XML parsing error: \n    " << res.description();

	pugi::xml_node root = doc.child("theme");
	if(!root)
		throw error << "Missing <theme> tag!";

	// parse version
	mVersion = root.child("formatVersion").text().as_float(-404);
	if(mVersion == -404)
		throw error << "<formatVersion> tag missing!\n   It's either out of date or you need to add <formatVersion>" << CURRENT_THEME_FORMAT_VERSION << "</formatVersion> inside your <theme> tag.";

	if(mVersion < MINIMUM_THEME_FORMAT_VERSION)
		throw error << "Theme uses format version " << mVersion << ". Minimum supported version is " << MINIMUM_THEME_FORMAT_VERSION << ".";

	parseVariables(root);
	parseTheme(root);
	
	if (system != "splash" && system != "imageviewer")
	{
		mMenuTheme = nullptr;
		mDefaultTheme = this;
	}
}

const std::shared_ptr<ThemeData::ThemeMenu>& ThemeData::getMenuTheme()
{
	if (mMenuTheme == nullptr)
	{
		if (mDefaultTheme != nullptr)
			mMenuTheme = std::shared_ptr<ThemeData::ThemeMenu>(new ThemeMenu(mDefaultTheme));
		else
		{
			auto emptyData = ThemeData();
			mMenuTheme = std::shared_ptr<ThemeData::ThemeMenu>(new ThemeMenu(&emptyData));
		}
	}

	return mMenuTheme;
}

std::string ThemeData::resolveSystemVariable(const std::string& systemThemeFolder, const std::string& path)
{
	size_t start_pos = path.find("$");
	if (start_pos == std::string::npos)
		return path;

	std::string result = path;

	start_pos = result.find("$country");
	if (start_pos != std::string::npos)
		result.replace(start_pos, 8, mRegion);

	start_pos = result.find("$language");
	if (start_pos != std::string::npos)
		result.replace(start_pos, 9, mLanguage);

	start_pos = result.find("$system");
	if (start_pos != std::string::npos)
	{
		result.replace(start_pos, 7, systemThemeFolder);

		if (!Utils::FileSystem::exists(result))
		{
			std::string compatibleFolder = systemThemeFolder;

			if (compatibleFolder == "sg-1000")
				compatibleFolder = "sg1000";
			else if (compatibleFolder == "msx")
				compatibleFolder = "msx1";
			else if (compatibleFolder == "atarilynx")
				compatibleFolder = "lynx";
			else if (compatibleFolder == "atarijaguar")
				compatibleFolder = "jaguar";
			else if (compatibleFolder == "gameandwatch")
				compatibleFolder = "gw";
			else if (compatibleFolder == "amiga")
				compatibleFolder = "amiga600";
			else if (compatibleFolder == "amiga500")
				compatibleFolder = "amiga600";
			else if (compatibleFolder == "auto-favorites")
				compatibleFolder = "favorites";
			else if (compatibleFolder == "thomson")
				compatibleFolder = "to8";
			else if (compatibleFolder == "prboom")
				compatibleFolder = "doom";

			if (compatibleFolder != systemThemeFolder)
			{
				result = path;
				result.replace(start_pos, 7, compatibleFolder);
			}
		}
	}

	return result;
}

bool ThemeData::isFirstSubset(const pugi::xml_node& node)
{
	const std::string subsetToFind = resolvePlaceholders(node.attribute("subset").as_string());
	const std::string name = node.attribute("name").as_string();

	for (const auto& it : mSubsets)
		if (it.subset == subsetToFind)
			return it.name == name;

	return false;
}

bool ThemeData::parseSubset(const pugi::xml_node& node)
{
	if (!node.attribute("subset"))
		return true;

	const std::string subsetAttr = resolvePlaceholders(node.attribute("subset").as_string());
	const std::string nameAttr = resolvePlaceholders(node.attribute("name").as_string());

	if (!subsetAttr.empty())
	{
		std::string displayNameAttr = resolvePlaceholders(node.attribute("displayName").as_string());
		if (displayNameAttr.empty())
			displayNameAttr = nameAttr;

		std::string subSetDisplayNameAttr = resolvePlaceholders(node.attribute("subSetDisplayName").as_string());
		if (subSetDisplayNameAttr.empty())
		{
			std::string byVarName = getVariable("subset." + subsetAttr);
			if (!byVarName.empty())
				subSetDisplayNameAttr = byVarName;
		}

		bool add = true;

		for (auto sb : mSubsets) {
			if (sb.subset == subsetAttr && sb.name == nameAttr) {
				add = false; break;
			}
		}

		if (add)
		{
			Subset subSet(subsetAttr, nameAttr, displayNameAttr, subSetDisplayNameAttr);

			std::string appliesToAttr = resolvePlaceholders(node.attribute("appliesTo").as_string());
			if (!appliesToAttr.empty())
				subSet.appliesTo = Utils::String::splitAny(appliesToAttr, ",");

			mSubsets.push_back(subSet);
		}
	}

	
	if (subsetAttr == "colorset")
	{
		std::string perSystemSetName = Settings::getInstance()->getString("subset." + mSystemThemeFolder + ".colorset");
		if (!perSystemSetName.empty())
		{
			if (nameAttr == perSystemSetName)
				return true;
		}
		else if (nameAttr == mColorset || (mColorset.empty() && isFirstSubset(node)))
			return true;
	}
	else if (subsetAttr == "iconset")
	{
		std::string perSystemSetName = Settings::getInstance()->getString("subset." + mSystemThemeFolder + ".iconset");
		if (!perSystemSetName.empty())
		{
			if (nameAttr == perSystemSetName)
				return true;
		}
		else if (nameAttr == mIconset || (mIconset.empty() && isFirstSubset(node)))
			return true;
	}
	else if (subsetAttr == "menu")
	{
		if (nameAttr == mMenu || (mMenu.empty() && isFirstSubset(node)))
			return true;
	}
	else if (subsetAttr == "systemview")
	{
		if (nameAttr == mSystemview || (mSystemview.empty() && isFirstSubset(node)))
			return true;
	}
	else if (subsetAttr == "gamelistview")
	{
		std::string perSystemSetName = Settings::getInstance()->getString("subset." + mSystemThemeFolder + ".gamelistview");
		if (!perSystemSetName.empty())
		{
			if (nameAttr == perSystemSetName)
				return true;
		}
		else if (nameAttr == mGamelistview || (mGamelistview.empty() && isFirstSubset(node)))
			return true;
	}
	else
	{
		std::string perSystemSetName = Settings::getInstance()->getString("subset." + mSystemThemeFolder + "." + subsetAttr);
		if (!perSystemSetName.empty())
		{
			if (nameAttr == perSystemSetName)
				return true;
		}
		else
		{
			std::string setID = Settings::getInstance()->getString("subset." + subsetAttr);
			if (nameAttr == setID || (setID.empty() && isFirstSubset(node)))
				return true;
		}
	}

	return false;
}



void ThemeData::parseInclude(const pugi::xml_node& node)
{
	if (!parseFilterAttributes(node))
		return;

	if (!parseSubset(node))
		return;

	std::string relPath = resolvePlaceholders(node.text().as_string());
	if (relPath.empty())
		return;

	std::string path = Utils::FileSystem::resolveRelativePath(resolveSystemVariable(mSystemThemeFolder, relPath), Utils::FileSystem::getParent(mPaths.back()), true);

	if (!ResourceManager::getInstance()->fileExists(path))
	{
		if (relPath.find("$") != std::string::npos && relPath.find("${") == std::string::npos)
		{
			path = Utils::FileSystem::resolveRelativePath(resolveSystemVariable("default", relPath), Utils::FileSystem::getParent(mPaths.back()), true);
			if (ResourceManager::getInstance()->fileExists(path))
			{
				if (mPaths.size() == 1)
					mSystemThemeFolder = "default";
			}
			else
			{
				LOG(LogWarning) << "Included file \"" << relPath << "\" not found! (resolved to \"" << path << "\")";
				return;
			}
		}
		else
		{
			LOG(LogWarning) << "Included file \"" << relPath << "\" not found! (resolved to \"" << path << "\")";
			return;
		}
	}

	mPaths.push_back(path);

	pugi::xml_document includeDoc;
	pugi::xml_parse_result result = includeDoc.load_file(path.c_str());
	if (!result)
	{
		LOG(LogWarning) << "Error parsing file: \n    " << result.description() << "    from included file \"" << relPath << "\":\n    ";
		return;
	}

	pugi::xml_node theme = includeDoc.child("theme");
	if (!theme)
	{
		LOG(LogWarning) << "Missing <theme> tag!" << "    from included file \"" << relPath << "\":\n    ";
		return;
	}

	parseVariables(theme);
	parseTheme(theme);
	
	mPaths.pop_back();
}

void ThemeData::parseFeature(const pugi::xml_node& node)
{
	if (!node.attribute("supported"))
	{
		LOG(LogWarning) << "Feature missing \"supported\" attribute!";
		return;
	}

	if (!parseFilterAttributes(node))
		return;

	const std::string supportedAttr = node.attribute("supported").as_string();

	if (supportedAttr == "manufacturer")
	{
		auto it = mVariables.find("system.manufacturer");
		if (it == mVariables.cend() || (*it).second.empty())
			return;
	}

	if (std::find(sSupportedFeatures.cbegin(), sSupportedFeatures.cend(), supportedAttr) != sSupportedFeatures.cend())
		parseViews(node);
}

void ThemeData::parseVariable(const pugi::xml_node& node)
{
	std::string key = node.name();
	if (key.empty())
		return;

	if (!parseFilterAttributes(node))
		return;

	std::string val = node.text().as_string();
	//if (val.empty()) return;
	
	mVariables.erase(key);
	mVariables.insert(std::pair<std::string, std::string>(key, val));	
}

void ThemeData::parseVariables(const pugi::xml_node& root)
{
	// ThemeException error;
	// error.setFiles(mPaths);
    
	for (pugi::xml_node variables = root.child("variables"); variables; variables = variables.next_sibling("variables"))
	{
		if (!parseFilterAttributes(variables))
			continue;

		for (pugi::xml_node_iterator it = variables.begin(); it != variables.end(); ++it)
			parseVariable(*it);
	}
}

void ThemeData::parseViewElement(const pugi::xml_node& node)
{
	if (!node.attribute("name"))
	{
		LOG(LogWarning) << "View missing \"name\" attribute!";
		return;
	}

	const char* delim = " \t\r\n,";
	const std::string nameAttr = node.attribute("name").as_string();
	size_t prevOff = nameAttr.find_first_not_of(delim, 0);
	size_t off = nameAttr.find_first_of(delim, prevOff);
	std::string viewKey;
	while (off != std::string::npos || prevOff != std::string::npos)
	{
		viewKey = nameAttr.substr(prevOff, off - prevOff);
		prevOff = nameAttr.find_first_not_of(delim, off);
		off = nameAttr.find_first_of(delim, prevOff);

		if (std::find(sSupportedViews.cbegin(), sSupportedViews.cend(), viewKey) != sSupportedViews.cend())
		{	
			ThemeView& view = mViews.insert(std::pair<std::string, ThemeView>(viewKey, ThemeView())).first->second;
			parseView(node, view);

			for (auto it = mViews.cbegin(); it != mViews.cend(); ++it)
			{
				if (it->second.isCustomView && it->second.baseType == viewKey)
				{
					ThemeView& customView = (ThemeView&)it->second;
					parseView(node, customView);
				}
			}
		}
	}
}

bool ThemeData::parseFilterAttributes(const pugi::xml_node& node)
{
	if (!parseRegion(node))
		return false;

	if (!parseLanguage(node))
		return false;
	
	if (node.attribute("tinyScreen"))
	{
		const std::string tinyScreenAttr = node.attribute("tinyScreen").as_string();

		if (!Renderer::isSmallScreen() && tinyScreenAttr == "true")
			return false;
		else if (Renderer::isSmallScreen() && tinyScreenAttr == "false")
			return false;
	}

	if (node.attribute("ifHelpPrompts"))
	{
		const std::string helpVisibleAttr = node.attribute("ifHelpPrompts").as_string();
		bool help = Settings::getInstance()->getBool("ShowHelpPrompts");

		if (!help && helpVisibleAttr == "true")
			return false;
		else if (help && helpVisibleAttr == "false")
			return false;
	}

	return true;
}

void ThemeData::parseTheme(const pugi::xml_node& root)
{
	if (root.attribute("defaultView"))
		mDefaultView = root.attribute("defaultView").as_string();

	if (mVersion <= 6)
	{
		// Unfortunately, recalbox does not do things in order, features have to be loaded after
		for (pugi::xml_node node = root.child("include"); node; node = node.next_sibling("include"))
			parseInclude(node);

		for (pugi::xml_node node = root.child("view"); node; node = node.next_sibling("view"))
			parseViewElement(node);

		for (pugi::xml_node node = root.child("customView"); node; node = node.next_sibling("customView"))
			parseCustomView(node, root);

		// Unfortunately, recalbox & retropie don't process elements in order, features have to be loaded last
		for (pugi::xml_node node = root.child("feature"); node; node = node.next_sibling("feature"))
			parseFeature(node);
	}
	else
	{
		for (pugi::xml_node node = root.first_child(); node; node = node.next_sibling())
		{
			if (!parseFilterAttributes(node))
				continue;

			std::string name = node.name();

			if (name == "include")
				parseInclude(node);
			else if (name == "view")
				parseViewElement(node);
			else if (name == "customView")
				parseCustomView(node, root);
			else if (name == "subset")
				parseSubsetElement(node);
			else if (name == "feature")
				parseFeature(node);
		}
	}
}

void ThemeData::parseSubsetElement(const pugi::xml_node& root)
{
	if (!parseFilterAttributes(root))
		return;

	const std::string name = root.attribute("name").as_string();
	const std::string displayName = resolvePlaceholders(root.attribute("displayName").as_string());
	const std::string appliesTo = root.attribute("appliesTo").as_string();

	for (pugi::xml_node node = root.child("include"); node; node = node.next_sibling("include"))
	{
		node.remove_attribute("subset");
		node.append_attribute("subset") = name.c_str();

		if (!appliesTo.empty())
		{
			node.remove_attribute("appliesTo");
			node.append_attribute("appliesTo") = appliesTo.c_str();
		}

		if (!displayName.empty())
		{
			node.remove_attribute("subSetDisplayName");
			node.append_attribute("subSetDisplayName") = displayName.c_str();
		}

		parseInclude(node);
	}
}

void ThemeData::parseViews(const pugi::xml_node& root)
{
	// ThemeException error;
	// error.setFiles(mPaths);

	// parse views
	for (pugi::xml_node node = root.child("view"); node; node = node.next_sibling("view"))
		parseViewElement(node);	
}

void ThemeData::parseCustomViewBaseClass(const pugi::xml_node& root, ThemeView& view, std::string baseClass)
{	
	auto baseviewit = mViews.find(baseClass);
	if (baseviewit == mViews.cend())
		return;
	
	// Avoid recursion
	if (std::find(view.baseTypes.cbegin(), view.baseTypes.cend(), baseClass) != view.baseTypes.cend())
		return;

	view.baseType = baseClass;
	view.baseTypes.push_back(baseClass);

	ThemeView& baseView = baseviewit->second;
	if (!baseView.baseType.empty())
		parseCustomViewBaseClass(root, view, baseView.baseType);

	for (auto element : baseView.elements)
	{
		view.elements.erase(element.first);			
		view.elements.insert(std::pair<std::string, ThemeElement>(element.first, element.second));

		if (std::find(view.orderedKeys.cbegin(), view.orderedKeys.cend(), element.first) == view.orderedKeys.cend())
			view.orderedKeys.push_back(element.first);
	}	
}

void ThemeData::parseCustomView(const pugi::xml_node& node, const pugi::xml_node& root)
{
	if (!node.attribute("name"))
		return;

	if (!parseFilterAttributes(node))
		return;

	std::string viewKey = node.attribute("name").as_string();
	std::string inherits = node.attribute("inherits").as_string();

	if (viewKey.find(",") != std::string::npos && inherits.empty())
	{
		for (auto name : Utils::String::split(viewKey, ','))
		{
			std::string trim = Utils::String::trim(name);
			if (mViews.find(trim) != mViews.cend())
			{
				ThemeView& view = mViews.insert(std::pair<std::string, ThemeView>(trim, ThemeView())).first->second;

				if (node.attribute("displayName"))
					view.displayName = resolvePlaceholders(node.attribute("displayName").as_string());

				parseView(node, view);
			}
		}

		return;
	}

	ThemeView& view = mViews.insert(std::pair<std::string, ThemeView>(viewKey, ThemeView())).first->second;

	if (node.attribute("displayName"))
		view.displayName = resolvePlaceholders(node.attribute("displayName").as_string());
	else if (view.displayName.empty())
		view.displayName = viewKey;

	view.isCustomView = true;

	if (!inherits.empty())
		parseCustomViewBaseClass(root, view, inherits);

	parseView(node, view);
}

void ThemeData::parseView(const pugi::xml_node& root, ThemeView& view, bool overwriteElements)
{
	// ThemeException error;
	// error.setFiles(mPaths);

	if (!parseFilterAttributes(root))
		return;

	for(pugi::xml_node node = root.first_child(); node; node = node.next_sibling())
	{
		if(!node.attribute("name"))
		{		
			LOG(LogWarning) << "Element of type \"" << node.name() << "\" missing \"name\" attribute!";
			continue;
		}		

		auto elemTypeIt = sElementMap.find(node.name());
		if(elemTypeIt == sElementMap.cend())
		{		
			LOG(LogWarning) << "Unknown element of type \"" << node.name() << "\"!";
			continue;
		}		

		if (!parseFilterAttributes(node))
			continue;
		
		const char* delim = " \t\r\n,";
		const std::string nameAttr = node.attribute("name").as_string();
		size_t prevOff = nameAttr.find_first_not_of(delim, 0);
		size_t off = nameAttr.find_first_of(delim, prevOff);
		while (off != std::string::npos || prevOff != std::string::npos)
		{
			std::string elemKey = nameAttr.substr(prevOff, off - prevOff);
			prevOff = nameAttr.find_first_not_of(delim, off);
			off = nameAttr.find_first_of(delim, prevOff);

			parseElement(node, elemTypeIt->second,
				view.elements.insert(std::pair<std::string, ThemeElement>(elemKey, ThemeElement())).first->second, overwriteElements);

			if (std::find(view.orderedKeys.cbegin(), view.orderedKeys.cend(), elemKey) == view.orderedKeys.cend())
				view.orderedKeys.push_back(elemKey);
		}		
	}
}

bool ThemeData::parseLanguage(const pugi::xml_node& node)
{
	if (!node.attribute("lang"))
		return true;

	const std::string nameAttr = Utils::String::toLower(node.attribute("lang").as_string());
	if (nameAttr.empty() || nameAttr == "default")
		return true;

	const char* delim = " \t\r\n,";

	size_t prevOff = nameAttr.find_first_not_of(delim, 0);
	size_t off = nameAttr.find_first_of(delim, prevOff);
	while (off != std::string::npos || prevOff != std::string::npos)
	{
		std::string elemKey = nameAttr.substr(prevOff, off - prevOff);
		prevOff = nameAttr.find_first_not_of(delim, off);
		off = nameAttr.find_first_of(delim, prevOff);
		if (elemKey == mLanguage)
			return true;
	}

	return false;
}

bool ThemeData::parseRegion(const pugi::xml_node& node)
{
	if (!node.attribute("region"))
		return true;

	const std::string nameAttr = Utils::String::toLower(node.attribute("region").as_string());	
	if (nameAttr.empty() || nameAttr == "default")
		return true;

	bool add = true;

	for (auto sb : mSubsets) {
		if (sb.subset == "region" && sb.name == nameAttr) {
			add = false; break;
		}
	}

	if (add)
		mSubsets.push_back(Subset("region", nameAttr, nameAttr, "region"));

	const char* delim = " \t\r\n,";
	
	size_t prevOff = nameAttr.find_first_not_of(delim, 0);
	size_t off = nameAttr.find_first_of(delim, prevOff);
	while (off != std::string::npos || prevOff != std::string::npos)
	{
		std::string elemKey = nameAttr.substr(prevOff, off - prevOff);
		prevOff = nameAttr.find_first_not_of(delim, off);
		off = nameAttr.find_first_of(delim, prevOff);
		if (elemKey == mRegion)
			return true;
	}

	return false;
}

void ThemeData::parseElement(const pugi::xml_node& root, const std::map<std::string, ElementPropertyType>& typeMap, ThemeElement& element, bool overwrite)
{
	// ThemeException error;
	// error.setFiles(mPaths);

	element.type = root.name();

	if (root.attribute("extra"))
	{
		std::string extra = Utils::String::toLower(root.attribute("extra").as_string());
		
		if (extra == "true")
			element.extra = 1;
		else if (extra == "static")
			element.extra = 2;
	}	

	for(pugi::xml_node node = root.first_child(); node; node = node.next_sibling())
	{
		if (!parseFilterAttributes(node))
			continue;

		ElementPropertyType type = STRING;

		auto typeIt = typeMap.find(node.name());
		if(typeIt == typeMap.cend())
		{
			// Exception for menuIcons that can be extended
			if (element.type == "menuIcons")
				type = PATH;
			else if (std::string(node.name()) == "animate" && std::string(root.name()) == "imagegrid")
				node.set_name("animateSelection");
			else
			{
				LOG(LogWarning) << "Unknown property type \"" << node.name() << "\" (for element of type " << root.name() << ").";
				continue;
			}
		}
		else
			type = typeIt->second;
		
		if (!overwrite && element.properties.find(node.name()) != element.properties.cend())
			continue;

		std::string str = resolveSystemVariable(mSystemThemeFolder, resolvePlaceholders(node.text().as_string()));

		switch(type)
		{
		case NORMALIZED_RECT:
		{
			Vector4f val;

			auto splits = Utils::String::split(str, ' ');
			if (splits.size() == 2)
			{
				val = Vector4f((float)atof(splits.at(0).c_str()), (float)atof(splits.at(1).c_str()),
					(float)atof(splits.at(0).c_str()), (float)atof(splits.at(1).c_str()));
			}
			else if (splits.size() == 4)
			{
				val = Vector4f((float)atof(splits.at(0).c_str()), (float)atof(splits.at(1).c_str()),
					(float)atof(splits.at(2).c_str()), (float)atof(splits.at(3).c_str()));
			}

			element.properties[node.name()] = val;
			break;
		}
		case NORMALIZED_PAIR:
		{
			size_t divider = str.find(' ');
			if(divider == std::string::npos) 
			{			
				if (str.empty())
				{
					LOG(LogWarning) << "invalid normalized pair (property \"" << node.name() << "\", value \"" << str.c_str() << "\")";
					break;
				}

				Vector2f val((float)atof(str.c_str()), (float)atof(str.c_str()));
				element.properties[node.name()] = val;
				break;
			}			

			float first = atof(str.substr(0, divider).c_str());
			float second = atof(str.substr(divider, std::string::npos).c_str());
			element.properties[node.name()] = Vector2f(first, second);
			break;
		}
		case STRING:
			element.properties[node.name()] = str;
			break;
		case PATH:
		{
			if (str.empty())
				break;

			std::string path = Utils::FileSystem::resolveRelativePath(str, Utils::FileSystem::getParent(mPaths.back()), true);
			
			if (Utils::String::startsWith(path, "{random"))
			{
				pugi::xml_node parent = root.parent();

				if (!element.extra)
					LOG(LogWarning) << "random is only supported in extras";
				else if (element.type != "image" && element.type != "video")
					LOG(LogWarning) << "random is only supported in video or image elements";
				else if (std::string(parent.name()) != "view" || std::string(parent.attribute("name").as_string()) != "system")
					LOG(LogWarning) << "random is only supported in systemview";
				else if (element.type == "video" && path != "{random}")
					LOG(LogWarning) << "video element only supports {random} element";
				else if (element.type == "image" && path != "{random}" && path != "{random:thumbnail}" && path != "{random:marquee}" && path != "{random:image}")
					LOG(LogWarning) << "unknow random element " << path;
				else
					element.properties[node.name()] = path;

				break;
			}

			if (path[0] == '/')
			{
#if WIN32 || defined _ENABLEEMUELEC
				path = Utils::String::replace(path,
					"/recalbox/share_init/system/.emulationstation/themes",
					Utils::FileSystem::getEsConfigPath() + "/themes");
#else
				path = Utils::String::replace(path,
					"/recalbox/share_init/system/.emulationstation/themes",
					"/userdata/themes");
#endif
			}

			if (!ResourceManager::getInstance()->fileExists(path))
			{
				std::string rootPath = Utils::FileSystem::resolveRelativePath(str, Utils::FileSystem::getParent(mPaths.front()), true);
				if (rootPath != path && ResourceManager::getInstance()->fileExists(rootPath))
					path = rootPath;
			}

			if(!ResourceManager::getInstance()->fileExists(path))
			{
				std::stringstream ss;
				ss << "Warning : could not find file \"" << node.text().get() << "\" ";
				if(node.text().get() != path)
					ss << "(which resolved to \"" << path << "\") ";
				LOG(LogWarning) << ss.str();
			}
			else
				element.properties[node.name()] = path;

			break;
		}
		case COLOR:
			element.properties[node.name()] = getHexColor(str.c_str());
			break;
		case FLOAT:
		{
			element.properties[node.name()] = (float) atof(str.c_str());
			break;
		}

		case BOOLEAN:
		{
			// only look at first char
			char first = str[0];
			// 1*, t* (true), T* (True), y* (yes), Y* (YES)
			bool boolVal = (first == '1' || first == 't' || first == 'T' || first == 'y' || first == 'Y');

			element.properties[node.name()] = boolVal;
			break;
		}
		default:
			LOG(LogWarning) << "Unknown ElementPropertyType for \"" << root.attribute("name").as_string() << "\", property " << node.name();
			break;
		}
	}
}

std::string ThemeData::getCustomViewBaseType(const std::string& view)
{
	auto viewIt = mViews.find(view);
	if (viewIt != mViews.cend())
		return viewIt->second.baseType;

	return "";
}

std::string ThemeData::getViewDisplayName(const std::string& view)
{
	auto viewIt = mViews.find(view);
	if (viewIt != mViews.cend())
	{
		if (viewIt->second.displayName.empty())
			return _(view.c_str());

		return viewIt->second.displayName;
	}
	
	return view;
}

bool ThemeData::isCustomView(const std::string& view)
{
	auto viewIt = mViews.find(view);
	if (viewIt != mViews.cend())
		return viewIt->second.isCustomView;

	return false;
}

bool ThemeData::hasView(const std::string& view)
{
	auto viewIt = mViews.find(view);
	return (viewIt != mViews.cend());
}

const ThemeData::ThemeElement* ThemeData::getElement(const std::string& view, const std::string& element, const std::string& expectedType) const
{
	auto viewIt = mViews.find(view);
	if(viewIt == mViews.cend())
		return NULL; // not found

	auto elemIt = viewIt->second.elements.find(element);
	if(elemIt == viewIt->second.elements.cend()) return NULL;

	if(elemIt->second.type != expectedType && !expectedType.empty())
	{
		LOG(LogWarning) << " requested mismatched theme type for [" << view << "." << element << "] - expected \"" 
			<< expectedType << "\", got \"" << elemIt->second.type << "\"";
		return NULL;
	}

	return &elemIt->second;
}

const std::shared_ptr<ThemeData>& ThemeData::getDefault()
{
	static std::shared_ptr<ThemeData> theme = nullptr;
	if(theme == nullptr)
	{
		theme = std::shared_ptr<ThemeData>(new ThemeData());

		const std::string path = Utils::FileSystem::getEsConfigPath() + "/es_theme_default.xml";
		if(Utils::FileSystem::exists(path))
		{
			try
			{
				std::map<std::string, std::string> emptyMap;
				theme->loadFile("", emptyMap, path);
			} catch(ThemeException& e)
			{
				LOG(LogError) << e.what();
				theme = std::shared_ptr<ThemeData>(new ThemeData()); //reset to empty
			}
		}
	}

	return theme;
}

std::vector<GuiComponent*> ThemeData::makeExtras(const std::shared_ptr<ThemeData>& theme, const std::string& view, Window* window, bool forceLoad)
{
	std::vector<GuiComponent*> comps;

	auto viewIt = theme->mViews.find(view);
	if(viewIt == theme->mViews.cend())
		return comps;
	
	for(auto it = viewIt->second.orderedKeys.cbegin(); it != viewIt->second.orderedKeys.cend(); it++)
	{
		ThemeElement& elem = viewIt->second.elements.at(*it);
		if(elem.extra)
		{
			GuiComponent* comp = nullptr;

			const std::string& t = elem.type;
			if(t == "image")
				comp = new ImageComponent(window, forceLoad);
			else if(t == "text")
				comp = new TextComponent(window);
			else if (t == "ninepatch")
				comp = new NinePatchComponent(window);
			else if (t == "video")
				comp = new VideoVlcComponent(window);

			if (comp == nullptr)
				continue;

			comp->setIsStaticExtra(elem.extra == 2);
			comp->setTag((*it).c_str());
			comp->setDefaultZIndex(10);
			comp->applyTheme(theme, view, *it, ThemeFlags::ALL);
			comps.push_back(comp);
		}
	}

	return comps;
}

std::map<std::string, ThemeSet> ThemeData::getThemeSets()
{
	std::map<std::string, ThemeSet> sets;

	static const size_t pathCount = 3;
	std::string paths[pathCount] =
	{ 
		"/etc/emulationstation/themes",
		Utils::FileSystem::getEsConfigPath() + "/themes",
		"/userdata/themes" // batocera
	};

	for(size_t i = 0; i < pathCount; i++)
	{
		if(!Utils::FileSystem::isDirectory(paths[i]))
			continue;

		Utils::FileSystem::stringList dirContent = Utils::FileSystem::getDirContent(paths[i]);

		for(Utils::FileSystem::stringList::const_iterator it = dirContent.cbegin(); it != dirContent.cend(); ++it)
		{
			if(Utils::FileSystem::isDirectory(*it))
			{
				ThemeSet set = {*it};
				sets[set.getName()] = set;
			}
		}
	}

	return sets;
}

std::string ThemeData::getThemeFromCurrentSet(const std::string& system)
{
	std::map<std::string, ThemeSet> themeSets = ThemeData::getThemeSets();
	if(themeSets.empty())
	{
		// no theme sets available
		return "";
	}

	std::map<std::string, ThemeSet>::const_iterator set = themeSets.find(Settings::getInstance()->getString("ThemeSet"));
	if(set == themeSets.cend())
	{
		// currently selected theme set is missing, so just pick the first available set
		set = themeSets.cbegin();
		Settings::getInstance()->setString("ThemeSet", set->first);
	}

	return set->second.getThemePath(system);
}

ThemeData::ThemeMenu::ThemeMenu(ThemeData* theme)
{
	Title.font = Font::get(FONT_SIZE_LARGE);
	Footer.font = Font::get(FONT_SIZE_SMALL);
	Text.font = Font::get(FONT_SIZE_MEDIUM);
	TextSmall.font = Font::get(FONT_SIZE_SMALL);
	Group.font = Font::get(FONT_SIZE_SMALL);

	auto elem = theme->getElement("menu", "menubg", "menuBackground");
	if (elem)
	{
		if (elem->has("path") && ResourceManager::getInstance()->fileExists(elem->get<std::string>("path")))
			Background.path = elem->get<std::string>("path");

		if (elem->has("fadePath") && ResourceManager::getInstance()->fileExists(elem->get<std::string>("fadePath")))
			Background.fadePath = elem->get<std::string>("fadePath");

		if (elem->has("color"))
		{
			Background.color = elem->get<unsigned int>("color");
			Background.centerColor = Background.color;
		}

		if (elem->has("centerColor"))
		{
			Background.centerColor = elem->get<unsigned int>("centerColor");
			if (!elem->has("color"))
				Background.color = Background.centerColor;
		}

		if (elem->has("cornerSize"))
			Background.cornerSize = elem->get<Vector2f>("cornerSize");
	}

	elem = theme->getElement("menu", "menutitle", "menuText");
	if (elem)
	{
		if (elem->has("fontPath") || elem->has("fontSize"))
			Title.font = Font::getFromTheme(elem, ThemeFlags::ALL, Font::get(FONT_SIZE_LARGE));
		if (elem->has("color"))
			Title.color = elem->get<unsigned int>("color");
		if (elem->has("selectorColor"))
			Title.selectorColor = elem->get<unsigned int>("selectorColor");
	}

	elem = theme->getElement("menu", "menufooter", "menuText");
	if (elem)
	{
		if (elem->has("fontPath") || elem->has("fontSize"))
			Footer.font = Font::getFromTheme(elem, ThemeFlags::ALL, Font::get(FONT_SIZE_SMALL));
		if (elem->has("color"))
			Footer.color = elem->get<unsigned int>("color");
		if (elem->has("selectorColor"))
			Footer.selectorColor = elem->get<unsigned int>("selectorColor");
	}

	elem = theme->getElement("menu", "menutextsmall", "menuTextSmall");
	if (elem)
	{		
		Group.visible = true;

		if (elem->has("fontPath") || elem->has("fontSize"))
		{
			TextSmall.font = Font::getFromTheme(elem, ThemeFlags::ALL, Font::get(FONT_SIZE_SMALL));
			Group.font = TextSmall.font;
		}

		if (elem->has("color"))
		{
			TextSmall.color = elem->get<unsigned int>("color");
			Group.color = TextSmall.color;
		}

		if (elem->has("selectedColor"))
			Text.selectedColor = elem->get<unsigned int>("selectedColor");
		if (elem->has("selectorColor"))
			Text.selectedColor = elem->get<unsigned int>("selectorColor");
	}

	elem = theme->getElement("menu", "menutext", "menuText");
	if (elem)
	{
		if (elem->has("fontPath") || elem->has("fontSize"))
			Text.font = Font::getFromTheme(elem, ThemeFlags::ALL, Font::get(FONT_SIZE_MEDIUM));

		if (elem->has("color"))
			Text.color = elem->get<unsigned int>("color");
		if (elem->has("separatorColor"))
		{			
			Text.separatorColor = elem->get<unsigned int>("separatorColor");
			Group.separatorColor = Text.separatorColor;
		}
		if (elem->has("selectedColor"))
			Text.selectedColor = elem->get<unsigned int>("selectedColor");
		if (elem->has("selectorColor"))
		{
			Text.selectorColor = elem->get<unsigned int>("selectorColor");
			Text.selectorGradientColor = Text.selectorColor;
		}
		if (elem->has("selectorColorEnd"))
			Text.selectorGradientColor = elem->get<unsigned int>("selectorColorEnd");
		if (elem->has("selectorGradientType"))
			Text.selectorGradientType = (elem->get<std::string>("selectorGradientType").compare("horizontal"));
	}


	elem = theme->getElement("menu", "menugroup", "menuGroup");
	if (elem)
	{
		Group.visible = true;

		if (elem->has("fontPath") || elem->has("fontSize"))
			Group.font = Font::getFromTheme(elem, ThemeFlags::ALL, Font::get(FONT_SIZE_SMALL));
		if (elem->has("color"))
			Group.color = elem->get<unsigned int>("color");
		if (elem->has("backgroundColor"))
			Group.backgroundColor = elem->get<unsigned int>("backgroundColor");
		if (elem->has("separatorColor"))
			Group.separatorColor = elem->get<unsigned int>("separatorColor");
		if (elem->has("lineSpacing"))
			Group.lineSpacing = elem->get<float>("lineSpacing");
		if (elem->has("visible"))
			Group.visible = elem->get<bool>("visible");

		if (elem->has("alignment"))
		{
			std::string str = elem->get<std::string>("alignment");
			if (str == "left")
				Group.alignment = ALIGN_LEFT;
			else if (str == "center")
				Group.alignment = ALIGN_CENTER;
			else if (str == "right")
				Group.alignment = ALIGN_RIGHT;
		}
	}

	elem = theme->getElement("menu", "menubutton", "menuButton");
	if (elem)
	{
		if (elem->has("path"))
			Icons.button = elem->get<std::string>("path");
		if (elem->has("filledPath"))
			Icons.button_filled = elem->get<std::string>("filledPath");
	}

	elem = theme->getElement("menu", "menutextedit", "menuTextEdit");
	if (elem)
	{
		if (elem->has("active") && ResourceManager::getInstance()->fileExists(elem->get<std::string>("active")))
			Icons.textinput_ninepatch_active = elem->get<std::string>("active");
		if (elem->has("inactive") && ResourceManager::getInstance()->fileExists(elem->get<std::string>("inactive")))
			Icons.textinput_ninepatch = elem->get<std::string>("inactive");
	}

	elem = theme->getElement("menu", "menuswitch", "menuSwitch");
	if (elem)
	{
		if (elem->has("pathOn") && ResourceManager::getInstance()->fileExists(elem->get<std::string>("pathOn")))
			Icons.on = elem->get<std::string>("pathOn");
		if (elem->has("pathOff") && ResourceManager::getInstance()->fileExists(elem->get<std::string>("pathOff")))
			Icons.off = elem->get<std::string>("pathOff");
	}

	elem = theme->getElement("menu", "menuslider", "menuSlider");
	if (elem && elem->has("path") && ResourceManager::getInstance()->fileExists(elem->get<std::string>("path")))
		Icons.knob = elem->get<std::string>("path");

	elem = theme->getElement("menu", "menuicons", "menuIcons");
	if (elem)
	{
		for (auto prop : elem->properties)
		{
			std::string path = prop.second.s;
			if (!path.empty() && ResourceManager::getInstance()->fileExists(path))
				mMenuIcons[prop.first] = path;
		}
	}
}

void ThemeData::setDefaultTheme(ThemeData* theme) 
{ 
	mDefaultTheme = theme; 
	mMenuTheme = nullptr;
};

std::vector<std::pair<std::string, std::string>> ThemeData::getViewsOfTheme()
{
	std::vector<std::pair<std::string, std::string>> ret;
	for (auto it = mViews.cbegin(); it != mViews.cend(); ++it)
	{
		if (it->first == "menu" || it->first == "system" || it->first == "screen")
			continue;

		ret.push_back(std::pair<std::string, std::string>(it->first, it->second.displayName.empty() ? it->first : it->second.displayName));
	}

	return ret;
}

std::vector<Subset> ThemeData::getSubSet(const std::vector<Subset>& subsets, const std::string& subset)
{
	std::vector<Subset> ret;

	for (const auto& it : subsets)
		if (it.subset == subset)
			ret.push_back(it);

	return ret;
}

std::vector<std::string> ThemeData::getSubSetNames(const std::string ofView)
{
	std::vector<std::string> ret;

	for (const auto& it : mSubsets)
	{
		if (std::find(ret.cbegin(), ret.cend(), it.subset) == ret.cend())
		{
			if (ofView.empty())
				ret.push_back(it.subset);
			else
			{
				if (std::find(it.appliesTo.cbegin(), it.appliesTo.cend(), ofView) != it.appliesTo.cend())
					ret.push_back(it.subset);
			/*	else
				{
					auto viewIt = mViews.find(ofView);
					if (viewIt != mViews.cend())
					{					
						for (auto applyTo : it.appliesTo)
						{
							if (viewIt->second.isOfType(applyTo))
							{
								ret.push_back(it.subset);
								break;
							}
						}
					}
				}*/
			}
		}
	}

	return ret;
}

std::string	ThemeData::getDefaultSubSetValue(const std::string subsetname)
{
	for (const auto& it : mSubsets)
		if (it.subset == subsetname)
			return it.name;

	return "";
}

bool ThemeData::ThemeView::isOfType(const std::string type)
{
	return baseType == type || std::find(baseTypes.cbegin(), baseTypes.cend(), type) != baseTypes.cend();
};