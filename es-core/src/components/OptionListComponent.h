#pragma once
#ifndef ES_CORE_COMPONENTS_OPTION_LIST_COMPONENT_H
#define ES_CORE_COMPONENTS_OPTION_LIST_COMPONENT_H

#include "GuiComponent.h"
#include "Log.h"
#include "Window.h"
#include "LocaleES.h"
#include "ThemeData.h"
#include "components/MultiLineMenuEntry.h"
#include "components/MenuComponent.h"

#include <tuple>

//Used to display a list of options.
//Can select one or multiple options.

// if !multiSelect
// * <- curEntry ->

// always
// * press a -> open full list

#define CHECKED_PATH ":/checkbox_checked.svg"
#define UNCHECKED_PATH ":/checkbox_unchecked.svg"

template<typename T>
class OptionListComponent : public GuiComponent
{
private:
	struct OptionListData
	{
		std::string name;
		std::string description;

		T object;
		bool selected;
		bool treeChild;

		std::string group;
	};

	class OptionListPopup : public GuiComponent
	{
	private:
		MenuComponent mMenu;
		OptionListComponent<T>* mParent;
		// for select all/none

		struct CheckBoxElement
		{
			ImageComponent* checkbox;
			OptionListData* item;
		};

		std::vector<CheckBoxElement> mCheckBoxes;

	public:
		OptionListPopup(Window* window, OptionListComponent<T>* parent, const std::string& title, const std::function<void(T& data, ComponentListRow& row)> callback = nullptr) : GuiComponent(window),
			mMenu(window, title.c_str()), mParent(parent)
		{
			auto menuTheme = ThemeData::getMenuTheme();
			auto font = menuTheme->Text.font;
			auto color = menuTheme->Text.color;

		//	if (parent->mMultiSelect && parent->mMultiSelectShowNames)
		//		font = menuTheme->TextSmall.font;

			ComponentListRow row;
					
			for(auto it = mParent->mEntries.begin(); it != mParent->mEntries.end(); it++)
			{
				row.elements.clear();

				OptionListData& e = *it;
				
				if (callback != nullptr)
				{
					callback(e.object, row);

					if (mParent->mMultiSelect)
					{
						row.makeAcceptInputHandler([this, &e]
						{
							e.selected = !e.selected;														
							mParent->onSelectedChanged();
						});
					}
					else
					{
						row.makeAcceptInputHandler([this, &e]
						{
							mParent->mEntries.at(mParent->getSelectedId()).selected = false;
							e.selected = true;
							mParent->onSelectedChanged();
							delete this;
						});
					}
				}
				else
				{
					if (!it->description.empty())
						row.addElement(std::make_shared<MultiLineMenuEntry>(mWindow, Utils::String::toUpper(it->name), it->description), true);
					else
					{
						auto text = std::make_shared<TextComponent>(mWindow, e.treeChild ? "      " + Utils::String::toUpper(it->name) : Utils::String::toUpper(it->name), font, color);
						if (EsLocale::isRTL())
							text->setHorizontalAlignment(Alignment::ALIGN_RIGHT);

						row.addElement(text, true);
					}

					if (mParent->mMultiSelect)
					{
						// add checkbox
						auto checkbox = std::make_shared<ImageComponent>(mWindow);
						checkbox->setImage(it->selected ? CHECKED_PATH : UNCHECKED_PATH);
						checkbox->setResize(0, font->getLetterHeight());
						row.addElement(checkbox, false);

						// input handler
						// update checkbox state & selected value
						row.makeAcceptInputHandler([this, &e, checkbox]
						{
							e.selected = !e.selected;
							checkbox->setImage(e.selected ? CHECKED_PATH : UNCHECKED_PATH);
							mParent->onSelectedChanged();
						});

						CheckBoxElement el;
						el.checkbox = checkbox.get();
						el.item = &e;

						// for select all/none
						mCheckBoxes.push_back(el);
					}
					else {
						// input handler for non-multiselect
						// update selected value and close
						row.makeAcceptInputHandler([this, &e]
						{
							mParent->mEntries.at(mParent->getSelectedId()).selected = false;
							e.selected = true;
							mParent->onSelectedChanged();
							delete this;
						});
					}
				}


				if (!e.group.empty())
					mMenu.addGroup(e.group);

				// also set cursor to this row if we're not multi-select and this row is selected
				mMenu.addRow(row, (!mParent->mMultiSelect && it->selected), false);
			}

			mMenu.addButton(_("BACK"), _("accept"), [this] { delete this; }); // batocera

			if (mParent->mMultiSelect)
			{
				mMenu.addButton(_("SELECT ALL"), _("select all"), [this]
				{
					for (unsigned int i = 0; i < mParent->mEntries.size(); i++)
					{
						mParent->mEntries.at(i).selected = true;
						mCheckBoxes.at(i).checkbox->setImage(CHECKED_PATH);
					}
					mParent->onSelectedChanged();
				});

				mMenu.addButton(_("SELECT NONE"), _("select none"), [this]
				{
					for (unsigned int i = 0; i < mParent->mEntries.size(); i++)
					{
						mParent->mEntries.at(i).selected = false;
						mCheckBoxes.at(i).checkbox->setImage(UNCHECKED_PATH);
					}
					mParent->onSelectedChanged();
				});
			}

			if (Renderer::isSmallScreen())
				mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, (Renderer::getScreenHeight() - mMenu.getSize().y()) / 2);
			else
				mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, Renderer::getScreenHeight() * 0.15f);

