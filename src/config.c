#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <stdio.h>
#include <ctype.h>

#include "config.h"
#include "logging.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// default config values
#define DEFAULT_PORT "80"
#define DEFAULT_SECURE_PORT "443"
#define DEFAULT_DATABASE "/var/lib/linky/linky.db"
#define DEFAULT_CERT_CHAIN "/etc/linky/cert.pem"
#define DEFAULT_CERT_KEY "/etc/linky/privkey.pem"
#define DEFAULT_JWT_AUDIENCE "linky"

static config_t *_config = NULL;

static bool file_exists(const char *file)
{
    return access(file, F_OK) == 0;
}

static const char *coalesce(const char *a, const char *b)
{
    return a ? a : b;
}

static bool validate_config(const config_t *config)
{
    bool result = true;
    if (config->port && config->database)
    {
        if (!config->port || !config->port[0])
        {
            critical_error("No port specified");
            result = false;
        }

        bool secure_wanted = (config->certificate_chain_path && config->certificate_chain_path[0]) ||
                             (config->certificate_key_path && config->certificate_key_path[0]) ||
                             (config->secure_port && config->secure_port[0]);
        if (secure_wanted)
        {
            if (!config->certificate_chain_path || !config->certificate_chain_path[0])
            {
                warn("Certificate chain not specified");
            }
            else if (!file_exists(config->certificate_chain_path))
            {
                warnf("Cannot open certificate chain file %s", config->certificate_chain_path);
            }

            if (!config->certificate_key_path || !config->certificate_key_path[0])
            {
                warn("Certificate key not specified");
            }
            else if (!file_exists(config->certificate_key_path))
            {
                warnf("Cannot open certificate key file %s", config->certificate_key_path);
            }
        }

        bool jwt_wanted = (config->jwt_audience && config->jwt_audience[0]) ||
                          (config->jwt_issuer && config->jwt_issuer[0]) ||
                          (config->jwt_issuer_key && config->jwt_issuer_key[0]);
        if (jwt_wanted)
        {
            if (!config->jwt_audience || !config->jwt_audience[0])
            {
                warn("JWT audience not specified");
            }
            if (!config->jwt_issuer || !config->jwt_issuer[0])
            {
                warn("JWT issuer not specified");
            }
            if (!config->jwt_issuer_key || !config->jwt_issuer_key[0])
            {
                warn("JWT issuer key not specified");
            }
            else
            {
                if (!strstr(config->jwt_issuer_key, "-----BEGIN PUBLIC KEY-----") && !file_exists(config->jwt_issuer_key))
                {
                    warn("JWT issuer key invalid or does not exist");
                }
            }
        }
    }

    return result;
}

static void print_config(const config_t *config)
{
    if (config->logging)
    {
        debugf("debug logging: %s", config->logging ? "enabled" : "disabled");
        debugf("listen port: %s", config->port);
        debugf("TLS listen port: %s", coalesce(config->secure_port, "<N/A>"));
        debugf("database file: %s", config->database);
        debugf("certificate chain file: %s", coalesce(config->certificate_chain_path, "<N/A>"));
        debugf("certificate key file: %s", coalesce(config->certificate_key_path, "<N/A>"));
        debugf("JWT audience: %s", coalesce(config->jwt_audience, "<N/A>"));
        debugf("JWT issuer: %s", coalesce(config->jwt_issuer, "<N/A>"));
        debugf("JWT issuer key: %s", coalesce(config->jwt_issuer_key, "<N/A>"));
        debugf("setgid: %d", config->setgid);
        debugf("setuid: %d", config->setuid);
    }
}

static bool is_true(const char *msg)
{
    return msg && msg[0] &&
           (strcmp(msg, "1") == 0 ||
            strcasecmp(msg, "true") ||
            strcasecmp(msg, "yes"));
}

bool config_load()
{
    if (!_config)
    {
        config_t *newconfig = (config_t *)malloc(sizeof(config_t));

        newconfig->logging = is_true(getenv("LINKY_LOGGING"));

        newconfig->port = coalesce(getenv("LINKY_PORT"), DEFAULT_PORT);
        newconfig->secure_port = coalesce(getenv("LINKY_SECURE_PORT"), DEFAULT_SECURE_PORT);
        newconfig->database = coalesce(getenv("LINKY_DATABASE"), DEFAULT_DATABASE);
        newconfig->certificate_chain_path = coalesce(getenv("LINKY_CERT_CHAIN"), DEFAULT_CERT_CHAIN);
        newconfig->certificate_key_path = coalesce(getenv("LINKY_CERT_KEY"), DEFAULT_CERT_KEY);
        newconfig->jwt_audience = coalesce(getenv("LINKY_JWT_AUDIENCE"), DEFAULT_JWT_AUDIENCE);
        newconfig->jwt_issuer = getenv("LINKY_JWT_ISSUER");
        newconfig->jwt_issuer_key = getenv("LINKY_JWT_ISSUER_KEY");

        const char *setuidval = getenv("LINKY_UID");
        const char *setgidval = getenv("LINKY_UID");

        // parse setuid and setgid
        if (setuidval && setuidval[0])
        {
            newconfig->setuid = strtoul(setuidval, NULL, 10);
            if (newconfig->setuid == 0)
            {
                warnf("Cannot setuid to %s", setuidval);
            }
        }
        if (setgidval && setgidval[0])
        {
            newconfig->setgid = strtoul(setgidval, NULL, 10);
            if (newconfig->setgid == 0)
            {
                warnf("Cannot setgid to %s", setgidval);
            }
        }

        // warn if only one of setgid or setuid is set
        if (newconfig->setgid ^ newconfig->setuid)
        {
            warn("only one of setuid and setgid is valid");
        }

        if (validate_config(newconfig))
        {
            _config = newconfig;
            print_config(_config);
        }
    }

    return !!_config;
}

config config_get()
{
    config_load();
    return _config;
}
