# About

The goal of this fork is to make it possible to extract the hashes of an Apple FileSystem (APFS) which is encrypted with Filevault.
Hashcat can crack these hashes with mode: `-m 18300`

The work was initially done by @kholia (apfs2john) but was no longer maintained.

Finally, a big thanks to @sgan81 for updating `apfs-fuse`in order to make it compatible with latest APFS.



## APFS FUSE Driver for Linux

This project is a read-only FUSE driver for the new Apple File System. Since Apple didn't yet document
the disk format of APFS, this driver should be considered experimental. It may not be able to read all
files, it may return wrong data, or it may simply crash. Use at your own risk. But since it's read-only,
at least the data on your apfs drive should be safe.

Be aware that not all compression methods are supported yet (only the ones I have encountered so far).
Thus, the driver may return compressed files instead of uncompressed ones. Although most of the time it
should just report an error.



## Usage

### Compile the source code (tested on Ubuntu 19.04 and Kali 2019.2)
Prerequisites :
```
sudo apt update
sudo apt install fuse libfuse3-dev bzip2 libbz2-dev cmake g++ git libattr1-dev zlib1g-dev
```
Clone this fork :
```
git clone https://github.com/Banaanhangwagen/apfs2hashcat.git
cd apfs2hashcat
git submodule init
git submodule update
```
The driver uses Apple's lzfse library and includes it as a submodule.

Compile the driver:
```
mkdir build
cd build
cmake ..
ccmake . ## Only needed if you want to change build options; for example: when you want to use 'FUSE2' instead
make
```


### Extract the hash
```
$ sudo ./build/apfs-dump-quick <image.dmg> <log.txt>
```


### Example output
This is an example output (redacted).
```
Info: Found valid GPT partition table on main device. Dumping first APFS partition.
starting LoadKeybag
 all blocks verified
starting LoadKeybag
 all blocks verified
Volume Macintosh HD is encrypted.
starting LoadKeybag
 all blocks verified
starting LoadKeybag
 all blocks verified
Dumping Keybag (recs)

Keys    :    3

Key 0:
UUID    : XXXXXXXX-A79A-4E2F-A7BB-66917C8F4XXX
KEK Wrpd: XXXXXXXXC06036363BB33B56545150371B1C8E4B74571FF19BFEF824E033B090DD301E69408E8XXX
Iterat's: 111759
Salt    : XXXXXXXXC5209B9D0E39A5338F8D6XXX


Key 1:
UUID    : XXXXXXXX-B618-4ED6-BD8D-50F361C27XXX
KEK Wrpd: XXXXXXXXCD3CED469C2BA8BC2288603AD4F4C852F56DA50F0DDE1AA0B0E9B30A6210F57A8C18CXXX
Iterat's: 209067
Salt    : XXXXXXXX95FC819E7A00FE2871A88XXX


Key 2:
Invalid BLOB Header!!!



Formatted hash to use with Hashcat. Check corresponding UUID.
-------------------------------------------------------------
$fvde$2$16$XXXXXXXXC5209B9D0E39A5338F8D6XXX$111759$XXXXXXXXC06036363BB33B56545150371B1C8E4B74571FF19BFEF824E033B090DD301E69408E8XXX



Formatted hash to use with Hashcat. Check corresponding UUID.
-------------------------------------------------------------
$fvde$2$16$XXXXXXXX95FC819E7A00FE2871A88XXX$209067$XXXXXXXXCD3CED469C2BA8BC2288603AD4F4C852F56DA50F0DDE1AA0B0E9B30A6210F57A8C18CXXX


Password doesn't work for any key.
Wrong password!
Volume 1: Preboot
Volume 2: Recovery
Volume 3: VM
```


## UUID
It is possible that multiple hashes are extracted because there are multiple UUID on the system.
Normally, the first one is the right one (the *Local Open Directory User*).

To be sure, you can double-check this by:

### Method 1
Search for `\Preboot\[GUID]\var\db\CryptoUserInfo.plist` and read the mentionend UUID just before the username.

### Method 2
Attach your DMG to a macOS: 
```
hdiutil attach <image.dmg> -nomount
```
If this does not work, try:
```
hdiutil attach <image.dmg> -blocksize 4096 -nomount
```

Select the correct APFS Volume Disk and type:
```
diskutil apfs listcryptousers /dev/diskXsY
Cryptographic users for diskXsY (3 found)
|
+-- XXXXXXXX-A79A-4E2F-A7BB-66917C8F4XXX
|   Type: Local Open Directory User
|
+-- XXXXXXXX-B618-4ED6-BD8D-50F361C27XXX
|   Type: iCloud Recovery User
|   Note: Unlock with iCloud account data + iCloud Recovery External Key data
|
+-- XXXXXXXX-0000-11AA-AA11-00306543EXXX
    Type: iCloud Recovery External Key
    Note: Stores partial credentials for the iCloud Recovery User
```
It's the UUID of *Local Open Directory User* that we want. Cross-check with *apfs-dump-quick* output.
