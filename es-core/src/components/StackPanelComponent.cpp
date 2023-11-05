#include "components/StackPanelComponent.h"
#include "Window.h"
#include "renderers/Renderer.h"
#include "components/ImageComponent.h"
#include "components/TextComponent.h"

StackPanelComponent::StackPanelComponent(Window* window) : GuiComponent(window), mHorizontal(true), mReverse(false), mSeparator(0.0f)
{

}

void StackPanelComponent::render(const Transform4x4f& parentTrans)
{
	if (!mVisible)
		return;

	Transform4x4f trans = parentTrans * getTransform();

	auto rect = Renderer::getScreenRect(trans, mSize);
	if (!Renderer::isVisibleOnScreen(rect))
		return;

	if (Settings::DebugImage())
	{
		Renderer::setMatrix(trans);
		Renderer::drawRect(0.0f, 0.0f, mSize.x(), mSize.y(), 0xFF00FF50, 0xFF00FF50);
	}

	GuiComponent::renderChildren(trans);
}


void StackPanelComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	GuiComponent::applyTheme(theme, view, element, properties);

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, "stackpanel");
	if (!elem)
		return;

	if (elem->has("orientation"))
		mHorizontal = (elem->get<std::string>("orientation") != "vertical");

	if (elem->has("reverse"))
		mReverse = elem->get<bool>("reverse");

	if (elem->has("separator"))
	{
		mSeparator = elem->get<float>("separator");
		if (mSeparator > 0 && mSeparator < 1)
		{
			Vector2f scale = getParent() ? getParent()->getSize() : Vector2f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
			mSeparator *= mHorizontal ? scale.y() : scale.x();
		}
	}

	performLayout();
}

void StackPanelComponent::onSizeChanged()
{
	GuiComponent::onSizeChanged();
	performLayout();
}

void StackPanelComponent::performLayout()
{
	int pos = 0;

	if (mReverse)
		pos = mHorizontal ? mSize.x() : mSize.y();

	for (auto child : mChildren)
	{
		if (!child->isVisible())
			continue;

		bool maxSize = false;

		ImageComponent* image = dynamic_cast<ImageComponent*>(child);
		if (image != nullptr)
			maxSize = image->getTargetIsMax();

		TextComponent* text = dynamic_cast<TextComponent*>(child);		

		if (mHorizontal)
		{
			if (image != nullptr && maxSize)
				image->setMaxSize(child->getSize().x(), mSize.y());
			else if (text != nullptr) 
			{				
				child->setSize(0, mSize.y());
				child->setSize(child->getSize().x(), mSize.y());				
			}
			else
				child->setSize(child->getSize().x(), mSize.y());

			if (mReverse)
			{
				child->setPosition(pos - child->getSize().x(), 0);
				pos -= child->getSize().x() + mSeparator;
			}
			else
			{
				child->setPosition(pos, 0);
				pos += child->getSize().x() + mSeparator;
			}
		}
		else
		{
			if (image != nullptr && maxSize)
				image->setMaxSize(mSize.x(), child->getSize().y());
			else
				child->setSize(mSize.x(), child->getSize().y());

			if (mReverse)
			{
				child->setPosition(0, pos - child->getSize().y());
				pos -= child->getSize().y() + mSeparator;
			}
			else
			{
				child->setPosition(0, pos);
				pos += child->getSize().y() + mSeparator;
			}
		}
	}
}

void StackPanelComponent::update(int deltaTime)
{	
	Vector2f szBefore;
	for (auto child : mChildren)
		if (child->isVisible())
			szBefore += child->getSize();

	GuiComponent::update(deltaTime);

	Vector2f szAfter;
	for (auto child : mChildren)
		if (child->isVisible())
			szAfter += child->getSize();

	if (szBefore != szAfter)
		performLayout();
}