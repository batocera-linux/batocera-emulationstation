#pragma once
#ifndef ES_CORE_VECTORHELPER_H
#define ES_CORE_VECTORHELPER_H

#include <vector>
#include <functional>

namespace VectorHelper
{
	template <typename StructType, typename FieldSelectorUnaryFn>
	static auto groupBy(const std::vector<StructType>& instances, const FieldSelectorUnaryFn& fieldChooser)
		-> std::vector<std::pair<decltype(fieldChooser(StructType())), std::vector<StructType>>> // For C++ 11
	{
		StructType _;
		using FieldType = decltype(fieldChooser(_));
		std::vector<std::pair<FieldType, std::vector<StructType>>> instancesByField;
		for (auto& instance : instances)
		{
			FieldType val = fieldChooser(instance);

			auto it = std::find_if(instancesByField.begin(), instancesByField.end(), [val](std::pair<FieldType, std::vector<StructType>>& x) { return x.first == val; });
			if (it == instancesByField.end())
			{
				std::vector<StructType> vec;
				vec.push_back(instance);
				instancesByField.push_back(std::pair<FieldType, std::vector<StructType>>(val, vec));
			}
			else
				it->second.push_back(instance);
		}

		return instancesByField;
	}

	template <typename StructType, typename FieldSelectorUnaryFn>
	static auto select(const std::vector<StructType>& instances, const FieldSelectorUnaryFn& fieldChooser)
		-> std::vector<decltype(fieldChooser(StructType()))> // For C++ 11
	{
		StructType _;
		using FieldType = decltype(fieldChooser(_));

		std::vector<FieldType> result;
		for (auto& instance : instances)
			result.push_back(fieldChooser(instance));

		return result;
	}

	template <typename StructType, typename FieldSelectorUnaryFn>
	static auto distinct(const std::vector<StructType>& instances, const FieldSelectorUnaryFn& fieldChooser)
		-> std::vector<decltype(fieldChooser(StructType()))> // For C++ 11
	{
		StructType _;
		using FieldType = decltype(fieldChooser(_));

		std::vector<FieldType> result;
		for (auto& instance : instances)
		{
			FieldType val = fieldChooser(instance);

			auto it = std::find_if(result.begin(), result.end(), [val](FieldType& x) { return x == val; });
			if (it == result.end())
				result.push_back(val);
		}

		return result;
	}


	template <typename StructType>
	static std::vector<StructType> where(const std::vector<StructType>& instances, const std::function<bool(StructType)>& fieldChooser)
	{
		std::vector<StructType> result;

		for (auto& instance : instances)
			if (fieldChooser(instance))
				result.push_back(instance);

		return result;
	}

	template <typename StructType>
	static bool any(const std::vector<StructType>& instances, const std::function<bool(StructType)>& fieldChooser)
	{
		for (auto& instance : instances)
			if (fieldChooser(instance))
				return true;

		return false;
	}

	template <typename StructType>
	static int count(const std::vector<StructType>& instances, const std::function<bool(StructType)>& fieldChooser)
	{
		int ret = 0;

		for (auto& instance : instances)
			if (fieldChooser(instance))
				ret++;

		return ret;
	}

	template <typename StructType>
	static StructType* firstOrDefault(const std::vector<StructType>& instances, const std::function<bool(StructType)>& fieldChooser)
	{
		for (auto& instance : instances)
			if (fieldChooser(instance))
				return (StructType*) &instance;

		return nullptr;
	}

}

template<typename T>
class VectorEx : public std::vector<T>
{
public:
	VectorEx() {}
	VectorEx(std::initializer_list<T> list) : std::vector<T>(list) { }
	VectorEx(const std::vector<T>& values) : std::vector<T>(values) { }

	int count(const std::function<bool(T)>& fieldChooser) const { return VectorHelper::count(*this, fieldChooser); }
	bool any(const std::function<bool(T)>& fieldChooser) const { return VectorHelper::any(*this, fieldChooser); }
	VectorEx<T> where(const std::function<bool(T)>& fieldChooser) const { return VectorHelper::where(*this, fieldChooser); }
	T* firstOrDefault(const std::function<bool(T)>& fieldChooser) const { return VectorHelper::firstOrDefault(*this, fieldChooser); }

	template <typename FieldSelectorUnaryFn>
	VectorEx<T> orderBy(const FieldSelectorUnaryFn& fieldChooser, bool ascending = true) const
	{
		VectorEx<T> ret(*this);

		if (ascending)
			std::sort(ret.begin(), ret.end(), [fieldChooser](T a, T b) { return fieldChooser(a) < fieldChooser(b); });
		else
			std::sort(ret.begin(), ret.end(), [fieldChooser](T a, T b) { return fieldChooser(b) < fieldChooser(a); });

		return ret;
	}
	
	template <typename FieldSelectorUnaryFn>
	auto groupBy(const FieldSelectorUnaryFn& fieldChooser) const
		->VectorEx<std::pair<decltype(fieldChooser(T())), VectorEx<T>>> // For C++ 11
	{
		T _;
		using FieldType = decltype(fieldChooser(_));

		VectorEx<std::pair<FieldType, VectorEx<T>>> instancesByField;
		for (auto& instance : *this)
		{
			FieldType val = fieldChooser(instance);

			auto it = std::find_if(instancesByField.begin(), instancesByField.end(), [val](std::pair<FieldType, VectorEx<T>>& x) { return x.first == val; });
			if (it == instancesByField.end())
			{
				VectorEx<T> vec;
				vec.push_back(instance);
				instancesByField.push_back(std::pair<FieldType, VectorEx<T>>(val, vec));
			}
			else
				it->second.push_back(instance);
		}

		return instancesByField;
	}
};

#endif