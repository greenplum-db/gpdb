---
title: Data Science Package for Python 
---

Greenplum Database provides a collection of data science-related Python modules that can be used with the Greenplum Database PL/Python language. You can download these modules in `.gppkg` format from [VMware Tanzu Network](https://network.pivotal.io/products/pivotal-gpdb).

This section contains the following information:

-   [Data Science Package for Python 3.9 Modules](#topic_pydatascimod3)
-   [Installing a Data Science Package for Python](#topic_instpdsm)
-   [Uninstalling a Data Science Package for Python](#topic_removepdsm)

For information about the Greenplum Database PL/Python Language, see [Greenplum PL/Python Language Extension](../analytics/pl_python.html).

**Parent topic:** [Installing Optional Extensions \(VMware Greenplum\)](data_sci_pkgs.html)

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
| datasets | HuggingFace community-driven open-source library of datasets |
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
| InstructorEmbedding | Text embedding tool |
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
| lime | Local Interpretable Model-Agnostic Explanations for machine learning classifiers |
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
| orjson | Fast, correct Python JSON library supporting dataclasses, datetimes, and numpy |
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
| rouge | Full Python ROUGE Score Implementation (not a wrapper) |
| rsa | OAuthlib authentication support for Requests |
| sacrebleu | Hassle-free computation of shareable, comparable, and reproducible BLEU, chrF, and TER scores |
| scikit-learn | Machine learning data mining and analysis |
| scipy | Scientific computing |
| semver | Python helper for Semantic Versioning |
| sentence_transformers | Multilingual Sentence, Paragraph, and Image Embeddings using BERT & Co. |
| sgmllib3k | Py3k port of sgmllib |
| shap | A unified approach to explain the output of any machine learning model |
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
| transformers | State-of-the-art Machine Learning for JAX, PyTorch and TensorFlow |
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
| tensorflow | Numerical computation using data flow graphs |
| keras | An implementation of the Keras API that uses TensorFlow as a backend |

## <a id="topic_instpdsm"></a>Installing a Data Science Package for Python 

Before you install a Data Science Package for Python, make sure that your Greenplum Database is running, you have sourced `greenplum_path.sh`, and that the `$COORDINATOR_DATA_DIRECTORY` and `$GPHOME` environment variables are set.

> **Note** The `PyMC3` module depends on `Tk`. If you want to use `PyMC3`, you must install the `tk` OS package on every node in your cluster. For example:

```
$ sudo yum install tk
```

1.  Locate the Data Science Package for Python that you built or downloaded.

    The file name format of the package is `DataSciencePython<pythonversion>-gp7-rhel<n>-x86_64.gppkg`.  For example: `DataSciencePython3.9-3.0.0-gp7-rhel8_x86_64.gppkg`.

2.  Copy the package to the Greenplum Database coordinator host.
3.  Follow the instructions in [Verifying the Greenplum Database Software Download](../install_guide/verify_sw.html) to verify the integrity of the *Greenplum Procedural Languages Python Data Science Package* software.
4.  Use the `gppkg` command to install the package. For example:

    ```
    $ gppkg install DataSciencePython3.9-1.2.0-gp7-el8_x86_64.gppkg
    ```

    `gppkg` installs the Data Science Package for Python modules on all nodes in your Greenplum Database cluster. The command also updates the `PYTHONPATH`, `PATH`, and `LD_LIBRARY_PATH` environment variables in your `greenplum_path.sh` file.

5.  Restart Greenplum Database. You must re-source `greenplum_path.sh` before restarting your Greenplum cluster:

    ```
    $ source /usr/local/greenplum-db/greenplum_path.sh
    $ gpstop -r
    ```


The Data Science Package for Python modules are installed in the following directory:

```
$GPHOME/ext/DataSciencePython/lib/python3.9/site-packages/
```

## <a id="topic_removepdsm"></a>Uninstalling a Data Science Package for Python 

Use the `gppkg` utility to uninstall a Data Science Package for Python. You must include the version number in the package name you provide to `gppkg`.

To determine your Data Science Package for Python version number and remove this package:

```
$ gppkg query | grep DataSciencePython
DataSciencePython-<version>
$ gppkg remove DataSciencePython-<version>
```

The command removes the Data Science Package for Python modules from your Greenplum Database cluster. It also updates the `PYTHONPATH`, `PATH`, and `LD_LIBRARY_PATH` environment variables in your `greenplum_path.sh` file to their pre-installation values.

Re-source `greenplum_path.sh` and restart Greenplum Database after you remove the Python Data Science Module package:

```
$ . /usr/local/greenplum-db/greenplum_path.sh
$ gpstop -r 
```

> **Note** After you uninstall a Data Science Package for Python from your Greenplum Database cluster, any UDFs that you have created that import Python modules installed with this package will return an error.

