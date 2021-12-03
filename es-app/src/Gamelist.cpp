#include "Gamelist.h"

#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "FileData.h"
#include "FileFilterIndex.h"
#include "Log.h"
#include "Settings.h"
#include "SystemData.h"
#include <pugixml/src/pugixml.hpp>
#include "Genres.h"

#ifdef WIN32
#include <Windows.h>
#include <direct.h>
#else
#include <unistd.h>
#endif

std::string getGamelistRecoveryPath(SystemData* system)
{
	return Utils::FileSystem::getGenericPath(Utils::FileSystem::getEsConfigPath() + "/recovery/" + system->getName());
}

FileData* findOrCreateFile(SystemData* system, const std::string& path, FileType type, std::unordered_map<std::string, FileData*>& fileMap)
{
	auto pGame = fileMap.find(path);
	if (pGame != fileMap.end())
		return pGame->second;

	// first, verify that path is within the system's root folder
	FolderData* root = system->getRootFolder();
	bool contains = false;
	std::string relative = Utils::FileSystem::removeCommonPath(path, root->getPath(), contains);

	if(!contains)
	{
		LOG(LogWarning) << "File path \"" << path << "\" is outside system path \"" << system->getStartPath() << "\"";
		return NULL;
	}

	auto pathList = Utils::FileSystem::getPathList(relative);
	auto path_it = pathList.begin();
	FolderData* treeNode = root;

	while(path_it != pathList.end())
	{
		std::string key = Utils::FileSystem::combine(treeNode->getPath(), *path_it);
		FileData* item = (fileMap.find(key) != fileMap.end()) ? fileMap[key] : nullptr;
		if (item != nullptr)
		{
			if (item->getType() == FOLDER)
				treeNode = (FolderData*) item;
			else
				return item;
		}
		
		// this is the end
		if(path_it == --pathList.end())
		{
			if(type == FOLDER)
			{
				LOG(LogWarning) << "gameList: folder doesn't already exist, won't create";
				return NULL;
			}

			// Skip if the extension in the gamelist is unknown
			if (!system->getSystemEnvData()->isValidExtension(Utils::String::toLower(Utils::FileSystem::getExtension(path))))
			{
				LOG(LogWarning) << "gameList: file extension is not known by systemlist";
				return NULL;
			}

			// Add final game
			item = new FileData(GAME, path, system);
			if (!item->isArcadeAsset())
			{
				fileMap[key] = item;
				treeNode->addChild(item);
			}

			return item;
		}

		if(item == nullptr)
		{
			// don't create folders unless it's leading up to a game
			// if type is a folder it's gonna be empty, so don't bother
			if(type == FOLDER)
			{
				LOG(LogWarning) << "gameList: folder doesn't already exist, won't create";
				return NULL;
			}

			// create missing folder
			FolderData* folder = new FolderData(treeNode->getPath() + "/" + *path_it, system);
			fileMap[key] = folder;
			treeNode->addChild(folder);
			treeNode = folder;
		}

		path_it++;
	}

	return NULL;
}

