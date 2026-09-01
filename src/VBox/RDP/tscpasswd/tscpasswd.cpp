/* $Id: tscpasswd.cpp 111747 2025-11-14 16:43:28Z klaus.espenlaub@oracle.com $ */

/*
 * Copyright (C) 2008-2025 Oracle and/or its affiliates.
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

#include <iprt/win/windows.h>
#include <Wincrypt.h>
#include <locale.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

/** Maximum string length for statically allocated strings. Must be relatively
 * long, as encoded passwords take up quite some string length. */
#define STRMAX 4096

/**
 * Encrypt RDP password so that it's in the suitable format for storing in
 * the RDP connection file. This is actually not genuine encryption, it's
 * making the password unreadable by anyone not knowing the SID of the user.
 *
 * @returns true if successful, false otherwise.
 * @param   pszPassword     The password string in clear text, UTF-8.
 * @param   pszPasswordEnc  The destination for the encryption, UTF-8.
 * @param   cbPasswordEnc   Size of the destination in bytes.
 */
static BOOL tscEncryptRDPPasswd(const char *pszPassword,
                                char *pszPasswordEnc, size_t cbPasswordEnc)
{
    wchar_t swzPassword[256];
    DATA_BLOB DataIn;
    DATA_BLOB DataOut;
    size_t cbPassword, cwLen;
    BOOL ret = true;

    /* NOTE: this code is tuned to produce the same length password entries
     * as mstsc.exe (with a really weird extra byte being always 0 in this
     * implementation, mstsc.exe has varying values there), but strictly
     * speaking this isn't necessary, as mstsc.exe would accept also minimal
     * encrypted buffers. Just cosmetics (and avoids dynamic allocation). */

    /* Convert password from UTF-8 to UCS-2. */
    memset(&swzPassword[0], '\0', sizeof(swzPassword));
    cbPassword = strlen(pszPassword);
    if (cbPassword * sizeof(wchar_t) >= sizeof(swzPassword))
        return false;
    cwLen = mbstowcs(&swzPassword[0], pszPassword, cbPassword);
    if (cwLen == (size_t)-1 || cwLen > cbPassword)
        return false;
    DataIn.cbData = sizeof(swzPassword);
    DataIn.pbData = (BYTE *)&swzPassword[0];

    DataOut.cbData = 0;
    DataOut.pbData = NULL;

    ret = CryptProtectData(&DataIn, L"psw", NULL, NULL, NULL,
                           CRYPTPROTECT_UI_FORBIDDEN, &DataOut);
    if (ret)
    {
        if (DataOut.pbData && cbPasswordEnc > DataOut.cbData * 2 + 1 + 1)
        {
            char *pszEnc = pszPasswordEnc;
            for (const BYTE *p = DataOut.pbData;
                 (size_t)(p - DataOut.pbData) < DataOut.cbData;
                 p++)
            {
                sprintf(pszEnc, "%02hhX", *p);
                pszEnc += 2;
            }
            sprintf(pszEnc, "0");
        }
        else
            ret = false;
    }

    if (DataOut.pbData)
        LocalFree(DataOut.pbData);

    return ret;
}

/**
 * Print usage message.
 */
static void tscUsage(void)
{
    printf("tscpasswd [-h] -u <user> [ -d <domain> ] -p <password> file\n"
           "\n"
           "   -h /? -help           display this help message\n"
           "   -u -user              set username\n"
           "   -d -domain            set domain name\n"
           "   -p -password          set password\n");
}

/**
 * Append UCS-2 string to a limited buffer, moving the buffer pointer.
 *
 * @param   ppwszDest       Pointer to current buffer location.
 * @param   pwcRemaining    Pointer to remaining buffer size.
 * @param   pwszSrc         Pointer to source string.
 * @param   cwSrc           Length of source scring.
 */
static void tscAppendUCS2(wchar_t **ppwszDest, size_t *pcwRemaining,
                          const wchar_t *pwszSrc, size_t cwSrc)
{
    if (cwSrc < *pcwRemaining)
    {
        memcpy(*ppwszDest, pwszSrc, cwSrc * sizeof(wchar_t));
        *ppwszDest += cwSrc;
        *pcwRemaining -= cwSrc;
    }
    else
        *pcwRemaining = 0;
}


