#include "CustomFeatures.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include "Log.h"
#include "Paths.h"

bool CustomFeatures::FeaturesLoaded = false;

CustomFeatures CustomFeatures::GlobalFeatures;
CustomFeatures CustomFeatures::SharedFeatures;
std::map<std::string, EmulatorData> CustomFeatures::EmulatorFeatures;

EmulatorFeatures::Features operator|(EmulatorFeatures::Features a, EmulatorFeatures::Features b)
{
	return static_cast<EmulatorFeatures::Features>(static_cast<int>(a) | static_cast<int>(b));
}

EmulatorFeatures::Features operator&(EmulatorFeatures::Features a, EmulatorFeatures::Features b)
{
	return static_cast<EmulatorFeatures::Features>(static_cast<int>(a) & static_cast<int>(b));
}

EmulatorFeatures::Features EmulatorFeatures::parseFeatures(const std::string features)
{
	EmulatorFeatures::Features ret = EmulatorFeatures::Features::none;

	for (auto name : Utils::String::split(features, ','))
	{
		std::string trim = Utils::String::trim(name);

		if (trim == "autosave") ret = ret | EmulatorFeatures::Features::autosave;
		if (trim == "netplay") ret = ret | EmulatorFeatures::Features::netplay;
		if (trim == "cheevos") ret = ret | EmulatorFeatures::Features::cheevos;
		if (trim == "padtokeyboard" || trim == "joystick2pad") ret = ret | EmulatorFeatures::Features::padTokeyboard;

		// The next features can be overriden with sharedFeatures
		if (CustomFeatures::SharedFeatures.any([trim](auto x) { return x.value == trim; }))
			continue;

		if (trim == "ratio") ret = ret | EmulatorFeatures::Features::ratio;
		if (trim == "rewind") ret = ret | EmulatorFeatures::Features::rewind;
		if (trim == "smooth") ret = ret | EmulatorFeatures::Features::smooth;
		if (trim == "shaders") ret = ret | EmulatorFeatures::Features::shaders;
		if (trim == "videofilters") ret = ret | EmulatorFeatures::Features::videofilters;
		if (trim == "pixel_perfect") ret = ret | EmulatorFeatures::Features::pixel_perfect;
		if (trim == "decoration") ret = ret | EmulatorFeatures::Features::decoration;
		if (trim == "latency_reduction") ret = ret | EmulatorFeatures::Features::latency_reduction;
		if (trim == "game_translation") ret = ret | EmulatorFeatures::Features::game_translation;
		if (trim == "fullboot") ret = ret | EmulatorFeatures::Features::fullboot;
		if (trim == "emulated_wiimotes") ret = ret | EmulatorFeatures::Features::emulated_wiimotes;
		if (trim == "screen_layout") ret = ret | EmulatorFeatures::Features::screen_layout;
		if (trim == "internal_resolution") ret = ret | EmulatorFeatures::Features::internal_resolution;
		if (trim == "videomode") ret = ret | EmulatorFeatures::Features::videomode;
		if (trim == "colorization") ret = ret | EmulatorFeatures::Features::colorization;
		if (trim == "autocontrollers") ret = ret | EmulatorFeatures::Features::autocontrollers;
	}

	return ret;
}

void CustomFeatures::importXmlElements(pugi::xml_node& from, const std::string& elementName, pugi::xml_node& to, const std::string& remplacementKey)
{
	for (pugi::xml_node system = from.child(elementName.c_str()); system; system = system.next_sibling(elementName.c_str()))
	{
		if (!remplacementKey.empty())
		{
			// Remove existing one ?
			if (!system.attribute(remplacementKey.c_str()))
				continue;

			std::string name = system.attribute(remplacementKey.c_str()).as_string();
			if (name.empty())
				continue;

			for (pugi::xml_node& srcSystem : to.children())
			{
				if (std::string(srcSystem.name()) != elementName)
					continue;

				std::string srcName = srcSystem.attribute(remplacementKey.c_str()).as_string();
				if (srcName != name)
					continue;

				srcSystem.remove_child(srcSystem);
				break;
			}
		}
			
		to.append_copy(system);
	}
}

