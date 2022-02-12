#pragma once

#include <charconv>
#include <shellapi.h>
#include <limits>
#include <string>
#include <vector>

namespace waves
{
    class runtime_config
    {
        bool _auto_start{ false };

        int _scene{ 0 };

        

    public:

        runtime_config()
        {
        }

        static std::string wcs2mbs(std::wstring w_string)
        {
            const wchar_t* wcs_ind_string = w_string.c_str();
            auto buffer_size = (w_string.size() + 10) * 8 + 1024;

            std::vector<char> buffer(buffer_size);

            mbstate_t       mbstate;
            ::memset((void*)&mbstate, 0, sizeof(mbstate));

            size_t  converted;
            errno_t err = wcsrtombs_s(&converted, buffer.data(), buffer_size, &wcs_ind_string, buffer_size, &mbstate);

            if (err == EILSEQ)
                return "";
            return std::string(buffer.data(), converted-1);
        }

        const wchar_t* get_usage()
        {
            return L"Usage: \nwaves.exe [--scene <n>] [--auto-start]";
        }

        bool parse_command_line(LPWSTR lpszCmdLine)
        {
            if (wcscmp(lpszCmdLine, L"") == 0)
            {
                return true;
            }

            int argc;
            LPWSTR* argv = CommandLineToArgvW(lpszCmdLine, &argc);

            uint64_t report_every_n_seconds = 1000;
            uint64_t duration = 0; // infinite 

            for (int idx = 0; idx < argc; idx++)
            {
                if (wcscmp(argv[idx], L"--scene") == 0 && (idx + 1) < argc)
                {
                    _scene = std::stoi(std::wstring{ argv[idx + 1] });
                    idx++;
                }
                else if (wcscmp(argv[idx], L"--auto-start") == 0)
                {
                    _auto_start = true;
                }
                else
                {
                    return false;
                }
            }

            return true;
        }

        inline bool auto_star() const noexcept
        {
            return _auto_start;
        }

        inline int scene() const noexcept
        {
            return _scene;
        }
    };

}