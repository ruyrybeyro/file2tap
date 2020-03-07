// file2tap 
//
// (C) 2020 Rui Ribeiro

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// header
// 19 0 db 0 db 3 "name" dw size dw mempos dw 0 xor-checsum
void write_header(FILE * fout, int len, char * s, int address)
{
   char h[21] = { 19, 0, // block size  
                  0,     // header
                  3,     // bytes/code
                  ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
                  len % 256,
                  len / 256,
                  address % 256,
                  address / 256,
                  0,
                  0,
                  0  // checksum
                };  
   int i = 0;
   char chksum = 0;
   char * p;
  
   // put filename in header (10 bytes) 
   p=s;
   i=0;

   while(*p)
   {
      h[4+i] = *p; 
      p++;
      i++;
      if (i > 10)
         break;
   }

   // calculate header block checksum
   for ( i = 2 ; i < 20 ; i++ )
      chksum ^= h[i];
   h[20] = chksum;

   // write header to destination file
   for ( i = 0 ; i < 21 ; i++ )
      fputc(h[i], fout);
}

// body
// len+2 db $ff data xor-checksum
void write_data(FILE * fout, int len, FILE * fin)
{
   char chksum = 0xff;
   char c;

   // write block size
   fputc((len+2) % 256, fout);
   fputc((len+2) / 256, fout);

   // data/bytes block type
   fputc(0xff, fout); 
  
   // while src file has data 
   while(len)
   {
      //get char
      c=fgetc(fin);
      // xor it for checksum
      chksum ^= c;
      // write it to destination file
      fputc(c, fout);
      len--;
   }
   // write checksum at the end
   fputc(chksum, fout);
}

char *getExt (const char *fspec) {
    char *e = strrchr (fspec, '.');
    if (e == NULL)
        e = ""; // fast method, could also use &(fspec[strlen(fspec)]).
    return e;
}

void usage()
{
   printf(" Usage:\n");
   printf(" file2tap old_file new.tap [starting_address] [name of block]\n");
   printf("\n");
}

int main (int argc, char **argv)
{
     FILE * fin;
     FILE * fout;
     int len;
     int address = 0xFFFF;

     if ( argc < 3 )
     {
        printf("Insuficient parameters\n");
        usage();
        exit(1);
     }

     fin  = fopen(argv[1], "r");
     fout = fopen(argv[2], "w");

     if ( strcasecmp(getExt(argv[1]),"scr"))
        address = 0x4000;
     if ( strcasecmp(getExt(argv[1]),"rom"))
        address = 0;

     if ( argc > 3 )
     {
        address = atoi(argv[3]);
     }

     if ( address == 0xFFFF )
     {
        printf("Need a starting memory address\n");
        exit(1);
     }

     fseek(fin, 0L, SEEK_END);
     len = ftell(fin);
     rewind(fin);

     if ( argc > 4 )
        write_header(fout, len, argv[4], address);
     else
        write_header(fout, len, argv[1], address);

     write_data(fout, len, fin);

     fclose(fin);
     fclose(fout);

     return (0);
}

