/*
 * This program displays the names of all files in the current directory.
 */

#include <dirent.h> 
#include <stdio.h> 
#include <unistd.h>

int main(void)
{
    char name[50];
    gethostname(name, 50);
    printf("hostname: %s\n", name);
    
    // reference: http://stackoverflow.com/questions/4204666/how-to-list-files-in-a-directory-in-a-c-program
    DIR *directoryHandle;
    struct dirent *directoryStruct;
    
    directoryHandle = opendir(".");
    if (directoryHandle)
    {
        while ((directoryStruct = readdir(directoryHandle)) != NULL)
        {
            if (strcmp(directoryStruct->d_name, ".") != 0 && strcmp(directoryStruct->d_name, "..") != 0) 
            {
                printf("%s\n", directoryStruct->d_name);
            }
        }
        
        closedir(directoryHandle);
    }
    
    return(0);
}