std::vector<FileData*> loadGamelistFile(const std::string xmlpath, SystemData* system, std::unordered_map<std::string, FileData*>& fileMap, size_t checkSize, bool fromFile)
{	
	std::vector<FileData*> ret;

	bool trustGamelist = Settings::getInstance()->getBool("ParseGamelistOnly");

	LOG(LogInfo) << "Parsing XML file \"" << xmlpath << "\"...";

	pugi::xml_document doc;
	pugi::xml_parse_result result = fromFile ? doc.load_file(xmlpath.c_str()) : doc.load_string(xmlpath.c_str());

	if (!result)
	{
		LOG(LogError) << "Error parsing XML file \"" << xmlpath << "\"!\n	" << result.description();
		return ret;
	}

	pugi::xml_node root = doc.child("gameList");
	if (!root)
	{
		LOG(LogError) << "Could not find <gameList> node in gamelist \"" << xmlpath << "\"!";
		return ret;
	}

	if (checkSize != SIZE_MAX)
	{
		auto parentSize = root.attribute("parentHash").as_uint();
		if (parentSize != checkSize)
		{
			LOG(LogWarning) << "gamelist size don't match !";
			return ret;
		}
	}

	std::string relativeTo = system->getStartPath();

	for (pugi::xml_node fileNode : root.children())
	{
		FileType type = GAME;

		std::string tag = fileNode.name();

		if (tag == "folder")
			type = FOLDER;
		else if (tag != "game")
			continue;

		const std::string path = Utils::FileSystem::resolveRelativePath(fileNode.child("path").text().get(), relativeTo, false);
				
		if (!trustGamelist && !Utils::FileSystem::exists(path))
		{
			LOG(LogWarning) << "File \"" << path << "\" does not exist! Ignoring.";
			continue;
		}

		FileData* file = findOrCreateFile(system, path, type, fileMap);
		if (!file)
		{
			LOG(LogError) << "Error finding/creating FileData for \"" << path << "\", skipping.";
			continue;
		}
		else if (!file->isArcadeAsset())
		{
			std::string defaultName = file->getMetadata(MetaDataId::Name);
			file->setMetadata(MetaDataList::createFromXML(type == FOLDER ? FOLDER_METADATA : GAME_METADATA, fileNode, system));
			file->getMetadata().migrate(file, fileNode);

			//make sure name gets set if one didn't exist
			if (file->getMetadata(MetaDataId::Name).empty())
				file->setMetadata(MetaDataId::Name, defaultName);

			if (!trustGamelist && !file->getHidden() && Utils::FileSystem::isHidden(path))
				file->getMetadata().set(MetaDataId::Hidden, "true");

			Genres::convertGenreToGenreIds(&file->getMetadata());

			if (checkSize != SIZE_MAX)
				file->getMetadata().setDirty();
			else
				file->getMetadata().resetChangedFlag();

			ret.push_back(file);
		}
	}

	return ret;
}

void clearTemporaryGamelistRecovery(SystemData* system)
{	
	auto path = getGamelistRecoveryPath(system);
	Utils::FileSystem::deleteDirectoryFiles(path, true);
}

void parseGamelist(SystemData* system, std::unordered_map<std::string, FileData*>& fileMap)
{
	std::string xmlpath = system->getGamelistPath(false);

	auto size = Utils::FileSystem::getFileSize(xmlpath);
	if (size != 0)
		loadGamelistFile(xmlpath, system, fileMap, SIZE_MAX, true);

	auto files = Utils::FileSystem::getDirContent(getGamelistRecoveryPath(system), true);
	for (auto file : files)
		loadGamelistFile(file, system, fileMap, size, true);

	if (size != SIZE_MAX)
		system->setGamelistHash(size);	
}

bool addFileDataNode(pugi::xml_node& parent, FileData* file, const char* tag, SystemData* system, bool fullPaths = false)
{
	//create game and add to parent node
	pugi::xml_node newNode = parent.append_child(tag);

	//write metadata
	file->getMetadata().appendToXML(newNode, true, system->getStartPath(), fullPaths);

	if(newNode.children().begin() == newNode.child("name") //first element is name
		&& ++newNode.children().begin() == newNode.children().end() //theres only one element
		&& newNode.child("name").text().get() == file->getDisplayName()) //the name is the default
	{
		//if the only info is the default name, don't bother with this node
		//delete it and ultimately do nothing
		parent.remove_child(newNode);
		return false;
	}

	if (fullPaths)
		newNode.prepend_child("path").text().set(file->getPath().c_str());
	else
	{
		// there's something useful in there so we'll keep the node, add the path
		// try and make the path relative if we can so things still work if we change the rom folder location in the future
		std::string path = Utils::FileSystem::createRelativePath(file->getPath(), system->getStartPath(), false).c_str();
		if (path.empty() && file->getType() == FOLDER)
			path = ".";

		newNode.prepend_child("path").text().set(path.c_str());
	}
	return true;	
}

