#pragma once
#ifndef ES_APP_GUIS_GUI_SETTINGS_H
#define ES_APP_GUIS_GUI_SETTINGS_H

#include "components/MenuComponent.h"

// This is just a really simple template for a GUI that calls some save functions when closed.
class GuiSettings : public GuiComponent
{
public:
	GuiSettings(Window* window, 
		const std::string title,
		const std::string customButton = "",
		const std::function<void(GuiSettings*)>& func = nullptr,
		bool animate = false);
	virtual ~GuiSettings(); // just calls save();

	void close();

	void save();
	inline void addRow(const ComponentListRow& row) { mMenu.addRow(row); };
	inline void addWithLabel(const std::string& label, const std::shared_ptr<GuiComponent>& comp, bool setCursorHere = false) { mMenu.addWithLabel(label, comp, nullptr, "", setCursorHere); };
	inline void addWithDescription(const std::string& label, const std::string& description, const std::shared_ptr<GuiComponent>& comp, bool setCursorHere = false) { mMenu.addWithDescription(label, description, comp, nullptr, "", setCursorHere); };
	inline void addSaveFunc(const std::function<void()>& func) { mSaveFuncs.push_back(func); };
	inline void addEntry(const std::string name, bool add_arrow = false, const std::function<void()>& func = nullptr, const std::string iconName = "", bool onButtonRelease = false, bool setCursorHere = false) { mMenu.addEntry(name, add_arrow, func, iconName, setCursorHere, true, onButtonRelease); };

	inline void addGroup(const std::string& label) { mMenu.addGroup(label); };

	void addInputTextRow(std::string title, const char* settingsID, bool password, bool storeInSettings = false, const std::function<void(Window*, std::string/*title*/, std::string /*value*/, const std::function<void(std::string)>& onsave)>& customEditor = nullptr);

	void addSubMenu(const std::string& label, const std::function<void()>& func);

    inline void setSave(bool sav) { mDoSave = sav; }; // batocera

	bool input(InputConfig* config, Input input) override;
	std::vector<HelpPrompt> getHelpPrompts() override;
	HelpStyle getHelpStyle() override;

	MenuComponent& getMenu() { return mMenu; }

	inline void onFinalize(const std::function<void()>& func) { mOnFinalizeFunc = func; };

	bool getVariable(const std::string name) 
	{
		if (mVariableMap.find(name) == mVariableMap.cend())
			return false;

		return mVariableMap[name];
	}

	void setCloseButton(const std::string name) { mCloseButton = name; }
	void setVariable(const std::string name, bool value) { mVariableMap[name] = value; }

	void setTitle(const std::string title) { mMenu.setTitle(title); }
	void setSubTitle(const std::string text) { mMenu.setSubTitle(text); }
	void setTitleImage(std::shared_ptr<ImageComponent> titleImage) { mMenu.setTitleImage(titleImage); }

protected:
	MenuComponent mMenu;

private:
	bool mDoSave = true; // batocera

	std::vector< std::function<void()> > mSaveFuncs;
	std::function<void()> mOnFinalizeFunc;

	std::map<std::string, bool> mVariableMap;

	std::string		mCloseButton;
};

#endif // ES_APP_GUIS_GUI_SETTINGS_H
