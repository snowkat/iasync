IASYNC(1) - General Commands Manual

# NAME

**iasync** - sync files to iOS app folders

# SYNOPSIS

**iasync**
\[*options*]
*command*
\[*command\_options*]  
**iasync**
*lsdevs*
\[command\_options]  
**iasync**
*lsapps*
\[command\_options]  
**iasync**
*ls*
\[command\_options]  
**iasync**
*sync*
\[command\_options]
*source*
*target*

# DESCRIPTION

**iasync**
syncs files to Documents folders for iOS apps that support file sharing.

## Global Options

The following options largely affect all commands, and should be provided before
the command name.

**-n,** **--name** *name*

**-u,** **--udid** *udid*

> Connect to the device with the provided name or UDID. This can be discovered
> with the
> *lsdevs*
> command if the device is already connected.

**-v,** **--verbose**

> Increases the verbosity. This can be used multiple times. This is mutally exclusive with
> **--quiet**.

**-q,** **--quiet**

> Lowers the verbosity. This is mutually exclusive with
> **--verbose**.

## Commands

**iasync**
**lsdevs**
\[**-n** | **--no-headers**]

> Lists devices connected to the computer as a table. For each connected device,
> this shows the name, whether it's connected over USB or Network (Wi-Fi), and its
> UDID.

> **-n,** **--no-headers**

> > Suppress the table headers.

**iasync**
**lsapps**
\[**-n** | **--no-headers**]

> Lists apps that support file sharing for the connected device.

> **-n,** **--no-headers**

> > Suppress the table headers.

**iasync**
**ls**
\[**-a** | **--all**]
*app\_id*\[*:path*]

> Lists all files in the directory provided by the
> *path*
> argument, relative to the root of the app's Documents folder.

> **-a,** **--all**

> > Display entries with names starting with a dot (\`.').

**iasync**
**sync**
\[**-Dnp**]
*source*
*app\_id*\[*:path*]

> Copies the files from the
> *source*
> directory to the path provided by the
> *app\_id*
> and
> *path*
> arguments.

> **-D,** **--allow-delete**

> > Allow the sync operation to delete remote files to ensure the remote directory
> > tree matches the local tree.

> **-n,** **--dry-run**

> > Explain what would have been done, instead of performing the operations.

> **-p,** **--progress**

> > Print progress information for files being copied.

# EXIT STATUS

**iasync**
exits with either 1 or 2 if an error occurred.

# SEE ALSO

ifuse(1),
idevicepair(1)

# BUGS

**iasync**
is still heavily in development, and bugs are to be expected. While efforts are
made to avoid loss of data, users should ensure their files are backed up before
performing a sync if they have any important data that could be lost.

Bugs can be reported on GitHub at:

> [https://github.com/snowkat/iasync](https://github.com/snowkat/iasync)

or by sending an e-mail to
[snow@datagirl.xyz](mailto:snow@datagirl.xyz).

Linux 6.12.7-gentoo-dist - January 22, 2025
