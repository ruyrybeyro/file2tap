//
// file2loader.c : automates the creation of a BASIC block with embebed machine code
//                 in the A$ variable
//
//                 Rui Ribeiro/2020-2021
//  
//            Example       ./file2loader joy joy.tap -n JOY -x 32768 
//
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>

#define VERSION "2.00"

void usage(char * s)
{
   printf(" Usage:\n%s z80.bin dest.tap [-n TAPE_BLOCK_NAME] [-x address_of_routine] [-s SP] [-p n] [-a n][-V A]\n\n", s);
   printf(" z80.bin\tblock of machine code file\n");
   printf(" dest.tap\tTAP file with the BASIC loader w/ machine code embebed\n");
   printf(" -n NAME\tname of the TAP BASIC section seen by LOAD \"\"\n");
   printf(" -x addr\tdecimal address of code recolocation/execution\n");
   printf(" -s SP\t\tSP to set code before transfer in decimal\n");
   printf(" -p n\t\t128K block page to page in $C000\n");
   printf("\t\t\tn page block 0-7\n");
   printf(" -a n\t\tpost Amstrad special paging modes\n");
   printf("\t\t\tn special paging mode 0-3\n");
   printf(" -V var\t\tvar change BASIC variable in use for storing M/C\n");
   printf("\t\t\tvar A-Z, by default is A (A$)\n");
   printf(" -h\t\t(print help and exit)\n");
   printf(" -v\t\t(print version and exit)\n");
   printf("\n");
   exit(1);
}

int doing_paging_128=0;
int doing_paging_special=0;

char header_block[] =
{
   0x13,0x00, // [0] block len --> TAP

   0x00,      // [2] header block
   0x00,      // [3] BASIC
   ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ', // [4]
   0x2c,0x00, // [14] total lenght of data block (-2 flag and checksum - has to be changed)
   0x0a,0x00, // [16] line 10 autorun
   0x28,0x00, // [18] lenght of BASIC (minus VARS) 

   0x0d       // [20] checksum
};

char data_basic_block_vars[] =
{
   0x00,0x00, // [0] total len (including flag+checksum) --> TAP - has to be changed
   
   0xff,                            // [2] data block

   0x00,0x0a,                       // [3] line 10
   0x24,0x00,                       // [5] len BASIC line

   0xf9,0xc0,                       // [7] RAND USR
   '(',                             // [9]
   0xbe,0xb0,                       // [10] PEEK VAL
   '"','2','3','6','2','7','"','+', // [12]
   0xbe,0xb0,                       // [20] PEEK VAL
   '"','2','3','6','2','8','"','*', // [22]
   0xb0,                            // [30] VAL 
   '"','2','5','6','"','+',         // [31]
   0xb0,                            // [37] VAL
   '"','3','"',')',0x0d,            // [38] 
   0x41, 0xFF, 0xFF                 // [43] A + 16 bit value of LEN of relocate program + M/C

//+ LEN (2 bytes - len of assembly routine) assembly routine
//+ checksum

};

// 128K default page banks : ROM, 5, 2, 0 ($C000 last one)

char paging128[] =
{
// Paging variable from 128K model onwards
// $7ffd - Bits 0-2: RAM page (0-7) to map into memory at 0xc000.

   // RAM page/bank to load at $C000

   0xF3,             //  DI
   0xD9,             //  EXX                    ; save BC
   0x3A, 0x5C, 0x5B, //  LD      A,($5b5c)      ; system copy of $7ffd
   0xE6, 0xF8,       //  AND     $f8            ; reset bit 0-2
   0xF6, 0x03,       //  [7][8] OR      3              ; Select bank 3 (non contended)
   0x01, 0xFD, 0x7F, //  LD      BC,$7ffd
   0x32, 0x5C, 0x5B, //  LD      ($5b5c),A      ; save copy of the new value
   0xED, 0x79,       //  OUT     (C),A          ; change bank 0 for bank 3 at $C000
   0xD9              //  EXX                    ; restore BC
};

