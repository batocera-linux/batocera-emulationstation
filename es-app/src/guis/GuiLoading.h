//
// Created by matthieu on 03/08/15.
//

#ifndef EMULATIONSTATION_ALL_GUILOADING_H
#define EMULATIONSTATION_ALL_GUILOADING_H

#include "Window.h"
#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include "components/BusyComponent.h"
#include "guis/GuiMsgBox.h"
#include <string>
#include <thread>
#include "Settings.h"

template<typename T>
class GuiLoading : public GuiComponent
{
public:
	GuiLoading(Window *window, const std::string title, const std::function<T()> &func, const std::function<void(T)> &func2 = nullptr) 
		: GuiComponent(window), mBusyAnim(window), mFunc(func), mFunc2(func2)
	{
		setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

		setTag("popup");

		mRunning = true;
		mHandle = new std::thread(&GuiLoading::threadLoading, this);
		mBusyAnim.setText(title);
		mBusyAnim.setSize(mSize);
	}
	
	~GuiLoading()
	{
		mRunning = false;
		mHandle->join();
	}

	void render(const Transform4x4f &parentTrans) override
	{
		Transform4x4f trans = parentTrans * getTransform();

		renderChildren(trans);

		Renderer::setMatrix(trans);
		Renderer::drawRect(0.f, 0.f, mSize.x(), mSize.y(), 0x00000011);

		if (mRunning)
			mBusyAnim.render(trans);
	}

	bool input(InputConfig *config, Input input) override
	{
		return false;	
	}

	std::vector<HelpPrompt> getHelpPrompts() override
	{
		return std::vector<HelpPrompt>();
	}

	void update(int deltaTime) override
	{
		GuiComponent::update(deltaTime);
		mBusyAnim.update(deltaTime);

		if (!mRunning)
		{
			if (mFunc2)
			{
				Window* window = mWindow;
				auto func = mFunc2;
				auto ret = result;

				delete this;

				window->postToUiThread([func, ret](Window* win) { func(ret); });
			}
			else
				delete this;
		}
	}
	
private:
	void threadLoading()
	{
		result = mFunc();
		mRunning = false;
	}

    BusyComponent mBusyAnim;
    std::thread *mHandle;
    bool mRunning;
    const std::function<T()> mFunc;
    const std::function<void(T)> mFunc2;
    T result;
};

#endif //EMULATIONSTATION_ALL_GUILOADING_H
