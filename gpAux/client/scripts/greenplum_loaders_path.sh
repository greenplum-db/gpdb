if [ `uname -s` = "HP-UX" ]; then
    set +o nounset
fi

GPHOME_LOADERS=`pwd`
PATH=${GPHOME_LOADERS}/bin:${GPHOME_LOADERS}/ext/python/bin:${PATH}
PYTHONPATH=${GPHOME_LOADERS}/bin/ext:${PYTHONPATH}

export GPHOME_LOADERS
export PATH
export PYTHONPATH

# Mac OSX uses a different library path variable
if [ xDarwin = x`uname -s` ]; then
  DYLD_LIBRARY_PATH=${GPHOME_LOADERS}/lib:${GPHOME_LOADERS}/ext/python/lib:${DYLD_LIBRARY_PATH}
  export DYLD_LIBRARY_PATH
else
  LD_LIBRARY_PATH=${GPHOME_LOADERS}/lib:${GPHOME_LOADERS}/ext/python/lib:${LD_LIBRARY_PATH}
  export LD_LIBRARY_PATH
fi

# AIX uses yet another library path variable
# Also, Python on AIX requires special copies of some libraries.  Hence, lib/pware.
if [ xAIX = x`uname -s` ]; then
  LIBPATH=${GPHOME_LOADERS}/lib/pware:${GPHOME_LOADERS}/lib:${GPHOME_LOADERS}/ext/python/lib:/usr/lib/threads:${LIBPATH}
  export LIBPATH
  GP_LIBPATH_FOR_PYTHON=${GPHOME_LOADERS}/lib/pware
  export GP_LIBPATH_FOR_PYTHON
fi

if [ "$1" != "-q" ]; then
  type python >/dev/null 2>&1
  if [ $? -ne 0 ]; then
    echo "Warning: Python not found.  Python-2.5.1 or better is required to run gpload."
  fi
fi
