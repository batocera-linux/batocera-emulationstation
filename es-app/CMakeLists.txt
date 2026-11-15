project("emulationstation")


set(ES_HEADERS	
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EmulationStation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FileData.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/GameLauncher.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/HotkeyExitWatcher.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/FileTag.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FileSorts.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MetaData.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PlatformId.h    
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SystemData.h    
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Gamelist.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/Genres.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FileFilterIndex.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SystemScreenSaver.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CollectionSystemManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/NetworkThread.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/ContentInstaller.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/catalog/CatalogInstaller.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/bezels/BezelProjectList.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/bezels/TheBezelProject.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/updates/Release.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/updates/SelfUpdater.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/system/SystemServices.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/catalog/EmulatorCatalog.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/catalog/CatalogSystemConfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scrapers/ThreadedScraper.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/ThreadedHasher.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/ThreadedBluetooth.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/SystemRandomPlaylist.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/LangParser.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/KeyboardMapping.h	
	${CMAKE_CURRENT_SOURCE_DIR}/src/services/HttpServerThread.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/services/HttpApi.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/services/HttpFileApi.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/services/HttpFileAccess.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/services/httplib.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/RetroAchievements.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/SaveState.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/SaveStateRepository.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SaveStateConfigFile.h    
	${CMAKE_CURRENT_SOURCE_DIR}/src/CustomFeatures.h

    # GuiComponents    
    ${CMAKE_CURRENT_SOURCE_DIR}/src/components/ScraperSearchComponent.h    

    # Guis    
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiMetaDataEd.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiGameScraper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiGameOptions.h    
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiGamelistOptions.h    
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiGeneralScreensaverOptions.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiMenu.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiSettings.h    
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiScraperStart.h		
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiHashStart.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiGamelistFilter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiCollectionSystemsOptions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiLoading.h # batocera
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiThemeInstaller.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiBezelInstaller.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiEmulatorCatalog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiNetPlay.h # batocera
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiNetPlaySettings.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiRetroAchievements.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiRetroAchievementsSettings.h	
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiSystemInformation.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiBluetoothDevices.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiBluetoothPair.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiBluetoothDeviceOptions.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiWifi.h # batocera
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiImageViewer.h # batocera
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiBios.h # batocera	
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiControllersSettings.h # batocera
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiKeyboardtopads.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiKeyMappingEditor.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiKeyboardLayout.h	
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiGameAchievements.h		
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiSaveState.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiFileBrowser.h	
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiFavoriteMusicSelector.h	

    # Scrapers
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scrapers/Scraper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scrapers/ScraperKeyFormat.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scrapers/ScraperKeys.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scrapers/GamesDBJSONScraper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scrapers/GamesDBJSONScraperResources.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scrapers/ArcadeDBJSONScraper.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/scrapers/HfsDBScraper.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/scrapers/IGDBScraper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scrapers/ScreenScraper.h    

    # Views
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/gamelist/BasicGameListView.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/views/gamelist/CarouselGameListView.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/gamelist/DetailedGameListView.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/gamelist/IGameListView.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/gamelist/ISimpleGameListView.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/gamelist/GridGameListView.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/gamelist/VideoGameListView.h
	${CMAKE_CURRENT_SOURCE_DIR}/src/views/gamelist/DetailedContainer.h	
	${CMAKE_CURRENT_SOURCE_DIR}/src/views/gamelist/GameNameFormatter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/SystemView.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/ViewController.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/UIModeController.h

    # Animations
    ${CMAKE_CURRENT_SOURCE_DIR}/src/animations/LaunchAnimation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/animations/MoveCameraAnimation.h

    ${CMAKE_CURRENT_SOURCE_DIR}/src/ApiSystem.h # batocera
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ZaparooSupport.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/LibretroRatio.h # batocera
)

