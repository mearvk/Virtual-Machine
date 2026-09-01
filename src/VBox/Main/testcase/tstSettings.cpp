/* $Id: tstSettings.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */
/** @file
 * Settings testcases - No Main API involved.
 */

/*
 * Copyright (C) 2023-2025 Oracle and/or its affiliates.
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <typeinfo>
#include <stdexcept>

#include <iprt/errcore.h>
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/test.h>

#include <VBox/com/string.h>
#include <VBox/settings.h>

using namespace com;
using namespace settings;


/**
 * Tests a single .vbox machine configuration file.
 *
 * @param   pszSrcFile  The source XML file (readonly).
 * @param   pszOutput   Optional output file. If NULL, a temporary file is used.
 */
static void tstFileMachine(const char *pszSrcFile, const char *pszOutput)
{
    RTTestIPrintf(RTTESTLVL_ALWAYS, "Testing machine file: %s\n", pszSrcFile);
    char szFileDst[RTPATH_MAX] = { 0 };

    try
    {
        Utf8Str const     strFileSrc(pszSrcFile);
        MachineConfigFile fileSrc(&strFileSrc);
        RTTestIPrintf(RTTESTLVL_ALWAYS, "Successfully read in the file. Version: %u\n", fileSrc.getSettingsVersion());
        RTTESTI_CHECK(fileSrc == fileSrc /* Only have a == operator. */);

        RTTESTI_CHECK_RC_OK_RETV(RTPathTemp(szFileDst, sizeof(szFileDst)));
        RTTESTI_CHECK_RC_OK_RETV(RTPathAppend(szFileDst, sizeof(szFileDst), "tstSettings-XXXXXX.vbox"));
        RTTESTI_CHECK_RC_OK_RETV(RTFileCreateTemp(szFileDst, 0600));

        /* Write source file to a temporary destination file. */
        if (!pszOutput)
        {
            RTTESTI_CHECK_RC_OK_RETV(RTPathTemp(szFileDst, sizeof(szFileDst)));
            RTTESTI_CHECK_RC_OK_RETV(RTPathAppend(szFileDst, sizeof(szFileDst), "tstSettings-XXXXXX.vbox"));
            RTTESTI_CHECK_RC_OK_RETV(RTFileCreateTemp(szFileDst, 0600));
            pszOutput = szFileDst;
        }
        fileSrc.write(szFileDst);
        RTTestIPrintf(RTTESTLVL_DEBUG, "Destination file written to: %s\n", szFileDst);

        try
        {
            /* Load destination file to see if it works. */
            Utf8Str const     strFileDst(pszOutput);
            MachineConfigFile fileDst(&strFileDst);
            RTTestIPrintf(RTTESTLVL_ALWAYS, "Successfully read the file we wrote. Version: %u\n", fileDst.getSettingsVersion());
            RTTESTI_CHECK(fileSrc == fileDst /* Only have a == operator. */);
        }
        catch (const std::exception &err)
        {
            RTTestIFailed("Exception caught when re-reading: %s\n", err.what());
        }
    }
    catch (const std::exception &err)
    {
        RTTestIFailed("Exception caught: %s\n", err.what());
    }

    /* Clean up. */
    if (szFileDst[0])
        RTTESTI_CHECK_RC_OK(RTFileDelete(szFileDst));
}


/**
 * Tests a single VirtualBox.xml configuration file.
 *
 * @param   pszSrcFile  The source XML file (readonly).
 * @param   pszOutput   Optional output file. If NULL, a temporary file is used.
 */
static void tstFileMain(const char *pszSrcFile, const char *pszOutput)
{
    RTTestIPrintf(RTTESTLVL_ALWAYS, "Testing main file: %s\n", pszSrcFile);
    char szFileDst[RTPATH_MAX] = { 0 };

    try
    {
        Utf8Str const  strFileSrc(pszSrcFile);
        MainConfigFile fileSrc(&strFileSrc);
        RTTestIPrintf(RTTESTLVL_ALWAYS, "Successfully read in the file. Version: %u\n", fileSrc.getSettingsVersion());
        //RTTESTI_CHECK(fileSrc == fileSrc /* Only have a == operator. */); - no comparison operator for MainConfigFile

        /* Write source file to a temporary destination file. */
        if (!pszOutput)
        {
            RTTESTI_CHECK_RC_OK_RETV(RTPathTemp(szFileDst, sizeof(szFileDst)));
            RTTESTI_CHECK_RC_OK_RETV(RTPathAppend(szFileDst, sizeof(szFileDst), "tstSettings-XXXXXX.vbox"));
            RTTESTI_CHECK_RC_OK_RETV(RTFileCreateTemp(szFileDst, 0600));
            pszOutput = szFileDst;
        }
        fileSrc.write(pszOutput);
        RTTestIPrintf(RTTESTLVL_ALWAYS, "Successfully wrote it out: %s\n", pszOutput);

        try
        {
            /* Load destination file to see if it works. */
            Utf8Str const  strFileDst(pszOutput);
            MainConfigFile fileDst(&strFileDst);
            RTTestIPrintf(RTTESTLVL_ALWAYS, "Successfully read the file we wrote. Version: %u\n", fileDst.getSettingsVersion());
            //RTTESTI_CHECK(fileSrc == fileDst /* Only have a == operator. */); - no comparison operator for MainConfigFile
        }
        catch (const std::exception &err)
        {
            RTTestIFailed("Exception caught when re-reading: %s\n", err.what());
        }
    }
    catch (const std::exception &err)
    {
        RTTestIFailed("Exception caught: %s\n", err.what());
    }

    /* Clean up. */
    if (szFileDst[0])
        RTTESTI_CHECK_RC_OK(RTFileDelete(szFileDst));
}


int main(int argc, char *argv[])
{
    RTTEST      hTest;
    RTEXITCODE  rcExit = RTTestInitAndCreate("tstSettings", &hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;
    RTTestBanner(hTest);
                                                  //-r162547=2.9.14; r162738=2.12.6; r164446=2.13.2
    /*
     * Process options.
     */
    static const RTGETOPTDEF aOpts[] =
    {
        { "--output",              'o',               RTGETOPT_REQ_STRING },
        { "--machine",             'm',               RTGETOPT_REQ_NOTHING },
        { "--virtualbox-xml",      'M',               RTGETOPT_REQ_NOTHING },
    };

    RTGETOPTSTATE GetState;
    int rc = RTGetOptInit(&GetState, argc, argv, aOpts, RT_ELEMENTS(aOpts), 1 /*idxFirst*/, 0 /*fFlags - must not sort! */);
    AssertRCReturn(rc, RTEXITCODE_INIT);

    unsigned      cFiles      = 0;
    bool          fMachineXml = true;
    const char   *pszOutput   = NULL;

    int           ch;
    RTGETOPTUNION ValueUnion;
    while ((ch = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (ch)
        {
            case 'o':
                pszOutput = ValueUnion.psz;
                if (!*pszOutput)
                    pszOutput = NULL;
                break;

            case 'm':
                fMachineXml = true;
                break;
            case 'M':
                fMachineXml = false;
                break;

            case VINF_GETOPT_NOT_OPTION:
                cFiles++;
                if (fMachineXml)
                    tstFileMachine(ValueUnion.psz, pszOutput);
                else
                    tstFileMain(ValueUnion.psz, pszOutput);
                break;

            default:
                return RTGetOptPrintError(ch, &ValueUnion);
        }
    }

    if (!cFiles)
        return RTTestSkipAndDestroy(hTest, "At least one .vbox machine file must be specified to test!\n");
    return RTTestSummaryAndDestroy(hTest);
}

