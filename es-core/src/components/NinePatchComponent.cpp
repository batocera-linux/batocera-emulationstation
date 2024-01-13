#include "components/NinePatchComponent.h"

#include "resources/TextureResource.h"
#include "Log.h"
#include "ThemeData.h"

NinePatchComponent::NinePatchComponent(Window* window, const std::string& path, unsigned int edgeColor, unsigned int centerColor) : GuiComponent(window),
	mCornerSize(16, 16),
	mEdgeColor(edgeColor), mCenterColor(centerColor),
	mVertices(NULL), mCustomShaderIsPostProcess(false), mCustomShaderIsCacheable(true), mPostProcessTextureId(-1)
{
	mTimer = 0;
	mAnimateTiming = 0;
	mAnimateColor = 0xFFFFFFFF;
	
	mPreviousSize = Vector2f(0, 0);
	setImagePath(path);
}

NinePatchComponent::~NinePatchComponent()
{
	if (mPostProcessTextureId != -1)
	{
		Renderer::destroyTexture(mPostProcessTextureId);
		mPostProcessTextureId = -1;
	}

	if (mTexture != nullptr)
		mTexture->setRequired(false);

	if (mVertices != NULL)
		delete[] mVertices;
}

void NinePatchComponent::onOpacityChanged()
{
	updateColors();
}

void NinePatchComponent::update(int deltaTime)
{
	GuiComponent::update(deltaTime);

	if (mAnimateTiming > 0)
	{
		mTimer += deltaTime;
		if (mTimer >= 2 * mAnimateTiming)
			mTimer = 0;
	}
}

void NinePatchComponent::updateColors()
{
	if (mPostProcessTextureId != -1)
	{
		Renderer::destroyTexture(mPostProcessTextureId);
		mPostProcessTextureId = -1;
	}

	if (mVertices == nullptr)
		return;

	float opacity = getOpacity() / 255.0;

	unsigned int e = mEdgeColor;
	unsigned int c = mCenterColor;

	if (mAnimateTiming > 0)
	{
		float percent = std::abs(mAnimateTiming - mTimer) / mAnimateTiming;
		e = Renderer::mixColors(e, mAnimateColor, percent);
		c = Renderer::mixColors(e, mAnimateColor, percent);
	}

	const unsigned int edgeColor = Renderer::convertColor(e & 0xFFFFFF00 | (unsigned char)((e & 0xFF) * opacity));
	const unsigned int centerColor = Renderer::convertColor(c & 0xFFFFFF00 | (unsigned char)((c & 0xFF) * opacity));

	for(int i = 0; i < 6*9; i++)
		mVertices[i].col = edgeColor;

	for(int i = 0; i < 6; i++)
		mVertices[(4 * 6) + i].col = centerColor;
}

