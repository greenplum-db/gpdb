--

/*CUT-HERE*/
CREATE EXTENSION ip4r;

-- Check whether any of our opclasses fail amvalidate

DO $d$
  DECLARE
    r record;
  BEGIN
    IF current_setting('server_version_num')::integer >= 90600 THEN
      FOR r IN SELECT amname,
                      CASE amname WHEN 'btree' THEN 6
		                  WHEN 'hash' THEN 6
				  WHEN 'gist' THEN 3
				  ELSE 0 END as expected,
		      count(nullif(amvalidate(opc.oid),false)) as actual
                 FROM pg_opclass opc
                      LEFT JOIN pg_am am ON am.oid = opcmethod
                WHERE opcintype IN ('ip4'::regtype,
                                    'ip4r'::regtype,
                                    'ip6'::regtype,
                                    'ip6r'::regtype,
                                    'ipaddress'::regtype,
                                    'iprange'::regtype)
                GROUP BY amname
                ORDER BY amname
      LOOP
         IF r.expected IS DISTINCT FROM r.actual THEN
           RAISE INFO '% % operator classes did not validate', r.expected - r.actual, r.amname;
         END IF;
      END LOOP;
    END IF;
  END;
$d$;
/*CUT-END*/

\set VERBOSITY terse

SET extra_float_digits = 0;

--
-- Valid and invalid addresses
--

--valid ip4
select '1.2.3.4'::ip4;
select '0.0.0.0'::ip4;
select '255.255.255.255'::ip4;
select '0.0.0.255'::ip4;
select '0.0.255.0'::ip4;
select '0.255.0.0'::ip4;
select '255.0.0.0'::ip4;
select '192.168.123.210'::ip4;
select '127.0.0.1'::ip4;

--invalid ip4
select '1.2.3'::ip4;
select '0'::ip4;
select ' 1.2.3.4'::ip4;
select '1.2.3.4 '::ip4;
select '0.0.0.256'::ip4;
select '0.0.256'::ip4;
select '0..255.0'::ip4;
select '+0.255.0.0'::ip4;
select '1.2.3.4-1.2.3.4'::ip4;

-- valid ip6
select '0000:0000:0000:0000:0000:0000:0000:0000'::ip6;
select '0000:0000:0000:0000:0000:0000:0000:0001'::ip6;
select '0:0:0:0:0:0:0:0'::ip6;
select '0:0:0:0:0:0:0:1'::ip6;
select '0:0:0:0:0:0:13.1.68.3'::ip6;
select '0:0:0:0:0:FFFF:129.144.52.38'::ip6;
select '0::0'::ip6;
select '1:2:3:4:5:6:1.2.3.4'::ip6;
select '1:2:3:4:5:6:7:8'::ip6;
select '1:2:3:4:5:6::'::ip6;
select '1:2:3:4:5:6::8'::ip6;
select '1:2:3:4:5::'::ip6;
select '1:2:3:4:5::1.2.3.4'::ip6;
select '1:2:3:4:5::7:8'::ip6;
select '1:2:3:4:5::8'::ip6;
select '1:2:3:4::'::ip6;
select '1:2:3:4::1.2.3.4'::ip6;
select '1:2:3:4::5:1.2.3.4'::ip6;
select '1:2:3:4::7:8'::ip6;
select '1:2:3:4::8'::ip6;
select '1:2:3::'::ip6;
select '1:2:3::1.2.3.4'::ip6;
select '1:2:3::5:1.2.3.4'::ip6;
select '1:2:3::7:8'::ip6;
select '1:2:3::8'::ip6;
select '1:2::'::ip6;
select '1:2::1.2.3.4'::ip6;
select '1:2::5:1.2.3.4'::ip6;
select '1:2::7:8'::ip6;
select '1:2::8'::ip6;
select '1::'::ip6;
select '1::1.2.3.4'::ip6;
select '1::2:3'::ip6;
select '1::2:3:4'::ip6;
select '1::2:3:4:5'::ip6;
select '1::2:3:4:5:6'::ip6;
select '1::2:3:4:5:6:7'::ip6;
select '1::5:1.2.3.4'::ip6;
select '1::5:11.22.33.44'::ip6;
select '1::7:8'::ip6;
select '1::8'::ip6;
select '2001:0000:1234:0000:0000:C1C0:ABCD:0876'::ip6;
select '2001:0db8:0000:0000:0000:0000:1428:57ab'::ip6;
select '2001:0db8:0000:0000:0000::1428:57ab'::ip6;
select '2001:0db8:0:0:0:0:1428:57ab'::ip6;
select '2001:0db8:0:0::1428:57ab'::ip6;
select '2001:0db8:1234:0000:0000:0000:0000:0000'::ip6;
select '2001:0db8:1234::'::ip6;
select '2001:0db8:1234:ffff:ffff:ffff:ffff:ffff'::ip6;
select '2001:0db8:85a3:0000:0000:8a2e:0370:7334'::ip6;
select '2001:0db8::1428:57ab'::ip6;
select '2001:10::'::ip6;
select '2001::'::ip6;
select '2001:DB8:0:0:8:800:200C:417A'::ip6;
select '2001:DB8::8:800:200C:417A'::ip6;
select '2001:db8:85a3:0:0:8a2e:370:7334'::ip6;
select '2001:db8:85a3::8a2e:370:7334'::ip6;
select '2001:db8::'::ip6;
select '2001:db8::'::ip6;
select '2001:db8::1428:57ab'::ip6;
select '2001:db8:a::123'::ip6;
select '2002::'::ip6;
select '2::10'::ip6;
select '3ffe:0b00:0000:0000:0001:0000:0000:000a'::ip6;
select '::'::ip6;
select '::1'::ip6;
select '::127.0.0.1'::ip6;
select '::13.1.68.3'::ip6;
select '::2:3'::ip6;
select '::2:3:4'::ip6;
select '::2:3:4:5'::ip6;
select '::2:3:4:5:6'::ip6;
select '::2:3:4:5:6:7'::ip6;
select '::2:3:4:5:6:7:8'::ip6;
select '::8'::ip6;
select '::FFFF:129.144.52.38'::ip6;
select '::ffff:0:0'::ip6;
select '::ffff:0:192.168.1.1'::ip6;
select '::ffff:1:1.2.3.4'::ip6;
select '::ffff:0c22:384e'::ip6;
select '::ffff:12.34.56.78'::ip6;
select '::ffff:192.0.2.128'::ip6;
select '::ffff:192.168.1.1'::ip6;
select '::ffff:192.168.1.26'::ip6;
select '::ffff:c000:280'::ip6;
select 'FF01:0:0:0:0:0:0:101'::ip6;
select 'FF01::101'::ip6;
select 'FF02:0000:0000:0000:0000:0000:0000:0001'::ip6;
select 'fc00::'::ip6;
select 'fe80:0000:0000:0000:0204:61ff:fe9d:f156'::ip6;
select 'fe80:0:0:0:204:61ff:254.157.241.86'::ip6;
select 'fe80:0:0:0:204:61ff:fe9d:f156'::ip6;
select 'fe80::'::ip6;
select 'fe80::1'::ip6;
select 'fe80::204:61ff:254.157.241.86'::ip6;
select 'fe80::204:61ff:fe9d:f156'::ip6;
select 'fe80::217:f2ff:254.7.237.98'::ip6;
select 'fe80::217:f2ff:fe07:ed62'::ip6;
select 'ff02::1'::ip6;

-- invalid ip6
select ''::ip6;
select '02001:0000:1234:0000:0000:C1C0:ABCD:0876'::ip6;
select '1.2.3.4:1111:2222:3333:4444::5555'::ip6;
select '1.2.3.4:1111:2222:3333::5555'::ip6;
select '1.2.3.4:1111:2222::5555'::ip6;
select '1.2.3.4:1111::5555'::ip6;
select '1.2.3.4::'::ip6;
select '1.2.3.4::5555'::ip6;
select '1111:'::ip6;
select '1111:2222:3333:4444::5555:'::ip6;
select '1111:2222:3333::5555:'::ip6;
select '1111:2222::5555:'::ip6;
select '1111::5555:'::ip6;
select '123'::ip6;
select '12345::6:7:8'::ip6;
select '127.0.0.1'::ip6;
select '1:2:3:4:5:6:7:8:9'::ip6;
select '1:2:3::4:5:6:7:8:9'::ip6;
select '1:2:3::4:5::7:8'::ip6;
select '1::1.2.256.4'::ip6;
select '1::1.2.3.256'::ip6;
select '1::1.2.3.300'::ip6;
select '1::1.2.3.900'::ip6;
select '1::1.2.300.4'::ip6;
select '1::1.2.900.4'::ip6;
select '1::1.256.3.4'::ip6;
select '1::1.300.3.4'::ip6;
select '1::1.900.3.4'::ip6;
select '1::256.2.3.4'::ip6;
select '1::260.2.3.4'::ip6;
select '1::2::3'::ip6;
select '1::300.2.3.4'::ip6;
select '1::300.300.300.300'::ip6;
select '1::3000.30.30.30'::ip6;
select '1::400.2.3.4'::ip6;
select '1::5:1.2.256.4'::ip6;
select '1::5:1.2.3.256'::ip6;
select '1::5:1.2.3.300'::ip6;
select '1::5:1.2.3.900'::ip6;
select '1::5:1.2.300.4'::ip6;
select '1::5:1.2.900.4'::ip6;
select '1::5:1.256.3.4'::ip6;
select '1::5:1.300.3.4'::ip6;
select '1::5:1.900.3.4'::ip6;
select '1::5:256.2.3.4'::ip6;
select '1::5:260.2.3.4'::ip6;
select '1::5:300.2.3.4'::ip6;
select '1::5:300.300.300.300'::ip6;
select '1::5:3000.30.30.30'::ip6;
select '1::5:400.2.3.4'::ip6;
select '1::5:900.2.3.4'::ip6;
select '1::900.2.3.4'::ip6;
select '1:::3:4:5'::ip6;
select '2001:0000:1234: 0000:0000:C1C0:ABCD:0876'::ip6;
select '2001:0000:1234:0000:00001:C1C0:ABCD:0876'::ip6;
select '2001:0000:1234:0000:0000:C1C0:ABCD:0876 0'::ip6;
select '2001::FFD3::57ab'::ip6;
select '2001:DB8:0:0:8:800:200C:417A:221'::ip6;
select '2001:db8:85a3::8a2e:37023:7334'::ip6;
select '2001:db8:85a3::8a2e:370k:7334'::ip6;
select '3ffe:0b00:0000:0001:0000:0000:000a'::ip6;
select '3ffe:b00::1::a'::ip6;
select ':'::ip6;
select ':1111:2222:3333:4444::5555'::ip6;
select ':1111:2222:3333::5555'::ip6;
select ':1111:2222::5555'::ip6;
select ':1111::5555'::ip6;
select '::1.2.256.4'::ip6;
select '::1.2.3.256'::ip6;
select '::1.2.3.300'::ip6;
select '::1.2.3.900'::ip6;
select '::1.2.300.4'::ip6;
select '::1.2.900.4'::ip6;
select '::1.256.3.4'::ip6;
select '::1.300.3.4'::ip6;
select '::1.900.3.4'::ip6;
select '::1111:2222:3333:4444:5555:6666::'::ip6;
select '::256.2.3.4'::ip6;
select '::260.2.3.4'::ip6;
select '::300.2.3.4'::ip6;
select '::300.300.300.300'::ip6;
select '::3000.30.30.30'::ip6;
select '::400.2.3.4'::ip6;
select '::5555:'::ip6;
select '::900.2.3.4'::ip6;
select ':::'::ip6;
select ':::5555'::ip6;
select '::ffff:2.3.4'::ip6;
select '::ffff:257.1.2.3'::ip6;
select 'FF01::101::2'::ip6;
select 'FF02:0000:0000:0000:0000:0000:0000:0000:0001'::ip6;
select 'ldkfj'::ip6;

-- valid ip4r
select '1.2.3.4'::ip4r;
select '255.255.255.255/32'::ip4r;
select '255.255.255.254/31'::ip4r;
select '255.255.255.252/30'::ip4r;
select '255.255.255.248/29'::ip4r;
select '255.255.255.240/28'::ip4r;
select '255.255.255.224/27'::ip4r;
select '255.255.255.192/26'::ip4r;
select '255.255.255.128/25'::ip4r;
select '255.255.255.0/24'::ip4r;
select '255.255.254.0/23'::ip4r;
select '255.255.252.0/22'::ip4r;
select '255.255.248.0/21'::ip4r;
select '255.255.240.0/20'::ip4r;
select '255.255.224.0/19'::ip4r;
select '255.255.192.0/18'::ip4r;
select '255.255.128.0/17'::ip4r;
select '255.255.0.0/16'::ip4r;
select '255.254.0.0/15'::ip4r;
select '255.252.0.0/14'::ip4r;
select '255.248.0.0/13'::ip4r;
select '255.240.0.0/12'::ip4r;
select '255.224.0.0/11'::ip4r;
select '255.192.0.0/10'::ip4r;
select '255.128.0.0/9'::ip4r;
select '255.0.0.0/8'::ip4r;
select '254.0.0.0/7'::ip4r;
select '252.0.0.0/6'::ip4r;
select '248.0.0.0/5'::ip4r;
select '240.0.0.0/4'::ip4r;
select '224.0.0.0/3'::ip4r;
select '192.0.0.0/2'::ip4r;
select '128.0.0.0/1'::ip4r;
select '0.0.0.0/0'::ip4r;
select '1.2.3.4-5.6.7.8'::ip4r;
select '5.6.7.8-1.2.3.4'::ip4r;
select '1.2.3.4-1.2.3.4'::ip4r;

-- invalid ip4r
select '1.2.3'::ip4r;
select '255.255.255.255.255.255.255.255.255'::ip4r;
select '255.255.255.255.255-255.255.255.255.255'::ip4r;
select '255.255.255.255-1.2.3.4.5'::ip4r;
select '255.255.255.255-1.2.3'::ip4r;
select '0.0.0.1/31'::ip4r;
select '0.0.0.1/30'::ip4r;
select '0.0.0.1/29'::ip4r;
select '0.0.0.1/28'::ip4r;
select '0.0.0.1/27'::ip4r;
select '0.0.0.1/26'::ip4r;
select '0.0.0.1/25'::ip4r;
select '0.0.0.1/24'::ip4r;
select '0.0.0.1/23'::ip4r;
select '0.0.0.1/22'::ip4r;
select '0.0.0.1/21'::ip4r;
select '0.0.0.1/20'::ip4r;
select '0.0.0.1/19'::ip4r;
select '0.0.0.1/18'::ip4r;
select '0.0.0.1/17'::ip4r;
select '0.0.0.1/16'::ip4r;
select '0.0.0.1/15'::ip4r;
select '0.0.0.1/14'::ip4r;
select '0.0.0.1/13'::ip4r;
select '0.0.0.1/12'::ip4r;
select '0.0.0.1/11'::ip4r;
select '0.0.0.1/10'::ip4r;
select '0.0.0.1/9'::ip4r;
select '0.0.0.1/8'::ip4r;
select '0.0.0.1/7'::ip4r;
select '0.0.0.1/6'::ip4r;
select '0.0.0.1/5'::ip4r;
select '0.0.0.1/4'::ip4r;
select '0.0.0.1/3'::ip4r;
select '0.0.0.1/2'::ip4r;
select '0.0.0.1/1'::ip4r;
select '0.0.0.1/0'::ip4r;
select '0.0.0.2/30'::ip4r;
select '0.0.0.4/29'::ip4r;
select '0.0.0.8/28'::ip4r;
select '0.0.0.16/27'::ip4r;
select '0.0.0.32/26'::ip4r;
select '0.0.0.64/25'::ip4r;
select '0.0.0.128/24'::ip4r;
select '0.0.1.0/23'::ip4r;
select '0.0.2.0/22'::ip4r;
select '0.0.4.0/21'::ip4r;
select '0.0.8.0/20'::ip4r;
select '0.0.16.0/19'::ip4r;
select '0.0.32.0/18'::ip4r;
select '0.0.64.0/17'::ip4r;
select '0.0.128.0/16'::ip4r;
select '0.1.0.0/15'::ip4r;
select '0.2.0.0/14'::ip4r;
select '0.4.0.0/13'::ip4r;
select '0.8.0.0/12'::ip4r;
select '0.16.0.0/11'::ip4r;
select '0.32.0.0/10'::ip4r;
select '0.64.0.0/9'::ip4r;
select '0.128.0.0/8'::ip4r;
select '1.0.0.0/7'::ip4r;
select '2.0.0.0/6'::ip4r;
select '4.0.0.0/5'::ip4r;
select '8.0.0.0/4'::ip4r;
select '16.0.0.0/3'::ip4r;
select '32.0.0.0/2'::ip4r;
select '64.0.0.0/1'::ip4r;
select '128.0.0.0/0'::ip4r;
select '0.0.0.0/33'::ip4r;
select '0.0.0.0/3.0'::ip4r;
select '0.0.0.0/+33'::ip4r;

