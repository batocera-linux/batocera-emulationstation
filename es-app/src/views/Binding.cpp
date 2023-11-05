#include "Binding.h"
#include "SystemData.h"
#include "FileData.h"
#include "utils/StringUtil.h"
#include "LocaleES.h"
#include <time.h>

#include "components/TextComponent.h"
#include "components/ImageComponent.h"
#include "components/VideoComponent.h"
#include "components/StackPanelComponent.h"

#include "utils/Platform.h"
#include "SystemConf.h"
#include "utils/MathExpr.h"

static std::string valueOrUnknown(const std::string& value) 
{ 
	return value.empty() ? _("Unknown") : value == "0" ? _("None") : value;
};

static std::string getGlobalProperty(const std::string& name)
{
	if (name == "help")
		return Settings::getInstance()->getBool("ShowHelpPrompts") ? _("YES") : _("NO");
	
	if (name == "clock")
		return Settings::DrawClock() ? _("YES") : _("NO");

	if (name == "architecture")
		return Utils::Platform::getArchString();

	if (name == "cheevos")
		return SystemConf::getInstance()->getBool("global.retroachievements") ? _("YES") : _("NO");

	if (name == "cheevosUser")
		return SystemConf::getInstance()->getBool("global.retroachievements") ? SystemConf::getInstance()->get("global.retroachievements.username") : "";

	if (name == "netplay")
		return SystemConf::getInstance()->getBool("global.netplay") ? "true" : "false";

	if (name == "netplay.username")
		return SystemConf::getInstance()->getBool("global.netplay") ? SystemConf::getInstance()->get("global.netplay.nickname") : "";

	if (name == "ip")
		return Utils::Platform::queryIPAdress();

	if (name == "network")
		return !Utils::Platform::queryIPAdress().empty() ? _("YES") : _("NO");

	if (name == "battery") 
		return Utils::Platform::queryBatteryInformation().hasBattery ? _("YES") : _("NO");

	if (name == "batteryLevel")
		return std::to_string(Utils::Platform::queryBatteryInformation().level);

	if (name == "screenWidth" || name == "width")
		return std::to_string(Renderer::getScreenWidth());

	if (name == "screenHeight" || name == "height")
		return std::to_string(Renderer::getScreenHeight());

	if (name == "screenRatio" || name == "ratio")
		return Renderer::getAspectRatio();

	if (name == "vertical")
		return  Renderer::isVerticalScreen() ? _("YES") : _("NO");

	return "";
}

static Utils::MathExpr evaluator;

static void _updateBindings(GuiComponent* comp, FileData* file, SystemData* system, bool showDefaultText)
{
	if (comp == nullptr || comp->getExtraType() == ExtraType::BUILTIN)
		return;

	if (system == nullptr && file == nullptr)
		return;

	TextComponent* text = dynamic_cast<TextComponent*>(comp);

	auto expressions = comp->getBindingExpressions();
	for (auto expression : expressions)
	{
		std::string xp = expression.second;
		if (xp.empty())
			continue;

		std::string propertyName = expression.first;

		auto existing = comp->getProperty(propertyName);
		if (existing.type == ThemeData::ThemeElement::Property::PropertyType::Unknown)
			continue;

		bool negate = xp[0] == '!';
		if (negate)
			xp = xp.substr(1);

		if (file != nullptr)
		{
			for (auto name : Utils::String::extractStrings(xp, "{game:", "}"))
			{
				std::string data = file->getProperty(name);

				if (text != nullptr && showDefaultText)
					data = valueOrUnknown(data);

				xp = Utils::String::replace(xp, "{game:" + name + "}", data);
			}
		}

		if (system != nullptr)
		{
			for (auto name : Utils::String::extractStrings(xp, "{binding:", "}"))
			{
				auto val = system->getProperty(name);
				if (showDefaultText)
					val = valueOrUnknown(val);

				xp = Utils::String::replace(xp, "{binding:" + name + "}", val);
			}

			for (auto name : Utils::String::extractStrings(xp, "{system:", "}"))
			{
				auto val = system->getProperty(name);
				if (showDefaultText)
					val = valueOrUnknown(val);

				xp = Utils::String::replace(xp, "{system:" + name + "}", val);
			}
		}

		for (auto name : Utils::String::extractStrings(xp, "{global:", "}"))
		{
			auto val = getGlobalProperty(name);
			if (showDefaultText)
				val = valueOrUnknown(val);

			xp = Utils::String::replace(xp, "{global:" + name + "}", val);
		}

		switch (existing.type)
		{
		case ThemeData::ThemeElement::Property::PropertyType::String:
			comp->setProperty(propertyName, xp);
			break;
		case ThemeData::ThemeElement::Property::PropertyType::Int:
			comp->setProperty(propertyName, (unsigned int)Utils::String::toInteger(xp));
			break;
		case ThemeData::ThemeElement::Property::PropertyType::Float:
			comp->setProperty(propertyName, Utils::String::toFloat(xp));
			break;
		case ThemeData::ThemeElement::Property::PropertyType::Bool:
		{
			bool isExpression = false;

			for (char ch : expression.second)
			{
				if (ch == ' ' || ch == '=' || ch == '<' || ch == '>' || ch == '&' || ch == '|' || ch == '+' || ch == '-')
				{
					isExpression = true;
					break;
				}
			}

			bool value = xp == _("YES");

			if (isExpression)
			{
				try
				{
					std::string evalxp = Utils::String::replace(xp, _("YES"), "1");
					evalxp = Utils::String::replace(evalxp, _("NO"), "0");

					auto ret = evaluator.eval(evalxp.c_str());
					if (ret.type == Utils::MathExpr::NUMBER)
						value = (ret.number != 0);
				}
				catch (...)
				{

				}
			}
			
			comp->setProperty(propertyName, negate ? !value : value);
		}
		break;
		}
	}

	for (int i = 0; i < comp->getChildCount(); i++)
		_updateBindings(comp->getChild(i), file, system, showDefaultText);

	StackPanelComponent* stack = dynamic_cast<StackPanelComponent*>(comp);
	if (stack != nullptr)
		stack->onSizeChanged();
}

void Binding::updateBindings(GuiComponent* comp, SystemData* system, bool showDefaultText)
{
	_updateBindings(comp, nullptr, system, showDefaultText);
}

void Binding::updateBindings(GuiComponent* comp, FileData* file, bool showDefaultText)
{
	_updateBindings(comp, file, file->getSystem(),  showDefaultText);
}