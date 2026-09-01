/** $Id: DevE1000Phy.cpp 112643 2026-01-19 14:25:02Z aleksey.ilyushin@oracle.com $ */
/** @file
 * DevE1000Phy - Intel 82540EM Ethernet Controller Internal PHY Emulation.
 *
 * Implemented in accordance with the specification:
 *      PCI/PCI-X Family of Gigabit Ethernet Controllers Software Developer's
 *      Manual 82540EP/EM, 82541xx, 82544GC/EI, 82545GM/EM, 82546GB/EB, and
 *      82547xx
 *
 *      317453-002 Revision 3.5
 */

/*
 * Copyright (C) 2007-2025 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#define LOG_GROUP LOG_GROUP_DEV_E1000

/** @todo Remove me! For now I want asserts to work in release code. */
// #ifndef RT_STRICT
// #define RT_STRICT
#include <iprt/assert.h>
// #undef  RT_STRICT
// #endif

#include <iprt/errcore.h>
#include <VBox/log.h>
#ifdef IN_RING3
# include <VBox/vmm/pdmdev.h>
#endif
#include "DevE1000Ver.h"
#include "DevE1000Phy.h"

/* Little helpers ************************************************************/
#ifdef PHY_UNIT_TEST
# ifdef CPP_UNIT
#  include <stdio.h>
#  define PhyLog(a)               printf a
# else
#  include <iprt/test.h>
#  define PhyLogC99(...)         RTTestIPrintf(RTTESTLVL_ALWAYS, __VA_ARGS__)
#  define PhyLog(a)              PhyLogC99 a
# endif
#else  /* !PHY_UNIT_TEST */
# define PhyLog(a)               Log(a)
#endif /* !PHY_UNIT_TEST */

#define REG(x) pPhy->regs.r##x


/* Internals */
namespace Phy {
#if defined(LOG_ENABLED) || defined(PHY_UNIT_TEST) || defined(IN_RING3)
    /** Retrieves state name by id */
    static const char * getStateName(uint16_t u16State);
#endif
    /** Look up register index by address. */
    static int lookupRegister(uint32_t u32Address);
    /** Software-triggered reset. */
    static void softReset(PPHY pPhy, PPDMDEVINS pDevIns);

    /** Read callback. */
    typedef uint16_t FNREAD(PPHY pPhy, uint32_t index, PPDMDEVINS pDevIns);
    /** Write callback. */
    typedef void     FNWRITE(PPHY pPhy, uint32_t index, uint16_t u16Value, PPDMDEVINS pDevIns);

