#include "components/PostProcessShaderComponent.h"
#include "Window.h"
#include "renderers/Renderer.h"

PostProcessShaderComponent::PostProcessShaderComponent(Window* window) : GuiComponent(window)
{

}

void PostProcessShaderComponent::render(const Transform4x4f& parentTrans)
{
	if (!mVisible || mShaderPath.empty())
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

	Renderer::postProcessShader(mShaderPath, rect.x, rect.y, rect.w, rect.h, mParameters);
}

void PostProcessShaderComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	GuiComponent::applyTheme(theme, view, element, properties);

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, "shader");
	if (elem == nullptr)
		return;

	if (elem->has("path"))
		mShaderPath = elem->get<std::string>("path");

	for (auto prop : elem->properties)
	{
		if (prop.first == "pos" || prop.first == "path" || prop.first == "size" || prop.first == "zIndex")
			continue;

		if (prop.second.type != ThemeData::ThemeElement::Property::PropertyType::String)
			continue;

		mParameters[prop.first] = prop.second.s;
	}
}