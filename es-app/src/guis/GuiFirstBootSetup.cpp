#include "guis/GuiFirstBootSetup.h"

#include <sys/wait.h>

#include "guis/GuiSettings.h"
#include "guis/GuiWifi.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiLoading.h"
#include "guis/GuiAiGraphics.h"
#include "components/TextComponent.h"
#include "SystemConf.h"
#include "ApiSystem.h"
#include "ThemeData.h"
#include "Window.h"
#include "LocaleES.h"
#include "Log.h"

GuiFirstBootSetup::GuiFirstBootSetup(Window* window)
    : GuiComponent(window)
{
    setSize(0, 0);
}

// ---------------------------------------------------------------------------
// Step 1 - User name
// ---------------------------------------------------------------------------
void GuiFirstBootSetup::showStepName()
{
    // "NEXT" advances; "BACK" just closes this screen without advancing.
    auto* s = new GuiSettings(mWindow, _("WELCOME TO BATOCERA AI"), _("NEXT"),
        [this](GuiSettings* gui)
        {
            gui->save();            // runs addInputTextRow save funcs + saveSystemConf
            gui->setSave(false);    // prevent double-save on close
            showStepWifi();
            gui->close();
        });

    s->setSubTitle(_("Enter your name to personalize your experience."));
    s->addInputTextRow(_("Your Name"), "system.username", /*password=*/false, /*storeInSettings=*/false);

    mWindow->pushGui(s);
}

// ---------------------------------------------------------------------------
// Step 2 - WiFi setup
// ---------------------------------------------------------------------------
void GuiFirstBootSetup::showStepWifi(const std::string& error)
{
    const std::string baseSSID = SystemConf::getInstance()->get("wifi.ssid");
    const std::string baseKEY  = SystemConf::getInstance()->get("wifi.key");

    // "NEXT" attempts to connect and advances on success, or re-shows this
    // screen with an inline error message on failure.
    auto* s = new GuiSettings(mWindow, _("NETWORK SETUP"), _("NEXT"),
        [this, baseSSID, baseKEY](GuiSettings* gui)
        {
            gui->save();            // writes wifi.ssid / wifi.key to SystemConf
            gui->setSave(false);

            std::string newSSID = SystemConf::getInstance()->get("wifi.ssid");
            std::string newKEY  = SystemConf::getInstance()->get("wifi.key");

            if (newSSID.empty())
            {
                gui->close();
                showStepWifi(_("Please select a WiFi network to continue."));
                return;
            }

            if (newSSID != baseSSID || newKEY != baseKEY)
            {
                SystemConf::getInstance()->set("wifi.enabled", "1");
                SystemConf::getInstance()->saveSystemConf();

                bool ok = ApiSystem::getInstance()->enableWifi(newSSID, newKEY);
                if (!ok)
                {
                    // Re-show this screen with an inline error instead of a popup.
                    gui->close();
                    showStepWifi(_("Could not connect. Check your password and try again."));
                    return;
                }
            }

            showStepModels();
            gui->close();
        });

    s->setSubTitle(_("Connect to WiFi to enable AI features and model downloads."));

    // Inline error message shown when a previous connection attempt failed.
    if (!error.empty())
    {
        auto theme = ThemeData::getMenuTheme();
        ComponentListRow row;
        auto errorText = std::make_shared<TextComponent>(
            mWindow, error, theme->Text.font, 0xFF0000FF, ALIGN_CENTER);
        row.addElement(errorText, true);
        s->addRow(row);
    }

    // SSID row - tapping opens GuiWifi (network list), matching main settings.
    auto openWifi = [](Window* win, std::string title, std::string data,
                       const std::function<void(std::string)>& onsave)
    {
        win->pushGui(new GuiWifi(win, title, data, onsave));
    };
    s->addInputTextRow(_("WIFI SSID"), "wifi.ssid", /*password=*/false, /*storeInSettings=*/false, openWifi);

    // Password row - opens on-screen keyboard / text popup with masking.
    s->addInputTextRow(_("WIFI KEY"), "wifi.key", /*password=*/true);

    // SKIP button - skips WiFi and model download, goes straight to finish.
    s->getMenu().addButton(_("SKIP"), "skip wifi setup", [this, s]()
    {
        s->setSave(false);
        s->close();
        finalize();
    });

    mWindow->pushGui(s);
}

// ---------------------------------------------------------------------------
// Step 3 - AI model install
// ---------------------------------------------------------------------------
void GuiFirstBootSetup::showStepModels()
{
    Window* window = mWindow;

    window->pushGui(new GuiMsgBox(window,
        _("Would you like to download the AI model files now?\n\nThis requires an internet connection and approximately 5 GB of free disk space."),
        _("YES"), [this, window]()
        {
            // Use std::string as result: empty = success, non-empty = error message.
            window->pushGui(new GuiLoading<std::string>(window,
                _("DOWNLOADING AI MODELS - THIS MAY TAKE A WHILE..."),
                [](IGuiLoadingHandler* handler) -> std::string
                {
                    LOG(LogInfo) << "GuiFirstBootSetup: running local-llm-setup-models";
                    handler->setText(_("Downloading AI models, please wait..."));

                    FILE* pipe = popen("/usr/bin/local-llm-setup-models 2>&1", "r");
                    if (pipe == nullptr)
                        return _("Failed to start download process.");

                    std::string lastLine;
                    char line[1024];
                    while (fgets(line, sizeof(line), pipe))
                    {
                        // Strip trailing newline and keep updating the progress text.
                        std::string s(line);
                        if (!s.empty() && s.back() == '\n')
                            s.pop_back();
                        if (!s.empty())
                        {
                            lastLine = s;
                            handler->setText(s);
                        }
                    }

                    int exitCode = WEXITSTATUS(pclose(pipe));
                    if (exitCode != 0)
                    {
                        LOG(LogWarning) << "GuiFirstBootSetup: local-llm-setup-models exited with code " << exitCode;
                        return lastLine.empty()
                            ? _("Download failed (unknown error).")
                            : lastLine;
                    }
                    return std::string(); // success
                },
                [this, window](const std::string& error)
                {
                    if (!error.empty())
                    {
                        window->pushGui(new GuiMsgBox(window,
                            _("Model download failed:") + "\n\n" + error +
                            "\n\n" + _("You can retry later from the main menu."),
                            _("OK"), [this]() { finalize(); }));
                    }
                    else
                    {
                        finalize();
                    }
                }));
        },
        _("SKIP"), [this]() { finalize(); },
        ICON_QUESTION));
}

// ---------------------------------------------------------------------------
// Finalize - mark first-boot done and launch the main screen
// ---------------------------------------------------------------------------
void GuiFirstBootSetup::finalize()
{
    SystemConf::getInstance()->set("system.firstboot", "done");
    SystemConf::getInstance()->saveSystemConf();

    LOG(LogInfo) << "GuiFirstBootSetup: setup complete, launching GuiAiGraphics";

    mWindow->pushGui(new GuiAiGraphics(mWindow));
    delete this;
}
