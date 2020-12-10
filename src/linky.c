#include "config.h"
#include "listener.h"
#include "logging.h"

int main()
{
    if (!config_load())
    {
        critical_error("Could not load configuration");
        return -1;
    }

    if (!linky_listen())
    {
        critical_error("Could not listen");
        return -1;
    }

    return 0;
}
