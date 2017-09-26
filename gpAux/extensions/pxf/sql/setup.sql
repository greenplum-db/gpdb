------------------------------------------------------------------
-- PXF Extension Creation
------------------------------------------------------------------

CREATE EXTENSION pxf;

-- Also test dropping the extension. This may seem a bit random, but we
-- currently don't have any other tests for dropping an extension that
-- contains an external protocol.
DROP EXTENSION pxf;

CREATE EXTENSION pxf;

CREATE EXTERNAL TABLE pxf_read_test (a TEXT, b TEXT, c TEXT)
LOCATION ('pxf://tmp/dummy1'
'?FRAGMENTER=org.apache.hawq.pxf.api.examples.DemoFragmenter'
'&ACCESSOR=org.apache.hawq.pxf.api.examples.DemoAccessor'
'&RESOLVER=org.apache.hawq.pxf.api.examples.DemoTextResolver')
FORMAT 'TEXT' (DELIMITER ',');

CREATE EXTERNAL TABLE pxf_readcustom_test (a TEXT, b TEXT, c TEXT)
LOCATION ('pxf://tmp/dummy1'
'?FRAGMENTER=org.apache.hawq.pxf.api.examples.DemoFragmenter'
'&ACCESSOR=org.apache.hawq.pxf.api.examples.DemoAccessor'
'&RESOLVER=org.apache.hawq.pxf.api.examples.DemoResolver')
FORMAT 'CUSTOM' (formatter='pxfwritable_import');