bool saveToXml(FileData* file, const std::string& fileName, bool fullPaths)
{
	SystemData* system = file->getSourceFileData()->getSystem();
	if (system == nullptr)
		return false;	

	pugi::xml_document doc;
	pugi::xml_node root = doc.append_child("gameList");

	const char* tag = file->getType() == GAME ? "game" : "folder";

	root.append_attribute("parentHash").set_value(system->getGamelistHash());

	if (addFileDataNode(root, file, tag, system, fullPaths))
	{
		std::string folder = Utils::FileSystem::getParent(fileName);
		if (!Utils::FileSystem::exists(folder))
			Utils::FileSystem::createDirectory(folder);

		Utils::FileSystem::removeFile(fileName);
		if (!doc.save_file(fileName.c_str()))
		{
			LOG(LogError) << "Error saving metadata to \"" << fileName << "\" (for system " << system->getName() << ")!";
			return false;
		}

		return true;
	}

	return false;
}

bool saveToGamelistRecovery(FileData* file)
{
	if (!Settings::getInstance()->getBool("SaveGamelistsOnExit"))
		return false;

	SystemData* system = file->getSourceFileData()->getSystem();
	if (!Settings::HiddenSystemsShowGames() && !system->isVisible())
		return false;

	std::string fp = file->getFullPath();
	fp = Utils::FileSystem::createRelativePath(file->getFullPath(), system->getRootFolder()->getFullPath(), true);
	fp = Utils::FileSystem::getParent(fp) + "/" + Utils::FileSystem::getStem(fp) + ".xml";

	std::string path = Utils::FileSystem::getAbsolutePath(fp, getGamelistRecoveryPath(system));
	path = Utils::FileSystem::getCanonicalPath(path);

	return saveToXml(file, path);
}

bool removeFromGamelistRecovery(FileData* file)
{
	SystemData* system = file->getSourceFileData()->getSystem();
	if (system == nullptr)
		return false;

	std::string fp = file->getFullPath();
	fp = Utils::FileSystem::createRelativePath(file->getFullPath(), system->getRootFolder()->getFullPath(), true);
	fp = Utils::FileSystem::getParent(fp) + "/" + Utils::FileSystem::getStem(fp) + ".xml";

	std::string path = Utils::FileSystem::getAbsolutePath(fp, getGamelistRecoveryPath(system));
	path = Utils::FileSystem::getCanonicalPath(path);

	if (Utils::FileSystem::exists(path))
		return Utils::FileSystem::removeFile(path);

	return false;
}

bool hasDirtyFile(SystemData* system)
{
	if (system == nullptr || !system->isGameSystem() || (!Settings::HiddenSystemsShowGames() && !system->isVisible())) // || system->hasPlatformId(PlatformIds::IMAGEVIEWER))
		return false;

	FolderData* rootFolder = system->getRootFolder();
	if (rootFolder == nullptr)
		return false;

	for (auto file : rootFolder->getFilesRecursive(GAME | FOLDER))
		if (file->getMetadata().wasChanged())
			return true;

	return false;
}