-- valid ip6r
select '::'::ip6r;
select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff'::ip6r;
select '1::2'::ip6r;
select '::-ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff'::ip6r;
select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff-::'::ip6r;
select '1::2-3::4'::ip6r;
select '3::4-3::4'::ip6r;
select '3::4-1::2'::ip6r;
select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff/128'::ip6r;
select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:fffe/127'::ip6r;
select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:fffc/126'::ip6r;
select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:fff8/125'::ip6r;
select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:fff0/124'::ip6r;
select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ff00/120'::ip6r;
select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:f000/116'::ip6r;
select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:0000/112'::ip6r;
select 'ffff:ffff:ffff:ffff:ffff:ffff:0000:0000/96'::ip6r;
select 'ffff:ffff:ffff:ffff:ffff:0000:0000:0000/80'::ip6r;
select 'ffff:ffff:ffff:ffff:fff0:0000:0000:0000/76'::ip6r;
select 'ffff:ffff:ffff:ffff:ff00:0000:0000:0000/72'::ip6r;
select 'ffff:ffff:ffff:ffff:f000:0000:0000:0000/68'::ip6r;
select 'ffff:ffff:ffff:ffff:e000:0000:0000:0000/67'::ip6r;
select 'ffff:ffff:ffff:ffff:c000:0000:0000:0000/66'::ip6r;
select 'ffff:ffff:ffff:ffff:8000:0000:0000:0000/65'::ip6r;
select 'ffff:ffff:ffff:ffff:0000:0000:0000:0000/64'::ip6r;
select 'ffff:ffff:ffff:fffe:0000:0000:0000:0000/63'::ip6r;
select 'ffff:ffff:ffff:fffc:0000:0000:0000:0000/62'::ip6r;
select 'ffff:ffff:ffff:fff8:0000:0000:0000:0000/61'::ip6r;
select 'ffff:ffff:ffff:fff0:0000:0000:0000:0000/60'::ip6r;
select 'ffff:ffff:ffff:ff00:0000:0000:0000:0000/56'::ip6r;
select 'ffff:ffff:ffff:f000:0000:0000:0000:0000/52'::ip6r;
select 'ffff:ffff:ffff:0000:0000:0000:0000:0000/48'::ip6r;
select 'ffff:ffff:0000:0000:0000:0000:0000:0000/32'::ip6r;
select 'ffff:0000:0000:0000:0000:0000:0000:0000/16'::ip6r;
select 'fff0:0000:0000:0000:0000:0000:0000:0000/12'::ip6r;
select 'ff00:0000:0000:0000:0000:0000:0000:0000/8'::ip6r;
select 'f000:0000:0000:0000:0000:0000:0000:0000/4'::ip6r;
select 'e000:0000:0000:0000:0000:0000:0000:0000/3'::ip6r;
select 'c000:0000:0000:0000:0000:0000:0000:0000/2'::ip6r;
select '8000:0000:0000:0000:0000:0000:0000:0000/1'::ip6r;
select '0000:0000:0000:0000:0000:0000:0000:0000/0'::ip6r;

-- invalid ip6r
select '::-::-::'::ip6r;
select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff-ffff'::ip6r;
select '::-ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff'::ip6r;
select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff-::'::ip6r;
select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff/128'::ip6r;
select '0000:0000:0000:0000:0000:0000:0000:0001/127'::ip6r;
select '0000:0000:0000:0000:0000:0000:0000:0001/120'::ip6r;
select '0000:0000:0000:0000:0000:0000:0000:0001/112'::ip6r;
select '0000:0000:0000:0000:0000:0000:0000:0001/96'::ip6r;
select '0000:0000:0000:0000:0000:0000:0000:0001/80'::ip6r;
select '0000:0000:0000:0000:0000:0000:0000:0001/64'::ip6r;
select '0000:0000:0000:0000:0000:0000:0000:0001/48'::ip6r;
select '0000:0000:0000:0000:0000:0000:0000:0001/32'::ip6r;
select '0000:0000:0000:0000:0000:0000:0000:0001/16'::ip6r;
select '0000:0000:0000:0000:0000:0000:0000:0001/8'::ip6r;
select '0000:0000:0000:0000:0000:0000:0000:0001/4'::ip6r;
select '0000:0000:0000:0000:0000:0000:0000:0001/0'::ip6r;
select '0000:0000:0000:0000:0000:0000:0000:0008/124'::ip6r;
select '0000:0000:0000:0000:0000:0000:0000:0080/120'::ip6r;
select '0000:0000:0000:0000:0000:0000:0000:8000/112'::ip6r;
select '0000:0000:0000:0000:0000:0000:8000:0000/96'::ip6r;
select '0000:0000:0000:0000:0000:8000:0000:0000/80'::ip6r;
select '0000:0000:0000:0000:8000:0000:0000:0000/64'::ip6r;
select '0000:0000:0000:8000:0000:0000:0000:0000/48'::ip6r;
select '0000:0000:8000:0000:0000:0000:0000:0000/32'::ip6r;
select '0000:8000:0000:0000:0000:0000:0000:0000/16'::ip6r;
select '0080:0000:0000:0000:0000:0000:0000:0000/8'::ip6r;
select '0800:0000:0000:0000:0000:0000:0000:0000/4'::ip6r;
select '8000:0000:0000:0000:0000:0000:0000:0000/0'::ip6r;
select '::/129'::ip6r;
select '::/255'::ip6r;
select '::/256'::ip6r;
select '::/+0'::ip6r;
select '::/0-0'::ip6r;
select '::-::/0'::ip6r;

-- valid ipaddress
select a, family(a) from (select '1.2.3.4'::ipaddress as a) s1;
select a, family(a) from (select '0.0.0.0'::ipaddress as a) s1;
select a, family(a) from (select '255.255.255.255'::ipaddress as a) s1;
select a, family(a) from (select '0.0.0.255'::ipaddress as a) s1;
select a, family(a) from (select '0.0.255.0'::ipaddress as a) s1;
select a, family(a) from (select '0.255.0.0'::ipaddress as a) s1;
select a, family(a) from (select '255.0.0.0'::ipaddress as a) s1;
select a, family(a) from (select '192.168.123.210'::ipaddress as a) s1;
select a, family(a) from (select '127.0.0.1'::ipaddress as a) s1;
select a, family(a) from (select '0000:0000:0000:0000:0000:0000:0000:0000'::ipaddress as a) s1;
select a, family(a) from (select '0000:0000:0000:0000:0000:0000:0000:0001'::ipaddress as a) s1;
select a, family(a) from (select '0:0:0:0:0:0:0:0'::ipaddress as a) s1;
select a, family(a) from (select '0:0:0:0:0:0:0:1'::ipaddress as a) s1;
select a, family(a) from (select '0:0:0:0:0:0:13.1.68.3'::ipaddress as a) s1;
select a, family(a) from (select '0:0:0:0:0:FFFF:129.144.52.38'::ipaddress as a) s1;
select a, family(a) from (select '0::0'::ipaddress as a) s1;
select a, family(a) from (select '1:2:3:4:5:6:1.2.3.4'::ipaddress as a) s1;
select a, family(a) from (select '1:2:3:4:5:6:7:8'::ipaddress as a) s1;
select a, family(a) from (select '1:2:3:4:5:6::'::ipaddress as a) s1;
select a, family(a) from (select '1:2:3:4:5:6::8'::ipaddress as a) s1;
select a, family(a) from (select '1:2:3:4:5::'::ipaddress as a) s1;
select a, family(a) from (select '1:2:3:4:5::1.2.3.4'::ipaddress as a) s1;
select a, family(a) from (select '1:2:3:4:5::7:8'::ipaddress as a) s1;
select a, family(a) from (select '1:2:3:4:5::8'::ipaddress as a) s1;
select a, family(a) from (select '1:2:3:4::'::ipaddress as a) s1;
select a, family(a) from (select '1:2:3:4::1.2.3.4'::ipaddress as a) s1;
select a, family(a) from (select '1:2:3:4::5:1.2.3.4'::ipaddress as a) s1;
select a, family(a) from (select '1:2:3:4::7:8'::ipaddress as a) s1;
select a, family(a) from (select '1:2:3:4::8'::ipaddress as a) s1;
select a, family(a) from (select '1:2:3::'::ipaddress as a) s1;
select a, family(a) from (select '1:2:3::1.2.3.4'::ipaddress as a) s1;
select a, family(a) from (select '1:2:3::5:1.2.3.4'::ipaddress as a) s1;
select a, family(a) from (select '1:2:3::7:8'::ipaddress as a) s1;
select a, family(a) from (select '1:2:3::8'::ipaddress as a) s1;
select a, family(a) from (select '1:2::'::ipaddress as a) s1;
select a, family(a) from (select '1:2::1.2.3.4'::ipaddress as a) s1;
select a, family(a) from (select '1:2::5:1.2.3.4'::ipaddress as a) s1;
select a, family(a) from (select '1:2::7:8'::ipaddress as a) s1;
select a, family(a) from (select '1:2::8'::ipaddress as a) s1;
select a, family(a) from (select '1::'::ipaddress as a) s1;
select a, family(a) from (select '1::1.2.3.4'::ipaddress as a) s1;
select a, family(a) from (select '1::2:3'::ipaddress as a) s1;
select a, family(a) from (select '1::2:3:4'::ipaddress as a) s1;
select a, family(a) from (select '1::2:3:4:5'::ipaddress as a) s1;
select a, family(a) from (select '1::2:3:4:5:6'::ipaddress as a) s1;
select a, family(a) from (select '1::2:3:4:5:6:7'::ipaddress as a) s1;
select a, family(a) from (select '1::5:1.2.3.4'::ipaddress as a) s1;
select a, family(a) from (select '1::5:11.22.33.44'::ipaddress as a) s1;
select a, family(a) from (select '1::7:8'::ipaddress as a) s1;
select a, family(a) from (select '1::8'::ipaddress as a) s1;
select a, family(a) from (select '2001:0000:1234:0000:0000:C1C0:ABCD:0876'::ipaddress as a) s1;
select a, family(a) from (select '2001:0db8:0000:0000:0000:0000:1428:57ab'::ipaddress as a) s1;
select a, family(a) from (select '2001:0db8:0000:0000:0000::1428:57ab'::ipaddress as a) s1;
select a, family(a) from (select '2001:0db8:0:0:0:0:1428:57ab'::ipaddress as a) s1;
select a, family(a) from (select '2001:0db8:0:0::1428:57ab'::ipaddress as a) s1;
select a, family(a) from (select '2001:0db8:1234:0000:0000:0000:0000:0000'::ipaddress as a) s1;
select a, family(a) from (select '2001:0db8:1234::'::ipaddress as a) s1;
select a, family(a) from (select '2001:0db8:1234:ffff:ffff:ffff:ffff:ffff'::ipaddress as a) s1;
select a, family(a) from (select '2001:0db8:85a3:0000:0000:8a2e:0370:7334'::ipaddress as a) s1;
select a, family(a) from (select '2001:0db8::1428:57ab'::ipaddress as a) s1;
select a, family(a) from (select '2001:10::'::ipaddress as a) s1;
select a, family(a) from (select '2001::'::ipaddress as a) s1;
select a, family(a) from (select '2001:DB8:0:0:8:800:200C:417A'::ipaddress as a) s1;
select a, family(a) from (select '2001:DB8::8:800:200C:417A'::ipaddress as a) s1;
select a, family(a) from (select '2001:db8:85a3:0:0:8a2e:370:7334'::ipaddress as a) s1;
select a, family(a) from (select '2001:db8:85a3::8a2e:370:7334'::ipaddress as a) s1;
select a, family(a) from (select '2001:db8::'::ipaddress as a) s1;
select a, family(a) from (select '2001:db8::'::ipaddress as a) s1;
select a, family(a) from (select '2001:db8::1428:57ab'::ipaddress as a) s1;
select a, family(a) from (select '2001:db8:a::123'::ipaddress as a) s1;
select a, family(a) from (select '2002::'::ipaddress as a) s1;
select a, family(a) from (select '2::10'::ipaddress as a) s1;
select a, family(a) from (select '3ffe:0b00:0000:0000:0001:0000:0000:000a'::ipaddress as a) s1;
select a, family(a) from (select '::'::ipaddress as a) s1;
select a, family(a) from (select '::1'::ipaddress as a) s1;
select a, family(a) from (select '::127.0.0.1'::ipaddress as a) s1;
select a, family(a) from (select '::13.1.68.3'::ipaddress as a) s1;
select a, family(a) from (select '::2:3'::ipaddress as a) s1;
select a, family(a) from (select '::2:3:4'::ipaddress as a) s1;
select a, family(a) from (select '::2:3:4:5'::ipaddress as a) s1;
select a, family(a) from (select '::2:3:4:5:6'::ipaddress as a) s1;
select a, family(a) from (select '::2:3:4:5:6:7'::ipaddress as a) s1;
select a, family(a) from (select '::2:3:4:5:6:7:8'::ipaddress as a) s1;
select a, family(a) from (select '::8'::ipaddress as a) s1;
select a, family(a) from (select '::FFFF:129.144.52.38'::ipaddress as a) s1;
select a, family(a) from (select '::ffff:0:0'::ipaddress as a) s1;
select a, family(a) from (select '::ffff:0:192.168.1.1'::ipaddress as a) s1;
select a, family(a) from (select '::ffff:1:1.2.3.4'::ipaddress as a) s1;
select a, family(a) from (select '::ffff:0c22:384e'::ipaddress as a) s1;
select a, family(a) from (select '::ffff:12.34.56.78'::ipaddress as a) s1;
select a, family(a) from (select '::ffff:192.0.2.128'::ipaddress as a) s1;
select a, family(a) from (select '::ffff:192.168.1.1'::ipaddress as a) s1;
select a, family(a) from (select '::ffff:192.168.1.26'::ipaddress as a) s1;
select a, family(a) from (select '::ffff:c000:280'::ipaddress as a) s1;
select a, family(a) from (select 'FF01:0:0:0:0:0:0:101'::ipaddress as a) s1;
select a, family(a) from (select 'FF01::101'::ipaddress as a) s1;
select a, family(a) from (select 'FF02:0000:0000:0000:0000:0000:0000:0001'::ipaddress as a) s1;
select a, family(a) from (select 'fc00::'::ipaddress as a) s1;
select a, family(a) from (select 'fe80:0000:0000:0000:0204:61ff:fe9d:f156'::ipaddress as a) s1;
select a, family(a) from (select 'fe80:0:0:0:204:61ff:254.157.241.86'::ipaddress as a) s1;
select a, family(a) from (select 'fe80:0:0:0:204:61ff:fe9d:f156'::ipaddress as a) s1;
select a, family(a) from (select 'fe80::'::ipaddress as a) s1;
select a, family(a) from (select 'fe80::1'::ipaddress as a) s1;
select a, family(a) from (select 'fe80::204:61ff:254.157.241.86'::ipaddress as a) s1;
select a, family(a) from (select 'fe80::204:61ff:fe9d:f156'::ipaddress as a) s1;
select a, family(a) from (select 'fe80::217:f2ff:254.7.237.98'::ipaddress as a) s1;
select a, family(a) from (select 'fe80::217:f2ff:fe07:ed62'::ipaddress as a) s1;
select a, family(a) from (select 'ff02::1'::ipaddress as a) s1;

-- invalid ipaddress
select '1.2.3'::ipaddress;
select '0'::ipaddress;
select ' 1.2.3.4'::ipaddress;
select '1.2.3.4 '::ipaddress;
select '0.0.0.256'::ipaddress;
select '0.0.256'::ipaddress;
select '0..255.0'::ipaddress;
select '+0.255.0.0'::ipaddress;
select '1.2.3.4-1.2.3.4'::ipaddress;
select ''::ipaddress;
select '02001:0000:1234:0000:0000:C1C0:ABCD:0876'::ipaddress;
select '1.2.3.4:1111:2222:3333:4444::5555'::ipaddress;
select '1.2.3.4:1111:2222:3333::5555'::ipaddress;
select '1.2.3.4:1111:2222::5555'::ipaddress;
select '1.2.3.4:1111::5555'::ipaddress;
select '1.2.3.4::'::ipaddress;
select '1.2.3.4::5555'::ipaddress;
select '1111:'::ipaddress;
select '1111:2222:3333:4444::5555:'::ipaddress;
select '1111:2222:3333::5555:'::ipaddress;
select '1111:2222::5555:'::ipaddress;
select '1111::5555:'::ipaddress;
select '123'::ipaddress;
select '12345::6:7:8'::ipaddress;
select '1:2:3:4:5:6:7:8:9'::ipaddress;
select '1:2:3::4:5:6:7:8:9'::ipaddress;
select '1:2:3::4:5::7:8'::ipaddress;
select '1::1.2.256.4'::ipaddress;
select '1::1.2.3.256'::ipaddress;
select '1::1.2.3.300'::ipaddress;
select '1::1.2.3.900'::ipaddress;
select '1::1.2.300.4'::ipaddress;
select '1::1.2.900.4'::ipaddress;
select '1::1.256.3.4'::ipaddress;
select '1::1.300.3.4'::ipaddress;
select '1::1.900.3.4'::ipaddress;
select '1::256.2.3.4'::ipaddress;
select '1::260.2.3.4'::ipaddress;
select '1::2::3'::ipaddress;
select '1::300.2.3.4'::ipaddress;
select '1::300.300.300.300'::ipaddress;
select '1::3000.30.30.30'::ipaddress;
select '1::400.2.3.4'::ipaddress;
select '1::5:1.2.256.4'::ipaddress;
select '1::5:1.2.3.256'::ipaddress;
select '1::5:1.2.3.300'::ipaddress;
select '1::5:1.2.3.900'::ipaddress;
select '1::5:1.2.300.4'::ipaddress;
select '1::5:1.2.900.4'::ipaddress;
select '1::5:1.256.3.4'::ipaddress;
select '1::5:1.300.3.4'::ipaddress;
select '1::5:1.900.3.4'::ipaddress;
select '1::5:256.2.3.4'::ipaddress;
select '1::5:260.2.3.4'::ipaddress;
select '1::5:300.2.3.4'::ipaddress;
select '1::5:300.300.300.300'::ipaddress;
select '1::5:3000.30.30.30'::ipaddress;
select '1::5:400.2.3.4'::ipaddress;
select '1::5:900.2.3.4'::ipaddress;
select '1::900.2.3.4'::ipaddress;
select '1:::3:4:5'::ipaddress;
select '2001:0000:1234: 0000:0000:C1C0:ABCD:0876'::ipaddress;
select '2001:0000:1234:0000:00001:C1C0:ABCD:0876'::ipaddress;
select '2001:0000:1234:0000:0000:C1C0:ABCD:0876 0'::ipaddress;
select '2001::FFD3::57ab'::ipaddress;
select '2001:DB8:0:0:8:800:200C:417A:221'::ipaddress;
select '2001:db8:85a3::8a2e:37023:7334'::ipaddress;
select '2001:db8:85a3::8a2e:370k:7334'::ipaddress;
select '3ffe:0b00:0000:0001:0000:0000:000a'::ipaddress;
select '3ffe:b00::1::a'::ipaddress;
select ':'::ipaddress;
select ':1111:2222:3333:4444::5555'::ipaddress;
select ':1111:2222:3333::5555'::ipaddress;
select ':1111:2222::5555'::ipaddress;
select ':1111::5555'::ipaddress;
select '::1.2.256.4'::ipaddress;
select '::1.2.3.256'::ipaddress;
select '::1.2.3.300'::ipaddress;
select '::1.2.3.900'::ipaddress;
select '::1.2.300.4'::ipaddress;
select '::1.2.900.4'::ipaddress;
select '::1.256.3.4'::ipaddress;
select '::1.300.3.4'::ipaddress;
select '::1.900.3.4'::ipaddress;
select '::1111:2222:3333:4444:5555:6666::'::ipaddress;
select '::256.2.3.4'::ipaddress;
select '::260.2.3.4'::ipaddress;
select '::300.2.3.4'::ipaddress;
select '::300.300.300.300'::ipaddress;
select '::3000.30.30.30'::ipaddress;
select '::400.2.3.4'::ipaddress;
select '::5555:'::ipaddress;
select '::900.2.3.4'::ipaddress;
select ':::'::ipaddress;
select ':::5555'::ipaddress;
select '::ffff:2.3.4'::ipaddress;
select '::ffff:257.1.2.3'::ipaddress;
select 'FF01::101::2'::ipaddress;
select 'FF02:0000:0000:0000:0000:0000:0000:0000:0001'::ipaddress;
select 'ldkfj'::ipaddress;