set(ES_SOURCES
    # first: the longest file to compile, so that a parallel build starts it early
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiMenu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FileData.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/GameLauncher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/HotkeyExitWatcher.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/FileTag.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FileSorts.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MetaData.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PlatformId.cpp    
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SystemData.cpp    
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Gamelist.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Genres.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/FileFilterIndex.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SystemScreenSaver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CollectionSystemManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/NetworkThread.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/ContentInstaller.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/catalog/CatalogInstaller.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/bezels/BezelProjectList.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/bezels/TheBezelProject.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/updates/Release.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/updates/SelfUpdater.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/system/SystemServices.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/catalog/EmulatorCatalog.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/catalog/CatalogSystemConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scrapers/ThreadedScraper.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/ThreadedHasher.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/ThreadedBluetooth.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/SystemRandomPlaylist.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/LangParser.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/KeyboardMapping.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/services/HttpServerThread.cpp	
	${CMAKE_CURRENT_SOURCE_DIR}/src/services/HttpApi.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/services/HttpFileApi.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/services/HttpFileAccess.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/RetroAchievements.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/SaveState.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/SaveStateRepository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SaveStateConfigFile.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/CustomFeatures.cpp	

    # GuiComponents    
    ${CMAKE_CURRENT_SOURCE_DIR}/src/components/ScraperSearchComponent.cpp	

    # Guis    
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiMetaDataEd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiGameScraper.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiGameOptions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiGamelistOptions.cpp    
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiGeneralScreensaverOptions.cpp    
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiSettings.cpp    
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiScraperStart.cpp		
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiHashStart.cpp	
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiGamelistFilter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiCollectionSystemsOptions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiThemeInstaller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiBezelInstaller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiEmulatorCatalog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiNetPlay.cpp # batocera
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiNetPlaySettings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiRetroAchievements.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiRetroAchievementsSettings.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiSystemInformation.cpp	
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiBluetoothDevices.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiBluetoothPair.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiBluetoothDeviceOptions.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiWifi.cpp # batocera
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiImageViewer.cpp # batocera
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiBios.cpp # batocera	
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiControllersSettings.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiKeyboardtopads.cpp # batocera
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiKeyMappingEditor.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiKeyboardLayout.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiGameAchievements.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiSaveState.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiFileBrowser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/guis/GuiFavoriteMusicSelector.cpp

    # Scrapers
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scrapers/Scraper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scrapers/ScraperKeyFormat.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scrapers/ScraperKeys.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scrapers/GamesDBJSONScraper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scrapers/GamesDBJSONScraperResources.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scrapers/ArcadeDBJSONScraper.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/scrapers/HfsDBScraper.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/scrapers/IGDBScraper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scrapers/ScreenScraper.cpp	

    # Views
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/gamelist/BasicGameListView.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/views/gamelist/CarouselGameListView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/gamelist/DetailedGameListView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/gamelist/IGameListView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/gamelist/ISimpleGameListView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/gamelist/GridGameListView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/gamelist/VideoGameListView.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/views/gamelist/DetailedContainer.cpp	
	${CMAKE_CURRENT_SOURCE_DIR}/src/views/gamelist/GameNameFormatter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/SystemView.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/views/ViewController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views/UIModeController.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/ApiSystem.cpp # batocera
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ZaparooSupport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/LibretroRatio.cpp # batocera
    ${CMAKE_CURRENT_SOURCE_DIR}/src/BuildInfo.cpp
)


