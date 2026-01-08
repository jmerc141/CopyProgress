import os, sys, shutil, time, pathlib

# 8MB  - for quicker progress bar response, disk copy
# 64MB - better speed, less responsive, network copy
chunk = 1024**2 * 8

# On termnials that support 256 colors
colors = {
"RED":  "\033[38;5;160m",
"BRED": "\033[48;5;52m",

"GRN":  "\033[38;5;34m",
"BGRN": "\033[48;2;0;30;0m",

"BLU":  "\033[38;5;26m",
"BBLU": "\033[48;2;0;0;30m",

"YEL":  "\033[38;5;220m",
"BYEL": "\033[48;5;3m",

"PURP": "\033[38;5;91m",
"BPUR": "\033[48;5;53m",

"GREY": "\033[38;5;230m",
"BGRY": "\033[48;5;235m",

"END": "\033[0m",
"CLEAR": "\033[K"
}

#_BLOCKS = ["░", "▒", "▓", "█"]
#_BLOCKS = [" ", "▏", "▎", "▍", "▌", "▋", "▊", "▉", "█"]
_BLOCKS = [" ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"]
#_BLOCKS = [" ", "╸", "━"]
#_BLOCKS = ["\033[38;5;236m━\033[0m", "\033[38;5;34m━\033[0m"]

_NUM_BLOCKS = len(_BLOCKS) - 1

bar_width = os.get_terminal_size().columns // 4
total_steps = bar_width * _NUM_BLOCKS


def progress_percentage(perc, speed, eta):
    # Bound and pre-convert once
    if perc < 0.0: perc = 0.0
    elif perc > 100.0: perc = 100.0

    # Convert percent directly into total block steps (integer math)
    filled_steps = int(perc / 100.0 * total_steps)
    full_blocks = filled_steps // _NUM_BLOCKS
    remainder = filled_steps % _NUM_BLOCKS

    if remainder > 0 and full_blocks < bar_width:
        bar = f"{_BLOCKS[-1] * full_blocks}{_BLOCKS[remainder]}{_BLOCKS[0] * (bar_width - full_blocks - 1)}"
    else:
        bar = f"{_BLOCKS[-1] * full_blocks}{_BLOCKS[0] * (bar_width - full_blocks)}"

    sys.stdout.write(f"\r{bar} ┋ {colors['YEL']}{perc:5.2f}%{colors['END']} ┋ {colors['RED']}{speed}MB/s{colors['END']} ┋ {colors['PURP']}{eta[0]}m {eta[1]}s{colors['END']}{colors['CLEAR']}")
    sys.stdout.flush()


def copyfileobj(fsrc, fdst, total, length=chunk):
    copied      = 0
    speed       = 0.0
    m           = 0
    s           = 0
    last_update = 0
    last_update2= 0
    last_copied = 0
    start = time.perf_counter()

    while True:
        buf = fsrc.read(length)
        if not buf:
            break
        fdst.write(buf)
        copied += len(buf)
        
        elap_total = time.perf_counter() - start
        if elap_total - last_update >= 0.08:
            bytes_since_update = copied - last_copied
            time_since_update = elap_total - last_update
            speed = bytes_since_update / time_since_update
            #h, m = divmod(m, 60)

            progress_percentage(100*copied/total, f"{(speed / 1048576):6.2f}", (m, s))

            last_copied = copied
            last_update = elap_total
        if elap_total - last_update2 >= 1:
            eta = (total - copied) / speed
            m, s = divmod(int(eta), 60)
            progress_percentage(100*copied/total, f"{(speed / 1048576):6.2f}", (m, s))
            last_update2 = elap_total


    progress_percentage(total, f"{(speed / 1048576):5.2f}", (m, s))
    print(f"\n{round(total / 1048576,3)}MB copied in {round(elap_total,3)}s")


