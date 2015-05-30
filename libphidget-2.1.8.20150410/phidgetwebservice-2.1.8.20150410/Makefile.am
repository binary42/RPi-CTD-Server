## Process this file with automake to produce Makefile.in

AM_CFLAGS = -I$(top_srcdir) -I$(top_srcdir)/phidget21 -I$(top_srcdir)/phidget21/utils -I$(top_srcdir)/phidget21/include -I$(top_srcdir)/phidget21/linux -I$(top_srcdir)/phidget21/dict -D_LINUX

bin_PROGRAMS = phidgetwebservice21

phidgetwebservice21_SOURCES = phidgetinterface.c \
	phidgetinterface.h \
	eventhandlers.c \
	eventhandlers.h \
	PhidgetWebservice21.c \
	PhidgetWebservice21.h \
	stdafx.h \
	phidget21/dict/pdict.c \
	phidget21/dict/pdict.h \
	phidget21/dict/pdictserver.c \
	phidget21/dict/pdictserver.h \
	phidget21/dict/pdict-impl.h \
	phidget21/cphidgetconstants.c \
	phidget21/cphidgetconstants.h \
	phidget21/cphidgetlist.c \
	phidget21/cphidgetlist.h \
	phidget21/utils/md5.c \
	phidget21/utils/md5.h \
	phidget21/utils/plist.c \
	phidget21/utils/plist.h \
	phidget21/utils/ptree.c \
	phidget21/utils/ptree.h \
	phidget21/utils/utils.c \
	phidget21/utils/utils.h \
	phidget21/phidget21int.h \
	phidget21/cphidget.h \
	phidget21/cphidgetmacros.h \
	phidget21/cphidgetattr.h \
	phidget21/cphidgetconstantsinternal.h \
	phidget21/cphidgetaccelerometer.h \
	phidget21/cphidgetadvancedservo.h \
	phidget21/cphidgetanalog.h \
	phidget21/cphidgetbridge.h \
	phidget21/cphidgetencoder.h \
	phidget21/cphidgetfrequencycounter.h \
	phidget21/cphidgetgps.h \
	phidget21/cphidgetinterfacekit.h \
	phidget21/cphidgetir.h \
	phidget21/cphidgetled.h \
	phidget21/cphidgetmotorcontrol.h \
	phidget21/cphidgetphsensor.h \
	phidget21/cphidgetrfid.h \
	phidget21/cphidgetservo.h \
	phidget21/cphidgetspatial.h \
	phidget21/cphidgetstepper.h \
	phidget21/cphidgettemperaturesensor.h \
	phidget21/cphidgettextlcd.h \
	phidget21/cphidgettextled.h \
	phidget21/cphidgetweightsensor.h \
	phidget21/cphidgetmanager.h \
	phidget21/cphidgetdictionary.h \
	phidget21/cphidgetgeneric.h \
	phidget21/cphidgetsbc.h \
	phidget21/clog.h \
	phidget21/csocket.h \
	phidget21/cthread.h \
	phidget21/cphidgetconstants.h \
	phidget21/cphidgetlist.h

if ZEROCONF
phidgetwebservice21_SOURCES += zeroconf.h
else
AM_CFLAGS += -DNO_ZEROCONF
endif

if INTERNAL_UNICONV
AM_CFLAGS += -DUSE_INTERNAL_UNICONV
endif
	
if ZEROCONF_AVAHI
phidgetwebservice21_SOURCES += zeroconf_avahi.c \
	phidget21/linux/avahi-client/client.h \
	phidget21/linux/avahi-client/lookup.h \
	phidget21/linux/avahi-client/publish.h \
	phidget21/linux/avahi-common/address.h \
	phidget21/linux/avahi-common/alternative.h \
	phidget21/linux/avahi-common/cdecl.h \
	phidget21/linux/avahi-common/defs.h \
	phidget21/linux/avahi-common/domain.h \
	phidget21/linux/avahi-common/error.h \
	phidget21/linux/avahi-common/gccmacro.h \
	phidget21/linux/avahi-common/llist.h \
	phidget21/linux/avahi-common/malloc.h \
	phidget21/linux/avahi-common/rlist.h \
	phidget21/linux/avahi-common/simple-watch.h \
	phidget21/linux/avahi-common/strlst.h \
	phidget21/linux/avahi-common/thread-watch.h \
	phidget21/linux/avahi-common/timeval.h \
	phidget21/linux/avahi-common/watch.h
endif

if ZEROCONF_BONJOUR
phidgetwebservice21_SOURCES += zeroconf.c \
	phidget21/include/dns_sd.h
endif

if DEBUG
AM_CFLAGS += -D_DEBUG -DDEBUG
endif

EXTRA_DIST = phidget21/version.sh
	
deb:
	@echo "Making Debian Packages"
	@cp debian/changelog.template debian/changelog
	@sed -i s/"VERSION"/"$(PACKAGE_VERSION)"/ debian/changelog
	@sed -i s/"DATE"/"$(shell date +"%a, %d %b %Y %T %z")"/ debian/changelog
	@if [ -z "$(DPKG_ARCH)" ]; then dpkg-buildpackage -I.svn -us -uc; else dpkg-buildpackage -I.svn -us -uc -a$(DPKG_ARCH); fi
