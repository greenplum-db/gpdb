if [ `uname -s` = "HP-UX" ]; then
    set +o nounset
fi

GPHOME_CLIENTS=`pwd`
PATH=${GPHOME_CLIENTS}/bin:${PATH}

export GPHOME_CLIENTS
export PATH

# Mac OSX uses a different library path variable
if [ xDarwin = x`uname -s` ]; then
  DYLD_LIBRARY_PATH=${GPHOME_CLIENTS}/lib:${DYLD_LIBRARY_PATH}
  export DYLD_LIBRARY_PATH
else
    if [ -f /etc/SuSE-release ] && [ -d /lib64 ]; then
        LD_LIBRARY_PATH=${GPHOME_CLIENTS}/lib:/lib64:${LD_LIBRARY_PATH}
    else
        LD_LIBRARY_PATH=${GPHOME_CLIENTS}/lib:${LD_LIBRARY_PATH}
    fi
  export LD_LIBRARY_PATH
fi

# AIX uses yet another library path variable
if [ xAIX = x`uname -s` ]; then
  LIBPATH=${GPHOME_CLIENTS}/lib:${LIBPATH}
  export LIBPATH
fi
