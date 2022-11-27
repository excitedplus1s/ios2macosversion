#include <string>
#include <sstream>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>


#define FAT_MAGIC	0xcafebabe
#define FAT_CIGAM	0xbebafeca	/* NXSwapLong(FAT_MAGIC) */

typedef struct fat_header {
	uint32_t	magic;		/* FAT_MAGIC or FAT_MAGIC_64 */
	uint32_t	nfat_arch;	/* number of structs that follow */
} fat_header;


#define LC_VERSION_MIN_MACOSX 0x24   /* build for MacOSX min OS version */
#define LC_VERSION_MIN_IPHONEOS 0x25 /* build for iPhoneOS min OS version */

typedef struct load_command {
	uint32_t cmd;		/* type of load command */
	uint32_t cmdsize;	/* total size of command in bytes */
} load_command;


typedef struct version_min_command {
    uint32_t	cmd;		/* LC_VERSION_MIN_MACOSX or
				   LC_VERSION_MIN_IPHONEOS or
				   LC_VERSION_MIN_WATCHOS or
				   LC_VERSION_MIN_TVOS */
    uint32_t	cmdsize;	/* sizeof(struct min_version_command) */
    uint32_t	version;	/* X.Y.Z is encoded in nibbles xxxx.yy.zz */
    uint32_t	sdk;		/* X.Y.Z is encoded in nibbles xxxx.yy.zz */
} version_min_command;


#define MH_MAGIC_64 0xfeedfacf /* the 64-bit mach magic number */

typedef struct mach_header_64 {
	uint32_t	magic;		/* mach magic number identifier */
	int32_t		cputype;	/* cpu specifier */
	int32_t		cpusubtype;	/* machine specifier */
	uint32_t	filetype;	/* type of file */
	uint32_t	ncmds;		/* number of load commands */
	uint32_t	sizeofcmds;	/* the size of all the load commands */
	uint32_t	flags;		/* flags */
	uint32_t	reserved;	/* reserved */
} mach_header_64;

void print_usage() {
    ::std::cout << "ios2macosversion -sdk=sdkversion -minversion=minversion filename" << ::std::endl;
}

uint32_t calc_version(::std::string arg) {
    uint32_t versionnum = 0;
    ::std::istringstream version(arg);
    ::std::string subversion;
    uint8_t index = 0;
    while (::std::getline(version, subversion, '.')) {
        ::std::stringstream sstream;
        sstream << subversion;
        uint32_t _versionnum; 
        sstream >> _versionnum;
        switch(index)
        {
            case 0:
                versionnum = _versionnum<<16;
                break;
            case 1:
                versionnum |= _versionnum<<8;
                break;
            case 2:
                versionnum |= _versionnum;
                break;
            default:
                break;
        }
        index += 1;
    }
    if(0 == versionnum)
    {
        return 657930;
    }
    return versionnum;
}

void replacemachheader(::std::string filename, uint32_t sdk, uint32_t minversion) {
    int filehandle = open(filename.c_str(), O_RDWR, 0);
    if (-1 == filehandle)
    {
        ::std::cout << "Error: Open File "
                    << filename
                    << " Failed"
                    << ::std::endl;
        close(filehandle);
        exit(1);
    }
    struct stat filestat;
    if(-1 == fstat(filehandle, &filestat))
    {
        ::std::cout << "Error: Get File "
                    << filename
                    << " State Failed"
                    << ::std::endl;
        close(filehandle);
        exit(2);
    }
    size_t file_size = filestat.st_size;
    void *maped_file = mmap(NULL, file_size, PROT_READ|PROT_WRITE, MAP_SHARED, filehandle, 0);
    if (MAP_FAILED == maped_file) {
        ::std::cout << "Error: mmap Failed"
                    << ::std::endl;
        close(filehandle);
        exit(3);
    }

    const fat_header *pfat_header = (fat_header *)maped_file;
    if(FAT_MAGIC == pfat_header->magic  || FAT_CIGAM == pfat_header->magic)
    {
        ::std::cout << "Error: FAT Macho is not support."
                    << ::std::endl;
        munmap(maped_file, file_size);
        close(filehandle);
        exit(4);
    }
    
    const mach_header_64 *pmach_header_64 = (mach_header_64 *)maped_file;
    if(pmach_header_64->magic != MH_MAGIC_64)
    {
        ::std::cout << "Error: Magic is Wrong."
                    << ::std::endl;
        munmap(maped_file, file_size);
        close(filehandle);
        exit(5);
    }

    // Skip mach_header_64
    uint8_t *pfile_header = (uint8_t*)maped_file;
    pfile_header += sizeof(mach_header_64);

    // Get ncmds
    uint32_t command_num = pmach_header_64->ncmds;

    // Find and Replace LC_VERSION_MIN_IPHONEOS to LC_VERSION_MIN_MACOSX
    if(command_num > 0)
    {
        // Enter load_command Entry
        load_command *command = (load_command*)pfile_header;
        for(int i = 0; i < command_num; ++i)
        {
            if(LC_VERSION_MIN_IPHONEOS == command->cmd)
            {
                version_min_command version_cmd = *(version_min_command*)pfile_header;
                // Modify LC_VERSION_MIN_IPHONEOS to LC_VERSION_MIN_MACOSX
                version_cmd.cmd = LC_VERSION_MIN_MACOSX;
                version_cmd.version = minversion;
                version_cmd.sdk = sdk;
                version_cmd.cmdsize = sizeof(version_min_command);
                memcpy(pfile_header, &version_cmd, version_cmd.cmdsize);
                break;
            }
            pfile_header += command->cmdsize;
            command = (load_command*)pfile_header;
        }
    }

    // sync modify to file
    msync(maped_file, file_size, MS_SYNC);
    munmap(maped_file, file_size);
    close(filehandle);
}

int main(int argc, const char * argv[]) {
    if (argc != 4)
    {
        print_usage();
        exit(-1);
    }
    uint32_t sdk = calc_version(::std::string(argv[1] + 5));
    uint32_t minversion = calc_version(::std::string(argv[2] + 12));
    ::std::string filename = ::std::string(argv[3]);
    replacemachheader(filename, sdk, minversion);
    return 0;
}