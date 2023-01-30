# Date/Time Types 

Greenplum supports the full set of SQL date and time types, shown in [Table 1](#datatype-datetime-table). The operations available on these data types are described in [Date/Time Functions and Operators](https://www.postgresql.org/docs/12/functions-datetime.html) in the PostgreSQL documentation. Dates are counted according to the Gregorian calendar, even in years before that calendar was introduced \(see [History of Units](https://www.postgresql.org/docs/12/datetime-units-history.html) in the PostgreSQL documentation for more information\).

|Name|Storage Size|Description|Low Value|High Value|Resolution|
|----|------------|-----------|---------|----------|----------|
|timestamp \[ \(p\) \] \[ without time zone \]|8 bytes|both date and time \(no time zone\)|4713 BC|294276 AD|1 microsecond / 14 digits|
|timestamp \[ \(p\) \] with time zone|8 bytes|both date and time, with time zone|4713 BC|294276 AD|1 microsecond / 14 digits|
|date|4 bytes|date \(no time of day\)|4713 BC|5874897 AD|1 day|
|time \[ \(p\) \] \[ without time zone \]|8 bytes|time of day \(no date\)|00:00:00|24:00:00|1 microsecond / 14 digits|
|time \[ \(p\) \] with time zone|12 bytes|times of day only, with time zone|00:00:00+1459|24:00:00-1459|1 microsecond / 14 digits|
|interval \[ fields \] \[ \(p\) \]|16 bytes|time interval|-178000000 years|178000000 years|1 microsecond / 14 digits|

> **Note** The SQL standard requires that writing just `timestamp` be equivalent to `timestamp without time zone`, and Greenplum honors that behavior. `timestamptz` is accepted as an abbreviation for `timestamp with time zone`; this is a PostgreSQL extension.

`time`, `timestamp`, and `interval` accept an optional precision value p which specifies the number of fractional digits retained in the seconds field. By default, there is no explicit bound on precision. The allowed range of p is from 0 to 6 for the `timestamp` and `interval` types.

> **Note** When `timestamp` values are stored as eight-byte integers \(currently the default\), microsecond precision is available over the full range of values. When `timestamp` values are stored as double precision floating-point numbers instead \(a deprecated compile-time option\), the effective limit of precision might be less than 6. `timestamp` values are stored as seconds before or after midnight 2000-01-01. When `timestamp` values are implemented using floating-point numbers, microsecond precision is achieved for dates within a few years of 2000-01-01, but the precision degrades for dates further away. Note that using floating-point datetimes allows a larger range of `timestamp` values to be represented than shown above: from 4713 BC up to 5874897 AD.

The same compile-time option also determines whether `time` and `interval` values are stored as floating-point numbers or eight-byte integers. In the floating-point case, large `interval` values degrade in precision as the size of the interval increases.

For the `time` types, the allowed range of p is from 0 to 6 when eight-byte integer storage is used, or from 0 to 10 when floating-point storage is used.

The `interval` type has an additional option, which is to restrict the set of stored fields by writing one of these phrases:

```

YEAR
MONTH
DAY
HOUR
MINUTE
SECOND
YEAR TO MONTH
DAY TO HOUR
DAY TO MINUTE
DAY TO SECOND
HOUR TO MINUTE
HOUR TO SECOND
MINUTE TO SECOND

```

Note that if both fields and p are specified, the fields must include `SECOND`, since the precision applies only to the seconds.

The type `time with time zone` is defined by the SQL standard, but the definition exhibits properties which lead to questionable usefulness. In most cases, a combination of `date`, `time`, `timestamp without time zone`, and `timestamp with time zone` should provide a complete range of date/time functionality required by any application.

The types `abstime` and `reltime` are lower precision types which are used internally. You are discouraged from using these types in applications; these internal types might disappear in a future release.

Greenplum Database 6 and later releases do not automatically cast text from the deprecated timestamp format `YYYYMMDDHH24MISS`. The format could not be parsed unambiguously in previous Greenplum Database releases.

For example, this command returns an error in Greenplum Database 6. In previous releases, a timestamp is returned.

```
# select to_timestamp('20190905140000');
```

In Greenplum Database 6, this command returns a timestamp.

```
# select to_timestamp('20190905140000','YYYYMMDDHH24MISS');
```

## <a id="datatype-datetime-input"></a>Date/Time Input 

Date and time input is accepted in almost any reasonable format, including ISO 8601, SQL-compatible, traditional POSTGRES, and others. For some formats, ordering of day, month, and year in date input is ambiguous and there is support for specifying the expected ordering of these fields. Set the [DateStyle](config_params/guc-list.html) parameter to `MDY` to select month-day-year interpretation, `DMY` to select day-month-year interpretation, or `YMD` to select year-month-day interpretation.

Greenplum is more flexible in handling date/time input than the SQL standard requires. See [Appendix B. Date/Time Support](https://www.postgresql.org/docs/12/datetime-appendix.html) in the PostgreSQL documentation for the exact parsing rules of date/time input and for the recognized text fields including months, days of the week, and time zones.

Remember that any date or time literal input needs to be enclosed in single quotes, like text strings. SQL requires the following syntax

```

<type> [ (<p>) ] '<value>'

```

where p is an optional precision specification giving the number of fractional digits in the seconds field. Precision can be specified for `time`, `timestamp`, and `interval` types. The allowed values are mentioned above. If no precision is specified in a constant specification, it defaults to the precision of the literal value.

## <a id="dates"></a>Dates 

[Table 2](#table_owm_dfr_qfb) shows some possible inputs for the `date` type.

|Example|Description|
|-------|-----------|
|1999-01-08|ISO 8601; January 8 in any mode \(recommended format\)|
|January 8, 1999|unambiguous in any datestyle input mode|
|1/8/1999|January 8 in `MDY` mode; August 1 in `DMY` mode|
|1/18/1999|January 18 in `MDY` mode; rejected in other modes|
|01/02/03|January 2, 2003 in `MDY` mode; February 1, 2003 in `DMY` mode; February 3, 2001 in `YMD` mode|
|1999-Jan-08|January 8 in any mode|
|Jan-08-1999|January 8 in any mode|
|08-Jan-1999|January 8 in any mode|
|99-Jan-08|January 8 in `YMD` mode, else error|
|08-Jan-99|January 8, except error in `YMD` mode|
|Jan-08-99|January 8, except error in `YMD` mode|
|19990108|ISO 8601; January 8, 1999 in any mode|
|990108|ISO 8601; January 8, 1999 in any mode|
|1999.008|year and day of year|
|J2451187|Julian date|
|January 8, 99 BC|year 99 BC|

## <a id="times"></a>Times 

The time-of-day types are `time [ (p) ] without time zone` and `time [ (p) ] with time zone`. `time` alone is equivalent to `time without time zone`.

Valid input for these types consists of a time of day followed by an optional time zone. \(See [Table 3](#datatype-datetime-time-table) and [Table 4](#datatype-timezone-table).\) If a time zone is specified in the input for `time without time zone`, it is silently ignored. You can also specify a date but it will be ignored, except when you use a time zone name that involves a daylight-savings rule, such as `America/New_York`. In this case specifying the date is required in order to determine whether standard or daylight-savings time applies. The appropriate time zone offset is recorded in the `time with time zone` value.

|Example|Description|
|-------|-----------|
|`04:05:06.789`|ISO 8601|
|`04:05:06`|ISO 8601|
|`04:05`|ISO 8601|
|`040506`|ISO 8601|
|`04:05 AM`|same as 04:05; AM does not affect value|
|`04:05 PM`|same as 16:05; input hour must be <= 12|
|`04:05:06.789-8`|ISO 8601|
|`04:05:06-08:00`|ISO 8601|
|`04:05-08:00`|ISO 8601|
|`040506-08`|ISO 8601|
|`04:05:06 PST`|time zone specified by abbreviation|
|`2003-04-12 04:05:06 America/New_York`|time zone specified by full name|

|Example|Description|
|-------|-----------|
|`PST`|Abbreviation \(for Pacific Standard Time\)|
|`America/New_York`|Full time zone name|
|`PST8PDT`|POSIX-style time zone specification|
|`-8:00`|ISO-8601 offset for PST|
|`-800`|ISO-8601 offset for PST|
|`-8`|ISO-8601 offset for PST|
|`zulu`|Military abbreviation for UTC|
|`z`|Short form of `zulu`|

Refer to [Time Zones](#datatype-timezones) for more information on how to specify time zones.

## <a id="tstamps"></a>Time Stamps 

Valid input for the time stamp types consists of the concatenation of a date and a time, followed by an optional time zone, followed by an optional `AD` or `BC`. \(Alternatively, `AD`/`BC` can appear before the time zone, but this is not the preferred ordering.\) Thus: `1999-01-08 04:05:06` and: `1999-01-08 04:05:06 -8:00` are valid values, which follow the ISO 8601 standard. In addition, the common format: `January 8 04:05:06 1999 PST` is supported.

The SQL standard differentiates `timestamp without time zone` and `timestamp with time zone` literals by the presence of a `+` or `-` symbol and time zone offset after the time. Hence, according to the standard, `TIMESTAMP '2004-10-19 10:23:54'` is a `timestamp without time zone`, while `TIMESTAMP '2004-10-19 10:23:54+02'` is a `timestamp with time zone`. Greenplum never examines the content of a literal string before determining its type, and therefore will treat both of the above as `timestamp without time zone`. To ensure that a literal is treated as `timestamp with time zone`, give it the correct explicit type: `TIMESTAMP WITH TIME ZONE '2004-10-19 10:23:54+02'` In a literal that has been determined to be `timestamp without time zone`, Greenplum will silently ignore any time zone indication. That is, the resulting value is derived from the date/time fields in the input value, and is not adjusted for time zone.

For `timestamp with time zone`, the internally stored value is always in UTC \(Universal Coordinated Time, traditionally known as Greenwich Mean Time, GMT\). An input value that has an explicit time zone specified is converted to UTC using the appropriate offset for that time zone. If no time zone is stated in the input string, then it is assumed to be in the time zone indicated by the system's `TimeZone` parameter, and is converted to UTC using the offset for the timezone zone.

When a `timestamp with time zone` value is output, it is always converted from UTC to the current timezone zone, and displayed as local time in that zone. To see the time in another time zone, either change timezone or use the `AT TIME ZONE` construct \(see [AT TIME ZONE](https://www.postgresql.org/docs/12/functions-datetime.html#FUNCTIONS-DATETIME-ZONECONVERT) in the PostgreSQL documentation\).

Conversions between `timestamp without time zone` and `timestamp with time zone` normally assume that the `timestamp without time zone` value should be taken or given as timezone local time. A different time zone can be specified for the conversion using `AT TIME ZONE`.

## <a id="specialvals"></a>Special Values 

Greenplum supports several special date/time input values for convenience, as shown in [Table 5](#datatype-datetime-special-table). The values `infinity` and `-infinity` are specially represented inside the system and will be displayed unchanged; but the others are simply notational shorthands that will be converted to ordinary date/time values when read. \(In particular, `now` and related strings are converted to a specific time value as soon as they are read.\) All of these values need to be enclosed in single quotes when used as constants in SQL commands.

|Input String|Valid Types|Description|
|------------|-----------|-----------|
|`epoch`|`date`, `timestamp`|1970-01-01 00:00:00+00 \(Unix system time zero\)|
|`infinity`|`date`, `timestamp`|later than all other time stamps|
|`-infinity`|`date`, `timestamp`|earlier than all other time stamps|
|`now`|`date`, `time`, `timestamp`|current transaction's start time|
|`today`|`date`, `timestamp`|midnight today|
|`tomorrow`|`date`, `timestamp`|midnight tomorrow|
|`yesterday`|`date`, `timestamp`|midnight yesterday|
|`allballs`|`time`|00:00:00.00 UTC|

The following SQL-compatible functions can also be used to obtain the current time value for the corresponding data type: `CURRENT_DATE`, `CURRENT_TIME`, `CURRENT_TIMESTAMP`, `LOCALTIME`, `LOCALTIMESTAMP`. The latter four accept an optional subsecond precision specification. \(See [Current Date/Time](https://www.postgresql.org/docs/12/functions-datetime.html#FUNCTIONS-DATETIME-CURRENT) in the PostgreSQL documentation.\) Note that these are SQL functions and are *not* recognized in data input strings.

## <a id="datatype-datetime-output"></a>Date/Time Output 

The output format of the date/time types can be set to one of the four styles ISO 8601, SQL \(Ingres\), traditional POSTGRES \(Unix `date` format\), or German. The default is the ISO format. \(The SQL standard requires the use of the ISO 8601 format. The name of the `SQL` output format is a historical accident.\) [Table 6](#datatype-datetime-output-table) shows examples of each output style. The output of the `date` and `time` types is generally only the date or time part in accordance with the given examples. However, the POSTGRES style outputs date-only values in ISO format.

|Style Specification|Description|Example|
|-------------------|-----------|-------|
|`ISO`|ISO 8601, SQL standard|`1997-12-17 07:37:16-08`|
|`SQL`|traditional style|`12/17/1997 07:37:16.00 PST`|
|`Postgres`|original style|`Wed Dec 17 07:37:16 1997 PST`|
|`German`|regional style|`17.12.1997 07:37:16.00 PST`|

> **Note** ISO 8601 specifies the use of uppercase letter `T` to separate the date and time. Greenplum accepts that format on input, but on output it uses a space rather than `T`, as shown above. This is for readability and for consistency with RFC 3339 as well as some other database systems.

In the SQL and POSTGRES styles, day appears before month if DMY field ordering has been specified, otherwise month appears before day. \(See [Table 2](#table_owm_dfr_qfb) for how this setting also affects interpretation of input values.\) [Table 7](#datatype-datetime-output2-table) shows examples.

|datestyle Setting|Input Ordering|Example Output|
|-----------------|--------------|--------------|
|`SQL, DMY`|day/month/year|`17/12/1997 15:37:16.00 CET`|
|`SQL, MDY`|month/day/year|`12/17/1997 07:37:16.00 PST`|
|`Postgres, DMY`|day/month/year|`Wed 17 Dec 07:37:16 1997 PST`|

The date/time style can be selected by the user using the `SET datestyle` command, the `DateStyle` parameter in the `postgresql.conf` configuration file, or the `PGDATESTYLE` environment variable on the server or client.

The formatting function `to_char` \(see [Data Type Formatting Functions](https://www.postgresql.org/docs/12/functions-formatting.html)\) is also available as a more flexible way to format date/time output.

## <a id="datatype-timezones"></a>Time Zones 

Time zones, and time-zone conventions, are influenced by political decisions, not just earth geometry. Time zones around the world became somewhat standardized during the 1900s, but continue to be prone to arbitrary changes, particularly with respect to daylight-savings rules. Greenplum uses the widely-used IANA \(Olson\) time zone database for information about historical time zone rules. For times in the future, the assumption is that the latest known rules for a given time zone will continue to be observed indefinitely far into the future.

Greenplum endeavors to be compatible with the SQL standard definitions for typical usage. However, the SQL standard has an odd mix of date and time types and capabilities. Two obvious problems are:

1.  Although the `date` type cannot have an associated time zone, the `time` type can. Time zones in the real world have little meaning unless associated with a date as well as a time, since the offset can vary through the year with daylight-saving time boundaries.
2.  The default time zone is specified as a constant numeric offset from UTC. It is therefore impossible to adapt to daylight-saving time when doing date/time arithmetic across DST boundaries.

To address these difficulties, we recommend using date/time types that contain both date and time when using time zones. We do *not* recommend using the type `time with time zone` \(though it is supported by Greenplum for legacy applications and for compliance with the SQL standard\). Greenplum assumes your local time zone for any type containing only date or time.

All timezone-aware dates and times are stored internally in UTC. They are converted to local time in the zone specified by the `TimeZone` configuration parameter before being displayed to the client.

Greenplum allows you to specify time zones in three different forms:

1.  A full time zone name, for example `America/New_York`. The recognized time zone names are listed in the `pg_timezone_names` view. Greenplum uses the widely-used IANA time zone data for this purpose, so the same time zone names are also recognized by other software.
2.  A time zone abbreviation, for example `PST`. Such a specification merely defines a particular offset from UTC, in contrast to full time zone names which can imply a set of daylight savings transition-date rules as well. The recognized abbreviations are listed in the `pg_timezone_abbrevs` view. You cannot set the configuration parameters [TimeZone](config_params/guc-list.html) or [log\_timezone](config_params/guc-list.html) to a time zone abbreviation, but you can use abbreviations in date/time input values and with the `AT TIME ZONE` operator.
3.  In addition to the timezone names and abbreviations, Greenplum will accept POSIX-style time zone specifications of the form STDoffset or STDoffsetDST, where STD is a zone abbreviation, offset is a numeric offset in hours west from UTC, and DST is an optional daylight-savings zone abbreviation, assumed to stand for one hour ahead of the given offset. For example, if `EST5EDT` were not already a recognized zone name, it would be accepted and would be functionally equivalent to United States East Coast time. In this syntax, a zone abbreviation can be a string of letters, or an arbitrary string surrounded by angle brackets \(`<>`\). When a daylight-savings zone abbreviation is present, it is assumed to be used according to the same daylight-savings transition rules used in the IANA time zone database's entry. In a standard Greenplum installation, is the same as `US/Eastern`, so that POSIX-style time zone specifications follow USA daylight-savings rules. If needed, you can adjust this behavior by replacing the file.

In short, this is the difference between abbreviations and full names: abbreviations represent a specific offset from UTC, whereas many of the full names imply a local daylight-savings time rule, and so have two possible UTC offsets. As an example, `2014-06-04 12:00 America/New_York` represents noon local time in New York, which for this particular date was Eastern Daylight Time \(UTC-4\). So `2014-06-04 12:00 EDT` specifies that same time instant. But `2014-06-04 12:00 EST` specifies noon Eastern Standard Time \(UTC-5\), regardless of whether daylight savings was nominally in effect on that date.

To complicate matters, some jurisdictions have used the same timezone abbreviation to mean different UTC offsets at different times; for example, in Moscow `MSK` has meant UTC+3 in some years and UTC+4 in others. Greenplum interprets such abbreviations according to whatever they meant \(or had most recently meant\) on the specified date; but, as with the `EST` example above, this is not necessarily the same as local civil time on that date.

One should be wary that the POSIX-style time zone feature can lead to silently accepting bogus input, since there is no check on the reasonableness of the zone abbreviations. For example, `SET TIMEZONE TO FOOBAR0` will work, leaving the system effectively using a rather peculiar abbreviation for UTC. Another issue to keep in mind is that in POSIX time zone names, positive offsets are used for locations of Greenwich. Everywhere else, Greenplum follows the ISO-8601 convention that positive timezone offsets are of Greenwich.

In all cases, timezone names and abbreviations are recognized case-insensitively.

Neither timezone names nor abbreviations are hard-wired into the server; they are obtained from configuration files \(see [Configuring Localization Settings](../install_guide/localization.html)\).

The `TimeZone` configuration parameter can be set in the file , or in any of the other standard ways for setting configuration parameters. There are also some special ways to set it:

1.  The SQL command `SET TIME ZONE` sets the time zone for the session. This is an alternative spelling of `SET TIMEZONE TO` with a more SQL-spec-compatible syntax.
2.  The `PGTZ` environment variable is used by `libpq` clients to send a `SET TIME ZONE` command to the server upon connection.

## <a id="datatype-interval-input"></a>Interval Input 

`interval` values can be written using the following verbose syntax:

```

<@> <quantity> <unit> <quantity> <unit>... <direction>

```

where quantity is a number \(possibly signed\); unit is `microsecond`, `millisecond`, `second`, `minute`, `hour`, `day`, `week`, `month`, `year`, `decade`, `century`, `millennium`, or abbreviations or plurals of these units; direction can be `ago` or empty. The at sign \(`@`\) is optional noise. The amounts of the different units are implicitly added with appropriate sign accounting. `ago` negates all the fields. This syntax is also used for interval output, if [IntervalStyle](config_params/guc-list.html) is set to `postgres_verbose`.

Quantities of days, hours, minutes, and seconds can be specified without explicit unit markings. For example, `'1 12:59:10'` is read the same as `'1 day 12 hours 59 min 10 sec'`. Also, a combination of years and months can be specified with a dash; for example `'200-10'` is read the same as `'200 years 10 months'`. \(These shorter forms are in fact the only ones allowed by the SQL standard, and are used for output when IntervalStyle is set to `sql_standard`.\)

Interval values can also be written as ISO 8601 time intervals, using either the `format with designators` of the standard's section 4.4.3.2 or the `alternative format` of section 4.4.3.3. The format with designators looks like this:

```

P <quantity> <unit> <quantity> <unit> ...  T  <quantity> <unit> ...

```

The string must start with a `P`, and may include a `T` that introduces the time-of-day units. The available unit abbreviations are given in [Table 8](#datatype-interval-iso8601-units). Units may be omitted, and may be specified in any order, but units smaller than a day must appear after `T`. In particular, the meaning of `M` depends on whether it is before or after `T`.

|Abbreviation|Meaning|
|------------|-------|
|Y|Years|
|M|Months \(in the date part\)|
|W|Weeks|
|D|Days|
|H|Hours|
|M|Minutes \(in the time part\)|
|S|Seconds|

In the alternative format:

```

P  <years>-<months>-<days>   T <hours>:<minutes>:<seconds> 

```

the string must begin with `P`, and a `T` separates the date and time parts of the interval. The values are given as numbers similar to ISO 8601 dates.

When writing an interval constant with a fields specification, or when assigning a string to an interval column that was defined with a fields specification, the interpretation of unmarked quantities depends on the fields. For example `INTERVAL '1' YEAR` is read as 1 year, whereas `INTERVAL '1'` means 1 second. Also, field values `to the right` of the least significant field allowed by the fields specification are silently discarded. For example, writing `INTERVAL '1 day 2:03:04' HOUR TO MINUTE` results in dropping the seconds field, but not the day field.

According to the SQL standard all fields of an interval value must have the same sign, so a leading negative sign applies to all fields; for example the negative sign in the interval literal `'-1 2:03:04'` applies to both the days and hour/minute/second parts. Greenplum allows the fields to have different signs, and traditionally treats each field in the textual representation as independently signed, so that the hour/minute/second part is considered positive in this example. If IntervalStyle is set to `sql_standard` then a leading sign is considered to apply to all fields \(but only if no additional signs appear\). Otherwise the traditional Greenplum interpretation is used. To avoid ambiguity, it's recommended to attach an explicit sign to each field if any field is negative.

In the verbose input format, and in some fields of the more compact input formats, field values can have fractional parts; for example `'1.5 week'` or `'01:02:03.45'`. Such input is converted to the appropriate number of months, days, and seconds for storage. When this would result in a fractional number of months or days, the fraction is added to the lower-order fields using the conversion factors 1 month = 30 days and 1 day = 24 hours. For example, `'1.5 month'` becomes 1 month and 15 days. Only seconds will ever be shown as fractional on output.

[Table 9](#datatype-interval-input-examples) shows some examples of valid `interval` input.

|Example|Description|
|-------|-----------|
|1-2|SQL standard format: 1 year 2 months|
|3 4:05:06|SQL standard format: 3 days 4 hours 5 minutes 6 seconds|
|1 year 2 months 3 days 4 hours 5 minutes 6 seconds|Traditional Postgres format: 1 year 2 months 3 days 4 hours 5 minutes 6 seconds|
|P1Y2M3DT4H5M6S|ISO 8601 `format with designators`: same meaning as above|
|P0001-02-03T04:05:06|ISO 8601 `alternative format`: same meaning as above|

Internally `interval` values are stored as months, days, and seconds. This is done because the number of days in a month varies, and a day can have 23 or 25 hours if a daylight savings time adjustment is involved. The months and days fields are integers while the seconds field can store fractions. Because intervals are usually created from constant strings or `timestamp` subtraction, this storage method works well in most cases, but can cause unexpected results: `SELECT EXTRACT(hours from '80 minutes'::interval); date_part ----------- 1 SELECT EXTRACT(days from '80 hours'::interval); date_part ----------- 0` Functions `justify_days` and `justify_hours` are available for adjusting days and hours that overflow their normal ranges.

## <a id="datatype-interval-output"></a>Interval Output 

The output format of the interval type can be set to one of the four styles `sql_standard`, `postgres`, `postgres_verbose`, or `iso_8601`, using the command `SET intervalstyle`. The default is the `postgres` format. [Table 10](#interval-style-output-table) shows examples of each output style.

The `sql_standard` style produces output that conforms to the SQL standard's specification for interval literal strings, if the interval value meets the standard's restrictions \(either year-month only or day-time only, with no mixing of positive and negative components\). Otherwise the output looks like a standard year-month literal string followed by a day-time literal string, with explicit signs added to disambiguate mixed-sign intervals.

The output of the `postgres` style matches the output of PostgreSQL releases prior to 8.4 when the [DateStyle](config_params/guc-list.html) parameter was set to `ISO`.

The output of the `postgres_verbose` style matches the output of PostgreSQL releases prior to 8.4 when the DateStyle parameter was set to non-`ISO` output.

The output of the `iso_8601` style matches the `format with designators` described in section 4.4.3.2 of the ISO 8601 standard.

|Style Specification|Year-Month Interval|Day-Time Interval|Mixed Interval|
|-------------------|-------------------|-----------------|--------------|
|`sql_standard`|1-2|3 4:05:06|-1-2 +3 -4:05:06|
|`postgres`|1 year 2 mons|3 days 04:05:06|-1 year -2 mons +3 days -04:05:06|
|`postgres_verbose`|@ 1 year 2 mons|@ 3 days 4 hours 5 mins 6 secs|@ 1 year 2 mons -3 days 4 hours 5 mins 6 secs ago|
|`iso_8601`|P1Y2M|P3DT4H5M6S|P-1Y-2M3DT-4H-5M-6S|

**Parent topic:** [Data Types](data_types.html)

