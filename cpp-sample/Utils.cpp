#include <time.h>

#include "Utils.h"

std::string Utils::get_timestamp()
{
    time_t rawtime;
    struct tm timeinfo;
    char buffer[80];

    time(&rawtime);
#ifdef WIN32    
    localtime_s(&timeinfo, &rawtime);
#else
    localtime_r(&rawtime, &timeinfo);
#endif    

    strftime(buffer, 80, "[%F %T] ", &timeinfo);
    return buffer;
}
