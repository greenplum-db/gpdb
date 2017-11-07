------------------------------------------------------------------
-- PXF read test
------------------------------------------------------------------

SELECT * from pxf_read_test order by a;
SELECT * from pxf_readcustom_test order by a;

------------------------------------------------------------------
-- PXF write test
------------------------------------------------------------------
INSERT INTO pxf_write_test SELECT * from origin;
