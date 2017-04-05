# prevent included makefiles from stealing default target
all: default;

BLD_TOP=../../../..
include $(BLD_TOP)/Makefile.global
include $(BLD_TOP)/Makefile.thirdparty

VERSION=3.1.1.1-build-2

SHELL:=$(shell which bash)
WORK_DIR:=$(shell pwd)
INSTALLER_TEMPLATE=${WORK_DIR}/installer-header-template.txt
INSTALLER_HEADER=${WORK_DIR}/installer-header.txt
PRODUCT=Clients
PACKAGE=greenplum-clients
README=GPClientToolsUnix
INSTALLER_NAME=${PACKAGE}-${VERSION}
INSTALL_DIR=${WORK_DIR}/../../../..
INSTALL_SRC=${INSTALL_DIR}/${PACKAGE}
DATESTAMP=`date +%d%m%y`

INSTALL_FILESET=$(shell cd $(INSTALL_SRC) && ls)

TAR=tar

default:
	@if [ -z "${MPP_ARCH}" ] ; then \
	    echo "Error: MPP_ARCH is not defined." ; \
	    exit 1; \
	fi ; \

	@echo "Modifying the installer template"
	@sed -e 's,VERSION_PLACEHOLDER,${VERSION},' -e 's,__PRODUCT_PLACEHOLDER__,${PRODUCT},' -e 's,__PACKAGE_PLACEHOLDER__,${PACKAGE},' -e 's,__DOC_PLACEHOLDER__,${README},' ${INSTALLER_TEMPLATE} > ${INSTALLER_HEADER}

	@echo "Creating ${INSTALLER_NAME}-${MPP_ARCH}.tar"
	$(TAR) cf ${INSTALL_DIR}/${INSTALLER_NAME}-${MPP_ARCH}.tar -C ${INSTALL_SRC} ${INSTALL_FILESET}
	@echo "Compressing ${INSTALLER_NAME}-${MPP_ARCH}.tar"
	gzip -f ${INSTALL_DIR}/${INSTALLER_NAME}-${MPP_ARCH}.tar
	@echo "Creating the installer"
	cat ${INSTALLER_HEADER} \
	    ${INSTALL_DIR}/${INSTALLER_NAME}-${MPP_ARCH}.tar.gz \
	    > ${INSTALL_DIR}/${INSTALLER_NAME}-${MPP_ARCH}.bin
	chmod 755 ${INSTALL_DIR}/${INSTALLER_NAME}-${MPP_ARCH}.bin
	( cd ${INSTALL_DIR}; \
	  zip -q ${INSTALLER_NAME}-${MPP_ARCH}.zip ${INSTALLER_NAME}-${MPP_ARCH}.bin; \
	  rm ${INSTALLER_NAME}-${MPP_ARCH}.bin )
	@echo "Created ${INSTALL_DIR}/${INSTALLER_NAME}-${MPP_ARCH}.zip"

dist:
	if [ -z "${MPP_ARCH}" ] ; then \
	    echo "Error: MPP_ARCH is not defined." ; \
	    exit 1; \
	fi ; \
	ssh build "mkdir -p /var/www/html/releases/greenplum-db-client-tools/nightly/HEAD/${DATESTAMP}/"
	scp ${INSTALL_DIR}/${INSTALLER_NAME}-${MPP_ARCH}.zip build:/var/www/html/releases/greenplum-db-client-tools/nightly/HEAD/${DATESTAMP}/

clean:
	rm -f ${INSTALL_DIR}/${INSTALLER_NAME}*[gz,bin,zip]