char recolocate_loader[] =
{
   0x60,             // [0]  LD   H,B
   0x69,             // [1]  LD   L,C
   0x11, 0x16, 0x00, // [2]  LD   DE,$0016    --> it will be changed
   0x19,             // [5]  ADD  HL,DE 
   0x11, 0xFF, 0xFF, // [6]  LD   DE,$7918    --> has to change - M/C address
   0x01, 0xFF, 0xFF, // [9]  LD   BC,$0217    --> has to change - size of M/C
   0, 0, 0,          // [12] placeholder for LD SP,nn 0x31 00 00
   0xED, 0xB0        // [16] LDIR
};

// from +2A on - 64KB of RAM
//
// bit 0 = 1 (special
// bits 2-1
// 0 - banks 0,1,2,3
// 1 -       4,5,6,7
// 2 -       4,5,6,3
// 3 -       4,7,6,3

char special_paging[] =
{
   //0x3A, 0x67, 0x5B, //  LD      A,($5b67)       ; system copy of $1ffd
   //0xF6, 0x05,       //  OR      $05
   //0xEE, 0x2A,     //  XOR     $02A            ; Special mode, mode 2
   0x3E, 0x05,       //  LD      A,$05
   0x01, 0xFD, 0x1F, //  LD      BC,$1ffd
   0xF3,             //  DI
   0x32, 0x67, 0x5B, //  LD      ($5b67),A       ; save copy of the new value
   0xED, 0x79        //  OUT     (C),A           ; change for special mode 2
                     //  4->5->6->3
};

char jump_to_asm[] =
{
   0x21, 0x00, 0x00, // [0] LD  HL,

                     // put BC with address of program being called for
                     // compability with some ASM code that expects it
   0x44,             // [3] LD   B,H
   0x4D,             // [4] LD   C,L
   0xE9              // [5] JP   (HL)
};

