# Helper invoked from MacOSBundle.cmake to render Info.plist.in at bundle
# assembly time. Invoked as:
#   cmake -DPROJECT_VERSION=... -DCMAKE_OSX_DEPLOYMENT_TARGET=... \
#         -DINPUT=... -DOUTPUT=... -P cmake/RenderInfoPlist.cmake
configure_file(${INPUT} ${OUTPUT} @ONLY)
