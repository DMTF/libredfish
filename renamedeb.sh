#!/bin/bash
if compgen -G "build/*.deb" > /dev/null; then
    if [ -z "$VERSION" ]; then 
        VERSION="0.0.1"
    fi
    mv build/libredfish_$VERSION-1_amd64.deb build/libredfish_$VERSION-$OS-$DIST_amd64.deb
fi