-- valid iprange
select r, family(r), iprange_size(r) from (select '-'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '1.2.3.4'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '255.255.255.255/32'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '255.255.255.254/31'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '255.255.255.252/30'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '255.255.255.248/29'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '255.255.255.240/28'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '255.255.255.224/27'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '255.255.255.192/26'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '255.255.255.128/25'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '255.255.255.0/24'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '255.255.254.0/23'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '255.255.252.0/22'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '255.255.248.0/21'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '255.255.240.0/20'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '255.255.224.0/19'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '255.255.192.0/18'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '255.255.128.0/17'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '255.255.0.0/16'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '255.254.0.0/15'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '255.252.0.0/14'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '255.248.0.0/13'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '255.240.0.0/12'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '255.224.0.0/11'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '255.192.0.0/10'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '255.128.0.0/9'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '255.0.0.0/8'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '254.0.0.0/7'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '252.0.0.0/6'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '248.0.0.0/5'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '240.0.0.0/4'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '224.0.0.0/3'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '192.0.0.0/2'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '128.0.0.0/1'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '0.0.0.0/0'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '1.2.3.4-5.6.7.8'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '5.6.7.8-1.2.3.4'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '1.2.3.4-1.2.3.4'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '::'::iprange as r) s;
select r, family(r), iprange_size(r) from (select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '1::2'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '::-ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff'::iprange as r) s;
select r, family(r), iprange_size(r) from (select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff-::'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '1::2-3::4'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '3::4-3::4'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '3::4-1::2'::iprange as r) s;
select r, family(r), iprange_size(r) from (select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff/128'::iprange as r) s;
select r, family(r), iprange_size(r) from (select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:fffe/127'::iprange as r) s;
select r, family(r), iprange_size(r) from (select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:fffc/126'::iprange as r) s;
select r, family(r), iprange_size(r) from (select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:fff8/125'::iprange as r) s;
select r, family(r), iprange_size(r) from (select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:fff0/124'::iprange as r) s;
select r, family(r), iprange_size(r) from (select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ff00/120'::iprange as r) s;
select r, family(r), iprange_size(r) from (select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:f000/116'::iprange as r) s;
select r, family(r), iprange_size(r) from (select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:0000/112'::iprange as r) s;
select r, family(r), iprange_size(r) from (select 'ffff:ffff:ffff:ffff:ffff:ffff:0000:0000/96'::iprange as r) s;
select r, family(r), iprange_size(r) from (select 'ffff:ffff:ffff:ffff:ffff:0000:0000:0000/80'::iprange as r) s;
select r, family(r), iprange_size(r) from (select 'ffff:ffff:ffff:ffff:fff0:0000:0000:0000/76'::iprange as r) s;
select r, family(r), iprange_size(r) from (select 'ffff:ffff:ffff:ffff:ff00:0000:0000:0000/72'::iprange as r) s;
select r, family(r), iprange_size(r) from (select 'ffff:ffff:ffff:ffff:f000:0000:0000:0000/68'::iprange as r) s;
select r, family(r), iprange_size(r) from (select 'ffff:ffff:ffff:ffff:e000:0000:0000:0000/67'::iprange as r) s;
select r, family(r), iprange_size(r) from (select 'ffff:ffff:ffff:ffff:c000:0000:0000:0000/66'::iprange as r) s;
select r, family(r), iprange_size(r) from (select 'ffff:ffff:ffff:ffff:8000:0000:0000:0000/65'::iprange as r) s;
select r, family(r), iprange_size(r) from (select 'ffff:ffff:ffff:ffff:0000:0000:0000:0000/64'::iprange as r) s;
select r, family(r), iprange_size(r) from (select 'ffff:ffff:ffff:fffe:0000:0000:0000:0000/63'::iprange as r) s;
select r, family(r), iprange_size(r) from (select 'ffff:ffff:ffff:fffc:0000:0000:0000:0000/62'::iprange as r) s;
select r, family(r), iprange_size(r) from (select 'ffff:ffff:ffff:fff8:0000:0000:0000:0000/61'::iprange as r) s;
select r, family(r), iprange_size(r) from (select 'ffff:ffff:ffff:fff0:0000:0000:0000:0000/60'::iprange as r) s;
select r, family(r), iprange_size(r) from (select 'ffff:ffff:ffff:ff00:0000:0000:0000:0000/56'::iprange as r) s;
select r, family(r), iprange_size(r) from (select 'ffff:ffff:ffff:f000:0000:0000:0000:0000/52'::iprange as r) s;
select r, family(r), iprange_size(r) from (select 'ffff:ffff:ffff:0000:0000:0000:0000:0000/48'::iprange as r) s;
select r, family(r), iprange_size(r) from (select 'ffff:ffff:0000:0000:0000:0000:0000:0000/32'::iprange as r) s;
select r, family(r), iprange_size(r) from (select 'ffff:0000:0000:0000:0000:0000:0000:0000/16'::iprange as r) s;
select r, family(r), iprange_size(r) from (select 'fff0:0000:0000:0000:0000:0000:0000:0000/12'::iprange as r) s;
select r, family(r), iprange_size(r) from (select 'ff00:0000:0000:0000:0000:0000:0000:0000/8'::iprange as r) s;
select r, family(r), iprange_size(r) from (select 'f000:0000:0000:0000:0000:0000:0000:0000/4'::iprange as r) s;
select r, family(r), iprange_size(r) from (select 'e000:0000:0000:0000:0000:0000:0000:0000/3'::iprange as r) s;
select r, family(r), iprange_size(r) from (select 'c000:0000:0000:0000:0000:0000:0000:0000/2'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '8000:0000:0000:0000:0000:0000:0000:0000/1'::iprange as r) s;
select r, family(r), iprange_size(r) from (select '0000:0000:0000:0000:0000:0000:0000:0000/0'::iprange as r) s;

-- invalid iprange
select '1.2.3'::iprange;
select '255.255.255.255.255.255.255.255.255'::iprange;
select '255.255.255.255.255-255.255.255.255.255'::iprange;
select '255.255.255.255-1.2.3.4.5'::iprange;
select '255.255.255.255-1.2.3'::iprange;
select '0.0.0.1/31'::iprange;
select '0.0.0.1/30'::iprange;
select '0.0.0.1/29'::iprange;
select '0.0.0.1/28'::iprange;
select '0.0.0.1/27'::iprange;
select '0.0.0.1/26'::iprange;
select '0.0.0.1/25'::iprange;
select '0.0.0.1/24'::iprange;
select '0.0.0.1/23'::iprange;
select '0.0.0.1/22'::iprange;
select '0.0.0.1/21'::iprange;
select '0.0.0.1/20'::iprange;
select '0.0.0.1/19'::iprange;
select '0.0.0.1/18'::iprange;
select '0.0.0.1/17'::iprange;
select '0.0.0.1/16'::iprange;
select '0.0.0.1/15'::iprange;
select '0.0.0.1/14'::iprange;
select '0.0.0.1/13'::iprange;
select '0.0.0.1/12'::iprange;
select '0.0.0.1/11'::iprange;
select '0.0.0.1/10'::iprange;
select '0.0.0.1/9'::iprange;
select '0.0.0.1/8'::iprange;
select '0.0.0.1/7'::iprange;
select '0.0.0.1/6'::iprange;
select '0.0.0.1/5'::iprange;
select '0.0.0.1/4'::iprange;
select '0.0.0.1/3'::iprange;
select '0.0.0.1/2'::iprange;
select '0.0.0.1/1'::iprange;
select '0.0.0.1/0'::iprange;
select '0.0.0.2/30'::iprange;
select '0.0.0.4/29'::iprange;
select '0.0.0.8/28'::iprange;
select '0.0.0.16/27'::iprange;
select '0.0.0.32/26'::iprange;
select '0.0.0.64/25'::iprange;
select '0.0.0.128/24'::iprange;
select '0.0.1.0/23'::iprange;
select '0.0.2.0/22'::iprange;
select '0.0.4.0/21'::iprange;
select '0.0.8.0/20'::iprange;
select '0.0.16.0/19'::iprange;
select '0.0.32.0/18'::iprange;
select '0.0.64.0/17'::iprange;
select '0.0.128.0/16'::iprange;
select '0.1.0.0/15'::iprange;
select '0.2.0.0/14'::iprange;
select '0.4.0.0/13'::iprange;
select '0.8.0.0/12'::iprange;
select '0.16.0.0/11'::iprange;
select '0.32.0.0/10'::iprange;
select '0.64.0.0/9'::iprange;
select '0.128.0.0/8'::iprange;
select '1.0.0.0/7'::iprange;
select '2.0.0.0/6'::iprange;
select '4.0.0.0/5'::iprange;
select '8.0.0.0/4'::iprange;
select '16.0.0.0/3'::iprange;
select '32.0.0.0/2'::iprange;
select '64.0.0.0/1'::iprange;
select '128.0.0.0/0'::iprange;
select '0.0.0.0/33'::iprange;
select '0.0.0.0/3.0'::iprange;
select '0.0.0.0/+33'::iprange;
select '::-::-::'::iprange;
select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff-ffff'::ip6r;
select '::-ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff'::iprange;
select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff-::'::iprange;
select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff/128'::iprange;
select '0000:0000:0000:0000:0000:0000:0000:0001/127'::iprange;
select '0000:0000:0000:0000:0000:0000:0000:0001/120'::iprange;
select '0000:0000:0000:0000:0000:0000:0000:0001/112'::iprange;
select '0000:0000:0000:0000:0000:0000:0000:0001/96'::iprange;
select '0000:0000:0000:0000:0000:0000:0000:0001/80'::iprange;
select '0000:0000:0000:0000:0000:0000:0000:0001/64'::iprange;
select '0000:0000:0000:0000:0000:0000:0000:0001/48'::iprange;
select '0000:0000:0000:0000:0000:0000:0000:0001/32'::iprange;
select '0000:0000:0000:0000:0000:0000:0000:0001/16'::iprange;
select '0000:0000:0000:0000:0000:0000:0000:0001/8'::iprange;
select '0000:0000:0000:0000:0000:0000:0000:0001/4'::iprange;
select '0000:0000:0000:0000:0000:0000:0000:0001/0'::iprange;
select '0000:0000:0000:0000:0000:0000:0000:0008/124'::iprange;
select '0000:0000:0000:0000:0000:0000:0000:0080/120'::iprange;
select '0000:0000:0000:0000:0000:0000:0000:8000/112'::iprange;
select '0000:0000:0000:0000:0000:0000:8000:0000/96'::iprange;
select '0000:0000:0000:0000:0000:8000:0000:0000/80'::iprange;
select '0000:0000:0000:0000:8000:0000:0000:0000/64'::iprange;
select '0000:0000:0000:8000:0000:0000:0000:0000/48'::iprange;
select '0000:0000:8000:0000:0000:0000:0000:0000/32'::iprange;
select '0000:8000:0000:0000:0000:0000:0000:0000/16'::iprange;
select '0080:0000:0000:0000:0000:0000:0000:0000/8'::iprange;
select '0800:0000:0000:0000:0000:0000:0000:0000/4'::iprange;
select '8000:0000:0000:0000:0000:0000:0000:0000/0'::iprange;
select '::/129'::iprange;
select '::/255'::iprange;
select '::/256'::iprange;
select '::/+0'::iprange;
select '::/0-0'::iprange;
select '::-::/0'::iprange;
select '-::'::iprange;
select '-1.2.3.4'::iprange;
select '1.2.3.4-'::iprange;

-- canaries for portability issues in binary output

select r, encode(ip4_send(r),'hex') from (select '128.1.255.0'::ip4 as r) s;
select r, encode(ip4_send(r),'hex') from (select '1.128.0.255'::ip4 as r) s;
select r, encode(ip4r_send(r),'hex') from (select '128.1.255.0/24'::ip4r as r) s;
select r, encode(ip4r_send(r),'hex') from (select '128.1.255.1-128.1.255.2'::ip4r as r) s;

select r, encode(ip6_send(r),'hex') from (select 'ffff::8000'::ip6 as r) s;
select r, encode(ip6_send(r),'hex') from (select '8000::ffff'::ip6 as r) s;
select r, encode(ip6r_send(r),'hex') from (select 'ffff::8000/128'::ip6r as r) s;
select r, encode(ip6r_send(r),'hex') from (select '8000::ffff/128'::ip6r as r) s;

select r, encode(ipaddress_send(r),'hex') from (select '128.1.255.0'::ipaddress as r) s;
select r, encode(ipaddress_send(r),'hex') from (select '1.128.0.255'::ipaddress as r) s;
select r, encode(iprange_send(r),'hex') from (select '128.1.255.0/24'::iprange as r) s;
select r, encode(iprange_send(r),'hex') from (select '128.1.255.1-128.1.255.2'::iprange as r) s;

select r, encode(ipaddress_send(r),'hex') from (select 'ffff::8000'::ipaddress as r) s;
select r, encode(ipaddress_send(r),'hex') from (select '8000::ffff'::ipaddress as r) s;
select r, encode(iprange_send(r),'hex') from (select 'ffff::8000/128'::iprange as r) s;
select r, encode(iprange_send(r),'hex') from (select 'ffff::8000/120'::iprange as r) s;
select r, encode(iprange_send(r),'hex') from (select 'ffff::8001-ffff::8002'::iprange as r) s;

select r, encode(iprange_send(r),'hex') from (select '-'::iprange as r) s;

-- text casts and cross-type casts

select r::text  from (select '-'::iprange as r) s;

select r::text, r::ip4r::text, lower(r)::ip4::text, upper(r::ip4r)::text from (select '1.2.3.4'::iprange as r) s;
select r::text, r::ip4r::text, lower(r)::ip4::text, upper(r::ip4r)::text from (select '255.255.255.255/32'::iprange as r) s;
select r::text, r::ip4r::text, lower(r)::ip4::text, upper(r::ip4r)::text from (select '255.255.255.254/31'::iprange as r) s;
select r::text, r::ip4r::text, lower(r)::ip4::text, upper(r::ip4r)::text from (select '255.255.255.252/30'::iprange as r) s;
select r::text, r::ip4r::text, lower(r)::ip4::text, upper(r::ip4r)::text from (select '255.255.255.248/29'::iprange as r) s;
select r::text, r::ip4r::text, lower(r)::ip4::text, upper(r::ip4r)::text from (select '255.255.255.240/28'::iprange as r) s;
select r::text, r::ip4r::text, lower(r)::ip4::text, upper(r::ip4r)::text from (select '255.255.255.224/27'::iprange as r) s;
select r::text, r::ip4r::text, lower(r)::ip4::text, upper(r::ip4r)::text from (select '255.255.255.192/26'::iprange as r) s;
select r::text, r::ip4r::text, lower(r)::ip4::text, upper(r::ip4r)::text from (select '255.255.255.128/25'::iprange as r) s;
select r::text, r::ip4r::text, lower(r)::ip4::text, upper(r::ip4r)::text from (select '255.255.255.0/24'::iprange as r) s;
select r::text, r::ip4r::text, lower(r)::ip4::text, upper(r::ip4r)::text from (select '255.255.254.0/23'::iprange as r) s;
select r::text, r::ip4r::text, lower(r)::ip4::text, upper(r::ip4r)::text from (select '255.255.252.0/22'::iprange as r) s;
select r::text, r::ip4r::text, lower(r)::ip4::text, upper(r::ip4r)::text from (select '255.255.248.0/21'::iprange as r) s;
select r::text, r::ip4r::text, lower(r)::ip4::text, upper(r::ip4r)::text from (select '255.255.240.0/20'::iprange as r) s;
select r::text, r::ip4r::text, lower(r)::ip4::text, upper(r::ip4r)::text from (select '255.255.224.0/19'::iprange as r) s;
select r::text, r::ip4r::text, lower(r)::ip4::text, upper(r::ip4r)::text from (select '255.255.192.0/18'::iprange as r) s;
select r::text, r::ip4r::text, lower(r)::ip4::text, upper(r::ip4r)::text from (select '255.255.128.0/17'::iprange as r) s;
select r::text, r::ip4r::text, lower(r)::ip4::text, upper(r::ip4r)::text from (select '255.255.0.0/16'::iprange as r) s;
select r::text, r::ip4r::text, lower(r)::ip4::text, upper(r::ip4r)::text from (select '255.254.0.0/15'::iprange as r) s;
select r::text, r::ip4r::text, lower(r)::ip4::text, upper(r::ip4r)::text from (select '255.252.0.0/14'::iprange as r) s;
select r::text, r::ip4r::text, lower(r)::ip4::text, upper(r::ip4r)::text from (select '255.248.0.0/13'::iprange as r) s;
select r::text, r::ip4r::text, lower(r)::ip4::text, upper(r::ip4r)::text from (select '255.240.0.0/12'::iprange as r) s;
select r::text, r::ip4r::text, lower(r)::ip4::text, upper(r::ip4r)::text from (select '255.224.0.0/11'::iprange as r) s;
select r::text, r::ip4r::text, lower(r)::ip4::text, upper(r::ip4r)::text from (select '255.192.0.0/10'::iprange as r) s;
select r::text, r::ip4r::text, lower(r)::ip4::text, upper(r::ip4r)::text from (select '255.128.0.0/9'::iprange as r) s;
select r::text, r::ip4r::text, lower(r)::ip4::text, upper(r::ip4r)::text from (select '255.0.0.0/8'::iprange as r) s;
select r::text, r::ip4r::text, lower(r)::ip4::text, upper(r::ip4r)::text from (select '254.0.0.0/7'::iprange as r) s;
select r::text, r::ip4r::text, lower(r)::ip4::text, upper(r::ip4r)::text from (select '252.0.0.0/6'::iprange as r) s;
select r::text, r::ip4r::text, lower(r)::ip4::text, upper(r::ip4r)::text from (select '248.0.0.0/5'::iprange as r) s;
select r::text, r::ip4r::text, lower(r)::ip4::text, upper(r::ip4r)::text from (select '240.0.0.0/4'::iprange as r) s;
select r::text, r::ip4r::text, lower(r)::ip4::text, upper(r::ip4r)::text from (select '224.0.0.0/3'::iprange as r) s;
select r::text, r::ip4r::text, lower(r)::ip4::text, upper(r::ip4r)::text from (select '192.0.0.0/2'::iprange as r) s;
select r::text, r::ip4r::text, lower(r)::ip4::text, upper(r::ip4r)::text from (select '128.0.0.0/1'::iprange as r) s;
select r::text, r::ip4r::text, lower(r)::ip4::text, upper(r::ip4r)::text from (select '0.0.0.0/0'::iprange as r) s;
select r::text, r::ip4r::text, lower(r)::ip4::text, upper(r::ip4r)::text from (select '1.2.3.4-5.6.7.8'::iprange as r) s;
select r::text, r::ip4r::text, lower(r)::ip4::text, upper(r::ip4r)::text from (select '5.6.7.8-1.2.3.4'::iprange as r) s;
select r::text, r::ip4r::text, lower(r)::ip4::text, upper(r::ip4r)::text from (select '1.2.3.4-1.2.3.4'::iprange as r) s;

select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select '::'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select '1::2'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select '::-ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff-::'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select '1::2-3::4'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select '3::4-3::4'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select '3::4-1::2'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff/128'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:fffe/127'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:fffc/126'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:fff8/125'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:fff0/124'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ff00/120'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:f000/116'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:0000/112'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select 'ffff:ffff:ffff:ffff:ffff:ffff:0000:0000/96'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select 'ffff:ffff:ffff:ffff:ffff:0000:0000:0000/80'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select 'ffff:ffff:ffff:ffff:fff0:0000:0000:0000/76'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select 'ffff:ffff:ffff:ffff:ff00:0000:0000:0000/72'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select 'ffff:ffff:ffff:ffff:f000:0000:0000:0000/68'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select 'ffff:ffff:ffff:ffff:e000:0000:0000:0000/67'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select 'ffff:ffff:ffff:ffff:c000:0000:0000:0000/66'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select 'ffff:ffff:ffff:ffff:8000:0000:0000:0000/65'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select 'ffff:ffff:ffff:ffff:0000:0000:0000:0000/64'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select 'ffff:ffff:ffff:fffe:0000:0000:0000:0000/63'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select 'ffff:ffff:ffff:fffc:0000:0000:0000:0000/62'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select 'ffff:ffff:ffff:fff8:0000:0000:0000:0000/61'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select 'ffff:ffff:ffff:fff0:0000:0000:0000:0000/60'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select 'ffff:ffff:ffff:ff00:0000:0000:0000:0000/56'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select 'ffff:ffff:ffff:f000:0000:0000:0000:0000/52'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select 'ffff:ffff:ffff:0000:0000:0000:0000:0000/48'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select 'ffff:ffff:0000:0000:0000:0000:0000:0000/32'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select 'ffff:0000:0000:0000:0000:0000:0000:0000/16'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select 'fff0:0000:0000:0000:0000:0000:0000:0000/12'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select 'ff00:0000:0000:0000:0000:0000:0000:0000/8'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select 'f000:0000:0000:0000:0000:0000:0000:0000/4'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select 'e000:0000:0000:0000:0000:0000:0000:0000/3'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select 'c000:0000:0000:0000:0000:0000:0000:0000/2'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select '8000:0000:0000:0000:0000:0000:0000:0000/1'::iprange as r) s;
select r::text, r::ip6r::text, lower(r)::ip6::text, upper(r::ip6r)::text from (select '0000:0000:0000:0000:0000:0000:0000:0000/0'::iprange as r) s;

select a::ipaddress, a::ip4 from (select '1.2.3.4'::text as a) s1;
select a::ipaddress, a::ip4 from (select '0.0.0.0'::text as a) s1;
select a::ipaddress, a::ip4 from (select '255.255.255.255'::text as a) s1;
select a::ipaddress, a::ip4 from (select '0.0.0.255'::text as a) s1;
select a::ipaddress, a::ip4 from (select '0.0.255.0'::text as a) s1;
select a::ipaddress, a::ip4 from (select '0.255.0.0'::text as a) s1;
select a::ipaddress, a::ip4 from (select '255.0.0.0'::text as a) s1;
select a::ipaddress, a::ip4 from (select '192.168.123.210'::text as a) s1;
select a::ipaddress, a::ip4 from (select '127.0.0.1'::text as a) s1;

select a::ipaddress, a::ip6 from (select '0000:0000:0000:0000:0000:0000:0000:0000'::text as a) s1;
select a::ipaddress, a::ip6 from (select '0000:0000:0000:0000:0000:0000:0000:0001'::text as a) s1;
select a::ipaddress, a::ip6 from (select '0:0:0:0:0:0:0:0'::text as a) s1;
select a::ipaddress, a::ip6 from (select '0:0:0:0:0:0:0:1'::text as a) s1;
select a::ipaddress, a::ip6 from (select '0:0:0:0:0:0:13.1.68.3'::text as a) s1;
select a::ipaddress, a::ip6 from (select '0:0:0:0:0:FFFF:129.144.52.38'::text as a) s1;
select a::ipaddress, a::ip6 from (select '0::0'::text as a) s1;
select a::ipaddress, a::ip6 from (select '1:2:3:4:5:6:1.2.3.4'::text as a) s1;
select a::ipaddress, a::ip6 from (select '1:2:3:4:5:6:7:8'::text as a) s1;
select a::ipaddress, a::ip6 from (select '1:2:3:4:5:6::'::text as a) s1;
select a::ipaddress, a::ip6 from (select '1:2:3:4:5:6::8'::text as a) s1;
select a::ipaddress, a::ip6 from (select '1:2:3:4:5::'::text as a) s1;
select a::ipaddress, a::ip6 from (select '1:2:3:4:5::1.2.3.4'::text as a) s1;
select a::ipaddress, a::ip6 from (select '1:2:3:4:5::7:8'::text as a) s1;
select a::ipaddress, a::ip6 from (select '1:2:3:4:5::8'::text as a) s1;
select a::ipaddress, a::ip6 from (select '1:2:3:4::'::text as a) s1;
select a::ipaddress, a::ip6 from (select '1:2:3:4::1.2.3.4'::text as a) s1;
select a::ipaddress, a::ip6 from (select '1:2:3:4::5:1.2.3.4'::text as a) s1;
select a::ipaddress, a::ip6 from (select '1:2:3:4::7:8'::text as a) s1;
select a::ipaddress, a::ip6 from (select '1:2:3:4::8'::text as a) s1;
select a::ipaddress, a::ip6 from (select '1:2:3::'::text as a) s1;
select a::ipaddress, a::ip6 from (select '1:2:3::1.2.3.4'::text as a) s1;
select a::ipaddress, a::ip6 from (select '1:2:3::5:1.2.3.4'::text as a) s1;
select a::ipaddress, a::ip6 from (select '1:2:3::7:8'::text as a) s1;
select a::ipaddress, a::ip6 from (select '1:2:3::8'::text as a) s1;
select a::ipaddress, a::ip6 from (select '1:2::'::text as a) s1;
select a::ipaddress, a::ip6 from (select '1:2::1.2.3.4'::text as a) s1;
select a::ipaddress, a::ip6 from (select '1:2::5:1.2.3.4'::text as a) s1;
select a::ipaddress, a::ip6 from (select '1:2::7:8'::text as a) s1;
select a::ipaddress, a::ip6 from (select '1:2::8'::text as a) s1;
select a::ipaddress, a::ip6 from (select '1::'::text as a) s1;
select a::ipaddress, a::ip6 from (select '1::1.2.3.4'::text as a) s1;
select a::ipaddress, a::ip6 from (select '1::2:3'::text as a) s1;
select a::ipaddress, a::ip6 from (select '1::2:3:4'::text as a) s1;
select a::ipaddress, a::ip6 from (select '1::2:3:4:5'::text as a) s1;
select a::ipaddress, a::ip6 from (select '1::2:3:4:5:6'::text as a) s1;
select a::ipaddress, a::ip6 from (select '1::2:3:4:5:6:7'::text as a) s1;
select a::ipaddress, a::ip6 from (select '1::5:1.2.3.4'::text as a) s1;
select a::ipaddress, a::ip6 from (select '1::5:11.22.33.44'::text as a) s1;
select a::ipaddress, a::ip6 from (select '1::7:8'::text as a) s1;
select a::ipaddress, a::ip6 from (select '1::8'::text as a) s1;
select a::ipaddress, a::ip6 from (select '2001:0000:1234:0000:0000:C1C0:ABCD:0876'::text as a) s1;
select a::ipaddress, a::ip6 from (select '2001:0db8:0000:0000:0000:0000:1428:57ab'::text as a) s1;
select a::ipaddress, a::ip6 from (select '2001:0db8:0000:0000:0000::1428:57ab'::text as a) s1;
select a::ipaddress, a::ip6 from (select '2001:0db8:0:0:0:0:1428:57ab'::text as a) s1;
select a::ipaddress, a::ip6 from (select '2001:0db8:0:0::1428:57ab'::text as a) s1;
select a::ipaddress, a::ip6 from (select '2001:0db8:1234:0000:0000:0000:0000:0000'::text as a) s1;
select a::ipaddress, a::ip6 from (select '2001:0db8:1234::'::text as a) s1;
select a::ipaddress, a::ip6 from (select '2001:0db8:1234:ffff:ffff:ffff:ffff:ffff'::text as a) s1;
select a::ipaddress, a::ip6 from (select '2001:0db8:85a3:0000:0000:8a2e:0370:7334'::text as a) s1;
select a::ipaddress, a::ip6 from (select '2001:0db8::1428:57ab'::text as a) s1;
select a::ipaddress, a::ip6 from (select '2001:10::'::text as a) s1;
select a::ipaddress, a::ip6 from (select '2001::'::text as a) s1;
select a::ipaddress, a::ip6 from (select '2001:DB8:0:0:8:800:200C:417A'::text as a) s1;
select a::ipaddress, a::ip6 from (select '2001:DB8::8:800:200C:417A'::text as a) s1;
select a::ipaddress, a::ip6 from (select '2001:db8:85a3:0:0:8a2e:370:7334'::text as a) s1;
select a::ipaddress, a::ip6 from (select '2001:db8:85a3::8a2e:370:7334'::text as a) s1;
select a::ipaddress, a::ip6 from (select '2001:db8::'::text as a) s1;
select a::ipaddress, a::ip6 from (select '2001:db8::'::text as a) s1;
select a::ipaddress, a::ip6 from (select '2001:db8::1428:57ab'::text as a) s1;
select a::ipaddress, a::ip6 from (select '2001:db8:a::123'::text as a) s1;
select a::ipaddress, a::ip6 from (select '2002::'::text as a) s1;
select a::ipaddress, a::ip6 from (select '2::10'::text as a) s1;
select a::ipaddress, a::ip6 from (select '3ffe:0b00:0000:0000:0001:0000:0000:000a'::text as a) s1;
select a::ipaddress, a::ip6 from (select '::'::text as a) s1;
select a::ipaddress, a::ip6 from (select '::1'::text as a) s1;
select a::ipaddress, a::ip6 from (select '::127.0.0.1'::text as a) s1;
select a::ipaddress, a::ip6 from (select '::13.1.68.3'::text as a) s1;
select a::ipaddress, a::ip6 from (select '::2:3'::text as a) s1;
select a::ipaddress, a::ip6 from (select '::2:3:4'::text as a) s1;
select a::ipaddress, a::ip6 from (select '::2:3:4:5'::text as a) s1;
select a::ipaddress, a::ip6 from (select '::2:3:4:5:6'::text as a) s1;
select a::ipaddress, a::ip6 from (select '::2:3:4:5:6:7'::text as a) s1;
select a::ipaddress, a::ip6 from (select '::2:3:4:5:6:7:8'::text as a) s1;
select a::ipaddress, a::ip6 from (select '::8'::text as a) s1;
select a::ipaddress, a::ip6 from (select '::FFFF:129.144.52.38'::text as a) s1;
select a::ipaddress, a::ip6 from (select '::ffff:0:0'::text as a) s1;
select a::ipaddress, a::ip6 from (select '::ffff:0:192.168.1.1'::text as a) s1;
select a::ipaddress, a::ip6 from (select '::ffff:1:1.2.3.4'::text as a) s1;
select a::ipaddress, a::ip6 from (select '::ffff:0c22:384e'::text as a) s1;
select a::ipaddress, a::ip6 from (select '::ffff:12.34.56.78'::text as a) s1;
select a::ipaddress, a::ip6 from (select '::ffff:192.0.2.128'::text as a) s1;
select a::ipaddress, a::ip6 from (select '::ffff:192.168.1.1'::text as a) s1;
select a::ipaddress, a::ip6 from (select '::ffff:192.168.1.26'::text as a) s1;
select a::ipaddress, a::ip6 from (select '::ffff:c000:280'::text as a) s1;
select a::ipaddress, a::ip6 from (select 'FF01:0:0:0:0:0:0:101'::text as a) s1;
select a::ipaddress, a::ip6 from (select 'FF01::101'::text as a) s1;
select a::ipaddress, a::ip6 from (select 'FF02:0000:0000:0000:0000:0000:0000:0001'::text as a) s1;
select a::ipaddress, a::ip6 from (select 'fc00::'::text as a) s1;
select a::ipaddress, a::ip6 from (select 'fe80:0000:0000:0000:0204:61ff:fe9d:f156'::text as a) s1;
select a::ipaddress, a::ip6 from (select 'fe80:0:0:0:204:61ff:254.157.241.86'::text as a) s1;
select a::ipaddress, a::ip6 from (select 'fe80:0:0:0:204:61ff:fe9d:f156'::text as a) s1;
select a::ipaddress, a::ip6 from (select 'fe80::'::text as a) s1;
select a::ipaddress, a::ip6 from (select 'fe80::1'::text as a) s1;
select a::ipaddress, a::ip6 from (select 'fe80::204:61ff:254.157.241.86'::text as a) s1;
select a::ipaddress, a::ip6 from (select 'fe80::204:61ff:fe9d:f156'::text as a) s1;
select a::ipaddress, a::ip6 from (select 'fe80::217:f2ff:254.7.237.98'::text as a) s1;
select a::ipaddress, a::ip6 from (select 'fe80::217:f2ff:fe07:ed62'::text as a) s1;
select a::ipaddress, a::ip6 from (select 'ff02::1'::text as a) s1;

select r::iprange from (select '-'::text as r) s;
select r::iprange, r::ip4r from (select '1.2.3.4'::text as r) s;
select r::iprange, r::ip4r from (select '255.255.255.255/32'::text as r) s;
select r::iprange, r::ip4r from (select '255.255.255.254/31'::text as r) s;
select r::iprange, r::ip4r from (select '255.255.255.252/30'::text as r) s;
select r::iprange, r::ip4r from (select '255.255.255.248/29'::text as r) s;
select r::iprange, r::ip4r from (select '255.255.255.240/28'::text as r) s;
select r::iprange, r::ip4r from (select '255.255.255.224/27'::text as r) s;
select r::iprange, r::ip4r from (select '255.255.255.192/26'::text as r) s;
select r::iprange, r::ip4r from (select '255.255.255.128/25'::text as r) s;
select r::iprange, r::ip4r from (select '255.255.255.0/24'::text as r) s;
select r::iprange, r::ip4r from (select '255.255.254.0/23'::text as r) s;
select r::iprange, r::ip4r from (select '255.255.252.0/22'::text as r) s;
select r::iprange, r::ip4r from (select '255.255.248.0/21'::text as r) s;
select r::iprange, r::ip4r from (select '255.255.240.0/20'::text as r) s;
select r::iprange, r::ip4r from (select '255.255.224.0/19'::text as r) s;
select r::iprange, r::ip4r from (select '255.255.192.0/18'::text as r) s;
select r::iprange, r::ip4r from (select '255.255.128.0/17'::text as r) s;
select r::iprange, r::ip4r from (select '255.255.0.0/16'::text as r) s;
select r::iprange, r::ip4r from (select '255.254.0.0/15'::text as r) s;
select r::iprange, r::ip4r from (select '255.252.0.0/14'::text as r) s;
select r::iprange, r::ip4r from (select '255.248.0.0/13'::text as r) s;
select r::iprange, r::ip4r from (select '255.240.0.0/12'::text as r) s;
select r::iprange, r::ip4r from (select '255.224.0.0/11'::text as r) s;
select r::iprange, r::ip4r from (select '255.192.0.0/10'::text as r) s;
select r::iprange, r::ip4r from (select '255.128.0.0/9'::text as r) s;
select r::iprange, r::ip4r from (select '255.0.0.0/8'::text as r) s;
select r::iprange, r::ip4r from (select '254.0.0.0/7'::text as r) s;
select r::iprange, r::ip4r from (select '252.0.0.0/6'::text as r) s;
select r::iprange, r::ip4r from (select '248.0.0.0/5'::text as r) s;
select r::iprange, r::ip4r from (select '240.0.0.0/4'::text as r) s;
select r::iprange, r::ip4r from (select '224.0.0.0/3'::text as r) s;
select r::iprange, r::ip4r from (select '192.0.0.0/2'::text as r) s;
select r::iprange, r::ip4r from (select '128.0.0.0/1'::text as r) s;
select r::iprange, r::ip4r from (select '0.0.0.0/0'::text as r) s;
select r::iprange, r::ip4r from (select '1.2.3.4-5.6.7.8'::text as r) s;
select r::iprange, r::ip4r from (select '5.6.7.8-1.2.3.4'::text as r) s;
select r::iprange, r::ip4r from (select '1.2.3.4-1.2.3.4'::text as r) s;

select r::iprange, r::ip6r from (select '::'::text as r) s;
select r::iprange, r::ip6r from (select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff'::text as r) s;
select r::iprange, r::ip6r from (select '1::2'::text as r) s;
select r::iprange, r::ip6r from (select '::-ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff'::text as r) s;
select r::iprange, r::ip6r from (select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff-::'::text as r) s;
select r::iprange, r::ip6r from (select '1::2-3::4'::text as r) s;
select r::iprange, r::ip6r from (select '3::4-3::4'::text as r) s;
select r::iprange, r::ip6r from (select '3::4-1::2'::text as r) s;
select r::iprange, r::ip6r from (select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff/128'::text as r) s;
select r::iprange, r::ip6r from (select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:fffe/127'::text as r) s;
select r::iprange, r::ip6r from (select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:fffc/126'::text as r) s;
select r::iprange, r::ip6r from (select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:fff8/125'::text as r) s;
select r::iprange, r::ip6r from (select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:fff0/124'::text as r) s;
select r::iprange, r::ip6r from (select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ff00/120'::text as r) s;
select r::iprange, r::ip6r from (select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:f000/116'::text as r) s;
select r::iprange, r::ip6r from (select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:0000/112'::text as r) s;
select r::iprange, r::ip6r from (select 'ffff:ffff:ffff:ffff:ffff:ffff:0000:0000/96'::text as r) s;
select r::iprange, r::ip6r from (select 'ffff:ffff:ffff:ffff:ffff:0000:0000:0000/80'::text as r) s;
select r::iprange, r::ip6r from (select 'ffff:ffff:ffff:ffff:fff0:0000:0000:0000/76'::text as r) s;
select r::iprange, r::ip6r from (select 'ffff:ffff:ffff:ffff:ff00:0000:0000:0000/72'::text as r) s;
select r::iprange, r::ip6r from (select 'ffff:ffff:ffff:ffff:f000:0000:0000:0000/68'::text as r) s;
select r::iprange, r::ip6r from (select 'ffff:ffff:ffff:ffff:e000:0000:0000:0000/67'::text as r) s;
select r::iprange, r::ip6r from (select 'ffff:ffff:ffff:ffff:c000:0000:0000:0000/66'::text as r) s;
select r::iprange, r::ip6r from (select 'ffff:ffff:ffff:ffff:8000:0000:0000:0000/65'::text as r) s;
select r::iprange, r::ip6r from (select 'ffff:ffff:ffff:ffff:0000:0000:0000:0000/64'::text as r) s;
select r::iprange, r::ip6r from (select 'ffff:ffff:ffff:fffe:0000:0000:0000:0000/63'::text as r) s;
select r::iprange, r::ip6r from (select 'ffff:ffff:ffff:fffc:0000:0000:0000:0000/62'::text as r) s;
select r::iprange, r::ip6r from (select 'ffff:ffff:ffff:fff8:0000:0000:0000:0000/61'::text as r) s;
select r::iprange, r::ip6r from (select 'ffff:ffff:ffff:fff0:0000:0000:0000:0000/60'::text as r) s;
select r::iprange, r::ip6r from (select 'ffff:ffff:ffff:ff00:0000:0000:0000:0000/56'::text as r) s;
select r::iprange, r::ip6r from (select 'ffff:ffff:ffff:f000:0000:0000:0000:0000/52'::text as r) s;
select r::iprange, r::ip6r from (select 'ffff:ffff:ffff:0000:0000:0000:0000:0000/48'::text as r) s;
select r::iprange, r::ip6r from (select 'ffff:ffff:0000:0000:0000:0000:0000:0000/32'::text as r) s;
select r::iprange, r::ip6r from (select 'ffff:0000:0000:0000:0000:0000:0000:0000/16'::text as r) s;
select r::iprange, r::ip6r from (select 'fff0:0000:0000:0000:0000:0000:0000:0000/12'::text as r) s;
select r::iprange, r::ip6r from (select 'ff00:0000:0000:0000:0000:0000:0000:0000/8'::text as r) s;
select r::iprange, r::ip6r from (select 'f000:0000:0000:0000:0000:0000:0000:0000/4'::text as r) s;
select r::iprange, r::ip6r from (select 'e000:0000:0000:0000:0000:0000:0000:0000/3'::text as r) s;
select r::iprange, r::ip6r from (select 'c000:0000:0000:0000:0000:0000:0000:0000/2'::text as r) s;
select r::iprange, r::ip6r from (select '8000:0000:0000:0000:0000:0000:0000:0000/1'::text as r) s;
select r::iprange, r::ip6r from (select '0000:0000:0000:0000:0000:0000:0000:0000/0'::text as r) s;

-- invalid text casts

select '1.2.3'::text::ip4;
select '0'::text::ip4;
select ' 1.2.3.4'::text::ip4;
select '1.2.3.4 '::text::ip4;
select '0.0.0.256'::text::ip4;
select '0.0.256'::text::ip4;
select '0..255.0'::text::ip4;
select '+0.255.0.0'::text::ip4;
select '1.2.3.4-1.2.3.4'::text::ip4;

select '1.2.3'::text::ip4r;
select '255.255.255.255.255.255.255.255.255'::text::ip4r;
select '255.255.255.255.255-255.255.255.255.255'::text::ip4r;
select '255.255.255.255-1.2.3.4.5'::text::ip4r;
select '255.255.255.255-1.2.3'::text::ip4r;
select '0.0.0.1/31'::text::ip4r;
select '0.0.0.1/30'::text::ip4r;
select '0.0.0.1/29'::text::ip4r;
select '0.0.0.1/28'::text::ip4r;
select '0.0.0.1/27'::text::ip4r;
select '0.0.0.1/26'::text::ip4r;
select '0.0.0.1/25'::text::ip4r;
select '0.0.0.1/24'::text::ip4r;
select '0.0.0.1/23'::text::ip4r;
select '0.0.0.1/22'::text::ip4r;
select '0.0.0.1/21'::text::ip4r;
select '0.0.0.1/20'::text::ip4r;
select '0.0.0.1/19'::text::ip4r;
select '0.0.0.1/18'::text::ip4r;
select '0.0.0.1/17'::text::ip4r;
select '0.0.0.1/16'::text::ip4r;
select '0.0.0.1/15'::text::ip4r;
select '0.0.0.1/14'::text::ip4r;
select '0.0.0.1/13'::text::ip4r;
select '0.0.0.1/12'::text::ip4r;
select '0.0.0.1/11'::text::ip4r;
select '0.0.0.1/10'::text::ip4r;
select '0.0.0.1/9'::text::ip4r;
select '0.0.0.1/8'::text::ip4r;
select '0.0.0.1/7'::text::ip4r;
select '0.0.0.1/6'::text::ip4r;
select '0.0.0.1/5'::text::ip4r;
select '0.0.0.1/4'::text::ip4r;
select '0.0.0.1/3'::text::ip4r;
select '0.0.0.1/2'::text::ip4r;
select '0.0.0.1/1'::text::ip4r;
select '0.0.0.1/0'::text::ip4r;
select '0.0.0.2/30'::text::ip4r;
select '0.0.0.4/29'::text::ip4r;
select '0.0.0.8/28'::text::ip4r;
select '0.0.0.16/27'::text::ip4r;
select '0.0.0.32/26'::text::ip4r;
select '0.0.0.64/25'::text::ip4r;
select '0.0.0.128/24'::text::ip4r;
select '0.0.1.0/23'::text::ip4r;
select '0.0.2.0/22'::text::ip4r;
select '0.0.4.0/21'::text::ip4r;
select '0.0.8.0/20'::text::ip4r;
select '0.0.16.0/19'::text::ip4r;
select '0.0.32.0/18'::text::ip4r;
select '0.0.64.0/17'::text::ip4r;
select '0.0.128.0/16'::text::ip4r;
select '0.1.0.0/15'::text::ip4r;
select '0.2.0.0/14'::text::ip4r;
select '0.4.0.0/13'::text::ip4r;
select '0.8.0.0/12'::text::ip4r;
select '0.16.0.0/11'::text::ip4r;
select '0.32.0.0/10'::text::ip4r;
select '0.64.0.0/9'::text::ip4r;
select '0.128.0.0/8'::text::ip4r;
select '1.0.0.0/7'::text::ip4r;
select '2.0.0.0/6'::text::ip4r;
select '4.0.0.0/5'::text::ip4r;
select '8.0.0.0/4'::text::ip4r;
select '16.0.0.0/3'::text::ip4r;
select '32.0.0.0/2'::text::ip4r;
select '64.0.0.0/1'::text::ip4r;
select '128.0.0.0/0'::text::ip4r;
select '0.0.0.0/33'::text::ip4r;
select '0.0.0.0/3.0'::text::ip4r;
select '0.0.0.0/+33'::text::ip4r;

select ''::text::ip6;
select '02001:0000:1234:0000:0000:C1C0:ABCD:0876'::text::ip6;
select '1.2.3.4:1111:2222:3333:4444::5555'::text::ip6;
select '1.2.3.4:1111:2222:3333::5555'::text::ip6;
select '1.2.3.4:1111:2222::5555'::text::ip6;
select '1.2.3.4:1111::5555'::text::ip6;
select '1.2.3.4::'::text::ip6;
select '1.2.3.4::5555'::text::ip6;
select '1111:'::text::ip6;
select '1111:2222:3333:4444::5555:'::text::ip6;
select '1111:2222:3333::5555:'::text::ip6;
select '1111:2222::5555:'::text::ip6;
select '1111::5555:'::text::ip6;
select '123'::text::ip6;
select '12345::6:7:8'::text::ip6;
select '127.0.0.1'::text::ip6;
select '1:2:3:4:5:6:7:8:9'::text::ip6;
select '1:2:3::4:5:6:7:8:9'::text::ip6;
select '1:2:3::4:5::7:8'::text::ip6;
select '1::1.2.256.4'::text::ip6;
select '1::1.2.3.256'::text::ip6;
select '1::1.2.3.300'::text::ip6;
select '1::1.2.3.900'::text::ip6;
select '1::1.2.300.4'::text::ip6;
select '1::1.2.900.4'::text::ip6;
select '1::1.256.3.4'::text::ip6;
select '1::1.300.3.4'::text::ip6;
select '1::1.900.3.4'::text::ip6;
select '1::256.2.3.4'::text::ip6;
select '1::260.2.3.4'::text::ip6;
select '1::2::3'::text::ip6;
select '1::300.2.3.4'::text::ip6;
select '1::300.300.300.300'::text::ip6;
select '1::3000.30.30.30'::text::ip6;
select '1::400.2.3.4'::text::ip6;
select '1::5:1.2.256.4'::text::ip6;
select '1::5:1.2.3.256'::text::ip6;
select '1::5:1.2.3.300'::text::ip6;
select '1::5:1.2.3.900'::text::ip6;
select '1::5:1.2.300.4'::text::ip6;
select '1::5:1.2.900.4'::text::ip6;
select '1::5:1.256.3.4'::text::ip6;
select '1::5:1.300.3.4'::text::ip6;
select '1::5:1.900.3.4'::text::ip6;
select '1::5:256.2.3.4'::text::ip6;
select '1::5:260.2.3.4'::text::ip6;
select '1::5:300.2.3.4'::text::ip6;
select '1::5:300.300.300.300'::text::ip6;
select '1::5:3000.30.30.30'::text::ip6;
select '1::5:400.2.3.4'::text::ip6;
select '1::5:900.2.3.4'::text::ip6;
select '1::900.2.3.4'::text::ip6;
select '1:::3:4:5'::text::ip6;
select '2001:0000:1234: 0000:0000:C1C0:ABCD:0876'::text::ip6;
select '2001:0000:1234:0000:00001:C1C0:ABCD:0876'::text::ip6;
select '2001:0000:1234:0000:0000:C1C0:ABCD:0876 0'::text::ip6;
select '2001::FFD3::57ab'::text::ip6;
select '2001:DB8:0:0:8:800:200C:417A:221'::text::ip6;
select '2001:db8:85a3::8a2e:37023:7334'::text::ip6;
select '2001:db8:85a3::8a2e:370k:7334'::text::ip6;
select '3ffe:0b00:0000:0001:0000:0000:000a'::text::ip6;
select '3ffe:b00::1::a'::text::ip6;
select ':'::text::ip6;
select ':1111:2222:3333:4444::5555'::text::ip6;
select ':1111:2222:3333::5555'::text::ip6;
select ':1111:2222::5555'::text::ip6;
select ':1111::5555'::text::ip6;
select '::1.2.256.4'::text::ip6;
select '::1.2.3.256'::text::ip6;
select '::1.2.3.300'::text::ip6;
select '::1.2.3.900'::text::ip6;
select '::1.2.300.4'::text::ip6;
select '::1.2.900.4'::text::ip6;
select '::1.256.3.4'::text::ip6;
select '::1.300.3.4'::text::ip6;
select '::1.900.3.4'::text::ip6;
select '::1111:2222:3333:4444:5555:6666::'::text::ip6;
select '::256.2.3.4'::text::ip6;
select '::260.2.3.4'::text::ip6;
select '::300.2.3.4'::text::ip6;
select '::300.300.300.300'::text::ip6;
select '::3000.30.30.30'::text::ip6;
select '::400.2.3.4'::text::ip6;
select '::5555:'::text::ip6;
select '::900.2.3.4'::text::ip6;
select ':::'::text::ip6;
select ':::5555'::text::ip6;
select '::ffff:2.3.4'::text::ip6;
select '::ffff:257.1.2.3'::text::ip6;
select 'FF01::101::2'::text::ip6;
select 'FF02:0000:0000:0000:0000:0000:0000:0000:0001'::text::ip6;
select 'ldkfj'::text::ip6;

select '::-::-::'::text::ip6r;
select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff-ffff'::text::ip6r;
select '::-ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff'::text::ip6r;
select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff-::'::text::ip6r;
select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff/128'::text::ip6r;
select '0000:0000:0000:0000:0000:0000:0000:0001/127'::text::ip6r;
select '0000:0000:0000:0000:0000:0000:0000:0001/120'::text::ip6r;
select '0000:0000:0000:0000:0000:0000:0000:0001/112'::text::ip6r;
select '0000:0000:0000:0000:0000:0000:0000:0001/96'::text::ip6r;
select '0000:0000:0000:0000:0000:0000:0000:0001/80'::text::ip6r;
select '0000:0000:0000:0000:0000:0000:0000:0001/64'::text::ip6r;
select '0000:0000:0000:0000:0000:0000:0000:0001/48'::text::ip6r;
select '0000:0000:0000:0000:0000:0000:0000:0001/32'::text::ip6r;
select '0000:0000:0000:0000:0000:0000:0000:0001/16'::text::ip6r;
select '0000:0000:0000:0000:0000:0000:0000:0001/8'::text::ip6r;
select '0000:0000:0000:0000:0000:0000:0000:0001/4'::text::ip6r;
select '0000:0000:0000:0000:0000:0000:0000:0001/0'::text::ip6r;
select '0000:0000:0000:0000:0000:0000:0000:0008/124'::text::ip6r;
select '0000:0000:0000:0000:0000:0000:0000:0080/120'::text::ip6r;
select '0000:0000:0000:0000:0000:0000:0000:8000/112'::text::ip6r;
select '0000:0000:0000:0000:0000:0000:8000:0000/96'::text::ip6r;
select '0000:0000:0000:0000:0000:8000:0000:0000/80'::text::ip6r;
select '0000:0000:0000:0000:8000:0000:0000:0000/64'::text::ip6r;
select '0000:0000:0000:8000:0000:0000:0000:0000/48'::text::ip6r;
select '0000:0000:8000:0000:0000:0000:0000:0000/32'::text::ip6r;
select '0000:8000:0000:0000:0000:0000:0000:0000/16'::text::ip6r;
select '0080:0000:0000:0000:0000:0000:0000:0000/8'::text::ip6r;
select '0800:0000:0000:0000:0000:0000:0000:0000/4'::text::ip6r;
select '8000:0000:0000:0000:0000:0000:0000:0000/0'::text::ip6r;
select '::/129'::text::ip6r;
select '::/255'::text::ip6r;
select '::/256'::text::ip6r;
select '::/+0'::text::ip6r;
select '::/0-0'::text::ip6r;
select '::-::/0'::text::ip6r;

select '1.2.3'::text::ipaddress;
select '0'::text::ipaddress;
select ' 1.2.3.4'::text::ipaddress;
select '1.2.3.4 '::text::ipaddress;
select '0.0.0.256'::text::ipaddress;
select '0.0.256'::text::ipaddress;
select '0..255.0'::text::ipaddress;
select '+0.255.0.0'::text::ipaddress;
select '1.2.3.4-1.2.3.4'::text::ipaddress;
select ''::text::ipaddress;
select '02001:0000:1234:0000:0000:C1C0:ABCD:0876'::text::ipaddress;
select '1.2.3.4:1111:2222:3333:4444::5555'::text::ipaddress;
select '1.2.3.4:1111:2222:3333::5555'::text::ipaddress;
select '1.2.3.4:1111:2222::5555'::text::ipaddress;
select '1.2.3.4:1111::5555'::text::ipaddress;
select '1.2.3.4::'::text::ipaddress;
select '1.2.3.4::5555'::text::ipaddress;
select '1111:'::text::ipaddress;
select '1111:2222:3333:4444::5555:'::text::ipaddress;
select '1111:2222:3333::5555:'::text::ipaddress;
select '1111:2222::5555:'::text::ipaddress;
select '1111::5555:'::text::ipaddress;
select '123'::text::ipaddress;
select '12345::6:7:8'::text::ipaddress;
select '1:2:3:4:5:6:7:8:9'::text::ipaddress;
select '1:2:3::4:5:6:7:8:9'::text::ipaddress;
select '1:2:3::4:5::7:8'::text::ipaddress;
select '1::1.2.256.4'::text::ipaddress;
select '1::1.2.3.256'::text::ipaddress;
select '1::1.2.3.300'::text::ipaddress;
select '1::1.2.3.900'::text::ipaddress;
select '1::1.2.300.4'::text::ipaddress;
select '1::1.2.900.4'::text::ipaddress;
select '1::1.256.3.4'::text::ipaddress;
select '1::1.300.3.4'::text::ipaddress;
select '1::1.900.3.4'::text::ipaddress;
select '1::256.2.3.4'::text::ipaddress;
select '1::260.2.3.4'::text::ipaddress;
select '1::2::3'::text::ipaddress;
select '1::300.2.3.4'::text::ipaddress;
select '1::300.300.300.300'::text::ipaddress;
select '1::3000.30.30.30'::text::ipaddress;
select '1::400.2.3.4'::text::ipaddress;
select '1::5:1.2.256.4'::text::ipaddress;
select '1::5:1.2.3.256'::text::ipaddress;
select '1::5:1.2.3.300'::text::ipaddress;
select '1::5:1.2.3.900'::text::ipaddress;
select '1::5:1.2.300.4'::text::ipaddress;
select '1::5:1.2.900.4'::text::ipaddress;
select '1::5:1.256.3.4'::text::ipaddress;
select '1::5:1.300.3.4'::text::ipaddress;
select '1::5:1.900.3.4'::text::ipaddress;
select '1::5:256.2.3.4'::text::ipaddress;
select '1::5:260.2.3.4'::text::ipaddress;
select '1::5:300.2.3.4'::text::ipaddress;
select '1::5:300.300.300.300'::text::ipaddress;
select '1::5:3000.30.30.30'::text::ipaddress;
select '1::5:400.2.3.4'::text::ipaddress;
select '1::5:900.2.3.4'::text::ipaddress;
select '1::900.2.3.4'::text::ipaddress;
select '1:::3:4:5'::text::ipaddress;
select '2001:0000:1234: 0000:0000:C1C0:ABCD:0876'::text::ipaddress;
select '2001:0000:1234:0000:00001:C1C0:ABCD:0876'::text::ipaddress;
select '2001:0000:1234:0000:0000:C1C0:ABCD:0876 0'::text::ipaddress;
select '2001::FFD3::57ab'::text::ipaddress;
select '2001:DB8:0:0:8:800:200C:417A:221'::text::ipaddress;
select '2001:db8:85a3::8a2e:37023:7334'::text::ipaddress;
select '2001:db8:85a3::8a2e:370k:7334'::text::ipaddress;
select '3ffe:0b00:0000:0001:0000:0000:000a'::text::ipaddress;
select '3ffe:b00::1::a'::text::ipaddress;
select ':'::text::ipaddress;
select ':1111:2222:3333:4444::5555'::text::ipaddress;
select ':1111:2222:3333::5555'::text::ipaddress;
select ':1111:2222::5555'::text::ipaddress;
select ':1111::5555'::text::ipaddress;
select '::1.2.256.4'::text::ipaddress;
select '::1.2.3.256'::text::ipaddress;
select '::1.2.3.300'::text::ipaddress;
select '::1.2.3.900'::text::ipaddress;
select '::1.2.300.4'::text::ipaddress;
select '::1.2.900.4'::text::ipaddress;
select '::1.256.3.4'::text::ipaddress;
select '::1.300.3.4'::text::ipaddress;
select '::1.900.3.4'::text::ipaddress;
select '::1111:2222:3333:4444:5555:6666::'::text::ipaddress;
select '::256.2.3.4'::text::ipaddress;
select '::260.2.3.4'::text::ipaddress;
select '::300.2.3.4'::text::ipaddress;
select '::300.300.300.300'::text::ipaddress;
select '::3000.30.30.30'::text::ipaddress;
select '::400.2.3.4'::text::ipaddress;
select '::5555:'::text::ipaddress;
select '::900.2.3.4'::text::ipaddress;
select ':::'::text::ipaddress;
select ':::5555'::text::ipaddress;
select '::ffff:2.3.4'::text::ipaddress;
select '::ffff:257.1.2.3'::text::ipaddress;
select 'FF01::101::2'::text::ipaddress;
select 'FF02:0000:0000:0000:0000:0000:0000:0000:0001'::text::ipaddress;
select 'ldkfj'::text::ipaddress;

-- numeric casts

select n::ip4 from (select 0::bigint as n) s;
select n::ip4 from (select 256::bigint as n) s;
select n::ip4 from (select 65536::bigint as n) s;
select n::ip4 from (select 16777216::bigint as n) s;
select n::ip4 from (select -1::bigint as n) s;
select n::ip4 from (select -2147483647::bigint as n) s;
select n::ip4 from (select -2147483648::bigint as n) s;
select n::ip4 from (select 2147483647::bigint as n) s;
select n::ip4 from (select 2147483648::bigint as n) s;
select n::ip4 from (select 4294967295::bigint as n) s;

select n::ip4 from (select 0::float8 as n) s;
select n::ip4 from (select 256::float8 as n) s;
select n::ip4 from (select 65536::float8 as n) s;
select n::ip4 from (select 16777216::float8 as n) s;
select n::ip4 from (select -1::float8 as n) s;
select n::ip4 from (select -2147483647::float8 as n) s;
select n::ip4 from (select -2147483648::float8 as n) s;
select n::ip4 from (select 2147483647::float8 as n) s;
select n::ip4 from (select 2147483648::float8 as n) s;
select n::ip4 from (select 4294967295::float8 as n) s;

select n::ip4 from (select 0::numeric as n) s;
select n::ip4 from (select 256::numeric as n) s;
select n::ip4 from (select 65536::numeric as n) s;
select n::ip4 from (select 16777216::numeric as n) s;
select n::ip4 from (select -1::numeric as n) s;
select n::ip4 from (select -2147483647::numeric as n) s;
select n::ip4 from (select -2147483648::numeric as n) s;
select n::ip4 from (select 2147483647::numeric as n) s;
select n::ip4 from (select 2147483648::numeric as n) s;
select n::ip4 from (select 4294967295::numeric as n) s;

select n::ip6 from (select 0::numeric as n) s;
select n::ip6 from (select 256::numeric as n) s;
select n::ip6 from (select 65536::numeric as n) s;
select n::ip6 from (select 16777216::numeric as n) s;
select n::ip6 from (select 4294967296::numeric as n) s;
select n::ip6 from (select 281474976710656::numeric as n) s;
select n::ip6 from (select 18446744073709551616::numeric as n) s;
select n::ip6 from (select 1208925819614629174706176::numeric as n) s;
select n::ip6 from (select 79228162514264337593543950336::numeric as n) s;
select n::ip6 from (select 5192296858534827628530496329220096::numeric as n) s;
select n::ip6 from (select 170141183460469231731687303715884105728::numeric as n) s;
select n::ip6 from (select 340282366920938463463374607431768211455::numeric as n) s;

select a::numeric, a::ipaddress::numeric, a::bigint, a::float8 from (select '0.0.0.0'::ip4 as a) s;
select a::numeric, a::ipaddress::numeric, a::bigint, a::float8 from (select '255.255.255.255'::ip4 as a) s;
select a::numeric, a::ipaddress::numeric, a::bigint, a::float8 from (select '0.0.0.1'::ip4 as a) s;
select a::numeric, a::ipaddress::numeric, a::bigint, a::float8 from (select '0.0.1.0'::ip4 as a) s;
select a::numeric, a::ipaddress::numeric, a::bigint, a::float8 from (select '0.1.0.0'::ip4 as a) s;
select a::numeric, a::ipaddress::numeric, a::bigint, a::float8 from (select '1.0.0.0'::ip4 as a) s;

select a::numeric, a::ipaddress::numeric from (select '::'::ip6 as a) s;
select a::numeric, a::ipaddress::numeric from (select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff'::ip6 as a) s;
select a::numeric, a::ipaddress::numeric from (select '::1'::ip6 as a) s;
select a::numeric, a::ipaddress::numeric from (select '::1:0'::ip6 as a) s;
select a::numeric, a::ipaddress::numeric from (select '::1:0:0'::ip6 as a) s;
select a::numeric, a::ipaddress::numeric from (select '::1:0:0:0'::ip6 as a) s;
select a::numeric, a::ipaddress::numeric from (select '0:0:0:1::'::ip6 as a) s;
select a::numeric, a::ipaddress::numeric from (select '0:0:1::'::ip6 as a) s;
select a::numeric, a::ipaddress::numeric from (select '0:1::'::ip6 as a) s;
select a::numeric, a::ipaddress::numeric from (select '1::'::ip6 as a) s;

-- invalid numeric casts

select (-4294967295::bigint)::ip4;
select (4294967296::bigint)::ip4;

select 0.1::float8::ip4;
select (-0.1)::float8::ip4;
select (-4294967295::float8)::ip4;
select (4294967296::float8)::ip4;
select (-3000000000::float8)::ip4;
select 6000000000::float8::ip4;
select 6e10::float8::ip4;
select 6e30::float8::ip4;

select (-1::numeric)::ip6;
select 340282366920938463463374607431768211456::numeric::ip6;
select 0.1::numeric::ip6;
select 0.00000000000000000001::numeric::ip6;
select (-0.00000000000000000001::numeric)::ip6;

-- inet/cidr casts

select a::ip4, a::ipaddress from (select '0.0.0.0'::inet as a) s;
select a::ip4, a::ipaddress from (select '1.2.3.4'::inet as a) s;
select a::ip4, a::ipaddress from (select '255.255.255.255'::inet as a) s;
select a::ip4, a::ipaddress from (select '10.20.30.40/24'::inet as a) s;
select a::ip4, a::ipaddress from (select '10.20.30.40/16'::inet as a) s;

select a::ip6, a::ipaddress from (select '::'::inet as a) s;
select a::ip6, a::ipaddress from (select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff'::inet as a) s;
select a::ip6, a::ipaddress from (select '::1'::inet as a) s;
select a::ip6, a::ipaddress from (select '1234:2345:3456:4567:5678:6789:789a:89ab'::inet as a) s;
select a::ip6, a::ipaddress from (select '1234:2345:3456:4567:5678:6789:789a:89ab/96'::inet as a) s;
select a::ip6, a::ipaddress from (select '1234:2345:3456:4567:5678:6789:789a:89ab/64'::inet as a) s;
select a::ip6, a::ipaddress from (select '1234:2345:3456:4567:5678:6789:789a:89ab/32'::inet as a) s;

select a::ip4r, a::iprange from (select '0.0.0.0/16'::cidr as a) s;
select a::ip4r, a::iprange from (select '0.1.0.0/16'::cidr as a) s;
select a::ip4r, a::iprange from (select '1.2.3.0/24'::cidr as a) s;
select a::ip4r, a::iprange from (select '0.0.0.0/0'::cidr as a) s;

select a::ip6r, a::iprange from (select '::/0'::cidr as a) s;
select a::ip6r, a::iprange from (select 'ffff::/64'::cidr as a) s;
select a::ip6r, a::iprange from (select '0:0:0:0:ffff::/128'::cidr as a) s;
select a::ip6r, a::iprange from (select '1234:2345:3456:4567:5678:6789::/96'::cidr as a) s;

select '1.2.3.4'::ip4::cidr;
select '0.0.0.0'::ip4::cidr;
select '255.255.255.255'::ip4::cidr;

select '1.2.3.0/24'::ip4r::cidr;
select '0.0.0.0/0'::ip4r::cidr;
select '255.255.255.255/32'::ip4r::cidr;

select '1234:2345:3456:4567:5678:6789:789a:89ab'::ip6::cidr;
select '::'::ip6::cidr;
select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff'::ip6::cidr;

select '1234:2345:3456:4567:5678:6789:789a:0000/112'::ip6r::cidr;
select '::/0'::ip6r::cidr;
select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff/128'::ip6r::cidr;
select 'ffff:ffff:ffff::/48'::ip6r::cidr;

select '1.2.3.4'::ipaddress::cidr;
select '0.0.0.0'::ipaddress::cidr;
select '255.255.255.255'::ipaddress::cidr;
select '1234:2345:3456:4567:5678:6789:789a:89ab'::ipaddress::cidr;
select '::'::ipaddress::cidr;
select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff'::ipaddress::cidr;

select '1.2.3.0/24'::iprange::cidr;
select '0.0.0.0/0'::iprange::cidr;
select '255.255.255.255/32'::iprange::cidr;
select '1234:2345:3456:4567:5678:6789:789a:0000/112'::iprange::cidr;
select '::/0'::iprange::cidr;
select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff/128'::iprange::cidr;
select 'ffff:ffff:ffff::/48'::iprange::cidr;

select '-'::iprange::cidr;

-- invalid inet/cidr casts

select '::'::inet::ip4;
select '0.0.0.0'::inet::ip6;
select '::/128'::cidr::ip4r;
select '0.0.0.0/32'::cidr::ip6r;

-- invalid cross-type casts

select '::/0'::iprange::ip4r;
select '0.0.0.0/0'::iprange::ip6r;

select '::'::ipaddress::ip4;
select '0.0.0.0'::ipaddress::ip6;

-- bit casts

select (x'01020304')::ip4;
select (x'fff0fff1fff2fff3000000000000fff4')::ip6;
select (x'01020304')::ipaddress;
select (x'fff0fff1fff2fff3000000000000fff4')::ipaddress;

select (b'0001')::varbit::ip4r;
select (b'0001')::varbit::ip6r;
select (x'fff0fff1fff2fff3000000000000fff')::varbit::ip6r;

select (ip4 '1.2.3.4')::varbit;
select (ip6 'fff0:fff1:fff2:fff3::fff4')::varbit;
select (ipaddress '1.2.3.4')::varbit;
select (ipaddress 'fff0:fff1:fff2:fff3::fff4')::varbit;

select (ip4r '1.2.3.0/24')::varbit;
select (ip6r 'fff0::/12')::varbit;
select (ip6r 'fff0::/127')::varbit;

select (iprange '-')::varbit;
select (iprange '1.2.3.0/24')::varbit;
select (iprange '1.2.3.1-1.2.3.2')::varbit;
select (iprange 'fff0::/12')::varbit;
select (iprange 'fff0::0001-fff0::0002')::varbit;

-- invalid bit casts

select (x'0102030')::ip4;
select (x'0102030405')::ip4;
select (x'fff0fff1fff2fff3000000000000fff')::ip6;
select (x'fff0fff1fff2fff3000000000000fff4f')::ip6;
select (x'0102030')::ipaddress;
select (x'0102030405')::ipaddress;
select (x'fff0fff1fff2fff3000000000000fff')::ipaddress;
select (x'fff0fff1fff2fff3000000000000fff4f')::ipaddress;

-- bytea casts

select (decode('01020304','hex'))::ip4;
select (decode('fff0fff1fff2fff3000000000000fff4','hex'))::ip6;
select (decode('01020304','hex'))::ipaddress;
select (decode('fff0fff1fff2fff3000000000000fff4','hex'))::ipaddress;

select encode((ip4 '1.2.3.4')::bytea,'hex');
select encode((ip6 'fff0:fff1:fff2:fff3::fff4')::bytea,'hex');
select encode((ipaddress '1.2.3.4')::bytea,'hex');
select encode((ipaddress 'fff0:fff1:fff2:fff3::fff4')::bytea,'hex');

-- invalid bytea casts

select (decode('010203','hex'))::ip4;
select (decode('0102030405','hex'))::ip4;
select (decode('fff0fff1fff2fff3000000000000ff','hex'))::ip6;
select (decode('0102030405','hex'))::ipaddress;
select (decode('fff0fff1fff2fff3000000000000ff','hex'))::ipaddress;
select (decode('fff0fff1fff2fff3000000000000ffffff','hex'))::ipaddress;

-- constructor functions

select ip4r('0.0.0.0','255.255.255.255');
select ip4r('255.255.255.255','0.0.0.0');
select ip4r('1.2.3.4','5.6.7.8');

select ip6r('::','ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff');
select ip6r('ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff','::');
select ip6r('1234:2345:3456:4567:5678:6789:789a:89ab','ffff::ffff');

select iprange('0.0.0.0'::ip4,'255.255.255.255'::ip4);
select iprange('255.255.255.255'::ip4,'0.0.0.0'::ip4);
select iprange('1.2.3.4'::ip4,'5.6.7.8'::ip4);
select iprange('::'::ip6,'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff'::ip6);
select iprange('ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff'::ip6,'::'::ip6);
select iprange('1234:2345:3456:4567:5678:6789:789a:89ab'::ip6,'ffff::ffff'::ip6);

select iprange('0.0.0.0'::ipaddress,'255.255.255.255'::ipaddress);
select iprange('255.255.255.255'::ipaddress,'0.0.0.0'::ipaddress);
select iprange('1.2.3.4'::ipaddress,'5.6.7.8'::ipaddress);
select iprange('::'::ipaddress,'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff'::ipaddress);
select iprange('ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff'::ipaddress,'::'::ipaddress);
select iprange('1234:2345:3456:4567:5678:6789:789a:89ab'::ipaddress,'ffff::ffff'::ipaddress);

-- utility functions

-- (family, lower, upper were tested above)

select ip4_netmask(0), ip4_netmask(1), ip4_netmask(31), ip4_netmask(32);
select ip4_netmask(33);
select ip4_netmask(-1);

select ip6_netmask(0), ip6_netmask(1);
select ip6_netmask(63), ip6_netmask(64), ip6_netmask(65);
select ip6_netmask(127), ip6_netmask(128);
select ip6_netmask(129);
select ip6_netmask(-1);

select is_cidr(a), is_cidr(a::iprange), masklen(a), masklen(a::iprange) from (select '1.2.3.4-5.6.7.8'::ip4r as a) s;
select is_cidr(a), is_cidr(a::iprange), masklen(a), masklen(a::iprange) from (select '1.2.3.0-1.2.3.255'::ip4r as a) s;
select is_cidr(a), is_cidr(a::iprange), masklen(a), masklen(a::iprange) from (select '0.0.0.0/32'::ip4r as a) s;
select is_cidr(a), is_cidr(a::iprange), masklen(a), masklen(a::iprange) from (select '0.0.0.0/31'::ip4r as a) s;
select is_cidr(a), is_cidr(a::iprange), masklen(a), masklen(a::iprange) from (select '0.0.0.0/1'::ip4r as a) s;
select is_cidr(a), is_cidr(a::iprange), masklen(a), masklen(a::iprange) from (select '0.0.0.0/0'::ip4r as a) s;
select is_cidr(a), is_cidr(a::iprange), masklen(a), masklen(a::iprange) from (select '255.255.255.255/32'::ip4r as a) s;
select is_cidr(a), is_cidr(a::iprange), masklen(a), masklen(a::iprange) from (select '255.255.255.254/31'::ip4r as a) s;
select is_cidr(a), is_cidr(a::iprange), masklen(a), masklen(a::iprange) from (select '128.0.0.0/1'::ip4r as a) s;

select is_cidr(a), is_cidr(a::iprange), masklen(a), masklen(a::iprange) from (select '1234::-5678::ffff:0'::ip6r as a) s;
select is_cidr(a), is_cidr(a::iprange), masklen(a), masklen(a::iprange) from (select '::1234-::5678'::ip6r as a) s;
select is_cidr(a), is_cidr(a::iprange), masklen(a), masklen(a::iprange) from (select 'ffff::-ffff::ffff:0'::ip6r as a) s;
select is_cidr(a), is_cidr(a::iprange), masklen(a), masklen(a::iprange) from (select 'ffff::-ffff::ffff'::ip6r as a) s;
select is_cidr(a), is_cidr(a::iprange), masklen(a), masklen(a::iprange) from (select 'ffff::-ffff:0:0:0:ffff:ffff:ffff:ffff'::ip6r as a) s;
select is_cidr(a), is_cidr(a::iprange), masklen(a), masklen(a::iprange) from (select 'ffff::-ffff:0:0:1:ffff:ffff:ffff:ffff'::ip6r as a) s;
select is_cidr(a), is_cidr(a::iprange), masklen(a), masklen(a::iprange) from (select '::/128'::ip6r as a) s;
select is_cidr(a), is_cidr(a::iprange), masklen(a), masklen(a::iprange) from (select '::/127'::ip6r as a) s;
select is_cidr(a), is_cidr(a::iprange), masklen(a), masklen(a::iprange) from (select '::/1'::ip6r as a) s;
select is_cidr(a), is_cidr(a::iprange), masklen(a), masklen(a::iprange) from (select '::/0'::ip6r as a) s;
select is_cidr(a), is_cidr(a::iprange), masklen(a), masklen(a::iprange) from (select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff/128'::ip6r as a) s;
select is_cidr(a), is_cidr(a::iprange), masklen(a), masklen(a::iprange) from (select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:fffe/127'::ip6r as a) s;
select is_cidr(a), is_cidr(a::iprange), masklen(a), masklen(a::iprange) from (select 'ffff:ffff:ffff:ffff:8000::/65'::ip6r as a) s;
select is_cidr(a), is_cidr(a::iprange), masklen(a), masklen(a::iprange) from (select 'ffff:ffff:ffff:ffff::/64'::ip6r as a) s;
select is_cidr(a), is_cidr(a::iprange), masklen(a), masklen(a::iprange) from (select 'ffff:ffff:ffff:fffe::/63'::ip6r as a) s;
select is_cidr(a), is_cidr(a::iprange), masklen(a), masklen(a::iprange) from (select 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff/128'::ip6r as a) s;

select ip4_net_lower('255.255.255.255',0);
select ip4_net_lower('255.255.255.255',1);
select ip4_net_lower('255.255.255.255',24);
select ip4_net_lower('255.255.255.255',31);
select ip4_net_lower('255.255.255.255',32);
select ip4_net_lower('255.255.255.255',33);
select ip4_net_lower('255.255.255.255',-1);
select ip4_net_upper('0.0.0.0',0);
select ip4_net_upper('0.0.0.0',1);
select ip4_net_upper('0.0.0.0',24);
select ip4_net_upper('0.0.0.0',31);
select ip4_net_upper('0.0.0.0',32);
select ip4_net_upper('0.0.0.0',33);
select ip4_net_upper('0.0.0.0',-1);

select ip6_net_lower('ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff',0);
select ip6_net_lower('ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff',1);
select ip6_net_lower('ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff',63);
select ip6_net_lower('ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff',64);
select ip6_net_lower('ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff',65);
select ip6_net_lower('ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff',127);
select ip6_net_lower('ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff',128);
select ip6_net_lower('ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff',129);
select ip6_net_lower('ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff',-1);
select ip6_net_upper('::',0);
select ip6_net_upper('::',1);
select ip6_net_upper('::',63);
select ip6_net_upper('::',64);
select ip6_net_upper('::',65);
select ip6_net_upper('::',127);
select ip6_net_upper('::',128);
select ip6_net_upper('::',129);
select ip6_net_upper('::',-1);

select ipaddress_net_lower('255.255.255.255',0);
select ipaddress_net_lower('255.255.255.255',1);
select ipaddress_net_lower('255.255.255.255',24);
select ipaddress_net_lower('255.255.255.255',31);
select ipaddress_net_lower('255.255.255.255',32);
select ipaddress_net_lower('255.255.255.255',33);
select ipaddress_net_lower('255.255.255.255',-1);
select ipaddress_net_upper('0.0.0.0',0);
select ipaddress_net_upper('0.0.0.0',1);
select ipaddress_net_upper('0.0.0.0',24);
select ipaddress_net_upper('0.0.0.0',31);
select ipaddress_net_upper('0.0.0.0',32);
select ipaddress_net_upper('0.0.0.0',33);
select ipaddress_net_upper('0.0.0.0',-1);

select ipaddress_net_lower('ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff',0);
select ipaddress_net_lower('ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff',1);
select ipaddress_net_lower('ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff',63);
select ipaddress_net_lower('ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff',64);
select ipaddress_net_lower('ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff',65);
select ipaddress_net_lower('ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff',127);
select ipaddress_net_lower('ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff',128);
select ipaddress_net_lower('ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff',129);
select ipaddress_net_lower('ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff',-1);
select ipaddress_net_upper('::',0);
select ipaddress_net_upper('::',1);
select ipaddress_net_upper('::',63);
select ipaddress_net_upper('::',64);
select ipaddress_net_upper('::',65);
select ipaddress_net_upper('::',127);
select ipaddress_net_upper('::',128);
select ipaddress_net_upper('::',129);
select ipaddress_net_upper('::',-1);

select ip4r_union('1.0.0.0/24','2.0.0.0/16');
select ip4r_union('0.0.0.0/0','2.0.0.0/16');
select ip4r_union('0.0.0.0-3.0.0.0','2.0.0.0-4.0.0.0');

select ip6r_union('2000::/16','3000::/16');
select ip6r_union('2000::-4000::','3000::-5000::');
select ip6r_union('::/0','3000::-5000::');

select iprange_union('0.0.0.0/0','::/0');
select iprange_union('128.0.0.0/32','8000::/16');
select iprange_union('1.0.0.0/24','2.0.0.0/16');
select iprange_union('0.0.0.0/0','2.0.0.0/16');
select iprange_union('0.0.0.0-3.0.0.0','2.0.0.0-4.0.0.0');
select iprange_union('2000::/16','3000::/16');
select iprange_union('2000::-4000::','3000::-5000::');
select iprange_union('::/0','3000::-5000::');

select ip4r_inter('1.0.0.0/24','2.0.0.0/16');
select ip4r_inter('0.0.0.0/0','2.0.0.0/16');
select ip4r_inter('0.0.0.0-3.0.0.0','2.0.0.0-4.0.0.0');

select ip6r_inter('2000::/16','3000::/16');
select ip6r_inter('2000::-4000::','3000::-5000::');
select ip6r_inter('::/0','3000::-5000::');

select iprange_inter('0.0.0.0/0','::/0');
select iprange_inter('128.0.0.0/32','8000::/16');
select iprange_inter('1.0.0.0/24','2.0.0.0/16');
select iprange_inter('0.0.0.0/0','2.0.0.0/16');
select iprange_inter('0.0.0.0-3.0.0.0','2.0.0.0-4.0.0.0');
select iprange_inter('2000::/16','3000::/16');
select iprange_inter('2000::-4000::','3000::-5000::');
select iprange_inter('::/0','3000::-5000::');

-- split

select * from cidr_split(ip4r '1.2.3.4-5.6.7.8');
select * from cidr_split(ip4r '1.2.3.5-5.6.7.7');
select * from cidr_split(ip4r '1.0.0.0-1.0.255.255');
select * from cidr_split(ip4r '0.0.0.0-255.255.255.255');
select * from cidr_split(ip4r '0.0.0.0-0.0.0.9');
select * from cidr_split(ip4r '255.255.255.251-255.255.255.255');

select * from cidr_split(ip6r 'ffff::1234-ffff::1243');
select * from cidr_split(ip6r 'ffff:0:0:1234::-ffff:0:0:1243::');
select * from cidr_split(ip6r 'aaaa::cdef-aaaa::fedc');
select * from cidr_split(ip6r 'ffff:0:0:aaaa::/64');
select * from cidr_split(ip6r '::-::0009');
select * from cidr_split(ip6r 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:fff3-ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff');

select * from cidr_split(iprange '1.2.3.4-5.6.7.8');
select * from cidr_split(iprange '1.2.3.5-5.6.7.7');
select * from cidr_split(iprange 'ffff::1234-ffff::1243');
select * from cidr_split(iprange 'aaaa::cdef-aaaa::fedc');
select * from cidr_split(iprange '1.0.0.0/16');
select * from cidr_split(iprange 'ffff:0:0:aaaa::/64');
select * from cidr_split(iprange '-');

-- rescan

with d(a) as (values (ip4r '1.2.3.4-1.2.4.3'),(ip4r '10.2.3.5-10.2.4.4'))
select *, (select * from cidr_split(a) limit 1) as s from d;

with d(a) as (values (ip6r 'ffff::1234-ffff::1243'),(ip6r 'aaaa::cdef-aaaa::fedc'))
select *, (select * from cidr_split(a) limit 1) as s from d;

with d(a) as (values (iprange '-'),(iprange '1.2.3.4-1.2.4.3'),
                     (iprange 'aaaa::fedc-aaaa::cdef'),(iprange '-'))
select *, (select * from cidr_split(a) limit 1) as s from d;

-- operators

select ip4 '255.255.255.255' / 32;
select ip4 '255.255.255.255' / 31;
select ip4 '255.255.255.255' / 16;
select ip4 '255.255.255.255' / 0;
select ip4 '255.255.255.255' / 1;
select ip4 '255.255.255.255' / 33;
select ip4 '255.255.255.255' / -1;

select ip4 '255.255.255.255' / ip4 '255.255.255.0';
select ip4 '255.255.255.255' / ip4 '255.0.0.0';

select ip4 '255.255.255.255' / ip4 '0.255.255.255';
select ip4 '255.255.255.255' / ip4 '255.254.255.255';

select ip6 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff' / 128;
select ip6 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff' / 127;
select ip6 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff' / 65;
select ip6 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff' / 64;
select ip6 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff' / 63;
select ip6 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff' / 1;
select ip6 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff' / 0;
select ip6 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff' / 129;
select ip6 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff' / -1;

select ip6 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff' / ip6 'ffff:ffff:ffff:ffff:ffff::';
select ip6 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff' / ip6 'ffff:ffff:ffff::';

select ip6 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff' / ip6 'ffff:ffff:ffff:ffff:ffff::ffff';
select ip6 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff' / ip6 'ffff:ffff:ffff::ffff';

select ipaddress '255.255.255.255' / 32;
select ipaddress '255.255.255.255' / 31;
select ipaddress '255.255.255.255' / 16;
select ipaddress '255.255.255.255' / 0;
select ipaddress '255.255.255.255' / 1;
select ipaddress '255.255.255.255' / 33;
select ipaddress '255.255.255.255' / -1;

select ipaddress '255.255.255.255' / ipaddress '255.255.255.0';
select ipaddress '255.255.255.255' / ipaddress '255.0.0.0';

select ipaddress 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff' / 128;
select ipaddress 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff' / 127;
select ipaddress 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff' / 65;
select ipaddress 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff' / 64;
select ipaddress 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff' / 63;
select ipaddress 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff' / 1;
select ipaddress 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff' / 0;
select ipaddress 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff' / 129;
select ipaddress 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff' / -1;

select ipaddress 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff' / ipaddress 'ffff:ffff:ffff:ffff:ffff::';
select ipaddress 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff' / ipaddress 'ffff:ffff:ffff::';

select ipaddress '255.255.255.255' / ipaddress '0.255.255.255';
select ipaddress '255.255.255.255' / ipaddress '255.254.255.255';

select ipaddress 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff' / ipaddress 'ffff:ffff:ffff:ffff:ffff::ffff';
select ipaddress 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff' / ipaddress 'ffff:ffff:ffff::ffff';

select ipaddress 'ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff' / ipaddress '0.0.0.0';
select ipaddress '255.255.255.255' / ipaddress '::';

select @ a, @@ a, @ (a::iprange), @@ (a::iprange) from (select '0.0.0.0/32'::ip4r as a) s;
select @ a, @@ a, @ (a::iprange), @@ (a::iprange) from (select '0.0.0.0/31'::ip4r as a) s;
select @ a, @@ a, @ (a::iprange), @@ (a::iprange) from (select '0.0.0.0/16'::ip4r as a) s;
select @ a, @@ a, @ (a::iprange), @@ (a::iprange) from (select '0.0.0.0/1'::ip4r as a) s;
select @ a, @@ a, @ (a::iprange), @@ (a::iprange) from (select '0.0.0.0/0'::ip4r as a) s;
select @ a, @@ a, @ (a::iprange), @@ (a::iprange) from (select '1.2.3.4-5.6.7.8'::ip4r as a) s;

select @ a, @@ a, @ (a::iprange), @@ (a::iprange) from (select '::/128'::ip6r as a) s;
select @ a, @@ a, @ (a::iprange), @@ (a::iprange) from (select '::/127'::ip6r as a) s;
select @ a, @@ a, @ (a::iprange), @@ (a::iprange) from (select '::/65'::ip6r as a) s;
select @ a, @@ a, @ (a::iprange), @@ (a::iprange) from (select '::/64'::ip6r as a) s;
select @ a, @@ a, @ (a::iprange), @@ (a::iprange) from (select '::/63'::ip6r as a) s;
select @ a, @@ a, @ (a::iprange), @@ (a::iprange) from (select '::/1'::ip6r as a) s;
select @ a, @@ a, @ (a::iprange), @@ (a::iprange) from (select '::/0'::ip6r as a) s;

select @ a, @@ a, @ (a::iprange), @@ (a::iprange) from (select 'ffff:ffff:ffff:ffff::-ffff:ffff:ffff:ffff:8000::'::ip6r as a) s;
select @ a, @@ a, @ (a::iprange), @@ (a::iprange) from (select 'ffff:ffff:ffff::-ffff:ffff:ffff:8000::'::ip6r as a) s;
select @ a, @@ a, @ (a::iprange), @@ (a::iprange) from (select 'ffff:ffff::-ffff:ffff:8000::'::ip6r as a) s;
select @ a, @@ a, @ (a::iprange), @@ (a::iprange) from (select 'ffff::-ffff:8000::'::ip6r as a) s;

select @ a, @@ a from (select '-'::iprange as a) s;

-- bitwise ops

select a & b, a::ipaddress & b::ipaddress from (select ip4 '1.2.3.4' as a, ip4 '255.0.255.0' as b) s;
select a | b, a::ipaddress | b::ipaddress from (select ip4 '1.2.3.4' as a, ip4 '255.0.255.0' as b) s;
select a # b, a::ipaddress # b::ipaddress from (select ip4 '1.2.3.4' as a, ip4 '255.0.255.0' as b) s;
select ~a, ~(a::ipaddress) from (select ip4 '1.2.3.4' as a) s;


select a & b, a::ipaddress & b::ipaddress from (select ip6 '1234::5678' as a, ip6 'ffff:0000:ffff:0000:ffff:0000:ffff:0000' as b) s;
select a | b, a::ipaddress | b::ipaddress from (select ip6 '1234::5678' as a, ip6 'ffff:0000:ffff:0000:ffff:0000:ffff:0000' as b) s;
select a # b, a::ipaddress # b::ipaddress from (select ip6 '1234::5678' as a, ip6 'ffff:0000:ffff:0000:ffff:0000:ffff:0000' as b) s;
select ~a, ~(a::ipaddress) from (select ip6 '1234::5678' as a) s;

-- arithmetic

select a + 1234, a::ipaddress + 1234 from (select ip4 '0.0.0.0' as a) s;
select a + 1, a::ipaddress + 1 from (select ip4 '255.255.255.254' as a) s;
select a + 1 from (select ip4 '255.255.255.255' as a) s;
select a::ipaddress + 1 from (select ip4 '255.255.255.255' as a) s;

select a - 1234, a::ipaddress - 1234 from (select ip4 '1.0.0.0' as a) s;
select a - 1, a::ipaddress - 1 from (select ip4 '0.0.0.1' as a) s;
select a - 1 from (select ip4 '0.0.0.0' as a) s;
select a::ipaddress - 1 from (select ip4 '0.0.0.0' as a) s;

select a + 1234::bigint, a::ipaddress + 1234::bigint from (select ip4 '0.0.0.0' as a) s;
select a + 1::bigint, a::ipaddress + 1::bigint from (select ip4 '255.255.255.254' as a) s;
select a + 1::bigint from (select ip4 '255.255.255.255' as a) s;
select a::ipaddress + 1::bigint from (select ip4 '255.255.255.255' as a) s;
select a + 4294967296::bigint from (select ip4 '0.0.0.0' as a) s;
select a::ipaddress + 4294967296::bigint from (select ip4 '0.0.0.0' as a) s;

select a - 1234::bigint, a::ipaddress - 1234::bigint from (select ip4 '1.0.0.0' as a) s;
select a - 1::bigint, a::ipaddress - 1::bigint from (select ip4 '0.0.0.1' as a) s;
select a - 1::bigint from (select ip4 '0.0.0.0' as a) s;
select a::ipaddress - 1::bigint from (select ip4 '0.0.0.0' as a) s;
select a - 4294967296::bigint from (select ip4 '255.255.255.255' as a) s;
select a::ipaddress - 4294967296::bigint from (select ip4 '255.255.255.255' as a) s;

select a + 1234::numeric, a::ipaddress + 1234::numeric from (select ip4 '0.0.0.0' as a) s;
select a + 1::numeric, a::ipaddress + 1::numeric from (select ip4 '255.255.255.254' as a) s;
select a + 1::numeric from (select ip4 '255.255.255.255' as a) s;
select a::ipaddress + 1::numeric from (select ip4 '255.255.255.255' as a) s;
select a + 4294967296::numeric from (select ip4 '0.0.0.0' as a) s;
select a::ipaddress + 4294967296::numeric from (select ip4 '0.0.0.0' as a) s;

select a - 1234::numeric, a::ipaddress - 1234::numeric from (select ip4 '1.0.0.0' as a) s;
select a - 1::numeric, a::ipaddress - 1::numeric from (select ip4 '0.0.0.1' as a) s;
select a - 1::numeric from (select ip4 '0.0.0.0' as a) s;
select a::ipaddress - 1::numeric from (select ip4 '0.0.0.0' as a) s;
select a - 4294967296::bigint from (select ip4 '255.255.255.255' as a) s;
select a::ipaddress - 4294967296::bigint from (select ip4 '255.255.255.255' as a) s;

-- predicates and indexing

create table ipranges (r iprange, r4 ip4r, r6 ip6r) distributed by (r);

insert into ipranges
select r, null, r
  from (select ip6r(regexp_replace(ls, E'(....(?!$))', E'\\1:', 'g')::ip6,
                    regexp_replace(substring(ls for n+1) || substring(us from n+2),
                                   E'(....(?!$))', E'\\1:', 'g')::ip6) as r
          from (select md5(i || ' lower 1') as ls,
                       md5(i || ' upper 1') as us,
                       (i % 11) + (i/11 % 11) + (i/121 % 11) as n
                  from generate_series(1,13310) i) s1) s2;

insert into ipranges
select r, r, null
  from (select ip4r(ip4 '0.0.0.0' + ((la & '::ffff:ffff') - ip6 '::'),
                    ip4 '0.0.0.0' + ((( (la & ip6_netmask(127-n)) | (ua & ~ip6_netmask(127-n)) ) & '::ffff:ffff') - ip6 '::')) as r
          from (select regexp_replace(md5(i || ' lower 2'), E'(....(?!$))', E'\\1:', 'g')::ip6 as la,
                       regexp_replace(md5(i || ' upper 2'), E'(....(?!$))', E'\\1:', 'g')::ip6 as ua,
                       (i % 11) + (i/11 % 11) + (i/121 % 11) as n
                  from generate_series(1,1331) i) s1) s2;

insert into ipranges
select r, null, r
  from (select n::ip6 / 68 as r
          from (select ((8192 + i/256)::numeric * (2::numeric ^ 112)
                       + (131072 + (i % 256))::numeric * (2::numeric ^ 60)) as n
                  from generate_series(0,4095) i) s1) s2;

insert into ipranges
select r, r, null
  from (select n / 28 as r
          from (select ip4 '172.16.0.0' + (i * 256) as n
                  from generate_series(0,4095) i) s1) s2;

insert into ipranges
select r, null, r
  from (select n::ip6 / 48 as r
          from (select ((8192 + i/256)::numeric * (2::numeric ^ 112)
                       + (i % 256)::numeric * (2::numeric ^ 84)) as n
                  from generate_series(0,4095) i) s1) s2;

insert into ipranges
select r, r, null
  from (select n / 16 as r
          from (select ip4 '128.0.0.0' + (i * 65536) as n
                  from generate_series(0,4095) i) s1) s2;

insert into ipranges values ('-',null,null);

create table ipaddrs (a ipaddress, a4 ip4, a6 ip6) distributed by (a);

insert into ipaddrs
select a, null, a
  from (select regexp_replace(md5(i || ' address 1'), E'(....(?!$))', E'\\1:', 'g')::ip6 as a
          from generate_series(1,256) i) s1;

insert into ipaddrs
select a, a, null
  from (select ip4 '0.0.0.0' + ((regexp_replace(md5(i || ' address 1'), E'(....(?!$))', E'\\1:', 'g')::ip6 & '::ffff:ffff') - '::') as a
          from generate_series(1,16) i) s1;

select * from ipranges where r >>= '5555::' order by r;
select * from ipranges where r <<= '5555::/16' order by r;
select * from ipranges where r && '5555::/16' order by r;
select * from ipranges where r6 >>= '5555::' order by r6;
select * from ipranges where r6 <<= '5555::/16' order by r6;
select * from ipranges where r6 && '5555::/16' order by r6;
select * from ipranges where r >>= '172.16.2.0' order by r;
select * from ipranges where r <<= '10.0.0.0/12' order by r;
select * from ipranges where r && '10.128.0.0/12' order by r;
select * from ipranges where r4 >>= '172.16.2.0' order by r4;
select * from ipranges where r4 <<= '10.0.0.0/12' order by r4;
select * from ipranges where r4 && '10.128.0.0/12' order by r4;

select * from ipranges where r >>= '2001:0:0:2000:a123::' order by r;
select * from ipranges where r >>= '2001:0:0:2000::' order by r;
select * from ipranges where r >>= '2001:0:0:2000::/68' order by r;
select * from ipranges where r >> '2001:0:0:2000::/68' order by r;

select * from ipranges where r6 >>= '2001:0:0:2000:a123::' order by r6;
select * from ipranges where r6 >>= '2001:0:0:2000::' order by r6;
select * from ipranges where r6 >>= '2001:0:0:2000::/68' order by r6;
select * from ipranges where r6 >> '2001:0:0:2000::/68' order by r6;

select * from ipranges where r4 >>= '172.16.2.0/28' order by r4;
select * from ipranges where r4 >> '172.16.2.0/28' order by r4;

select * from ipaddrs where a between '8.0.0.0' and '15.0.0.0' order by a;
select * from ipaddrs where a4 between '8.0.0.0' and '15.0.0.0' order by a4;

create index ipranges_r on ipranges using gist (r);
create index ipranges_r4 on ipranges using gist (r4);
create index ipranges_r6 on ipranges using gist (r6);
create index ipaddrs_a on ipaddrs (a);
create index ipaddrs_a4 on ipaddrs (a4);
create index ipaddrs_a6 on ipaddrs (a6);

select * from ipranges where r >>= '5555::' order by r;
select * from ipranges where r <<= '5555::/16' order by r;
select * from ipranges where r && '5555::/16' order by r;
select * from ipranges where r6 >>= '5555::' order by r6;
select * from ipranges where r6 <<= '5555::/16' order by r6;
select * from ipranges where r6 && '5555::/16' order by r6;
select * from ipranges where r >>= '172.16.2.0' order by r;
select * from ipranges where r <<= '10.0.0.0/12' order by r;
select * from ipranges where r && '10.128.0.0/12' order by r;
select * from ipranges where r4 >>= '172.16.2.0' order by r4;
select * from ipranges where r4 <<= '10.0.0.0/12' order by r4;
select * from ipranges where r4 && '10.128.0.0/12' order by r4;

select * from ipranges where r >>= '2001:0:0:2000:a123::' order by r;
select * from ipranges where r >>= '2001:0:0:2000::' order by r;
select * from ipranges where r >>= '2001:0:0:2000::/68' order by r;
select * from ipranges where r >> '2001:0:0:2000::/68' order by r;

select * from ipranges where r6 >>= '2001:0:0:2000:a123::' order by r6;
select * from ipranges where r6 >>= '2001:0:0:2000::' order by r6;
select * from ipranges where r6 >>= '2001:0:0:2000::/68' order by r6;
select * from ipranges where r6 >> '2001:0:0:2000::/68' order by r6;

select * from ipranges where r4 >>= '172.16.2.0/28' order by r4;
select * from ipranges where r4 >> '172.16.2.0/28' order by r4;

select * from ipaddrs where a between '8.0.0.0' and '15.0.0.0' order by a;
select * from ipaddrs where a4 between '8.0.0.0' and '15.0.0.0' order by a4;

select * from ipaddrs a join ipranges r on (r.r >>= a.a) order by a,r;
select * from ipaddrs a join ipranges r on (r.r4 >>= a.a4) order by a4,r4;
select * from ipaddrs a join ipranges r on (r.r6 >>= a.a6) order by a6,r6;

-- index-only, on versions that support it:

vacuum ipranges;
analyze ipranges;

select r from ipranges where r >>= '5555::' order by r;
select r6 from ipranges where r6 >>= '5555::' order by r6;
select r4 from ipranges where r4 >>= '172.16.2.0' order by r4;

-- hashing

select lower(a / 4), count(*) from ipaddrs group by 1 order by 2,1;
select a4 & '240.0.0.0', count(*) from ipaddrs group by 1 order by 2,1;
select a6 & 'f000::', count(*) from ipaddrs group by 1 order by 2,1;

select a / 4, count(*) from ipaddrs group by 1 order by 2,1;
select a4 / 4, count(*) from ipaddrs group by 1 order by 2,1;
select a6 / 4, count(*) from ipaddrs group by 1 order by 2,1;

-- comparison ops

select
  sum((r < '2000::/48')::integer) as s_lt,
  sum((r <= '2000::/48')::integer) as s_le,
  sum((r = '2000::/48')::integer) as s_eq,
  sum((r >= '2000::/48')::integer) as s_ge,
  sum((r > '2000::/48')::integer) as s_gt,
  sum((r <> '2000::/48')::integer) as s_ne
  from ipranges;

select
  sum((r6 < '2000::/48')::integer) as s_lt,
  sum((r6 <= '2000::/48')::integer) as s_le,
  sum((r6 = '2000::/48')::integer) as s_eq,
  sum((r6 >= '2000::/48')::integer) as s_ge,
  sum((r6 > '2000::/48')::integer) as s_gt,
  sum((r6 <> '2000::/48')::integer) as s_ne
  from ipranges;

select
  sum((r4 < '172.16.0.0/28')::integer) as s_lt,
  sum((r4 <= '172.16.0.0/28')::integer) as s_le,
  sum((r4 = '172.16.0.0/28')::integer) as s_eq,
  sum((r4 >= '172.16.0.0/28')::integer) as s_ge,
  sum((r4 > '172.16.0.0/28')::integer) as s_gt,
  sum((r4 <> '172.16.0.0/28')::integer) as s_ne
  from ipranges;

select
  sum((a < '8ecc:14db:7aba:bbde:f2a7:c4bc:7a1e:c8c1')::integer) as s_lt,
  sum((a <= '8ecc:14db:7aba:bbde:f2a7:c4bc:7a1e:c8c1')::integer) as s_le,
  sum((a = '8ecc:14db:7aba:bbde:f2a7:c4bc:7a1e:c8c1')::integer) as s_eq,
  sum((a >= '8ecc:14db:7aba:bbde:f2a7:c4bc:7a1e:c8c1')::integer) as s_ge,
  sum((a > '8ecc:14db:7aba:bbde:f2a7:c4bc:7a1e:c8c1')::integer) as s_gt,
  sum((a <> '8ecc:14db:7aba:bbde:f2a7:c4bc:7a1e:c8c1')::integer) as s_ne
  from ipaddrs;

select
  sum((a6 < '8ecc:14db:7aba:bbde:f2a7:c4bc:7a1e:c8c1')::integer) as s_lt,
  sum((a6 <= '8ecc:14db:7aba:bbde:f2a7:c4bc:7a1e:c8c1')::integer) as s_le,
  sum((a6 = '8ecc:14db:7aba:bbde:f2a7:c4bc:7a1e:c8c1')::integer) as s_eq,
  sum((a6 >= '8ecc:14db:7aba:bbde:f2a7:c4bc:7a1e:c8c1')::integer) as s_ge,
  sum((a6 > '8ecc:14db:7aba:bbde:f2a7:c4bc:7a1e:c8c1')::integer) as s_gt,
  sum((a6 <> '8ecc:14db:7aba:bbde:f2a7:c4bc:7a1e:c8c1')::integer) as s_ne
  from ipaddrs;

select
  sum((a4 < '104.175.11.72')::integer) as s_lt,
  sum((a4 <= '104.175.11.72')::integer) as s_le,
  sum((a4 = '104.175.11.72')::integer) as s_eq,
  sum((a4 >= '104.175.11.72')::integer) as s_ge,
  sum((a4 > '104.175.11.72')::integer) as s_gt,
  sum((a4 <> '104.175.11.72')::integer) as s_ne
  from ipaddrs;

-- end
