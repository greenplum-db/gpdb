[logging]
default = FILE:/var/log/krb5libs.log
kdc = FILE:/var/log/krb5kdc.log
admin_server = FILE:/var/log/kadmind.log
[libdefaults]
default_realm = HD.PIVOTAL.COM
dns_lookup_realm = false
dns_lookup_kdc = false
ticket_lifetime = 24h
forwardable = yes
udp_preference_limit = 1

[realms]
HD.PIVOTAL.COM = {
kdc = %HOSTNAME%:88
admin_server = %HOSTNAME%:749
default_domain = %DOMAIN%
}

[domain_realm]
.%DOMAIN% = HD.PIVOTAL.COM
%DOMAIN% = HD.PIVOTAL.COM
[appdefaults]

pam = {
debug = false
ticket_lifetime = 36000
renew_lifetime = 36000
forwardable = true
krb4_convert = false
}