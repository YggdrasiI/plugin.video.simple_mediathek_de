import os
import stat


def binary_setup(addon_path):
    """ Made script and binary executable. (Required in Krypton 17.1)
    """

    arch = os.uname()[4]
    root_path = os.path.join(addon_path, "root")
    script = os.path.join(root_path, "bin", "simple_mediathek")
    executable = os.path.join(root_path, arch, "bin", "simple_mediathek.bin")

#    try:
    if True:
        stat_script = os.stat(script)
        if not stat_script.st_mode & stat.S_IEXEC:
            os.chmod(script, stat_script.st_mode | stat.S_IEXEC)

            if(arch == "armv6l" and
               not os.path.isdir(os.path.join(root_path, arch))):
                # Current unzipping mechasim convert sybolic link into file
                # Restore link to "[...]/root/armv7l/ ...
                if os.path.isfile(os.path.join(root_path, arch)):
                    os.unlink(os.path.join(root_path, arch))

                cwd_back = os.path.realpath(os.curdir)
                os.chdir(root_path)
                os.symlink("armv7l", arch)
                os.chdir(cwd_back)

            stat_executable = os.stat(executable)
            if not stat_executable.st_mode & stat.S_IEXEC:
                os.chmod(executable, stat_executable.st_mode | stat.S_IEXEC)

#    except OSError:
#        pass
