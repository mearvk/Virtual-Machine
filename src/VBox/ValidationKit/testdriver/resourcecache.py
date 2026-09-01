# -*- coding: utf-8 -*-
# $Id: resourcecache.py 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $
# pylint: disable=too-many-lines

"""
Simple local resource cache layer
"""

__copyright__ = \
"""
Copyright (C) 2025 Oracle and/or its affiliates.

This file is part of VirtualBox base platform packages, as
available from https://www.virtualbox.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation, in version 3 of the
License.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <https://www.gnu.org/licenses>.

The contents of this file may alternatively be used under the terms
of the Common Development and Distribution License Version 1.0
(CDDL), a copy of it is provided in the "COPYING.CDDL" file included
in the VirtualBox distribution, in which case the provisions of the
CDDL are applicable instead of those of the GPL.

You may elect to license modified versions of this file under the
terms and conditions of either the GPL or the CDDL or both.

SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
"""
__version__ = "$Revision: 111747 $"


# Standard Python imports.
import json;
import os;
import shutil;

from collections import OrderedDict

# Validation Kit imports.
from testdriver import reporter;


class LocalRsrcCache(object):
    """
    A local resource cache class.
    """

    def __init__(self, sResourcePath, sLocalCachePath, cbCacheMax):
        """
        Constructs the cache object, initializing the cache.
        """
        # Initialize all members first.
        self.sResourcePath   = sResourcePath;
        self.sLocalCachePath = sLocalCachePath;
        self.cbCacheMax      = cbCacheMax;
        self.oCacheLru       = OrderedDict();
        self.cbCache         = 0;

        # Try loading the cache TOC
        sCacheToc = os.path.join(self.sLocalCachePath, 'cache-toc.json');
        fRc = True;
        try:
            fRc = os.path.isfile(sCacheToc);
            if fRc:
                oTocJson = None;
                with open(sCacheToc, encoding = 'utf-8') as oFileToc:
                    oTocJson = json.load(oFileToc);
                if oTocJson is not None:
                    for sItem in oTocJson:
                        sPath = os.path.join(self.sLocalCachePath, sItem);
                        cbObj = os.path.getsize(sPath);
                        self.cbCache += cbObj;
                        self.oCacheLru[sItem] = cbObj;
                else:
                    fRc = False;
        except:
            reporter.logXcpt();
            fRc = False;

        if not fRc:
            reporter.log('Couldn\'t load cache TOC from %s, clearing directory and starting over...' % (sCacheToc,));
            shutil.rmtree(self.sLocalCachePath, True);
            self.oCacheLru = OrderedDict();
            self.cbCache   = 0;

    def writeToc(self):
        """
        Cleans up the cache, writing the TOC file.
        """
        asToc = [];
        for sKey, _ in self.oCacheLru.items():
            asToc.append(sKey);

        sCacheToc = os.path.join(self.sLocalCachePath, 'cache-toc.json');
        with open(sCacheToc, 'w', encoding = 'utf-8') as oFileToc:
            json.dump(asToc, oFileToc, ensure_ascii = False, indent = 4);

    def getCachedResource(self, sName):
        """
        Tries to fetch the resource from the cache, copying it over if not existing.
        """
        sCachePath = os.path.join(self.sLocalCachePath, sName);
        if os.path.exists(sCachePath):
            # Resource is already cached, return this variant and place it at the top of the cache.
            cbObj = self.oCacheLru.pop(sName);
            self.oCacheLru[sName] = cbObj;
            self.writeToc();
            return sCachePath;

        # Cache it
        sResourcePath = os.path.join(self.sResourcePath, sName);
        if os.path.exists(sResourcePath):
            cbObj = os.path.getsize(sResourcePath);
            # No point in caching if the object exceeds the limit
            if cbObj > self.cbCacheMax:
                return sResourcePath;

            # Need to make room in the cache?
            if self.cbCache + cbObj > self.cbCacheMax:
                cbEvict = (self.cbCache + cbObj) - self.cbCacheMax;
                while cbEvict > 0:
                    sCachedPath, cbCachedObj = self.oCacheLru.popitem(False);
                    os.remove(os.path.join(self.sLocalCachePath, sCachedPath));
                    self.cbCache = self.cbCache - cbCachedObj;
                    cbEvict = cbEvict - min(cbEvict, cbCachedObj);

            sCachedPath = os.path.join(self.sLocalCachePath, sName);
            reporter.log('Caching %s (%d) at %s...' % (sResourcePath, cbObj, sCachedPath));

            # Create all non existant sub-directories
            sCacheDirPath = os.path.dirname(sCachedPath);
            if not os.path.exists(sCacheDirPath):
                os.makedirs(sCacheDirPath);

            shutil.copyfile(sResourcePath, sCachedPath);
            self.oCacheLru[sName] = cbObj;
            self.writeToc();
            return sCachedPath;

        return sResourcePath;

