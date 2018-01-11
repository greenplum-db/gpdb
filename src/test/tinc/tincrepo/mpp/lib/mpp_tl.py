"""
Copyright (c) 2004-Present Pivotal Software, Inc.

This program and the accompanying materials are made available under
the terms of the under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""

from mpp.models.mpp_tc import _MPPMetaClassType

class MPPTestLib(object):
    # MPPTestLib class is of type MPPMetaClassType
    # MPPMetaClassType will take of reconfiguring the bases of all the derived classes that have product-specific hidden libraries
    __metaclass__ = _MPPMetaClassType
    def __init__(self):
        self.make_me_product_agnostic()
        super(MPPTestLib, self).__init__()

class __gpdbMPPTestLib__(MPPTestLib):
    pass
class __hawqMPPTestLib__(MPPTestLib):
    pass
