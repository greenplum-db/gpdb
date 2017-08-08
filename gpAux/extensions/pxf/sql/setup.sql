------------------------------------------------------------------
-- PXF Extension Creation
------------------------------------------------------------------

CREATE EXTENSION pxf;

CREATE EXTERNAL TABLE pxf_read_test (a TEXT, b TEXT, c TEXT)
LOCATION ('pxf://default/tmp/dummy1'
'?FRAGMENTER=org.apache.hawq.pxf.api.examples.DemoFragmenter'
'&ACCESSOR=org.apache.hawq.pxf.api.examples.DemoAccessor'
'&RESOLVER=org.apache.hawq.pxf.api.examples.DemoTextResolver')
FORMAT 'TEXT' (DELIMITER ',');