void updateGamelist(SystemData* system)
{
	//We do this by reading the XML again, adding changes and then writing it back,
	//because there might be information missing in our systemdata which would then miss in the new XML.
	//We have the complete information for every game though, so we can simply remove a game
	//we already have in the system from the XML, and then add it back from its GameData information...

	if(system == nullptr || Settings::getInstance()->getBool("IgnoreGamelist"))
		return;

	if (!system->isGameSystem() || (!Settings::HiddenSystemsShowGames() && !system->isVisible())) // || system->hasPlatformId(PlatformIds::IMAGEVIEWER))
		return;

	FolderData* rootFolder = system->getRootFolder();
	if (rootFolder == nullptr)
	{
		LOG(LogError) << "Found no root folder for system \"" << system->getName() << "\"!";
		return;
	}

	std::vector<FileData*> dirtyFiles;
	std::vector<FileData*> files = rootFolder->getFilesRecursive(GAME | FOLDER, false, nullptr, false);
	for (auto file : files)
		if (file->getSystem() == system && file->getMetadata().wasChanged())
			dirtyFiles.push_back(file);

	if (dirtyFiles.size() == 0)
	{
		clearTemporaryGamelistRecovery(system);
		return;
	}

	int numUpdated = 0;

	pugi::xml_document doc;
	pugi::xml_node root;
	std::string xmlReadPath = system->getGamelistPath(false);

	if(Utils::FileSystem::exists(xmlReadPath))
	{
		//parse an existing file first
		pugi::xml_parse_result result = doc.load_file(xmlReadPath.c_str());
		if(!result)
			LOG(LogError) << "Error parsing XML file \"" << xmlReadPath << "\"!\n	" << result.description();

		root = doc.child("gameList");
		if(!root)
		{
			LOG(LogError) << "Could not find <gameList> node in gamelist \"" << xmlReadPath << "\"!";
			root = doc.append_child("gameList");
		}
	}
	else //set up an empty gamelist to append to		
		root = doc.append_child("gameList");

	std::map<std::string, pugi::xml_node> xmlMap;

	for (pugi::xml_node fileNode : root.children())
	{
		pugi::xml_node path = fileNode.child("path");
		if (path)
		{
			std::string nodePath = Utils::FileSystem::getCanonicalPath(Utils::FileSystem::resolveRelativePath(path.text().get(), system->getStartPath(), true));
			xmlMap[nodePath] = fileNode;
		}
	}
	
	// iterate through all files, checking if they're already in the XML
	for(auto file : dirtyFiles)
	{
		bool removed = false;

		// check if the file already exists in the XML
		// if it does, remove it before adding
		auto xmf = xmlMap.find(Utils::FileSystem::getCanonicalPath(file->getPath()));
		if (xmf != xmlMap.cend())
		{
			removed = true;
			root.remove_child(xmf->second);
		}
		
		const char* tag = (file->getType() == GAME) ? "game" : "folder";

		// it was either removed or never existed to begin with; either way, we can add it now
		if (addFileDataNode(root, file, tag, system))
			++numUpdated; // Only if really added
		else if (removed)
			++numUpdated; // Only if really removed
	}

	// Now write the file
	if (numUpdated > 0) 
	{
		//make sure the folders leading up to this path exist (or the write will fail)
		std::string xmlWritePath(system->getGamelistPath(true));
		Utils::FileSystem::createDirectory(Utils::FileSystem::getParent(xmlWritePath));

		LOG(LogInfo) << "Added/Updated " << numUpdated << " entities in '" << xmlReadPath << "'";

		if (!doc.save_file(xmlWritePath.c_str()))
			LOG(LogError) << "Error saving gamelist.xml to \"" << xmlWritePath << "\" (for system " << system->getName() << ")!";
		else
			clearTemporaryGamelistRecovery(system);
	}
	else
		clearTemporaryGamelistRecovery(system);
}


