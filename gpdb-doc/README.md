# Greenplum Database End-User Documentation

The latest check-ins to this repo are automatically built and published to [http://docs-greenplum-db-staging.cfapps.io](http://docs-greenplum-db-staging.cfapps.io).

Greenplum Database provides the full source for end-user documentation in DITA XML format. You can build the DITA XML into HTML output using [Bookbinder](https://github.com/cloudfoundry-incubator/bookbinder) and the DITA Open Toolkit (DITA-OT).  

Bookbinder is a Ruby gem that binds together a unified documentation web application from markdown, html, and/or DITA source material. The source material for bookbinder must be stored either in local directories or in GitHub repositories. Bookbinder runs [middleman](http://middlemanapp.com/) to produce a Rackup app that can be deployed locally or as a Web application.

This document contains instructions for building the local Greenplum Database documentation. It contains the sections:

* [Bookbinder Usage](#usage)
* [Prerequisites](#prereq)
* [Building the Documentation](#building)
* [Publishing the Documentation](#publishing)
* [Getting More Information](#moreinfo)

<a name="usage"></a>
## Bookbinder Usage

Bookbinder is meant to be used from within a project called a **book**. The book includes a configuration file that describes which documentation repositories to use as source materials. Bookbinder provides a set of scripts to aggregate those repositories and publish them to various locations in your final web application.

For Greenplum Database, a preconfigured **book** is provided in the `/gpdb-doc/book` directory of this repo.  You can use this configuration to build HTML for Greenplum Database on your local system.

<a name="prereq"></a>
## Prerequisites

* Ruby version 2.3.0 or higher.
* Ruby [bundler](http://bundler.io/) installed for gem package management.
* Java 1.7 or higher
* Ant
* DITA Open Toolkit (DITA-OT) 1.7.5 Full-easy-install package ([.zip](https://sourceforge.net/projects/dita-ot/files/DITA-OT%20Stable%20Release/DITA%20Open%20Toolkit%201.7/DITA-OT1.7.5_full_easy_install_bin.zip/download) or [.tar.gz](https://sourceforge.net/projects/dita-ot/files/DITA-OT%20Stable%20Release/DITA%20Open%20Toolkit%201.7/DITA-OT1.7.5_full_easy_install_bin.tar.gz/download)). Uncompress the DITA-OT to a local directory.
* An environment variable, `PATH_TO_DITA_OT_LIBRARY`, that points to your local DITA-OT installation. For example, if you installed the DITA_OT to `/Users/gpdb-user/DITA-OT1.7.5`:

    ``` bash
    $ export PATH_TO_DITA_OT_LIBRARY=/Users/gpdb-user/DITA-OT1.7.5
    ```

<a name="building"></a>
## Building the Documentation

1. Change to the `/gpdb-doc/book` directory of this repo.

2. Install bookbinder and its dependent gems. Make sure you are in the `book` directory and enter:

    ``` bash
    $ bundle install
    ```

3. The installed `config.yml` file configures the Greenplum Database book for building from your local source files.  Build the output HTML files by executing the command:

    ``` bash
    $ bundle exec bookbinder bind local
    ```

   Bookbinder converts the XML source into HTML, and puts the final output in the `final_app` directory.
  
5. The `final_app` directory stages the HTML into a web application that you can view using the rack gem. To view the documentation build:

    ``` bash
    $ cd final_app
    $ bundle install
    $ rackup
    ```

   Your local documentation is now available for viewing at[http://localhost:9292](http://localhost:9292)

<a name="publishing"></a>
## Publishing the Documentation
Because the `final_app` directory contains the full output of the HTML conversion process, you can easily publish this directory as a hosted Web application. The instructions that follow describe how to push your local build of the Greenplum Database user documentation either to a locally-installed instance of [Pivotal Cloud Foundry](http://cloudfoundry.org/index.html) or to an application instance on [Pivotal Web Services](https://run.pivotal.io/).  

1. Make sure that you have created an account on your local Cloud Foundry instance or on Pivotal Web Services, and that you have installed the latest [cf command line interface](http://docs.cloudfoundry.org/devguide/installcf/). For example:

    ``` bash
    $ cf --version
    cf version 6.10.0-b78bf10-2015-02-11T22:25:45+00:00
    ```

2. Log into Cloud Foundry or Pivotal Web Services, specifying the organization and space in which you will push the application. For example:

    ``` bash
    $ cf login -u me@mycompany.com -o myorg -s gpdb-space
    ```

3. Change to the `final_app` directory where you built the Greenplum Database documentation:

    ``` bash
    $ cd /github/gpdb/gpdb-doc/book/final_app
    ```
        
4. Use the `cf` command to push the current directory as a new application to a specified host and domain. For example:

    ``` bash
    $ cf push Greenplum Database-docs-build -p . -n my-gpdb-docs -d example.com
    ```

  The `cf` tool uploads and starts the application, using a default Web server. In the above example, the pushed application becomes available at [http://my-gpdb-docs.example.com](http://my-gpdb-docs.example.com).

<a name="moreinfo"></a>  
## Getting More Information

Bookbinder provides additional functionality to construct books from multiple Github repos, to perform variable substitution, and also to automatically build documentation in a continuous integration pipeline.  For more information, see [https://github.com/cloudfoundry-incubator/bookbinder](https://github.com/cloudfoundry-incubator/bookbinder).

The latest check-ins to this repo are automatically built and published to [http://docs-greenplum-db-staging.cfapps.io](http://docs-greenplum-db-staging.cfapps.io).

For more information about publishing applications to Cloud Foundry, see [Deploying an Application](http://docs.pivotal.io/pivotalcf/devguide/deploy-apps/deploy-app.html).
