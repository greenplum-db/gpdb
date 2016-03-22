CREATE FUNCTION pljava_call_handler()  RETURNS language_handler AS 'pljava' LANGUAGE C;
CREATE FUNCTION pljavau_call_handler() RETURNS language_handler AS 'pljava' LANGUAGE C;
CREATE TRUSTED LANGUAGE java HANDLER pljava_call_handler;
CREATE LANGUAGE javaU HANDLER pljavau_call_handler;
