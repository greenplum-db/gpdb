---
title: Data Science Package for Python 
---

Greenplum Database provides a collection of data science-related Python modules that can be used with the Greenplum Database PL/Python language. You can download these modules in `.gppkg` format from [VMware Tanzu Network](https://network.pivotal.io/products/pivotal-gpdb). Separate modules are provided for Python 2.7 and Python 3.9 development.

This section contains the following information:

-   [Data Science Package for Python 2.7 Modules](#topic_pydatascimod)
-   [Data Science Package for Python 3.9 Modules](#topic_pydatascimod3)
-   [Installing a Data Science Package for Python](#topic_instpdsm)
-   [Uninstalling a Data Science Package for Python](#topic_removepdsm)

For information about the Greenplum Database PL/Python Language, see [Greenplum PL/Python Language Extension](../analytics/pl_python.html).

**Parent topic:** [Installing Optional Extensions \(VMware Greenplum\)](data_sci_pkgs.html)

## <a id="topic_pydatascimod"></a>Data Science Package for Python 2.7 Modules 

The following table lists the modules that are provided in the Data Science Package for Python 2.7.

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
|Keras |Deep learning|
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
| regex | Alternative regular expression module, to replace re |
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
|Tensorflow |Numerical computation using data flow graphs|
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

## <a id="topic_pydatascimod3"></a>Data Science Package for Python 3.9 Modules 

The following table lists the modules that are provided in the Data Science Package for Python 3.9.

|Module Name|Description/Used For|
|-----------|--------------------|
| absl-py | Abseil Python Common Libraries |
| arviz | Exploratory analysis of Bayesian models |
| astor | Read/rewrite/write Python ASTs |
| astunparse | An AST unparser for Python |
| autograd | Efficiently computes derivatives of numpy code |
| autograd-gamma | autograd compatible approximations to the derivatives of the Gamma-family of functions |
| backports.csv | Backport of Python 3 csv module |
| beautifulsoup4 | Screen-scraping library |
| blis | The Blis BLAS-like linear algebra library, as a self-contained C-extension |
| cachetools | Extensible memoizing collections and decorators |
| catalogue | Super lightweight function registries for your library |
| certifi | Python package for providing Mozilla's CA Bundle |
| cffi | Foreign Function Interface for Python calling C code |
| cftime | Time-handling functionality from netcdf4-python |
| charset-normalizer | The Real First Universal Charset Detector. Open, modern and actively maintained alternative to Chardet. |
| cheroot | Highly-optimized, pure-python HTTP server |
| CherryPy | Object-Oriented HTTP framework |
| click | Composable command line interface toolkit |
| convertdate | Converts between Gregorian dates and other calendar systems |
| cryptography | A set of functions useful in cryptography and linear algebra |
| cycler | Composable style cycles |
| cymem | Manage calls to calloc/free through Cython |
| Cython | The Cython compiler for writing C extensions for the Python language |
| deprecat | Python @deprecat decorator to deprecate old python classes, functions or methods |
| dill | serialize all of python |
| fastprogress | A nested progress with plotting options for fastai |
| feedparser | Universal feed parser, handles RSS 0.9x, RSS 1.0, RSS 2.0, CDF, Atom 0.3, and Atom 1.0 feeds |
| filelock | A platform independent file lock |
| flatbuffers | The FlatBuffers serialization format for Python |
| fonttools | Tools to manipulate font files |
| formulaic | An implementation of Wilkinson formulas |
| funcy | A fancy and practical functional tools |
| future | Clean single-source support for Python 3 and 2 |
| gast | Python AST that abstracts the underlying Python version |
| gensim | Python framework for fast Vector Space Modelling |
| gluonts | GluonTS is a Python toolkit for probabilistic time series modeling, built around MXNet |
| google-auth | Google Authentication Library |
| google-auth-oauthlib | Google Authentication Library |
| google-pasta | pasta is an AST-based Python refactoring library |
| graphviz | Simple Python interface for Graphviz |
| greenlet | Lightweight in-process concurrent programming |
| grpcio | HTTP/2-based RPC framework |
| h5py | Read and write HDF5 files from Python |
| hijri-converter | Accurate Hijri-Gregorian dates converter based on the Umm al-Qura calendar |
| holidays | Generate and work with holidays in Python |
| idna | Internationalized Domain Names in Applications (IDNA) |
| importlib-metadata | Read metadata from Python packages |
| interface-meta | Provides a convenient way to expose an extensible API with enforced method signatures and consistent documentation |
| jaraco.classes | Utility functions for Python class constructs |
| jaraco.collections | Collection objects similar to those in stdlib by jaraco |
| jaraco.context | Context managers by jaraco |
| jaraco.functools | Functools like those found in stdlib |
| jaraco.text | Module for text manipulation |
| Jinja2 | A very fast and expressive template engine |
| joblib | Lightweight pipelining with Python functions |
| keras | Deep learning for humans |
| Keras-Preprocessing | Easy data preprocessing and data augmentation for deep learning models |
| kiwisolver | A fast implementation of the Cassowary constraint solver |
| korean-lunar-calendar | Korean Lunar Calendar |
| langcodes | Tools for labeling human languages with IETF language tags |
| libclang | Clang Python Bindings, mirrored from the official LLVM repo |
| lifelines | Survival analysis in Python, including Kaplan Meier, Nelson Aalen and regression |
| llvmlite | lightweight wrapper around basic LLVM functionality |
| lxml | Powerful and Pythonic XML processing library combining libxml2/libxslt with the ElementTree API |
| Markdown | Python implementation of Markdown |
| MarkupSafe | Safely add untrusted strings to HTML/XML markup |
| matplotlib | Python plotting package |
| more-itertools | More routines for operating on iterables, beyond itertools |
| murmurhash | Cython bindings for MurmurHash |
| mxnet | An ultra-scalable deep learning framework |
| mysqlclient | Python interface to MySQL |
| netCDF4 | Provides an object-oriented python interface to the netCDF version 4 library |
| nltk | Natural language toolkit |
| numba | Compiling Python code using LLVM |
| numexpr | Fast numerical expression evaluator for NumPy |
| numpy | Scientific computing |
| oauthlib | A generic, spec-compliant, thorough implementation of the OAuth request-signing logic |
| opt-einsum | Optimizing numpys einsum function |
| packaging | Core utilities for Python packages |
| pandas | Data analysis |
| pathy | pathlib.Path subclasses for local and cloud bucket storage |
| patsy | Package for describing statistical models and for building design matrices |
| Pattern | Web mining module for Python |
| pdfminer.six | PDF parser and analyzer |
| Pillow | Python Imaging Library |
| pmdarima | Python's forecast::auto.arima equivalent |
| portend | TCP port monitoring and discovery |
| preshed | Cython hash table that trusts the keys are pre-hashed |
| prophet | Automatic Forecasting Procedure |
| protobuf | Protocol buffers |
| psycopg2 | PostgreSQL database adapter for Python |
| psycopg2-binary | psycopg2 - Python-PostgreSQL Database Adapter |
| pyasn1 | ASN.1 types and codecs |
| pyasn1-modules | pyasn1-modules |
| pycparser | C parser in Python |
| pydantic | Data validation and settings management using python type hints |
| pyLDAvis | Interactive topic model visualization |
| pymc3 | Statistical modeling and probabilistic machine learning |
| PyMeeus | Python implementation of Jean Meeus astronomical routines |
| pyparsing | Python parsing |
| python-dateutil | Extensions to the standard Python datetime module |
| python-docx | Create and update Microsoft Word .docx files |
| PyTorch | Tensors and Dynamic neural networks in Python with strong GPU acceleration |
| pytz | World timezone definitions, modern and historical |
| regex | Alternative regular expression module, to replace re |
| requests | HTTP library |
| requests-oauthlib | OAuthlib authentication support for Requests |
| rsa | OAuthlib authentication support for Requests |
| scikit-learn | Machine learning data mining and analysis |
| scipy | Scientific computing |
| semver | Python helper for Semantic Versioning |
| sgmllib3k | Py3k port of sgmllib |
| six | Python 2 and 3 compatibility library |
| sklearn | A set of python modules for machine learning and data mining |
| smart-open | Utilities for streaming large files \(S3, HDFS, gzip, bz2, and so forth\) |
| soupsieve | A modern CSS selector implementation for Beautiful Soup |
| spacy | Large scale natural language processing |
| spacy-legacy | Legacy registered functions for spaCy backwards compatibility |
| spacy-loggers | Logging utilities for SpaCy |
| spectrum | Spectrum Analysis Tools |
| SQLAlchemy | Database Abstraction Library |
| srsly | Modern high-performance serialization utilities for Python |
| statsmodels | Statistical modeling |
| tempora | Objects and routines pertaining to date and time |
| tensorboard | TensorBoard lets you watch Tensors Flow |
| tensorboard-data-server | Fast data loading for TensorBoard |
| tensorboard-plugin-wit | What-If Tool TensorBoard plugin |
| tensorflow | Numerical computation using data flow graphs |
| tensorflow-estimator | What-If Tool TensorBoard plugin |
| tensorflow-io-gcs-filesystem | TensorFlow IO |
| termcolor | simple termcolor wrapper |
| Theano-PyMC | Theano-PyMC |
| thinc | Practical Machine Learning for NLP |
| threadpoolctl | Python helpers to limit the number of threads used in the threadpool-backed of common native libraries used for scientific computing and data science |
| toolz | List processing tools and functional utilities |
| tqdm | Fast, extensible progress meter |
| tslearn | A machine learning toolkit dedicated to time-series data |
| typer | Typer, build great CLIs. Easy to code. Based on Python type hints |
| typing_extensions | Backported and Experimental Type Hints for Python 3.7+ |
| urllib3 | HTTP library with thread-safe connection pooling, file post, and more |
| wasabi | Lightweight console printing and formatting toolkit |
| Werkzeug | Comprehensive WSGI web application library |
| wrapt | Module for decorators, wrappers and monkey patching |
| xarray | N-D labeled arrays and datasets in Python |
| xarray-einstats | Stats, linear algebra and einops for xarray |
| xgboost | Gradient boosting, classifying, ranking |
| xmltodict | Makes working with XML feel like you are working with JSON |
| zc.lockfile | Basic inter-process locks |
| zipp | Backport of pathlib-compatible object wrapper for zip files |
| tensorflow-gpu | An open source software library for high performance numerical computation |
| tensorflow | Numerical computation using data flow graphs |
| keras | An implementation of the Keras API that uses TensorFlow as a backend |

## <a id="topic_instpdsm"></a>Installing a Data Science Package for Python 

Before you install a Data Science Package for Python, make sure that your Greenplum Database is running, you have sourced `greenplum_path.sh`, and that the `$MASTER_DATA_DIRECTORY` and `$GPHOME` environment variables are set.

> **Note** The `PyMC3` module depends on `Tk`. If you want to use `PyMC3`, you must install the `tk` OS package on every node in your cluster. For example:

```
$ sudo yum install tk

```

1.  Locate the Data Science Package for Python that you built or downloaded.

    The file name format of the package is `DataSciencePython<pythonversion>-gp7-rhel<n>-x86_64.gppkg`.  For example, the Data Science Package for Python 2.7 for Redhat 8 file is `DataSciencePython2.7-2.0.4-gp7-rhel8_x86_64.gppkg`, and the Python 3.9 package is `DataSciencePython3.9-3.0.0-gp7-rhel8_x86_64.gppkg`.

2.  Copy the package to the Greenplum Database coordinator host.
3.  Follow the instructions in [Verifying the Greenplum Database Software Download](../install_guide/verify_sw.html) to verify the integrity of the *Greenplum Procedural Languages Python Data Science Package* software.
4.  Use the `gppkg` command to install the package. For example:

    ```
    $ gppkg -i DataSciencePython<pythonversion>-gp7-rhel<n>-x86_64.gppkg
    ```

    `gppkg` installs the Data Science Package for Python modules on all nodes in your Greenplum Database cluster. The command also updates the `PYTHONPATH`, `PATH`, and `LD_LIBRARY_PATH` environment variables in your `greenplum_path.sh` file.

5.  Restart Greenplum Database. You must re-source `greenplum_path.sh` before restarting your Greenplum cluster:

    ```
    $ source /usr/local/greenplum-db/greenplum_path.sh
    $ gpstop -r
    ```


The Data Science Package for Python modules are installed in the following directory for Python 2.7:

```
$GPHOME/ext/DataSciencePython/lib/python2.7/site-packages/
```

For Python 3.9 the directory is:

```
$GPHOME/ext/DataSciencePython/lib/python3.9/site-packages/
```

## <a id="topic_removepdsm"></a>Uninstalling a Data Science Package for Python 

Use the `gppkg` utility to uninstall a Data Science Package for Python. You must include the version number in the package name you provide to `gppkg`.

To determine your Data Science Package for Python version number and remove this package:

```
$ gppkg -q --all | grep DataSciencePython
DataSciencePython-<version>
$ gppkg -r DataSciencePython-<version>
```

The command removes the Data Science Package for Python modules from your Greenplum Database cluster. It also updates the `PYTHONPATH`, `PATH`, and `LD_LIBRARY_PATH` environment variables in your `greenplum_path.sh` file to their pre-installation values.

Re-source `greenplum_path.sh` and restart Greenplum Database after you remove the Python Data Science Module package:

```
$ . /usr/local/greenplum-db/greenplum_path.sh
$ gpstop -r 
```

> **Note** After you uninstall a Data Science Package for Python from your Greenplum Database cluster, any UDFs that you have created that import Python modules installed with this package will return an error.

