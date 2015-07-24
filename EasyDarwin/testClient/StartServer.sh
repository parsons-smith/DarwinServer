#!/bin/bash

#start CMS Server
cd ../CMS
./CMSServer &

#start storage serv er
cd ../live555/StorageServer
./StorageServer &

#start video deal server
cd ../live555/VideoServer
./VideoServer &

#start darwin server
cd ../../DarwinServerDeploy
./DarwinServer -c darwinserver.xml -d &