void CustomFeatures::loadAdditionnalFeatures(pugi::xml_node& srcSystems)
{
	std::vector<std::string> rootPaths = { Paths::getUserEmulationStationPath(), Paths::getEmulationStationPath() };
	for (auto rootPath : VectorHelper::distinct(rootPaths, [](auto x) { return x; }))
	{
		for (auto customPath : Utils::FileSystem::getDirContent(rootPath, false, false))
		{
			if (Utils::FileSystem::getExtension(customPath) != ".cfg" || !Utils::String::startsWith(Utils::FileSystem::getFileName(customPath), "es_features_"))
				continue;

			pugi::xml_document doc;
			pugi::xml_parse_result res = doc.load_file(WINSTRINGW(customPath).c_str());
			if (!res)
			{
				LOG(LogError) << "Could not parse " << Utils::FileSystem::getFileName(customPath) << " file!";
				continue;
			}

			pugi::xml_node systemList = doc.child("features");
			if (!systemList)
			{
				LOG(LogError) << Utils::FileSystem::getFileName(customPath) << " is missing the <features> tag !";
				continue;
			}

			// import/replace shared features
			pugi::xml_node sharedFeaturesToAdd = systemList.child("sharedFeatures");
			if (sharedFeaturesToAdd)
			{
				pugi::xml_node sharedFeatures = srcSystems.child("sharedFeatures");
				if (sharedFeatures)
					importXmlElements(sharedFeaturesToAdd, "feature", sharedFeatures, "value");
				else
					srcSystems.append_copy(sharedFeaturesToAdd);
			}

			// import/replace global features
			pugi::xml_node globalFeaturesToAdd = systemList.child("globalFeatures");
			if (globalFeaturesToAdd)
			{
				pugi::xml_node globalFeatures = srcSystems.child("globalFeatures");
				if (globalFeatures)
				{
					importXmlElements(globalFeaturesToAdd, "sharedFeature", globalFeatures, "value");
					importXmlElements(globalFeaturesToAdd, "feature", globalFeatures, "value");
				}
				else
					srcSystems.append_copy(globalFeaturesToAdd);
			}

			// import emulators
			importXmlElements(systemList, "emulator", srcSystems);
		}
	}

	/* Uncomment to see final XML result
	std::string fileName = "c:\\temp\\test.xml";
	Utils::FileSystem::removeFile(fileName);
	doc.save_file(WINSTRINGW(fileName).c_str());
	*/
}

