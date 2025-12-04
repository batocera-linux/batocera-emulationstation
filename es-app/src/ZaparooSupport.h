#ifndef ZAPAROO_SUPPORT_H
#define ZAPAROO_SUPPORT_H

#include <string>

class Zaparoo
{
public:
	static Zaparoo* getInstance();

	bool isZaparooEnabled();
	bool writeZaparooCard(std::string name);

protected:
	Zaparoo();
	bool executeScript(const std::string command);

private:
	static Zaparoo* sInstance;
};

#endif // ZAPAROO_SUPPORT_H
