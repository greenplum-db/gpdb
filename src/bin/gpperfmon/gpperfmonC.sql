-- This SQL file contains schema changes for gpperfmon needed to complete the creation of the schema

revoke all on database gpperfmon from public;

-- for web ui auth everyone needs connect permissions
grant connect on database gpperfmon to public;
