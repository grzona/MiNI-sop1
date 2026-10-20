#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>

int does_exists(char * path) {
    struct stat path_stat;
    
    if (lstat(path, &path_stat) == 0) {
        return 1;
    }
    
    if (errno == ENOENT) {
        return 0; // Safely confirm it does not exist
    }
    
    // Intercept permission denial specifically
    if (errno == EACCES) {
        // --- Insert your special logic here ---
        // For example, you might want to log the specific file that caused the issue:
        fprintf(stderr, "Warning: Search permission denied for a component of the path prefix: %s\n", path);
        
        // You must still call ERR if you want to end the program as originally required,
        // or return a distinct value (like -1) if your requirements have changed.
        ERR("lstat_permission_denied"); 
    }
    
    // Catch-all for any other errors (e.g., ENAMETOOLONG, ELOOP, ENOTDIR)
    ERR("lstat");
    
    return 0;
}
