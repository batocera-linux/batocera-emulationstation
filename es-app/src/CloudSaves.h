#pragma once
#ifndef ES_APP_SERVICES_CLOUDSAVE_SERVICE_H
#define ES_APP_SERVICES_CLOUDSAVE_SERVICE_H

#include "Window.h"
#include "FileData.h"
#include "GuiComponent.h"

class CloudSaves
{
  public:
    static CloudSaves& getInstance()
    {
      static CloudSaves instance;
      return instance;
    }
  private:
    CloudSaves() {}
    CloudSaves(CloudSaves const&);
    void operator=(CloudSaves const&);
  public:
    void save(Window* window, FileData* game);
    void load(Window* window, FileData *game, GuiComponent* guiComp,
    const std::function<void(GuiComponent*)>& callback);
    bool isSupported(FileData* game);
};

extern void guiSaveStateLoad(Window* window, FileData* game);

#endif