void NinePatchComponent::buildVertices()
{
	if(mTexture == nullptr)
		return;

	if(mVertices != NULL)
		delete[] mVertices;	

	if(mTexture->getSize() == Vector2i::Zero())
	{
		mVertices = NULL;
		LOG(LogWarning) << "NinePatchComponent missing texture!";
		return;
	}

	mVertices = new Renderer::Vertex[6 * 9];

	const Vector2f texSize = Vector2f((float)mTexture->getSize().x(), (float)mTexture->getSize().y());
	if (texSize.x() <= 0 || texSize.y() <= 0)
		return;

	const float imgSizeX[3] = { mCornerSize.x(), mSize.x() - mCornerSize.x() * 2 - mPadding.x() - mPadding.z(), mCornerSize.x()};
	const float imgSizeY[3] = { mCornerSize.y(), mSize.y() - mCornerSize.y() * 2 - mPadding.y() - mPadding.w(), mCornerSize.y()};
	const float imgPosX[3]  = { mPadding.x(), mPadding.y() + imgSizeX[0], mPadding.x() + imgSizeX[0] + imgSizeX[1] };
	const float imgPosY[3]  = { mPadding.x(), mPadding.y() + imgSizeY[0], mPadding.y() + imgSizeY[0] + imgSizeY[1] };
	
	//the "1 +" in posY and "-" in sizeY is to deal with texture coordinates having a bottom left corner origin vs. verticies having a top left origin
	const float texSizeX[3] = {  mCornerSize.x() / texSize.x(),  (texSize.x() - mCornerSize.x() * 2) / texSize.x(),  mCornerSize.x() / texSize.x() };
	const float texSizeY[3] = { -mCornerSize.y() / texSize.y(), -(texSize.y() - mCornerSize.y() * 2) / texSize.y(), -mCornerSize.y() / texSize.y() };
	const float texPosX[3]  = {  0,     texSizeX[0],     texSizeX[0] + texSizeX[1] };
	const float texPosY[3]  = {  1, 1 + texSizeY[0], 1 + texSizeY[0] + texSizeY[1] };

	int v = 0;
	for(int slice = 0; slice < 9; slice++)
	{
		const int      sliceX  = slice % 3;
		const int      sliceY  = slice / 3;
		const Vector2f imgPos  = Vector2f(imgPosX[sliceX], imgPosY[sliceY]);
		const Vector2f imgSize = Vector2f(imgSizeX[sliceX], imgSizeY[sliceY]);
		const Vector2f texPos  = Vector2f(texPosX[sliceX], texPosY[sliceY]);
		const Vector2f texSize = Vector2f(texSizeX[sliceX], texSizeY[sliceY]);

		mVertices[v + 1] = { { imgPos.x()              , imgPos.y()               }, { texPos.x(),               texPos.y()               }, 0 };
		mVertices[v + 2] = { { imgPos.x()              , imgPos.y() + imgSize.y() }, { texPos.x(),               texPos.y() + texSize.y() }, 0 };
		mVertices[v + 3] = { { imgPos.x() + imgSize.x(), imgPos.y()               }, { texPos.x() + texSize.x(), texPos.y()               }, 0 };
		mVertices[v + 4] = { { imgPos.x() + imgSize.x(), imgPos.y() + imgSize.y() }, { texPos.x() + texSize.x(), texPos.y() + texSize.y() }, 0 };

		// round vertices
		// for(int i = 1; i < 5; ++i)
		//	mVertices[v + i].pos.round();

		// make duplicates of first and last vertex so this can be rendered as a triangle strip
		mVertices[v + 0] = mVertices[v + 1];
		mVertices[v + 5] = mVertices[v + 4];

		v += 6;
	}

	updateColors();
}

