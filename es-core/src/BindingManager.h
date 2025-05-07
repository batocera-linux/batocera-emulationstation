#pragma once
#ifndef ES_CORE_BINDINGMANAGER_H
#define ES_CORE_BINDINGMANAGER_H

#include <string>
#include <vector>

class GuiComponent;
class IBindable;

enum class BindablePropertyType
{
	String,
	Path,
	Int,
	Float,
	Bool,
	Bindable,
	Null
};

struct BindableProperty
{
public:
	static BindableProperty Null;
	static BindableProperty EmptyString;

	BindableProperty() { i = 0; type = BindablePropertyType::Null; };

	BindableProperty(const std::string& value, const BindablePropertyType valueType = BindablePropertyType::String) { s = value; type = valueType; };
	BindableProperty(const int& value) { i = value; type = BindablePropertyType::Int; };
	BindableProperty(const float& value) { f = value; type = BindablePropertyType::Float; };
	BindableProperty(const bool& value) { b = value; type = BindablePropertyType::Bool; };
	BindableProperty(const char* value) { if (value == nullptr) type = BindablePropertyType::Null; else { s = value; type = BindablePropertyType::String; } };

	BindableProperty(IBindable* value) { bindable = value; type = BindablePropertyType::Bindable; };

	void operator= (const std::string& value) { s = value; type = BindablePropertyType::String; }
	void operator= (const int& value) { i = value; type = BindablePropertyType::Int; }
	void operator= (const float& value) { f = value; type = BindablePropertyType::Float; }
	void operator= (const bool& value) { b = value; type = BindablePropertyType::Bool; }

	void operator= (IBindable* value) { bindable = value; type = BindablePropertyType::Bindable; }

	bool isString() { return type == BindablePropertyType::String || type == BindablePropertyType::Path; }

	std::string toString();
	bool toBoolean();
	int toInteger();
	int toFloat();
	
	union
	{
		int			 i;
		float        f;
		bool         b;
	};

	std::string  s;
	IBindable* bindable;
	BindablePropertyType type;
};

class IBindable
{
public:
	virtual BindableProperty getProperty(const std::string& name) = 0;
	virtual std::string getBindableTypeName() = 0;
	virtual IBindable* getBindableParent() { return nullptr; };
};

/// <summary>
/// Class for Child Components, that expose {name:XXX} properties to binding where name is the theme name attribute ( image name="toto" -> {toto.visible} )
/// </summary>
class ComponentBinding : public IBindable
{
public:
	ComponentBinding(GuiComponent* comp, IBindable* parent);

	BindableProperty getProperty(const std::string& name) override;
	std::string getBindableTypeName() override { return mTypeName; }
	IBindable* getBindableParent() override { return mParent; };

protected:
	IBindable*    mParent;
	std::string   mTypeName;
	GuiComponent* mComponent;
};

/// <summary>
/// Class for ItemTemplate roots, that exposes {grid:XXX} properties to binding. {grid:label} is exposed though name parameter
/// </summary>
class GridTemplateBinding : public ComponentBinding
{
public:
	GridTemplateBinding(GuiComponent* comp, const std::string& label, IBindable* parent);
	BindableProperty getProperty(const std::string& name) override;

	std::string getBindableTypeName() override { return "grid"; }

protected:
	std::string   mLabel;
};

class BindingManager
{
public:
	static void          updateBindings(GuiComponent* comp, IBindable* system, bool recursive = true);
	static std::string   evaluateBindableExpression(const std::string& xp, IBindable* bindable);

private:
	static void          bindValues(IBindable* current, std::string& xp, bool showDefaultText, std::string& evaluableExpression);
	static std::string   updateBoundExpression(std::string& xp, IBindable* bindable, bool showDefaultText);
};

#endif
