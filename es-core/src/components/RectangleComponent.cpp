#include "components/RectangleComponent.h"
#include "utils/StringUtil.h"
#include "resources/TextureResource.h"

RectangleComponent::RectangleComponent(Window* window) : GuiComponent(window),
	mColor(0), mBorderColor(0), mBorderSize(1.0f), mRoundCorners(0.0f)
{	
	mCustomShader.path = ":/shaders/border.glsl";
}

RectangleComponent::~RectangleComponent()
{

}

void RectangleComponent::render(const Transform4x4f& parentTrans)
{
	if (!mVisible)
		return;

	Transform4x4f trans = parentTrans * getTransform();

	// Don't use soft clip if rotation applied : let renderer do the work
	if (mRotation == 0 && trans.r0().y() == 0)
	{
		auto rect = Renderer::getScreenRect(trans, mSize);
		if (!Renderer::isVisibleOnScreen(rect))
			return;
	}

	Renderer::setMatrix(trans);

	bool rendered = false;

	if (Renderer::supportShaders() && mRoundCorners != 0 && !mCustomShader.path.empty())
	{
		if (mWhiteTexture == nullptr)
			mWhiteTexture = TextureResource::get(":/white.png");			

		if (mWhiteTexture->bind())
		{
			if (mColor != 0)
			{
				const unsigned int color = Renderer::convertColor(mColor);

				Renderer::Vertex mVertices[4];
				mVertices[0] = { { 0.0f, 0.0f  }, { 0.0f, 0.0f }, color };
				mVertices[1] = { { 0.0f, mSize.y()  }, { 0.0f, 1.0f }, color };
				mVertices[2] = { { mSize.x(), 0.0f  }, { 1.0f, 0.0f }, color };
				mVertices[3] = { { mSize.x(), mSize.y()  }, { 1.0f, 1.0f }, color };

				if (mBorderSize != 0 || mRoundCorners != 0)
				{					
					mCustomShader.parameters =
					{
						{ "borderSize", std::to_string(mBorderSize) },
						{ "borderColor", "00000000" },
						{ "cornerRadius", std::to_string(mRoundCorners) }
					};

					mVertices[0].customShader = &mCustomShader;
				}

				Renderer::drawTriangleStrips(&mVertices[0], 4);
			}

			GuiComponent::renderChildren(trans);

			if (mBorderSize != 0 && mBorderColor != 0 && mWhiteTexture->bind())
			{
				mCustomShader.parameters =
				{
					{ "borderSize", std::to_string(mBorderSize) },
					{ "borderColor", Utils::String::toHexString(mBorderColor) },
					{ "cornerRadius", std::to_string(mRoundCorners) }
				};

				Renderer::Vertex mVertices2[4];
				mVertices2[0] = { { 0.0f, 0.0f  }, { 0.0f, 0.0f }, 0 };
				mVertices2[1] = { { 0.0f, mSize.y()  }, { 0.0f, 1.0f }, 0 };
				mVertices2[2] = { { mSize.x(), 0.0f  }, { 1.0f, 0.0f }, 0 };
				mVertices2[3] = { { mSize.x(), mSize.y()  }, { 1.0f, 1.0f }, 0 };
				mVertices2[0].customShader = &mCustomShader;

				Renderer::setMatrix(trans);
				Renderer::drawTriangleStrips(&mVertices2[0], 4);
			}

			rendered = true;
		}
	}

	if (!rendered)
	{
		float borderPadding = mRoundCorners != 0.0f ? (mBorderSize + 1.0f) / 2.0f : 0.0f;

		if (mColor != 0)
		{
			Renderer::setMatrix(trans);
			Renderer::drawSolidRectangle(borderPadding, borderPadding, mSize.x() - 2.0f * borderPadding, mSize.y() - 2.0f * borderPadding, mColor, 0, mBorderSize, mRoundCorners);
		}
		
		GuiComponent::renderChildren(trans);

		if (mBorderSize != 0 && mBorderColor != 0)
		{
			Renderer::setMatrix(trans);
			Renderer::drawSolidRectangle(borderPadding, borderPadding, mSize.x() - 2.0f * borderPadding, mSize.y() - 2.0f * borderPadding, 0, mBorderColor, mBorderSize, mRoundCorners);
		}
	}


}

void RectangleComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	const ThemeData::ThemeElement* elem = theme->getElement(view, element, getThemeTypeName());
	if (!elem)
		return;

	if (elem->has("color"))
		mColor = elem->get<unsigned int>("color");

	if (elem->has("borderColor"))
		mBorderColor = elem->get<unsigned int>("borderColor");

	if (elem->has("borderSize"))
		mBorderSize = elem->get<float>("borderSize");		

	if (elem->has("roundCorners"))
		mRoundCorners = elem->get<float>("roundCorners");

	GuiComponent::applyTheme(theme, view, element, properties);
}

ThemeData::ThemeElement::Property RectangleComponent::getProperty(const std::string name)
{
	if (name == "color")
		return mColor;

	if (name == "borderColor")
		return mBorderColor;

	if (name == "borderSize")
		return mBorderSize;

	if (name == "roundCorners")
		return mRoundCorners;


	return GuiComponent::getProperty(name);
}

void RectangleComponent::setProperty(const std::string name, const ThemeData::ThemeElement::Property& value)
{
	if (value.type == ThemeData::ThemeElement::Property::PropertyType::Int && name == "color")
		mColor = value.i;
	else if (value.type == ThemeData::ThemeElement::Property::PropertyType::Int && name == "borderColor")
		mBorderColor = value.i;
	else if (value.type == ThemeData::ThemeElement::Property::PropertyType::Float && name == "borderSize")
		mBorderSize = value.f;
	else if (value.type == ThemeData::ThemeElement::Property::PropertyType::Float && name == "roundCorners")
		mRoundCorners = value.f;
	else
		GuiComponent::setProperty(name, value);
}
