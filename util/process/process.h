// Copyright 2021-2022 FLECS Technologies GmbH
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef E969507F_A779_48AF_8F25_F942A12CCA68
#define E969507F_A779_48AF_8F25_F942A12CCA68

#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <list>
#include <string>

namespace FLECS {

class process_t
{
public:
    process_t();

    ~process_t();

    int spawn(const char* path) { return do_spawn(path, false); }

    template <typename... Args>
    int spawn(const char* file, Args&&... args)
    {
        _args = decltype(_args){args...};
        return do_spawn(file, false);
    }

    int spawnp(const char* file) { return do_spawn(file, true); }

    template <typename... Args>
    int spawnp(const char* file, Args&&... args)
    {
        _args = decltype(_args){args...};
        return do_spawn(file, true);
    }

    void arg(std::string arg) { _args.emplace_back(arg); }

    int wait(bool dump_stdout, bool dump_stderr) noexcept;
    void dump_stdout() const noexcept;
    void dump_stderr() const noexcept;
    int exit_code() const noexcept;

    std::string stdout() const noexcept;
    std::string stderr() const noexcept;

private:
    int do_spawn(const char* exec, bool path);

    std::string output(int fd) const noexcept;

    std::list<std::string> _args;
    char _filename_stdout[25];
    char _filename_stderr[25];
    int _fd_stdout;
    int _fd_stderr;
    pid_t _pid;
    int _status;
};

} // namespace FLECS

#endif // E969507F_A779_48AF_8F25_F942A12CCA68
