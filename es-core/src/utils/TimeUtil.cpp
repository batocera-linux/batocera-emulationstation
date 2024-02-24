#include "utils/TimeUtil.h"
#include "utils/StringUtil.h"
#include "LocaleES.h"

#if WIN32
#include <Windows.h>
#elif defined(__linux__)
#include <langinfo.h>
#endif

namespace Utils
{
	namespace Time
	{
		std::string getSystemDateFormat()
		{
			static std::string value;
			if (!value.empty())
				return value;

			std::string ret;

#if WIN32
			int bufferSize = GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SSHORTDATE, nullptr, 0);
			if (bufferSize > 0)
			{
				// Allocate a buffer to store the short date format
				char* buffer = new char[bufferSize];

				char dateTimeStr[64];
				SYSTEMTIME st;
				GetDateFormatA(LOCALE_USER_DEFAULT, DATE_SHORTDATE, NULL, NULL, dateTimeStr, sizeof(dateTimeStr));

				// Get the short date format
				if (GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SSHORTDATE, buffer, bufferSize) > 0)
				{
					ret = buffer;

					if (ret.find("yyyy") != std::string::npos)
						ret = Utils::String::replace(ret, "yyyy", "%Y");
					else
						ret = Utils::String::replace(ret, "yy", "%y");

					if (ret.find("MM") != std::string::npos)
						ret = Utils::String::replace(ret, "MM", "%m");
					else
						ret = Utils::String::replace(ret, "M", "%m"); // %#m

					if (ret.find("dd") != std::string::npos)
						ret = Utils::String::replace(ret, "dd", "%d");
					else
						ret = Utils::String::replace(ret, "d", "%d");

					if (ret.find("tt") != std::string::npos)
						ret = Utils::String::replace(ret, "tt", "%p");
					else
						ret = Utils::String::replace(ret, "t", "%p");

					if (ret.find("HH") != std::string::npos)
						ret = Utils::String::replace(ret, "HH", "%H");
					else
						ret = Utils::String::replace(ret, "H", "%H");

					if (ret.find("hh") != std::string::npos)
						ret = Utils::String::replace(ret, "hh", "%I");
					else
						ret = Utils::String::replace(ret, "h", "%I");

					ret = Utils::String::replace(ret, "mm", "%M");
					ret = Utils::String::replace(ret, "ss", "%S");
				}


				delete[] buffer;
			}
#elif defined(__linux__)
			char* date = nl_langinfo(D_FMT);
			if (date)
				ret = date;
#endif

			if (ret.empty())
				ret = "%m/%d/%Y";

