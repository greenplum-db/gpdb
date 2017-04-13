#ifndef (${VERSION})
VERSION=head-devel
#endif

VERSION_FOR_MSI:=$(shell echo $(VERSION) | perl -p -i -e 's,^([0-9.]+).*$$,\1,')
ifeq "$(VERSION_FOR_MSI)" ""
VERSION_FOR_MSI=0.0.0.0
endif

PACKAGE=greenplum-clients
NAME=${PACKAGE}-${VERSION}
MSI=../../${NAME}-WinXP-x86_32.msi
SRCDIR=../../${PACKAGE}-devel
DATESTAMP=$(shell datestamp.pl)

#Wix related
CANDLE="C:/Program Files (x86)/WiX Toolset v3.10/bin/candle.exe"
CANDLE_OPTS=-nologo
LIGHT="C:/Program Files (x86)/WiX Toolset v3.10/bin/light.exe"
LIGHT_OPTS=-nologo -sval
WXS_TEMPLATE=${PACKAGE}.wxs
WXS=${NAME}.wxs
WIXOBJ=${NAME}.wixobj

#MSI Signing
TIMESTAMP_SERVER=http://timestamp.verisign.com/scripts/timestamp.dll

#For scp copy of msi
SCP_USER=build
SCP_HOST=intranet
SCP_PATH=/var/www/html/internal-builds/greenplum-db/rc/${VERSION}
SCP_DEST=${SCP_USER}@${SCP_HOST}:${SCP_PATH}

default: msi sign

msi:
	@echo "Copying ${WXS_TEMPLATE} -> ${WXS}"
	echo "$(VERSION_FOR_MSI)" | grep "^main" > /dev/null ; \
	if [ $$? = 0 ]; then \
	perl -p -e 's,__VERSION_PLACEHOLDER__,99.99,' ${WXS_TEMPLATE} > ${WXS} ; \
	else \
	perl -p -e 's,__VERSION_PLACEHOLDER__,$(VERSION_FOR_MSI),' ${WXS_TEMPLATE} > ${WXS} ; \
	fi
	@echo Creating ${WIXOBJ}
	${CANDLE} ${CANDLE_OPTS} ${WXS} -out ${WIXOBJ} -dSRCDIR=${SRCDIR} -dVERSION=${VERSION}
	@echo Creating ${MSI}
	${LIGHT} ${LIGHT_OPTS} ${WIXOBJ} -out ${MSI} 

sign:
	@echo Signing ${MSI}
	@echo "`date` -- WARNING: signing disabled while we acquire a new certificate"
	#signtool sign /a /t ${TIMESTAMP_SERVER} ${MSI}

dist:
	@echo "Copying ${MSI} -> ${SCP_DEST}"
	scp -P 210 -q ${MSI} ${SCP_DEST}

clean:
	rm -f ${WIXOBJ} ${MSI}
