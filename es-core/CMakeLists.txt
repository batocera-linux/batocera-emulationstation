project("core")

set(CORE_HEADERS
	${CMAKE_CURRENT_SOURCE_DIR}/src/AsyncHandle.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/AudioManager.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/CECInput.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/GuiComponent.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/HelpStyle.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/HttpReq.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/ImageIO.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/InputConfig.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/InputManager.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/Log.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/MameNames.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/LocaleES.h # batocera
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SystemConf.h # batocera
	${CMAKE_CURRENT_SOURCE_DIR}/src/platform.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/PowerSaver.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/Settings.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/Sound.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/Splash.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/ThemeData.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/VolumeControl.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/Window.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/MultiStateInput.h

	# Animations
	${CMAKE_CURRENT_SOURCE_DIR}/src/animations/Animation.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/animations/AnimationController.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/animations/LambdaAnimation.h

	# Theme animations
	${CMAKE_CURRENT_SOURCE_DIR}/src/anim/ThemeAnimation.h	
	${CMAKE_CURRENT_SOURCE_DIR}/src/anim/ThemeStoryboard.h	
	${CMAKE_CURRENT_SOURCE_DIR}/src/anim/StoryboardAnimator.h
	
	# GuiComponents
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/AnimatedImageComponent.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/components/AsyncNotificationComponent.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/BatteryIndicatorComponent.h	
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/BusyComponent.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/ButtonComponent.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/ControllerActivityComponent.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/ComponentGrid.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/ComponentList.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/DateTimeComponent.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/DateTimeEditComponent.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/HelpComponent.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/GridTileComponent.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/IList.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/ImageComponent.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/ImageGridComponent.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/MenuComponent.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/NinePatchComponent.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/OptionListComponent.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/ScrollableContainer.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/SliderComponent.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/SwitchComponent.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/TextComponent.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/TextEditComponent.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/VideoComponent.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/VideoPlayerComponent.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/VideoVlcComponent.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/VolumeInfoComponent.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/MultiLineMenuEntry.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/ScrollbarComponent.h	
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/ComponentTab.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/WebImageComponent.h

	# Guis
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiDetectDevice.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiInputConfig.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiMsgBox.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiInfoPopup.h # batocera
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiTextEditPopup.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiTextEditPopupKeyboard.h # batocera

	# Math
	${CMAKE_CURRENT_SOURCE_DIR}/src/math/Misc.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/math/Transform4x4f.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/math/Vector2f.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/math/Vector2i.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/math/Vector3f.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/math/Vector4f.h

	# Renderers
	${CMAKE_CURRENT_SOURCE_DIR}/src/renderers/Renderer.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/renderers/GlExtensions.h	

	# Resources
	${CMAKE_CURRENT_SOURCE_DIR}/src/resources/Font.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/resources/ResourceManager.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/resources/TextureResource.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/resources/TextureData.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/resources/TextureDataManager.h

	# Utils
	${CMAKE_CURRENT_SOURCE_DIR}/src/utils/FileSystemUtil.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/utils/StringUtil.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/utils/TimeUtil.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/utils/ThreadPool.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/utils/zip_file.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/utils/ZipFile.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/utils/md5.h
)

set(CORE_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/src/AudioManager.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/CECInput.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/GuiComponent.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/HelpStyle.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/HttpReq.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/ImageIO.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/InputConfig.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/InputManager.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Log.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/MameNames.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/LocaleES.cpp # batocera
	${CMAKE_CURRENT_SOURCE_DIR}/src/platform.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/PowerSaver.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Scripting.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Settings.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Sound.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Splash.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/ThemeData.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/VolumeControl.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Window.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/MultiStateInput.cpp

	# Animations
	${CMAKE_CURRENT_SOURCE_DIR}/src/animations/AnimationController.cpp	
	
	# Theme animations
	${CMAKE_CURRENT_SOURCE_DIR}/src/anim/ThemeStoryboard.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/anim/StoryboardAnimator.cpp
	
	# GuiComponents
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/AnimatedImageComponent.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/AsyncNotificationComponent.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/BatteryIndicatorComponent.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/BusyComponent.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/ButtonComponent.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/ControllerActivityComponent.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/ComponentGrid.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/ComponentList.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/DateTimeComponent.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/DateTimeEditComponent.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/HelpComponent.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/GridTileComponent.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/ImageComponent.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/MenuComponent.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/NinePatchComponent.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/ScrollableContainer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/SliderComponent.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/SwitchComponent.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/TextComponent.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/TextEditComponent.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/VideoComponent.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/VideoPlayerComponent.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/VideoVlcComponent.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/VolumeInfoComponent.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/ScrollbarComponent.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/MultiLineMenuEntry.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/ComponentTab.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/components/WebImageComponent.cpp	

	# Guis
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiDetectDevice.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiInputConfig.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiMsgBox.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiInfoPopup.cpp # batocera
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiTextEditPopup.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiTextEditPopupKeyboard.cpp # batocera
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SystemConf.cpp # batocera

	# Math
	${CMAKE_CURRENT_SOURCE_DIR}/src/math/Misc.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/math/Transform4x4f.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/math/Vector2f.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/math/Vector2i.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/math/Vector3f.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/math/Vector4f.cpp

	# Renderer
	${CMAKE_CURRENT_SOURCE_DIR}/src/renderers/Renderer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/renderers/Renderer_GL21.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/renderers/Renderer_GLES10.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/renderers/Renderer_GLES20.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/renderers/GlExtensions.cpp	
	${CMAKE_CURRENT_SOURCE_DIR}/src/renderers/Shader.cpp	

	# Resources
	${CMAKE_CURRENT_SOURCE_DIR}/src/resources/Font.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/resources/ResourceManager.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/resources/TextureResource.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/resources/TextureData.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/resources/TextureDataManager.cpp

	# Utils
	${CMAKE_CURRENT_SOURCE_DIR}/src/utils/FileSystemUtil.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/utils/StringUtil.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/utils/TimeUtil.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/utils/ThreadPool.cpp	
	${CMAKE_CURRENT_SOURCE_DIR}/src/utils/ZipFile.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/utils/md5.cpp
)

# Keep Directory structure in Visual Studio
if(MSVC)
	file(
		 GLOB_RECURSE source_list RELATIVE
		 "${CMAKE_CURRENT_SOURCE_DIR}"
		 *.cpp *.c
		)
	foreach(source IN LISTS source_list)
		get_filename_component(source_path "${source}" PATH)
		string(REPLACE "/" "\\" source_path_msvc "${source_path}")
		string(REPLACE "src" "" source_path_final "${source_path_msvc}")
		source_group("${source_path_final}" FILES "${source}")
	endforeach()  	
endif()

include_directories(${COMMON_INCLUDE_DIRS})
add_library(es-core STATIC ${CORE_SOURCES} ${CORE_HEADERS})
target_link_libraries(es-core ${COMMON_LIBRARIES})
