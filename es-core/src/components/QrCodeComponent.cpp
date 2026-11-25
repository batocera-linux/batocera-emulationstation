#include "components/QrCodeComponent.h"
#include "renderers/Renderer.h"
#include "utils/qrcodegen.hpp"
#include "Log.h"

#include <algorithm>
#include <cmath>

#define QR_QUIET_ZONE 4

QrCodeComponent::QrCodeComponent(Window* window) : GuiComponent(window), mModuleCount(0)
{
}

bool QrCodeComponent::setText(const std::string& text)
{
	mModules.clear();
	mModuleCount = 0;

	if (text.empty())
		return false;

	try
	{
		qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(text.c_str(), qrcodegen::QrCode::Ecc::MEDIUM);

		int size = qr.getSize();
		mModules.resize((size_t)size * size);

		for (int y = 0; y < size; y++)
			for (int x = 0; x < size; x++)
				mModules[(size_t)y * size + x] = qr.getModule(x, y);

		mModuleCount = size;
		return true;
	}
	catch (const std::exception& e)
	{
		LOG(LogError) << "QrCodeComponent: unable to encode text : " << e.what();
	}

	return false;
}

void QrCodeComponent::render(const Transform4x4f& parentTrans)
{
	if (!mVisible || mModuleCount <= 0)
		return;

	Transform4x4f trans = parentTrans * getTransform();
	Renderer::setMatrix(trans);

	const int total = mModuleCount + QR_QUIET_ZONE * 2;
	const float side = std::min(mSize.x(), mSize.y());

	// Integer module size keeps edges crisp; fall back to fractional on tiny sizes.
	float cell = std::floor(side / (float)total);
	if (cell < 1.0f)
		cell = side / (float)total;

	const float codeSide = cell * total;
	const float x0 = std::round((mSize.x() - codeSide) / 2.0f);
	const float y0 = std::round((mSize.y() - codeSide) / 2.0f);

	// Light background, quiet zone included
	Renderer::drawRect(x0, y0, codeSide, codeSide, 0xFFFFFFFF);

	// Dark modules, merged into horizontal runs to limit draw calls
	for (int y = 0; y < mModuleCount; y++)
	{
		int x = 0;
		while (x < mModuleCount)
		{
			if (!mModules[(size_t)y * mModuleCount + x])
			{
				x++;
				continue;
			}

			int start = x;
			while (x < mModuleCount && mModules[(size_t)y * mModuleCount + x])
				x++;

			Renderer::drawRect(
				x0 + (start + QR_QUIET_ZONE) * cell,
				y0 + (y + QR_QUIET_ZONE) * cell,
				(x - start) * cell, cell, 0x000000FF);
		}
	}
}
