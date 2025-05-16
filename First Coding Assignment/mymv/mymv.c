#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#define COUNT 100

int main (int argc, char* argv[])
{
        if(argc > 3)
        {
        int buf[COUNT];
        int fdSource = open(argv[1],O_RDONLY);
        if(fdSource < 0)
        {
                printf("Couldn't open source file!\n");
                exit(-1);
        }
        int fdDest = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC ,0644);
        if( fdDest < 0 )
        {
                printf("Couldn't open Destination file!\n");
                close(fdSource);
                exit(-2);
        }
        int numRead;
        while((numRead = read(fdSource,buf,COUNT)) > 0 )
        {
                if(write(fdDest,buf,numRead) < 0)
                {
                        printf("could not write to destintation!\n");
                        exit(-3);
                }
        }
        close(fdSource);
        close(fdDest);
        unlink(argv[1]);
        }
        else {exit(-4);}
        return 0;
}
