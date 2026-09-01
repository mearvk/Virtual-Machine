#!/usr/bin/env python
# -*- coding: utf-8 -*-
# $Id: tstArm64-1-codegen.py 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $
# pylint: disable=invalid-name

"""
tstArm64-1 code generator.
"""

from __future__ import print_function;

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

SPDX-License-Identifier: GPL-3.0-only
"""
__version__ = "$Revision: 111747 $"

# pylint: enable=invalid-name

# Standard python imports.
import argparse;
import random;
import os;
import sys;



# Imports from the VMMAll directory.
sys.path.append(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'VMMAll'));
import PyCommonVmm as pycmn;    # pylint: disable=import-error


#
# Utility functions.
#

def isPowerOfTwo(uValue):
    """ Check if 'uValue' is a power of two. """
    return (uValue & (uValue - 1)) == 0 and uValue != 0;

def bitsOnes(cBits):
    """ Returns a value with 'cBits' ones in it. """
    return (1<<cBits) - 1;

def bitsReplicate(cBits, uValue, cTimes):
    """ Return the 'uValue' ('cBits' wide) replicated 'cTimes' times. """
    uRet = uValue;
    while cTimes > 1:
        cTimes -= 1;
        uRet <<= cBits;
        uRet  |= uValue;
    return uRet;

def bitsSignedToInt(cBits, uValue):
    """ Returns a signed integer value. """
    fSignFlag = 1 << (cBits - 1);
    if uValue & fSignFlag:
        return -(~uValue & (fSignFlag - 1)) - 1;
    return uValue;

def bitsSignExtend(cBits, uValue, cToBits):
    """ Returns the low `cBits` bits of `uValue` signextended to `cToBits` bits. """
    assert cToBits > cBits;
    uValue &= bitsOnes(cBits);
    if uValue & (1 << (cBits - 1)):
        uValue |= bitsOnes(cToBits - cBits) << cBits;
    return uValue;

def bitsRor(cBits, uValue, cShift):
    """ Rotates 'uValue' ('cBits' wide) 'cShift' times to the right. """
    assert isPowerOfTwo(cBits);
    cShift &= cBits - 1;
    fMask   = bitsOnes(cBits);
    return ((uValue & fMask) >> cShift) | ((uValue << (cBits - cShift)) & fMask);

## The random number generator we use.
g_oRandom = random.Random();

def randBool():
    return g_oRandom.randrange(0, 2) == 1;

def randU8():
    return g_oRandom.randrange(0, 1<<8);

def randU16():
    return g_oRandom.randrange(0, 1<<16);

def randU32():
    return g_oRandom.randrange(0, 1<<32);

def randU64():
    return g_oRandom.randrange(0, 1<<64);

def randUx(iLast):
    return g_oRandom.randrange(0, iLast+1);

def randUBits(cBits):
    return g_oRandom.randrange(0, 1 << cBits);

def randURange(uFirst, uEnd): # uEnd is exclusive
    return g_oRandom.randrange(uFirst, uEnd);


#
# Register allocators.
#

class Arm64RegAllocator(object):
    """ This is mainly for random register allocating. """
    def __init__(self, cMax = 32, bmPreallocated = 0):
        self.cMax        = cMax;
        self.bmAllocated = bmPreallocated;
        self.auValues    = [None for _ in range(cMax)];

    def allocOnly(self):
        iReg      = randUx(self.cMax - 1);
        fRegMask  = 1 << iReg;
        iStartReg = iReg;
        while self.bmAllocated & fRegMask:
            iReg = (iReg + 1) % self.cMax;
            assert iReg != iStartReg;
            fRegMask = 1 << iReg;
        self.bmAllocated |= fRegMask;
        return iReg;

    def alloc(self, uValue = None):
        """ Allocates a random register. """
        iReg = self.allocOnly();
        self.auValues[iReg] = uValue;
        return iReg;

    def allocFixed(self, iReg, uValue = None):
        """ Allocates a specific register. """
        assert 0 <= iReg < self.cMax;
        fRegMask = 1 << iReg;
        assert not (self.bmAllocated & fRegMask);
        self.bmAllocated   |= fRegMask;
        self.auValues[iReg] = uValue;
        return iReg;

    def updateValue(self, iReg, uValue):
        """ Updates the value of an allocated register. """
        assert self.bmAllocated & (1 << iReg);
        self.auValues[iReg] = uValue;
        return uValue;

    def free(self, iReg):
        """ Frees a previously allocated register. """
        if iReg != -1:
            assert 0 <= iReg < self.cMax;
            fRegMask = 1 << iReg;
            assert self.bmAllocated & fRegMask;
            self.bmAllocated &= ~fRegMask;
        return None;

    def freeList(self, aiRegs):
        for iReg in aiRegs:
            self.free(iReg);
        return None;


class Arm64GprAllocator(Arm64RegAllocator):
    """ GPR allocator.  This excludes the SP/XZR register. """

    def __init__(self, bmPreallocated = 0):
        Arm64RegAllocator.__init__(self, cMax = 32, bmPreallocated = bmPreallocated);
        self.bmAllocated |= 1 << 18; # PR - platform register.

    def allocEx(self, uValue = None, fIncludingReg31 = True, fReg31IsSp = False):
        iReg = self.allocOnly();
        if iReg == 31 and not fIncludingReg31:
            iReg = self.allocOnly();
            self.free(31);

        if iReg != 31 or fReg31IsSp:
            self.auValues[iReg] = uValue;
        return iReg;

    def alloc(self, uValue = None):
        return self.allocEx(uValue, fIncludingReg31 = True, fReg31IsSp = False);

    def allocNo31(self, uValue = None):
        return self.allocEx(uValue = uValue, fIncludingReg31 = False)



g_dGpr64NamesZr = {
    0:  'x0',       1:  'x1',       2:  'x2',       3:  'x3',       4:  'x4',       5:  'x5',       6:  'x6',       7:  'x7',
    8:  'x8',       9:  'x9',       10: 'x10',      11: 'x11',      12: 'x12',      13: 'x13',      14: 'x14',      15: 'x15',
    16: 'x16',      17: 'x17',      18: 'x18',      19: 'x19',      20: 'x20',      21: 'x21',      22: 'x22',      23: 'x23',
    24: 'x24',      25: 'x25',      26: 'x26',      27: 'x27',      28: 'x28',      29: 'x29',      30: 'x30',      31: 'xzr',
};
g_dGpr32NamesZr = {
    0:  'w0',       1:  'w1',       2:  'w2',       3:  'w3',       4:  'w4',       5:  'w5',       6:  'w6',       7:  'w7',
    8:  'w8',       9:  'w9',       10: 'w10',      11: 'w11',      12: 'w12',      13: 'w13',      14: 'w14',      15: 'w15',
    16: 'w16',      17: 'w17',      18: 'w18',      19: 'w19',      20: 'w20',      21: 'w21',      22: 'w22',      23: 'w23',
    24: 'w24',      25: 'w25',      26: 'w26',      27: 'w27',      28: 'w28',      29: 'w29',      30: 'w30',      31: 'wzr',
};
g_ddGprNamesZrByBits = { 64: g_dGpr64NamesZr, 32: g_dGpr32NamesZr };

g_dGpr64NamesSp = {
    0:  'x0',       1:  'x1',       2:  'x2',       3:  'x3',       4:  'x4',       5:  'x5',       6:  'x6',       7:  'x7',
    8:  'x8',       9:  'x9',       10: 'x10',      11: 'x11',      12: 'x12',      13: 'x13',      14: 'x14',      15: 'x15',
    16: 'x16',      17: 'x17',      18: 'x18',      19: 'x19',      20: 'x20',      21: 'x21',      22: 'x22',      23: 'x23',
    24: 'x24',      25: 'x25',      26: 'x26',      27: 'x27',      28: 'x28',      29: 'x29',      30: 'x30',      31: 'sp',
};
g_dGpr32NamesSp = {
    0:  'w0',       1:  'w1',       2:  'w2',       3:  'w3',       4:  'w4',       5:  'w5',       6:  'w6',       7:  'w7',
    8:  'w8',       9:  'w9',       10: 'w10',      11: 'w11',      12: 'w12',      13: 'w13',      14: 'w14',      15: 'w15',
    16: 'w16',      17: 'w17',      18: 'w18',      19: 'w19',      20: 'w20',      21: 'w21',      22: 'w22',      23: 'w23',
    24: 'w24',      25: 'w25',      26: 'w26',      27: 'w27',      28: 'w28',      29: 'w29',      30: 'w30',      31: 'wsp',
};
g_ddGprNamesSpByBits = { 64: g_dGpr64NamesSp, 32: g_dGpr32NamesSp };

g_dddGprNamesBySpAndBits = {
    True:  g_ddGprNamesSpByBits,
    False: g_ddGprNamesZrByBits,
};


class Arm64FpRegAllocator(Arm64RegAllocator):
    """ FPU/SIMD allocator. """

    def __init__(self):
        Arm64RegAllocator.__init__(self, cMax = 32);


#
# Code generators
#

## Immediate value for BRK. Incremented for each instruction we emit.
g_iBrkNo = 1;
## Immediate value for BRK. Incremented for each instruction we emit.
g_iLabel = 100;

## Counters for each instruction test name used to generate unique label names.
g_dNameSeqNumbers = {}

class A64No1CodeGenBase(object):

    def __init__(self, sName, sInstr, oGprAllocator, fMayUseSp = False):
        assert isinstance(oGprAllocator, Arm64GprAllocator)
        self.sName         = sName;
        self.sInstr        = sInstr;
        self.oGprAllocator = oGprAllocator;
        self.iRegDataPtr   = oGprAllocator.allocNo31();
        self.fMayUseSp     = fMayUseSp;
        self.asCode        = [];        # Assembly code lines.
        self.asData        = [];        # Assembly data lines.
        self.cbLastData    = -1;
        self.cLastDataItems = 0;
        self.abData        = b'';

        self.sLabel        = sName;
        if sName not in g_dNameSeqNumbers:
            g_dNameSeqNumbers[sName] = 1;
        else:
            self.sLabel    = '%s_%u' % (sName, g_dNameSeqNumbers[sName]);
            g_dNameSeqNumbers[sName] += 1;


    def generate(self, oOptions):
        self.asCode += [
            '/* %s / %s */' % (self.sName, self.sInstr,),
            'BEGINPROC %s' % (self.sLabel,),
        ];
        self.emitInstr('stp',   'fp, lr, [sp, #-64]!');
        if self.fMayUseSp:
            self.asCode.append('/* Save SP so we can use it in the test: */');
        else:
            self.asCode.append('/* Save SP so we can check its value while testing: */');
        self.emitInstr('adrp',  'x%u, PAGE(g_u64SavedSp)' % (self.iRegDataPtr,));
        self.emitInstr('add',   'x%u, x%u, PAGEOFF(g_u64SavedSp)' % (self.iRegDataPtr, self.iRegDataPtr,));
        iTmp = self.oGprAllocator.allocNo31();
        self.emitInstr('mov',   'x%u, sp' % (iTmp,));
        self.emitInstr('str',   'x%u, [x%u]' % (iTmp, self.iRegDataPtr));
        self.oGprAllocator.free(iTmp);
        self.asCode.append('');

        self.emitInstr('adrp',  'x%u, PAGE(g_DataEnd_%s)' % (self.iRegDataPtr, self.sLabel,));
        self.emitInstr('add',   'x%u, x%u, PAGEOFF(g_DataEnd_%s)' % (self.iRegDataPtr, self.iRegDataPtr, self.sLabel,));
        self.asCode.append('');

        if self.fMayUseSp:
            uSpFiller = randU64() & ~15; # load known value into SP.
            self.oGprAllocator.free(self.emitGprLoad(self.oGprAllocator.allocFixed(31, uSpFiller), uSpFiller, fReg31IsSp = True));
            self.asCode.append('');

        self.generateBody(oOptions, oOptions.cCheckAllRegsInterval);

        self.asCode.append('');
        if self.fMayUseSp:
            self.asCode.append('/* Restore SP: */');
            self.emitInstr('adrp',  'x%u, PAGE(g_u64SavedSp)' % (self.iRegDataPtr,));
            self.emitInstr('add',   'x%u, x%u, PAGEOFF(g_u64SavedSp)' % (self.iRegDataPtr, self.iRegDataPtr,));
            self.emitInstr('ldr',   'x%u, [x%u]' % (self.iRegDataPtr, self.iRegDataPtr));
            self.emitInstr('mov',   'sp, x%u' % (self.iRegDataPtr,));
        self.asCode.append('');
        self.emitInstr('ldp',   'fp, lr, [sp], #64');
        self.emitInstr('ret',   'lr');
        self.asCode.append('ENDPROC %s' % (self.sLabel,));

        # Serialize the data.
        self.asData += [
            'g_DataStart_%s: /* %s / %s */' % (self.sLabel, self.sName, self.sInstr),
        ];
        offData = 0;
        while offData < len(self.abData):
            self.asData.append('    .byte %s' % (','.join(['%#04x' % (b,) for b in self.abData[offData:offData+16]]),));
            offData += 16;
        self.asData += [
            'g_DataEnd_%s: /* %s / %s */' % (self.sLabel, self.sName, self.sInstr),
        ];

        return None;

    def generateBody(self, oOptions, cLeftToAllCheck):
        _ = oOptions; _ = cLeftToAllCheck;
        return None;

    def emitCommentLine(self, sComment):
        self.asCode.append('        /* %s */' % (sComment,));

    def emitInstr(self, sInstr, sOperands, sComment = None):
        sTmp = '        %-8s %s' % (sInstr, sOperands,);
        if sComment:
            sTmp = '%-47s /* %s */' % (sTmp, sComment,);
        self.asCode.append(sTmp);

    def localLabel(self):
        global g_iLabel;
        sRet = '.L%u' % (g_iLabel,);
        g_iLabel += 1;
        return sRet;

    def emitLabel(self, sLabel):
        self.asCode.append('%s:' % (sLabel,));

    def emitBrk(self):
        global g_iBrkNo;
        self.emitInstr('brk', '#0x%x' % (g_iBrkNo & 0xffff,));
        g_iBrkNo += 1;

    def _findInData(self, abValue, cbMult):
        """
        Locates abValue in self.abData, if possible.
        Returns offset relative to the end (iRegDataPtr) and the load instruction to use.

        Note! We accumulate content of abData in reverse order and iRegDataPtr starts at
              at the end of the data area, so that we can use the 12-bit unsigned offset
              variant of LDR as well as LDUR to get at the data.
        """
        cbMax    = 0xfff * cbMult + cbMult - 1;
        offValue = self.abData.find(abValue, 0, cbMax + 1);
        while offValue > 0 and (offValue & (cbMult - 1)) != 0:
            if offValue < 256:
                return (offValue, 'ldur');
            offValue = self.abData.find(abValue, offValue + 1, cbMax);
        if offValue > 0 and (offValue & (cbMult - 1)) == 0:
            assert offValue / cbMult <= 0xfff;
            return (offValue, 'ldr');
        return (-1, None);

    def emitGprLoad(self, iReg, uValue, fReg31IsSp = True, fTempValue = False):
        """
        Generates loading uValue into GPR 'iReg'.
        Returns iReg.
        """
        assert uValue >= 0;
        assert iReg != 31 or fReg31IsSp;

        # Allocate temp register if we're loading SP as it's difficult to do in a generic manner.
        iRegToLoad = iReg;
        if iReg == 31 and fReg31IsSp:
            iRegToLoad = self.oGprAllocator.allocNo31(uValue);

        uInvVal = ~uValue & 0xffffffffffffffff
        # 16-bit immediates
        if 0 <= uValue <= 0xffff:
            self.emitInstr('movz',  'x%u, #0x%x' % (iRegToLoad, uValue));
        elif (uValue & ~0xffff0000) == 0:
            self.emitInstr('movz',  'x%u,  #0x%x, LSL #16' % (iRegToLoad, uValue >> 16), hex(uValue));
        elif (uValue & ~0xffff00000000) == 0:
            self.emitInstr('movz',  'x%u, #0x%x, LSL #32' % (iRegToLoad, uValue >> 32), hex(uValue));
        elif (uValue & ~0xffff000000000000) == 0:
            self.emitInstr('movz',  'x%u, #0x%x, LSL #48' % (iRegToLoad, uValue >> 48), hex(uValue));
        # 16-bit inverted immediates
        elif 0 <= uInvVal <= 0xffff:
            self.emitInstr('movn',  'x%u, #0x%x' % (iRegToLoad, uInvVal), hex(uValue));
        elif (uInvVal & ~0xffff0000) == 0:
            self.emitInstr('movn',  'x%u, #0x%x, LSL #16' % (iRegToLoad, uInvVal >> 16), hex(uValue));
        elif (uInvVal & ~0xffff00000000) == 0:
            self.emitInstr('movn',  'x%u, #0x%x, LSL #32' % (iRegToLoad, uInvVal >> 32), hex(uValue));
        elif (uInvVal & ~0xffff000000000000) == 0:
            self.emitInstr('movn',  'x%u, #0x%x, LSL #48' % (iRegToLoad, uInvVal >> 48), hex(uValue));
        # Load data.
        elif 0 <= uValue <= 0xffffffff:
            abValue             = uValue.to_bytes(4, byteorder = 'little');
            (offValue, sLdrIns) = self._findInData(abValue, 4);
            if offValue >= 0:
                self.emitInstr(sLdrIns, 'w%u, [x%u, #%u]' % (iRegToLoad, self.iRegDataPtr, offValue), hex(uValue));
            else:
                self.abData = abValue + self.abData;
                self.emitInstr('ldr',   'w%u, [x%u, #-4]!' % (iRegToLoad, self.iRegDataPtr), hex(uValue));
        else:
            abValue             = uValue.to_bytes(8, byteorder = 'little');
            (offValue, sLdrIns) = self._findInData(abValue, 8);
            if offValue >= 0:
                self.emitInstr(sLdrIns, 'x%u, [x%u, #%u]' % (iRegToLoad, self.iRegDataPtr, offValue), hex(uValue));
            else:
                self.abData = abValue + self.abData;
                self.emitInstr('ldr',   'x%u, [x%u, #-8]!' % (iRegToLoad, self.iRegDataPtr), hex(uValue));

        # SP hack:
        if iReg != iRegToLoad:
            self.emitInstr('mov', 'sp, %s' % (g_dGpr64NamesZr[iRegToLoad],));
            self.oGprAllocator.free(iRegToLoad);

        if not fTempValue and (iReg != 31 or fReg31IsSp):
            self.oGprAllocator.updateValue(iReg, uValue);
        return iReg;

    def emitGprValCheck(self, iRegToCheck, uExpectedValue, iRegTmp = -1, fReg31IsSp = False):
        """ Emits a register value check. """
        #
        # If the register we're supposed to check is XZR, we should try make
        # sure SP wasn't modified by accident (this have happened already).
        #
        if iRegToCheck == 31 and not fReg31IsSp:
            if self.fMayUseSp:
                # The testcase uses SP, so it should normally have a known value,
                # modify the parameters to check for this in SP instead.
                uExpectedValue = self.oGprAllocator.auValues[31];
                if uExpectedValue is None:
                    return None;
                fReg31IsSp = True;
            else:
                # If the testcase doesn't use SP, it should have the incoming stack
                # value which the prolog saved in a global variable. Load and compare.
                iRegTmp = self.oGprAllocator.allocNo31();
                self.emitInstr('adrp',  'x%u, PAGE(g_u64SavedSp)' % (iRegTmp,));
                self.emitInstr('add',   'x%u, x%u, PAGEOFF(g_u64SavedSp)' % (iRegTmp, iRegTmp,));
                self.emitInstr('ldr',   'x%u, [x%u]' % (iRegTmp, iRegTmp));
                self.emitInstr('cmp',   'sp, x%u, UXTX' % (iRegTmp));
                sLabel = self.localLabel();
                self.emitInstr('b.eq',  sLabel);
                self.emitBrk();
                self.emitLabel(sLabel);
                self.oGprAllocator.free(iRegTmp);
                return None;

        #
        # Regular check.
        #
        sRegToCheck = g_dddGprNamesBySpAndBits[fReg31IsSp][64][iRegToCheck];
        sCmpForceExtended = ', UXTX' if iRegToCheck == 31 and fReg31IsSp else '';

        sLabel = self.localLabel();
        if uExpectedValue == 0 and not fReg31IsSp:
            self.emitInstr('cbz',   '%s, %s' % (sRegToCheck, sLabel,));
        else:
            if uExpectedValue < 0x1000:
                self.emitInstr('cmp',   '%s, #0x%x' % (sRegToCheck, uExpectedValue,));
            elif iRegTmp >= 0:
                self.emitGprLoad(iRegTmp, uExpectedValue);
                self.emitInstr('cmp',   '%s, x%u%s' % (sRegToCheck, iRegTmp, sCmpForceExtended,));
            else:
                iRegTmp = self.oGprAllocator.allocNo31(uExpectedValue);
                self.emitGprLoad(iRegTmp, uExpectedValue);
                self.emitInstr('cmp',   '%s, x%u%s' % (sRegToCheck, iRegTmp, sCmpForceExtended,));
                self.oGprAllocator.free(iRegTmp);
            self.emitInstr('b.eq',  sLabel);
        self.emitBrk();
        self.emitLabel(sLabel);
        return None;

    def emitAllGprChecks(self):
        """ Check the content of all GPRs with a known value. """
        self.asCode.append('        /* Start all GPR check: */');
        iRegTmp  = self.oGprAllocator.allocNo31();
        cChecked = 0;
        for iReg, uValue in enumerate(self.oGprAllocator.auValues):
            if uValue is not None:
                self.emitGprValCheck(iReg, uValue, iRegTmp = iRegTmp, fReg31IsSp = True);
                cChecked += 1;
        self.oGprAllocator.free(iRegTmp);
        self.asCode.append('        /* End all GPR check (%u regs checked) */' % (cChecked,));
        return None;

    def maybeEmitAllGprChecks(self, cLeft, oOptions):
        if cLeft > 0:
            return cLeft - 1;
        self.emitAllGprChecks();
        return oOptions.cCheckAllRegsInterval;

    def allocGprAndLoadRandUBits(self, cBits = 64, fIncludingReg31 = True, fReg31IsSp = False):
        """
        Allocates a register and load a random value into them.
        return (uValue, iReg)
        """
        uValue = randUBits(cBits);
        iReg = self.oGprAllocator.allocEx(uValue, fIncludingReg31 = fIncludingReg31, fReg31IsSp = fReg31IsSp);
        if iReg != 31 or fReg31IsSp:
            self.emitGprLoad(iReg, uValue, fReg31IsSp = fReg31IsSp);
        else:
            uValue = 0;
            assert fIncludingReg31;
        return (uValue, iReg);

    def emitLoadNzcv(self, u4Nzcv):
        assert 0 <= u4Nzcv < 16;
        iRegTmp = self.emitGprLoad(self.oGprAllocator.allocNo31(), u4Nzcv << 28);
        self.emitInstr('msr',   'NZCV, x%u' % (iRegTmp,));
        self.oGprAllocator.free(iRegTmp);

    def emitFlagsCheck(self, fExpectedNzcv, iRegTmp = -1):
        """ Emits a NZCV flags check. """
        assert iRegTmp != 31;
        iRegTmpToUse = iRegTmp if iRegTmp >= 0 else self.oGprAllocator.allocNo31(fExpectedNzcv);
        self.emitInstr('mrs',   'x%u, NZCV' % (iRegTmpToUse,));
        self.emitGprValCheck(iRegTmpToUse, fExpectedNzcv);
        if iRegTmpToUse != iRegTmp:
            self.oGprAllocator.free(iRegTmpToUse);
        return None;

    def emitBaseRegAddrCalc(self, sRegBase, iRegSlot0Ptr, idxPreSlot):
        """ Handles a idxPreSlot range of +/-0xffffff. """
        if idxPreSlot >= 0:
            sInstr     = 'add';
        else:
            sInstr     = 'sub';
            idxPreSlot = -idxPreSlot;
        assert idxPreSlot <= 0xffffff
        if idxPreSlot <= 0xfff:
            self.emitInstr(sInstr, '%s, x%u, #%u' % (sRegBase, iRegSlot0Ptr, idxPreSlot,));
        else:
            self.emitInstr(sInstr, '%s, x%u, #%u, LSL 12' % (sRegBase, iRegSlot0Ptr, idxPreSlot >> 12,));
            self.emitInstr(sInstr, '%s, %s, #%u' % (sRegBase, sRegBase, idxPreSlot & 0xfff,));
        return None;

    def emitBaseAddrSaveInNewRegAlloc(self, sRegBase):
        """ Allocates a new register (returned) and stores a copy of the sRegBase register there. """
        iRegBaseCp  = self.oGprAllocator.allocNo31();
        self.emitInstr('mov', 'x%u, %s' % (iRegBaseCp, sRegBase,));
        return iRegBaseCp;

    def emitBaseAddrCheckAgainstRegAndFreeIt(self, sRegBase, iRegBaseCp):
        """ Destructive checking of sRegBase against iRegBaseCp (clobbered + freed). """
        sLabel = self.localLabel();
        self.emitInstr('sub', 'x%u, %s, x%u' % (iRegBaseCp, sRegBase, iRegBaseCp,)); # sub because of sp.
        self.emitInstr('cbz', 'x%u, %s' % (iRegBaseCp, sLabel,));
        self.emitBrk();
        self.emitLabel(sLabel);
        self.oGprAllocator.updateValue(iRegBaseCp, 0);
        self.oGprAllocator.free(iRegBaseCp);
        return None;


g_kdFprBytesToInfix     = { 1: 'B', 2: 'H', 4: 'S', 8: 'D', 16: 'Q' };
g_kdFprBytesToRegPrefix = { 1: 'b', 2: 'h', 4: 's', 8: 'd', 16: 'q' };

class A64No1CodeGenFprBase(A64No1CodeGenBase):
    """
    Floating Point and SIMD
    """

    def __init__(self, sName, sInstr, cbMem, oFprAllocator = None, oGprAllocator = None):
        A64No1CodeGenBase.__init__(self, sName, sInstr, oGprAllocator if oGprAllocator else Arm64GprAllocator());
        self.oFprAllocator = oFprAllocator if oFprAllocator else Arm64FpRegAllocator();
        self.sPrefix       = g_kdFprBytesToRegPrefix[cbMem];


    kaFprLoadSpecs = [
        ( 'b',  1 <<   8,  1,  '.byte',   1, 19 ),
        ( 'h',  1 <<  16,  2,  '.hword',  2, 13 ),
        ( 's',  1 <<  32,  4,  '.int',    4,  9 ),
        ( 'd',  1 <<  64,  8,  '.quad',   8,  5 ),
        ( 'q',  1 << 128, 16,  '.quad',   8,  5 ),
    ];

    def emitFprLoad(self, iReg, uValue):
        """
        Generates loading uValue into FP/SIMD register 'iReg'.
        Returns iReg.
        """
        assert uValue >= 0;

        for chRegPfx, uMax, cb, _, _, _ in self.kaFprLoadSpecs:
            if 0 <= uValue < uMax:
                abValue             = uValue.to_bytes(cb, byteorder = 'little');
                (offValue, sLdrIns) = self._findInData(abValue, cb);
                if offValue >= 0:
                    self.emitInstr(sLdrIns, '%s%u, [x%u, #%u]' % (chRegPfx, iReg, self.iRegDataPtr, offValue), hex(uValue));
                else:
                    self.abData = abValue + self.abData;
                    self.emitInstr('ldr',   '%s%u, [x%u, #-%u]!' % (chRegPfx, iReg, self.iRegDataPtr, cb), hex(uValue));
                return iReg;
        self.oFprAllocator.updateValue(iReg, uValue);
        return iReg;

    def emitFprValCheck(self, iRegToCheck, uExpectedValue, iRegTmp = -1):
        """ Emits a register value check. """
        iRegTmpFree = -1;
        if iRegTmp < 0:
            iRegTmp = self.oFprAllocator.alloc();
            iRegTmpFree = iRegTmp;

        self.emitFprLoad(iRegTmp, uExpectedValue);
        self.emitInstr('cmeq',  'v%u.16b, v%u.16b, v%u.16b' % (iRegTmp, iRegTmp, iRegToCheck,));
        self.emitInstr('uminv', 'b%u, v%u.16b' % (iRegTmp, iRegTmp,));
        iGprTmp = self.oGprAllocator.allocNo31();
        self.emitInstr('fmov',  'w%u, s%u' % (iGprTmp, iRegTmp,));
        sLabel = self.localLabel();
        self.emitInstr('cbnz',  'w%u, %s' % (iGprTmp, sLabel,));
        self.emitBrk();
        self.emitLabel(sLabel);

        self.oGprAllocator.free(iGprTmp);
        self.oFprAllocator.free(iRegTmpFree);
        return None;

    def allocFprAndLoadRandUBits(self, cBits = 128):
        """
        Allocates a SIMD&FP register and load a random value into them.
        return (uValue, iReg)
        """
        uValue = randUBits(cBits);
        iReg   = self.oFprAllocator.alloc(uValue);
        self.emitFprLoad(iReg, uValue);
        return (uValue, iReg);

    def emitFpSrLoad(self, fFpSrIn):
        """
        Loads 'fFpSrIn' into to the FPSR (FPU status) register
        """
        if fFpSrIn == 0:
            self.emitInstr('msr', 'FPSR, xzr');
        else:
            iRegTmp = self.emitGprLoad(self.oGprAllocator.allocNo31(fFpSrIn), fFpSrIn);
            self.emitInstr('msr', 'FPSR, x%u' % (iRegTmp,));
            self.oGprAllocator.free(iRegTmp);
        return None;

    def emitFpSrCheck(self, fFpSrOut):
        """
        Checks that the FPSR (FPU status) register value is 'fFpSrOut'.
        """
        iRegTmp = self.oGprAllocator.allocNo31(fFpSrOut);
        self.emitInstr('mrs', 'x%u, FPSR' % (iRegTmp,));
        self.emitGprValCheck(iRegTmp, fFpSrOut);
        self.oGprAllocator.free(iRegTmp);
        return None;


#
# The code generators for specific instructions.
#

class A64No1CodeGenAddSubImm(A64No1CodeGenBase):
    """
    C4.1.92.3 addsub_imm

    All variants take SP as input register, but only the non-flag variants
    can use SP a the target register (the flag ones targets XZR in order to
    implement CMP and such).
    """
    def __init__(self, sInstr, fnCalc, fWithFlags = False):
        A64No1CodeGenBase.__init__(self, sInstr + '_imm', sInstr, Arm64GprAllocator(), fMayUseSp = True);
        self.fnCalc      = fnCalc;
        self.fWithFlags  = fWithFlags;
        self.fWithSpDst  = not fWithFlags;

    def generateBody(self, oOptions, cLeftToAllCheck):
        for cBits in (32, 64,):
            for _ in range(oOptions.cTestsPerInstruction):
                (uVal1, iRegIn1) = self.allocGprAndLoadRandUBits(fReg31IsSp = True);
                uVal2   = randUBits(12);

                fShift = randBool();
                uVal2Shifted = uVal2 << 12 if fShift else uVal2;

                uRes, fNzcv = self.fnCalc(cBits, uVal1, uVal2Shifted, 0);
                iRegDst = self.oGprAllocator.allocEx(uRes, fReg31IsSp = self.fWithSpDst);

                self.emitInstr(self.sInstr,
                               '%s, %s, #0x%x%s' % (g_dddGprNamesBySpAndBits[self.fWithSpDst][cBits][iRegDst],
                                                    g_ddGprNamesSpByBits[cBits][iRegIn1], uVal2, ', LSL #12' if fShift else '',));
                if self.fWithFlags:
                    self.emitFlagsCheck(fNzcv);
                self.emitGprValCheck(iRegDst, uRes, fReg31IsSp = self.fWithSpDst);

                self.oGprAllocator.freeList((iRegIn1, iRegDst,));
                cLeftToAllCheck = self.maybeEmitAllGprChecks(cLeftToAllCheck, oOptions);


class A64No1CodeGenExtrImm(A64No1CodeGenBase):
    """
    C4.1.92.9 extract

    No SP use.
    """
    def __init__(self, sInstr, fnCalc):
        A64No1CodeGenBase.__init__(self, sInstr + '_imm', sInstr, Arm64GprAllocator());
        self.fnCalc = fnCalc;
        assert self.oGprAllocator.auValues[31] is None;

    def generateBody(self, oOptions, cLeftToAllCheck):
        for cBits in (32, 64,):
            for _ in range(oOptions.cTestsPerInstruction):
                (uVal1, iRegIn1) = self.allocGprAndLoadRandUBits();
                fRor             = randUBits(2) == 0;
                (uVal2, iRegIn2) = self.allocGprAndLoadRandUBits() if not fRor else (uVal1, iRegIn1);
                cShift  = randUBits(5 if cBits == 32 else 6);

                uRes = self.fnCalc(cBits, uVal1, uVal2, cShift);
                iRegDst = self.oGprAllocator.alloc(uRes);

                self.emitInstr(self.sInstr,
                               '%s, %s, %s, #%u' % (g_ddGprNamesZrByBits[cBits][iRegDst], g_ddGprNamesZrByBits[cBits][iRegIn1],
                                                    g_ddGprNamesZrByBits[cBits][iRegIn2], cShift));
                self.emitGprValCheck(iRegDst, uRes);

                self.oGprAllocator.freeList((iRegIn1, iRegDst, -1 if iRegIn1 == iRegIn2 else iRegIn2));
                cLeftToAllCheck = self.maybeEmitAllGprChecks(cLeftToAllCheck, oOptions);

def calcExtr(cBits, uVal1, uVal2, cShift):
    fMask = bitsOnes(cBits);
    return ((((uVal1 & fMask) << cBits) | (uVal2 & fMask)) >> cShift) & fMask;


class A64No1CodeGenShiftedReg(A64No1CodeGenBase):
    """
    C4.1.94.3 log_shift
    C4.1.94.4 addsub_shift

    None of these instructions operates of SP.
    """
    def __init__(self, sInstr, fnCalc, fWithFlags = False, fWithRor = False):
        A64No1CodeGenBase.__init__(self, sInstr + '_shifted_reg', sInstr, Arm64GprAllocator());
        self.fnCalc      = fnCalc;
        self.fWithFlags  = fWithFlags;
        self.cShiftTypes = 4 if fWithRor else 3;

    kdShiftNm = {0: 'LSL', 1: 'LSR', 2: 'ASR', 3: 'ROR',}

    @staticmethod
    def shiftRegValue(uValue, bShiftType, cShift, cBits):
        fMask = (1 << cBits) - 1;
        if bShiftType == 0:     # LSL
            uValue <<= cShift;
        elif bShiftType == 1:   # LSR
            uValue  &= fMask;
            uValue >>= cShift;
        elif bShiftType == 2:   # ASR
            fOrMask  = 0;
            if (uValue >> (cBits - 1)) & 1:
                fOrMask = 0xffffffffffffffff << (cBits - cShift);
            uValue  &= fMask;
            uValue >>= cShift;
            uValue  |= fOrMask;
        else:                   # ROR
            assert bShiftType == 3;
            uValue  &= fMask;
            uValue   = (uValue >> cShift) | (uValue << (cBits - cShift));
        return uValue & fMask;

    def generateBody(self, oOptions, cLeftToAllCheck):
        for cBits in (32, 64,):
            for i in range(oOptions.cTestsPerInstruction):
                (uVal1, iRegIn1) = self.allocGprAndLoadRandUBits();
                (uVal2, iRegIn2) = self.allocGprAndLoadRandUBits();

                bShiftType = randU8() % self.cShiftTypes;
                cShift     = randU8() % cBits if i & 1 else 0;
                uVal2Shifted = self.shiftRegValue(uVal2, bShiftType, cShift, cBits);

                uRes, fNzcv = self.fnCalc(cBits, uVal1, uVal2Shifted, 0);
                iRegDst = self.oGprAllocator.alloc(uRes);

                self.emitInstr(self.sInstr, '%s, %s, %s, %s #%u'
                               % (g_ddGprNamesZrByBits[cBits][iRegDst], g_ddGprNamesZrByBits[cBits][iRegIn1],
                                  g_ddGprNamesZrByBits[cBits][iRegIn2], self.kdShiftNm[bShiftType], cShift,));
                if self.fWithFlags:
                    self.emitFlagsCheck(fNzcv);
                self.emitGprValCheck(iRegDst, uRes);

                self.oGprAllocator.freeList((iRegIn1, iRegIn2, iRegDst,));
                cLeftToAllCheck = self.maybeEmitAllGprChecks(cLeftToAllCheck, oOptions);


class A64No1CodeGenExtendedReg(A64No1CodeGenBase):
    """
    C4.1.94.5 addsub_ext

    All variants take SP as input register, but only the non-flag variants
    can use SP a the target register (the flag ones targets XZR in order to
    implement CMP and such).
    """
    def __init__(self, sInstr, fnCalc, fWithFlags = False):
        A64No1CodeGenBase.__init__(self, sInstr + '_shifted_reg', sInstr, Arm64GprAllocator(), fMayUseSp = True);
        self.fnCalc      = fnCalc;
        self.fWithFlags  = fWithFlags;
        self.fWithSpDst  = not fWithFlags;

    kdOp = { 0: 'UXTB', 1: 'UXTH', 2: 'UXTW', 3: 'UXTX', 4: 'SXTB', 5: 'SXTH', 6: 'SXTW', 7: 'SXTX', };

    @staticmethod
    def extendAndShift(uValue, bOpType, cShift, cBits):
        if   bOpType == 0:  # UXTB
            uValue &= 0xff;
        elif bOpType == 1:  # UXTH
            uValue &= 0xffff;
        elif bOpType == 2:  # UXTW
            uValue &= 0xffffffff;
        elif bOpType == 3:  # UXTX
            pass;
        elif bOpType == 4:  # SXTB
            uValue &= 0xff;
            if uValue & 0x80:
                uValue |= 0xffffffffffffff00;
        elif bOpType == 5:  # SXTH
            uValue &= 0xffff;
            if uValue & 0x8000:
                uValue |= 0xffffffffffff0000;
        elif bOpType == 6:  # SXTW
            uValue &= 0xffffffff;
            if uValue & 0x80000000:
                uValue |= 0xffffffff00000000;
        elif bOpType == 7:  # SXTX
            pass;
        else:
            assert False, str(bOpType);
        uValue <<= cShift;
        return uValue & ((1 << cBits) - 1);

    def generateBody(self, oOptions, cLeftToAllCheck):
        for cBits in (32, 64,):
            for i in range(oOptions.cTestsPerInstruction):
                (uVal1, iRegIn1) = self.allocGprAndLoadRandUBits(fReg31IsSp = True);
                (uVal2, iRegIn2) = self.allocGprAndLoadRandUBits();

                bOpType = randUBits(3);
                cShift  = randUBits(2) if i & 1 else 0;
                uVal2Shifted = self.extendAndShift(uVal2, bOpType, cShift, cBits);
                cBitsRegIn2  = cBits;
                if cBits == 64 and bOpType not in (3, 7):
                    cBitsRegIn2 = 32;

                uRes, fNzcv = self.fnCalc(cBits, uVal1, uVal2Shifted, 0);
                iRegDst = self.oGprAllocator.allocEx(uRes, fReg31IsSp = self.fWithSpDst);

                self.emitInstr(self.sInstr, '%s, %s, %s, %s #%u'
                               % (g_dddGprNamesBySpAndBits[self.fWithSpDst][cBits][iRegDst], g_ddGprNamesSpByBits[cBits][iRegIn1],
                                  g_ddGprNamesZrByBits[cBitsRegIn2][iRegIn2], self.kdOp[bOpType], cShift,));
                if self.fWithFlags:
                    self.emitFlagsCheck(fNzcv);
                self.emitGprValCheck(iRegDst, uRes, fReg31IsSp = self.fWithSpDst);

                self.oGprAllocator.freeList((iRegIn1, iRegIn2, iRegDst,));
                cLeftToAllCheck = self.maybeEmitAllGprChecks(cLeftToAllCheck, oOptions);


class A64No1CodeGenAddSubCarry(A64No1CodeGenBase):
    """
    C4.1.94.6 addsub_carry

    None of these instructions operates on SP.
    """
    def __init__(self, sInstr, fnCalc, fWithFlags = False):
        A64No1CodeGenBase.__init__(self, sInstr + '_carry', sInstr, Arm64GprAllocator());
        self.fnCalc      = fnCalc;
        self.fWithFlags  = fWithFlags;

    def generateBody(self, oOptions, cLeftToAllCheck):
        for cBits in (32, 64,):
            fSignMax = (1 << (cBits - 1)) - 1
            aFixed   = (
                (0, fSignMax, 1),
                (0, fSignMax, 0),
                (fSignMax, 0, 1),
                (fSignMax, 0, 0),
                (fSignMax, fSignMax, 1),
                (fSignMax, fSignMax, 0),
                (0, 0, 1),
                (0, 0, 0)
            );
            for i in range(oOptions.cTestsPerInstruction + len(aFixed)):
                if i < len(aFixed):
                    (uVal1, uVal2, fCarry) = aFixed[i];
                    iRegIn1 = self.emitGprLoad(self.oGprAllocator.allocNo31(uVal1), uVal1);
                    iRegIn2 = self.emitGprLoad(self.oGprAllocator.allocNo31(uVal2), uVal2);
                else:
                    (uVal1, iRegIn1) = self.allocGprAndLoadRandUBits();
                    (uVal2, iRegIn2) = self.allocGprAndLoadRandUBits();
                    fCarry           = randUBits(1);

                if fCarry:
                    self.emitInstr('subs', 'wzr, wzr, wzr', 'clears carry'); # N=0, Z=1, C=1, V=0
                else:
                    self.emitInstr('adds', 'wzr, wzr, wzr', 'sets carry');   # N=0, Z=1, C=0, V=0

                uRes, fNzcv = self.fnCalc(cBits, uVal1, uVal2, fCarry);
                iRegDst = self.oGprAllocator.alloc(uRes);

                self.emitInstr(self.sInstr, '%s, %s, %s'
                               % (g_ddGprNamesZrByBits[cBits][iRegDst], g_ddGprNamesZrByBits[cBits][iRegIn1],
                                  g_ddGprNamesZrByBits[cBits][iRegIn2],));
                if self.fWithFlags:
                    self.emitFlagsCheck(fNzcv);
                self.emitGprValCheck(iRegDst, uRes);

                self.oGprAllocator.freeList((iRegIn1, iRegIn2, iRegDst,));
                cLeftToAllCheck = self.maybeEmitAllGprChecks(cLeftToAllCheck, oOptions);


class A64No1CodeGenBitfieldMove(A64No1CodeGenBase):
    """
    C4.1.92.8 bitfield

    None of these instructions operates on SP.
    """
    def __init__(self, sInstr, fnCalc):
        A64No1CodeGenBase.__init__(self, sInstr + '_carry', sInstr, Arm64GprAllocator());
        self.fnCalc      = fnCalc;

    def generateBody(self, oOptions, cLeftToAllCheck):
        for cBits in (32, 64,):
            for _ in range(oOptions.cTestsPerInstruction):
                (uSrc, iRegIn)  = self.allocGprAndLoadRandUBits();
                (uDst, iRegDst) = self.allocGprAndLoadRandUBits();
                uImmR           = randUBits(5 if cBits == 32 else 6);
                uImmS           = randUBits(5 if cBits == 32 else 6);

                uRes = self.fnCalc(cBits, uDst, uSrc, uImmR, uImmS);

                self.emitInstr(self.sInstr, '%s, %s, #0x%x, #0x%x'
                               % (g_ddGprNamesZrByBits[cBits][iRegDst], g_ddGprNamesZrByBits[cBits][iRegIn],
                                  uImmR, uImmS,));
                if iRegDst != 31:
                    self.oGprAllocator.updateValue(iRegDst, uRes);
                self.emitGprValCheck(iRegDst, uRes);

                self.oGprAllocator.freeList((iRegIn, iRegDst,));
                cLeftToAllCheck = self.maybeEmitAllGprChecks(cLeftToAllCheck, oOptions);

def decodeBitMasks(uImmR, fN, uImmS, cBits, fImmediate = False): # pylint:disable=invalid-name
    uTmp = ((fN & 1) << 6) | (~uImmS & 0x3f);
    cBitsLength = uTmp.bit_length() - 1;
    assert cBitsLength >= 0;

    cLevels = bitsOnes(cBitsLength);
    assert cLevels < cBits, 'r=%#x N=%u s=%#x bits=%u uTmp=%#x: cLevels=%u' % (uImmR, fN, uImmS, cBits, uTmp, cLevels,);
    assert not fImmediate or (uImmS & cLevels) != cLevels;

    uImmS &= cLevels;
    uImmR &= cLevels;
    uDiff  = (uImmS - uImmR) & cLevels;

    cBitsElement = 1 << cBitsLength;

    fWElem   = bitsOnes(uImmS + 1);
    fTopElem = bitsOnes(uDiff + 1);

    fWMask   = bitsReplicate(cBitsElement, bitsRor(cBitsElement, fWElem, uImmR), cBits // cBitsElement);
    fTopMask = bitsReplicate(cBitsElement, fTopElem,                             cBits // cBitsElement);
    return (fWMask, fTopMask)

def calcBfm(cBits, uDst, uSrc, uImmR, uImmS):
    (fWMask, fTopMask) = decodeBitMasks(uImmR, 1 if cBits == 64 else 0,  uImmS, cBits);
    fBottom = (uDst & ~fWMask) | (bitsRor(cBits, uSrc, uImmR) & fWMask);
    return (uDst & ~fTopMask & bitsOnes(cBits)) | (fBottom & fTopMask);

def calcSbfm(cBits, _, uSrc, uImmR, uImmS):
    (fWMask, fTopMask) = decodeBitMasks(uImmR, 1 if cBits == 64 else 0,  uImmS, cBits);
    fBottom = bitsRor(cBits, uSrc, uImmR) & fWMask;
    fTop    = bitsReplicate(1, (uSrc >> uImmS) & 1, cBits);
    return (fTop & ~fTopMask) | (fBottom & fTopMask);

def calcUbfm(cBits, _, uSrc, uImmR, uImmS):
    (fWMask, fTopMask) = decodeBitMasks(uImmR, 1 if cBits == 64 else 0,  uImmS, cBits);
    fBottom = bitsRor(cBits, uSrc, uImmR) & fWMask;
    return fBottom & fTopMask;


class A64No1CodeGenMoviGrp(A64No1CodeGenFprBase):
    """
    C4.1.95.25 Advanced SIMD modified immediate.
    """
    def __init__(self):
        A64No1CodeGenFprBase.__init__(self, 'asimdimm', 'asimdimm', 16);

    def generateBody(self, oOptions, cLeftToAllCheck):
        for i in range(oOptions.cTestsPerInstruction):
            u1Q     = randUBits(1);
            u1Op    = randUBits(1);
            u4Mode  = randUBits(4);
            u8Imm   = randUBits(8);
            uSrc    = randUBits(128);
            u1O2    = 0;

            sInstr  = 'movi';
            if u4Mode == 0b1111:
                sInstr  = 'fmov';
                cBits   = 64 if u1Op else 32;
                if u1Q == 0 and u1Op == 1: # unallocated -> FMOV.4H/8H
                    u1O2  = 1;
                    u1Op  = 0;
                    cBits = 16;
                    u1Q   = randUBits(1);
                uRes    = aarchExpandAdvSimdImmFp(u8Imm, cBits);
                assert uRes >= 0;
                if u1Q:
                    uRes |= uRes << 64;
            else:
                uRes    = aarchExpandAdvSimdImm(u1Op, u4Mode, u8Imm);
                assert uRes >= 0;
                uRes   |= (uRes << 64);
                if u1Op == 0 and ((u4Mode & 0b1001) == 0b0001 or (u4Mode & 0b1101) == 0b1001): # ORR
                    sInstr  = 'orr';
                    uRes |= uSrc;
                    assert uRes >= 0;
                elif u1Op == 1 and ((u4Mode & 0b1001) == 0b0001 or (u4Mode & 0b1101) == 0b1001): # BIC
                    sInstr  = 'bic';
                    uRes = uSrc & ~uRes;
                    assert uRes >= 0;
                elif u1Op == 1 and u4Mode != 0b1110: # MVNI
                    sInstr  = 'mvni';
                    uRes ^= bitsOnes(128);
                    assert uRes >= 0;
                if not u1Q:
                    uRes &= bitsOnes(64);
                    assert uRes >= 0;

            iRegDst = self.oFprAllocator.alloc(uRes);

            self.emitCommentLine('i=%u Q=%u op=%u mode=%s (%s) imm=%s (%x) Rd=%u uSrc=%#x uRes=%#x'
                                 % (i, u1Q, u1Op, bin(u4Mode), sInstr, bin(u8Imm), u8Imm, iRegDst, uSrc, uRes,));
            self.emitFprLoad(iRegDst, uSrc);
            uInstr = (  0x0f000400 | iRegDst | ((u8Imm & 0x1f) << 5) | (u1O2 << 11) | (u4Mode << 12) | (u8Imm >> 5 << 16)
                      | (u1Op << 29) | (u1Q << 30));
            self.emitInstr('.int', '%#x' % (uInstr,));

            self.emitFprValCheck(iRegDst, uRes);

            self.oFprAllocator.free(iRegDst);
        _  = cLeftToAllCheck;


def aarchExpandAdvSimdImm(u1Op, u4Mode, u8Imm):
    """ AdvSIMDExpandImm """
    if u4Mode < 8:
        u8Imm <<= 8 * (u4Mode >> 1);
        return (u8Imm << 32) | u8Imm;

    if u4Mode < 12:
        if u4Mode >= 10:
            u8Imm <<= 8;
        return (u8Imm << 48) | (u8Imm << 32) | (u8Imm << 16) | u8Imm;

    if u4Mode < 14:
        if u4Mode == 12:
            u8Imm <<= 8;
            u8Imm  |= 0xff;
        else:
            u8Imm <<= 16;
            u8Imm  |= 0xffff;
        return (u8Imm << 32) | u8Imm;

    if u4Mode == 14:
        if u1Op == 0:
            return (  (u8Imm <<  0) | (u8Imm <<  8) | (u8Imm << 16) | (u8Imm << 24)
                    | (u8Imm << 32) | (u8Imm << 40) | (u8Imm << 48) | (u8Imm << 56));
        return (  (0xff00000000000000 if u8Imm & 0x80 else 0)
                | (0x00ff000000000000 if u8Imm & 0x40 else 0)
                | (0x0000ff0000000000 if u8Imm & 0x20 else 0)
                | (0x000000ff00000000 if u8Imm & 0x10 else 0)
                | (0x00000000ff000000 if u8Imm & 0x08 else 0)
                | (0x0000000000ff0000 if u8Imm & 0x04 else 0)
                | (0x000000000000ff00 if u8Imm & 0x02 else 0)
                | (0x00000000000000ff if u8Imm & 0x01 else 0));

    assert u4Mode == 15;
    u32Imm = (  ((u8Imm & 0x80) << (31 - 7))
              | (0x3e000000 if u8Imm & 0x40 else 0x40000000)
              | ((u8Imm & 0x3f) << 19));
    if u1Op == 0:
        return (u32Imm << 32) | u32Imm;
    return u32Imm << 32;

def aarchExpandAdvSimdImmFp(u8Imm, cBits):
    """ The floating point variant of aarchExpandAdvSimdImm (C2.2.3). """
    if cBits == 16:
        u16Tmp = (  (( u8Imm & 0x80) << (15 - 7)) # sign
                  | ((~u8Imm & 0x40) << (14 - 6))
                  | (( u8Imm & 0x40) << (13 - 6))
                  | (( u8Imm & 0x40) << (12 - 6))
                  | (( u8Imm & 0x3f) << 6) );
        return (u16Tmp << 48) | (u16Tmp << 32) | (u16Tmp << 16) | u16Tmp;
    if cBits == 32:
        u32Tmp = (  (( u8Imm & 0x80) << (31 - 7)) # sign
                  | ((~u8Imm & 0x40) << (30 - 6))
                  | (( u8Imm & 0x40) << (29 - 6))
                  | (( u8Imm & 0x40) << (28 - 6))
                  | (( u8Imm & 0x40) << (27 - 6))
                  | (( u8Imm & 0x40) << (26 - 6))
                  | (( u8Imm & 0x40) << (25 - 6))
                  | (( u8Imm & 0x3f) << 19) );
        return (u32Tmp << 32) | u32Tmp;
    if cBits == 64:
        return (  (( u8Imm & 0x80) << (63 - 7)) # sign
                | ((~u8Imm & 0x40) << (62 - 6))
                | (( u8Imm & 0x40) << (61 - 6))
                | (( u8Imm & 0x40) << (60 - 6))
                | (( u8Imm & 0x40) << (59 - 6))
                | (( u8Imm & 0x40) << (58 - 6))
                | (( u8Imm & 0x40) << (57 - 6))
                | (( u8Imm & 0x40) << (56 - 6))
                | (( u8Imm & 0x40) << (55 - 6))
                | (( u8Imm & 0x40) << (54 - 6))
                | (( u8Imm & 0x3f) << 48) );
    raise Exception(str(cBits));



def aarchExpandAdvSimdImmFMov(u8Imm):
    """ FEAT_FP16 variant of aarchExpandAdvSimdImm. """
    u16Imm = (  (( u8Imm & 0x80) << (15 - 7))
              | ((~u8Imm & 0x40) << (14 - 6))
              | (( u8Imm & 0x40) << (13 - 6))
              | (( u8Imm & 0x40) << (12 - 6))
              | (( u8Imm & 0x3f) <<  6) );
    return (u16Imm << 48) | (u16Imm << 32) | (u16Imm << 16) | u16Imm;


## Condition codes to name
g_kdCondNames = {
    0:  'EQ',
    1:  'NE',
    2:  'CS',
    3:  'CC',
    4:  'MI',
    5:  'PL',
    6:  'VS',
    7:  'VC',
    8:  'HI',
    9:  'LS',
    10: 'GE',
    11: 'LT',
    12: 'GT',
    13: 'LE',
    14: 'AL',
    15: 'NV',
};

def calcCond(u4Nzcv, u4Cond):
    """ Matches the given condition to the NZCV value. """
    fInv = u4Cond & 1;
    u4Cond >>= 1;
    if u4Cond == 0:     fResult = (u4Nzcv & 4) != 0;                   # Z != 0            # EQ / NE
    elif u4Cond == 1:   fResult = (u4Nzcv & 2) != 0;                   # C != 0            # CS / CC
    elif u4Cond == 2:   fResult = (u4Nzcv & 8) != 0;                   # N != 0            # MI / PL
    elif u4Cond == 3:   fResult = (u4Nzcv & 1) != 0;                   # V != 0            # VS / VC
    elif u4Cond == 4:   fResult = (u4Nzcv & 6) == 2;                   # C == 1 && Z == 0  # HI / LS
    elif u4Cond == 5:   fResult = ((u4Nzcv >> 3) & 1) == (u4Nzcv & 1); # N == V            # GE / LT
    elif u4Cond == 6:   fResult = ((u4Nzcv >> 3) & 1) == (u4Nzcv & 5); # N == V && Z == 0  # GT / LE
    elif u4Cond == 7:   return True;                                                       # AL / NV
    else: assert False;
    if fInv:
        fResult = not fResult;
    return fResult;


class A64No1CodeGenCondCmpReg(A64No1CodeGenBase):
    """ C4.1.94.10 Conditional Compare (register) - CCMN, CCMP """
    def __init__(self, sInstr, fnCalc):
        A64No1CodeGenBase.__init__(self, sInstr + '_reg', sInstr, Arm64GprAllocator());
        self.fnCalc = fnCalc; # calcSub/calcAdd

    def generateBody(self, oOptions, cLeftToAllCheck):
        for cBits in (64, 32):
            for _ in range(oOptions.cTestsPerInstruction):
                (uSrc1, iRegIn1)    = self.allocGprAndLoadRandUBits();
                (uSrc2, iRegIn2)    = self.allocGprAndLoadRandUBits();
                u4Cond              = randUBits(4);
                u4NzcvMiss          = randUBits(4);
                u4NzcvIn            = randUBits(4);
                if u4Cond < 14:
                    fDoCmp          = randBool();
                    while fDoCmp != calcCond(u4NzcvIn, u4Cond):
                        u4NzcvIn    = randUBits(4);

                if calcCond(u4NzcvIn, u4Cond):
                    fExpectedNzcv   = self.fnCalc(cBits, uSrc1, uSrc2, 0)[1];
                    while fExpectedNzcv == u4NzcvMiss << 28:
                        u4NzcvMiss  = randUBits(4);
                else:
                    fExpectedNzcv   = u4NzcvMiss << 28;

                # Load flags and execute the instruction.
                self.emitLoadNzcv(u4NzcvIn);
                self.emitInstr(self.sInstr, '%s, %s, #%u, %s'
                               % (g_ddGprNamesZrByBits[cBits][iRegIn1], g_ddGprNamesZrByBits[cBits][iRegIn2],
                                  u4NzcvMiss, g_kdCondNames[u4Cond],));

                # Check the resulting flags.
                self.emitFlagsCheck(fExpectedNzcv);

                self.oGprAllocator.freeList((iRegIn1, iRegIn2,));
                cLeftToAllCheck = self.maybeEmitAllGprChecks(cLeftToAllCheck, oOptions);


class A64No1CodeGenCondCmpImm(A64No1CodeGenBase):
    """ C4.1.94.11 Conditional Compare (immediate) - CCMN, CCMP """
    def __init__(self, sInstr, fnCalc):
        A64No1CodeGenBase.__init__(self, sInstr + '_imm', sInstr, Arm64GprAllocator());
        self.fnCalc = fnCalc; # calcSub/calcAdd

    def generateBody(self, oOptions, cLeftToAllCheck):
        for cBits in (64, 32):
            for _ in range(oOptions.cTestsPerInstruction):
                (uSrc, iRegIn)      = self.allocGprAndLoadRandUBits();
                u5Imm               = randUBits(5);
                u4Cond              = randUBits(4);
                u4NzcvMiss          = randUBits(4);
                u4NzcvIn            = randUBits(4);
                if u4Cond < 14:
                    fDoCmp          = randBool();
                    while fDoCmp != calcCond(u4NzcvIn, u4Cond):
                        u4NzcvIn    = randUBits(4);

                if calcCond(u4NzcvIn, u4Cond):
                    fExpectedNzcv   = self.fnCalc(cBits, uSrc, u5Imm, 0)[1];
                    while fExpectedNzcv == u4NzcvMiss << 28:
                        u4NzcvMiss  = randUBits(4);
                else:
                    fExpectedNzcv   = u4NzcvMiss << 28;

                # Load flags and execute the instruction.
                self.emitLoadNzcv(u4NzcvIn);
                self.emitInstr(self.sInstr, '%s, #%u, #%u, %s'
                               % (g_ddGprNamesZrByBits[cBits][iRegIn], u5Imm, u4NzcvMiss, g_kdCondNames[u4Cond],));

                # Check the resulting flags.
                self.emitFlagsCheck(fExpectedNzcv);

                self.oGprAllocator.free(iRegIn);
                cLeftToAllCheck = self.maybeEmitAllGprChecks(cLeftToAllCheck, oOptions);


class A64No1CodeGenCondSel(A64No1CodeGenBase):
    """ C4.1.94.12 Conditional Select - CSEL, CSINC, CSINV, CSNEG """
    def __init__(self, sInstr, fnCalc):
        A64No1CodeGenBase.__init__(self, sInstr, sInstr, Arm64GprAllocator());
        self.fnCalc = fnCalc;

    def generateBody(self, oOptions, cLeftToAllCheck):
        for cBits in (64, 32):
            for _ in range(oOptions.cTestsPerInstruction):
                (uSrc1, iRegIn1)    = self.allocGprAndLoadRandUBits();
                (uSrc2, iRegIn2)    = self.allocGprAndLoadRandUBits();
                u4Nzcv              = randUBits(4);
                u4Cond              = randUBits(4);

                uRes = self.fnCalc(cBits, uSrc1, uSrc2, u4Nzcv, u4Cond);

                # Load flags and execute the instruction.
                self.emitLoadNzcv(u4Nzcv);
                iRegDst = self.oGprAllocator.alloc(uRes);
                self.emitInstr(self.sInstr, '%s, %s, %s, %s'
                               % (g_ddGprNamesZrByBits[cBits][iRegDst], g_ddGprNamesZrByBits[cBits][iRegIn1],
                                  g_ddGprNamesZrByBits[cBits][iRegIn2], g_kdCondNames[u4Cond],));
                self.emitGprValCheck(iRegDst, uRes);

                self.oGprAllocator.freeList((iRegIn1, iRegIn2, iRegDst,));
                cLeftToAllCheck = self.maybeEmitAllGprChecks(cLeftToAllCheck, oOptions);

def calcCondSel(cBits, uSrc1, uSrc2, u4Nzcv, u4Cond):
    """ Regular CSEL result. """
    uRes = uSrc1 if calcCond(u4Nzcv, u4Cond) else uSrc2;
    return uRes & bitsOnes(cBits);

def calcCondSelInc(cBits, uSrc1, uSrc2, u4Nzcv, u4Cond):
    """ Calc CSINC result. """
    return calcCondSel(cBits, uSrc1, uSrc2 + 1, u4Nzcv, u4Cond);

def calcCondSelInv(cBits, uSrc1, uSrc2, u4Nzcv, u4Cond):
    """ Calc CSINV result. """
    return calcCondSel(cBits, uSrc1, ~uSrc2, u4Nzcv, u4Cond);

def calcCondSelNeg(cBits, uSrc1, uSrc2, u4Nzcv, u4Cond):
    """ Calc CSNEG result. """
    return calcCondSel(cBits, uSrc1, -uSrc2, u4Nzcv, u4Cond);


class A64No1CodeGenData1Op(A64No1CodeGenBase):
    """ C4.1.94.2 Data-processing (1 source) """
    def __init__(self, sInstr, fnCalc, acBits = (32, 64)):
        A64No1CodeGenBase.__init__(self, sInstr, sInstr, Arm64GprAllocator());
        self.fnCalc = fnCalc;
        self.acBits = acBits;

    def generateBody(self, oOptions, cLeftToAllCheck):
        for cBits in self.acBits:
            for _ in range(oOptions.cTestsPerInstruction):
                (uSrc, iRegIn)  = self.allocGprAndLoadRandUBits();
                uRes            = self.fnCalc(cBits, uSrc);
                iRegDst         = self.oGprAllocator.alloc(uRes);
                self.emitInstr(self.sInstr,
                               '%s, %s' % (g_ddGprNamesZrByBits[cBits][iRegDst], g_ddGprNamesZrByBits[cBits][iRegIn],));
                self.emitGprValCheck(iRegDst, uRes);

                self.oGprAllocator.free(iRegDst);
                self.oGprAllocator.free(iRegIn);
                cLeftToAllCheck = self.maybeEmitAllGprChecks(cLeftToAllCheck, oOptions);

def calcRbit(cBits, uSrc):
    uRet = 0;
    for iBit in range(cBits):
        uRet |= ((uSrc >> iBit) & 1) << (cBits - iBit - 1);
    return uRet;

def calcRev(cBits, uSrc):
    uRet = 0;
    for iBit in range(0, cBits, 8):
        uRet |= ((uSrc >> iBit) & 0xff) << (cBits - iBit - 8);
    return uRet;

def calcRev16(cBits, uSrc):
    uRet = 0;
    for iBit in range(0, cBits, 16):
        uWord = (uSrc >> iBit) & 0xffff;
        uWord = ((uWord >> 8) & 0xff) | ((uWord & 0xff) << 8);
        uRet |= uWord << iBit;
    return uRet;

def calcRev32(cBits, uSrc):
    assert cBits == 64;
    return (calcRev(32, uSrc >> 32) << 32) | calcRev(32, uSrc);

def calcClz(cBits, uSrc):
    iTopBit = (uSrc & bitsOnes(cBits)).bit_length();
    return cBits - iTopBit;

def calcCls(cBits, uSrc):
    uRet     = 0;
    fSignBit = (uSrc >> (cBits - 1)) & 1;
    for iBit in range(cBits - 2, -1, -1):
        if ((uSrc >> iBit) & 1) != fSignBit:
            break;
        uRet += 1;
    return uRet;

def calcCtz(cBits, uSrc):
    uRet = 0;
    while cBits > 0 and (uSrc & 1) == 0:
        uSrc >>= 1;
        uRet  += 1;
        cBits -= 1;
    return uRet;

def calcCnt(cBits, uSrc):
    return (uSrc & bitsOnes(cBits)).bit_count();

def calcAbs(cBits, uSrc):
    if uSrc & (1 << (cBits - 1)):
        uSrc = -bitsSignedToInt(cBits, uSrc);
    return uSrc & bitsOnes(cBits);


class A64No1CodeGenData3Op(A64No1CodeGenBase):
    """ C4.1.94.13 Data-processing (3 sources) """
    def __init__(self, sInstr, fnCalc, acBits = (32, 64), fSrc1And2Are32Bit = False, fSrc3IsZero = False):
        A64No1CodeGenBase.__init__(self, sInstr, sInstr, Arm64GprAllocator());
        self.fnCalc = fnCalc;
        self.acBits = acBits;
        self.fSrc1And2Are32Bit = fSrc1And2Are32Bit;
        self.fSrc3IsZero       = fSrc3IsZero; # zero and omitted

    def generateBody(self, oOptions, cLeftToAllCheck):
        for cBits in self.acBits:
            cBitsIn1 = cBits if not self.fSrc1And2Are32Bit else 32;
            cBitsIn2 = cBitsIn1;
            for _ in range(oOptions.cTestsPerInstruction):
                (uSrc1, iRegIn1) = self.allocGprAndLoadRandUBits();
                (uSrc2, iRegIn2) = self.allocGprAndLoadRandUBits();
                (uSrc3, iRegIn3) = self.allocGprAndLoadRandUBits() if not self.fSrc3IsZero else (0, -1);
                uRes             = self.fnCalc(cBits, uSrc1, uSrc2, uSrc3);
                iRegDst          = self.oGprAllocator.alloc(uRes);
                if not self.fSrc3IsZero:
                    self.emitInstr(self.sInstr,
                                   '%s, %s, %s, %s'
                                   % (g_ddGprNamesZrByBits[cBits][iRegDst], g_ddGprNamesZrByBits[cBitsIn1][iRegIn1],
                                      g_ddGprNamesZrByBits[cBitsIn2][iRegIn2], g_ddGprNamesZrByBits[cBits][iRegIn3]));
                else:
                    self.emitInstr(self.sInstr,
                                   '%s, %s, %s'
                                   % (g_ddGprNamesZrByBits[cBits][iRegDst], g_ddGprNamesZrByBits[cBitsIn1][iRegIn1],
                                      g_ddGprNamesZrByBits[cBitsIn2][iRegIn2]));
                self.emitGprValCheck(iRegDst, uRes);

                self.oGprAllocator.freeList((iRegDst, iRegIn1, iRegIn2, iRegIn3,));
                cLeftToAllCheck = self.maybeEmitAllGprChecks(cLeftToAllCheck, oOptions);

def calcMAdd(cBits, uSrc1, uSrc2, uSrc3):
    return ((uSrc1 * uSrc2) + uSrc3) & bitsOnes(cBits);

def calcMSub(cBits, uSrc1, uSrc2, uSrc3):
    return (uSrc3 - (uSrc1 * uSrc2)) & bitsOnes(cBits);

def calcSMAddL(cBits, uSrc1, uSrc2, uSrc3):
    #return ((bitsSignedToInt(32, uSrc1) * bitsSignedToInt(32, uSrc2)) + bitsSignedToInt(cBits, uSrc3)) & bitsOnes(cBits);
    return (  ((bitsSignExtend(32, uSrc1, cBits) * bitsSignExtend(32, uSrc2, cBits)) + bitsSignedToInt(cBits, uSrc3))
            & bitsOnes(cBits));

def calcSMSubL(cBits, uSrc1, uSrc2, uSrc3):
    #return (bitsSignedToInt(cBits, uSrc3) - (bitsSignedToInt(32, uSrc1) * bitsSignedToInt(32, uSrc2))) & bitsOnes(cBits);
    return (  (bitsSignedToInt(cBits, uSrc3) - (bitsSignExtend(32, uSrc1, cBits) * bitsSignExtend(32, uSrc2, cBits)))
            & bitsOnes(cBits));

def calcSMulH(cBits, uSrc1, uSrc2, uSrc3):
    return (  (   (  (bitsSignExtend(cBits, uSrc1, cBits * 2) * bitsSignExtend(cBits, uSrc2, cBits * 2))
                   + bitsSignedToInt(cBits, uSrc3))
               >> cBits)
            & bitsOnes(cBits));

def calcUMAddL(cBits, uSrc1, uSrc2, uSrc3):
    return (((uSrc1 & bitsOnes(32)) * (uSrc2 & bitsOnes(32))) + uSrc3) & bitsOnes(cBits);

def calcUMSubL(cBits, uSrc1, uSrc2, uSrc3):
    return (uSrc3 - ((uSrc1 & bitsOnes(32)) * (uSrc2 & bitsOnes(32)))) & bitsOnes(cBits);

def calcUMulH(cBits, uSrc1, uSrc2, uSrc3):
    return (((uSrc1 * uSrc2) + uSrc3) >> cBits) & bitsOnes(cBits);


#
# Branches.
#

class A64No1CodeGenTestBranch(A64No1CodeGenBase):
    """ C4.1.93.16 Test and branch (immediate) """
    def __init__(self, sInstr, fJumpIfSet):
        A64No1CodeGenBase.__init__(self, sInstr, sInstr, Arm64GprAllocator());
        self.fJumpIfSet = fJumpIfSet;
        assert self.oGprAllocator.auValues[31] is None;

    def generateBody(self, oOptions, cLeftToAllCheck):
        for _ in range(oOptions.cTestsPerInstruction):
            assert self.oGprAllocator.auValues[31] is None;
            (uSrc, iRegIn)      = self.allocGprAndLoadRandUBits();
            assert self.oGprAllocator.auValues[31] is None;
            iBitNo              = randUBits(6);
            sJmpLabel           = self.localLabel();
            self.emitInstr(self.sInstr, '%s, #%u, %s' % (g_dGpr64NamesZr[iRegIn], iBitNo, sJmpLabel,));
            if (((uSrc >> iBitNo) & 1) != 0) == self.fJumpIfSet:
                self.emitBrk();
                self.emitLabel(sJmpLabel);
            else:
                sJmpLabel2      = self.localLabel();
                self.emitInstr('b', sJmpLabel2);
                self.emitLabel(sJmpLabel);
                self.emitBrk();
                self.emitLabel(sJmpLabel2);

            self.oGprAllocator.free(iRegIn);
            assert self.oGprAllocator.auValues[31] is None;
            cLeftToAllCheck = self.maybeEmitAllGprChecks(cLeftToAllCheck, oOptions);



#
# Load generators.
#

class A64No1CodeGenLdBase(A64No1CodeGenBase):
    """ Base class for load tests. """
    def __init__(self, sName, sInstr, fnCalc, cbMem = 1, cBits = None):
        A64No1CodeGenBase.__init__(self, sName, sInstr, Arm64GprAllocator(), fMayUseSp = True);
        self.fnCalc  = fnCalc;
        self.cbMem   = cbMem;
        self.cBits   = cBits if cBits is not None else 32 << (cbMem >= 8);

    @staticmethod
    def getSlotValue(iSlot, cbMem):
        """ Returns the values from slots 'iSlot' thru 'iSlot + cbMem - '. """
        uRet = iSlot;
        off  = 1;
        while off < cbMem:
            uRet |= (iSlot + off) << (off * 8);
            off  += 1;
        return uRet;

    def generateBody(self, oOptions, cLeftToAllCheck):
        # Load a pointer to the first slot into a register.
        iRegSlot0Ptr = self.oGprAllocator.allocNo31();
        self.emitInstr('adrp',  'x%u, PAGE(NAME(g_ReadArea))' % (iRegSlot0Ptr,));
        self.emitInstr('add',   'x%u, x%u, PAGEOFF(NAME(g_ReadArea))' % (iRegSlot0Ptr, iRegSlot0Ptr,));

        self.generateBodyLd(oOptions, cLeftToAllCheck, iRegSlot0Ptr)

        self.oGprAllocator.free(iRegSlot0Ptr);

    def generateBodyLd(self, oOptions, cLeftToAllCheck, iRegSlot0Ptr):
        """ Inner body generator. """
        _ = oOptions;
        _ = cLeftToAllCheck;
        _ = iRegSlot0Ptr;
        assert False;


def calcLdUnsigned(cBits, uSrc, cbMem):
    _ = cBits; _ = cbMem;
    return uSrc;

def calcLdSigned(cBits, uSrc, cbMem):
    cSrcBits = cbMem * 8;
    assert cSrcBits < cBits;
    if uSrc & (1 << (cSrcBits - 1)):
        uSrc |= ((1 << (cBits - cSrcBits)) - 1) << cSrcBits;
    return uSrc;


class A64No1CodeGenLdImm9(A64No1CodeGenLdBase):
    """
    LDR w/ 9-bit signed immediate and either post-index or pre-index adding.

    Note! SP can be used as base register (but not destination).
    """

    def __init__(self, sInstr, fnCalc = calcLdUnsigned, cbMem = 1, cBits = None, sType = None, cbAlign = 1, sBaseName = None):
        assert sType in { 'preidx', 'postidx', 'unscaled' };
        A64No1CodeGenLdBase.__init__(self, (sBaseName or sInstr) + '_' + sType, sInstr, fnCalc, cbMem, cBits);
        self.sType = sType;
        self.cbAlign = cbAlign;

    def generateBodyLd(self, oOptions, cLeftToAllCheck, iRegSlot0Ptr):
        # Generate loads from different loads with various immediate values.
        for i in range(oOptions.cTestsPerInstruction):
            uImm9       = randUBits(9);                  # (signed)
            iOffset     = bitsSignedToInt(9, uImm9);
            idxMemSlot  = randURange(0, 256 - self.cbMem);
            idxMemSlot -= idxMemSlot & (self.cbAlign - 1);
            idxPreSlot  = idxMemSlot - iOffset if self.sType != 'postidx' else idxMemSlot;

            iRegBase    = self.oGprAllocator.allocEx(fReg31IsSp = True);
            sRegBase    = g_dGpr64NamesSp[iRegBase];
            if iRegBase == 31 and (abs(idxPreSlot) & 15) != 0: # SP value must be 16 byte aligned in most host OS contexts.
                idxMemSlot -= idxPreSlot & 15
                if idxMemSlot < 0:
                    idxMemSlot += 16;
                if self.cbAlign > 1 and (idxMemSlot & (self.cbAlign - 1)) and self.sType != 'postidx':
                    iOffset    -= idxMemSlot & (self.cbAlign - 1);
                    idxMemSlot -= idxMemSlot & (self.cbAlign - 1);
                idxPreSlot = idxMemSlot - iOffset if self.sType != 'postidx' else idxMemSlot;
                assert (abs(idxPreSlot) & 15) == 0;

            uSrc        = self.getSlotValue(idxMemSlot, self.cbMem);
            uRes        = self.fnCalc(self.cBits, uSrc, self.cbMem);
            self.emitCommentLine('i=%u iOffset=%#x idxMemSlot=%#x uRes=%#x' % (i, iOffset, idxMemSlot, uRes,));

            # Calculate the base address and perform the load instruction.
            self.emitInstr('add' if idxPreSlot >= 0 else 'sub', '%s, x%u, #%u' % (sRegBase, iRegSlot0Ptr, abs(idxPreSlot),));
            iRegDst     = self.emitLdInstrAndAllocDst(sRegBase, iOffset, uRes);

            # Check the offset.
            if self.sType == 'preidx':      u64PostSlot = idxMemSlot;
            elif self.sType == 'postidx':   u64PostSlot = (idxMemSlot + iOffset) & 0xffffffffffffffff;
            else:                           u64PostSlot = idxPreSlot             & 0xffffffffffffffff;
            iRegTmp     = self.oGprAllocator.allocNo31(u64PostSlot);
            self.emitInstr('sub', 'x%u, %s, x%u' % (iRegTmp, sRegBase, iRegSlot0Ptr,)); # iRegTmp == idxEffSlot
            self.emitGprValCheck(iRegTmp, u64PostSlot);
            self.oGprAllocator.free(iRegTmp);

            # Check the value.
            self.emitLdValCheckAndFreeDst(iRegDst, uRes);

            self.oGprAllocator.free(iRegBase);
            cLeftToAllCheck = self.maybeEmitAllGprChecks(cLeftToAllCheck, oOptions);

    def emitLdInstrAndAllocDst(self, sRegBase, iOffset, uExpected):
        iRegDst = self.oGprAllocator.alloc(uExpected);
        if self.sType == 'preidx':
            self.emitInstr(self.sInstr, '%s, [%s, #%d]!' % (g_ddGprNamesZrByBits[self.cBits][iRegDst], sRegBase, iOffset,));
        elif self.sType == 'unscaled':
            self.emitInstr(self.sInstr, '%s, [%s, #%d]'  % (g_ddGprNamesZrByBits[self.cBits][iRegDst], sRegBase, iOffset,));
        else:
            self.emitInstr(self.sInstr, '%s, [%s], #%d'  % (g_ddGprNamesZrByBits[self.cBits][iRegDst], sRegBase, iOffset,));
        return iRegDst;

    def emitLdValCheckAndFreeDst(self, iRegDst, uExpected):
        self.emitGprValCheck(iRegDst, uExpected);
        self.oGprAllocator.free(iRegDst);


class A64No1CodeGenLdImm9Fp(A64No1CodeGenFprBase, A64No1CodeGenLdImm9):

    def __init__(self, sInstr, fnCalc, cbMem = 1, sType = None):
        sBaseName = sInstr + '_' + g_kdFprBytesToInfix[cbMem];
        A64No1CodeGenFprBase.__init__(self, sBaseName, sInstr, cbMem);
        A64No1CodeGenLdImm9.__init__(self, sInstr, fnCalc, cbMem, cbMem * 8, sType, sBaseName = sBaseName);

    def emitLdInstrAndAllocDst(self, sRegBase, iOffset, uExpected):
        iRegDst = self.oFprAllocator.alloc(uExpected);
        if self.sType == 'preidx':
            self.emitInstr(self.sInstr, '%s%u, [%s, #%d]!' % (self.sPrefix, iRegDst, sRegBase, iOffset,));
        elif self.sType == 'unscaled':
            self.emitInstr(self.sInstr, '%s%u, [%s, #%d]'  % (self.sPrefix, iRegDst, sRegBase, iOffset,));
        else:
            self.emitInstr(self.sInstr, '%s%u, [%s], #%d'  % (self.sPrefix, iRegDst, sRegBase, iOffset,));
        return iRegDst;

    def emitLdValCheckAndFreeDst(self, iRegDst, uExpected):
        self.emitFprValCheck(iRegDst, uExpected);
        self.oFprAllocator.free(iRegDst);


class A64No1CodeGenLdImm12(A64No1CodeGenLdBase):
    """
    LDR w/ 12-bit unsigned scaled immediate.

    Note! SP can be used as base register (but not destination).
    """

    def __init__(self, sInstr, fnCalc, cbMem = 1, cBits = None, sBaseName = None):
        if cBits is None:
            cBits = 64 if cbMem == 8 else 32;
        A64No1CodeGenLdBase.__init__(self, sBaseName or sInstr, sInstr, fnCalc, cbMem, cBits);

    def generateBodyLd(self, oOptions, cLeftToAllCheck, iRegSlot0Ptr):
        # Generate loads from different loads with various immediate values.
        for i in range(oOptions.cTestsPerInstruction):
            uImm12      = randUBits(12);
            uOffset     = uImm12 * self.cbMem;
            idxMemSlot  = randURange(0, 256 - self.cbMem);
            idxPreSlot  = idxMemSlot - uOffset;

            iRegBase    = self.oGprAllocator.allocEx(fReg31IsSp = True);
            sRegBase    = g_dGpr64NamesSp[iRegBase];
            if iRegBase == 31 and (abs(idxPreSlot) & 15) != 0: # SP value must be 16 byte aligned in most host OS contexts.
                idxMemSlot -= idxPreSlot & 15
                if idxMemSlot < 0:
                    idxMemSlot += 16;
                idxPreSlot = idxMemSlot - uOffset;
                assert (abs(idxPreSlot) & 15) == 0;

            uSrc        = self.getSlotValue(idxMemSlot, self.cbMem);
            uRes        = self.fnCalc(self.cBits, uSrc, self.cbMem);
            self.emitCommentLine('i=%u uOffset=%#x idxMemSlot=%#x uRes=%#x' % (i, uOffset, idxMemSlot, uRes,));

            # Calculate the base address and perform the load instruction.
            self.emitBaseRegAddrCalc(sRegBase, iRegSlot0Ptr, idxPreSlot);
            iRegBaseCp  = self.emitBaseAddrSaveInNewRegAlloc(sRegBase);
            iRegDst     = self.emitLdInstrAndAllocDst(sRegBase, uOffset, uRes);

            # Check the offset.
            self.emitBaseAddrCheckAgainstRegAndFreeIt(sRegBase, iRegBaseCp);

            # Check the value.
            self.emitLdValCheckAndFreeDst(iRegDst, uRes);

            self.oGprAllocator.free(iRegBase);
            cLeftToAllCheck = self.maybeEmitAllGprChecks(cLeftToAllCheck, oOptions);

    def emitLdInstrAndAllocDst(self, sRegBase, uOffset, uExpected):
        iRegDst = self.oGprAllocator.alloc(uExpected);
        self.emitInstr(self.sInstr, '%s, [%s, #%u]' % (g_ddGprNamesZrByBits[self.cBits][iRegDst], sRegBase, uOffset,));
        return iRegDst;

    def emitLdValCheckAndFreeDst(self, iRegDst, uExpected):
        self.emitGprValCheck(iRegDst, uExpected);
        self.oGprAllocator.free(iRegDst);


class A64No1CodeGenLdImm12Fp(A64No1CodeGenFprBase, A64No1CodeGenLdImm12):
    def __init__(self, sInstr, fnCalc, cbMem = 1):
        sBaseName = sInstr + '_' + g_kdFprBytesToInfix[cbMem];
        A64No1CodeGenFprBase.__init__(self, sBaseName, sInstr, cbMem);
        A64No1CodeGenLdImm12.__init__(self, sInstr, fnCalc, cbMem, cbMem * 8, sBaseName = sBaseName);

    def emitLdInstrAndAllocDst(self, sRegBase, uOffset, uExpected):
        iRegDst = self.oFprAllocator.alloc(uExpected);
        self.emitInstr(self.sInstr, '%s%u, [%s, #%u]' % (self.sPrefix, iRegDst, sRegBase, uOffset,));
        return iRegDst;

    def emitLdValCheckAndFreeDst(self, iRegDst, uExpected):
        self.emitFprValCheck(iRegDst, uExpected);
        self.oFprAllocator.free(iRegDst);


class A64No1CodeGenLdReg(A64No1CodeGenLdBase):
    """
    LDR w/ index register.

    Note! SP can be used as base register (but not destination or index).
    """

    def __init__(self, sInstr, fnCalc, cbMem = 1, cBits = None, cShift = None, sName = None):
        if cBits is None:
            cBits = 64 if cbMem == 8 else 32;
        A64No1CodeGenLdBase.__init__(self, sName or sInstr, sInstr, fnCalc, cbMem, cBits);
        self.cShift = cShift if cShift is not None else cbMem.bit_length() - 1;

    kdOp = { 2: 'UXTW', 3: 'LSL', 6: 'SXTW', 7: 'SXTX', };

    def generateBodyLd(self, oOptions, cLeftToAllCheck, iRegSlot0Ptr):
        fMask64Bits = bitsOnes(64);
        # Generate loads from different loads with various immediate values.
        for i in range(oOptions.cTestsPerInstruction):
            idxMemSlot  = randURange(0, 256 - self.cbMem);
            iOption     = randUBits(3) | 2;
            cShift      = self.cShift if randBool() else 0;
            iRegIndex   = self.oGprAllocator.alloc();
            sRegIndex   = g_dGpr64NamesZr[iRegIndex] if iOption & 1 else g_dGpr32NamesZr[iRegIndex];
            uIndex      = randUBits(64) if iRegIndex != 31 else 0;
            if iOption == 2:        # UXTW
                uOffset = uIndex & 0xffffffff;
            elif iOption == 6:      # SXTW
                uOffset = bitsSignExtend(32, uIndex, 64);
            else:                   # LSL or SXTX
                uOffset = uIndex;
            uOffset   <<= cShift;
            uOffset    &= fMask64Bits;
            idxPreSlot  = (idxMemSlot - uOffset) & fMask64Bits;

            iRegBase    = self.oGprAllocator.allocEx(fReg31IsSp = True);
            sRegBase    = g_dGpr64NamesSp[iRegBase];
            if iRegBase == 31 and (idxPreSlot & 15) != 0: # SP value must be 16 byte aligned in most host OS contexts.
                idxMemSlot -= idxPreSlot & 15
                if idxMemSlot < 0:
                    idxMemSlot += 16;
                idxPreSlot  = (idxMemSlot - uOffset) & fMask64Bits;
                assert (idxPreSlot & 15) == 0;
            assert ((idxPreSlot + uOffset) & fMask64Bits) == idxMemSlot;

            uSrc        = self.getSlotValue(idxMemSlot, self.cbMem);
            uRes        = self.fnCalc(self.cBits, uSrc, self.cbMem);
            self.emitCommentLine('i=%u idxMemSlot=%#x uRes=%#x uOffset=%#x (uIndex=%#x %s %u) idxPreSlot=%#x'
                                 % (i, idxMemSlot, uRes, uOffset, uIndex, self.kdOp[iOption], cShift, idxPreSlot,));

            # Calculate the base address, load index and perform the load instruction.
            self.emitGprLoad(iRegBase, idxPreSlot, fTempValue = True);
            self.emitInstr('add', '%s, %s, x%u' % (sRegBase, sRegBase, iRegSlot0Ptr,));
            iRegBaseCp  = self.emitBaseAddrSaveInNewRegAlloc(sRegBase);
            if iRegIndex != 31:
                self.emitGprLoad(iRegIndex, uIndex);
            iRegDst     = self.emitLdInstrAndAllocDst(sRegBase, sRegIndex, self.kdOp[iOption], cShift, uRes);

            # Check the offset.
            self.emitBaseAddrCheckAgainstRegAndFreeIt(sRegBase, iRegBaseCp);

            # Check the value.
            self.emitLdValCheckAndFreeDst(iRegDst, uRes);

            self.oGprAllocator.free(iRegBase);
            self.oGprAllocator.free(iRegIndex);
            cLeftToAllCheck = self.maybeEmitAllGprChecks(cLeftToAllCheck, oOptions);

    def emitLdInstrAndAllocDst(self, sRegBase, sRegIndex, sOption, cShift, uExpected):
        iRegDst = self.oGprAllocator.alloc(uExpected);
        self.emitInstr(self.sInstr, '%s, [%s, %s, %s #%u]' % (g_ddGprNamesZrByBits[self.cBits][iRegDst],
                                                              sRegBase, sRegIndex, sOption, cShift,));
        return iRegDst;

    def emitLdValCheckAndFreeDst(self, iRegDst, uExpected):
        self.emitGprValCheck(iRegDst, uExpected);
        self.oGprAllocator.free(iRegDst);


class A64No1CodeGenLdRegFp(A64No1CodeGenFprBase, A64No1CodeGenLdReg):
    def __init__(self, sInstr, cbMem = 1):
        sName = sInstr + '_' + g_kdFprBytesToInfix[cbMem];
        A64No1CodeGenFprBase.__init__(self, sName, sInstr, cbMem);
        A64No1CodeGenLdReg.__init__(self, sInstr, fnCalc = calcLdUnsigned, cbMem = cbMem, cBits = cbMem * 8, sName = sName);

    def emitLdInstrAndAllocDst(self, sRegBase, sRegIndex, sOption, cShift, uExpected):
        iRegDst = self.oFprAllocator.alloc(uExpected);
        self.emitInstr(self.sInstr, '%s%u, [%s, %s, %s #%u]' % (self.sPrefix, iRegDst, sRegBase, sRegIndex, sOption, cShift,));
        return iRegDst;

    def emitLdValCheckAndFreeDst(self, iRegDst, uExpected):
        self.emitFprValCheck(iRegDst, uExpected);
        self.oFprAllocator.free(iRegDst);


class A64No1CodeGenLdMemOp(A64No1CodeGenLdBase):
    """
    LDAPR and such.

    Note! SP can be used as base register (but not destination).
    """

    def __init__(self, sInstr, fnCalc = calcLdUnsigned, cbMem = 1, cbAlign = None, cBits = None, offWriteback = 0, sName = None):
        if cBits is None:
            cBits = 64 if cbMem == 8 else 32;
        A64No1CodeGenLdBase.__init__(self, sName or sInstr, sInstr, fnCalc, cbMem, cBits);
        self.cbAlign      = cbAlign or cbMem;
        self.offWriteback = offWriteback;
        assert offWriteback == 0;

    def generateBodyLd(self, oOptions, cLeftToAllCheck, iRegSlot0Ptr):
        # Generate loads from different slots.
        for i in range(oOptions.cTestsPerInstruction):
            idxMemSlot  = randURange(0, 256 - self.cbMem);
            idxMemSlot -= idxMemSlot & (self.cbAlign - 1);

            iRegBase    = self.oGprAllocator.allocEx(fReg31IsSp = True);
            sRegBase    = g_dGpr64NamesSp[iRegBase];
            if iRegBase == 31 and (idxMemSlot & 15) != 0: # SP value must be 16 byte aligned in most host OS contexts.
                idxMemSlot -= idxMemSlot & 15

            uSrc        = self.getSlotValue(idxMemSlot, self.cbMem);
            uRes        = self.fnCalc(self.cBits, uSrc, self.cbMem);
            self.emitCommentLine('i=%u idxMemSlot=%#x uRes=%#x' % (i, idxMemSlot, uRes,));

            # Calculate the base address, load index and perform the load instruction.
            self.emitBaseRegAddrCalc(sRegBase, iRegSlot0Ptr, idxMemSlot)
            iRegBaseCp  = self.emitBaseAddrSaveInNewRegAlloc(sRegBase);
            iRegDst     = self.emitLdInstrAndAllocDst(sRegBase, self.offWriteback, uRes);

            # Check the offset.
            self.emitBaseAddrCheckAgainstRegAndFreeIt(sRegBase, iRegBaseCp);

            # Check the value.
            self.emitLdValCheckAndFreeDst(iRegDst, uRes);

            self.oGprAllocator.free(iRegBase);
            cLeftToAllCheck = self.maybeEmitAllGprChecks(cLeftToAllCheck, oOptions);

    def emitLdInstrAndAllocDst(self, sRegBase, offWriteback, uExpected):
        iRegDst = self.oGprAllocator.alloc(uExpected);
        if offWriteback == 0:
            self.emitInstr(self.sInstr, '%s, [%s]' % (g_ddGprNamesZrByBits[self.cBits][iRegDst], sRegBase,));
        elif offWriteback > 0:
            self.emitInstr(self.sInstr, '%s, [%s], #%u' % (g_ddGprNamesZrByBits[self.cBits][iRegDst], sRegBase, offWriteback,));
        else:
            self.emitInstr(self.sInstr, '%s, [%s, #%u]!' % (g_ddGprNamesZrByBits[self.cBits][iRegDst], sRegBase, offWriteback,));
        return iRegDst;

    def emitLdValCheckAndFreeDst(self, iRegDst, uExpected):
        self.emitGprValCheck(iRegDst, uExpected);
        self.oGprAllocator.free(iRegDst);


class A64No1CodeGenLdLiteral(A64No1CodeGenLdBase):
    """
    LDR with PC relative immediate.

    We cannot easily test the full immediate range here, as we don't have suffcient
    control over where the emitted code is placed stuff and/or don't want to blow
    it up to more than 2MB in size.  Instead we place data in these locations:
        0. Before the test function (largish negative offsets).
        1. After the test function (largish positive offsets).
        2. 0 to 16 instructions before the LDR (small negative offsets).
        3. 0 to 16 instructions after the LDR (small positive offsets).

    Note! SP cannot be the destination register.
    """

    def __init__(self, sInstr, fnCalc, cbMem = 1, cBits = None, sName = None):
        if cBits is None:
            cBits = 64 if cbMem == 8 else 32;
        A64No1CodeGenLdBase.__init__(self, sName or sInstr, sInstr, fnCalc, cbMem, cBits);
        self.asDataBefore = [];
        self.asDataAfter  = [];

    def generate(self, oOptions):
        # Generate the test code.
        super().generate(oOptions);
        # Insert in the data before and after the code.
        self.asCode = self.asDataBefore + self.asCode + self.asDataAfter;

    def generateBodyLd(self, oOptions, cLeftToAllCheck, iRegSlot0Ptr):
        for i in range(oOptions.cTestsPerInstruction):
            # The data value.
            uValue    = randUBits(self.cbMem * 8);
            sAsmValue = '.int '
            for iValuePart in range(self.cbMem // 4):
                sAsmValue += '%s%#x' % (', ' if iValuePart > 0 else '', (uValue >> (iValuePart * 32)) & 0xffffffff,);
            uRes      = self.fnCalc(self.cBits, uValue, self.cbMem);

            # Figure out where the data goes and emit the instruction.
            iDataLoc  = randUBits(2);
            if iDataLoc >= 2:
                # Small offset - data embedded into the code.
                sDataLabel = '.Ls_%s_%u' % (self.sLabel, i,);
                sJumpLabel = '.Ljmp_%s_%u' % (self.sLabel, i,);
                uOff       = randUBits(4) * 4;
                self.emitCommentLine('i=%u iDataLoc=%u uValue=%#x (uRes=%#x) uOff=%u' % (i, iDataLoc, uValue, uRes, uOff));
                if iDataLoc == 2:
                    # before
                    self.emitInstr('b', sJumpLabel);
                    self.asCode.append('%s: %s' % (sDataLabel, sAsmValue,));
                    if uOff > 0:
                        self.emitInstr('.int', ','.join(['%#x' % (randU32(),) for _ in range(uOff)]), 'padding');
                    self.emitLabel(sJumpLabel);
                    iRegDst = self.emitLdInstrAndAllocDst(sDataLabel, uRes);
                else:
                    # after
                    iRegDst = self.emitLdInstrAndAllocDst(sDataLabel, uRes);
                    self.emitInstr('b', sJumpLabel);
                    if uOff > 0:
                        self.emitInstr('.int', ','.join(['%#x' % (randU32(),) for _ in range(uOff)]), 'padding');
                    self.asCode.append('%s: %s' % (sDataLabel, sAsmValue,));
                    self.emitLabel(sJumpLabel);
            else:
                # Largish offset - either before or after the test function.
                self.emitCommentLine('i=%u iDataLoc=%u uValue=%#x (uRes=%#x)' % (i, iDataLoc, uValue, uRes,));
                sDataLabel = 'g_%s_%u' % (self.sLabel, i,);
                if iDataLoc == 0:
                    self.asDataBefore.append('%s: %s' % (sDataLabel, sAsmValue,));
                else:
                    self.asDataAfter.append('%s: %s' % (sDataLabel, sAsmValue,));
                iRegDst = self.emitLdInstrAndAllocDst(sDataLabel, uRes);

            # Check the value.
            self.emitLdValCheckAndFreeDst(iRegDst, uRes);

            cLeftToAllCheck = self.maybeEmitAllGprChecks(cLeftToAllCheck, oOptions);

    def emitLdInstrAndAllocDst(self, sDataLabel, uExpected):
        iRegDst = self.oGprAllocator.alloc(uExpected);
        self.emitInstr(self.sInstr, '%s, %s' % (g_ddGprNamesZrByBits[self.cBits][iRegDst], sDataLabel,));
        return iRegDst;

    def emitLdValCheckAndFreeDst(self, iRegDst, uExpected):
        self.emitGprValCheck(iRegDst, uExpected);
        self.oGprAllocator.free(iRegDst);


class A64No1CodeGenLdLiteralFp(A64No1CodeGenFprBase, A64No1CodeGenLdLiteral):
    def __init__(self, sInstr, cbMem = 1):
        sName = sInstr + '_' + g_kdFprBytesToInfix[cbMem];
        A64No1CodeGenFprBase.__init__(self, sName, sInstr, cbMem);
        A64No1CodeGenLdLiteral.__init__(self, sInstr, fnCalc = calcLdUnsigned, cbMem = cbMem, cBits = cbMem * 8, sName = sName);

    def emitLdInstrAndAllocDst(self, sDataLabel, uExpected):
        iRegDst = self.oFprAllocator.alloc(uExpected);
        self.emitInstr(self.sInstr, '%s%u, %s' % (self.sPrefix, iRegDst, sDataLabel,));
        return iRegDst;

    def emitLdValCheckAndFreeDst(self, iRegDst, uExpected):
        self.emitFprValCheck(iRegDst, uExpected);
        self.oFprAllocator.free(iRegDst);


class A64No1CodeGenLdpImm7(A64No1CodeGenLdBase):
    """
    LDP w/ 7-bit signed immediate, optionally with post-index or pre-index advancing.

    Note! SP can be used as base register (but not destination).
    """

    def __init__(self, sInstr, fnCalc, cbMem = 1, cBits = None, sType = None, sBaseName = None):
        assert sType in { 'preidx', 'postidx', 'signed' };
        A64No1CodeGenLdBase.__init__(self, (sBaseName or sInstr) + '_' + sType, sInstr, fnCalc, cbMem, cBits or (cbMem * 4));
        self.sType = sType;

    def generateBodyLd(self, oOptions, cLeftToAllCheck, iRegSlot0Ptr):
        # Generate loads from different loads with various immediate values.
        cbMemHalf = self.cbMem // 2;
        for i in range(oOptions.cTestsPerInstruction):
            uImm7       = randUBits(7);                  # (signed)
            iOffset     = bitsSignedToInt(7, uImm7) * cbMemHalf;
            idxMemSlot  = randURange(0, 256 - self.cbMem);
            idxPreSlot  = idxMemSlot - iOffset if self.sType != 'postidx' else idxMemSlot;

            iRegBase    = self.oGprAllocator.allocEx(fReg31IsSp = True);
            sRegBase    = g_dGpr64NamesSp[iRegBase];
            if iRegBase == 31 and (abs(idxPreSlot) & 15) != 0: # SP value must be 16 byte aligned in most host OS contexts.
                idxMemSlot -= idxPreSlot & 15
                if idxMemSlot < 0:
                    idxMemSlot += 16;
                idxPreSlot = idxMemSlot - iOffset if self.sType != 'postidx' else idxMemSlot;
                assert (abs(idxPreSlot) & 15) == 0;

            uSrc1       = self.getSlotValue(idxMemSlot, cbMemHalf);
            uSrc2       = self.getSlotValue(idxMemSlot + cbMemHalf, cbMemHalf);
            uRes1       = self.fnCalc(self.cBits, uSrc1, cbMemHalf);
            uRes2       = self.fnCalc(self.cBits, uSrc2, cbMemHalf);
            self.emitCommentLine('i=%u iOffset=%#x idxMemSlot=%#x uRes1=%#x uRes2=%#x' % (i, iOffset, idxMemSlot, uRes1, uRes2));

            # Calculate the base address and perform the load instruction.
            self.emitBaseRegAddrCalc(sRegBase, iRegSlot0Ptr, idxPreSlot);
            (iRegDst1, iRegDst2) = self.emitLdInstrAndAllocDst(sRegBase, iOffset, uRes1, uRes2);

            # Check the offset.
            if self.sType == 'preidx':      u64PostSlot = idxMemSlot;
            elif self.sType == 'postidx':   u64PostSlot = (idxMemSlot + iOffset) & 0xffffffffffffffff;
            else:                           u64PostSlot = idxPreSlot             & 0xffffffffffffffff;
            iRegTmp     = self.oGprAllocator.allocNo31(u64PostSlot);
            self.emitInstr('sub', 'x%u, %s, x%u' % (iRegTmp, sRegBase, iRegSlot0Ptr,)); # iRegTmp == idxEffSlot
            self.emitGprValCheck(iRegTmp, u64PostSlot);
            self.oGprAllocator.free(iRegTmp);

            # Check the values.
            self.emitLdValCheckAndFreeDst(iRegDst1, uRes1);
            self.emitLdValCheckAndFreeDst(iRegDst2, uRes2);

            self.oGprAllocator.free(iRegBase);
            cLeftToAllCheck = self.maybeEmitAllGprChecks(cLeftToAllCheck, oOptions);

    def emitLdInstrAndAllocDst(self, sRegBase, iOffset, uExpected1, uExpected2):
        iRegDst1 = self.oGprAllocator.alloc(uExpected1);
        iRegDst2 = self.oGprAllocator.alloc(uExpected2);
        if self.sType == 'preidx':
            self.emitInstr(self.sInstr, '%s, %s, [%s, #%d]!'
                           % (g_ddGprNamesZrByBits[self.cBits][iRegDst1], g_ddGprNamesZrByBits[self.cBits][iRegDst2],
                              sRegBase, iOffset,));
        elif self.sType == 'signed':
            self.emitInstr(self.sInstr, '%s, %s, [%s, #%d]'
                           % (g_ddGprNamesZrByBits[self.cBits][iRegDst1], g_ddGprNamesZrByBits[self.cBits][iRegDst2],
                              sRegBase, iOffset,));
        else:
            self.emitInstr(self.sInstr, '%s, %s, [%s], #%d'
                           % (g_ddGprNamesZrByBits[self.cBits][iRegDst1], g_ddGprNamesZrByBits[self.cBits][iRegDst2],
                              sRegBase, iOffset,));
        return (iRegDst1, iRegDst2);

    def emitLdValCheckAndFreeDst(self, iRegDst, uExpected):
        self.emitGprValCheck(iRegDst, uExpected);
        self.oGprAllocator.free(iRegDst);


class A64No1CodeGenLdpImm7Fp(A64No1CodeGenFprBase, A64No1CodeGenLdpImm7):

    def __init__(self, sInstr, cbMem = 1, sType = None):
        sBaseName = sInstr + '_' + g_kdFprBytesToInfix[cbMem // 2];
        A64No1CodeGenFprBase.__init__(self, sBaseName, sInstr, cbMem // 2);
        A64No1CodeGenLdpImm7.__init__(self, sInstr, calcLdUnsigned, cbMem, cbMem * 4, sType, sBaseName = sBaseName);

    def emitLdInstrAndAllocDst(self, sRegBase, iOffset, uExpected1, uExpected2):
        iRegDst1 = self.oFprAllocator.alloc(uExpected1);
        iRegDst2 = self.oFprAllocator.alloc(uExpected2);
        if self.sType == 'preidx':
            self.emitInstr(self.sInstr, '%s%u, %s%u, [%s, #%d]!'
                           % (self.sPrefix, iRegDst1, self.sPrefix, iRegDst2, sRegBase, iOffset,));
        elif self.sType == 'signed':
            self.emitInstr(self.sInstr, '%s%u, %s%u, [%s, #%d]'
                           % (self.sPrefix, iRegDst1, self.sPrefix, iRegDst2, sRegBase, iOffset,));
        else:
            self.emitInstr(self.sInstr, '%s%u, %s%u, [%s], #%d'
                           % (self.sPrefix, iRegDst1, self.sPrefix, iRegDst2, sRegBase, iOffset,));
        return (iRegDst1, iRegDst2);

    def emitLdValCheckAndFreeDst(self, iRegDst, uExpected):
        self.emitFprValCheck(iRegDst, uExpected);
        self.oFprAllocator.free(iRegDst);


#
# Store generators.
#

class A64No1CodeGenStBase(A64No1CodeGenBase):
    """ Base class for store tests. """
    def __init__(self, sName, sInstr, cbMem = 1, cBits = None, oGprAllocator = None):
        A64No1CodeGenBase.__init__(self, sName, sInstr,
                                   oGprAllocator or Arm64GprAllocator(bmPreallocated = 0x4000003f), # Excl. CheckWriteArea() regs.
                                   fMayUseSp = True);
        self.cbMem   = cbMem;
        self.cBits   = cBits if cBits is not None else 32 << (cbMem >= 8);

    @staticmethod
    def getSlotValue(iSlot, cbMem):
        """ Returns the values from slots 'iSlot' thru 'iSlot + cbMem - '. """
        uRet = iSlot;
        off  = 1;
        while off < cbMem:
            uRet |= (iSlot + off) << (off * 8);
            off  += 1;
        return uRet;

    def generateBody(self, oOptions, cLeftToAllCheck):
        # Load a pointer to the first slot into a register.
        iRegSlot0Ptr = self.oGprAllocator.allocNo31();
        self.emitInstr('adrp',  'x%u, PAGE(NAME(g_WriteArea))' % (iRegSlot0Ptr,));
        self.emitInstr('add',   'x%u, x%u, PAGEOFF(NAME(g_WriteArea))' % (iRegSlot0Ptr, iRegSlot0Ptr,));

        self.generateBodySt(oOptions, iRegSlot0Ptr)

        self.emitAllGprChecks();
        self.oGprAllocator.free(iRegSlot0Ptr);
        _ = cLeftToAllCheck;

    def generateBodySt(self, oOptions, iRegSlot0Ptr):
        """ Inner body generator. """
        _ = oOptions;
        _ = iRegSlot0Ptr;
        assert False;

    def emitStValCheck(self, idxMemSlot, uValue, iRegSlot0Ptr):
        #
        # We must check bytes both before and after. Figure out how much exact.
        #
        cbToCheck = self.cbMem;

        cbBefore  = idxMemSlot & 3;
        if cbBefore == 0:
            cbBefore = 2;
        cbBefore = min(cbBefore, idxMemSlot);
        cbToCheck += cbBefore;
        for _ in range(cbBefore):
            idxMemSlot -= 1;
            uValue    <<= 8;
            uValue     |= idxMemSlot;

        cbAfter   = ((cbToCheck + 3) & ~3) - cbToCheck;
        if cbAfter == 0:
            cbAfter = 2;
        cbAfter = min(256 - cbToCheck + idxMemSlot, cbAfter);
        for _ in range(cbAfter):
            uValue    |= (idxMemSlot + cbToCheck) << (cbToCheck * 8);
            cbToCheck += 1;

        #
        # Do the checking.
        #
        self.emitCommentLine('checking: idxMemSlot=%#x LB %u (cbBefore=%u cbMem=%u cbAfter=%u) == %#x'
                             % (idxMemSlot, cbToCheck, cbBefore, self.cbMem, cbAfter, uValue,));
        iRegLoad   = self.oGprAllocator.allocNo31();
        iRegExpect = self.oGprAllocator.allocNo31();
        while cbToCheck > 0:
            if (idxMemSlot & 7) == 0 and cbToCheck >= 8:
                self.emitGprLoad(iRegExpect, uValue & bitsOnes(64));
                self.emitInstr('ldr', '%s, [%s, #%d]' % (g_dGpr64NamesZr[iRegLoad], g_dGpr64NamesSp[iRegSlot0Ptr], idxMemSlot,));
                uValue    >>= 64;
                idxMemSlot += 8;
                cbToCheck  -= 8;
            elif (idxMemSlot & 3) == 0 and cbToCheck >= 4:
                self.emitGprLoad(iRegExpect, uValue & bitsOnes(32));
                self.emitInstr('ldr', '%s, [%s, #%d]' % (g_dGpr32NamesZr[iRegLoad], g_dGpr64NamesSp[iRegSlot0Ptr], idxMemSlot,));
                uValue    >>= 32;
                idxMemSlot += 4;
                cbToCheck  -= 4;
            else:
                assert (idxMemSlot & 1) == 0 and cbToCheck >= 2, 'idxMemSlot=%#x cbToCheck=%u' % (idxMemSlot, cbToCheck,);
                self.emitGprLoad(iRegExpect, uValue & bitsOnes(16));
                self.emitInstr('ldrh', '%s, [%s, #%d]' % (g_dGpr32NamesZr[iRegLoad], g_dGpr64NamesSp[iRegSlot0Ptr], idxMemSlot,));
                uValue    >>= 16;
                idxMemSlot += 2;
                cbToCheck  -= 2;
            self.emitInstr('cmp', '%s, %s' % (g_dGpr64NamesZr[iRegLoad], g_dGpr64NamesZr[iRegExpect],));
            sLabel = self.localLabel();
            self.emitInstr('b.eq', sLabel);
            self.emitBrk();
            self.emitLabel(sLabel);

        self.oGprAllocator.free(iRegExpect);
        self.oGprAllocator.free(iRegLoad);


class A64No1CodeGenStImm9(A64No1CodeGenStBase):
    """
    STR w/ 9-bit signed immediate and either post-index or pre-index adding.

    Note! SP can be used as base register (but not destination).
    """

    def __init__(self, sInstr, cbMem = 1, cBits = None, sType = None, sBaseName = None):
        assert sType in { 'preidx', 'postidx', 'unscaled' };
        A64No1CodeGenStBase.__init__(self, (sBaseName or sInstr) + '_' + sType, sInstr, cbMem, cBits);
        self.sType = sType;

    def generateBodySt(self, oOptions, iRegSlot0Ptr):
        self.emitInstr('bl', 'NAME(CheckWriteArea)');

        # Generate loads from different loads with various immediate values.
        for i in range(oOptions.cTestsPerInstruction):
            uImm9       = randUBits(9);                 # (signed)
            iOffset     = bitsSignedToInt(9, uImm9);
            idxMemSlot  = randURange(0, 256 - self.cbMem);
            idxPreSlot  = idxMemSlot - iOffset if self.sType != 'postidx' else idxMemSlot;

            iRegBase    = self.oGprAllocator.allocEx(fReg31IsSp = True);
            sRegBase    = g_dGpr64NamesSp[iRegBase];
            if iRegBase == 31 and (abs(idxPreSlot) & 15) != 0: # SP value must be 16 byte aligned in most host OS contexts.
                idxMemSlot -= idxPreSlot & 15
                if idxMemSlot < 0:
                    idxMemSlot += 16;
                idxPreSlot = idxMemSlot - iOffset if self.sType != 'postidx' else idxMemSlot;
                assert (abs(idxPreSlot) & 15) == 0;
            self.emitCommentLine('i=%u idxMemSlot=%#x' % (i, idxMemSlot,));

            # Calculate the base address, allocate and load the source register, then perform the store instruction.
            self.emitInstr('add' if idxPreSlot >= 0 else 'sub', '%s, x%u, #%u' % (sRegBase, iRegSlot0Ptr, abs(idxPreSlot),));
            uValue      = self.emitStInstrAndRandValue(sRegBase, iOffset);

            # Check the offset.
            if self.sType == 'preidx':      u64PostSlot = idxMemSlot;
            elif self.sType == 'postidx':   u64PostSlot = (idxMemSlot + iOffset) & 0xffffffffffffffff;
            else:                           u64PostSlot = idxPreSlot             & 0xffffffffffffffff;
            iRegTmp     = self.oGprAllocator.allocNo31(u64PostSlot);
            self.emitInstr('sub', 'x%u, %s, x%u' % (iRegTmp, sRegBase, iRegSlot0Ptr,)); # iRegTmp == idxEffSlot
            self.emitGprValCheck(iRegTmp, u64PostSlot);
            self.oGprAllocator.free(iRegTmp);

            # Check the value we stored and a few adjacent bytes.
            self.emitStValCheck(idxMemSlot, uValue, iRegSlot0Ptr);

            # Restore the original value using the same instruction.
            self.emitCommentLine('restoring idxMemSlot=%#x LB %u' % (idxMemSlot, self.cbMem,));
            if self.sType == 'preidx' and (iRegBase != 31 or (idxMemSlot & 15) == 0):
                self.emitStInstrForRestore(sRegBase, 0, self.getSlotValue(idxMemSlot, self.cbMem));
            else:
                if self.sType != 'unscaled':
                    self.emitInstr('add' if idxPreSlot >= 0 else 'sub', '%s, x%u, #%u'
                                                                         % (sRegBase, iRegSlot0Ptr, abs(idxPreSlot),));
                self.emitStInstrForRestore(sRegBase, iOffset, self.getSlotValue(idxMemSlot, self.cbMem));

            self.oGprAllocator.free(iRegBase);

            # Check that the write area is okay.
            self.emitInstr('bl', 'NAME(CheckWriteArea)');

    def emitStInstrAndFree(self, iRegSrc, sRegBase, iOffset):
        if self.sType == 'preidx':
            self.emitInstr(self.sInstr, '%s, [%s, #%d]!' % (g_ddGprNamesZrByBits[self.cBits][iRegSrc], sRegBase, iOffset,));
        elif self.sType == 'unscaled':
            self.emitInstr(self.sInstr, '%s, [%s, #%d]'  % (g_ddGprNamesZrByBits[self.cBits][iRegSrc], sRegBase, iOffset,));
        else:
            self.emitInstr(self.sInstr, '%s, [%s], #%d'  % (g_ddGprNamesZrByBits[self.cBits][iRegSrc], sRegBase, iOffset,));
        self.oGprAllocator.free(iRegSrc);
        return None;

    def emitStInstrAndRandValue(self, sRegBase, iOffset):
        (uValue, iRegSrc) = self.allocGprAndLoadRandUBits();
        self.emitStInstrAndFree(iRegSrc, sRegBase, iOffset);
        return uValue & bitsOnes(self.cbMem * 8);

    def emitStInstrForRestore(self, sRegBase, iOffset, uValue):
        iRegSrc = self.emitGprLoad(self.oGprAllocator.allocNo31(), uValue);
        return self.emitStInstrAndFree(iRegSrc, sRegBase, iOffset);


class A64No1CodeGenStImm9Fp(A64No1CodeGenFprBase, A64No1CodeGenStImm9):
    def __init__(self, sInstr, cbMem = 1, sType = None):
        sBaseName = sInstr + '_' + g_kdFprBytesToInfix[cbMem];
        A64No1CodeGenFprBase.__init__(self, sBaseName, sInstr, cbMem);
        A64No1CodeGenStImm9.__init__(self, sInstr, cbMem, cbMem * 8, sType, sBaseName = sBaseName);

    def emitStInstrAndRandValue(self, sRegBase, iOffset):
        uValue = randUBits(128);
        self.emitStInstrForRestore(sRegBase, iOffset, uValue);
        return uValue & bitsOnes(self.cbMem * 8);

    def emitStInstrForRestore(self, sRegBase, iOffset, uValue):
        iRegSrc = self.emitFprLoad(self.oFprAllocator.alloc(uValue), uValue);
        if self.sType == 'preidx':
            self.emitInstr(self.sInstr, '%s%u, [%s, #%d]!' % (self.sPrefix, iRegSrc, sRegBase, iOffset,));
        elif self.sType == 'unscaled':
            self.emitInstr(self.sInstr, '%s%u, [%s, #%d]'  % (self.sPrefix, iRegSrc, sRegBase, iOffset,));
        else:
            self.emitInstr(self.sInstr, '%s%u, [%s], #%d'  % (self.sPrefix, iRegSrc, sRegBase, iOffset,));
        self.oFprAllocator.free(iRegSrc);
        return None;


class A64No1CodeGenStImm12(A64No1CodeGenStBase):
    """
    STR w/ 12-bit unsigned scaled immediate.

    Note! SP can be used as base register (but not destination).
    """

    def __init__(self, sInstr, cbMem = 1, cBits = None):
        A64No1CodeGenStBase.__init__(self, sInstr, sInstr, cbMem, cBits);

    def generateBodySt(self, oOptions, iRegSlot0Ptr):
        self.emitInstr('bl', 'NAME(CheckWriteArea)');

        # Generate loads from different loads with various immediate values.
        for i in range(oOptions.cTestsPerInstruction):
            uImm12      = randUBits(12);
            uOffset     = uImm12 * self.cbMem;
            idxMemSlot  = randURange(0, 256 - self.cbMem);
            idxPreSlot  = idxMemSlot - uOffset;

            iRegBase    = self.oGprAllocator.allocEx(fReg31IsSp = True);
            sRegBase    = g_dGpr64NamesSp[iRegBase];
            if iRegBase == 31 and (abs(idxPreSlot) & 15) != 0: # SP value must be 16 byte aligned in most host OS contexts.
                idxMemSlot -= idxPreSlot & 15
                if idxMemSlot < 0:
                    idxMemSlot += 16;
                idxPreSlot = idxMemSlot - uOffset;
                assert (abs(idxPreSlot) & 15) == 0;
            self.emitCommentLine('i=%u idxMemSlot=%#x' % (i, idxMemSlot,));

            # Calculate the base address, allocate and load the source register, then perform the store instruction.
            self.emitBaseRegAddrCalc(sRegBase, iRegSlot0Ptr, idxPreSlot);
            iRegBaseCp  = self.emitBaseAddrSaveInNewRegAlloc(sRegBase);
            uValue      = self.emitStInstrAndRandValue(sRegBase, uOffset);

            # Check the offset.
            self.emitBaseAddrCheckAgainstRegAndFreeIt(sRegBase, iRegBaseCp);

            # Check the value we stored and a few adjacent bytes.
            self.emitStValCheck(idxMemSlot, uValue, iRegSlot0Ptr);

            # Restore the original value using the same instruction.
            self.emitCommentLine('restoring idxMemSlot=%#x LB %u' % (idxMemSlot, self.cbMem,));
            self.emitStInstrForRestore(sRegBase, uOffset, self.getSlotValue(idxMemSlot, self.cbMem));

            self.oGprAllocator.free(iRegBase);

            # Check that the write area is okay.
            self.emitInstr('bl', 'NAME(CheckWriteArea)');

    def emitStInstrAndRandValue(self, sRegBase, uOffset):
        uValue = randUBits(self.cBits);
        self.emitStInstrForRestore(sRegBase, uOffset, uValue);
        return uValue & bitsOnes(self.cbMem * 8);

    def emitStInstrForRestore(self, sRegBase, uOffset, uValue):
        iRegSrc = self.emitGprLoad(self.oGprAllocator.allocNo31(), uValue);
        self.emitInstr(self.sInstr, '%s, [%s, #%u]' % (g_ddGprNamesZrByBits[self.cBits][iRegSrc], sRegBase, uOffset,));
        self.oGprAllocator.free(iRegSrc);
        return None;


class A64No1CodeGenStImm12Fp(A64No1CodeGenFprBase, A64No1CodeGenStImm12):
    def __init__(self, sInstr, cbMem = 1):
        A64No1CodeGenFprBase.__init__(self, sInstr, sInstr, cbMem);
        A64No1CodeGenStImm12.__init__(self, sInstr, cbMem, cBits = cbMem * 8);

    def emitStInstrAndRandValue(self, sRegBase, uOffset):
        uValue = randUBits(128);
        self.emitStInstrForRestore(sRegBase, uOffset, uValue);
        return uValue & bitsOnes(self.cbMem * 8);

    def emitStInstrForRestore(self, sRegBase, uOffset, uValue):
        iRegSrc = self.emitFprLoad(self.oFprAllocator.alloc(uValue), uValue);
        self.emitInstr(self.sInstr, '%s%u, [%s, #%d]' % (self.sPrefix, iRegSrc, sRegBase, uOffset,));
        self.oFprAllocator.free(iRegSrc);
        return None;


class A64No1CodeGenStReg(A64No1CodeGenStBase):
    """
    STR w/ index register.

    Note! SP can be used as base register (but not destination or index).
    """

    def __init__(self, sInstr, cbMem = 1, cBits = None, cShift = None, sName = None):
        if cBits is None:
            cBits = 64 if cbMem == 8 else 32;
        A64No1CodeGenStBase.__init__(self, sName or sInstr, sInstr, cbMem = cbMem, cBits = cBits);
        self.cShift = cShift if cShift is not None else cbMem.bit_length() - 1;

    kdOp = { 2: 'UXTW', 3: 'LSL', 6: 'SXTW', 7: 'SXTX', };

    def generateBodySt(self, oOptions, iRegSlot0Ptr):
        fMask64Bits = bitsOnes(64);
        self.emitInstr('bl', 'NAME(CheckWriteArea)');

        # Generate loads from different loads with various immediate values.
        for i in range(oOptions.cTestsPerInstruction):
            idxMemSlot  = randURange(0, 256 - self.cbMem);
            iOption     = randUBits(3) | 2;
            cShift      = self.cShift if randBool() else 0;
            iRegIndex   = self.oGprAllocator.alloc();
            sRegIndex   = g_dGpr64NamesZr[iRegIndex] if iOption & 1 else g_dGpr32NamesZr[iRegIndex];
            uIndex      = randUBits(64) if iRegIndex != 31 else 0;
            if iOption == 2:        # UXTW
                uOffset = uIndex & 0xffffffff;
            elif iOption == 6:      # SXTW
                uOffset = bitsSignExtend(32, uIndex, 64);
            else:                   # LSL or SXTX
                uOffset = uIndex;
            uOffset   <<= cShift;
            uOffset    &= fMask64Bits;
            idxPreSlot  = (idxMemSlot - uOffset) & fMask64Bits;

            iRegBase    = self.oGprAllocator.allocEx(fReg31IsSp = True);
            sRegBase    = g_dGpr64NamesSp[iRegBase];
            if iRegBase == 31 and (idxPreSlot & 15) != 0: # SP value must be 16 byte aligned in most host OS contexts.
                idxMemSlot -= idxPreSlot & 15
                if idxMemSlot < 0:
                    idxMemSlot += 16;
                idxPreSlot  = (idxMemSlot - uOffset) & fMask64Bits;
                assert (idxPreSlot & 15) == 0;
            assert ((idxPreSlot + uOffset) & fMask64Bits) == idxMemSlot;

            self.emitCommentLine('i=%u idxMemSlot=%#x uOffset=%#x (uIndex=%#x %s %u) idxPreSlot=%#x'
                                 % (i, idxMemSlot, uOffset, uIndex, self.kdOp[iOption], cShift, idxPreSlot,));

            # Calculate the base address, allocate and load the source register, then perform the store instruction.
            self.emitGprLoad(iRegBase, idxPreSlot, fTempValue = True);
            self.emitInstr('add', '%s, %s, x%u' % (sRegBase, sRegBase, iRegSlot0Ptr,));
            iRegBaseCp  = self.emitBaseAddrSaveInNewRegAlloc(sRegBase);
            if iRegIndex != 31:
                self.emitGprLoad(iRegIndex, uIndex);
            uValue      = self.emitStInstrAndRandValue(sRegBase, sRegIndex, self.kdOp[iOption], cShift);

            # Check the offset.
            self.emitBaseAddrCheckAgainstRegAndFreeIt(sRegBase, iRegBaseCp);

            # Check the value we stored and a few adjacent bytes.
            self.emitStValCheck(idxMemSlot, uValue, iRegSlot0Ptr);

            # Restore the original value using the same instruction.
            self.emitCommentLine('restoring idxMemSlot=%#x LB %u' % (idxMemSlot, self.cbMem,));
            self.emitStInstrForRestore(sRegBase, sRegIndex, self.kdOp[iOption], cShift,
                                       self.getSlotValue(idxMemSlot, self.cbMem));

            self.oGprAllocator.free(iRegBase);
            self.oGprAllocator.free(iRegIndex);

    def emitStInstrAndRandValue(self, sRegBase, sRegIndex, sOption, cShift):
        uValue = randUBits(self.cBits);
        self.emitStInstrForRestore(sRegBase, sRegIndex, sOption, cShift, uValue);
        return uValue & bitsOnes(self.cbMem * 8);

    def emitStInstrForRestore(self, sRegBase, sRegIndex, sOption, cShift, uValue):
        iRegSrc = self.emitGprLoad(self.oGprAllocator.allocNo31(), uValue);
        self.emitInstr(self.sInstr, '%s, [%s, %s, %s #%u]' % (g_ddGprNamesZrByBits[self.cBits][iRegSrc],
                                                              sRegBase, sRegIndex, sOption, cShift,));
        self.oGprAllocator.free(iRegSrc);
        return None;

class A64No1CodeGenStRegFp(A64No1CodeGenFprBase, A64No1CodeGenStReg):
    def __init__(self, sInstr, cbMem = 1):
        A64No1CodeGenFprBase.__init__(self, sInstr, sInstr, cbMem);
        A64No1CodeGenStReg.__init__(self, sInstr, cbMem, cBits = cbMem * 8);

    def emitStInstrAndRandValue(self, sRegBase, sRegIndex, sOption, cShift):
        uValue = randUBits(128);
        self.emitStInstrForRestore(sRegBase, sRegIndex, sOption, cShift, uValue);
        return uValue & bitsOnes(self.cbMem * 8);

    def emitStInstrForRestore(self, sRegBase, sRegIndex, sOption, cShift, uValue):
        iRegSrc = self.emitFprLoad(self.oFprAllocator.alloc(uValue), uValue);
        self.emitInstr(self.sInstr, '%s%u, [%s, %s, %s #%u]' % (self.sPrefix, iRegSrc, sRegBase, sRegIndex, sOption, cShift,));
        self.oFprAllocator.free(iRegSrc);
        return None;


class A64No1CodeGenStpImm7(A64No1CodeGenStBase):
    """
    STP w/ 7-bit signed immediate, optionally with post-index or pre-index advancing.

    Note! SP can be used as base register (but not destination).
    """

    def __init__(self, sInstr, cbMem = 1, cBits = None, sType = None, sBaseName = None):
        assert sType in { 'preidx', 'postidx', 'signed' };
        A64No1CodeGenStBase.__init__(self, (sBaseName or sInstr) + '_' + sType, sInstr,
                                     cbMem = cbMem, cBits = cBits or (cbMem * 4));
        self.sType = sType;

    def generateBodySt(self, oOptions, iRegSlot0Ptr):
        self.emitInstr('bl', 'NAME(CheckWriteArea)');
        # Generate loads from different loads with various immediate values.
        cbMemHalf = self.cbMem // 2;
        for i in range(oOptions.cTestsPerInstruction):
            uImm7       = randUBits(7);                  # (signed)
            iOffset     = bitsSignedToInt(7, uImm7) * cbMemHalf;
            idxMemSlot  = randURange(0, 256 - self.cbMem);
            idxPreSlot  = idxMemSlot - iOffset if self.sType != 'postidx' else idxMemSlot;

            iRegBase    = self.oGprAllocator.allocEx(fReg31IsSp = True);
            sRegBase    = g_dGpr64NamesSp[iRegBase];
            if iRegBase == 31 and (abs(idxPreSlot) & 15) != 0: # SP value must be 16 byte aligned in most host OS contexts.
                idxMemSlot -= idxPreSlot & 15
                if idxMemSlot < 0:
                    idxMemSlot += 16;
                idxPreSlot = idxMemSlot - iOffset if self.sType != 'postidx' else idxMemSlot;
                assert (abs(idxPreSlot) & 15) == 0;

            self.emitCommentLine('i=%u iOffset=%#x idxMemSlot=%#x' % (i, iOffset, idxMemSlot,));

            # Calculate the base address and perform the load instruction.
            self.emitBaseRegAddrCalc(sRegBase, iRegSlot0Ptr, idxPreSlot);
            (uValue1, uValue2) = self.emitStInstrAndRandValue(sRegBase, iOffset);

            # Check the offset.
            if self.sType == 'preidx':      u64PostSlot = idxMemSlot;
            elif self.sType == 'postidx':   u64PostSlot = (idxMemSlot + iOffset) & 0xffffffffffffffff;
            else:                           u64PostSlot = idxPreSlot             & 0xffffffffffffffff;
            iRegTmp     = self.oGprAllocator.allocNo31(u64PostSlot);
            self.emitInstr('sub', 'x%u, %s, x%u' % (iRegTmp, sRegBase, iRegSlot0Ptr,)); # iRegTmp == idxEffSlot
            self.emitGprValCheck(iRegTmp, u64PostSlot);
            self.oGprAllocator.free(iRegTmp);

            # Check the value we stored and a few adjacent bytes.
            self.emitStValCheck(idxMemSlot, uValue1 | (uValue2 << (cbMemHalf * 8)), iRegSlot0Ptr);

            # Restore the original value using the same instruction.
            self.emitCommentLine('restoring idxMemSlot=%#x LB %u' % (idxMemSlot, self.cbMem,));
            self.emitBaseRegAddrCalc(sRegBase, iRegSlot0Ptr, idxPreSlot);
            self.emitStInstrForRestore(sRegBase, iOffset, self.getSlotValue(idxMemSlot, cbMemHalf),
                                       self.getSlotValue(idxMemSlot + cbMemHalf, cbMemHalf));

            self.oGprAllocator.free(iRegBase);
            self.emitInstr('bl', 'NAME(CheckWriteArea)');

    def emitStInstrAndRandValue(self, sRegBase, iOffset):
        uValue1 = randUBits(64);
        uValue2 = randUBits(64);
        self.emitStInstrForRestore(sRegBase, iOffset, uValue1, uValue2);
        return (uValue1 & bitsOnes(self.cbMem * 4), uValue2 & bitsOnes(self.cbMem * 4));

    def emitStInstrForRestore(self, sRegBase, iOffset, uValue1, uValue2):
        iRegSrc1 = self.emitGprLoad(self.oGprAllocator.allocNo31(), uValue1);
        iRegSrc2 = self.emitGprLoad(self.oGprAllocator.allocNo31(), uValue2);
        if self.sType == 'preidx':
            self.emitInstr(self.sInstr, '%s, %s, [%s, #%d]!'
                           % (g_ddGprNamesZrByBits[self.cBits][iRegSrc1], g_ddGprNamesZrByBits[self.cBits][iRegSrc2],
                              sRegBase, iOffset,));
        elif self.sType == 'signed':
            self.emitInstr(self.sInstr, '%s, %s, [%s, #%d]'
                           % (g_ddGprNamesZrByBits[self.cBits][iRegSrc1], g_ddGprNamesZrByBits[self.cBits][iRegSrc2],
                              sRegBase, iOffset,));
        else:
            self.emitInstr(self.sInstr, '%s, %s, [%s], #%d'
                           % (g_ddGprNamesZrByBits[self.cBits][iRegSrc1], g_ddGprNamesZrByBits[self.cBits][iRegSrc2],
                              sRegBase, iOffset,));
        self.oGprAllocator.free(iRegSrc1);
        self.oGprAllocator.free(iRegSrc2);
        return None;


class A64No1CodeGenStpImm7Fp(A64No1CodeGenFprBase, A64No1CodeGenStpImm7):

    def __init__(self, sInstr, cbMem = 1, sType = None):
        sBaseName = sInstr + '_' + g_kdFprBytesToInfix[cbMem // 2];
        A64No1CodeGenFprBase.__init__(self, sBaseName, sInstr, cbMem // 2);
        A64No1CodeGenStpImm7.__init__(self, sInstr, cbMem = cbMem, cBits = cbMem * 4, sType = sType, sBaseName = sBaseName);

    def emitStInstrAndRandValue(self, sRegBase, iOffset):
        uValue1 = randUBits(128);
        uValue2 = randUBits(128);
        self.emitStInstrForRestore(sRegBase, iOffset, uValue1, uValue2);
        return (uValue1 & bitsOnes(self.cbMem * 4), uValue2 & bitsOnes(self.cbMem * 4));

    def emitStInstrForRestore(self, sRegBase, iOffset, uValue1, uValue2):
        iRegSrc1 = self.emitFprLoad(self.oFprAllocator.alloc(uValue1), uValue1);
        iRegSrc2 = self.emitFprLoad(self.oFprAllocator.alloc(uValue2), uValue2);
        if self.sType == 'preidx':
            self.emitInstr(self.sInstr, '%s%u, %s%u, [%s, #%d]!'
                           % (self.sPrefix, iRegSrc1, self.sPrefix, iRegSrc2, sRegBase, iOffset,));
        elif self.sType == 'signed':
            self.emitInstr(self.sInstr, '%s%u, %s%u, [%s, #%d]'
                           % (self.sPrefix, iRegSrc1, self.sPrefix, iRegSrc2, sRegBase, iOffset,));
        else:
            self.emitInstr(self.sInstr, '%s%u, %s%u, [%s], #%d'
                           % (self.sPrefix, iRegSrc1, self.sPrefix, iRegSrc2, sRegBase, iOffset,));
        self.oFprAllocator.free(iRegSrc1);
        self.oFprAllocator.free(iRegSrc2);
        return None;


#
# Advanced SIMD.
#

g_kdCfgToElemInfo = {
    '8B':  ( 1,  8,  8, 0xff, ),
    '16B': ( 1,  8, 16, 0xff, ),
    '4H':  ( 2, 16,  4, 0xffff, ),
    '8H':  ( 2, 16,  8, 0xffff, ),
    '2S':  ( 4, 32,  2, 0xffffffff, ),
    '4S':  ( 4, 32,  4, 0xffffffff, ),
    '1D':  ( 8, 64,  1, 0xffffffffffffffff, ),
    '2D':  ( 8, 64,  2, 0xffffffffffffffff, ),
};


class A64No1CodeGenAdvSimdAccross(A64No1CodeGenFprBase):
    """
    C4.1.95.22 Advanced SIMD across lanes
    """
    def __init__(self, sInstr, fnCalc, uFactor = 1, asSkip = None, asOnly = None, fWithFlags = False):
        A64No1CodeGenFprBase.__init__(self, sInstr + '_advsimd_across', sInstr, 16);
        self.fnCalc     = fnCalc;
        self.uFactor    = uFactor;
        self.asCfgs     = list(g_kdCfgToElemInfo.keys()) if not asOnly else list(asOnly);
        if asSkip:
            for sCfg in asSkip:
                self.asCfgs.remove(sCfg);
        self.fWithFlags = fWithFlags;

    kddFactorAndCfgToRegPfx = {
        1: {
            '8B':  'b',
            '16B': 'b',
            '4H':  'h',
            '8H':  'h',
            '2S':  's',
            '4S':  's',
            '1D':  'd',
            '2D':  'd',
        },
        2: {
            '8B':  'h',
            '16B': 'h',
            '4H':  's',
            '8H':  's',
            '2S':  'd',
            '4S':  'd',
            #'1D':  '',
            #'2D':  '',
        },
    };

    def generateBody(self, oOptions, cLeftToAllCheck):
        iCfg = 0;
        for _ in range(oOptions.cTestsPerInstruction):
            # Retrieve the vector configuration for this iteration and advance it.
            sCfg = self.asCfgs[iCfg];
            iCfg = (iCfg + 1) % len(self.asCfgs);
            (_, cBitsElem, cElems, fElemMask) = g_kdCfgToElemInfo[sCfg];

            # Get source values and calculate the result.
            (uSrc, iRegSrc) = self.allocFprAndLoadRandUBits();

            # Calc result.
            fFpSrIn = 0; ## @todo IEM doesn't handle random values right (RES0 bits are implicitly masked out).
            (uRes, fFpSrOut) = self.fnCalc(cBitsElem, fElemMask, None, uSrc & fElemMask, fFpSrIn);
            for iElem in range(1, cElems):
                uSrcElem = (uSrc >> (iElem * cBitsElem)) & fElemMask;
                (uRes, fFpSrElem) = self.fnCalc(cBitsElem, fElemMask, uRes, uSrcElem, fFpSrIn);
                fFpSrOut |= fFpSrElem;
            (uRes, fFpSrElem) = self.fnCalc(cBitsElem, fElemMask, uRes, None, fFpSrIn);

            # Load FPSR.
            if self.fWithFlags:
                self.emitFpSrLoad(fFpSrIn);

            # Emit the test instruction and result checks.
            iRegDst = self.oFprAllocator.alloc(uRes);
            self.emitInstr(self.sInstr, '%s%u, v%u.%s'
                           % (self.kddFactorAndCfgToRegPfx[self.uFactor][sCfg], iRegDst, iRegSrc, sCfg,));

            if self.fWithFlags:
                self.emitFpSrCheck(fFpSrOut);
            self.emitFprValCheck(iRegDst, uRes);

            self.oFprAllocator.freeList((iRegDst, iRegSrc));

def calcAdvSimdAcrossAddV(_, fElemMask, uResult, uSrcElem, fFpSr):
    if uResult is None:
        uResult = uSrcElem;
    elif uSrcElem is not None:
        uResult += uSrcElem;
    uResult &= fElemMask;
    return (uResult, fFpSr);

def calcAdvSimdAcrossUAddLV(cBitsElem, fElemMask, uResult, uSrcElem, fFpSr):
    if uResult is None:
        uResult = uSrcElem;
    elif uSrcElem is not None:
        uResult += uSrcElem;
    uResult &= fElemMask | (fElemMask << cBitsElem);
    return (uResult, fFpSr);

def calcAdvSimdAcrossUMaxV(_, fElemMask, uResult, uSrcElem, fFpSr):
    _  = fElemMask;
    if uResult is None:
        uResult = uSrcElem;
    elif uSrcElem is not None and uSrcElem > uResult:
        return (uSrcElem, fFpSr);
    return (uResult, fFpSr);

def calcAdvSimdAcrossUMinV(_, fElemMask, uResult, uSrcElem, fFpSr):
    _  = fElemMask;
    if uResult is None:
        uResult = uSrcElem;
    elif uSrcElem is not None and uSrcElem < uResult:
        return (uSrcElem, fFpSr);
    return (uResult, fFpSr);

def calcAdvSimdAcrossSAddLV(cBitsElem, fElemMask, iResult, uSrcElem, fFpSr):
    if iResult is None:
        iResult  = bitsSignedToInt(cBitsElem, uSrcElem);
    elif uSrcElem is not None:
        iResult += bitsSignedToInt(cBitsElem, uSrcElem);
    else:
        iResult &= fElemMask | (fElemMask << cBitsElem);
    return (iResult, fFpSr);

def calcAdvSimdAcrossSMaxV(cBitsElem, _, uResult, uSrcElem, fFpSr):
    if uResult is None:
        uResult  = uSrcElem
    elif uSrcElem is not None and bitsSignedToInt(cBitsElem, uSrcElem) > bitsSignedToInt(cBitsElem, uResult):
        uResult = uSrcElem;
    return (uResult, fFpSr);

def calcAdvSimdAcrossSMinV(cBitsElem, _, uResult, uSrcElem, fFpSr):
    if uResult is None:
        uResult  = uSrcElem
    elif uSrcElem is not None and bitsSignedToInt(cBitsElem, uSrcElem) < bitsSignedToInt(cBitsElem, uResult):
        uResult = uSrcElem;
    return (uResult, fFpSr);


class A64No1CodeGenAdvSimdThreeSame(A64No1CodeGenFprBase):
    """
    C4.1.95.24 Advanced SIMD three same
    """
    def __init__(self, sInstr, fnCalc, asSkip = None, asOnly = None, fWithFlags = False,
                 fPairElems = False, fWithDstInput = False):
        A64No1CodeGenFprBase.__init__(self, sInstr + '_advsimd_3same', sInstr, 16);
        self.fnCalc = fnCalc;
        self.asCfgs = list(g_kdCfgToElemInfo.keys()) if not asOnly else list(asOnly);
        if asSkip:
            for sCfg in asSkip:
                self.asCfgs.remove(sCfg);
        self.fWithFlags = fWithFlags;
        self.fPairElems = fPairElems;
        self.fWithDstInput = fWithDstInput;
        assert not fWithDstInput or not fPairElems;

    def generateBody(self, oOptions, cLeftToAllCheck):
        iCfg = 0;
        for _ in range(oOptions.cTestsPerInstruction):
            # Retrieve the vector configuration for this iteration and advance it.
            sCfg = self.asCfgs[iCfg];
            iCfg = (iCfg + 1) % len(self.asCfgs);
            (_, cBitsElem, cElems, fElemMask) = g_kdCfgToElemInfo[sCfg];

            # Get source values and calculate the result.
            (uSrc1, iRegSrc1) = self.allocFprAndLoadRandUBits();
            (uSrc2, iRegSrc2) = self.allocFprAndLoadRandUBits();
            if not self.fWithDstInput:
                iRegDst       = self.oFprAllocator.alloc();
            else:
                (uDstIn, iRegDst) = self.allocFprAndLoadRandUBits();

            uRes     = 0;
            fFpSrIn  = 0; ## @todo IEM doesn't handle random values right (RES0 bits are implicitly masked out).
            fFpSrOut = 0;
            if not self.fPairElems:
                for iElem in range(cElems):
                    uSrcElem1 = (uSrc1 >> (iElem * cBitsElem)) & fElemMask;
                    uSrcElem2 = (uSrc2 >> (iElem * cBitsElem)) & fElemMask;
                    if not self.fWithDstInput:
                        (uResElem, fFpSrElem) = self.fnCalc(cBitsElem, fElemMask, uSrcElem1, uSrcElem2, fFpSrIn);
                    else: # HACK ALERT!
                        uResElem = (uDstIn >> (iElem * cBitsElem)) & fElemMask;
                        (uResElem, fFpSrElem) = self.fnCalc(uResElem, fElemMask, uSrcElem1, uSrcElem2, fFpSrIn);
                    assert (uResElem & ~fElemMask) == 0, self.sInstr;
                    uRes     |= uResElem << (iElem * cBitsElem);
                    fFpSrOut |= fFpSrElem;
            else:
                cBitsPerReg  = cElems * cBitsElem;
                uSrcCombined = ((uSrc2 & bitsOnes(cBitsPerReg)) << cBitsPerReg) | (uSrc1 & bitsOnes(cBitsPerReg));
                for iElem in range(cElems):
                    uSrcElem1 = (uSrcCombined >> (iElem * 2       * cBitsElem)) & fElemMask;
                    uSrcElem2 = (uSrcCombined >> ((iElem * 2 + 1) * cBitsElem)) & fElemMask;
                    (uResElem, fFpSrElem) = self.fnCalc(cBitsElem, fElemMask, uSrcElem1, uSrcElem2, fFpSrIn);
                    assert (uResElem & ~fElemMask) == 0;
                    uRes     |= uResElem << (iElem * cBitsElem);
                    fFpSrOut |= fFpSrElem;

            # Load FPSR.
            if self.fWithFlags:
                self.emitFpSrLoad(fFpSrIn);

            # Emit the test instruction and result checks.
            self.emitInstr(self.sInstr, 'v%u.%s, v%u.%s, v%u.%s' % (iRegDst, sCfg, iRegSrc1, sCfg, iRegSrc2, sCfg,));

            if self.fWithFlags:
                self.emitFpSrCheck(fFpSrOut);
            self.emitFprValCheck(iRegDst, uRes);

            self.oFprAllocator.freeList((iRegDst, iRegSrc2, iRegSrc1));

def calcSignedSatQ(iSum, cBitsElem):
    fMask = bitsOnes(cBitsElem - 1);
    if iSum > fMask:
        return (fMask, True);
    if iSum < -fMask - 1:
        return (-fMask - 1, True);
    return (iSum, False);

def calcAdvSimd3Add(_, fElemMask, uSrcElem1, uSrcElem2, fFpSr):
    return ((uSrcElem1 + uSrcElem2) & fElemMask, fFpSr);

def calcAdvSimd3ShAdd(cBitsElem, fElemMask, uSrcElem1, uSrcElem2, fFpSr):
    iSum = bitsSignedToInt(cBitsElem, uSrcElem1) + bitsSignedToInt(cBitsElem, uSrcElem2);
    return ((iSum >> 1) & fElemMask, fFpSr);

def calcAdvSimd3SrhAdd(cBitsElem, fElemMask, uSrcElem1, uSrcElem2, fFpSr):
    iSum = bitsSignedToInt(cBitsElem, uSrcElem1) + bitsSignedToInt(cBitsElem, uSrcElem2) + 1;
    return ((iSum >> 1) & fElemMask, fFpSr);

def calcAdvSimd3SqAdd(cBitsElem, fElemMask, uSrcElem1, uSrcElem2, fFpSr):
    (iSum, fSat) = calcSignedSatQ(bitsSignedToInt(cBitsElem, uSrcElem1) + bitsSignedToInt(cBitsElem, uSrcElem2), cBitsElem);
    if fSat:
        fFpSr |= 1 << 27;
    return (iSum & fElemMask, fFpSr);


def calcAdvSimd3Sub(_, fElemMask, uSrcElem1, uSrcElem2, fFpSr):
    return ((uSrcElem1 - uSrcElem2) & fElemMask, fFpSr);

def calcAdvSimd3ShSub(cBitsElem, fElemMask, uSrcElem1, uSrcElem2, fFpSr):
    iSum = bitsSignedToInt(cBitsElem, uSrcElem1) - bitsSignedToInt(cBitsElem, uSrcElem2);
    return ((iSum >> 1) & fElemMask, fFpSr);

def calcAdvSimd3SqSub(cBitsElem, fElemMask, uSrcElem1, uSrcElem2, fFpSr):
    (iSum, fSat) = calcSignedSatQ(bitsSignedToInt(cBitsElem, uSrcElem1) - bitsSignedToInt(cBitsElem, uSrcElem2), cBitsElem);
    if fSat:
        fFpSr |= 1 << 27;
    return (iSum & fElemMask, fFpSr);


def calcAdvSimd3CmEq(_, fElemMask, uSrcElem1, uSrcElem2, fFpSr):
    if (uSrcElem1 & fElemMask) == (uSrcElem2 & fElemMask):
        return (fElemMask, fFpSr);
    return (0, fFpSr);

def calcAdvSimd3CmGt(cBitsElem, fElemMask, uSrcElem1, uSrcElem2, fFpSr):
    if bitsSignedToInt(cBitsElem, uSrcElem1) > bitsSignedToInt(cBitsElem, uSrcElem2):
        return (fElemMask, fFpSr);
    return (0, fFpSr);

def calcAdvSimd3CmGe(cBitsElem, fElemMask, uSrcElem1, uSrcElem2, fFpSr):
    if bitsSignedToInt(cBitsElem, uSrcElem1) >= bitsSignedToInt(cBitsElem, uSrcElem2):
        return (fElemMask, fFpSr);
    return (0, fFpSr);

def calcAdvSimd3CmHi(_, fElemMask, uSrcElem1, uSrcElem2, fFpSr):
    if (uSrcElem1 & fElemMask) > (uSrcElem2 & fElemMask):
        return (fElemMask, fFpSr);
    return (0, fFpSr);

def calcAdvSimd3CmHs(_, fElemMask, uSrcElem1, uSrcElem2, fFpSr):
    if (uSrcElem1 & fElemMask) >= (uSrcElem2 & fElemMask):
        return (fElemMask, fFpSr);
    return (0, fFpSr);

def calcAdvSimd3CmTst(_, fElemMask, uSrcElem1, uSrcElem2, fFpSr):
    if (uSrcElem1 & uSrcElem2 & fElemMask) != 0:
        return (fElemMask, fFpSr);
    return (0, fFpSr);


def calcAdvSimd3And(_, fElemMask, uSrcElem1, uSrcElem2, fFpSr):
    return (uSrcElem1 & uSrcElem2 & fElemMask, fFpSr);

def calcAdvSimd3Bic(_, fElemMask, uSrcElem1, uSrcElem2, fFpSr):
    return (uSrcElem1 & ~uSrcElem2 & fElemMask, fFpSr);

def calcAdvSimd3Orr(_, fElemMask, uSrcElem1, uSrcElem2, fFpSr):
    return ((uSrcElem1 | uSrcElem2) & fElemMask, fFpSr);

def calcAdvSimd3Orn(_, fElemMask, uSrcElem1, uSrcElem2, fFpSr):
    return ((uSrcElem1 | ~uSrcElem2) & fElemMask, fFpSr);

def calcAdvSimd3Eor(_, fElemMask, uSrcElem1, uSrcElem2, fFpSr):
    return ((uSrcElem1 ^ uSrcElem2) & fElemMask, fFpSr);

def calcAdvSimd3Bsl(uDstElem, fElemMask, uSrcElem1, uSrcElem2, fFpSr): # HACK
    return ((uSrcElem2 ^ ((uSrcElem1 ^ uSrcElem2) & uDstElem)) & fElemMask, fFpSr);

def calcAdvSimd3Bit(uDstElem, fElemMask, uSrcElem1, uSrcElem2, fFpSr): # HACK
    return ((uDstElem ^ ((uDstElem ^ uSrcElem1) & uSrcElem2)) & fElemMask, fFpSr);

def calcAdvSimd3Bif(uDstElem, fElemMask, uSrcElem1, uSrcElem2, fFpSr): # HACK
    return ((uDstElem ^ ((uDstElem ^ uSrcElem1) & ~uSrcElem2)) & fElemMask, fFpSr);


def calcAdvSimd3SMax(cBitsElem, fElemMask, uSrcElem1, uSrcElem2, fFpSr):
    return (max(bitsSignedToInt(cBitsElem, uSrcElem1), bitsSignedToInt(cBitsElem, uSrcElem2)) & fElemMask, fFpSr);

def calcAdvSimd3SMin(cBitsElem, fElemMask, uSrcElem1, uSrcElem2, fFpSr):
    return (min(bitsSignedToInt(cBitsElem, uSrcElem1), bitsSignedToInt(cBitsElem, uSrcElem2)) & fElemMask, fFpSr);

def calcAdvSimd3UMax(_, fElemMask, uSrcElem1, uSrcElem2, fFpSr):
    return (max(uSrcElem1 & fElemMask, uSrcElem2 & fElemMask), fFpSr);

def calcAdvSimd3UMin(_, fElemMask, uSrcElem1, uSrcElem2, fFpSr):
    return (min(uSrcElem1 & fElemMask, uSrcElem2 & fElemMask), fFpSr);


#
# Result calculation functions.
#

# Helpers for result calculations:
def flagsToMask(fNegative, fZero, fCarry, fOverflow):
    """ Returns the PSTATE.NZCV value for the 4 status flags. """
    return (fNegative << 31) | (fZero << 30) | (fCarry << 29) | (fOverflow << 28);


# calc result of addition and subtraction:
def calcAdd(cBits, uVal1, uVal2, fCarry):
    fMask      = (1 << cBits) - 1;
    uVal1     &= fMask;
    uVal2     &= fMask;

    uResultRaw = uVal1 + uVal2 + fCarry;
    uResult    = uResultRaw & fMask;

    fNeg       = (uResult >> (cBits - 1)) & 1;
    fZero      = 0 if uResult else 1;
    fCarry     = 1 if uResultRaw > uResult else 0;
    fOverflow  = ((~(uVal1 ^ uVal2) & (uResult ^ uVal1)) >> (cBits - 1)) & 1;
    return (uResult, flagsToMask(fNeg, fZero, fCarry, fOverflow));

def calcSub(cBits, uVal1, uVal2, _):
    fMask      = (1 << cBits) - 1;
    uVal1     &= fMask;
    uVal2     &= fMask;

    uResultRaw = uVal1 - uVal2;
    uResult    = uResultRaw & fMask;

    fNeg       = (uResult >> (cBits - 1)) & 1;
    fZero      = 0 if uResult else 1;
    fCarry     = 0 if uVal1 < uVal2 else 1;
    fOverflow  = (((uVal1 ^ uVal2) & (uResult ^ uVal1)) >> (cBits - 1)) & 1;
    return (uResult, flagsToMask(fNeg, fZero, fCarry, fOverflow));

def calcSubBorrow(cBits, uVal1, uVal2, fCarry):
    fBorrow    = 0 if fCarry else 1;
    fMask      = (1 << cBits) - 1;
    uVal1     &= fMask;
    uVal2     &= fMask;

    uResultRaw = uVal1 - uVal2 - fBorrow;
    uResult    = uResultRaw & fMask;

    fNeg       = (uResult >> (cBits - 1)) & 1;
    fZero      = 0 if uResult else 1;
    fCarry     = 0 if uVal1 < uVal2 + fBorrow else 1;
    fOverflow  = (((uVal1 ^ uVal2) & (uResult ^ uVal1)) >> (cBits - 1)) & 1;
    return (uResult, flagsToMask(fNeg, fZero, fCarry, fOverflow));


# Calc result of logical/bitwise operation:
def calcBitwiseCommon(cBits, uResult):
    uResult &= (1 << cBits) - 1;
    return (uResult, flagsToMask((uResult >> (cBits - 1)) & 1, 0 if uResult else 1, 0, 0));

def calcAnd(cBits, uVal1, uVal2, _):
    return calcBitwiseCommon(cBits, uVal1 & uVal2);

def calcBic(cBits, uVal1, uVal2, _):
    return calcBitwiseCommon(cBits, uVal1 & ~uVal2);

def calcOrr(cBits, uVal1, uVal2, _):
    return calcBitwiseCommon(cBits, uVal1 | uVal2);

def calcOrn(cBits, uVal1, uVal2, _):
    return calcBitwiseCommon(cBits, uVal1 | ~uVal2);

def calcEor(cBits, uVal1, uVal2, _):
    return calcBitwiseCommon(cBits, uVal1 ^ uVal2);

def calcEon(cBits, uVal1, uVal2, _):
    return calcBitwiseCommon(cBits, uVal1 ^ ~uVal2);


#
# Main class.
#

class Arm64No1CodeGen(object):
    """ Main code generator class. """

    def __init__(self):
        self.oOptions = None;


    def main(self, asArgs):
        """ Main function. """
        oArgParser = argparse.ArgumentParser();
        oArgParser.add_argument('--output',
                                metavar = 'file',
                                dest    = 'sFileOut',
                                action  = 'store',
                                default = None,
                                help    = 'The output file.');

        oArgParser.add_argument('--seed',
                                metavar = 'value',
                                dest    = 'iRandSeed',
                                action  = 'store',
                                default = None,
                                type    = lambda s: int(s, 0),
                                help    = 'The random seed to use.');

        oArgParser.add_argument('--tests-per-instruction',
                                metavar = 'value',
                                dest    = 'cTestsPerInstruction',
                                action  = 'store',
                                default = 256,
                                type    = lambda s: int(s, 0),
                                choices = range(1, 16384),
                                help    = 'Number of tests per instruction.');

        oArgParser.add_argument('--check-all-regs-interval',
                                metavar = 'value',
                                dest    = 'cCheckAllRegsInterval',
                                action  = 'store',
                                default = None,
                                type    = lambda s: int(s, 0),
                                choices = range(1, 16384),
                                help    = 'Number of tests between checking all registers. Zero to disable. '
                                          'Defaults to 1/4 of --tests-per-instruction with a minimum of 8.');



        # Do it!
        oOptions = oArgParser.parse_args(asArgs[1:]);
        self.oOptions = oOptions;
        if oOptions.cCheckAllRegsInterval is None:
            oOptions.cCheckAllRegsInterval = min(max(oOptions.cTestsPerInstruction / 4, 8), oOptions.cTestsPerInstruction);

        #
        # Seed random.
        #
        if oOptions.iRandSeed in (None, 0):
            oOptions.iRandSeed = sum((int(bVal) << (iByte * 8)) for iByte, bVal in enumerate(os.urandom(7)))
        g_oRandom.seed(oOptions.iRandSeed);
        print('info: Using seed %#x' % (oOptions.iRandSeed,));

        #
        # Instantiate the generators.
        #
        aoGenerators = [];

        if True: # pylint: disable=using-constant-test
            # C4.1.94.13 Data-processing - 3 sources
            aoGenerators += [
                A64No1CodeGenData3Op(         'madd',   calcMAdd),
                A64No1CodeGenData3Op(         'msub',   calcMSub),
                A64No1CodeGenData3Op(         'smaddl', calcSMAddL, fSrc1And2Are32Bit = True, acBits = (64,)),
                A64No1CodeGenData3Op(         'smsubl', calcSMSubL, fSrc1And2Are32Bit = True, acBits = (64,)),
                A64No1CodeGenData3Op(         'smulh',  calcSMulH,  fSrc3IsZero       = True, acBits = (64,)),
                A64No1CodeGenData3Op(         'umaddl', calcUMAddL, fSrc1And2Are32Bit = True, acBits = (64,)),
                A64No1CodeGenData3Op(         'umsubl', calcUMSubL, fSrc1And2Are32Bit = True, acBits = (64,)),
                A64No1CodeGenData3Op(         'umulh',  calcUMulH,  fSrc3IsZero       = True, acBits = (64,)),
            ];

        if True: # pylint: disable=using-constant-test
            # C4.1.95.22 Advanced SIMD across lanes
            aoGenerators += [
                A64No1CodeGenAdvSimdAccross(  'saddlv', calcAdvSimdAcrossSAddLV, asSkip = ('2S', '1D', '2D',), uFactor = 2),
                A64No1CodeGenAdvSimdAccross(  'smaxv',  calcAdvSimdAcrossSMaxV,  asSkip = ('2S', '1D', '2D',)),
                A64No1CodeGenAdvSimdAccross(  'sminv',  calcAdvSimdAcrossSMinV,  asSkip = ('2S', '1D', '2D',)),
                A64No1CodeGenAdvSimdAccross(  'addv',   calcAdvSimdAcrossAddV,   asSkip = ('2S', '1D', '2D',)),
                A64No1CodeGenAdvSimdAccross(  'uaddlv', calcAdvSimdAcrossUAddLV, asSkip = ('2S', '1D', '2D',), uFactor = 2),
                A64No1CodeGenAdvSimdAccross(  'umaxv',  calcAdvSimdAcrossUMaxV,  asSkip = ('2S', '1D', '2D',)),
                A64No1CodeGenAdvSimdAccross(  'uminv',  calcAdvSimdAcrossUMinV,  asSkip = ('2S', '1D', '2D',)),
            ];

        if True: # pylint: disable=using-constant-test
            # C4.1.95.24 Advanced SIMD three same
            aoGenerators += [
                A64No1CodeGenAdvSimdThreeSame('smax',   calcAdvSimd3SMax,    asSkip = ('1D', '2D',)),
                A64No1CodeGenAdvSimdThreeSame('smaxp',  calcAdvSimd3SMax,    asSkip = ('1D', '2D',), fPairElems = True),
                A64No1CodeGenAdvSimdThreeSame('smin',   calcAdvSimd3SMin,    asSkip = ('1D', '2D',)),
                A64No1CodeGenAdvSimdThreeSame('sminp',  calcAdvSimd3SMin,    asSkip = ('1D', '2D',), fPairElems = True),
                A64No1CodeGenAdvSimdThreeSame('umax',   calcAdvSimd3UMax,    asSkip = ('1D', '2D',)),
                A64No1CodeGenAdvSimdThreeSame('umaxp',  calcAdvSimd3UMax,    asSkip = ('1D', '2D',), fPairElems = True),
                A64No1CodeGenAdvSimdThreeSame('umin',   calcAdvSimd3UMin,    asSkip = ('1D', '2D',)),
                A64No1CodeGenAdvSimdThreeSame('uminp',  calcAdvSimd3UMin,    asSkip = ('1D', '2D',), fPairElems = True),

                A64No1CodeGenAdvSimdThreeSame('and',    calcAdvSimd3And,     asOnly = ('8B', '16B',)),
                A64No1CodeGenAdvSimdThreeSame('bic',    calcAdvSimd3Bic,     asOnly = ('8B', '16B',)),
                A64No1CodeGenAdvSimdThreeSame('orr',    calcAdvSimd3Orr,     asOnly = ('8B', '16B',)),
                A64No1CodeGenAdvSimdThreeSame('orn',    calcAdvSimd3Orn,     asOnly = ('8B', '16B',)),
                A64No1CodeGenAdvSimdThreeSame('eor',    calcAdvSimd3Eor,     asOnly = ('8B', '16B',)),
                A64No1CodeGenAdvSimdThreeSame('bsl',    calcAdvSimd3Bsl,     asOnly = ('8B', '16B',), fWithDstInput = True),
                A64No1CodeGenAdvSimdThreeSame('bit',    calcAdvSimd3Bit,     asOnly = ('8B', '16B',), fWithDstInput = True),
                A64No1CodeGenAdvSimdThreeSame('bif',    calcAdvSimd3Bif,     asOnly = ('8B', '16B',), fWithDstInput = True),

                A64No1CodeGenAdvSimdThreeSame('cmeq',   calcAdvSimd3CmEq,    asSkip = ('1D',)),
                A64No1CodeGenAdvSimdThreeSame('cmgt',   calcAdvSimd3CmGt,    asSkip = ('1D',)),
                A64No1CodeGenAdvSimdThreeSame('cmge',   calcAdvSimd3CmGe,    asSkip = ('1D',)),
                A64No1CodeGenAdvSimdThreeSame('cmhi',   calcAdvSimd3CmHi,    asSkip = ('1D',)),
                A64No1CodeGenAdvSimdThreeSame('cmhs',   calcAdvSimd3CmHs,    asSkip = ('1D',)),
                A64No1CodeGenAdvSimdThreeSame('cmtst',  calcAdvSimd3CmTst,   asSkip = ('1D',)),

                A64No1CodeGenAdvSimdThreeSame('add',    calcAdvSimd3Add,     asSkip = ('1D',)),
                A64No1CodeGenAdvSimdThreeSame('addp',   calcAdvSimd3Add,     asSkip = ('1D',), fPairElems = True),
                A64No1CodeGenAdvSimdThreeSame('shadd',  calcAdvSimd3ShAdd,   asSkip = ('1D', '2D',)),
                A64No1CodeGenAdvSimdThreeSame('srhadd', calcAdvSimd3SrhAdd,  asSkip = ('1D', '2D',)),
                A64No1CodeGenAdvSimdThreeSame('sqadd',  calcAdvSimd3SqAdd,   asSkip = ('1D',), fWithFlags = True),

                A64No1CodeGenAdvSimdThreeSame('sub',    calcAdvSimd3Sub,     asSkip = ('1D',)),
                A64No1CodeGenAdvSimdThreeSame('shsub',  calcAdvSimd3ShSub,   asSkip = ('1D', '2D',)),
                A64No1CodeGenAdvSimdThreeSame('sqsub',  calcAdvSimd3SqSub,   asSkip = ('1D',), fWithFlags = True),
            ];

        if True: # pylint: disable=using-constant-test
            # extract
            aoGenerators += [
                A64No1CodeGenExtrImm(   'extr',   calcExtr),
            ];

        if True: # pylint: disable=using-constant-test
            # testbranch
            aoGenerators += [
                A64No1CodeGenData1Op(    'rbit',   calcRbit),
                A64No1CodeGenData1Op(    'rev16',  calcRev16),
                A64No1CodeGenData1Op(    'rev',    calcRev,),
                A64No1CodeGenData1Op(    'rev32',  calcRev32, acBits = (64,)),
                A64No1CodeGenData1Op(    'clz',    calcClz),
                A64No1CodeGenData1Op(    'cls',    calcCls),
                #A64No1CodeGenData1Op(    'ctz',    calcCtz), - not supported by available HW (FEAT_CSSC)
                #A64No1CodeGenData1Op(    'cnt',    calcCnt), - not supported by available HW (FEAT_CSSC)
                #A64No1CodeGenData1Op(    'abs',    calcAbs), - not supported by available HW (FEAT_CSSC)
                ## @todo do PACIA and all that stuff...
            ];

        if True: # pylint: disable=using-constant-test
            # testbranch
            aoGenerators += [
                A64No1CodeGenTestBranch( 'tbz',     fJumpIfSet = False),
                A64No1CodeGenTestBranch( 'tbnz',    fJumpIfSet = True),
            ];

        if True: # pylint: disable=using-constant-test
            # condcmp_reg & condcmp_imm
            aoGenerators += [
                A64No1CodeGenCondCmpReg( 'ccmn',    calcAdd),
                A64No1CodeGenCondCmpReg( 'ccmp',    calcSub),
                A64No1CodeGenCondCmpImm( 'ccmn',    calcAdd),
                A64No1CodeGenCondCmpImm( 'ccmp',    calcSub),
            ];

        if True: # pylint: disable=using-constant-test
            # asimdimm
            aoGenerators += [
                A64No1CodeGenCondSel(    'csel',    calcCondSel),
                A64No1CodeGenCondSel(    'csinc',   calcCondSelInc),
                A64No1CodeGenCondSel(    'csinv',   calcCondSelInv),
                A64No1CodeGenCondSel(    'csneg',   calcCondSelNeg),
            ];

        if True: # pylint: disable=using-constant-test
            # asimdimm
            aoGenerators += [
                A64No1CodeGenMoviGrp(),
            ];

        if True: # pylint: disable=using-constant-test
            # Pair loads and stores (with 7-bit scaled immediates):
            aoGenerators += [
                A64No1CodeGenLdpImm7(    'ldp',   calcLdUnsigned, cbMem =  8 * 2, sType = 'postidx'),
                A64No1CodeGenLdpImm7(    'ldp',   calcLdUnsigned, cbMem =  8 * 2, sType = 'preidx'),
                A64No1CodeGenLdpImm7(    'ldp',   calcLdUnsigned, cbMem =  8 * 2, sType = 'signed'),
                A64No1CodeGenLdpImm7(    'ldp',   calcLdUnsigned, cbMem =  4 * 2, sType = 'postidx'),
                A64No1CodeGenLdpImm7(    'ldp',   calcLdUnsigned, cbMem =  4 * 2, sType = 'preidx'),
                A64No1CodeGenLdpImm7(    'ldp',   calcLdUnsigned, cbMem =  4 * 2, sType = 'signed'),
                A64No1CodeGenLdpImm7(    'ldpsw', calcLdSigned,   cbMem =  4 * 2, sType = 'postidx', cBits = 64),
                A64No1CodeGenLdpImm7(    'ldpsw', calcLdSigned,   cbMem =  4 * 2, sType = 'preidx',  cBits = 64),
                A64No1CodeGenLdpImm7(    'ldpsw', calcLdSigned,   cbMem =  4 * 2, sType = 'signed',  cBits = 64),
                A64No1CodeGenLdpImm7Fp(  'ldp',                   cbMem = 16 * 2, sType = 'postidx'),
                A64No1CodeGenLdpImm7Fp(  'ldp',                   cbMem = 16 * 2, sType = 'preidx'),
                A64No1CodeGenLdpImm7Fp(  'ldp',                   cbMem = 16 * 2, sType = 'signed'),
                A64No1CodeGenLdpImm7Fp(  'ldp',                   cbMem =  8 * 2, sType = 'postidx'),
                A64No1CodeGenLdpImm7Fp(  'ldp',                   cbMem =  8 * 2, sType = 'preidx'),
                A64No1CodeGenLdpImm7Fp(  'ldp',                   cbMem =  8 * 2, sType = 'signed'),
                A64No1CodeGenLdpImm7Fp(  'ldp',                   cbMem =  4 * 2, sType = 'postidx'),
                A64No1CodeGenLdpImm7Fp(  'ldp',                   cbMem =  4 * 2, sType = 'preidx'),
                A64No1CodeGenLdpImm7Fp(  'ldp',                   cbMem =  4 * 2, sType = 'signed'),

                A64No1CodeGenStpImm7(    'stp',                   cbMem =  8 * 2, sType = 'postidx'),
                A64No1CodeGenStpImm7(    'stp',                   cbMem =  8 * 2, sType = 'preidx'),
                A64No1CodeGenStpImm7(    'stp',                   cbMem =  8 * 2, sType = 'signed'),
                A64No1CodeGenStpImm7(    'stp',                   cbMem =  4 * 2, sType = 'postidx'),
                A64No1CodeGenStpImm7(    'stp',                   cbMem =  4 * 2, sType = 'preidx'),
                A64No1CodeGenStpImm7(    'stp',                   cbMem =  4 * 2, sType = 'signed'),
                A64No1CodeGenStpImm7Fp(  'stp',                   cbMem = 16 * 2, sType = 'postidx'),
                A64No1CodeGenStpImm7Fp(  'stp',                   cbMem = 16 * 2, sType = 'preidx'),
                A64No1CodeGenStpImm7Fp(  'stp',                   cbMem = 16 * 2, sType = 'signed'),
                A64No1CodeGenStpImm7Fp(  'stp',                   cbMem =  8 * 2, sType = 'postidx'),
                A64No1CodeGenStpImm7Fp(  'stp',                   cbMem =  8 * 2, sType = 'preidx'),
                A64No1CodeGenStpImm7Fp(  'stp',                   cbMem =  8 * 2, sType = 'signed'),
                A64No1CodeGenStpImm7Fp(  'stp',                   cbMem =  4 * 2, sType = 'postidx'),
                A64No1CodeGenStpImm7Fp(  'stp',                   cbMem =  4 * 2, sType = 'preidx'),
                A64No1CodeGenStpImm7Fp(  'stp',                   cbMem =  4 * 2, sType = 'signed'),

                A64No1CodeGenLdpImm7(    'ldnp',   calcLdUnsigned, cbMem =  8 * 2, sType = 'signed'),
                A64No1CodeGenLdpImm7(    'ldnp',   calcLdUnsigned, cbMem =  4 * 2, sType = 'signed'),
                A64No1CodeGenLdpImm7Fp(  'ldnp',                   cbMem = 16 * 2, sType = 'signed'),
                A64No1CodeGenLdpImm7Fp(  'ldnp',                   cbMem =  8 * 2, sType = 'signed'),
                A64No1CodeGenLdpImm7Fp(  'ldnp',                   cbMem =  4 * 2, sType = 'signed'),

                A64No1CodeGenStpImm7(    'stnp',                   cbMem =  8 * 2, sType = 'signed'),
                A64No1CodeGenStpImm7(    'stnp',                   cbMem =  4 * 2, sType = 'signed'),
                A64No1CodeGenStpImm7Fp(  'stnp',                   cbMem = 16 * 2, sType = 'signed'),
                A64No1CodeGenStpImm7Fp(  'stnp',                   cbMem =  8 * 2, sType = 'signed'),
                A64No1CodeGenStpImm7Fp(  'stnp',                   cbMem =  4 * 2, sType = 'signed'),
            ];

            # PC relative loads:
            aoGenerators += [
                A64No1CodeGenLdLiteral(  'ldr',   calcLdUnsigned, cbMem =  8),
                A64No1CodeGenLdLiteral(  'ldr',   calcLdUnsigned, cbMem =  4),
                A64No1CodeGenLdLiteral(  'ldrsw', calcLdSigned,   cbMem =  4, cBits = 64),
                A64No1CodeGenLdLiteralFp('ldr',                   cbMem = 16),
                A64No1CodeGenLdLiteralFp('ldr',                   cbMem =  8),
                A64No1CodeGenLdLiteralFp('ldr',                   cbMem =  4),
            ];

            # Loads and stores with scaled 12-bit immediates:
            aoGenerators += [
                A64No1CodeGenStReg(      'str',                   cbMem =  8),
                A64No1CodeGenStReg(      'str',                   cbMem =  4),
                A64No1CodeGenStReg(      'strh',                  cbMem =  2),
                A64No1CodeGenStReg(      'strb',                  cbMem =  1),
                A64No1CodeGenStRegFp(    'str',                   cbMem = 16),
                A64No1CodeGenStRegFp(    'str',                   cbMem =  8),
                A64No1CodeGenStRegFp(    'str',                   cbMem =  4),
                A64No1CodeGenStRegFp(    'str',                   cbMem =  2),
                A64No1CodeGenStRegFp(    'str',                   cbMem =  1),

                A64No1CodeGenLdReg(      'ldr',   calcLdUnsigned, cbMem =  8),
                A64No1CodeGenLdReg(      'ldr',   calcLdUnsigned, cbMem =  4),
                A64No1CodeGenLdReg(      'ldrh',  calcLdUnsigned, cbMem =  2),
                A64No1CodeGenLdReg(      'ldrsh', calcLdSigned,   cbMem =  2),
                A64No1CodeGenLdReg(      'ldrsh', calcLdSigned,   cbMem =  2, cBits = 64),
                A64No1CodeGenLdReg(      'ldrb',  calcLdUnsigned, cbMem =  1),
                A64No1CodeGenLdReg(      'ldrsb', calcLdSigned,   cbMem =  1),
                A64No1CodeGenLdReg(      'ldrsb', calcLdSigned,   cbMem =  1, cBits = 64),
                A64No1CodeGenLdRegFp(    'ldr',                   cbMem = 16),
                A64No1CodeGenLdRegFp(    'ldr',                   cbMem =  8),
                A64No1CodeGenLdRegFp(    'ldr',                   cbMem =  4),
                A64No1CodeGenLdRegFp(    'ldr',                   cbMem =  2),
                A64No1CodeGenLdRegFp(    'ldr',                   cbMem =  1),
            ];

            # Loads and stores with scaled 12-bit immediates:
            aoGenerators += [
                A64No1CodeGenStImm12Fp(  'str',                   cbMem = 16),
                A64No1CodeGenStImm12Fp(  'str',                   cbMem =  8),
                A64No1CodeGenStImm12Fp(  'str',                   cbMem =  4),
                A64No1CodeGenStImm12Fp(  'str',                   cbMem =  2),
                A64No1CodeGenStImm12Fp(  'str',                   cbMem =  1),
                A64No1CodeGenStImm12(    'str',                   cbMem =  8),
                A64No1CodeGenStImm12(    'str',                   cbMem =  4),
                A64No1CodeGenStImm12(    'strh',                  cbMem =  2),
                A64No1CodeGenStImm12(    'strb',                  cbMem =  1),

                A64No1CodeGenLdImm12(    'ldr',   calcLdUnsigned, cbMem =  8),
                A64No1CodeGenLdImm12(    'ldr',   calcLdUnsigned, cbMem =  4),
                A64No1CodeGenLdImm12(    'ldrsw', calcLdSigned,   cbMem =  4, cBits = 64),
                A64No1CodeGenLdImm12(    'ldrh',  calcLdUnsigned, cbMem =  2),
                A64No1CodeGenLdImm12(    'ldrsh', calcLdSigned,   cbMem =  2),
                A64No1CodeGenLdImm12(    'ldrsh', calcLdSigned,   cbMem =  2, cBits = 64),
                A64No1CodeGenLdImm12(    'ldrb',  calcLdUnsigned, cbMem =  1),
                A64No1CodeGenLdImm12(    'ldrsb', calcLdSigned,   cbMem =  1),
                A64No1CodeGenLdImm12(    'ldrsb', calcLdSigned,   cbMem =  1, cBits = 64),
                A64No1CodeGenLdImm12Fp(  'ldr',   calcLdUnsigned, cbMem = 16),
                A64No1CodeGenLdImm12Fp(  'ldr',   calcLdUnsigned, cbMem =  8),
                A64No1CodeGenLdImm12Fp(  'ldr',   calcLdUnsigned, cbMem =  4),
                A64No1CodeGenLdImm12Fp(  'ldr',   calcLdUnsigned, cbMem =  2),
                A64No1CodeGenLdImm12Fp(  'ldr',   calcLdUnsigned, cbMem =  1),
                A64No1CodeGenLdImm12Fp(  'ldr',   calcLdUnsigned, cbMem =  1),
            ];

            # Loads and stores with unscaled 9-bit signed immediates:
            aoGenerators += [
                A64No1CodeGenStImm9Fp(   'str',                   cbMem = 16,              sType = 'postidx'),
                A64No1CodeGenStImm9Fp(   'str',                   cbMem = 16,              sType = 'preidx'),
                A64No1CodeGenStImm9Fp(   'str',                   cbMem =  8,              sType = 'postidx'),
                A64No1CodeGenStImm9Fp(   'str',                   cbMem =  8,              sType = 'preidx'),
                A64No1CodeGenStImm9Fp(   'str',                   cbMem =  4,              sType = 'postidx'),
                A64No1CodeGenStImm9Fp(   'str',                   cbMem =  4,              sType = 'preidx'),
                A64No1CodeGenStImm9Fp(   'str',                   cbMem =  2,              sType = 'postidx'),
                A64No1CodeGenStImm9Fp(   'str',                   cbMem =  2,              sType = 'preidx'),
                A64No1CodeGenStImm9Fp(   'str',                   cbMem =  1,              sType = 'postidx'),
                A64No1CodeGenStImm9Fp(   'str',                   cbMem =  1,              sType = 'preidx'),
                A64No1CodeGenStImm9(     'str',                   cbMem =  8,              sType = 'postidx'),
                A64No1CodeGenStImm9(     'str',                   cbMem =  8,              sType = 'preidx'),
                A64No1CodeGenStImm9(     'str',                   cbMem =  4,              sType = 'postidx'),
                A64No1CodeGenStImm9(     'str',                   cbMem =  4,              sType = 'preidx'),
                A64No1CodeGenStImm9(     'strh',                  cbMem =  2,              sType = 'postidx'),
                A64No1CodeGenStImm9(     'strh',                  cbMem =  2,              sType = 'preidx'),
                A64No1CodeGenStImm9(     'strb',                  cbMem =  1,              sType = 'postidx'),
                A64No1CodeGenStImm9(     'strb',                  cbMem =  1,              sType = 'preidx'),
                A64No1CodeGenLdImm9(     'ldr',   calcLdUnsigned, cbMem =  8,              sType = 'postidx'),
                A64No1CodeGenLdImm9(     'ldr',   calcLdUnsigned, cbMem =  8,              sType = 'preidx'),
                A64No1CodeGenLdImm9(     'ldr',   calcLdUnsigned, cbMem =  4,              sType = 'postidx'),
                A64No1CodeGenLdImm9(     'ldr',   calcLdUnsigned, cbMem =  4,              sType = 'preidx'),
                A64No1CodeGenLdImm9(     'ldrh',  calcLdUnsigned, cbMem =  2,              sType = 'postidx'),
                A64No1CodeGenLdImm9(     'ldrh',  calcLdUnsigned, cbMem =  2,              sType = 'preidx'),
                A64No1CodeGenLdImm9(     'ldrb',  calcLdUnsigned, cbMem =  1,              sType = 'postidx'),
                A64No1CodeGenLdImm9(     'ldrb',  calcLdUnsigned, cbMem =  1,              sType = 'preidx'),
                A64No1CodeGenLdImm9(     'ldrsw', calcLdSigned,   cbMem =  4, cBits =  64, sType = 'postidx'),
                A64No1CodeGenLdImm9(     'ldrsw', calcLdSigned,   cbMem =  4, cBits =  64, sType = 'preidx'),
                A64No1CodeGenLdImm9(     'ldrsh', calcLdSigned,   cbMem =  2,              sType = 'postidx'),
                A64No1CodeGenLdImm9(     'ldrsh', calcLdSigned,   cbMem =  2,              sType = 'preidx'),
                A64No1CodeGenLdImm9(     'ldrsh', calcLdSigned,   cbMem =  2, cBits =  64, sType = 'postidx'),
                A64No1CodeGenLdImm9(     'ldrsh', calcLdSigned,   cbMem =  2, cBits =  64, sType = 'preidx'),
                A64No1CodeGenLdImm9(     'ldrsb', calcLdSigned,   cbMem =  1,              sType = 'postidx'),
                A64No1CodeGenLdImm9(     'ldrsb', calcLdSigned,   cbMem =  1,              sType = 'preidx'),
                A64No1CodeGenLdImm9(     'ldrsb', calcLdSigned,   cbMem =  1, cBits =  64, sType = 'postidx'),
                A64No1CodeGenLdImm9(     'ldrsb', calcLdSigned,   cbMem =  1, cBits =  64, sType = 'preidx'),
                A64No1CodeGenLdImm9Fp(   'ldr',   calcLdUnsigned, cbMem = 16,              sType = 'postidx'),
                A64No1CodeGenLdImm9Fp(   'ldr',   calcLdUnsigned, cbMem = 16,              sType = 'preidx'),
                A64No1CodeGenLdImm9Fp(   'ldr',   calcLdUnsigned, cbMem =  8,              sType = 'postidx'),
                A64No1CodeGenLdImm9Fp(   'ldr',   calcLdUnsigned, cbMem =  8,              sType = 'preidx'),
                A64No1CodeGenLdImm9Fp(   'ldr',   calcLdUnsigned, cbMem =  4,              sType = 'postidx'),
                A64No1CodeGenLdImm9Fp(   'ldr',   calcLdUnsigned, cbMem =  4,              sType = 'preidx'),
                A64No1CodeGenLdImm9Fp(   'ldr',   calcLdUnsigned, cbMem =  2,              sType = 'postidx'),
                A64No1CodeGenLdImm9Fp(   'ldr',   calcLdUnsigned, cbMem =  2,              sType = 'preidx'),
                A64No1CodeGenLdImm9Fp(   'ldr',   calcLdUnsigned, cbMem =  1,              sType = 'postidx'),
                A64No1CodeGenLdImm9Fp(   'ldr',   calcLdUnsigned, cbMem =  1,              sType = 'preidx'),
                A64No1CodeGenStImm9Fp(   'stur',                  cbMem = 16,              sType = 'unscaled'),
                A64No1CodeGenStImm9Fp(   'stur',                  cbMem =  8,              sType = 'unscaled'),
                A64No1CodeGenStImm9Fp(   'stur',                  cbMem =  4,              sType = 'unscaled'),
                A64No1CodeGenStImm9Fp(   'stur',                  cbMem =  2,              sType = 'unscaled'),
                A64No1CodeGenStImm9Fp(   'stur',                  cbMem =  1,              sType = 'unscaled'),
                A64No1CodeGenLdImm9Fp(   'ldur',  calcLdUnsigned, cbMem = 16,              sType = 'unscaled'),
                A64No1CodeGenLdImm9Fp(   'ldur',  calcLdUnsigned, cbMem =  8,              sType = 'unscaled'),
                A64No1CodeGenLdImm9Fp(   'ldur',  calcLdUnsigned, cbMem =  4,              sType = 'unscaled'),
                A64No1CodeGenLdImm9Fp(   'ldur',  calcLdUnsigned, cbMem =  2,              sType = 'unscaled'),
                A64No1CodeGenLdImm9Fp(   'ldur',  calcLdUnsigned, cbMem =  1,              sType = 'unscaled'),
                A64No1CodeGenStImm9(     'stur',                  cbMem =  8,              sType = 'unscaled'),
                A64No1CodeGenStImm9(     'stur',                  cbMem =  4,              sType = 'unscaled'),
                A64No1CodeGenStImm9(     'sturh',                 cbMem =  2,              sType = 'unscaled'),
                A64No1CodeGenStImm9(     'sturb',                 cbMem =  1,              sType = 'unscaled'),
                A64No1CodeGenLdImm9(     'ldur',  calcLdUnsigned, cbMem =  8,              sType = 'unscaled'),
                A64No1CodeGenLdImm9(     'ldur',  calcLdUnsigned, cbMem =  4,              sType = 'unscaled'),
                A64No1CodeGenLdImm9(     'ldursw', calcLdSigned,  cbMem =  4, cBits =  64, sType = 'unscaled'),
                A64No1CodeGenLdImm9(     'ldurh', calcLdUnsigned, cbMem =  2,              sType = 'unscaled'),
                A64No1CodeGenLdImm9(     'ldursh', calcLdSigned,  cbMem =  2,              sType = 'unscaled'),
                A64No1CodeGenLdImm9(     'ldursh', calcLdSigned,  cbMem =  2, cBits =  64, sType = 'unscaled'),
                A64No1CodeGenLdImm9(     'ldurb', calcLdUnsigned, cbMem =  1,              sType = 'unscaled'),
                A64No1CodeGenLdImm9(     'ldursb', calcLdSigned,  cbMem =  1,              sType = 'unscaled'),
                A64No1CodeGenLdImm9(     'ldursb', calcLdSigned,  cbMem =  1, cBits =  64, sType = 'unscaled'),
            ];

            # FEAT_LRCPC & FEAT_LRCPC2 instructions.
            aoGenerators += [
                A64No1CodeGenLdMemOp('ldapr',    cbMem = 8, cbAlign = 8),
                A64No1CodeGenLdMemOp('ldapr',    cbMem = 4, cbAlign = 4),
                A64No1CodeGenLdMemOp('ldaprh',   cbMem = 2, cbAlign = 2),
                A64No1CodeGenLdMemOp('ldaprb',   cbMem = 1, cbAlign = 1),

                A64No1CodeGenLdImm9( 'ldapur',   cbMem = 8, cbAlign = 8, sType = 'unscaled'),
                A64No1CodeGenLdImm9( 'ldapur',   cbMem = 4, cbAlign = 4, sType = 'unscaled'),
                A64No1CodeGenLdImm9( 'ldapurh',  cbMem = 2, cbAlign = 2, sType = 'unscaled'),
                A64No1CodeGenLdImm9( 'ldapurb',  cbMem = 1, cbAlign = 1, sType = 'unscaled'),
                A64No1CodeGenLdImm9( 'ldapursb', cbMem = 1, cbAlign = 1, sType = 'unscaled', cBits = 32, fnCalc = calcLdSigned),
                A64No1CodeGenLdImm9( 'ldapursb', cbMem = 1, cbAlign = 1, sType = 'unscaled', cBits = 64, fnCalc = calcLdSigned),
                A64No1CodeGenLdImm9( 'ldapursh', cbMem = 2, cbAlign = 2, sType = 'unscaled', cBits = 32, fnCalc = calcLdSigned),
                A64No1CodeGenLdImm9( 'ldapursh', cbMem = 2, cbAlign = 2, sType = 'unscaled', cBits = 64, fnCalc = calcLdSigned),
                A64No1CodeGenLdImm9( 'ldapursw', cbMem = 4, cbAlign = 4, sType = 'unscaled', cBits = 64, fnCalc = calcLdSigned),
            ];


        if True: # pylint: disable=using-constant-test
            # Add & Sub
            aoGenerators += [
                # addsub_imm:
                A64No1CodeGenAddSubImm(  'add',  calcAdd),
                A64No1CodeGenAddSubImm(  'adds', calcAdd, fWithFlags = True),
                A64No1CodeGenAddSubImm(  'sub',  calcSub),
                A64No1CodeGenAddSubImm(  'subs', calcSub, fWithFlags = True),
                # addsub_shift:
                A64No1CodeGenShiftedReg( 'add',  calcAdd),
                A64No1CodeGenShiftedReg( 'adds', calcAdd, fWithFlags = True),
                A64No1CodeGenShiftedReg( 'sub',  calcSub),
                A64No1CodeGenShiftedReg( 'subs', calcSub, fWithFlags = True),
                # addsub_ext
                A64No1CodeGenExtendedReg('add',  calcAdd),
                A64No1CodeGenExtendedReg('adds', calcAdd, fWithFlags = True),
                A64No1CodeGenExtendedReg('sub',  calcSub),
                A64No1CodeGenExtendedReg('subs', calcSub, fWithFlags = True),
                # addsub_carry:
                A64No1CodeGenAddSubCarry('adc',  calcAdd),
                A64No1CodeGenAddSubCarry('adcs', calcAdd, fWithFlags = True),
                A64No1CodeGenAddSubCarry('sbc',  calcSubBorrow),
                A64No1CodeGenAddSubCarry('sbcs', calcSubBorrow, fWithFlags = True),
            ];

        if True: # pylint: disable=using-constant-test
            # log_shift:
            aoGenerators += [
                A64No1CodeGenShiftedReg( 'and',  calcAnd, fWithRor = True),
                A64No1CodeGenShiftedReg( 'ands', calcAnd, fWithRor = True, fWithFlags = True),
                A64No1CodeGenShiftedReg( 'bic',  calcBic, fWithRor = True),
                A64No1CodeGenShiftedReg( 'bics', calcBic, fWithRor = True, fWithFlags = True),
                A64No1CodeGenShiftedReg( 'orr',  calcOrr, fWithRor = True),
                A64No1CodeGenShiftedReg( 'orn',  calcOrn, fWithRor = True),
                A64No1CodeGenShiftedReg( 'eor',  calcEor, fWithRor = True),
                A64No1CodeGenShiftedReg( 'eon',  calcEon, fWithRor = True),
            ];

        if True: # pylint: disable=using-constant-test
            # bitfield:
            aoGenerators += [
                A64No1CodeGenBitfieldMove('bfm',  calcBfm),
                A64No1CodeGenBitfieldMove('sbfm', calcSbfm),
                A64No1CodeGenBitfieldMove('ubfm', calcUbfm),
            ];


        #
        # Produce the file.
        #
        asLines = [
            '/* Automatically Generated. Do not edit! Seed: %#x */' % (oOptions.iRandSeed,),
            '#include <iprt/asmdefs-arm.h>',
            '',
            '#ifdef RT_OS_WINDOWS',
            '.arch_extension rcpc3',
            '#endif',
            '',
            '',
            '/*',
            ' * Globals',
            ' */',
            'BEGINDATA',
            'g_u64SavedSp: .quad 0',
            '',
            '',
            '',
            'BEGINCODE',
            '',
            '/*',
            ' * Top level dispatcher.',
            ' */',
            'BEGINPROC GeneratedTests',
            '        stp     fp, lr, [sp, #-64]!',
        ];
        for oGenerator in aoGenerators:
            asLines.append('        bl      NAME(%s)' % (oGenerator.sLabel,));
        asLines += [
            '        ldp     fp, lr, [sp], #64',
            '        ret',
            'ENDPROC   GeneratedTests',
        ];

        for oGenerator in aoGenerators:
            asLines.extend(('','','',));
            oGenerator.generate(self.oOptions);
            asLines.append('.balign 4') ## @todo ALIGNCODE doesn't work
            asLines.extend(oGenerator.asCode);
            asLines.extend(('','',));
            asLines.extend(oGenerator.asData);

        if self.oOptions.sFileOut:
            with open(self.oOptions.sFileOut, 'wt', encoding = 'utf-8', errors = 'strict') as oFile:
                oFile.write('\n'.join(asLines));
                oFile.write('\n');
        else:
            sys.stdout.write('\n'.join(asLines));
            sys.stdout.write('\n');

        return 0;




if __name__ == '__main__':
    sys.exit(pycmn.mainWrapperCatchXcptAndDoProfiling(Arm64No1CodeGen().main));