    /** @name Generic handlers
     * @{ */
    static FNWRITE regWriteReadOnly;
    /** @} */
    /** @name Register-specific handlers
     * @{ */
    static FNWRITE regWritePCTRL;
    static FNREAD  regReadPSTATUS;
    static FNREAD  regReadGSTATUS;
    /** @} */

#define PHY_RD_DEFAULT(reg) Phy::regReadDefault##reg
#define PHY_WR_DEFAULT(reg) Phy::regWriteDefault##reg
#define PHY_WR_READONLY(reg) Phy::regWriteReadOnly##reg

#define PHY_RD_DEFAULT_IMPL(reg) static uint16_t regReadDefault##reg(PPHY pPhy, uint32_t index, PPDMDEVINS pDevIns) \
{ RT_NOREF(index, pDevIns); return pPhy->regs.r##reg; }
#define PHY_WR_DEFAULT_IMPL(reg) static void regWriteDefault##reg(PPHY pPhy, uint32_t index, uint16_t u16Value, PPDMDEVINS pDevIns) \
{ RT_NOREF(index, pDevIns); pPhy->regs.r##reg = u16Value; }
#define PHY_WR_READONLY_IMPL(reg) static void regWriteReadOnly##reg(PPHY pPhy, uint32_t index, uint16_t u16Value, PPDMDEVINS pDevIns) \
{ regWriteReadOnly(pPhy, index, u16Value, pDevIns); }

/* Instantiate generic read and write handlers */
PHY_RD_DEFAULT_IMPL(PCTRL)
PHY_RD_DEFAULT_IMPL(PID)
PHY_RD_DEFAULT_IMPL(EPID)
PHY_RD_DEFAULT_IMPL(ANA)
PHY_RD_DEFAULT_IMPL(LPA)
PHY_RD_DEFAULT_IMPL(GCON)
PHY_RD_DEFAULT_IMPL(PSCON)
PHY_RD_DEFAULT_IMPL(PSSTAT)

PHY_WR_DEFAULT_IMPL(ANA)
PHY_WR_DEFAULT_IMPL(PSCON)

PHY_WR_READONLY_IMPL(PSTATUS)
PHY_WR_READONLY_IMPL(PID)
PHY_WR_READONLY_IMPL(EPID)
PHY_WR_READONLY_IMPL(LPA)
PHY_WR_READONLY_IMPL(ANE)
PHY_WR_READONLY_IMPL(LPN)
PHY_WR_READONLY_IMPL(GSTATUS)
PHY_WR_READONLY_IMPL(EPSTATUS)
PHY_WR_READONLY_IMPL(PSSTAT)
PHY_WR_READONLY_IMPL(PINTS)
PHY_WR_READONLY_IMPL(PREC)


    /**
    * PHY register map table.
    *
    * Override pfnRead and pfnWrite to implement register-specific behavior.
    */
    static struct RegMap_st
    {
        /** PHY register address. */
        uint32_t    u32Address;
        /** Read callback. */
        FNREAD     *pfnRead;
        /** Write callback. */
        FNWRITE    *pfnWrite;
        /** Abbreviated name. */
        const char *pszAbbrev;
        /** Full name. */
        const char *pszName;
    } s_regMap[] =
    {
        /*ra  read callback              write callback              abbrev      full name                     */
        /*--  -------------------------  --------------------------  ----------  ------------------------------*/
        {  0, PHY_RD_DEFAULT(PCTRL)    , Phy::regWritePCTRL        , "PCTRL"    , "PHY Control" },
        {  1, Phy::regReadPSTATUS      , PHY_WR_READONLY(PSTATUS)  , "PSTATUS"  , "PHY Status" },
        {  2, PHY_RD_DEFAULT(PID)      , PHY_WR_READONLY(PID)      , "PID"      , "PHY Identifier" },
        {  3, PHY_RD_DEFAULT(EPID)     , PHY_WR_READONLY(EPID)     , "EPID"     , "Extended PHY Identifier" },
        {  4, PHY_RD_DEFAULT(ANA)      , PHY_WR_DEFAULT(ANA)       , "ANA"      , "Auto-Negotiation Advertisement" },
        {  5, PHY_RD_DEFAULT(LPA)      , PHY_WR_READONLY(LPA)      , "LPA"      , "Link Partner Ability" },
        {  6, NULL                     , PHY_WR_READONLY(ANE)      , "ANE"      , "Auto-Negotiation Expansion" },
        {  7, NULL                     , NULL                      , "NPT"      , "Next Page Transmit" },
        {  8, NULL                     , PHY_WR_READONLY(LPN)      , "LPN"      , "Link Partner Next Page" },
        {  9, PHY_RD_DEFAULT(GCON)     , NULL                      , "GCON"     , "1000BASE-T Control" },
        { 10, Phy::regReadGSTATUS      , PHY_WR_READONLY(GSTATUS)  , "GSTATUS"  , "1000BASE-T Status" },
        { 15, NULL                     , PHY_WR_READONLY(EPSTATUS) , "EPSTATUS" , "Extended PHY Status" },
        { 16, PHY_RD_DEFAULT(PSCON)    , PHY_WR_DEFAULT(PSCON)     , "PSCON"    , "PHY Specific Control" },
        { 17, PHY_RD_DEFAULT(PSSTAT)   , PHY_WR_READONLY(PSSTAT)   , "PSSTAT"   , "PHY Specific Status" },
        { 18, NULL                     , NULL                      , "PINTE"    , "PHY Interrupt Enable" },
        { 19, NULL                     , PHY_WR_READONLY(PINTS)    , "PINTS"    , "PHY Interrupt Status" },
        { 20, NULL                     , NULL                      , "EPSCON1"  , "Extended PHY Specific Control 1" },
        { 21, NULL                     , PHY_WR_READONLY(PREC)     , "PREC"     , "PHY Receive Error Counter" },
        { 26, NULL                     , NULL                      , "EPSCON2"  , "Extended PHY Specific Control 2" },
        { 29, NULL                     , NULL                      , "R30PS"    , "MDI Register 30 Page Select" },
        { 30, NULL                     , NULL                      , "R30AW"    , "MDI Register 30 Access Window" }
    };
}

static void Phy::regWriteReadOnly(PPHY pPhy, uint32_t index, uint16_t u16Value, PPDMDEVINS pDevIns)
{
    RT_NOREF(pPhy, index, u16Value, pDevIns); \
    PhyLog(("PHY#%d At %02d write attempted to read-only '%s'\n", \
            pPhy->iInstance, s_regMap[index].u32Address, s_regMap[index].pszName)); \
}

/**
 * Search PHY register table for register with matching address.
 *
 * @returns Index in the register table or -1 if not found.
 *
 * @param   u32Address  Register address.
 */
static int Phy::lookupRegister(uint32_t u32Address)
{
    unsigned int index;

    for (index = 0; index < RT_ELEMENTS(s_regMap); index++)
    {
        if (s_regMap[index].u32Address == u32Address)
        {
            return (int)index;
        }
    }

    return -1;
}

/**
 * Read PHY register.
 *
 * @returns Value of specified PHY register.
 *
 * @param   u32Address  Register address.
 */
uint16_t Phy::readRegister(PPHY pPhy, uint32_t u32Address, PPDMDEVINS pDevIns)
{
    int      index = Phy::lookupRegister(u32Address);
    uint16_t u16   = 0;

    if (index >= 0)
    {
        if (s_regMap[index].pfnRead == NULL)
        {
            PhyLog(("PHY#%d At %02d read (%04X) attempt from unimplemented %s (%s)\n",
                    pPhy->iInstance, s_regMap[index].u32Address, u16,
                    s_regMap[index].pszAbbrev, s_regMap[index].pszName));

        }
        else
        {
            u16 = s_regMap[index].pfnRead(pPhy, (uint32_t)index, pDevIns);
            PhyLog(("PHY#%d At %02d read  %04X      from %s (%s)\n",
                    pPhy->iInstance, s_regMap[index].u32Address, u16,
                    s_regMap[index].pszAbbrev, s_regMap[index].pszName));
        }
    }
    else
    {
        PhyLog(("PHY#%d read attempted from non-existing register %08x\n",
                pPhy->iInstance, u32Address));
    }
    return u16;
}

/**
 * Write to PHY register.
 *
 * @param   u32Address  Register address.
 * @param   u16Value    Value to store.
 */
void Phy::writeRegister(PPHY pPhy, uint32_t u32Address, uint16_t u16Value, PPDMDEVINS pDevIns)
{
    int index = Phy::lookupRegister(u32Address);

    if (index >= 0)
    {
        if (s_regMap[index].pfnWrite == NULL)
        {
            PhyLog(("PHY#%d At %02d write attempt (%04X) to unimplemented %s (%s)\n",
                    pPhy->iInstance, s_regMap[index].u32Address, u16Value,
                    s_regMap[index].pszAbbrev, s_regMap[index].pszName));
        }
        else
        {
            PhyLog(("PHY#%d At %02d write      %04X  to  %s (%s)\n",
                    pPhy->iInstance, s_regMap[index].u32Address, u16Value,
                    s_regMap[index].pszAbbrev, s_regMap[index].pszName));
            s_regMap[index].pfnWrite(pPhy, (uint32_t)index, u16Value, pDevIns);
        }
    }
    else
    {
        PhyLog(("PHY#%d write attempted to non-existing register %08x\n",
                pPhy->iInstance, u32Address));
    }
}

/**
 * PHY constructor.
 *
 * Stores E1000 instance internally. Triggers PHY hard reset.
 *
 * @param   iNICInstance   Number of network controller instance this PHY is
 *                         attached to.
 * @param   u16EPid        Extended PHY Id.
 */
void Phy::init(PPHY pPhy, int iNICInstance, uint16_t u16EPid)
{
    pPhy->iInstance = iNICInstance;
    /* The PHY identifier composed of bits 3 through 18 of the OUI */
    /* (Organizationally Unique Identifier). OUI is 0x05043.       */
    REG(PID)      = 0x0141;
    /* Extended PHY identifier */
    REG(EPID)     = u16EPid;
    hardReset(pPhy);
}

/**
 * Hardware PHY reset.
 *
 * Sets all PHY registers to their initial values.
 */
void Phy::hardReset(PPHY pPhy)
{
    PhyLog(("PHY#%d Hard reset\n", pPhy->iInstance));
    REG(PCTRL) = PCTRL_SPDSELM | PCTRL_DUPMOD | PCTRL_ANEG;
    /*
     * 100 and 10 FD/HD, Extended Status, MF Preamble Suppression,
     * AUTO NEG AB, EXT CAP
     */
    REG(PSTATUS)  = 0x7949;
    REG(ANA)      = 0x01E1;
    /* No flow control by our link partner, all speeds */
    REG(LPA)      = 0x01E0;
    REG(ANE)      = 0x0000;
    REG(NPT)      = 0x2001;
    REG(LPN)      = 0x0000;
    REG(GCON)     = 0x1E00;
    REG(GSTATUS)  = 0x0000;
    REG(EPSTATUS) = 0x3000;
    REG(PSCON)    = 0x0068;
    REG(PSSTAT)   = 0x0000;
    REG(PINTE)    = 0x0000;
    REG(PINTS)    = 0x0000;
    REG(EPSCON1)  = 0x0D60;
    REG(PREC)     = 0x0000;
    REG(EPSCON2)  = 0x000C;
    REG(R30PS)    = 0x0000;
    REG(R30AW)    = 0x0000;

    pPhy->u16State = MDIO_IDLE;
}

/**
 * Software PHY reset.
 */
static void Phy::softReset(PPHY pPhy, PPDMDEVINS pDevIns)
{
    PhyLog(("PHY#%d Soft reset\n", pPhy->iInstance));

    REG(PCTRL)    = REG(PCTRL) & (PCTRL_SPDSELM | PCTRL_DUPMOD | PCTRL_ANEG | PCTRL_SPDSELL);
    /*
     * 100 and 10 FD/HD, Extended Status, MF Preamble Suppression,
     * AUTO NEG AB, EXT CAP
     */
    REG(PSTATUS)  = 0x7949;
    REG(PSSTAT)  &= 0xe001;
    PhyLog(("PHY#%d PSTATUS=%04x PSSTAT=%04x\n", pPhy->iInstance, REG(PSTATUS), REG(PSSTAT)));

#ifndef PHY_UNIT_TEST
    e1kPhyLinkResetCallback(pDevIns);
#else
    RT_NOREF(pDevIns);
#endif
}

/**
 * Get the current state of the link.
 *
 * @returns true if link is up.
 */
bool Phy::isLinkUp(PPHY pPhy)
{
    return (REG(PSSTAT) & PSSTAT_LINK) != 0;
}

/**
 * Set the current state of the link.
 *
 * @remarks Link Status bit in PHY Status register is latched-low and does
 *          not change the state when the link goes up.
 *
 * @param   fLinkIsUp   New state of the link.
 */
void Phy::setLinkStatus(PPHY pPhy, bool fLinkIsUp)
{
    if (fLinkIsUp)
    {
        REG(PSSTAT)  |= PSSTAT_LINK_ALL;
        REG(PSTATUS) |= PSTATUS_NEGCOMP; /* PSTATUS_LNKSTAT is latched low */
    }
    else
    {
        REG(PSSTAT)  &= ~PSSTAT_LINK_ALL;
        REG(PSTATUS) &= ~(PSTATUS_LNKSTAT | PSTATUS_NEGCOMP);
    }
    PhyLog(("PHY#%d setLinkStatus: PSTATUS=%04x PSSTAT=%04x\n", pPhy->iInstance, REG(PSTATUS), REG(PSSTAT)));
}

#ifdef IN_RING3

/*
 * PHY is not a separate device, but a part of e1000. The versions of PHY saved states
 * depend on the versions of e1000.
 */
static SSMFIELD const s_aPhyRegFields[] =
{
    SSMFIELD_ENTRY(Phy::PHYREGS, rPCTRL),
    SSMFIELD_ENTRY(Phy::PHYREGS, rPSTATUS),
    SSMFIELD_ENTRY(Phy::PHYREGS, rPID),
    SSMFIELD_ENTRY(Phy::PHYREGS, rEPID),
    SSMFIELD_ENTRY(Phy::PHYREGS, rANA),
    SSMFIELD_ENTRY(Phy::PHYREGS, rLPA),
    SSMFIELD_ENTRY(Phy::PHYREGS, rANE),
    SSMFIELD_ENTRY(Phy::PHYREGS, rNPT),
    SSMFIELD_ENTRY(Phy::PHYREGS, rLPN),
    SSMFIELD_ENTRY(Phy::PHYREGS, rGCON),
    SSMFIELD_ENTRY(Phy::PHYREGS, rGSTATUS),
    SSMFIELD_ENTRY(Phy::PHYREGS, rEPSTATUS),
    SSMFIELD_ENTRY(Phy::PHYREGS, rPSCON),
    SSMFIELD_ENTRY(Phy::PHYREGS, rPSSTAT),
    SSMFIELD_ENTRY(Phy::PHYREGS, rPINTE),
    SSMFIELD_ENTRY(Phy::PHYREGS, rPINTS),
    SSMFIELD_ENTRY(Phy::PHYREGS, rEPSCON1),
    SSMFIELD_ENTRY(Phy::PHYREGS, rPREC),
    SSMFIELD_ENTRY(Phy::PHYREGS, rEPSCON2),
    SSMFIELD_ENTRY(Phy::PHYREGS, rR30PS),
    SSMFIELD_ENTRY(Phy::PHYREGS, rR30AW),
    SSMFIELD_ENTRY_TERM()
};

/**
 * Save PHY state.
 *
 * @remarks Since PHY is aggregated into E1K it does not currently supports
 *          versioning of its own.
 *
 * @returns VBox status code.
 * @param   pHlp        Device helper table.
 * @param   pSSM        The handle to save the state to.
 * @param   pPhy        The pointer to this instance.
 */
int Phy::saveState(PCPDMDEVHLPR3 pHlp, PSSMHANDLE pSSM, PPHY pPhy)
{
    return pHlp->pfnSSMPutStructEx(pSSM, &pPhy->regs, sizeof(pPhy->regs), 0, s_aPhyRegFields, NULL);
}

/**
 * Restore previously saved PHY state.
 *
 * @remarks Since PHY is aggregated into E1K it does not currently supports
 *          versioning of its own.
 *
 * @returns VBox status code.
 * @param   pHlp        Device helper table.
 * @param   pSSM        The handle to save the state to.
 * @param   pPhy        The pointer to this instance.
 */
int Phy::loadState(PCPDMDEVHLPR3 pHlp, PSSMHANDLE pSSM, uint32_t uVersion, PPHY pPhy)
{
    int rc;
    if (uVersion <= E1K_SAVEDSTATE_VERSION_82583V)
    {
        uint16_t auRegs[21];
        int i = 0;
        rc = pHlp->pfnSSMGetMem(pSSM, auRegs, sizeof(auRegs));
        pPhy->regs.rPCTRL = auRegs[i++];
        pPhy->regs.rPSTATUS = auRegs[i++];
        pPhy->regs.rPID = auRegs[i++];
        pPhy->regs.rEPID = auRegs[i++];
        pPhy->regs.rANA = auRegs[i++];
        pPhy->regs.rLPA = auRegs[i++];
        pPhy->regs.rANE = auRegs[i++];
        pPhy->regs.rNPT = auRegs[i++];
        pPhy->regs.rLPN = auRegs[i++];
        pPhy->regs.rGCON = auRegs[i++];
        pPhy->regs.rGSTATUS = auRegs[i++];
        pPhy->regs.rEPSTATUS = auRegs[i++];
        pPhy->regs.rPSCON = auRegs[i++];
        pPhy->regs.rPSSTAT = auRegs[i++];
        pPhy->regs.rPINTE = auRegs[i++];
        pPhy->regs.rPINTS = auRegs[i++];
        pPhy->regs.rEPSCON1 = auRegs[i++];
        pPhy->regs.rPREC = auRegs[i++];
        pPhy->regs.rEPSCON2 = auRegs[i++];
        pPhy->regs.rR30PS = auRegs[i++];
        pPhy->regs.rR30AW = auRegs[i++];
        Assert(i == RT_ELEMENTS(auRegs));
    }
    else
        rc = pHlp->pfnSSMGetStructEx(pSSM, &pPhy->regs, sizeof(pPhy->regs), 0, s_aPhyRegFields, NULL);
    return rc;
}


/**
 * PHY status info callback (called from e1000 status info callback).
 *
 * @param   pDevIns     The device instance.
 * @param   pHlp        The output helpers.
 * @param   pszArgs     The arguments.
 * @param   pPhy        The pointer to this instance of PHY.
 */
void Phy::info(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs, PPHY pPhy)
{
    RT_NOREF(pDevIns, pszArgs);
    pHlp->pfnPrintf(pHlp, "PHY registers ----------------------------------------------------------------------\n");
    pHlp->pfnPrintf(pHlp, "    PCTRL=%04x  PSTATUS=%04x      PID=%04x     EPID=%04x      ANA=%04x      LPA=%04x\n",
                    REG(PCTRL), REG(PSTATUS), REG(PID), REG(EPID), REG(ANA), REG(LPA));
    pHlp->pfnPrintf(pHlp, "      ANE=%04x      NPT=%04x      LPN=%04x     GCON=%04x  GSTATUS=%04x EPSTATUS=%04x\n",
                    REG(ANE), REG(NPT), REG(LPN), REG(GCON), REG(GSTATUS), REG(EPSTATUS));
    pHlp->pfnPrintf(pHlp, "    PSCON=%04x   PSSTAT=%04x    PINTE=%04x    PINTS=%04x  EPSCON1=%04x     PREC=%04x\n",
                    REG(PSCON), REG(PSSTAT), REG(PINTE), REG(PINTS), REG(EPSCON1), REG(PREC));
    pHlp->pfnPrintf(pHlp, "  EPSCON2=%04x    R30PS=%04x    R30AW=%04x\n",
                    REG(EPSCON2), REG(R30PS), REG(R30AW));
    pHlp->pfnPrintf(pHlp, "\nPHY MDIO: state=%s acc=%u cnt=%u addr=%u\n\n",
                    Phy::getStateName(pPhy->u16State), pPhy->u16Acc, pPhy->u16Cnt, pPhy->u16RegAdr);
}
#endif /* IN_RING3 */

/* Register-specific handlers ************************************************/

/**
 * Write handler for PHY Control register.
 *
 * Handles reset.
 *
 * @param   index       Register index in register array.
 * @param   value       The value to store (ignored).
 */
static void Phy::regWritePCTRL(PPHY pPhy, uint32_t index, uint16_t u16Value, PPDMDEVINS pDevIns)
{
    RT_NOREF(index);
    if (u16Value & PCTRL_RESET)
        softReset(pPhy, pDevIns);
    else
        pPhy->regs.rPCTRL = u16Value;
}

/**
 * Read handler for PHY Status register.
 *
 * Handles Latched-Low Link Status bit.
 *
 * @returns Register value
 *
 * @param   index       Register index in register array.
 */
static uint16_t Phy::regReadPSTATUS(PPHY pPhy, uint32_t index, PPDMDEVINS pDevIns)
{
    RT_NOREF(pPhy, index, pDevIns);

    /* Read latched value */
    uint16_t u16 = REG(PSTATUS);
    if (REG(PSSTAT) & PSSTAT_LINK)
        REG(PSTATUS) |= PSTATUS_LNKSTAT;
    else
        REG(PSTATUS) &= ~PSTATUS_LNKSTAT;
    return u16;
}

/**
 * Read handler for 1000BASE-T Status register.
 *
 * @returns Register value
 *
 * @param   index       Register index in register array.
 */
static uint16_t Phy::regReadGSTATUS(PPHY pPhy, uint32_t index, PPDMDEVINS pDevIns)
{
    RT_NOREF(pPhy, index, pDevIns);

    /*
     * - Link partner is capable of 1000BASE-T half duplex
     * - Link partner is capable of 1000BASE-T full duplex
     * - Remote receiver OK
     * - Local receiver OK
     * - Local PHY config resolved to SLAVE
     */
    return 0x3C00;
}

#if defined(LOG_ENABLED) || defined(PHY_UNIT_TEST) || defined(IN_RING3)
static const char * Phy::getStateName(uint16_t u16State)
{
    static const char *pcszState[] =
    {
        "MDIO_IDLE",
        "MDIO_ST",
        "MDIO_OP_ADR",
        "MDIO_TA_RD",
        "MDIO_TA_WR",
        "MDIO_READ",
        "MDIO_WRITE"
    };

    return (u16State < RT_ELEMENTS(pcszState)) ? pcszState[u16State] : "<invalid>";
}
#endif

bool Phy::readMDIO(PPHY pPhy)
{
    bool fPin = false;

    switch (pPhy->u16State)
    {
        case MDIO_TA_RD:
            Assert(pPhy->u16Cnt == 1);
            fPin = false;
            pPhy->u16State = MDIO_READ;
            pPhy->u16Cnt   = 16;
            break;
        case MDIO_READ:
            /* Bits are shifted out in MSB to LSB order */
            fPin = (pPhy->u16Acc & 0x8000) != 0;
            pPhy->u16Acc <<= 1;
            if (--pPhy->u16Cnt == 0)
                pPhy->u16State = MDIO_IDLE;
            break;
        default:
            PhyLog(("PHY#%d WARNING! MDIO pin read in %s state\n", pPhy->iInstance, Phy::getStateName(pPhy->u16State)));
            pPhy->u16State = MDIO_IDLE;
    }
    return fPin;
}

/** Set the value of MDIO pin. */
void Phy::writeMDIO(PPHY pPhy, bool fPin, PPDMDEVINS pDevIns)
{
    switch (pPhy->u16State)
    {
        case MDIO_IDLE:
            if (!fPin)
                pPhy->u16State = MDIO_ST;
            break;
        case MDIO_ST:
            if (fPin)
            {
                pPhy->u16State = MDIO_OP_ADR;
                pPhy->u16Cnt   = 12; /* OP + PHYADR + REGADR */
                pPhy->u16Acc   = 0;
            }
            break;
        case MDIO_OP_ADR:
            Assert(pPhy->u16Cnt);
            /* Shift in 'u16Cnt' bits into accumulator */
            pPhy->u16Acc <<= 1;
            if (fPin)
                pPhy->u16Acc |= 1;
            if (--pPhy->u16Cnt == 0)
            {
                /* Got OP(2) + PHYADR(5) + REGADR(5) */
                /* Note: A single PHY is supported, ignore PHYADR */
                switch (pPhy->u16Acc >> 10)
                {
                    case MDIO_READ_OP:
                        pPhy->u16Acc = readRegister(pPhy, pPhy->u16Acc & 0x1F, pDevIns);
                        pPhy->u16State = MDIO_TA_RD;
                        pPhy->u16Cnt = 1;
                        break;
                    case MDIO_WRITE_OP:
                        pPhy->u16RegAdr = pPhy->u16Acc & 0x1F;
                        pPhy->u16State = MDIO_TA_WR;
                        pPhy->u16Cnt = 2;
                        break;
                    default:
                        PhyLog(("PHY#%d ERROR! Invalid MDIO op: %d\n", pPhy->iInstance, pPhy->u16Acc >> 10));
                        pPhy->u16State = MDIO_IDLE;
                        break;
                }
            }
            break;
        case MDIO_TA_WR:
            Assert(pPhy->u16Cnt <= 2);
            Assert(pPhy->u16Cnt > 0);
            if (--pPhy->u16Cnt == 0)
            {
                pPhy->u16State = MDIO_WRITE;
                pPhy->u16Cnt   = 16;
            }
            break;
        case MDIO_WRITE:
            Assert(pPhy->u16Cnt);
            pPhy->u16Acc <<= 1;
            if (fPin)
                pPhy->u16Acc |= 1;
            if (--pPhy->u16Cnt == 0)
            {
                writeRegister(pPhy, pPhy->u16RegAdr, pPhy->u16Acc, pDevIns);
                pPhy->u16State = MDIO_IDLE;
            }
            break;
        default:
            PhyLog(("PHY#%d ERROR! MDIO pin write in %s state\n", pPhy->iInstance, Phy::getStateName(pPhy->u16State)));
            pPhy->u16State = MDIO_IDLE;
            break;
    }
}

