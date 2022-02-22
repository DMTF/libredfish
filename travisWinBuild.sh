#!/bin/sh -x
#Get CURL
echo "Downloading CURL..."
if [[ "$VISUAL_STUDIO" == "Visual Studio 15 2017 Win64" ]]; then
CMAKE_FLAGS="-A x64"
wget --no-check-certificate https://curl.se/windows/curl-win64-latest.zip
7z x curl-win64-latest.zip
rm curl-win64-latest.zip
mv curl-* curl
else
CMAKE_FLAGS=
wget --no-check-certificate https://curl.se/windows/curl-win32-latest.zip
7z x curl-win32-latest.zip
rm curl-win32-latest.zip
mv curl-* curl
fi
cp curl/lib/libcurl.dll.a curl/lib/curl.lib
echo "Completed downloading CURL"
#Get Jansson
echo "Downloading Jansson..."
wget -O jansson.lib https://github.com/pboyd04/jansson/releases/download/v2.11/$JANSSON_LIB
wget -O include/jansson.h https://github.com/pboyd04/jansson/releases/download/v2.11/jansson.h
wget -O include/jansson_config.h https://github.com/pboyd04/jansson/releases/download/v2.11/jansson_config.h
echo "Completed downloading Jansson..."
#Get getopt.h for windows
echo "Downloading getopt.h for windows..."
wget -O include/getopt.h https://raw.githubusercontent.com/skandhurkat/Getopt-for-Visual-Studio/master/getopt.h
echo "Completed downloading getopt.h for windows..."
#Do Build
mkdir build
cd build 
echo "Configuring..."
cmake .. $CMAKE_FLAGS -DCURL_LIBRARY=../curl/lib/curl.lib -DCURL_INCLUDE_DIR=../curl/include -DJANSSON_LIBRARIES=../jansson.lib -DJANSSON_INCLUDE_DIRS=../
echo "Building..."
cmake --build . --config Release
echo "Making deployment directory..."
mkdir ../deploy
cp ../curl/bin/libcurl* ../deploy
cp bin/Release/redfish.dll ../deploy
cp bin/Release/redfishtest.exe ../deploy
