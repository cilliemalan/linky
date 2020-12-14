#include <stdlib.h>

#include "config.h"
#include "listener.h"
#include "database.h"
#include "logging.h"

int main()
{
    config cfg = NULL;
    database db = NULL;
    bool result = true;

    if (!config_load())
    {
        critical_error("Could not load configuration");
        result = false;
    }

    if (result)
    {
        cfg = config_get();
        result = !!cfg;
    }

    if (result)
    {
        db = database_open(cfg->database, true, cfg->setgid, cfg->setuid);
        result = !!db;
    }

    if (result)
    {
        if (!linky_listen())
        {
            critical_error("Could not listen");
            result = false;
        }
    }

    if (db)
    {
        database_close(db);
    }
    
    return result;
}
