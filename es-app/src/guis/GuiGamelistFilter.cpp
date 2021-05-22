#include "guis/GuiGamelistFilter.h"

#include "components/OptionListComponent.h"
#include "views/UIModeController.h"
#include "SystemData.h"
#include "guis/GuiTextEditPopup.h"
#include "guis/GuiTextEditPopupKeyboard.h"
#include "Genres.h"

GuiGamelistFilter::GuiGamelistFilter(Window* window, SystemData* system) : GuiComponent(window), mMenu(window, _("FILTER GAMELIST BY")), mSystem(system)
{
	mFilterIndex = mSystem->getIndex(true);
	initializeMenu();
}

GuiGamelistFilter::GuiGamelistFilter(Window* window, FileFilterIndex* filterIndex) : GuiComponent(window), mMenu(window, _("EDIT DYNAMIC COLLECTION FILTERS")), mSystem(nullptr)
{
	mFilterIndex = filterIndex;
	initializeMenu();
}

void GuiGamelistFilter::initializeMenu()
{
	addChild(&mMenu);

	ComponentListRow row;

	if (mSystem != nullptr)
		mMenu.addEntry(_("RESET ALL FILTERS"), false, std::bind(&GuiGamelistFilter::resetAllFilters, this));
	else
	{
		addTextFilterToMenu();
		addSystemFilterToMenu();
	}

	addFiltersToMenu();

	mMenu.addButton(_("BACK"), "back", std::bind(&GuiGamelistFilter::applyFilters, this));

	if (Renderer::isSmallScreen())
		mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, (Renderer::getScreenHeight() - mMenu.getSize().y()) / 2);
	else
		mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, Renderer::getScreenHeight() * 0.15f);
}

void GuiGamelistFilter::resetAllFilters()
{
	mFilterIndex->resetFilters();
	for (std::map<FilterIndexType, std::shared_ptr< OptionListComponent<std::string> >>::const_iterator it = mFilterOptions.cbegin(); it != mFilterOptions.cend(); ++it ) 
	{
		std::shared_ptr< OptionListComponent<std::string> > optionList = it->second;
		optionList->selectNone();
	}
}

GuiGamelistFilter::~GuiGamelistFilter()
{
	mFilterOptions.clear();

	if (!mFilterIndex->isFiltered() && mSystem != nullptr)
		mSystem->deleteIndex();
}

void GuiGamelistFilter::addTextFilterToMenu()
{
	auto theme = ThemeData::getMenuTheme();
	std::shared_ptr<Font> font = theme->Text.font;
	unsigned int color = theme->Text.color;

	ComponentListRow row;

	auto lbl = std::make_shared<TextComponent>(mWindow, _("FIND GAMES"), font, color);
	row.addElement(lbl, true); // label

	mTextFilter = std::make_shared<TextComponent>(mWindow, mFilterIndex->getTextFilter(), font, color, ALIGN_RIGHT);
	row.addElement(mTextFilter, true);

	auto spacer = std::make_shared<GuiComponent>(mWindow);
	spacer->setSize(Renderer::getScreenWidth() * 0.005f, 0);
	row.addElement(spacer, false);

	auto bracket = std::make_shared<ImageComponent>(mWindow);
	bracket->setImage(theme->Icons.arrow);
	bracket->setResize(Vector2f(0, lbl->getFont()->getLetterHeight()));
	row.addElement(bracket, false);

	auto updateVal = [this](const std::string& newVal) { mTextFilter->setValue(Utils::String::toUpper(newVal)); };

	row.makeAcceptInputHandler([this, updateVal] 
	{
		if (Settings::getInstance()->getBool("UseOSK"))
			mWindow->pushGui(new GuiTextEditPopupKeyboard(mWindow, _("FIND GAMES"), mTextFilter->getValue(), updateVal, false));		
		else
			mWindow->pushGui(new GuiTextEditPopup(mWindow, _("FIND GAMES"), mTextFilter->getValue(), updateVal, false));		
	});

	mMenu.addRow(row);
}

