#include <stdlib.h>
#include <stdio.h>

#include "config.h"
#include "logging.h"

int main()
{
    if (!config_load())
    {
        critical_error("Could not load configuration");
        return -1;
    }


    return 0;
}
