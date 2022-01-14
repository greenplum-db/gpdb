---
title: Python Data Science Module Package 
---

Greenplum Database provides a collection of data science-related Python modules that can be used with the Greenplum Database PL/Python language. You can download these modules in `.gppkg` format from [VMware Tanzu Network](https://network.pivotal.io/products/pivotal-gpdb).

This section contains the following information:

-   [Python Data Science Modules](#topic_pydatascimod)
-   [Installing the Python Data Science Module Package](#topic_instpdsm)
-   [Uninstalling the Python Data Science Module Package](#topic_removepdsm)

For information about the Greenplum Database PL/Python Language, see [Greenplum PL/Python Language Extension](../analytics/pl_python.html).

**Parent topic:**[Installing Optional Extensions \(Tanzu Greenplum\)](data_sci_pkgs.html)

## <a id="topic_pydatascimod"></a>Python Data Science Modules 

Modules provided in the Python Data Science package are listed below.

Packages required for Deep Learning features of MADlib are now included. Note that it is not supported for RHEL 6.

|Module Name|Description/Used For|
|-----------|--------------------|
|atomicwrites|Atomic file writes|
|attrs|Declarative approach for defining class attributes|
|Autograd|Gradient-based optimization|
|backports.functools-lru-cache|Backports `functools.lru_cache` from Python 3.3|
|Beautiful Soup|Navigating HTML and XML|
|Blis|Blis linear algebra routines|
|Boto|Amazon Web Services library|
|Boto3|The AWS SDK|
|botocore|Low-level, data-driven core of boto3|
|Bottleneck|Fast NumPy array functions|
|Bz2file|Read and write bzip2-compressed files|
|Certifi|Provides Mozilla CA bundle|
|Chardet|Universal encoding detector for Python 2 and 3|
|ConfigParser|Updated `configparser` module|
|contextlib2|Backports and enhancements for the `contextlib` module|
|Cycler|Composable style cycles|
|cymem|Manage calls to calloc/free through Cython|
|Docutils|Python documentation utilities|
|enum34|Backport of Python 3.4 Enum|
|Funcsigs|Python function signatures from PEP362|
|functools32|Backport of the `functools` module from Python 3.2.3|
|funcy|Functional tools focused on practicality|
|future|Compatibility layer between Python 2 and Python 3|
|futures|Backport of the `concurrent.futures` package from Python 3|
|Gensim|Topic modeling and document indexing|
|GluonTS \(Python 3 only\)|Probabilistic time series modeling|
|h5py|Read and write HDF5 files|
|idna|Internationalized Domain Names in Applications \(IDNA\)|
|importlib-metadata|Read metadata from Python packages|
|Jinja2|Stand-alone template engine|
|JMESPath|JSON Matching Expressions|
|Joblib|Python functions as pipeline jobs|
|jsonschema|JSON Schema validation|
|Keras \(RHEL/CentOS 7 only\)|Deep learning|
|Keras Applications|Reference implementations of popular deep learning models|
|Keras Preprocessing|Easy data preprocessing and data augmentation for deep learning models|
|Kiwi|A fast implementation of the Cassowary constraint solver|
|Lifelines|Survival analysis|
|lxml|XML and HTML processing|
|MarkupSafe|Safely add untrusted strings to HTML/XML markup|
|Matplotlib|Python plotting package|
|mock|Rolling backport of `unittest.mock`|
|more-itertools|More routines for operating on iterables, beyond itertools|
|MurmurHash|Cython bindings for MurmurHash|
|NLTK|Natural language toolkit|
|NumExpr|Fast numerical expression evaluator for NumPy|
|NumPy|Scientific computing|
|packaging|Core utilities for Python packages|
|Pandas|Data analysis|
|pathlib, pathlib2|Object-oriented filesystem paths|
|patsy|Package for describing statistical models and for building design matrices|
|Pattern-en|Part-of-speech tagging|
|pip|Tool for installing Python packages|
|plac|Command line arguments parser|
|pluggy|Plugin and hook calling mechanisms|
|preshed|Cython hash table that trusts the keys are pre-hashed|
|protobuf|Protocol buffers|
|py|Cross-python path, ini-parsing, io, code, log facilities|
|pyLDAvis|Interactive topic model visualization|
|PyMC3|Statistical modeling and probabilistic machine learning|
|pyparsing|Python parsing|
|pytest|Testing framework|
|python-dateutil|Extensions to the standard Python datetime module|
|pytz|World timezone definitions, modern and historical|
|PyYAML|YAML parser and emitter|
|requests|HTTP library|
|s3transfer|Amazon S3 transfer manager|
|scandir|Directory iteration function|
|scikit-learn|Machine learning data mining and analysis|
|SciPy|Scientific computing|
|setuptools|Download, build, install, upgrade, and uninstall Python packages|
|six|Python 2 and 3 compatibility library|
|smart-open|Utilities for streaming large files \(S3, HDFS, gzip, bz2, and so forth\)|
|spaCy|Large scale natural language processing|
|srsly|Modern high-performance serialization utilities for Python|
|StatsModels|Statistical modeling|
|subprocess32|Backport of the subprocess module from Python 3|
|Tensorflow \(RHEL/CentOS 7 only\)|Numerical computation using data flow graphs|
|Theano|Optimizing compiler for evaluating mathematical expressions on CPUs and GPUs|
|thinc|Practical Machine Learning for NLP|
|tqdm|Fast, extensible progress meter|
|urllib3|HTTP library with thread-safe connection pooling, file post, and more|
|wasabi|Lightweight console printing and formatting toolkit|
|wcwidth|Measures number of Terminal column cells of wide-character codes|
|Werkzeug|Comprehensive WSGI web application library|
|wheel|A built-package format for Python|
|XGBoost|Gradient boosting, classifying, ranking|
|zipp|Backport of pathlib-compatible object wrapper for zip files|

## <a id="topic_instpdsm"></a>Installing the Python Data Science Module Package 

Before you install the Python Data Science Module package, make sure that your Greenplum Database is running, you have sourced `greenplum_path.sh`, and that the `$MASTER_DATA_DIRECTORY` and `$GPHOME` environment variables are set.

**Note:** The `PyMC3` module depends on `Tk`. If you want to use `PyMC3`, you must install the `tk` OS package on every node in your cluster. For example:

```
$ sudo yum install tk

```

1.  Locate the Python Data Science module package that you built or downloaded.

    The file name format of the package is `DataSciencePython-<version>-relhel<N>-x86_64.gppkg`.

2.  Copy the package to the Greenplum Database coordinator host.
3.  Follow the instructions in [Verifying the Greenplum Database Software Download](../install_guide/verify_sw.html) to verify the integrity of the *Greenplum Procedural Languages Python Data Science Package* software.
4.  Use the `gppkg` command to install the package. For example:

    ```
    $ gppkg -i DataSciencePython-<version>-relhel<N>_x86_64.gppkg
    ```

    `gppkg` installs the Python Data Science modules on all nodes in your Greenplum Database cluster. The command also updates the `PYTHONPATH`, `PATH`, and `LD_LIBRARY_PATH` environment variables in your `greenplum_path.sh` file.

5.  Restart Greenplum Database. You must re-source `greenplum_path.sh` before restarting your Greenplum cluster:

    ```
    $ source /usr/local/greenplum-db/greenplum_path.sh
    $ gpstop -r
    ```


The Greenplum Database Python Data Science Modules are installed in the following directory:

```
$GPHOME/ext/DataSciencePython/lib/python2.7/site-packages/
```

## <a id="topic_removepdsm"></a>Uninstalling the Python Data Science Module Package 

Use the `gppkg` utility to uninstall the Python Data Science Module package. You must include the version number in the package name you provide to `gppkg`.

To determine your Python Data Science Module package version number and remove this package:

```
$ gppkg -q --all | grep DataSciencePython
DataSciencePython-<version>
$ gppkg -r DataSciencePython-<version>
```

The command removes the Python Data Science modules from your Greenplum Database cluster. It also updates the `PYTHONPATH`, `PATH`, and `LD_LIBRARY_PATH` environment variables in your `greenplum_path.sh` file to their pre-installation values.

Re-source `greenplum_path.sh` and restart Greenplum Database after you remove the Python Data Science Module package:

```
$ . /usr/local/greenplum-db/greenplum_path.sh
$ gpstop -r 
```

**Note:** When you uninstall the Python Data Science Module package from your Greenplum Database cluster, any UDFs that you have created that import Python modules installed with this package will return an error.

