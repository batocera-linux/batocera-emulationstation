#ifndef ZAPAROO_SUPPORT_H
#define ZAPAROO_SUPPORT_H

#include <string>
#include <functional>

class Zaparoo
{
public:
	static bool isZaparooEnabled(long defaultDelay = 100L);
	static void checkZaparooEnabledAsync(const std::function<void(bool enabled)>& func = nullptr);

	static bool writeZaparooCard(const std::string& name);
	static void invalidateCache();

private:
	static bool mChecked;
	static bool mAvailable;
};

#endif // ZAPAROO_SUPPORT_H
