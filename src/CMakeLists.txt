INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})

INCLUDE(compat_flags)


# Add Definitions, Compiler-Switches, etc.: -Wall -O2 -g3 ...
# MSVC compiler (cl.exe) does not accept the same switches as gcc, although preprocessor definitions in the -D form will work for both
IF(NOT MSVC)
	IF( NOT MINGW )
		#wxMSW generates > 9000 warnings with -wextra...
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wextra")
	ENDIF( NOT MINGW )
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-strict-aliasing")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-strict-aliasing")
ELSE(NOT MSVC)
	ADD_DEFINITIONS(-D_RC_MSVC)
ENDIF(NOT MSVC)

if(WIN32)
	add_definitions(
		-D__WXMSW__
		-D_WIN32_WINNT=0x0501
		-DBOOST_THREAD_USE_LIB
		-DSTRICT
		-DHAVE_W32API_H
	)
	set(Boost_THREADAPI win32)
endif(WIN32)

set(Boost_USE_MULTITHREADED      ON)
if( ${CMAKE_CROSSCOMPILING} )
	set(Boost_USE_STATIC_LIBS       ON)
	set(Boost_USE_STATIC_RUNTIME    ON)
endif()

FIND_PACKAGE(Boost 1.42.0 COMPONENTS thread system REQUIRED)
INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})

ADD_DEFINITIONS( -DLOCALE_INSTALL_DIR="${LOCALE_INSTALL_DIR}" )



if (CMAKE_CROSSCOMPILING)
	message(STATUS "Crosscompiling: using static flags for wxwidgets")
	set(WXSTATIC "--static")
	#FIXME: autodetect thise flags:
	add_definitions(-DAL_LIBTYPE_STATIC)
else()
	set(WXSTATIC "")
endif()
find_program(wxWidgets_CONFIG_EXECUTABLE wx-config ONLY_CMAKE_FIND_ROOT_PATH)
if (wxWidgets_CONFIG_EXECUTABLE-NOTFOUND)
	message(FATAL_ERROR "couldn't find wx-config!")
else()
	message(STATUS "found wx-config: ${wxWidgets_CONFIG_EXECUTABLE}")
endif()

execute_process(COMMAND ${wxWidgets_CONFIG_EXECUTABLE} "--version-full"
	OUTPUT_VARIABLE WX_VERSION
	OUTPUT_STRIP_TRAILING_WHITESPACE)

set(WX_VERSION_MIN "2.8")
if("${WX_VERSION}" VERSION_LESS "${WX_VERSION_MIN}")
	message(FATAL_ERROR "couldn't find wx-widgets >= ${WX_VERSION_MIN}, found ${WX_VERSION}")
else()
	message(STATUS "found wxwidgets: ${WX_VERSION}")
endif()

execute_process(COMMAND ${wxWidgets_CONFIG_EXECUTABLE} "--cppflags" "${WXSTATIC}"
	OUTPUT_VARIABLE WX_CXX_FLAGS
	OUTPUT_STRIP_TRAILING_WHITESPACE )
message(STATUS "Using WX_CXX_FLAGS: ${WX_CXX_FLAGS}")

execute_process(COMMAND ${wxWidgets_CONFIG_EXECUTABLE} "--cflags" "${WXSTATIC}"
	OUTPUT_VARIABLE WX_C_FLAGS
	OUTPUT_STRIP_TRAILING_WHITESPACE )
message(STATUS "Using WX_C_FLAGS: ${WX_C_FLAGS}")

execute_process(COMMAND ${wxWidgets_CONFIG_EXECUTABLE} "--libs" "net,qa,richtext,adv,aui,core" "${WXSTATIC}"
	OUTPUT_VARIABLE WX_LD_FLAGS
	OUTPUT_STRIP_TRAILING_WHITESPACE )
message(STATUS "Using WX_LD_FLAGS: ${WX_LD_FLAGS}")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WX_CXX_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WX_C_FLAGS}")

#----------------------------------------------------------------------------------------------------
# openAL libs dependency check
#----------------------------------------------------------------------------------------------------

IF( MINGW )
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -gstabs")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -gstabs")
	LINK_LIBRARIES( intl)
ENDIF( MINGW )

FIND_PACKAGE( CURL REQUIRED libcurl )
IF( NOT CURL_FOUND )
	MESSAGE( FATAL_ERROR "required library libCURL not found" )
ENDIF( NOT CURL_FOUND )

