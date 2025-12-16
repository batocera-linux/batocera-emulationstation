#ifndef ZAPAROO_SUPPORT_H
#define ZAPAROO_SUPPORT_H

#include <string>

class Zaparoo
{
public:
	static Zaparoo* getInstance();

	bool isZaparooEnabled();
	bool writeZaparooCard(std::string name);
	void invalidateCache();

protected:
	Zaparoo();

private:
	static Zaparoo* sInstance;
	// Cache whether we already checked Zaparoo availability and the result
	bool mChecked;
	bool mAvailable;
};

#endif // ZAPAROO_SUPPORT_H
