#pragma once
#ifndef ES_CORE_COMPONENTS_QR_CODE_COMPONENT_H
#define ES_CORE_COMPONENTS_QR_CODE_COMPONENT_H

#include "GuiComponent.h"
#include <string>
#include <vector>

// Renders a QR code for a short text (typically a URL).
// Encoding uses the vendored Nayuki qrcodegen library (MIT, utils/qrcodegen.hpp).
// The code is drawn as dark squares on a light square that includes the standard
// 4-module quiet zone, so it scans on dark themes too.
class QrCodeComponent : public GuiComponent
{
public:
	QrCodeComponent(Window* window);

	// Encodes the text. Returns false (and clears the code) if it cannot be encoded.
	bool setText(const std::string& text);
	inline bool hasCode() const { return mModuleCount > 0; }

	void render(const Transform4x4f& parentTrans) override;

private:
	std::vector<bool> mModules;
	int mModuleCount;
};

#endif // ES_CORE_COMPONENTS_QR_CODE_COMPONENT_H
