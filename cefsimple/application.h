#ifndef _APPLICATION_HYPHENATE_UIDEMO
#define _APPLICATION_HYPHENATE_UIDEMO

#include <emclient.h>
#include <sstream>
#include "simple_handler.h"
#include <mutex>
using namespace std;
extern easemob::EMClient *g_client;

class Utils{
public:
	static void CallJS(const std::stringstream & stream);
	static bool g_bRosterDownloaded;
	static std::mutex roster_mutex; //lock of g_bRosterDownloaded
	static bool g_bGroupListDownloaded;
	static std::mutex group_mutex; //lock of g_bGroupListDownloaded

	static inline BYTE toHex(const BYTE &x)
	{
		return x > 9 ? x - 10 + 'A' : x + '0';
	}

	inline static string URLEncode(const string &sIn)
	{
		string sOut;
		for (size_t ix = 0; ix < sIn.size(); ix++)
		{
			BYTE buf[4];
			memset(buf, 0, 4);
			if (isalnum((BYTE)sIn[ix]))
			{
				buf[0] = sIn[ix];
			}
			//else if (isspace((BYTE)sIn[ix]))//Character SPACE escape to "%20" rather than "+"
			//{
			//	buf[0] = '+';
			//}
			else
			{
				buf[0] = '%';
				buf[1] = toHex((BYTE)sIn[ix] >> 4);
				buf[2] = toHex((BYTE)sIn[ix] % 16);
			}
			sOut += (char *)buf;
		}
		return sOut;
	}
};

#endif