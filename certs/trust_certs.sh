#!/bin/bash

if [[ "$(id)" =~ 'uid=0(root) gid=0(root)' ]]
then

    cp ./ca_cert.pem /usr/local/share/ca-certificates/linky_ca.crt

    dpkg-reconfigure -fnoninteractive ca-certificates

else

    echo 'this command must be run as root'

fi