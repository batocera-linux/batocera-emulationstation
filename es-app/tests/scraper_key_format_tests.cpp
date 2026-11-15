// Unit tests of the runtime API key formatting and precedence (BYOK).
// Built with -DES_TESTS=ON, run with ctest.

#include "scrapers/ScraperKeyFormat.h"

#include <cstdio>
#include <string>

namespace
{
	int failures = 0;

	void check(bool condition, const char* expression, const char* file, int line)
	{
		if (!condition)
		{
			std::fprintf(stderr, "%s:%d: CHECK(%s) failed\n", file, line, expression);
			++failures;
		}
	}

	#define CHECK(x) check((x), #x, __FILE__, __LINE__)
}

int main()
{
	using namespace ScraperKeyFormat;

	// The user values win over the compiled fallback.
	CHECK(screenScraperDevLogin("dev", "pa#ss", "compiled") == "devid=dev&devpassword=pa%23ss");
	CHECK(gamesDBAPIKey("user-key", "compiled") == "user-key");
	CHECK(hfsDBLogin("user", "pa:ss", "compiled") == "user:pa:ss");
	CHECK(cheevosLogin("user", "web#key", "compiled") == "z=user&y=web%23key");

	// Without both user fields, the compiled fallback is used.
	CHECK(screenScraperDevLogin("", "", "compiled") == "compiled");
	CHECK(screenScraperDevLogin("dev", "", "compiled") == "compiled");
	CHECK(gamesDBAPIKey("", "compiled") == "compiled");
	CHECK(hfsDBLogin("user", "", "compiled") == "compiled");
	CHECK(cheevosLogin("", "webkey", "compiled") == "compiled");

	// An empty fallback stays empty.
	CHECK(screenScraperDevLogin("", "", "") == "");
	CHECK(gamesDBAPIKey("", "") == "");
	CHECK(hfsDBLogin("", "", "") == "");
	CHECK(cheevosLogin("", "", "") == "");

	// Reserved characters are percent-encoded like HttpReq::urlEncode.
	CHECK(urlEncode("a b") == "a%20b");
	CHECK(urlEncode("a+b") == "a%2Bb");
	CHECK(urlEncode("a/b?c=d&e") == "a%2Fb%3Fc%3Dd%26e");
	CHECK(urlEncode("AZaz09-_.~") == "AZaz09-_.~");

	if (failures == 0)
		std::printf("scraper key format tests passed\n");

	return failures == 0 ? 0 : 1;
}
