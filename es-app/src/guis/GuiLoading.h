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
#include "animations/LambdaAnimation.h"

class IGuiLoadingHandler
{
public:
	virtual void setText(const std::string& text) = 0;
};

template<typename T>
class GuiLoading : public GuiComponent, public IGuiLoadingHandler
{
public:
	GuiLoading(Window *window, const std::string title, const std::function<T(IGuiLoadingHandler*)> &func, const std::function<void(T)> &func2 = nullptr)
		: GuiComponent(window), mBusyAnim(window), mFunc(func), mFunc2(func2)
	{
		setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
		setTag("GuiLoading");
	
		mRunning = true;
		mHandle = new std::thread(&GuiLoading::threadLoading, this);
		mBusyAnim.setText(title);
		mBusyAnim.setSize(mSize);

		mBusyAnim.setOpacity(0);
		auto fadeFunc = [this](float t) { mBusyAnim.setOpacity((unsigned char) (Math::easeOutCubic(t) * 255.0f)); };
		setAnimation(new LambdaAnimation(fadeFunc, 450), 0, [this] { mBusyAnim.setOpacity(255); });
	}
	
	~GuiLoading()
	{
		mRunning = false;
		mHandle->join();
	}

	void setText(const std::string& text) override
	{
		mBusyAnim.setText(text);
		mBusyAnim.setSize(mSize);
	}

	void render(const Transform4x4f &parentTrans) override
	{
		if (!mRunning || !mVisible)
			return;

		Transform4x4f trans = parentTrans * getTransform();
		renderChildren(trans);

		Renderer::setMatrix(trans);
		mBusyAnim.render(trans);
	}

	bool input(InputConfig *config, Input input) override
	{
		return false;	
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

				func(ret);
			}
			else
				delete this;
		}
	}
	
private:
	void threadLoading()
	{
		result = mFunc(this);
		mRunning = false;
	}

    BusyComponent mBusyAnim;
    std::thread *mHandle;
    bool mRunning;
    const std::function<T(IGuiLoadingHandler*)> mFunc;
    const std::function<void(T)> mFunc2;
    T result;
};

#endif //EMULATIONSTATION_ALL_GUILOADING_H
