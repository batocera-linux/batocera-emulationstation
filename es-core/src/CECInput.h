#pragma once
#ifndef ES_CORE_CECINPUT_H
#define ES_CORE_CECINPUT_H

#include <string>

#ifdef HAVE_LIBCEC
#include <libcec/cec.h>
#endif

namespace CEC { class ICECAdapter; }

class CECInput
{
public:

	static void        init              ();
	static void        deinit            ();
	static std::string getAlertTypeString(const unsigned int _type);
	static std::string getOpCodeString   (const unsigned int _opCode);
	static std::string getKeyCodeString  (const unsigned int _keyCode);

private:
#ifdef HAVE_LIBCEC
	CEC::ICECCallbacks        callbacks;
	CEC::libcec_configuration config;
#endif

	 CECInput();
	~CECInput();

	static CECInput*  sInstance;

	CEC::ICECAdapter* mlibCEC;

}; // CECInput

#endif // ES_CORE_CECINPUT_H
