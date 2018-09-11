/* mock functions for pxfbridge.h */
void
gpbridge_cleanup(gphadoop_context* context)
{
	check_expected(context);
	mock();
}

void
gpbridge_import_start(gphadoop_context* context)
{
	check_expected(context);
	mock();
}

void
gpbridge_export_start(gphadoop_context *context)
{
	check_expected(context);
	mock();
}

int
gpbridge_read(gphadoop_context* context, char* databuf, int datalen)
{
	check_expected(context);
	check_expected(databuf);
	check_expected(datalen);
	return (int) mock();
}

int
gpbridge_write(gphadoop_context *context, char *databuf, int datalen)
{
	check_expected(context);
	check_expected(databuf);
	check_expected(datalen);
	return (int) mock();
}