			addChild(&mMenu);
		}

		bool input(InputConfig* config, Input input) override
		{
			if(config->isMappedTo(BUTTON_BACK, input) && input.value != 0) // batocera
			{
				delete this;
				return true;
			}

			return GuiComponent::input(config, input);
		}

		std::vector<HelpPrompt> getHelpPrompts() override
		{
			auto prompts = mMenu.getHelpPrompts();
			prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK")));
			return prompts;
		}
	};

public:
	OptionListComponent(Window* window, const std::string& name, bool multiSelect = false, bool multiSelectShowNames = false) : GuiComponent(window), mMultiSelect(multiSelect), mName(name),
		 mText(window), mLeftArrow(window), mRightArrow(window), mMultiSelectShowNames(multiSelectShowNames)
	{
		auto theme = ThemeData::getMenuTheme();

		mAddRowCallback = nullptr;

		mText.setFont(theme->Text.font);
		mText.setColor(theme->Text.color);
		mText.setHorizontalAlignment(ALIGN_CENTER);

		addChild(&mText);

		mLeftArrow.setResize(0, mText.getFont()->getLetterHeight());
		mRightArrow.setResize(0, mText.getFont()->getLetterHeight());

		if(mMultiSelect)
		{
			if (EsLocale::isRTL())
			{
				mLeftArrow.setImage(ThemeData::getMenuTheme()->Icons.arrow);
				mLeftArrow.setColorShift(theme->Text.color);
				mLeftArrow.setFlipX(true);
				addChild(&mLeftArrow);
			}
			else
			{
				mRightArrow.setImage(ThemeData::getMenuTheme()->Icons.arrow);
				mRightArrow.setColorShift(theme->Text.color);
				addChild(&mRightArrow);
			}
		}
		else
		{
			mLeftArrow.setImage(ThemeData::getMenuTheme()->Icons.option_arrow);
			mLeftArrow.setColorShift(theme->Text.color);
			mLeftArrow.setFlipX(true);
			addChild(&mLeftArrow);

			mRightArrow.setImage(ThemeData::getMenuTheme()->Icons.option_arrow); // ":/option_arrow.svg");
			mRightArrow.setColorShift(theme->Text.color);
			addChild(&mRightArrow);
		}

		setSize(mLeftArrow.getSize().x() + mRightArrow.getSize().x(), theme->Text.font->getHeight());
	}

	virtual void setColor(unsigned int color)
	{
		mText.setColor(color);
		mLeftArrow.setColorShift(color);
		mRightArrow.setColorShift(color);
	}

	// handles positioning/resizing of text and arrows
	void onSizeChanged() override
	{
		mLeftArrow.setResize(0, mText.getFont()->getLetterHeight());
		mRightArrow.setResize(0, mText.getFont()->getLetterHeight());

		if(mSize.x() < (mLeftArrow.getSize().x() + mRightArrow.getSize().x()))
			LOG(LogWarning) << "OptionListComponent too narrow!";

		mText.setSize(mSize.x() - mLeftArrow.getSize().x() - mRightArrow.getSize().x(), mText.getFont()->getHeight());

		// position
		mLeftArrow.setPosition(0, (mSize.y() - mLeftArrow.getSize().y()) / 2);
		mText.setPosition(mLeftArrow.getPosition().x() + mLeftArrow.getSize().x(), (mSize.y() - mText.getSize().y()) / 2);
		mRightArrow.setPosition(mText.getPosition().x() + mText.getSize().x(), (mSize.y() - mRightArrow.getSize().y()) / 2);
	}

	bool input(InputConfig* config, Input input) override
	{
		if(input.value != 0)
		{
			if(config->isMappedTo(BUTTON_OK, input))
			{
				if (mEntries.size() > 0)
					open();

				return true;
			}
			if(!mMultiSelect)
			{
				if(config->isMappedLike("left", input))
				{
					if (mEntries.size() == 0)
						return true;

					// move selection to previous
					unsigned int i = getSelectedId();
					int next = (int)i - 1;
					if(next < 0)
						next += (int)mEntries.size();

					mEntries.at(i).selected = false;
					mEntries.at(next).selected = true;
					onSelectedChanged();
					return true;

				}else if(config->isMappedLike("right", input))
				{
					if (mEntries.size() == 0)
						return true;

					// move selection to next
					unsigned int i = getSelectedId();
					int next = (i + 1) % mEntries.size();
					mEntries.at(i).selected = false;
					mEntries.at(next).selected = true;
					onSelectedChanged();
					return true;

				}
			}
		}
		return GuiComponent::input(config, input);
	}

	std::vector<T> getSelectedObjects()
	{
		std::vector<T> ret;
		for(auto it = mEntries.cbegin(); it != mEntries.cend(); it++)
		{
			if(it->selected)
				ret.push_back(it->object);
		}

		return ret;
	}

	T getSelected()
	{
		assert(mMultiSelect == false);
		auto selected = getSelectedObjects();
		assert(selected.size() == 1);
		return selected.at(0);
	}

  	bool IsMultiSelect() { return mMultiSelect; }
        
        // batocera
	std::string getSelectedName()
	{
                assert(mMultiSelect == false);
                for(unsigned int i = 0; i < mEntries.size(); i++)
		{
			if(mEntries.at(i).selected)
				return mEntries.at(i).name;
		}
                return "";
	}
        
	void addEx(const std::string& name, const std::string& description, const T& obj, bool selected, bool treeChild = false)
	{
		for (auto sysIt = mEntries.cbegin(); sysIt != mEntries.cend(); sysIt++)
			if (sysIt->name == name)
				return;

		OptionListData e;
		e.name = name;
		e.description = description;
		e.object = obj;
		e.selected = selected;
		e.treeChild = treeChild;
		e.group = mGroup;
		mGroup = "";

		if (selected)
			firstSelected = obj;

		mEntries.push_back(e);
		onSelectedChanged();
	}

	void add(const std::string& name, const T& obj, bool selected, bool distinct = true, bool treeChild = false)
	{
		if (distinct)
		{
			for (auto sysIt = mEntries.cbegin(); sysIt != mEntries.cend(); sysIt++)
				if (sysIt->name == name)
					return;
		}

		OptionListData e;
		e.name = name;
		e.object = obj;
		e.selected = selected;
		e.treeChild = treeChild;
		e.group = mGroup;
		mGroup = "";
                
		if(selected)
			firstSelected = obj;

		mEntries.push_back(e);
		onSelectedChanged();
	}

	void addRange(const std::vector<std::string> values, const std::string selectedValue = "")
	{
		for (auto value : values)
			add(_(value.c_str()), value, selectedValue == value);

		if (!hasSelection())
			selectFirstItem();
	}
	
	void addRange(const std::vector<std::pair<std::string, T>> values, const T selectedValue)
	{
		for (auto value : values)
			add(value.first.c_str(), value.second, selectedValue == value.second);

		if (!hasSelection())
			selectFirstItem();
	}

	void addRange(const std::vector<std::tuple<std::string, std::string, T>> values, const T selectedValue)
	{
		for (auto value : values)
			addEx(std::get<0>(value), std::get<1>(value), std::get<2>(value), selectedValue == std::get<2>(value));

		if (!hasSelection())
			selectFirstItem();
	}

	void addGroup(const std::string name)
	{
		mGroup = name;
	}

	void remove(const std::string& name)
	{
		for (auto sysIt = mEntries.cbegin(); sysIt != mEntries.cend(); sysIt++)
		{
			if (sysIt->name == name)
			{
				bool isSelect = sysIt->selected;

				mEntries.erase(sysIt);

				if (isSelect)
					selectFirstItem();

				break;
			}
		}
	}

        // batocera
	inline void setSelectedChangedCallback(const std::function<void(const T&)>& callback) {
		mSelectedChangedCallback = callback;
	}

	void selectAll()
	{
		for(unsigned int i = 0; i < mEntries.size(); i++)
		{
			mEntries.at(i).selected = true;
		}
		onSelectedChanged();
	}

	void selectNone()
	{
		for(unsigned int i = 0; i < mEntries.size(); i++)
		{
			mEntries.at(i).selected = false;
		}
		onSelectedChanged();
	}

        // batocera
	bool changed(){
	  auto selected = getSelectedObjects();
	  if(selected.size() != 1) return false;
	  return firstSelected != getSelected();
	}

	bool hasSelection()
	{
		for (unsigned int i = 0; i < mEntries.size(); i++)
			if (mEntries.at(i).selected)
				return true;

		return false;
	}

	int size()
	{
		return (int)mEntries.size();
	}

	//size_type
	void selectFirstItem()
	{
		for (unsigned int i = 0; i < mEntries.size(); i++)
			mEntries.at(i).selected = false;

		if (mEntries.size() > 0)
			mEntries.at(0).selected = true;

		onSelectedChanged();
	}

	void clear() {
		mEntries.clear();
	}

	inline void invalidate() {
		onSelectedChanged();
	}

	void setRowTemplate(std::function<void(T& data, ComponentListRow& row)> callback)
	{
		mAddRowCallback = callback;
	}