void NinePatchComponent::render(const Transform4x4f& parentTrans)
{
	if (!mVisible || mTexture == nullptr || mVertices == nullptr)
		return;

	Transform4x4f trans = parentTrans * getTransform();

	auto rect = Renderer::getScreenRect(trans, mSize);
	if (mRotation == 0 && trans.r0().y() == 0 && !Renderer::isVisibleOnScreen(rect))
		return;

	if (!mCustomShader.path.empty() && mCustomShaderIsPostProcess)
	{
		int margin = 50; // Apply pixel margin for postProcessShader

		int x = Math::max(0.0f, rect.x + mCornerSize.x() / 2.0f - margin);
		int y = Math::max(0.0f, rect.y + mCornerSize.y() / 2.0f - margin);
		int w = Math::min((float) Renderer::getScreenWidth(), rect.w - mCornerSize.x() + 2 * margin);
		int h = Math::min((float) Renderer::getScreenHeight(), rect.h - mCornerSize.y() + 2 * margin);

		if (Renderer::getScreenRotate() == 0)
		{
			if (mPostProcessTextureId == (unsigned int)-1)
				Renderer::postProcessShader(mCustomShader.path, x, y, w, h, mCustomShader.parameters, &mPostProcessTextureId);
			
			if (mPostProcessTextureId != (unsigned int) -1)
			{
				float xM = 0.0f;
				float yM = 0.0f;

				unsigned int color = 0xFFFFFF00 | getOpacity();

				Renderer::Vertex vertices[4];
				vertices[0] = { { (float)x    , (float)y       }, { xM,        1.0f - yM }, color };
				vertices[1] = { { (float)x    , (float)y + h   }, { xM,        yM        }, color };
				vertices[2] = { { (float)x + w, (float)y       }, { 1.0f - xM, 1.0f - yM }, color };
				vertices[3] = { { (float)x + w, (float)y + h   }, { 1.0f - xM, yM        }, color };

				Renderer::setMatrix(Transform4x4f::Identity());
				Renderer::bindTexture(mPostProcessTextureId);

				x = rect.x + mCornerSize.x() / 2.0f;
				y = rect.y + mCornerSize.y() / 2.0f;
				w = rect.w - mCornerSize.x();
				h = rect.h - mCornerSize.y();

				Renderer::pushClipRect(x, y, w, h);
				Renderer::drawTriangleStrips(&vertices[0], 4);
				Renderer::popClipRect();			

				if (!mCustomShaderIsCacheable)
				{
					Renderer::destroyTexture(mPostProcessTextureId);
					mPostProcessTextureId = (unsigned int)-1;
				}
			}
		}
		else
		{
			Renderer::postProcessShader(mCustomShader.path,
				rect.x + mCornerSize.x() / 2.0f,
				rect.y + mCornerSize.y() / 2.0f,
				rect.w - mCornerSize.x(),
				rect.h - mCornerSize.y(),
				mCustomShader.parameters);
		}
	}

	// Apply cornersize < 0 only with default ":/frame.png" image, not with customized ones
	if (mCornerSize.x() <= 1 && mCornerSize.y() <= 1 && mCornerSize.x() == mCornerSize.y() && mPath == ":/frame.png")
	{
		float opacity = getOpacity() / 255.0;

		unsigned int e = mEdgeColor;

		if (mAnimateTiming > 0)
		{
			float percent = std::abs(mAnimateTiming - mTimer) / mAnimateTiming;
			e = Renderer::mixColors(e, mAnimateColor, percent);
		}

		const unsigned int edgeColor = e & 0xFFFFFF00 | (unsigned char)((e & 0xFF) * opacity);

		Renderer::setMatrix(trans);
		
		if (mCornerSize.x() > 0)
		{
			int radius = Math::max(mSize.x(), mSize.y()) * mCornerSize.x();			
			Renderer::drawRoundRect(0, 0, mSize.x(), mSize.y(), radius, edgeColor);
		}
		else
			Renderer::drawRect(0.0, 0.0, mSize.x(), mSize.y(), edgeColor, edgeColor);
	}
	else if (mTexture->bind())
	{
		if (mAnimateTiming > 0)
		{
			float opacity = getOpacity() / 255.0;

			unsigned int e = mEdgeColor;
			unsigned int c = mCenterColor;

			float percent = std::abs(mAnimateTiming - mTimer) / mAnimateTiming;
			e = Renderer::mixColors(e, mAnimateColor, percent);
			c = Renderer::mixColors(c, mAnimateColor, percent);

			const unsigned int edgeColor = Renderer::convertColor(e & 0xFFFFFF00 | (unsigned char)((e & 0xFF) * opacity));
			const unsigned int centerColor = Renderer::convertColor(c & 0xFFFFFF00 | (unsigned char)((c & 0xFF) * opacity));

			for (int i = 0; i < 6 * 9; i++)
				mVertices[i].col = edgeColor;

			for (int i = 0; i < 6; i++)
				mVertices[(4 * 6) + i].col = centerColor;
		}

		Renderer::setMatrix(trans);
		Renderer::drawTriangleStrips(&mVertices[0], 6 * 9);

		if (mAnimateTiming > 0)
			updateColors();
	}

	renderChildren(trans);
}

const Vector2f& NinePatchComponent::getCornerSize() const
{
	return mCornerSize;
}

void NinePatchComponent::setCornerSize(float sizeX, float sizeY)
{
	if (mCornerSize.x() == sizeX && mCornerSize.y() == sizeY)
		return;

	mCornerSize = Vector2f(sizeX, sizeY);
	buildVertices();
}

void NinePatchComponent::fitTo(Vector2f size, Vector3f position, Vector2f padding)
{
	size += padding;
	position[0] -= padding.x() / 2;
	position[1] -= padding.y() / 2;

	setSize(size + mCornerSize * 2);
	setPosition(position.x() + Math::lerp(-mCornerSize.x(), mCornerSize.x(), mOrigin.x()),
				position.y() + Math::lerp(-mCornerSize.y(), mCornerSize.y(), mOrigin.y()));
}

void NinePatchComponent::setImagePath(const std::string& path)
{
	if (mPath == path)
		return;

	mPath = path;

	if (mTexture != nullptr)
		mTexture->setRequired(false);

	auto prev = mTexture;
	mTexture = TextureResource::get(mPath, false, true);

	if (isShowing() && mTexture != nullptr)
		mTexture->setRequired(true);

	buildVertices();
}