bool CustomFeatures::loadEsFeaturesFile()
{
	EmulatorFeatures.clear();
	FeaturesLoaded = false;

	GlobalFeatures.clear();
	SharedFeatures.clear();

	std::string path = Paths::getUserEmulationStationPath() + "/es_features.cfg";
	if (!Utils::FileSystem::exists(path))
		path = Paths::getEmulationStationPath() + "/es_features.cfg";

	if (!Utils::FileSystem::exists(path))
		return false;

	pugi::xml_document doc;
	pugi::xml_parse_result res = doc.load_file(WINSTRINGW(path).c_str());

	if (!res)
	{
		LOG(LogError) << "Could not parse es_features.cfg file!";
		LOG(LogError) << res.description();
		return false;
	}

	pugi::xml_node systemList = doc.child("features");
	if (!systemList)
	{
		LOG(LogError) << "es_features.cfg is missing the <features> tag!";
		return false;
	}

	loadAdditionnalFeatures(systemList);

	pugi::xml_node sharedFeatures = systemList.child("sharedFeatures");
	if (sharedFeatures)
		SharedFeatures = loadCustomFeatures(sharedFeatures);

	pugi::xml_node globalFeatures = systemList.child("globalFeatures");
	if (globalFeatures)
	{
		GlobalFeatures = loadCustomFeatures(globalFeatures);
		GlobalFeatures.sort();
	}

	FeaturesLoaded = true;

	for (pugi::xml_node emulator = systemList.child("emulator"); emulator; emulator = emulator.next_sibling("emulator"))
	{
		if (!emulator.attribute("name"))
			continue;

		std::string emulatorNames = emulator.attribute("name").value();
		for (auto tmpName : Utils::String::split(emulatorNames, ','))
		{
			std::string emulatorName = Utils::String::trim(tmpName);
			if (EmulatorFeatures.find(emulatorName) == EmulatorFeatures.cend())
			{
				EmulatorData emul;
				emul.name = emulatorName;
				EmulatorFeatures[emulatorName] = emul;
			}

			EmulatorFeatures::Features emulatorFeatures = EmulatorFeatures::Features::none;
			auto customEmulatorFeatures = loadCustomFeatures(emulator);

			if (customEmulatorFeatures.any([](auto x) { return x.value == "autosave"; })) // Watch if autosave is provided as shared
				emulatorFeatures = emulatorFeatures | EmulatorFeatures::Features::autosave;

			if (emulator.attribute("features") || customEmulatorFeatures.size() > 0)
			{
				if (emulator.attribute("features"))
					emulatorFeatures = EmulatorFeatures::parseFeatures(emulator.attribute("features").value());

				auto it = EmulatorFeatures.find(emulatorName);
				if (it != EmulatorFeatures.cend())
				{
					auto& emul = it->second;
					if (emul.name != emulatorName)
						continue;

					emul.features = emul.features | emulatorFeatures;
					for (auto feat : customEmulatorFeatures)
						emul.customFeatures.push_back(feat);
				}
			}

			pugi::xml_node coresNode = emulator.child("cores");
			if (coresNode == nullptr)
				coresNode = emulator;

			for (pugi::xml_node coreNode = coresNode.child("core"); coreNode; coreNode = coreNode.next_sibling("core"))
			{
				if (!coreNode.attribute("name"))
					continue;

				std::string coreNames = coreNode.attribute("name").value();
				for (auto tmpCoreName : Utils::String::split(coreNames, ','))
				{
					std::string coreName = Utils::String::trim(tmpCoreName);

					EmulatorFeatures::Features coreFeatures = coreNode.attribute("features") ? EmulatorFeatures::parseFeatures(coreNode.attribute("features").value()) : EmulatorFeatures::Features::none;					
					auto customCoreFeatures = loadCustomFeatures(coreNode);

					if (customCoreFeatures.any([](auto x) { return x.value == "autosave"; })) // Watch if autosave is provided as shared
						coreFeatures = coreFeatures | EmulatorFeatures::Features::autosave;
						
					bool coreFound = false;

					for (auto it = EmulatorFeatures.begin(); it != EmulatorFeatures.end(); it++)
					{
						auto& emul = it->second;
						if (emul.name != emulatorName)
							continue;

						for (auto& core : emul.cores)
						{
							if (core.name == coreName)
							{
								coreFound = true;
								core.features = core.features | coreFeatures;

								for (auto feat : customCoreFeatures)
									core.customFeatures.push_back(feat);
							}
						}

						if (!coreFound)
						{
							CoreData core;
							core.name = coreName;
							core.features = coreFeatures;
							core.customFeatures = customCoreFeatures;
							emul.cores.push_back(core);
						}
					}

					pugi::xml_node systemsCoresNode = coreNode.child("systems");
					if (systemsCoresNode == nullptr)
						systemsCoresNode = coreNode;

					for (pugi::xml_node systemNode = systemsCoresNode.child("system"); systemNode; systemNode = systemNode.next_sibling("system"))
					{
						if (!systemNode.attribute("name"))
							continue;

						std::string systemName = systemNode.attribute("name").value();

						EmulatorFeatures::Features systemFeatures = systemNode.attribute("features") ? EmulatorFeatures::parseFeatures(systemNode.attribute("features").value()) : EmulatorFeatures::Features::none;
						auto customSystemFeatures = loadCustomFeatures(systemNode);

						if (customSystemFeatures.any([](auto x) { return x.value == "autosave"; })) // Watch if autosave is provided as shared
							systemFeatures = systemFeatures | EmulatorFeatures::Features::autosave;

						if (systemFeatures == EmulatorFeatures::Features::none && customSystemFeatures.size() == 0)
							continue;

						for (auto it = EmulatorFeatures.begin(); it != EmulatorFeatures.end(); it++)
						{
							auto& emul = it->second;
							if (emul.name != emulatorName)
								continue;

							for (auto& core : emul.cores)
							{
								if (core.name == coreName)
								{
									bool systemFound = false;

									for (auto& systemFeature : core.systemFeatures)
									{
										if (systemFeature.name == systemName)
										{
											systemFound = true;
											systemFeature.features = systemFeature.features | systemFeatures;

											for (auto feat : customSystemFeatures)
												systemFeature.customFeatures.push_back(feat);
										}
									}

									if (!systemFound)
									{
										SystemFeature sysF;
										sysF.name = systemName;
										sysF.features = systemFeatures;
										sysF.customFeatures = customSystemFeatures;
										core.systemFeatures.push_back(sysF);
									}
								}
							}
						}
					}
				}
			}

			pugi::xml_node systemsNode = emulator.child("systems");
			if (systemsNode == nullptr)
				systemsNode = emulator;

			for (pugi::xml_node systemNode = systemsNode.child("system"); systemNode; systemNode = systemNode.next_sibling("system"))
			{
				if (!systemNode.attribute("name"))
					continue;

				std::string systemName = systemNode.attribute("name").value();

				EmulatorFeatures::Features systemFeatures = systemNode.attribute("features") ? EmulatorFeatures::parseFeatures(systemNode.attribute("features").value()) : EmulatorFeatures::Features::none;
				auto customSystemFeatures = loadCustomFeatures(systemNode);

				auto it = EmulatorFeatures.find(emulatorName);
				if (it != EmulatorFeatures.cend())
				{
					auto& emul = it->second;
					bool systemFound = false;
					for (auto& systemFeature : emul.systemFeatures)
					{
						if (systemFeature.name == systemName)
						{
							systemFound = true;
							systemFeature.features = systemFeature.features | systemFeatures;

							for (auto feat : customSystemFeatures)
								systemFeature.customFeatures.push_back(feat);
						}
					}

					if (!systemFound)
					{
						SystemFeature sysF;
						sysF.name = systemName;
						sysF.features = systemFeatures;
						sysF.customFeatures = customSystemFeatures;
						emul.systemFeatures.push_back(sysF);
					}
				}
			}
		}
	}
	
	return true;
}

