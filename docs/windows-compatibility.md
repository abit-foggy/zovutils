# Windows Compatibility & Baseline Reference

Zovutils native Windows support is baselined against Microsoft's official Windows coreutils distribution ([`microsoft/coreutils`](https://github.com/microsoft/coreutils)).

This document describes Zovutils architecture on Windows, path semantics, binary stream handling, terminal processing, shell integration, and command availability.

---

## 1. Architecture & Dispatching

### Multi-Call Dispatcher (`zovutils.exe`)
On Windows, Zovutils distributes as a single multi-call binary: `zovutils.exe`.
Any utility can be executed via:
```cmd
zovutils.exe <utility> [arguments...]
```

### Direct Executable Invocations (`<utility>.exe`)
In addition to the central multicall dispatcher, utilities can be invoked directly via hard links, copies, or `.cmd` wrappers (e.g., `cat.exe`, `ls.exe`, `tail.exe`).
- **Extension Stripping**: When invoked, the dispatcher strips `.exe` and `.EXE` suffixes case-insensitively.
- **Path Separator Independence**: Program name extraction recognizes both forward slash (`/`) and Windows backslash (`\`) separators.

---

## 2. Stream Safety & Windows Runtime Modes

### Binary Stream Preservation (`_O_BINARY`)
On Windows, standard C runtime stdio streams (`stdin`, `stdout`, `stderr`) default to `_O_TEXT` text mode. In text mode:
- Output newline bytes (`\n`, `0x0A`) are silently converted into CRLF (`\r\n`, `0x0D 0x0A`).
- Input streams treat `0x1A` (`Ctrl+Z`) as EOF, truncating binary data streams prematurely.

Zovutils initializes `stdin`, `stdout`, and `stderr` to `_O_BINARY` at process startup via:
```c
_setmode(_fileno(stream), _O_BINARY);
```
This guarantees 100% byte fidelity for binary processing utilities (including `cat`, `cp`, `dd`, `od`, `md5sum`, `sha256sum`, `base64`, `split`, `tr`, `head`, `tail`).

### Windows Console ANSI / Virtual Terminal Processing
Windows 10 (1607+) and Windows 11 console hosts support ANSI / VT100 escape sequences when virtual terminal processing is explicitly enabled.

At initialization, Zovutils activates virtual terminal processing on standard output and error handles:
```c
HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
DWORD dwMode = 0;
if (GetConsoleMode(hOut, &dwMode)) {
    SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}
```
This enables native colorization for `ls --color`, `dircolors`, and formatted diagnostic outputs in Windows Terminal, PowerShell, and CMD.

---

## 3. Path Syntax & Normalization

- **Dual Separator Support**: Accepts both `/` and `\` across all file operations and command arguments.
- **DOS Drive Letters**: Absolute Windows paths with drive letter prefixes (`C:\...`, `D:/...`) are preserved and normalized.
- **Null Device Mapping**: File redirection to null devices automatically resolves to Windows `"NUL"` instead of `/dev/null`.
- **Filesystem Queries**: `df` queries volume capacity using `GetDiskFreeSpaceExA`.

---

## 4. Shell Integration & Built-in Conflicts

Both the Windows Command Prompt (`cmd.exe`) and PowerShell define internal commands and aliases that shadow Unix coreutils names.

### PowerShell Aliases
PowerShell defines aliases for common commands. When typing without `.exe`, PowerShell executes internal cmdlets:

| Command | PowerShell Alias | Native Zovutils Invocation |
|---|---|---|
| `cat` | `Get-Content` | `cat.exe` or `zovutils cat` |
| `ls` | `Get-ChildItem` | `ls.exe` or `zovutils ls` |
| `cp` | `Copy-Item` | `cp.exe` or `zovutils cp` |
| `mv` | `Move-Item` | `mv.exe` or `zovutils mv` |
| `rm` | `Remove-Item` | `rm.exe` or `zovutils rm` |
| `echo` | `Write-Output` | `echo.exe` or `zovutils echo` |
| `tee` | `Tee-Object` | `tee.exe` or `zovutils tee` |
| `kill` | `Stop-Process` | `kill.exe` or `zovutils kill` |
| `sort` | `Sort-Object` | `sort.exe` or `zovutils sort` |

> [!TIP]
> To use Zovutils commands directly by short name in PowerShell, remove the default alias in your PowerShell `$PROFILE`:
> ```powershell
> Remove-Item Alias:ls, Alias:cat, Alias:cp, Alias:mv, Alias:rm, Alias:echo, Alias:sort, Alias:tee -Force -ErrorAction SilentlyContinue
> ```

### Command Prompt (`cmd.exe`) Built-ins
In `cmd.exe`, commands such as `dir`, `echo`, `mkdir`, and `rmdir` are built into `cmd.exe`. To run Zovutils versions, append `.exe` (e.g. `dir.exe`, `echo.exe`).

---

## 5. Utility Status Matrix (Aligned with Microsoft Coreutils)

### Fully Supported Utilities (80+ Tools)
- **Data & Text Processing**: `base32`, `base64`, `basenc`, `cat`, `cksum`, `comm`, `csplit`, `cut`, `expand`, `fmt`, `fold`, `head`, `join`, `nl`, `numfmt`, `od`, `paste`, `pr`, `ptx`, `sort`, `split`, `sum`, `tac`, `tail`, `tee`, `tr`, `tsort`, `unexpand`, `uniq`, `wc`.
- **Cryptographic Hashes**: `b2sum`, `md5sum`, `sha1sum`, `sha224sum`, `sha256sum`, `sha384sum`, `sha512sum`.
- **Files & Directories**: `basename`, `cp`, `dd`, `df`, `dir`, `dircolors`, `dirname`, `du`, `link`, `ln`, `ls`, `mkdir`, `mktemp`, `mv`, `pathchk`, `pwd`, `readlink`, `realpath`, `rm`, `rmdir`, `shred`, `stat`, `touch`, `truncate`, `unlink`, `vdir`.
- **System & Shell**: `date`, `echo`, `env`, `expr`, `factor`, `false`, `nproc`, `printenv`, `printf`, `seq`, `shuf`, `sleep`, `test`, `true`, `uname`, `whoami`, `yes`, `[`.

### POSIX-Specific Fallbacks (Not applicable on Windows)
Aligned with `microsoft/coreutils`, tools relying on POSIX-exclusive concepts provide safe stub handling and diagnostics:
- **Signals & Process Control**: `nice`, `nohup`, `timeout`, `stdbuf` (Windows does not have POSIX process preloading or POSIX signals).
- **POSIX Permissions & Ownership**: `chmod`, `chown`, `chgrp`, `chroot` (Windows uses ACLs rather than Unix UID/GID mode bits).
- **Special Devices**: `mkfifo`, `mknod` (Windows named pipes use `\\.\pipe\` rather than POSIX filesystem FIFO nodes).
- **TTY & Terminals**: `stty`, `tty`.
- **SELinux**: `chcon`, `runcon`.
- **UTMP User Accounting**: `who`, `users`, `pinky`, `logname`, `hostid` (Windows user sessions are managed through Windows Security APIs / `USERNAME`).

---

## 6. Installation & Verification

Build and install on Windows via Zov compiler:
```cmd
zovc build --release
```
To install to `%LOCALAPPDATA%\Programs\zovutils` or add to `%PATH%`, copy `target\zovutils.exe` and create executable symlinks/hardlinks or `.cmd` shims.
