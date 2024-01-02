#pragma once
#ifndef ES_CORE_UTILS_URI_H
#define ES_CORE_UTILS_URI_H

#include <string>
#include <cstring>
#include <vector>
#include <iostream>
#include <utility>
#include <iostream>
#include <sstream>
#include <algorithm>

namespace Utils
{
	class UriParameters
	{
		friend class Uri;

	public:
		std::string operator[](const std::string& key) const
		{
			return get(key);
		}

		std::string get(const std::string& key) const
		{
			for (const auto& argument : arguments)
				if (argument.first == key)
					return argument.second;

			return "";
		}

		void set(const std::string& key, const std::string& value)
		{
			remove(key);
			arguments.push_back(std::pair<std::string, std::string>(key, value));
		}

		void remove(const std::string& key)
		{
			arguments.erase(std::remove_if(arguments.begin(), arguments.end(), [key](const std::pair<std::string, std::string>& p) { return p.first == key; }), arguments.end());
		}

		const std::string toString() const
		{
			std::string parameters;

			for (auto arg : arguments)
			{
				if (parameters.empty())
					parameters = "?";
				else
					parameters += "&";

				parameters += arg.first + "=" + arg.second;
			}

			return parameters;
		}

	protected:
		void parse(const std::string& url)
		{
			arguments.clear();

			// Find the position of '?' in the URL
			size_t pos = url.find('?');

			// If '?' is found, extract parameters
			if (pos != std::string::npos) {
				// Get the substring after '?' (parameters part)
				std::string paramsString = url.substr(pos + 1);

				// Create a string stream to parse parameters
				std::istringstream iss(paramsString);

				// Parse parameters using '&' as a delimiter
				while (std::getline(iss, paramsString, '&')) 
				{
					// Split each parameter into key and value using '=' as a delimiter
					size_t equalPos = paramsString.find('=');
					if (equalPos != std::string::npos)
					{
						std::string key = paramsString.substr(0, equalPos);
						std::string value = paramsString.substr(equalPos + 1);
						arguments.push_back(std::pair<std::string, std::string>(key, value));
					}
				}
			}
		}

		std::vector<std::pair<std::string, std::string>> arguments;
	};

	class Uri
	{
	public:
		Uri(const std::string& url)
		{
			parseUrl(url);
		}

		std::string protocol;
		std::string domain;
		std::string relativePath;
		UriParameters arguments;

		const std::string toString() const
		{
			return protocol + "://" + domain + "/" + relativePath + arguments.toString();
		}

	private:
		void parseUrl(const std::string& url)
		{
			// Parsing arguments
			arguments.parse(url);

			std::istringstream iss(url);
			std::string temp;

			// Parsing protocol
			if (!std::getline(iss, protocol, ':'))
				return;

			// Skipping "//"
			if (iss.get() != '/' || iss.get() != '/')
				return;

			// Parsing domain
			if (!std::getline(iss, domain, '/'))
				return;

			// Parsing relative path
			if (!std::getline(iss, relativePath, '?'))
				return;

		}
	};
}

#endif // ES_CORE_UTILS_URI_H
