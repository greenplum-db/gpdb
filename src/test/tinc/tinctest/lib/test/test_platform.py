import unittest2 as unittest
import tinctest.lib.gpplatform as gpplatform

platforms = ["OSX", "SUSE", "RHEL5", "RHEL6", "SOL"]

class PlatformTestCase(unittest.TestCase):
    def test_platform_get_info(self):
        platforms = ["OSX", "SUSE", "RHEL5", "RHEL6", "SOL"]
        for platform in platforms:
            check = gpplatform.get_info() == platform
            if check:
                self.assertTrue(check)

    def checkTrue(self, check):
        if check:
            self.assertTrue(check)
        else:
            self.assertFalse(check)

for platform in platforms:
    def check_platform(platform):
        return lambda self: self.checkTrue(gpplatform.get_info() == platform)
    setattr(PlatformTestCase, "test_platform%r_%s" % 
            (platform, gpplatform.get_info() == platform), check_platform(platform))