void cleanupGamelist(SystemData* system)
{
	if (!system->isGameSystem() || system->isCollection() || (!Settings::HiddenSystemsShowGames() && !system->isVisible())) //  || system->hasPlatformId(PlatformIds::IMAGEVIEWER)
		return;

	FolderData* rootFolder = system->getRootFolder();
	if (rootFolder == nullptr)
	{
		LOG(LogError) << "CleanupGamelist : Found no root folder for system \"" << system->getName() << "\"!";
		return;
	}

	std::string xmlReadPath = system->getGamelistPath(false);
	if (!Utils::FileSystem::exists(xmlReadPath))
		return;

	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(xmlReadPath.c_str());
	if (!result)
	{
		LOG(LogError) << "CleanupGamelist : Error parsing XML file \"" << xmlReadPath << "\"!\n	" << result.description();
		return;
	}

	pugi::xml_node root = doc.child("gameList");
	if (!root)
	{
		LOG(LogError) << "CleanupGamelist : Could not find <gameList> node in gamelist \"" << xmlReadPath << "\"!";
		return;
	}

	std::map<std::string, FileData*> fileMap;
	for (auto file : rootFolder->getFilesRecursive(GAME | FOLDER, false, nullptr, false))
		fileMap[Utils::FileSystem::getCanonicalPath(file->getPath())] = file;

	bool dirty = false;

	std::set<std::string> knownXmlPaths;
	std::set<std::string> knownMedias;

	for (pugi::xml_node fileNode = root.first_child(); fileNode; )
	{
		pugi::xml_node next = fileNode.next_sibling();

		pugi::xml_node path = fileNode.child("path");
		if (!path)
		{
			dirty = true;
			root.remove_child(fileNode);
			fileNode = next;
			continue;
		}
		
		std::string gamePath = Utils::FileSystem::getCanonicalPath(Utils::FileSystem::resolveRelativePath(path.text().get(), system->getStartPath(), true));

		auto file = fileMap.find(gamePath);
		if (file == fileMap.cend())
		{
			dirty = true;
			root.remove_child(fileNode);
			fileNode = next;
			continue;
		}

		knownXmlPaths.insert(gamePath);

		for (auto mdd : MetaDataList::getMDD())
		{
			if (mdd.type != MetaDataType::MD_PATH)
				continue;

			pugi::xml_node mddPath = fileNode.child(mdd.key.c_str());

			std::string mddFullPath = (mddPath ? Utils::FileSystem::getCanonicalPath(Utils::FileSystem::resolveRelativePath(mddPath.text().get(), system->getStartPath(), true)) : "");
			if (!Utils::FileSystem::exists(mddFullPath))
			{
				std::string ext = ".jpg";
				std::string folder = "/images/";
				std::string suffix;

				switch (mdd.id)
				{
				case MetaDataId::Image: suffix = "image"; break;
				case MetaDataId::Thumbnail: suffix = "thumb"; break;
				case MetaDataId::Marquee: suffix = "marquee"; break;
				case MetaDataId::Video: suffix = "video"; folder = "/videos/"; ext = ".mp4"; break;
				case MetaDataId::FanArt: suffix = "fanart"; break;
				case MetaDataId::BoxBack: suffix = "boxback"; break;
				case MetaDataId::BoxArt: suffix = "box"; break;
				case MetaDataId::Wheel: suffix = "wheel"; break;
				case MetaDataId::TitleShot: suffix = "titleshot"; break;
				case MetaDataId::Manual: suffix = "manual"; folder = "/manuals/"; ext = ".pdf"; break;
				case MetaDataId::Magazine: suffix = "magazine"; folder = "/magazines/"; ext = ".pdf"; break;
				case MetaDataId::Map: suffix = "map"; break;
				case MetaDataId::Cartridge: suffix = "cartridge"; break;
				}

				if (!suffix.empty())
				{					
					std::string mediaPath = system->getStartPath() + folder + file->second->getDisplayName() + "-"+ suffix + ext;

					if (ext == ".pdf" && !Utils::FileSystem::exists(mediaPath))
					{
						mediaPath = system->getStartPath() + folder + file->second->getDisplayName() + ".pdf";
						if (!Utils::FileSystem::exists(mediaPath))
							mediaPath = system->getStartPath() + folder + file->second->getDisplayName() + ".cbz";
					}
					else if (ext != ".jpg" && !Utils::FileSystem::exists(mediaPath))
						mediaPath = system->getStartPath() + folder + file->second->getDisplayName() + ext;
					else if (ext == ".jpg" && !Utils::FileSystem::exists(mediaPath))
						mediaPath = system->getStartPath() + folder + file->second->getDisplayName() + "-" + suffix + ".png";

					if (mdd.id == MetaDataId::Image && !Utils::FileSystem::exists(mediaPath))
						mediaPath = system->getStartPath() + folder + file->second->getDisplayName() + ".jpg";
					if (mdd.id == MetaDataId::Image && !Utils::FileSystem::exists(mediaPath))
						mediaPath = system->getStartPath() + folder + file->second->getDisplayName() + ".png";

					if (Utils::FileSystem::exists(mediaPath))
					{
						auto relativePath = Utils::FileSystem::createRelativePath(mediaPath, system->getStartPath(), true);

						fileNode.append_child(mdd.key.c_str()).text().set(relativePath.c_str());

						LOG(LogInfo) << "CleanupGamelist : Add resolved " << mdd.key << " path " << mediaPath << " to game " << gamePath << " in system " << system->getName();
						dirty = true;

						knownMedias.insert(mediaPath);
						continue;
					}
				}

				if (mddPath)
				{
					LOG(LogInfo) << "CleanupGamelist : Remove " << mdd.key << " path to game " << gamePath << " in system " << system->getName();

					dirty = true;
					fileNode.remove_child(mdd.key.c_str());
				}

				continue;
			}

			knownMedias.insert(mddFullPath);
		}

		fileNode = next;
	}
	
	// iterate through all files, checking if they're already in the XML
	for (auto file : fileMap)
	{
		auto fileName = file.first;
		auto fileData = file.second;

		if (knownXmlPaths.find(fileName) != knownXmlPaths.cend())
			continue;

		const char* tag = (fileData->getType() == GAME) ? "game" : "folder";

		if (addFileDataNode(root, fileData, tag, system))
		{
			LOG(LogInfo) << "CleanupGamelist : Add " << fileName << " to system " << system->getName();
			dirty = true;
		}
	}

	// Cleanup unknown files in system rom path
	auto allFiles = Utils::FileSystem::getDirContent(system->getStartPath(), true);
	for (auto dirFile : allFiles)
	{
		if (knownXmlPaths.find(dirFile) != knownXmlPaths.cend())
			continue;

		if (knownMedias.find(dirFile) != knownMedias.cend())
			continue;

		if (dirFile.empty())
			continue;

		if (Utils::FileSystem::isDirectory(dirFile))
			continue;

		std::string parent = Utils::String::toLower(Utils::FileSystem::getFileName(Utils::FileSystem::getParent(dirFile)));
		if (parent != "images" && parent != "videos" && parent != "manuals" && parent != "downloaded_images" && parent != "downloaded_videos")
			continue;

		if (Utils::FileSystem::getParent(Utils::FileSystem::getParent(dirFile)) != system->getStartPath())
			if (Utils::FileSystem::getFileName(Utils::FileSystem::getParent(Utils::FileSystem::getParent(dirFile))) != "media")
				continue;

		std::string ext = Utils::String::toLower(Utils::FileSystem::getExtension(dirFile));
		if (ext == ".txt" || ext == ".xml" || ext == ".old")
			continue;
		
		LOG(LogInfo) << "CleanupGamelist : Remove unknown file " << dirFile << " to system " << system->getName();

		Utils::FileSystem::removeFile(dirFile);
	}

	// Now write the file
	if (dirty)
	{
		// Make sure the folders leading up to this path exist (or the write will fail)
		std::string xmlWritePath(system->getGamelistPath(true));
		Utils::FileSystem::createDirectory(Utils::FileSystem::getParent(xmlWritePath));		

		std::string oldXml = xmlWritePath + ".old";
		Utils::FileSystem::removeFile(oldXml);
		Utils::FileSystem::copyFile(xmlWritePath, oldXml);

		if (!doc.save_file(xmlWritePath.c_str()))
			LOG(LogError) << "Error saving gamelist.xml to \"" << xmlWritePath << "\" (for system " << system->getName() << ")!";
		else
			clearTemporaryGamelistRecovery(system);
	}
	else
		clearTemporaryGamelistRecovery(system);
}
