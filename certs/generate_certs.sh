#~/bin/bash


cat root.cnf.in | envsubst > root.cnf
cat intermediate.cnf.in | envsubst > intermediate.cnf

touch index 
truncate index --size 0

OVERWRITE_ALL=0

promptyesno()
{
    if [ $OVERWRITE_ALL -eq 1 ]
    then
        return 0;
    fi

    read -p "$1" -n 1 -r
    if [[ $REPLY =~ ^[Yy]$ ]]
    then
        return 0
    else
        if [[ $REPLY =~ ^[Aa]$ ]]
        then
            OVERWRITE_ALL=1
            return 0
        else
            return -1
        fi
    fi
}

checkoverwrite()
{
    if [ -f "$1" ]
    then
        promptyesno "$1 already exists, overwrite [y/N/a]?"
        echo
        return $?
    else
        return 0
    fi
}


# generate private key for root CA
checkoverwrite ca_privkey.pem && openssl ecparam -name prime256v1 -genkey -noout -out ca_privkey.pem
# generate public key for root CA
checkoverwrite ca_pubkey.pem && openssl ec -in ca_privkey.pem -pubout -out ca_pubkey.pem
# generate root CA certificate
checkoverwrite ca_cert.pem && openssl req -config root.cnf \
    -new \
    -key ca_privkey.pem \
    -x509 -days 10000 -sha256 \
    -extensions v3_ca -out ca_cert.pem \
    -subj "/C=US/ST=Denial/L=Atlanta/O=Nowhere/CN=Linky Root CA"

# generate private key for intermediate CA
checkoverwrite intermediate_privkey.pem && openssl ecparam -name prime256v1 -genkey -noout -out intermediate_privkey.pem
# generate public key for intermediate CA
checkoverwrite intermediate_pubkey.pem && openssl ec -in intermediate_privkey.pem -pubout -out intermediate_pubkey.pem
# generate intermediate certifiate
openssl req -config intermediate.cnf \
    -new -sha256 \
    -key intermediate_privkey.pem \
    -out intermediate_csr.pem \
    -subj "/C=US/ST=Trance/L=Rave/O=Somewhere/CN=Linky Intermediate CA" && \
    openssl ca -config root.cnf \
        -extensions v3_intermediate_ca \
        -rand_serial \
        -batch -notext \
        -days 10000 -notext -md sha256 \
        -in intermediate_csr.pem \
        -out intermediate_cert.pem

# generate private key for server CA
checkoverwrite privkey.pem && openssl ecparam -name prime256v1 -genkey -noout -out privkey.pem
# generate public key for server CA
checkoverwrite pubkey.pem && openssl ec -in intermediate_privkey.pem -pubout -out pubkey.pem
# generate server certifiate
openssl req -config intermediate.cnf \
    -new -sha256 \
    -key privkey.pem \
    -out csr.pem \
    -subj "/C=US/ST=The Art/L=Fancy/O=Between/CN=localhost" && \
    openssl ca -config intermediate.cnf \
        -extensions server_cert \
        -rand_serial \
        -batch -notext \
        -days 10000 -notext -md sha256 \
        -in csr.pem \
        -out cert.pem

# create the certificate chain
cat cert.pem intermediate_cert.pem ca_cert.pem > chain.pem


# generate private key for JWT issuer
checkoverwrite jwtissuer_privkey.pem && openssl ecparam -name prime256v1 -genkey -noout -out jwtissuer_privkey.pem
# generate public key for JWT issuer
checkoverwrite jwtissuer_pubkey.pem && openssl ec -in jwtissuer_privkey.pem -pubout -out jwtissuer_pubkey.pem
