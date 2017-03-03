import sys

try:
    import yaml
except ImportError:
    sys.stderr.write("DDboost requires pyyaml."
                     "You can get it from http://pyyaml.org.\n")
    sys.exit(2)


def read_config_yaml(yaml_file):
    """ Reads in a yaml file. """

    try:
        cfgfile = open(yaml_file, 'r')
    except IOError as e:
        raise Exception("Unable to open file %s: %s" % (yaml_file, e))

    try:
        cfgyamldata = yaml.load(cfgfile.read())
    except yaml.YAMLError, exc:
        raise Exception("Error reading file %s: %s" % (yaml_file, exc))
    finally:
        cfgfile.close()

    if len(cfgyamldata) == 0:
        raise Exception("The load of the config file %s failed.\
            No configuration information to continue testing operation." % yaml_file)
    else:
        return cfgyamldata