int main(int argc, char *argv[])
{
    int ret = 0;
    bool fUsage = false;
    const char *pszUsername = NULL;
    wchar_t swzUsername[STRMAX] = L"";
    const char *pszDomain = NULL;
    wchar_t swzDomain[STRMAX] = L"";
    const char *pszPassword = NULL;
    wchar_t swzPasswordEnc[STRMAX] = L"";
    const char *pszRDPFile = NULL;
    int arg;
    size_t cwUsername = 0;      /* Shut up MSC. */
    size_t cwDomain = 0;        /* ditto */
    size_t cwPasswordEnc = 0;   /* ditto */

    /* This makes really sure the locale settings are applied. This is not all
     * that important for the usual passwords containing just ASCII characters,
     * but in the future someone might feed unusual characters to this tool. */
    setlocale(LC_ALL, "");

    arg = 1;
    while (arg < argc)
    {
        if (argv[arg][0] == '-' || argv[arg][0] == '/')
        {
            const char *optname = &argv[arg][1];

            /* Handle the help option (the only one without argument) before
             * anything else. It suppresses any real activity. */
            if (   !strcmp(optname, "?")
                || !stricmp(optname, "h")
                || !stricmp(optname, "help"))
            {
                fUsage = true;
                break;
            }

            /* This is an option. There must be at least one more arg. */
            if (arg >= argc - 1)
            {
                printf("error: missing argument for option '%s'\n", argv[arg]);
                ret = 1;
                break;
            }
            if (   !stricmp(optname, "u")
                || !stricmp(optname, "user"))
            {
                arg++;
                pszUsername = argv[arg];
                cwUsername = mbstowcs(swzUsername, pszUsername, STRMAX);
                if (cwUsername == (size_t)-1 || cwUsername >= STRMAX)
                {
                    printf("error: cannot convert user name '%s' to UCS-2\n",
                           pszUsername);
                    ret = 1;
                    break;
                }
            }
            else if (   !stricmp(optname, "d")
                     || !stricmp(optname, "domain"))
            {
                arg++;
                pszDomain = argv[arg];
                cwDomain = mbstowcs(swzDomain, pszDomain, STRMAX);
                if (cwDomain == (size_t)-1 || cwDomain >= STRMAX)
                {
                    printf("error: cannot convert domain '%s' to UCS-2\n",
                           pszDomain);
                    ret = 1;
                    break;
                }
            }
            else if (   !stricmp(optname, "p")
                     || !stricmp(optname, "password"))
            {
                char szPasswordEnc[STRMAX];
                arg++;
                pszPassword = argv[arg];
                if (!tscEncryptRDPPasswd(pszPassword, szPasswordEnc, STRMAX))
                {
                    printf("error: cannot encrypt password\n");
                    ret = 1;
                    break;
                }
                cwPasswordEnc = mbstowcs(swzPasswordEnc, szPasswordEnc,
                                         STRMAX);
                if (cwPasswordEnc == (size_t)-1 || cwPasswordEnc >= STRMAX)
                {
                    printf("error: cannot convert encoded password to UCS-2\n");
                    ret = 1;
                    break;
                }
            }
            else
            {
                printf("error: unknown option '%s'\n", argv[arg]);
                ret = 1;
                break;
            }
        }
        else
        {
            if (!pszRDPFile)
                pszRDPFile = argv[arg];
            else
            {
                printf("error: more than one filename specified\n");
                ret = 1;
                break;
            }
        }
        arg++;
    }

    if (fUsage)
        tscUsage();
    else if (!ret)
    {
        do
        {
            if (!pszUsername)
            {
                printf("error: no username specified\n");
                ret = 1;
                break;
            }
            if (!pszPassword)
            {
                printf("error: no password specified\n");
                ret = 1;
                break;
            }
            if (!pszRDPFile)
            {
                printf("error: no file specified\n");
                ret = 1;
                break;
            }

            /* Collect all UCS-2 strings in one array, to simplify writing
             * the settings to the RDP connection file. */
            wchar_t swzSettings[3 * STRMAX];
            wchar_t *pswzSettings = &swzSettings[0];
            size_t cwSettingsRemaining = 3 * STRMAX;

            tscAppendUCS2(&pswzSettings, &cwSettingsRemaining,
                          L"username:s:", 11);
            tscAppendUCS2(&pswzSettings, &cwSettingsRemaining,
                          &swzUsername[0], cwUsername);
            tscAppendUCS2(&pswzSettings, &cwSettingsRemaining,
                          L"\r\n", 2);

            if (pszDomain)
            {
                tscAppendUCS2(&pswzSettings, &cwSettingsRemaining,
                              L"domain:s:", 9);
                tscAppendUCS2(&pswzSettings, &cwSettingsRemaining,
                              &swzDomain[0], cwDomain);
                tscAppendUCS2(&pswzSettings, &cwSettingsRemaining,
                              L"\r\n", 2);
            }

            tscAppendUCS2(&pswzSettings, &cwSettingsRemaining,
                          L"password 51:b:", 14);
            tscAppendUCS2(&pswzSettings, &cwSettingsRemaining,
                          &swzPasswordEnc[0], cwPasswordEnc);
            tscAppendUCS2(&pswzSettings, &cwSettingsRemaining,
                          L"\r\n", 2);

            if (cwSettingsRemaining == 0)
            {
                printf("error: not enough buffer space for settings\n");
                ret = 1;
                break;
            }

            HANDLE hFile = CreateFile(pszRDPFile, GENERIC_READ | GENERIC_WRITE,
                                      0, NULL, OPEN_EXISTING,
                                      FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile != INVALID_HANDLE_VALUE)
            {
                do
                {
                    /* Write the settings to the end of the RDP connection
                     * file. Assume that the username/domain/password settings
                     * are not yet part of the RDP connection file or that
                     * later values override the earlier values. */
                    if (!SetFilePointer(hFile, 0, NULL, FILE_END))
                    {
                        printf("error: cannot set file position in '%s': %#u\n",
                               pszRDPFile, (unsigned)GetLastError());
                        ret = 1;
                        break;
                    }

                    size_t cbWrite =   sizeof(swzSettings)
                                     - cwSettingsRemaining * sizeof(wchar_t);
                    DWORD cbWritten;
                    const BYTE *pbData = (const BYTE *)&swzSettings[0];
                    while (cbWrite)
                    {
                        if (WriteFile(hFile, pbData, (DWORD)cbWrite, &cbWritten, NULL))
                        {
                            if (cbWritten > cbWrite)
                            {
                                printf("error: wrote too much data\n");
                                cbWrite = 0;
                                ret = 1;
                                break;
                            }
                            cbWrite -= cbWritten;
                        }
                        else
                        {
                            printf("error: cannot append settings: %u\n",
                                   (unsigned)GetLastError());
                            cbWrite = 0;
                            ret = 1;
                            break;
                        }
                    }
                    if (cbWrite)
                    {
                        printf("error: cannot write all settings\n");
                        ret = 1;
                        break;
                    }
                } while (0);
                CloseHandle(hFile);
            }
            else
            {
                printf("error: cannot open file '%s': %u\n", pszRDPFile,
                       (unsigned)GetLastError());
                ret = 1;
            }
        } while (0);
    }

    return ret;
}