def copyfile(src, dst, *, follow_symlinks=True):
    """Copy data from src to dst.

    If follow_symlinks is not set and src is a symbolic link, a new
    symlink will be created instead of copying the file it points to.
    """
    if shutil._samefile(src, dst):
        raise shutil.SameFileError("{!r} and {!r} are the same file".format(src, dst))

    for fn in [src, dst]:
        try:
            st = os.stat(fn)
        except OSError:
            # File most likely does not exist
            pass
        else:
            # What about other special files? (sockets, devices...)
            if shutil.stat.S_ISFIFO(st.st_mode):
                raise shutil.SpecialFileError("`%s` is a named pipe" % fn)

    if not follow_symlinks and os.path.islink(src):
        os.symlink(os.readlink(src), dst)
    else:
        size = os.stat(src).st_size
        with open(src, "rb") as fsrc:
            with open(dst, "wb") as fdst:
                copyfileobj(fsrc, fdst, total=size)
    return dst


def copy_with_progress(src, dst, *, follow_symlinks=True):
    """
        Wrapper for copyfile
        src: source path as string
        dst: destination path as string
    """
    print(f"Copying {src} -> {dst}")
    copyfile(src, dst, follow_symlinks=follow_symlinks)
    shutil.copymode(src, dst)


def folderCopy(r, s, dst):
    """
        Folder to folder copy
        r: path of source
        s: list of source files
    """
    x=0
    for i in s:
        if os.path.isdir(i):
            os.makedirs(i.replace(r, dst), exist_ok=True)
        else:
            dest = i.replace(r, dst)
            copy_with_progress(s[x], dest)
        x+=1
    # Done copying all files
    print()


def is_network_location_linux(path):
    if not os.path.ismount(path):
        return False

    return False
    # TODO fix getting linux fstype
    try:
        # Use statvfs to get filesystem information
        #st = os.statvfs(path)
        for p in psutil.disk_partitions():
            if p.mountpoint == '/':
                root_type = p.fstype
                continue

            print(p.fstype, root_type)
            if path.startswith(p.mountpoint):
                return p.fstype
            return root_type
        # Check for common network filesystem types
        # This is a simplified check and may not cover all cases
        #if st.f_fstypename in ["nfs", "cifs", "fuse.sshfs"]:
        #    return True
        #else:
        #    return False
    except OSError:
        # Handle cases where statvfs might fail (e.g., inaccessible path)
        return False


if __name__ == "__main__":
    if len(sys.argv) >= 3:
        src = sys.argv[1]
        dst = sys.argv[2]

        if '-net' in sys.argv:
            chunk = 1024**2 * 64

        if '-old' in sys.argv:
            _BLOCKS = ["░", "▒", "▓", "█"]
            _NUM_BLOCKS = len(_BLOCKS) - 1
            total_steps = bar_width * _NUM_BLOCKS
            for c in colors:
                colors[c] = ''
        else:
            # Convert block colors
            c = colors['BLU']
            b = colors['BBLU']
            for i in range(len(_BLOCKS)):
                _BLOCKS[i] = f"{b}{c}{_BLOCKS[i]}{colors['END']}"
            

        if sys.platform == "win32":
            if r"\\" in dst or r"\\" in src:
                chunk = 1024**2 * 64
        elif sys.platform == "linux":
            if is_network_location_linux(dst):
                chunk = 1024**2 * 64

        # Assuming if src is a dir then destination should be a dir
        if os.path.isdir(src):
            try:
                os.makedirs(dst)
            except FileExistsError as e:
                # Already exists, continue
                pass
            srcFiles = dstFiles = []

            for root, dirs, files in os.walk(src):
                for f in files:
                    srcFiles.append(os.path.join(root, f))
                for d in dirs:
                    srcFiles.append(os.path.join(root, d))
            #print(srcFiles, dstFiles)
            
            folderCopy(src, srcFiles, dst)


        # If source is a file
        elif not os.path.isdir(src):
            # And dest is a dir
            if os.path.isdir(dst):
                #newFile = dst + "\\" + src.split("\\")[-1]
                newFile = os.path.join(dst, src.split(os.path.sep)[-1])
                print(newFile)
                if not os.path.exists(newFile):
                    copy_with_progress(src, newFile)
                    #copy_with_progress(src, f"{dst}\\{src.split('\\')[-1]}")
                else:
                    print(f"File {newFile} already exists")
            else:
                # Dest is a file
                copy_with_progress(src, dst)
        else:
            print("Folder to folder, file to folder or file to file")
    else:
        print("Usage: python copyProg.py <source> <destination>")
        print("  Source and destination can be files or directories")