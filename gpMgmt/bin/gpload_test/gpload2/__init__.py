import sys
sys.dont_write_bytecode = True
# prevent python from generating pyc files

# pytest will generate TEST_local_base.pyc file
# this file will cause an error if run pytest a second time