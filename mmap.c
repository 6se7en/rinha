


#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "mmap.h"

Result mopen(const char* path, u8** dest, size_t* dest_size) {
    
    const int fd = open(path, O_RDONLY);
    if (fd == -1) return errFailedToOpenFile;

    
    struct stat st;
    fstat(fd, &st);

    
    
    
    
    
    
    
    *dest = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0);
    close(fd);

    if (*dest == MAP_FAILED) {
        return errFailedToMemMap;
    }

    
    mlock(*dest, st.st_size);

    *dest_size = st.st_size;

    return OK;
}

void mclose(u8* dest, const size_t size) {
    munlock(dest, size);
    munmap(dest, size);
}
