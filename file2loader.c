//
// file2loader.c : automates the creation of a BASIC block with embebed machine code
//                 in the A$ variable
//
//                 Rui Ribeiro/2020
//  
// Usage: 
//    file2loader z80_assembled_code dest.tap [TAP_BLOCK_NAME] [address_of_routine] [SP]
//
//    where:
//            z80_assembled_code  is a machine code file
//            dest.tap            is the TAP file with the BASIC loader w/ machine code embebed
//            TAP_BLOCK_NAME      name of the basic section see by LOAD "" 
//            address of routine  address to recolocate ASM and jump to (implies TAP_BLOCK_NAME is there)
//            SP                  address of stack pointer to change (implies last two option are used)
//
//            As an example       ./a.out joy joy.tap JOY 32768 
//
//            Beware changing SP might compromise the return to BASIC
//            if that is the intended behaviour
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

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

char data_basic_block[] =
{
   0x00,0x00, // [0] total len (including flag+checksum) --> TAP - has to be changed
   
   0xff,                            // [2] data block

   0x00,0x0a,                       // [3] line 10
   0x24,0x00,                       // [5] len BASIC line

   0xf9,0xc0,                       // [17] RAND USR
   '(',                             // [19]
   0xbe,0xb0,                       // [20] PEEK VAL
   '"','2','3','6','2','7','"','+', // [22]
   0xbe,0xb0,                       // [30] PEEK VAL
   '"','2','3','6','2','8','"','*', // [32]
   0xb0,                            // [40] VAL 
   '"','2','5','6','"','+',         // [41]
   0xb0,                            // [47] VAL
   '"','3','"',')',0x0d,            // [48] 
   0x41, 0xFF, 0xFF                 // [53] A + 16 bit value of LEN of relocate program + M/C

//+ LEN (2 bytes - len of assembly routine) assembly routine
//+ checksum

};

char recolocate_loader[] =
{
   0x60,             // [0]  LD   H,B
   0x69,             // [1]  LD   L,C
   0x11, 0x16, 0x00, // [2]  LD   DE,$0016    --> sizeof(recolocate_loader)
   0x19,             // [5]  ADD  HL,DE 
   0x11, 0xFF, 0xFF, // [6]  LD   DE,$7918    --> has to change - M/C address
   0x01, 0xFF, 0xFF, // [9]  LD   BC,$0217    --> has to change - size of M/C
   0, 0, 0,          // [12] placeholder for LD SP,nn 0x31 00 00
   0xD5,             // [15] PUSH DE
   0xED, 0xB0,       // [16] LDIR
   0xE1,	     // [18] POP  HL
		     // put BC with address of program being called for
                     // compability with some ASM code that expects it
   0x44,             // [19] LD   B,H
   0x4D,             // [20] LD   C,L	      
   0xE9              // [21] JP   (HL)
                     // [22] $16
};

void usage(char * s)
{
   printf(" %s z80_assembled_code dest.tap [TAPE_BLOCK_NAME] [address_of_routine] [SP]\n", s);
}

int main(int argc, char ** argv)
{
   int RAMaddress = -1;
   int size2block = sizeof(data_basic_block) - 3;
   int sizeMachineCode = 0;
   int fullMachineCode = 0;
   char * s;
   FILE * in;
   FILE * out;
   int i;
   unsigned char c;
   unsigned char checksum = 0;

   // not enough arguments
   if (argc < 3)
   {
      usage(argv[0]);
      exit(1);
   }  

   // name of header (optional)
   if ( argc > 3)
      strncpy(header_block+4, argv[3], (strlen(argv[3])>10)?10:strlen(argv[3]));
   else
      strncpy(header_block+4, "LOADER", 6); // LOADER if none

   // RAM to relocate code machine (optional)
   // otherwise it is run directly from BASIC address
   if ( argc >= 5 )
   {
      RAMaddress = atoi(argv[4]);
      size2block += sizeof(recolocate_loader);
      fullMachineCode += sizeof(recolocate_loader);
   }   

   // SP to be changed for (optional)
   if ( argc == 6 )
   {
       int SP = atoi(argv[5]);

       recolocate_loader[12] = 0x31;	   // LD SP,
       recolocate_loader[13] = SP % 256;
       recolocate_loader[14] = SP / 256;
   }

   // open code machine blob for including in BASIC
   if ( (in = fopen(argv[1], "r")) == NULL )
   {
      printf("\n%s: could not open %s\n", argv[0], argv[1] );
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
   data_basic_block[0] = ( size2block +2) % 256;
   data_basic_block[1] = ( size2block +2) / 256;

   // A var lenght has the full machine code
   data_basic_block[sizeof(data_basic_block)-2] = fullMachineCode % 256;
   data_basic_block[sizeof(data_basic_block)-1] = fullMachineCode / 256;

   // RAM to jump to
   // block will be only written to disk if option given
   recolocate_loader[7] = RAMaddress % 256;
   recolocate_loader[8] = RAMaddress / 256;
   
   // size to recolocate
   recolocate_loader[10] = sizeMachineCode % 256;
   recolocate_loader[11] = sizeMachineCode / 256;

   // open tap file for writing
   out = fopen(argv[2], "w");

   // calculate checksum of header block
   for( i=2 ; i < sizeof(header_block)-1 ; i++)
      checksum ^= header_block[i];
   header_block[20] = checksum;

   // write header block to disk
   fwrite(header_block, sizeof(header_block), 1, out);

   // start calculating checksum of 2nd block
   // first BASIC area
   checksum = 0;
   for( i=2 ; i < sizeof(data_basic_block) ; i++)
      checksum ^= data_basic_block[i];

   // write BASIC area
   fwrite(data_basic_block, sizeof(data_basic_block), 1, out);

   // if relocating C/M, write the relocation code
   if ( RAMaddress != -1 )
   {
      for( i=0 ; i < sizeof(recolocate_loader) ; i++)
         checksum ^= recolocate_loader[i];

      fwrite(recolocate_loader, sizeof(recolocate_loader), 1, out);
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

