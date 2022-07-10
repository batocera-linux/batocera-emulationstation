#pragma once
#ifndef ES_APP_BINDING_H
#define ES_APP_BINDING_H

class SystemData;
class FileData;
class TextComponent;
class ImageComponent;
class GuiComponent;
class VideoComponent;

class Binding
{
public:
	static void updateBindings(TextComponent* comp, SystemData* system, bool showDefaultText = true);	
	static void updateBindings(GuiComponent* comp, FileData* file, bool showDefaultText = true);
};

#endif