int main(int argc, char ** argv)
{
   int RAMaddress = -1;
   int size2block = sizeof(data_basic_block_vars) - 3;
   int sizeMachineCode = 0;
   int fullMachineCode = 0;
   int blockname       = 0;
   char * s;
   FILE * in;
   FILE * out;
   int i;
   int c;
   int n;
   unsigned char checksum = 0;
   int aflag = 0;
   int bflag = 0;
   // char *cvalue = NULL;
   int index;
   int SP = -1;

   opterr = 0;

  while ((c = getopt (argc, argv, "vhn:x:s:p:a:")) != -1)
    switch (c)
      {
      case 'p':
        doing_paging_128 = 1;
        n=atoi(optarg);
        if ( (n < 0) || (n > 7) )
           usage(argv[0]);
        paging128[8] = n;
        break;
      case 'a':
        doing_paging_special = 1;
        if ( (n < 0) || (n > 3) )
           usage(argv[0]);
        if ( (n == 0) || (n == 3) )
           printf("warning: special mode %d pages standard BASIC area out of way\n", n);
        special_paging[1] = (n << 1) | 1;
        break;
      case 'n':
        blockname = 0;
        strncpy(header_block+4, optarg, (strlen(optarg)>10)?10:strlen(optarg));
        break;
      case 'x':
        RAMaddress = atoi(optarg);
        break;
      case 's':
       {
       SP = atoi(optarg);

       recolocate_loader[12] = 0x31;       // LD SP,
       recolocate_loader[13] = SP % 256;
       recolocate_loader[14] = SP / 256;
       }
        break;
      case 'V':
            strncpy(data_basic_block_vars, optarg, 1);
        break;
      case 'v':
        printf("file2loader %s\n", VERSION);
        exit(0);
        break;
      case 'h':
      default:
        usage(argv[0]);
      }

      if ( (argc - optind) != 2 )
         usage(argv[0]);

      // BASIC block name loader by default
      if (!blockname)
         strncpy(header_block+4, "LOADER", strlen("LOADER"));

      if ( ( doing_paging_128 || doing_paging_special ) && (SP == -1) )
         printf("warning: paging being used without setting a new SP\n");
      else
         if (SP != -1)
            printf("warning: Beware changing SP compromises returning to BASIC.\n");

   // RAM to relocate code machine (optional)
   // otherwise it is run directly from BASIC address
   if ( RAMaddress!=-1 )
   {
      int size = (doing_paging_128?sizeof(paging128):0) + sizeof(recolocate_loader) + (doing_paging_special?sizeof(special_paging):0) + sizeof(jump_to_asm);


      //RAMaddress = atoi(argv[4]);
      size2block += size;
      fullMachineCode += size;
      recolocate_loader[3] = size % 256; 
      recolocate_loader[4] = size / 256;
   }   

   // open code machine blob for including in BASIC
   if ( (in = fopen(argv[optind], "r")) == NULL )
   {
      printf("\n%s: could not open %s\n", argv[0], argv[optind] );
      exit(1);
   }

   // get size of code machine bytes
   fseek(in,0,SEEK_END);
   sizeMachineCode = ftell(in);

   // full block size - 2
   size2block      += sizeMachineCode;
   // full size of machine code (relocation code added if used)
   fullMachineCode += sizeMachineCode; 

   // header block has to have size of next block
   header_block[14] = size2block % 256;
   header_block[15] = size2block / 256;

   // TAP block has to know full size, flags+checksum
   data_basic_block_vars[0] = ( size2block +2) % 256;
   data_basic_block_vars[1] = ( size2block +2) / 256;

   // A var lenght has the full machine code
   data_basic_block_vars[sizeof(data_basic_block_vars)-2] = fullMachineCode % 256;
   data_basic_block_vars[sizeof(data_basic_block_vars)-1] = fullMachineCode / 256;

   // RAM to jump to
   // block will be only written to disk if option given
   recolocate_loader[7] = jump_to_asm[1] = RAMaddress % 256;
   recolocate_loader[8] = jump_to_asm[2] = RAMaddress / 256;
   
   // size to recolocate
   recolocate_loader[10] = sizeMachineCode % 256;
   recolocate_loader[11] = sizeMachineCode / 256;

   // open tap file for writing
   out = fopen(argv[optind+1], "w");

   // calculate checksum of header block
   for( i=2 ; i < sizeof(header_block)-1 ; i++)
      checksum ^= header_block[i];
   header_block[20] = checksum;

   // write header block to disk
   fwrite(header_block, sizeof(header_block), 1, out);

   // start calculating checksum of 2nd block
   // first BASIC area
   checksum = 0;
   for( i=2 ; i < sizeof(data_basic_block_vars) ; i++)
      checksum ^= data_basic_block_vars[i];

   // write BASIC area
   fwrite(data_basic_block_vars, sizeof(data_basic_block_vars), 1, out);

   // if relocating C/M, write the relocation code
   if ( RAMaddress != -1 )
   {
      if(doing_paging_128)
      {
         for( i=0 ; i < sizeof(paging128) ; i++)
            checksum ^=  paging128[i];
         fwrite(paging128, sizeof(paging128), 1, out);
      }

      for( i=0 ; i < sizeof(recolocate_loader) ; i++)
         checksum ^= recolocate_loader[i];
      fwrite(recolocate_loader, sizeof(recolocate_loader), 1, out);

      if(doing_paging_special)
      {
         for( i=0 ; i < sizeof(special_paging) ; i++)
            checksum ^=  special_paging[i];
         fwrite(special_paging, sizeof(special_paging), 1, out);
      }
      for( i=0 ; i < sizeof(jump_to_asm) ; i++)
         checksum ^= jump_to_asm[i];
      fwrite(jump_to_asm, sizeof(jump_to_asm), 1, out);
   }

   // go to the begin of the binary blob/machine code
   fseek(in,0,SEEK_SET);

   // read all bytes, writing them to tap
   // and affecting the checksum calculation
   for (i = 0 ; i < sizeMachineCode ; i++)
   { 
      c=fgetc(in);
      fputc(c, out);
      checksum ^= c;
   }
   // end file writing the calculated checksum of the 2nd block
   fputc(checksum, out);
   
   // close binary blob and new TAP
   fclose(in);
   fclose(out);

   // leave
   exit(0);
}

