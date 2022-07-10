#include "Binding.h"
#include "SystemData.h"
#include "FileData.h"
#include "utils/StringUtil.h"
#include "LocaleES.h"
#include <time.h>

#include "components/TextComponent.h"
#include "components/ImageComponent.h"
#include "components/VideoComponent.h"

void Binding::updateBindings(TextComponent* comp, SystemData* system, bool showDefaultText)
{
	if (comp == nullptr || system == nullptr)
		return;

	auto src = comp->getOriginalThemeText();
	if (src.empty() || (src.find("{binding:") == std::string::npos && src.find("{system:") == std::string::npos))
		return;

	auto valueOrUnknown = [](const std::string value) { return value.empty() ? _("Unknown") : value == "0" ? _("None") : value; };

	for (auto name : Utils::String::extractStrings(src, "{binding:", "}"))
	{		
		auto val = system->getProperty(name);
		if (showDefaultText)
			val = valueOrUnknown(val);

		src = Utils::String::replace(src, "{binding:" + name+ "}", val);
	}

	for (auto name : Utils::String::extractStrings(src, "{system:", "}", true))
	{
		auto val = system->getProperty(name);
		if (showDefaultText)
			val = valueOrUnknown(val);

		src = Utils::String::replace(src, "{system:" + name + "}", val);
	}
		
	comp->setText(src);
}

static void _updateTextBindings(TextComponent* comp, FileData* file, bool showDefaultText)
{
	if (comp == nullptr || system == nullptr)
		return;

	auto src = comp->getOriginalThemeText();
	if (src.empty() || src.find("{game:") == std::string::npos)
		return;

	auto valueOrUnknown = [](const std::string value) { return value.empty() ? _("Unknown") : value == "0" ? _("None") : value; };

	for (auto name : Utils::String::extractStrings(src, "{game:", "}"))
	{
		auto val = file->getProperty(name);
		
		if (showDefaultText)
			val = valueOrUnknown(val);

		src = Utils::String::replace(src, "{game:" + name + "}", val);
	}
	
	comp->setText(src);
}


static void _updateImageBindings(ImageComponent* comp, FileData* file)
{
	if (comp == nullptr || system == nullptr)
		return;

	auto src = comp->getOriginalThemePath();
	if (src.empty() || src.find("{game:") == std::string::npos)
		return;

	for (auto name : Utils::String::extractStrings(src, "{game:", "}"))
		src = Utils::String::replace(src, "{game:" + name + "}", file->getProperty(name));

	comp->setImage(src);
}

static void _updateVideoBindings(VideoComponent* comp, FileData* file)
{
	if (comp == nullptr || system == nullptr)
		return;

	auto src = comp->getOriginalThemePath();
	if (src.empty() || src.find("{game:") == std::string::npos)
		return;

	for (auto name : Utils::String::extractStrings(src, "{game:", "}"))
		src = Utils::String::replace(src, "{game:" + name + "}", file->getProperty(name));

	comp->setVideo(src);
}

void Binding::updateBindings(GuiComponent* comp, FileData* file, bool showDefaultText)
{
	if (comp == nullptr || system == nullptr)
		return;

	TextComponent* text = dynamic_cast<TextComponent*>(comp);
	if (text != nullptr)
		_updateTextBindings(text, file, showDefaultText);

	ImageComponent* image = dynamic_cast<ImageComponent*>(comp);
	if (image != nullptr)
		_updateImageBindings(image, file);

	VideoComponent* video = dynamic_cast<VideoComponent*>(comp);
	if (video != nullptr)
		_updateVideoBindings(video, file);

}