if(DEFINED PKG_CONFIG_EXECUTABLE) # override CURL_LIBRARIES if curl-config is set
	execute_process(COMMAND ${PKG_CONFIG_EXECUTABLE} "libcurl" "--libs"
		OUTPUT_VARIABLE CURL_LINKFLAGS
		OUTPUT_STRIP_TRAILING_WHITESPACE )

	SET(CURL_LINKFLAGS "${CURL_LINKFLAGS}" CACHE STRING "" FORCE)
	message(STATUS "${PKG_CONFIG_EXECUTABLE} returned ${CURL_LINKFLAGS} for linking to libcurl")

	execute_process(COMMAND ${PKG_CONFIG_EXECUTABLE} "libcurl" "--cflags-only-other"
		OUTPUT_VARIABLE CURL_DEFINITONS
		OUTPUT_STRIP_TRAILING_WHITESPACE )
	add_definitions(${CURL_DEFINITONS})
	message(STATUS "${PKG_CONFIG_EXECUTABLE} returned ${CURL_DEFINITONS} for compiling with libcurl")
endif()



ADD_DEFINITIONS( ${CURL_CFLAGS} )
#add_to_global(link_directories  ${CURL_LIB_DIRS} )
INCLUDE_DIRECTORIES( ${CURL_INCLUDE_DIR} )

#----------------------------------------------------------------------------------------------------
# libnotify dependency check
#----------------------------------------------------------------------------------------------------

IF( OPTION_NOTIFY AND NOT WIN32 )
	pkg_check_modules( LIBNOTIFY libnotify )
	pkg_check_modules( GLIB glib-2.0 )
	IF( LIBNOTIFY_FOUND )
		INCLUDE_DIRECTORIES( ${LIBNOTIFY_INCLUDE_DIRS} )
		LINK_LIBRARIES( ${LIBNOTIFY_LIBRARIES} )
		INCLUDE_DIRECTORIES( ${GLIB_INCLUDE_DIRS} )
		LINK_LIBRARIES( ${GLIB_LIBRARIES} )
		ADD_DEFINITIONS( -DHAVE_LIBNOTIFY )
	ENDIF( LIBNOTIFY_FOUND )
ENDIF( OPTION_NOTIFY AND NOT WIN32 )


set(springlobbySrc
	gui/slbook.cpp
	aui/auimanager.cpp

	gui/hosting/addbotdialog.cpp
	gui/hosting/autobalancedialog.cpp
	gui/hosting/battlemaptab.cpp
	gui/hosting/battleoptionstab.cpp
	gui/hosting/battleroomlistctrl.cpp
	gui/hosting/battleroomtab.cpp
	gui/hosting/hostbattledialog.cpp
	gui/hosting/mainjoinbattletab.cpp
	gui/hosting/mainsingleplayertab.cpp
	gui/hosting/mmoptionwindows.cpp
	gui/hosting/battleroomdownloads.cpp
	gui/hosting/battleroommmoptionstab.cpp
	autohost.cpp
	autohostmanager.cpp

	gui/options/downloadoptionspanel.cpp
	gui/options/mainoptionstab.cpp
	gui/options/springoptionstab.cpp
	gui/options/groupoptionspanel.cpp
	gui/options/lobbyoptionstab.cpp
	gui/options/chatoptionstab.cpp

	gui/playback/playbackfilter.cpp
	gui/playback/iplaybacklist.cpp
	gui/playback/playbacklistctrl.cpp
	gui/playback/playbacktab.cpp
	gui/playback/playbackthread.cpp
	gui/playback/replaylist.cpp
	gui/playback/savegamelist.cpp


	utils/TextCompletionDatabase.cpp
	gui/wxtextctrlhist.cpp
	gui/colorbutton.cpp

	gui/toasternotification.cpp
	gui/wxbackgroundimage.cpp
	gui/statusbar.cpp
	gui/pastedialog.cpp
	gui/notificationmanager.cpp
	gui/taskbar.cpp
	gui/contentdownloaddialog.cpp
	gui/contentsearchresultslistctrl.cpp

	gui/activitynotice.cpp
	utils/misc.cpp
	utils/tasutil.cpp
	utils/md5.c
	utils/battleevents.cpp

	gui/channel/channelchooser.cpp
	gui/channel/channelchooserdialog.cpp
	gui/channel/autojoinchanneldialog.cpp
	gui/channel/channellistctrl.cpp
	channel.cpp
	channellist.cpp

	gui/battlelist/battlelistctrl.cpp
	gui/battlelist/battlelistfilter.cpp
	gui/battlelist/battlelisttab.cpp
	battlelist.cpp

	downloader/prdownloader.cpp
	downloader/downloadlistctrl.cpp
	downloader/downloadsobserver.cpp
	downloader/httpdownloader.cpp

	gui/toasterbox/ToasterBox.cpp
	gui/toasterbox/ToasterBoxWindow.cpp
	gui/toasterbox/ToasterBoxWindowList.cpp

	gui/chatpanelmenu.cpp
	gui/agreementdialog.cpp
	utils/base64.cpp
	battle.cpp
	utils/crc.cpp
	chatlog.cpp
	gui/chatpanel.cpp
	gui/connectwindow.cpp
	countrycodes.cpp
	gui/customlistctrl.cpp
	flagimages.cpp
	ibattle.cpp
	iconimagelist.cpp
	gui/mainchattab.cpp
	gui/maindownloadtab.cpp
	gui/mainwindow.cpp
	gui/mapctrl.cpp
	gui/mapgridctrl.cpp
	gui/mapselectdialog.cpp
	contentsearchresult.cpp
	gui/nicklistctrl.cpp
	offlinebattle.cpp
	
	
	gui/selectusersdialog.cpp
	iserver.cpp
	serverevents.cpp
	gui/singleplayerbattle.cpp
	gui/singleplayertab.cpp
	socket.cpp
	spring.cpp
	springlobbyapp.cpp
	springprocess.cpp
	tasserver.cpp
	gui/ui.cpp
	user.cpp
	useractions.cpp
	userlist.cpp
	serverselector.cpp
	
	lsl/src/lsl/battle/tdfcontainer.cpp #FIXME
	springsettings/frame.cpp
)

