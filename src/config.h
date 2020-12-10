#pragma once
#include <stdbool.h>


// application configuration. Loaded from environment
typedef struct config_s {

    // whether or not logging is enable. from env LINKY_LOGGING.
    bool logging;

    // The port to listen on. From env LINKY_PORT. Default 80.
    const char* port;
    
    // The port to listen on. From env LINKY_SECURE_PORT. Default 443.
    const char* secure_port;

    // the database file. From env LINKY_DATABASE. Default /var/lib/linky/linky.db
    const char* database;

    // the certificate chain file. From env LINKY_CERT_CHAIN. Default /etc/linky/cert.pem
    const char* certificate_chain_path;
    
    // the certificate key file. From env LINKY_CERT_KEY. Default /etc/linky/privkey.pem
    const char* certificate_key_path;

    // JWT audience. from env LINKY_JWT_AUDIENCE. Default "linky"
    const char* jwt_audience;

    // name of the issuer to be considered valid. From env LINKY_JWT_ISSUER. No default. 
    // if not specified no token will be accepted.
    const char* jwt_issuer;

    // PEM encoded issuer public key. From env LINKY_JWT_ISSUER_KEY. No default.
    // if not specified no token will be accepted.
    const char* jwt_issuer_key;

} config_t;

bool config_load();
const config_t* config_get();