#pragma once

#include "math/Vector2i.h"

struct GridViewportState
{
	Vector2i dimensions = Vector2i::Zero();
	int entryCount = 0;
	int cursorIndex = -1;
	int startPosition = 0;
	int centerSelection = 0;
	bool vertical = true;
	bool scrollLoop = false;
};