SET(SLSharedWithSettings
	utils/wxTranslationHelper.cpp
	gui/uiutils.cpp
	updater/versionchecker.cpp
	updater/updatehelper.cpp
	gui/controls.cpp
	utils/conversion.cpp
	gui/customdialogs.cpp
	utils/globalevents.cpp
	utils/platform.cpp
	utils/slpaths.cpp
	gui/sltipwin.cpp
	utils/uievents.cpp
	utils/curlhelper.cpp
	crashreport.cpp
	settings.cpp
	gui/infodialog.cpp
	gui/windowattributespickle.cpp
	gui/spinctl/generic/spinctlg.cpp
	gui/spinctl/generic/spinctrldouble.cpp
	utils/slconfig.cpp
	gui/aboutbox.cpp
	log.cpp
)

FILE( GLOB header "*.h" )
FILE( GLOB header2 "*/*.h" )
# FILE( GLOB header3 "*/*/*.h" )

#grep -lR cpp\"$ * | sed -e "s/\.h/\.cpp/g"
SET( templatesources
	hosting/battleroommmoptionstab.cxx
	customvirtlistctrl.cpp
	playback/playbackfilter.cpp
	playback/playbacklist.cpp
	playback/playbacklistctrl.cpp
	playback/playbackthread.cpp
	playback/playbacktab.cpp
	utils/stringserializer.cpp
)

list(APPEND SLSharedWithSettings stacktrace.cpp)
add_library(slcommon STATIC
		${SLSharedWithSettings}
	)
target_link_libraries(slcommon
		springlobbyversion
		libsettings
		lsl-unitsync
		${CURL_LIBRARIES}
	)

add_subdirectory(downloader)
add_subdirectory(utils)

set(LSLSERVER OFF CACHE BOOL "disable lsl-server build" FORCE)
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/lsl/CMakeLists.txt")
	MESSAGE(FATAL_ERROR "lsl/CMakeLists.txt doesn't exist, please run \ngit submodule update --init")
else()
	add_definitions(-DHAVE_WX -DHAVE_SPRINGLOBBY)
	Add_Subdirectory(lsl)
	INCLUDE_DIRECTORIES(lsl/src)
	remove_definitions(-DHAVE_WX -DHAVE_SPRINGLOBBY)
endif()
add_subdirectory(springsettings)
add_subdirectory(sound)
add_subdirectory(json)


set_source_files_properties(  ${templatesources} PROPERTIES HEADER_FILE_ONLY 1 )

#mylink( ${EXTRA_LIB_DIRS} ${link_directories} )

IF(WIN32)
	set(springlobby_RC_FILE springlobby.rc)
ENDIF()
add_subdirectory(updater)


ADD_EXECUTABLE(springlobby WIN32 MACOSX_BUNDLE
		${springlobbySrc}
		${springlobby_RC_FILE}
	)
#the updater needs to be build for windows only
IF(WIN32)
	TARGET_LINK_LIBRARIES( springlobby
		iphlpapi
		nettle
		ws2_32
	)
	install(TARGETS springlobby RUNTIME DESTINATION .)
ELSE (WIN32)
	install(TARGETS springlobby RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin )
ENDIF (WIN32)

TARGET_LINK_LIBRARIES(springlobby lsl-utils lsl-unitsync pr-downloader_static json springlobbyversion libsettings slcommon
		${Boost_LIBRARIES}
		${CURL_LIBRARIES}
		${CURL_LINKFLAGS}
		${WX_LD_FLAGS}
		sound
	)
#add_dependencies(springlobby version)
IF(UNIX)
	FIND_PACKAGE(X11 REQUIRED)
	TARGET_LINK_LIBRARIES(springlobby ${X11_X11_LIB})
ENDIF(UNIX)

ENABLE_TESTING()
add_subdirectory(tests)

add_analyze(${springlobbySoundsrc} ${springlobbySrc} ${SLSharedWithSettings})
#add_format(${springlobbySoundsrc} ${springlobbySrc} ${SLSharedWithSettings} ${templatesources} ${header} ${header2})