#-------------------------------------------------------------------------------
# define target
include_directories(${COMMON_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_executable(emulationstation ${ES_SOURCES} ${ES_HEADERS})
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/BuildInfo.cpp APPEND PROPERTY
    COMPILE_DEFINITIONS ES_VERSION_STRING="${ES_VERSION_RESOLVED}")
target_link_libraries(emulationstation ${COMMON_LIBRARIES} es-core es-launcher)
if(ES_PCH)
    target_precompile_headers(emulationstation PRIVATE ${ES_PRECOMPILED_HEADERS})
endif()

# libes-launcher.so is installed in the lib folder of the same prefix; outside the default
# library paths (/usr/local/lib on some systems) the executable still finds it
set_target_properties(emulationstation PROPERTIES INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")

# Keep the executable at the top of the build folder, next to the compiled translations
# (locale/) and a link to the resources, so that it runs from there without installing.
set_target_properties(emulationstation PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
add_custom_command(TARGET emulationstation POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_SOURCE_DIR}/resources ${CMAKE_BINARY_DIR}/resources
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_SOURCE_DIR}/features/es_features.cfg ${CMAKE_BINARY_DIR}/es_features.cfg
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_SOURCE_DIR}/controllers/es_input_batocera.cfg ${CMAKE_BINARY_DIR}/es_input_batocera.cfg
    VERBATIM)

#-------------------------------------------------------------------------------
# unit tests: cmake -DES_TESTS=ON, then ctest

