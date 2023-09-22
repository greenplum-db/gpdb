# pgml

The `pgml` module provides functions for using tens of thousands of pre-trained open source AI/machine learning models in VMware Greenplum.

ksdfasdf
adsfasdf


## <a id="topic_reg"></a>Installing and Registering the Module

The `pgml` module is installed when you install Greenplum Database. Before you can use any of the data types, functions, or operators defined in the module, you must register the `pgml` extension in each database in which you want to use the objects:

```
CREATE EXTENSION pgml;
```

Refer to [Installing Additional Supplied Modules](../../install_guide/install_modules.html) for more information.

## <a id="UDF_summary"></a>User-Defined Functions

The `pgml` extension provides the following user-defined functions for accessing AI/machine learning models in VMware Greenplum:

- `pgml.embed()` - Generates an embedding for the dataset
- `pgml.transform()`: Applies a pre-trained transformer to process data
- `pgml.load_dataset()`: Loads a dataset into tables in VMware Greenplum using the  `INSERT` SQL command

### <a id="pgml_embed"></a>pgml.embed()

#### Syntax

```
pgml.embed(
    transformer TEXT, 
    text TEXT,
    kwargs JSON
)
```

where: 

- `transformer` is the huggingface sentence-transformer name
- `text` is the input to embed 
- `kwargs` is a set of optional arguments passes as JSON key-value pairs 





### <a id="pgml_transform"></a>pgml.transform()

#### Syntax

```
pgml.transform(
    task TEXT or JSONB, 
    call JSONB,
    inputs TEXT[] or BYTEA[]
)
```

where: 

- `task` is the huggingface sentence-transformer name passed as either a simple text string or, for more comples task setup, a JSONB object
- `text` is a text string containing the input to embed 
- `kwargs` is a set of optional arguments passes as JSON key-value pairs 

## <a id="Examples"></a>Examples

The following example XYXYXYXYXYX:

```
# Download the dataset from the internet and create table for it

SELECT pgml.load_dataset('tweet_eval', 'sentiment'); 

# Generate an embedding for the text 

SELECT pgml.embed('distilbert-base-uncased', 'Star Wars christmas special is on Disney')::vector AS embedding; 
--------------------------------------------------------------------------------------------- 

SELECT text, pgml.embed('distilbert-base-uncased', text) 

FROM pgml.tweet_eval; 

--------------------------------------------------------------------------------------------- 

CREATE TABLE tweet_embeddings AS 

SELECT text, pgml.embed('distilbert-base-uncased', text) AS embedding 

FROM pgml.tweet_eval; 

# Download and run pre-trained models

SELECT pgml.transform( 
    'translation_en_to_fr', 
    inputs => ARRAY[ 
        'Welcome to the future!', 
        'Where have you been all this time?' 
    ] 
) AS french; 
```