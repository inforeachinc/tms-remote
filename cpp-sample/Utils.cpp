#include <time.h>

#include "Utils.h"

std::string Utils::get_timestamp()
{
    time_t rawtime;
    struct tm timeinfo;
    char buffer[80];

    time(&rawtime);
    localtime_s(&timeinfo, &rawtime);

    strftime(buffer, 80, "[%F %T] ", &timeinfo);
    return buffer;
}
