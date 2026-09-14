// Minimal CASC extractor CLI for local (static-build) Blizzard storages, e.g. D2R Steam.
//   casc_extract list    <storagePath> <outListFile>
//   casc_extract extract <storagePath> <outDir> <substr1> [substr2 ...]
// Substring filters are matched case-insensitively against the full CASC file name.
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <direct.h>
#include "CascLib.h"

static std::string ToLower(const std::string & s)
{
    std::string r(s);
    for (size_t i = 0; i < r.size(); i++)
        r[i] = (char)tolower((unsigned char)r[i]);
    return r;
}

// "data:data\global\ui\layouts\x.json" -> "global/ui/layouts/x.json"
static std::string ToRelativePath(const std::string & cascName)
{
    std::string s(cascName);
    size_t colon = s.find(':');
    if (colon != std::string::npos)
        s = s.substr(colon + 1);
    for (size_t i = 0; i < s.size(); i++)
        if (s[i] == (char)92) s[i] = '/';
    while (!s.empty() && (s[0] == '/' ))
        s.erase(0, 1);
    // strip a leading "data/" component so the output tree is global/..., local/...
    if (s.size() > 5 && ToLower(s).compare(0, 5, "data/") == 0)
        s = s.substr(5);
    return s;
}

static void MakeDirs(const std::string & path)
{
    for (size_t i = 0; i < path.size(); i++)
    {
        if (path[i] == '/')
            _mkdir(path.substr(0, i).c_str());
    }
}

int main(int argc, char ** argv)
{
    if (argc < 4)
    {
        fprintf(stderr, "usage: casc_extract list    <storagePath> <outListFile>\n"
                        "       casc_extract extract <storagePath> <outDir> <substr> [substr ...]\n");
        return 2;
    }

    bool bList = (strcmp(argv[1], "list") == 0);
    const char * szStorage = argv[2];
    const char * szOut = argv[3];

    std::vector<std::string> filters;
    for (int i = 4; i < argc; i++)
        filters.push_back(ToLower(argv[i]));

    HANDLE hStorage = NULL;
    if (!CascOpenStorage(szStorage, 0, &hStorage))
    {
        fprintf(stderr, "CascOpenStorage failed on '%s', error %u\n", szStorage, (unsigned)GetLastError());
        return 1;
    }

    FILE * fList = NULL;
    if (bList)
    {
        fList = fopen(szOut, "wb");
        if (fList == NULL) { fprintf(stderr, "cannot write %s\n", szOut); return 1; }
    }

    CASC_FIND_DATA fd;
    HANDLE hFind = CascFindFirstFile(hStorage, "*", &fd, NULL);
    if (hFind == NULL)
    {
        fprintf(stderr, "CascFindFirstFile failed, error %u\n", (unsigned)GetLastError());
        return 1;
    }

    unsigned total = 0, matched = 0, extracted = 0, failed = 0;
    std::vector<char> buffer(1 << 20);

    do
    {
        total++;
        std::string name(fd.szFileName);
        if (bList)
        {
            fprintf(fList, "%s\n", name.c_str());
            continue;
        }

        std::string lower = ToLower(name);
        bool hit = false;
        for (size_t i = 0; i < filters.size() && !hit; i++)
            hit = (lower.find(filters[i]) != std::string::npos);
        if (!hit)
            continue;
        matched++;

        HANDLE hFile = NULL;
        if (!CascOpenFile(hStorage, fd.szFileName, 0, CASC_OPEN_BY_NAME | CASC_OVERCOME_ENCRYPTED, &hFile))
        {
            fprintf(stderr, "OPEN FAIL (%u): %s\n", (unsigned)GetLastError(), name.c_str());
            failed++;
            continue;
        }

        std::string outPath = std::string(szOut) + "/" + ToRelativePath(name);
        MakeDirs(outPath);
        FILE * f = fopen(outPath.c_str(), "wb");
        if (f == NULL)
        {
            fprintf(stderr, "CREATE FAIL: %s\n", outPath.c_str());
            CascCloseFile(hFile);
            failed++;
            continue;
        }

        DWORD dwRead = 0;
        bool ok = true;
        for (;;)
        {
            if (!CascReadFile(hFile, buffer.data(), (DWORD)buffer.size(), &dwRead))
            {
                fprintf(stderr, "READ FAIL (%u): %s\n", (unsigned)GetLastError(), name.c_str());
                ok = false;
                break;
            }
            if (dwRead == 0)
                break;
            fwrite(buffer.data(), 1, dwRead, f);
        }
        fclose(f);
        CascCloseFile(hFile);
        if (ok) extracted++; else failed++;
    }
    while (CascFindNextFile(hFind, &fd));

    CascFindClose(hFind);
    if (fList) fclose(fList);
    CascCloseStorage(hStorage);

    fprintf(stderr, "total=%u matched=%u extracted=%u failed=%u\n", total, matched, extracted, failed);
    return failed ? 3 : 0;
}
