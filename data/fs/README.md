# data/fs directory

This directory is used as storage for file systems loaded from hard drive images.  
File systems can be loaded from here in case they're placed here under the correct file name if they're not embeded in the RDB on a hard drive image.

"Correct file name" would be for instance `DOS.1` for `DOS/1` or `PFS.3` for `PFS/3`.  
Unfortunately this can't yet be used to detect a difference between different revisions of for instance the PFS3AIO file system drivers.