private:	
	std::function<void(T& data, ComponentListRow& row)> mAddRowCallback;

	unsigned int getSelectedId()
	{
		assert(mMultiSelect == false);
		for(unsigned int i = 0; i < mEntries.size(); i++)
		{
			if(mEntries.at(i).selected)
				return i;
		}

		LOG(LogWarning) << "OptionListComponent::getSelectedId() - no selected element found, defaulting to 0";
		return 0;
	}

	void open()
	{
		mWindow->pushGui(new OptionListPopup(mWindow, this, mName, mAddRowCallback));
	}

	void onSelectedChanged()
	{
		if (mMultiSelect && mMultiSelectShowNames)
		{
			std::string name;

			// display currently selected + l/r cursors
			for (auto it = mEntries.cbegin(); it != mEntries.cend(); it++)
			{
				if (it->selected)
				{
					if (name.empty())
						name = Utils::String::trim(it->name);
					else
						name = name + ", " + Utils::String::trim(it->name);
				}
			}

			mText.setText(name);
			mText.setSize(0, mText.getSize().y());
			setSize(mText.getSize().x() + mLeftArrow.getSize().x() + mRightArrow.getSize().x() + 24, mText.getSize().y());
			if (mParent) // hack since theres no "on child size changed" callback atm...
				mParent->onSizeChanged();
		}
		else if (mMultiSelect)
		{
			// display # selected
		  	char strbuf[256];
			int x = getSelectedObjects().size();
		  	snprintf(strbuf, 256, ngettext("%i SELECTED", "%i SELECTED", x), x);
			mText.setText(strbuf);

			mText.setSize(0, mText.getSize().y());
			setSize(mText.getSize().x() + mRightArrow.getSize().x() + 24, mText.getSize().y());
			if(mParent) // hack since theres no "on child size changed" callback atm...
				mParent->onSizeChanged();
		}
		else
		{
			// display currently selected + l/r cursors
			for(auto it = mEntries.cbegin(); it != mEntries.cend(); it++)
			{
				if(it->selected)
				{
					mText.setText(Utils::String::toUpper(it->name));
					mText.setSize(0, mText.getSize().y());
					setSize(mText.getSize().x() + mLeftArrow.getSize().x() + mRightArrow.getSize().x() + 24, mText.getSize().y());
					if(mParent) // hack since theres no "on child size changed" callback atm...
						mParent->onSizeChanged();
					break;
				}
			}
		}

        // batocera
		if (mSelectedChangedCallback)
			mSelectedChangedCallback(mEntries.at(getSelectedId()).object);		
	}

	std::vector<HelpPrompt> getHelpPrompts() override
	{
		std::vector<HelpPrompt> prompts;
		if(!mMultiSelect)
			prompts.push_back(HelpPrompt("left/right", _("CHANGE"))); // batocera

		prompts.push_back(HelpPrompt(BUTTON_OK, _("SELECT")));
		return prompts;
	}

	bool mMultiSelect;
	bool mMultiSelectShowNames;

	std::string mName;
	std::string mGroup;

	T firstSelected; // batocera
	TextComponent mText;
	ImageComponent mLeftArrow;
	ImageComponent mRightArrow;

	std::vector<OptionListData> mEntries;
	std::function<void(const T&)> mSelectedChangedCallback; // batocera
};

#endif // ES_CORE_COMPONENTS_OPTION_LIST_COMPONENT_H