CustomFeatures CustomFeatures::loadCustomFeatures(pugi::xml_node node)
{
	CustomFeatures ret;

	if (node.attribute("features"))
	{
		auto features = node.attribute("features").value();
		for (auto name : Utils::String::split(features, ','))
		{
			std::string featureValue = Utils::String::trim(name);

			auto it = std::find_if(SharedFeatures.cbegin(), SharedFeatures.cend(), [featureValue](const CustomFeature& x) { return x.value == featureValue; });
			if (it != SharedFeatures.cend())
				ret.push_back(*it);
		}
	}

	pugi::xml_node customFeatures = node.child("features");
	if (customFeatures == nullptr)
		customFeatures = node;

	for (pugi::xml_node featureNode = customFeatures.first_child(); featureNode; featureNode = featureNode.next_sibling())
	{
		std::string name = featureNode.name();
		if (name == "sharedFeature")
		{
			auto it = SharedFeatures.cend();

			if (featureNode.attribute("name"))
			{
				std::string featureName = featureNode.attribute("name").value();
				it = std::find_if(SharedFeatures.cbegin(), SharedFeatures.cend(), [featureName](const CustomFeature& x) { return x.name == featureName; });
			}

			if (it == SharedFeatures.cend() && featureNode.attribute("value"))
			{
				std::string featureValue = featureNode.attribute("value").value();
				it = std::find_if(SharedFeatures.cbegin(), SharedFeatures.cend(), [featureValue](const CustomFeature& x) { return x.value == featureValue; });
			}

			if (it != SharedFeatures.cend())
			{
				CustomFeature cs = *it;

				if (featureNode.attribute("submenu"))
					cs.submenu = featureNode.attribute("submenu").value();

				if (featureNode.attribute("preset"))
					cs.preset = featureNode.attribute("preset").value();

				if (featureNode.attribute("preset-parameters"))
					cs.preset_parameters = featureNode.attribute("preset-parameters").value();

				if (featureNode.attribute("group"))
					cs.group = featureNode.attribute("group").value();

				if (featureNode.attribute("order"))
					cs.order = Utils::String::toInteger(featureNode.attribute("order").value());

				ret.push_back(cs);
			}

			continue;
		}
		
		if (name != "feature" || !featureNode.attribute("name"))
			continue;

		CustomFeature feat;
		feat.name = featureNode.attribute("name").value();

		if (featureNode.attribute("description"))
			feat.description = featureNode.attribute("description").value();

		if (featureNode.attribute("submenu"))
			feat.submenu = featureNode.attribute("submenu").value();

		if (featureNode.attribute("preset"))
			feat.preset = featureNode.attribute("preset").value();

		if (featureNode.attribute("preset-parameters"))
			feat.preset_parameters = featureNode.attribute("preset-parameters").value();

		if (featureNode.attribute("group"))
			feat.group = featureNode.attribute("group").value();

		if (featureNode.attribute("order"))
			feat.order = Utils::String::toInteger(featureNode.attribute("order").value());

		if (featureNode.attribute("value"))
			feat.value = featureNode.attribute("value").value();
		else
			feat.value = Utils::String::replace(feat.name, " ", "_");

		for (pugi::xml_node value = featureNode.child("choice"); value; value = value.next_sibling("choice"))
		{
			if (!value.attribute("name"))
				continue;

			CustomFeatureChoice cv;
			cv.name = value.attribute("name").value();

			if (value.attribute("value"))
				cv.value = value.attribute("value").value();
			else
				cv.value = Utils::String::replace(cv.name, " ", "_");

			feat.choices.push_back(cv);
		}

		if (feat.choices.size() > 0 || !feat.preset.empty())
			ret.push_back(feat);
	}

	return ret;
}

void CustomFeatures::sort()
{
	// std::sort is not always keeping the original sequence, when elements equals. Use stable_sort
	std::stable_sort(begin(), end(), [](auto feat1, auto feat2) { return feat1.order < feat2.order; });
}

bool CustomFeatures::hasFeature(const std::string& name) const
{
	return any([name](auto x) { return x.value == name; });
}

bool CustomFeatures::hasGlobalFeature(const std::string& name) const
{
	return any([name](auto x) { return x.value == name || x.value == "global." + name; });
}