			value = ret;
			return ret;
		}

		DateTime DateTime::now()
		{
			return Utils::Time::DateTime(Utils::Time::now());
		}

		DateTime::DateTime()
		{
			mTime       = 0;
			mTimeStruct = { 0, 0, 0, 1, 0, 0, 0, 0, -1 };
			mIsoString  = "00000000T000000";

		} // DateTime::DateTime

		DateTime::DateTime(const time_t& _time)
		{
			setTime(_time);

		} // DateTime::DateTime

		DateTime::DateTime(const tm& _timeStruct)
		{
			setTimeStruct(_timeStruct);

		} // DateTime::DateTime

		DateTime::DateTime(const std::string& _isoString)
		{
			setIsoString(_isoString);

		} // DateTime::DateTime

		DateTime::~DateTime()
		{

		} // DateTime::~DateTime

		void DateTime::setTime(const time_t& _time)
		{
			try
			{
				mTime = (_time < 0) ? 0 : _time;
				mTimeStruct = *localtime(&mTime);
				mIsoString = timeToString(mTime);
			}
			catch (...)
			{
				mTime = 0;
				mTimeStruct = { 0, 0, 0, 1, 0, 0, 0, 0, -1 };
				mIsoString = "00000000T000000";
			}
		} // DateTime::setTime

		void DateTime::setTimeStruct(const tm& _timeStruct)
		{
			setTime(mktime((tm*)&_timeStruct));

		} // DateTime::setTimeStruct

		void DateTime::setIsoString(const std::string& _isoString)
		{
			setTime(stringToTime(_isoString));

		} // DateTime::setIsoString

		double	DateTime::elapsedSecondsSince(const DateTime& _since)
		{
			return difftime(mTime, _since.mTime);
		}

		std::string DateTime::toLocalTimeString()
		{
			time_t     clockNow = getTime();
			struct tm  clockTstruct = *localtime(&clockNow);

			char       clockBuf[256];
			strftime(clockBuf, sizeof(clockBuf), "%x %R", &clockTstruct);
			return clockBuf;
		}

		Duration::Duration(const time_t& _time)
		{
			mTotalSeconds = (unsigned int)_time;
			mDays         = (mTotalSeconds - (mTotalSeconds % (60*60*24))) / (60*60*24);
			mHours        = ((mTotalSeconds % (60*60*24)) - (mTotalSeconds % (60*60))) / (60*60);
			mMinutes      = ((mTotalSeconds % (60*60)) - (mTotalSeconds % (60))) / 60;
			mSeconds      = mTotalSeconds % 60;

		} // Duration::Duration

		Duration::~Duration()
		{

		} // Duration::~Duration

		time_t now()
		{
			time_t time;
			::time(&time);
			return time;

		} // now

		time_t stringToTime(const std::string& _string, const std::string& _format)
		{
			const char* s           = _string.c_str();
			const char* f           = _format.c_str();
			tm          timeStruct  = { 0, 0, 0, 1, 0, 0, 0, 0, -1 };
			size_t      parsedChars = 0;

			if(_string == "not-a-date-time")
				return mktime(&timeStruct);

			while(*f && (parsedChars < _string.length()))
			{
				if(*f == '%')
				{
					++f;
				
					switch(*f++)
					{
						case 'Y': // The year [1970,xxxx]
						{
							if((parsedChars + 4) <= _string.length())
							{
								timeStruct.tm_year  = (*s++ - '0') * 1000;
								timeStruct.tm_year += (*s++ - '0') * 100;
								timeStruct.tm_year += (*s++ - '0') * 10;
								timeStruct.tm_year += (*s++ - '0');
								if(timeStruct.tm_year >= 1900)
									timeStruct.tm_year -= 1900;
							}

							parsedChars += 4;
						}
						break;

						case 'y': // The year [1970,xxxx]
						{
							if ((parsedChars + 2) <= _string.length())
							{
								timeStruct.tm_year = (*s++ - '0') * 10;
								timeStruct.tm_year += (*s++ - '0');								
								if (timeStruct.tm_year < 50)
									timeStruct.tm_year += 100;
							}

							parsedChars += 2;
						}
						break;

						case 'm': // The month number [01,12]
						{
							if((parsedChars + 2) <= _string.length())
							{
								timeStruct.tm_mon  = (*s++ - '0') * 10;
								timeStruct.tm_mon += (*s++ - '0');
								if(timeStruct.tm_mon >= 1)
									timeStruct.tm_mon -= 1;
							}

							parsedChars += 2;
						}
						break;

						case 'd': // The day of the month [01,31]
						{
							if((parsedChars + 2) <= _string.length())
							{
								timeStruct.tm_mday  = (*s++ - '0') * 10;
								timeStruct.tm_mday += (*s++ - '0');
							}

							parsedChars += 2;
						}
						break;

						case 'H': // The hour (24-hour clock) [00,23]
						{
							if((parsedChars + 2) <= _string.length())
							{
								timeStruct.tm_hour  = (*s++ - '0') * 10;
								timeStruct.tm_hour += (*s++ - '0');
							}

							parsedChars += 2;
						}
						break;

						case 'M': // The minute [00,59]
						{
							if((parsedChars + 2) <= _string.length())
							{
								timeStruct.tm_min  = (*s++ - '0') * 10;
								timeStruct.tm_min += (*s++ - '0');
							}

							parsedChars += 2;
						}
						break;

						case 'S': // The second [00,59]
						{
							if((parsedChars + 2) <= _string.length())
							{
								timeStruct.tm_sec  = (*s++ - '0') * 10;
								timeStruct.tm_sec += (*s++ - '0');
							}

							parsedChars += 2;
						}
						break;
					}
				}
				else
				{
					++s;
					++f;
				}
			}

			return mktime(&timeStruct);

		} // stringToTime

		std::string timeToString(const time_t& _time, const std::string& _format)
		{
			const char* f = _format.c_str();
			const tm timeStruct = *localtime(&_time);
			char buf[256] = { '\0' };
			char* s = buf;

			while(*f)
			{
				if(*f == '%')
				{
					++f;
				
					switch(*f++)
					{
						case 'Y': // The year, including the century (1900)
						{
							const int year = timeStruct.tm_year + 1900;
							*s++ = (char)((year - (year % 1000)) / 1000) + '0';
							*s++ = (char)(((year % 1000) - (year % 100)) / 100) + '0';
							*s++ = (char)(((year % 100) - (year % 10)) / 10) + '0';
							*s++ = (char)(year % 10) + '0';
						}
						break;

						case 'y': // The year, without the century
						{
							const int year = timeStruct.tm_year + 1900;
							*s++ = (char)(((year % 100) - (year % 10)) / 10) + '0';
							*s++ = (char)(year % 10) + '0';
						}
						break;

						case 'm': // The month number [00,11]
						{
							const int mon = timeStruct.tm_mon + 1;
							*s++ = (char)(mon / 10) + '0';
							*s++ = (char)(mon % 10) + '0';
						}
						break;

						case 'd': // The day of the month [01,31]
						{
							*s++ = (char)(timeStruct.tm_mday / 10) + '0';
							*s++ = (char)(timeStruct.tm_mday % 10) + '0';
						}
						break;

						case 'H': // The hour (24-hour clock) [00,23]
						{
							*s++ = (char)(timeStruct.tm_hour / 10) + '0';
							*s++ = (char)(timeStruct.tm_hour % 10) + '0';
						}
						break;

						case 'I': // The hour (12-hour clock) [01,12]
						{
							int h = timeStruct.tm_hour;
							if (h >= 12)
								h -= 12;
							if (h == 0)
								h = 12;

							*s++ = (char)(h / 10) + '0';
							*s++ = (char)(h % 10) + '0';
						}
						break;

						case 'p': // AM / PM
						{
							*s++ = timeStruct.tm_hour < 12 ? 'A' : 'P';
							*s++ = 'M';
						}
						break;

						case 'M': // The minute [00,59]
						{
							*s++ = (char)(timeStruct.tm_min / 10) + '0';
							*s++ = (char)(timeStruct.tm_min % 10) + '0';
						}
						break;

						case 'S': // The second [00,59]
						{
							*s++ = (char)(timeStruct.tm_sec / 10) + '0';
							*s++ = (char)(timeStruct.tm_sec % 10) + '0';
						}
						break;
					}
				}
				else
				{
					*s++ = *f++;
				}

				*s = '\0';
			}

			return std::string(buf);

		} // timeToString

		  // transforms a number of seconds into a human readable string
		std::string secondsToString(const long seconds, bool asTime)
		{
			if (seconds == 0)
				return _("never");

			if (asTime)
			{
				int d = 0, h = 0, m = 0, s = 0;
				d = seconds / 86400;
				h = (seconds / 3600) % 24;
				m = (seconds / 60) % 60;
				s = seconds % 60;

				if (d > 0)
					return Utils::String::format("%02d %02d:%02d:%02d", d, h, m, s);
				else if (h > 0)
					return Utils::String::format("%02d:%02d:%02d", h, m, s);

				return Utils::String::format("%02d:%02d", m, s);
			}

			char buf[256];

			int d =0, h = 0, m = 0, s = 0;
			d = seconds / 86400;
			h = (seconds / 3600) % 24;
			m = (seconds / 60) % 60;
			s = seconds % 60;
			if (d > 1)
			{
				snprintf(buf, 256, _("%d d").c_str(), d);
				if (h > 0)
				{
					std::string days(buf);
					snprintf(buf, 256, _("%d h").c_str(), h);
					if(m > 0)
					{
						std::string hours(buf);
						snprintf(buf, 256, _("%d mn").c_str(), m);
						return days + " " + hours + " " + std::string(buf);
					}
					return days + " " + std::string(buf);
				}
				else if (m > 0)
				{
					std::string days(buf);
					snprintf(buf, 256, _("%d mn").c_str(), m);
					return days + " " + std::string(buf);
				}
			}
			else if (h > 0 || d > 0)
			{
				if (d > 0)
					h += d * 24;

				snprintf(buf, 256, _("%d h").c_str(), h);
				if (m > 0)
				{
					std::string hours(buf);
					snprintf(buf, 256, _("%d mn").c_str(), m);
					return hours + " " + std::string(buf);
				}
			}
			else if (m > 0)
				snprintf(buf, 256, _("%d mn").c_str(), m);
			else 
				snprintf(buf, 256, _("%d sec").c_str(), s);

			return std::string(buf);	
		}

		std::string getElapsedSinceString(const time_t& _time)
		{			
			if (_time == 0 || _time == -1)
				return _("never");

			Utils::Time::DateTime now(Utils::Time::now());
			Utils::Time::Duration dur(now.getTime() - _time);

			char buf[256];

			if (dur.getDays() > 365)
			{
				unsigned int years = dur.getDays() / 365;
				snprintf(buf, 256, ngettext("%d year ago", "%d years ago", years), years);
			}
			else if (dur.getDays() > 0)
				snprintf(buf, 256, ngettext("%d day ago", "%d days ago", dur.getDays()), dur.getDays());
			else if (dur.getDays() > 0)
				snprintf(buf, 256, ngettext("%d day ago", "%d days ago", dur.getDays()), dur.getDays());
			else if (dur.getHours() > 0)
				snprintf(buf, 256, ngettext("%d hour ago", "%d hours ago", dur.getHours()), dur.getHours());
			else if (dur.getMinutes() > 0)
				snprintf(buf, 256, ngettext("%d minute ago", "%d minutes ago", dur.getMinutes()), dur.getMinutes());
			else
				snprintf(buf, 256, ngettext("%d second ago", "%d seconds ago", dur.getSeconds()), dur.getSeconds());

			return std::string(buf);
		}

		int daysInMonth(const int _year, const int _month)
		{
			tm timeStruct = { 0, 0, 0, 0, _month, _year - 1900, 0, 0, -1 };
			mktime(&timeStruct);

			return timeStruct.tm_mday;

		} // daysInMonth

		int daysInYear(const int _year)
		{
			tm timeStruct = { 0, 0, 0, 0, 0, _year - 1900 + 1, 0, 0, -1 };
			mktime(&timeStruct);

			return timeStruct.tm_yday + 1;

		} // daysInYear

	} // Time::

} // Utils::
