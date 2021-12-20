#ifndef _LOCALE_H_
#define _LOCALE_H_


#if !defined(WIN32)

#include <string>

#ifdef HAVE_INTL

#define ENABLE_NLS 1
#include "gettext.h"
#define _(A) std::string(gettext(A))

#else

#define _(A) std::string(A)
char* ngettext(char* msgid, char* msgid_plural, unsigned long int n);
char* pgettext(char* context, char* msgid);

#endif

#define _U(x) x

class EsLocale
{
public:
	static std::string init(std::string locale, std::string path);
	static std::string changeLocale(const std::string& locale);

	static const bool isRTL();
private:
	static std::string default_LANGUAGE;
};

#else // WIN32

#include <string>
#include <map>
#include <functional>
#include "utils/StringUtil.h"

#define _U(x) Utils::String::convertFromWideString(L ## x)

struct PluralRule
{
	std::string key;
	std::string rule;
	std::function<int(int n)> evaluate;
};

class EsLocale
{
public:
	static const std::string getText(const std::string& text);
	static const std::string getTextWithContext(const std::string& context, const std::string& text);
	static const std::string nGetText(const std::string msgid, const std::string msgid_plural, int n);

	static const std::string getLanguage() { return mCurrentLanguage; }

	static const bool isRTL();

	static const void reset() { mCurrentLanguageLoaded = false; }

private:
	static void checkLocalisationLoaded();
	static std::map<std::string, std::string> mItems;
	static std::string mCurrentLanguage;
	static bool mCurrentLanguageLoaded;

	static PluralRule mPluralRule;
};

#define _U(x) Utils::String::convertFromWideString(L ## x)

#define _(x) EsLocale::getText(x)
#define ngettext(A, B, C) EsLocale::nGetText(A, B, C).c_str()
#define pgettext(A, B) EsLocale::getTextWithContext(A, B)

#endif // WIN32

#endif