void NinePatchComponent::setEdgeColor(unsigned int edgeColor)
{
	if (mEdgeColor == edgeColor)
		return;

	mEdgeColor = edgeColor;
	updateColors();
}

void NinePatchComponent::setCenterColor(unsigned int centerColor)
{
	if (mCenterColor == centerColor)
		return;

	mCenterColor = centerColor;
	updateColors();
}

void NinePatchComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	GuiComponent::applyTheme(theme, view, element, properties);

	using namespace ThemeFlags;

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, "ninepatch");
	if(!elem)
		return;

	if(properties & PATH && elem->has("path"))
		setImagePath(elem->get<std::string>("path"));

	if(properties & COLOR)
	{
		if(elem->has("color"))
		{
			setCenterColor(elem->get<unsigned int>("color"));
			setEdgeColor(elem->get<unsigned int>("color"));
		}

		if(elem->has("centerColor"))
			setCenterColor(elem->get<unsigned int>("centerColor"));

		if(elem->has("edgeColor"))
			setEdgeColor(elem->get<unsigned int>("edgeColor"));
	}

	if(elem->has("cornerSize"))
		setCornerSize(elem->get<Vector2f>("cornerSize"));

	if (elem->has("animateColor"))
		setAnimateColor(elem->get<unsigned int>("animateColor"));

	if (elem->has("animateColorTime"))
		setAnimateTiming(elem->get<float>("animateColorTime"));

	if (elem->has("padding"))
	{
		Vector2f scale = getParent() ? getParent()->getSize() : Vector2f((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

		auto padding = elem->get<Vector4f>("padding");
		if (abs(padding.x()) < 1 && abs(padding.y()) < 1 && abs(padding.z()) < 1 && abs(padding.w()) < 1)
			setPadding(padding * Vector4f(scale.x(), scale.y(), scale.x(), scale.y()));
		else
			setPadding(padding); // Pixel size
	}
	//	setPadding(elem->get<Vector4f>("padding"));
}

void NinePatchComponent::onShow()
{
	GuiComponent::onShow();

	if (mTexture != nullptr)
		mTexture->setRequired(true);	
}

void NinePatchComponent::onHide()
{
	GuiComponent::onHide();

	if (mTexture != nullptr)
		mTexture->setRequired(false);	
}

ThemeData::ThemeElement::Property NinePatchComponent::getProperty(const std::string name)
{
	if (name == "color")
		return mCenterColor;
	
	if (name == "centerColor")
		return mCenterColor;
	
	if (name == "edgeColor")
		return mEdgeColor;
	
	if (name == "cornerSize")
		return mCornerSize;
	
	if (name == "animateColor")
		return mAnimateColor;
	
	if (name == "path")
		return mPath;

	return GuiComponent::getProperty(name);
}

void NinePatchComponent::setProperty(const std::string name, const ThemeData::ThemeElement::Property& value)
{
	if (name == "color" && value.type == ThemeData::ThemeElement::Property::PropertyType::Int)
	{
		setCenterColor(value.i);
		setEdgeColor(value.i);
	}
	else if (name == "centerColor" && value.type == ThemeData::ThemeElement::Property::PropertyType::Int)
		setCenterColor(value.i);
	else if (name == "edgeColor" && value.type == ThemeData::ThemeElement::Property::PropertyType::Int)
		setEdgeColor(value.i);
	else if (name == "animateColor" && value.type == ThemeData::ThemeElement::Property::PropertyType::Int)
		setAnimateColor(value.i);
	else if (name == "cornerSize" && value.type == ThemeData::ThemeElement::Property::PropertyType::Pair)
		setCornerSize(value.v);
	else if (name == "path" && value.type == ThemeData::ThemeElement::Property::PropertyType::String)
		setImagePath(value.s);
	else
		GuiComponent::setProperty(name, value);
}

void NinePatchComponent::onSizeChanged()
{
	GuiComponent::onSizeChanged();

	if (mPreviousSize == mSize)
		return;

	mPreviousSize = mSize;
	buildVertices();
}

void NinePatchComponent::onPaddingChanged() 
{ 
	buildVertices();
}

void NinePatchComponent::setPostProcessShader(const Renderer::ShaderInfo& shader, bool cacheable)
{
	mCustomShader = shader;
	mCustomShaderIsCacheable = cacheable;
	mCustomShaderIsPostProcess = true;
}