option(ES_TESTS "Build the unit tests of es-app" OFF)
if(ES_TESTS)
    enable_testing()
    add_executable(es-app-catalog-tests
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/catalog_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/catalog/EmulatorCatalog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/catalog/CatalogSystemConfig.cpp
        ${CMAKE_SOURCE_DIR}/es-core/src/utils/Sha256.cpp
        ${CMAKE_SOURCE_DIR}/es-core/src/utils/StringUtil.cpp)
    target_include_directories(es-app-catalog-tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/es-core/src
        ${CMAKE_SOURCE_DIR}/external ${RAPIDJSON_INCLUDE_DIRS})
    target_link_libraries(es-app-catalog-tests PRIVATE pugixml)
    target_compile_options(es-app-catalog-tests PRIVATE -Wall -Wextra)
    add_test(NAME es-app-catalog-tests COMMAND es-app-catalog-tests)

    add_executable(es-app-scraper-key-format-tests
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/scraper_key_format_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/scrapers/ScraperKeyFormat.cpp)
    target_include_directories(es-app-scraper-key-format-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(es-app-scraper-key-format-tests PRIVATE -Wall -Wextra)
    add_test(NAME es-app-scraper-key-format-tests COMMAND es-app-scraper-key-format-tests)

    add_executable(es-app-update-tests
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/update_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/updates/Release.cpp)
    target_include_directories(es-app-update-tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src ${RAPIDJSON_INCLUDE_DIRS})
    target_compile_options(es-app-update-tests PRIVATE -Wall -Wextra)
    add_test(NAME es-app-update-tests COMMAND es-app-update-tests)

    add_executable(es-app-bezelproject-tests
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/bezelproject_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/bezels/BezelProjectList.cpp)
    target_include_directories(es-app-bezelproject-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_options(es-app-bezelproject-tests PRIVATE -Wall -Wextra)
    add_test(NAME es-app-bezelproject-tests COMMAND es-app-bezelproject-tests)

    add_executable(es-app-system-services-tests
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/system_services_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/system/SystemServices.cpp
        ${CMAKE_SOURCE_DIR}/es-core/src/utils/StringUtil.cpp)
    target_include_directories(es-app-system-services-tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/es-core/src ${RAPIDJSON_INCLUDE_DIRS})
    target_compile_options(es-app-system-services-tests PRIVATE -Wall -Wextra)
    add_test(NAME es-app-system-services-tests COMMAND es-app-system-services-tests)

    add_executable(es-app-http-file-access-tests
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/http_file_access_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/services/HttpFileAccess.cpp
        ${CMAKE_SOURCE_DIR}/es-core/src/utils/StringUtil.cpp)
    target_include_directories(es-app-http-file-access-tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/es-core/src)
    target_compile_options(es-app-http-file-access-tests PRIVATE -Wall -Wextra)
    add_test(NAME es-app-http-file-access-tests COMMAND es-app-http-file-access-tests)
endif()



#-------------------------------------------------------------------------------
# set up CPack install stuff so `make install` does something useful

install(TARGETS emulationstation
    RUNTIME
    DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY ${CMAKE_SOURCE_DIR}/resources
    DESTINATION ${CMAKE_INSTALL_DATADIR}/emulationstation)
# Options of each emulator in the per system and per game settings (features/build_es_features.py).
# A user es_features.cfg in ~/.config/emulationstation replaces it; es_features_*.cfg files add to it.
install(FILES ${CMAKE_SOURCE_DIR}/features/es_features.cfg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/emulationstation)
# Batocera's controller mappings, used for a pad the player has not mapped (InputManager)
install(FILES ${CMAKE_SOURCE_DIR}/controllers/es_input_batocera.cfg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/emulationstation)
# Default systems, used when the user has no es_systems.cfg in ~/.config/emulationstation.
# The user copy and the system-wide ones in $XDG_CONFIG_DIRS take precedence.
install(FILES ${CMAKE_SOURCE_DIR}/es_systems.cfg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/emulationstation)
# Ports is an extra config, so it is merged even when the user has an
# es_systems_custom.cfg base.
install(FILES ${CMAKE_SOURCE_DIR}/es_systems_ports.cfg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/emulationstation)
# Default port scripts, copied to ~/ROMs/ports on first start (OpenRA...).
install(DIRECTORY ${CMAKE_SOURCE_DIR}/ports/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/emulationstation/ports
    FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

# Updates from the menu: es-re-update installs the latest release, pkexec runs it as root
# without a password for the active local session (polkit policy)
set(ES_RELEASES_URL "https://github.com/JyuHo/emulationstation-re/releases")
set(ES_UPDATE_HELPER "${CMAKE_INSTALL_FULL_LIBEXECDIR}/emulationstation-re/es-re-update")
configure_file(${CMAKE_SOURCE_DIR}/packaging/update/es-re-update.in
    ${CMAKE_CURRENT_BINARY_DIR}/es-re-update @ONLY)
configure_file(${CMAKE_SOURCE_DIR}/packaging/update/org.emulationstation-re.update.policy.in
    ${CMAKE_CURRENT_BINARY_DIR}/org.emulationstation-re.update.policy @ONLY)
target_compile_definitions(emulationstation PRIVATE
    ES_RELEASES_URL="${ES_RELEASES_URL}" ES_UPDATE_HELPER="${ES_UPDATE_HELPER}")
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/es-re-update
    DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/emulationstation-re)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.emulationstation-re.update.policy
    DESTINATION ${CMAKE_INSTALL_DATADIR}/polkit-1/actions)

# Debian package: configure with -DCMAKE_INSTALL_PREFIX=/usr, then `cpack` in the build folder,
# as the release workflow does.
# The library dependencies are found by dpkg-shlibdeps; the Arch package is in packaging/arch.
set(CPACK_PACKAGE_NAME "emulationstation-re")
set(CPACK_PACKAGE_VERSION "${ES_VERSION_RESOLVED}")
set(CPACK_PACKAGE_VENDOR "EmulationStation-RE")
set(CPACK_PACKAGE_CONTACT "EmulationStation-RE <https://github.com/JyuHo/emulationstation-re>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Graphical front-end for emulators with controller navigation")
set(CPACK_PACKAGE_DESCRIPTION "EmulationStation-RE is a Linux build of Batocera's EmulationStation, a graphical front-end for emulators designed for controller navigation.")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.md")
set(CPACK_STRIP_FILES ON)

set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION "games")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/JyuHo/emulationstation-re")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS_PRIVATE_DIRS "${CMAKE_BINARY_DIR}/es-launcher") # libes-launcher.so ships in the package
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "fonts-droid-fallback, curl, pkexec") # curl and pkexec: updates from the menu
set(CPACK_DEBIAN_PACKAGE_CONFLICTS "emulationstation")

set(CPACK_GENERATOR "DEB")

include(CPack)
