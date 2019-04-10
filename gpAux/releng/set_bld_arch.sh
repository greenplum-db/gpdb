# ----------------------------------------------------------------------
# Define ARCH values
# ----------------------------------------------------------------------

case "`uname -s`" in
    Linux)
    if [ -f /etc/redhat-release ]; then
        case "`cat /etc/redhat-release`" in
            *)
            BLD_ARCH_HOST="rhel`cat /etc/redhat-release | sed -e 's/CentOS Linux/RedHat/' -e 's/Red Hat Enterprise Linux/RedHat/' -e 's/WS//' -e 's/Server//' -e 's/Client//' | awk '{print $3}' | awk -F. '{print $1}'`_`uname -p | sed -e s/i686/x86_32/`"
            ;;
        esac
    fi
    if [ -f /etc/SuSE-release ]; then
        case "`cat /etc/SuSE-release`" in
            *)
                SLES_VERSION="$(grep VERSION /etc/SuSE-release | grep -o '[0-9][0-9]*')"
                SLES_PLATFORM="$(uname -p | sed -e s/i686/x86_32/)"
                BLD_ARCH_HOST="sles${SLES_VERSION}_${SLES_PLATFORM}"
            ;;
        esac
    fi
    if [ -f /etc/lsb-release ]; then
       UBUNTU_CODENAME=$(grep DISTRIB_RELEASE /etc/lsb-release | awk -F '=' '{print $2}' | tr -d '.')
       BLD_ARCH_HOST=ubuntu${UBUNTU_CODENAME}_amd64
    fi
    ;;

    AIX)
    BLD_ARCH_HOST="aix`uname -v`_`prtconf | grep 'Processor Type' | awk '{print $3}' | perl -p -i -e 's,PowerPC_POWER.,ppc,'`_`prtconf -k | perl -p -i -e 's,Kernel Type: (\d+)-bit,\1,'`"
    ;;

    *)
    BLD_ARCH_HOST="BLD_ARCH_unknown"
    ;;
esac

echo ${BLD_ARCH_HOST}
