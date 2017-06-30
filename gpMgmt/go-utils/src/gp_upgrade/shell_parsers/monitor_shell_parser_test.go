package shell_parsers_test

import (
	"gp_upgrade/shell_parsers"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
)

var _ = Describe("ShellParser", func() {

	Describe("#isPgUpgradeRunning", func() {
		var subject shell_parsers.ShellParser
		BeforeEach(func() {
			subject = shell_parsers.RealShellParser{}
		})

		Context("when ShellParser has no ps output", func() {
			It("returns false", func() {
				return_value := subject.IsPgUpgradeRunning(7, "")
				Expect(return_value).To(BeFalse())
			})
		})

		Context("when ShellParser has ps output that contains pg_upgrade but isn't a pg_upgrade process", func() {
			It("returns false", func() {
				return_value := subject.IsPgUpgradeRunning(7,
					"gpadmin            7520   0.0  0.0  2432772    676 s004  S+    3:56PM   0:00.00 grep pg_upgrade")
				Expect(return_value).To(BeFalse())
			})
		})

		Context("when ShellParser has ps output with pg_upgrade running", func() {
			It("returns true when target port matches", func() {
				return_value := subject.IsPgUpgradeRunning(25437,
					`gpadmin            7520   0.0  0.0  2432772    676 s004  S+    3:56PM   0:00.00 grep pg_upgrade
pg_upgrade --verbose  --old-bindir /usr/local/greenplum-db-4.3.9.1/bin --new-bindir  /usr/local/greenplum-db-5/bin --old-datadir /data/gpdata/master/gpseg-1 --new-datadir /data/gp5data/master/gpseg-1 --old-port 25437 --new-port 6543 --link
`)
				Expect(return_value).To(BeTrue())
			})

			It("returns false when target port does not match", func() {
				return_value := subject.IsPgUpgradeRunning(25437,
					`gpadmin            7520   0.0  0.0  2432772    676 s004  S+    3:56PM   0:00.00 grep pg_upgrade
pg_upgrade --verbose  --old-bindir /usr/local/greenplum-db-4.3.9.1/bin --new-bindir  /usr/local/greenplum-db-5/bin --old-datadir /data/gpdata/master/gpseg-1 --new-datadir /data/gp5data/master/gpseg-1 --old-port 404 --new-port 6543 --link
`)
				Expect(return_value).To(BeFalse())
			})
		})
	})
})