void GuiGamelistFilter::addFiltersToMenu()
{
	std::vector<FilterDataDecl> decls = mFilterIndex->getFilterDataDecls();
	
	int skip = 0;
	if (!UIModeController::getInstance()->isUIModeFull())
		skip = 1;
	if (UIModeController::getInstance()->isUIModeKid())
		skip = 2;

	for (std::vector<FilterDataDecl>::const_iterator it = decls.cbegin(); it != decls.cend()-skip; ++it ) {

		FilterIndexType type = (*it).type; // type of filter
		std::map<std::string, int>* allKeys = (*it).allIndexKeys; // all possible filters for this type
		std::string menuLabel = (*it).menuLabel; // text to show in menu
		std::shared_ptr< OptionListComponent<std::string> > optionList;
		
		// add filters (with first one selected)
		ComponentListRow row;

		optionList = std::make_shared< OptionListComponent<std::string> >(mWindow, menuLabel, true);

		if (it->type == GENRE_FILTER)
		{
			std::map<std::string, std::string> keyValues;

			for (auto key : *allKeys)
				keyValues[Genres::genreStringFromIds({ key.first })] = key.first;
		
			for (auto key : keyValues)
			{
				auto label = key.first;

				auto split = label.find("/");
				if (split != std::string::npos)
				{
					auto parent = Utils::String::trim(label.substr(0, split));
					if (keyValues.find(parent) != keyValues.cend())
						label = "      " + Utils::String::trim(label.substr(split + 1));
				}

				optionList->add(label, key.second, mFilterIndex->isKeyBeingFilteredBy(key.second, type));
			}
		}
		else
		{
			for (auto key : *allKeys)
			{
				if (key.first == "UNKNOWN")
					optionList->add(_("Unknown"), key.first, mFilterIndex->isKeyBeingFilteredBy(key.first, type));
				else if (key.first == "TRUE")
					optionList->add(_("YES"), key.first, mFilterIndex->isKeyBeingFilteredBy(key.first, type));
				else if (key.first == "FALSE")
					optionList->add(_("NO"), key.first, mFilterIndex->isKeyBeingFilteredBy(key.first, type));
				else
				{
					std::string label = key.first;

					// Special display for GENRE
					if (it->type == GENRE_FILTER)
					{
						auto split = key.first.find("/");
						if (split != std::string::npos)
						{
							auto parent = Utils::String::trim(label.substr(0, split));
							if (allKeys->find(parent) != allKeys->cend())
								label = "      " + Utils::String::trim(label.substr(split + 1));
						}
					}

					optionList->add(_(label.c_str()), key.first, mFilterIndex->isKeyBeingFilteredBy(key.first, type), false);
				}
			}
		}

		if (allKeys->size() > 0)
			mMenu.addWithLabel(menuLabel, optionList);

		mFilterOptions[type] = optionList;
	}
}

bool GuiGamelistFilter::input(InputConfig* config, Input input)
{
	bool consumed = GuiComponent::input(config, input);
	if(consumed)
		return true;

	if(config->isMappedTo(BUTTON_BACK, input) && input.value != 0)
		applyFilters();

	return false;
}

std::vector<HelpPrompt> GuiGamelistFilter::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = mMenu.getHelpPrompts();
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK")));
	return prompts;
}

void GuiGamelistFilter::addSystemFilterToMenu()
{	
	CollectionFilter* cf = dynamic_cast<CollectionFilter*>(mFilterIndex);
	if (cf == nullptr)
		return;
	
	mSystemFilter = std::make_shared<OptionListComponent<SystemData*>>(mWindow, _("SYSTEMS"), true);
	
	for (auto system : SystemData::sSystemVector)
		if (!system->isCollection() && !system->isGroupSystem())
			mSystemFilter->add(system->getFullName(), system, cf->isSystemSelected(system->getName()));

	mMenu.addWithLabel(_("SYSTEMS"), mSystemFilter);


}

void GuiGamelistFilter::applyFilters()
{
	CollectionFilter* collectionFilter = dynamic_cast<CollectionFilter*>(mFilterIndex);

	if (mTextFilter)
		mFilterIndex->setTextFilter(mTextFilter->getValue());

	if (mSystemFilter != nullptr && collectionFilter != nullptr)
	{
		std::string hiddenSystems;

		std::vector<SystemData*> sys = mSystemFilter->getSelectedObjects();

		if (!mSystemFilter->hasSelection() || mSystemFilter->size() == sys.size())
			collectionFilter->resetSystemFilter();
		else
		{
			for (auto system : SystemData::sSystemVector)
			{
				if (system->isCollection() || system->isGroupSystem())
					continue;

				bool sel = std::find(sys.cbegin(), sys.cend(), system) != sys.cend();
				collectionFilter->setSystemSelected(system->getName(), sel);
			}
		}
	}

	std::vector<FilterDataDecl> decls = mFilterIndex->getFilterDataDecls();
	for (std::map<FilterIndexType, std::shared_ptr< OptionListComponent<std::string> >>::const_iterator it = mFilterOptions.cbegin(); it != mFilterOptions.cend(); ++it)
	{
		std::shared_ptr< OptionListComponent<std::string> > optionList = it->second;
		std::vector<std::string> filters = optionList->getSelectedObjects();
		mFilterIndex->setFilter(it->first, &filters);
	}

	if (collectionFilter != nullptr)
		collectionFilter->save();

	auto finalize = mOnFinalizeFunc;

	delete this;

	if (finalize != nullptr)
		finalize();
}