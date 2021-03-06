
/*
 * xa_mpg.h
 *
 * Copyright (C) 1995-1998,1999 by Mark Podlipec.
 * All rights reserved.
 *
 * This software may be freely used, copied and redistributed without
 * fee for non-commerical purposes provided that this copyright
 * notice is preserved intact on all copies.
 *
 * There is no warranty or other guarantee of fitness of this software.
 * It is provided solely "as is". The author disclaims all
 * responsibility and liability with respect to this software's usage
 * or its effect upon hardware or computer systems.
 *
 */


#include "xanim.h"

/*
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose, without fee, and without written agreement is
 * hereby granted, provided that the above copyright notice and the following
 * two paragraphs appear in all copies of this software.
 *
 * IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT
 * OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF
 * CALIFORNIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 */


typedef struct
{
  unsigned int value;       /* value for macroblock_address_increment */
  int num_bits;             /* length of the Huffman code */
} mb_addr_inc_entry;

/* Structure for an entry in the decoding table of macroblock_type */
typedef struct
{
  unsigned int mb_quant;              /* macroblock_quant */
  unsigned int mb_motion_forward;     /* macroblock_motion_forward */
  unsigned int mb_motion_backward;    /* macroblock_motion_backward */
  unsigned int mb_pattern;            /* macroblock_pattern */
  unsigned int mb_intra;              /* macroblock_intra */
  int num_bits;                       /* length of the Huffman code */
} mb_type_entry;

/* Structures for an entry in the decoding table of coded_block_pattern */
typedef struct
{
  unsigned int cbp;            /* coded_block_pattern */
  int num_bits;                /* length of the Huffman code */
} coded_block_pattern_entry;

/* Structure for an entry in the decoding table of motion vectors */
typedef struct
{
  int code;              /* value for motion_horizontal_forward_code,
                          * motion_vertical_forward_code,
                          * motion_horizontal_backward_code, or
                          * motion_vertical_backward_code.
                          */
  int num_bits;          /* length of the Huffman code */
} motion_vectors_entry;

typedef struct
{
  unsigned int value;    /* value of dct_dc_size (luminance or chrominance) */
  int num_bits;          /* length of the Huffman code */
} dct_dc_size_entry;


#define ERROR -1

coded_block_pattern_entry coded_block_pattern[512] = 
{ {(unsigned int)ERROR, 0}, {(unsigned int)ERROR, 0}, {39, 9}, {27, 9}, {59, 9}, {55, 9}, {47, 9}, {31, 9},
    {58, 8}, {58, 8}, {54, 8}, {54, 8}, {46, 8}, {46, 8}, {30, 8}, {30, 8},
    {57, 8}, {57, 8}, {53, 8}, {53, 8}, {45, 8}, {45, 8}, {29, 8}, {29, 8},
    {38, 8}, {38, 8}, {26, 8}, {26, 8}, {37, 8}, {37, 8}, {25, 8}, {25, 8},
    {43, 8}, {43, 8}, {23, 8}, {23, 8}, {51, 8}, {51, 8}, {15, 8}, {15, 8},
    {42, 8}, {42, 8}, {22, 8}, {22, 8}, {50, 8}, {50, 8}, {14, 8}, {14, 8},
    {41, 8}, {41, 8}, {21, 8}, {21, 8}, {49, 8}, {49, 8}, {13, 8}, {13, 8},
    {35, 8}, {35, 8}, {19, 8}, {19, 8}, {11, 8}, {11, 8}, {7, 8}, {7, 8},
    {34, 7}, {34, 7}, {34, 7}, {34, 7}, {18, 7}, {18, 7}, {18, 7}, {18, 7},
    {10, 7}, {10, 7}, {10, 7}, {10, 7}, {6, 7}, {6, 7}, {6, 7}, {6, 7}, 
    {33, 7}, {33, 7}, {33, 7}, {33, 7}, {17, 7}, {17, 7}, {17, 7}, {17, 7}, 
    {9, 7}, {9, 7}, {9, 7}, {9, 7}, {5, 7}, {5, 7}, {5, 7}, {5, 7}, 
    {63, 6}, {63, 6}, {63, 6}, {63, 6}, {63, 6}, {63, 6}, {63, 6}, {63, 6}, 
    {3, 6}, {3, 6}, {3, 6}, {3, 6}, {3, 6}, {3, 6}, {3, 6}, {3, 6}, 
    {36, 6}, {36, 6}, {36, 6}, {36, 6}, {36, 6}, {36, 6}, {36, 6}, {36, 6}, 
    {24, 6}, {24, 6}, {24, 6}, {24, 6}, {24, 6}, {24, 6}, {24, 6}, {24, 6}, 
    {62, 5}, {62, 5}, {62, 5}, {62, 5}, {62, 5}, {62, 5}, {62, 5}, {62, 5},
    {62, 5}, {62, 5}, {62, 5}, {62, 5}, {62, 5}, {62, 5}, {62, 5}, {62, 5},
    {2, 5}, {2, 5}, {2, 5}, {2, 5}, {2, 5}, {2, 5}, {2, 5}, {2, 5}, 
    {2, 5}, {2, 5}, {2, 5}, {2, 5}, {2, 5}, {2, 5}, {2, 5}, {2, 5}, 
    {61, 5}, {61, 5}, {61, 5}, {61, 5}, {61, 5}, {61, 5}, {61, 5}, {61, 5}, 
    {61, 5}, {61, 5}, {61, 5}, {61, 5}, {61, 5}, {61, 5}, {61, 5}, {61, 5}, 
    {1, 5}, {1, 5}, {1, 5}, {1, 5}, {1, 5}, {1, 5}, {1, 5}, {1, 5}, 
    {1, 5}, {1, 5}, {1, 5}, {1, 5}, {1, 5}, {1, 5}, {1, 5}, {1, 5}, 
    {56, 5}, {56, 5}, {56, 5}, {56, 5}, {56, 5}, {56, 5}, {56, 5}, {56, 5}, 
    {56, 5}, {56, 5}, {56, 5}, {56, 5}, {56, 5}, {56, 5}, {56, 5}, {56, 5}, 
    {52, 5}, {52, 5}, {52, 5}, {52, 5}, {52, 5}, {52, 5}, {52, 5}, {52, 5}, 
    {52, 5}, {52, 5}, {52, 5}, {52, 5}, {52, 5}, {52, 5}, {52, 5}, {52, 5}, 
    {44, 5}, {44, 5}, {44, 5}, {44, 5}, {44, 5}, {44, 5}, {44, 5}, {44, 5}, 
    {44, 5}, {44, 5}, {44, 5}, {44, 5}, {44, 5}, {44, 5}, {44, 5}, {44, 5}, 
    {28, 5}, {28, 5}, {28, 5}, {28, 5}, {28, 5}, {28, 5}, {28, 5}, {28, 5}, 
    {28, 5}, {28, 5}, {28, 5}, {28, 5}, {28, 5}, {28, 5}, {28, 5}, {28, 5}, 
    {40, 5}, {40, 5}, {40, 5}, {40, 5}, {40, 5}, {40, 5}, {40, 5}, {40, 5}, 
    {40, 5}, {40, 5}, {40, 5}, {40, 5}, {40, 5}, {40, 5}, {40, 5}, {40, 5}, 
    {20, 5}, {20, 5}, {20, 5}, {20, 5}, {20, 5}, {20, 5}, {20, 5}, {20, 5}, 
    {20, 5}, {20, 5}, {20, 5}, {20, 5}, {20, 5}, {20, 5}, {20, 5}, {20, 5}, 
    {48, 5}, {48, 5}, {48, 5}, {48, 5}, {48, 5}, {48, 5}, {48, 5}, {48, 5}, 
    {48, 5}, {48, 5}, {48, 5}, {48, 5}, {48, 5}, {48, 5}, {48, 5}, {48, 5}, 
    {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, 
    {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, {12, 5}, 
    {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, 
    {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, 
    {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, 
    {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, {32, 4}, 
    {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, 
    {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, 
    {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, 
    {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, {16, 4}, 
    {8, 4}, {8, 4}, {8, 4}, {8, 4}, {8, 4}, {8, 4}, {8, 4}, {8, 4}, 
    {8, 4}, {8, 4}, {8, 4}, {8, 4}, {8, 4}, {8, 4}, {8, 4}, {8, 4}, 
    {8, 4}, {8, 4}, {8, 4}, {8, 4}, {8, 4}, {8, 4}, {8, 4}, {8, 4}, 
    {8, 4}, {8, 4}, {8, 4}, {8, 4}, {8, 4}, {8, 4}, {8, 4}, {8, 4},
    {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4},
    {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, 
    {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, 
    {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4},
    {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, 
    {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, 
    {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, 
    {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, 
    {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, 
    {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, 
    {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, 
    {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}, {60, 3}
};

/* Decoding table for dct_dc_size_luminance */
dct_dc_size_entry dct_dc_size_luminance[128] =
{   {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, 
    {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, 
    {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, 
    {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, 
    {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, 
    {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, 
    {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, 
    {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, 
    {0, 3}, {0, 3}, {0, 3}, {0, 3}, {0, 3}, {0, 3}, {0, 3}, {0, 3}, 
    {0, 3}, {0, 3}, {0, 3}, {0, 3}, {0, 3}, {0, 3}, {0, 3}, {0, 3}, 
    {3, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3}, 
    {3, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3}, 
    {4, 3}, {4, 3}, {4, 3}, {4, 3}, {4, 3}, {4, 3}, {4, 3}, {4, 3}, 
    {4, 3}, {4, 3}, {4, 3}, {4, 3}, {4, 3}, {4, 3}, {4, 3}, {4, 3}, 
    {5, 4}, {5, 4}, {5, 4}, {5, 4}, {5, 4}, {5, 4}, {5, 4}, {5, 4}, 
    {6, 5}, {6, 5}, {6, 5}, {6, 5}, {7, 6}, {7, 6}, {8, 7}, {(unsigned int)ERROR, 0}
};

/* Decoding table for dct_dc_size_chrominance */
dct_dc_size_entry dct_dc_size_chrominance[256] =
{ {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, 
    {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, 
    {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, 
    {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, 
    {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, 
    {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, 
    {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, 
    {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, {0, 2}, 
    {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, 
    {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, 
    {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, 
    {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, 
    {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, 
    {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, 
    {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, 
    {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, {1, 2}, 
    {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, 
    {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, 
    {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, 
    {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, 
    {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, 
    {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, 
    {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, 
    {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, {2, 2}, 
    {3, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3}, 
    {3, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3}, 
    {3, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3}, 
    {3, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3}, {3, 3}, 
    {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, 
    {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, {4, 4}, 
    {5, 5}, {5, 5}, {5, 5}, {5, 5}, {5, 5}, {5, 5}, {5, 5}, {5, 5}, 
    {6, 6}, {6, 6}, {6, 6}, {6, 6}, {7, 7}, {7, 7}, {8, 8}, {(unsigned int)ERROR, 0}
};

/* DCT coeff tables. */

unsigned short int dct_coeff_tbl_0[256] =
{
0xffff, 0xffff, 0xffff, 0xffff, 
0xffff, 0xffff, 0xffff, 0xffff, 
0xffff, 0xffff, 0xffff, 0xffff, 
0xffff, 0xffff, 0xffff, 0xffff, 
0x052f, 0x051f, 0x050f, 0x04ff, 
0x183f, 0x402f, 0x3c2f, 0x382f, 
0x342f, 0x302f, 0x2c2f, 0x7c1f, 
0x781f, 0x741f, 0x701f, 0x6c1f, 
0x028e, 0x028e, 0x027e, 0x027e, 
0x026e, 0x026e, 0x025e, 0x025e, 
0x024e, 0x024e, 0x023e, 0x023e, 
0x022e, 0x022e, 0x021e, 0x021e, 
0x020e, 0x020e, 0x04ee, 0x04ee, 
0x04de, 0x04de, 0x04ce, 0x04ce, 
0x04be, 0x04be, 0x04ae, 0x04ae, 
0x049e, 0x049e, 0x048e, 0x048e, 
0x01fd, 0x01fd, 0x01fd, 0x01fd, 
0x01ed, 0x01ed, 0x01ed, 0x01ed, 
0x01dd, 0x01dd, 0x01dd, 0x01dd, 
0x01cd, 0x01cd, 0x01cd, 0x01cd, 
0x01bd, 0x01bd, 0x01bd, 0x01bd, 
0x01ad, 0x01ad, 0x01ad, 0x01ad, 
0x019d, 0x019d, 0x019d, 0x019d, 
0x018d, 0x018d, 0x018d, 0x018d, 
0x017d, 0x017d, 0x017d, 0x017d, 
0x016d, 0x016d, 0x016d, 0x016d, 
0x015d, 0x015d, 0x015d, 0x015d, 
0x014d, 0x014d, 0x014d, 0x014d, 
0x013d, 0x013d, 0x013d, 0x013d, 
0x012d, 0x012d, 0x012d, 0x012d, 
0x011d, 0x011d, 0x011d, 0x011d, 
0x010d, 0x010d, 0x010d, 0x010d, 
0x282c, 0x282c, 0x282c, 0x282c, 
0x282c, 0x282c, 0x282c, 0x282c, 
0x242c, 0x242c, 0x242c, 0x242c, 
0x242c, 0x242c, 0x242c, 0x242c, 
0x143c, 0x143c, 0x143c, 0x143c, 
0x143c, 0x143c, 0x143c, 0x143c, 
0x0c4c, 0x0c4c, 0x0c4c, 0x0c4c, 
0x0c4c, 0x0c4c, 0x0c4c, 0x0c4c, 
0x085c, 0x085c, 0x085c, 0x085c, 
0x085c, 0x085c, 0x085c, 0x085c, 
0x047c, 0x047c, 0x047c, 0x047c, 
0x047c, 0x047c, 0x047c, 0x047c, 
0x046c, 0x046c, 0x046c, 0x046c, 
0x046c, 0x046c, 0x046c, 0x046c, 
0x00fc, 0x00fc, 0x00fc, 0x00fc, 
0x00fc, 0x00fc, 0x00fc, 0x00fc, 
0x00ec, 0x00ec, 0x00ec, 0x00ec, 
0x00ec, 0x00ec, 0x00ec, 0x00ec, 
0x00dc, 0x00dc, 0x00dc, 0x00dc, 
0x00dc, 0x00dc, 0x00dc, 0x00dc, 
0x00cc, 0x00cc, 0x00cc, 0x00cc, 
0x00cc, 0x00cc, 0x00cc, 0x00cc, 
0x681c, 0x681c, 0x681c, 0x681c, 
0x681c, 0x681c, 0x681c, 0x681c, 
0x641c, 0x641c, 0x641c, 0x641c, 
0x641c, 0x641c, 0x641c, 0x641c, 
0x601c, 0x601c, 0x601c, 0x601c, 
0x601c, 0x601c, 0x601c, 0x601c, 
0x5c1c, 0x5c1c, 0x5c1c, 0x5c1c, 
0x5c1c, 0x5c1c, 0x5c1c, 0x5c1c, 
0x581c, 0x581c, 0x581c, 0x581c, 
0x581c, 0x581c, 0x581c, 0x581c, 
};

unsigned short int dct_coeff_tbl_1[16] = 
{
0x00bb, 0x202b, 0x103b, 0x00ab, 
0x084b, 0x1c2b, 0x541b, 0x501b, 
0x009b, 0x4c1b, 0x481b, 0x045b, 
0x0c3b, 0x008b, 0x182b, 0x441b, 
};

unsigned short int dct_coeff_tbl_2[4] =
{
0x4019, 0x1429, 0x0079, 0x0839, 
};

unsigned short int dct_coeff_tbl_3[4] = 
{
0x0449, 0x3c19, 0x3819, 0x1029, 
};

unsigned short int dct_coeff_next[256] = 
{
0xffff, 0xffff, 0xffff, 0xffff, 
0xf7d5, 0xf7d5, 0xf7d5, 0xf7d5, 
0x0826, 0x0826, 0x2416, 0x2416, 
0x0046, 0x0046, 0x2016, 0x2016, 
0x1c15, 0x1c15, 0x1c15, 0x1c15, 
0x1815, 0x1815, 0x1815, 0x1815, 
0x0425, 0x0425, 0x0425, 0x0425, 
0x1415, 0x1415, 0x1415, 0x1415, 
0x3417, 0x0067, 0x3017, 0x2c17, 
0x0c27, 0x0437, 0x0057, 0x2817, 
0x0034, 0x0034, 0x0034, 0x0034, 
0x0034, 0x0034, 0x0034, 0x0034, 
0x1014, 0x1014, 0x1014, 0x1014, 
0x1014, 0x1014, 0x1014, 0x1014, 
0x0c14, 0x0c14, 0x0c14, 0x0c14, 
0x0c14, 0x0c14, 0x0c14, 0x0c14, 
0x0023, 0x0023, 0x0023, 0x0023, 
0x0023, 0x0023, 0x0023, 0x0023, 
0x0023, 0x0023, 0x0023, 0x0023, 
0x0023, 0x0023, 0x0023, 0x0023, 
0x0813, 0x0813, 0x0813, 0x0813, 
0x0813, 0x0813, 0x0813, 0x0813, 
0x0813, 0x0813, 0x0813, 0x0813, 
0x0813, 0x0813, 0x0813, 0x0813, 
0x0412, 0x0412, 0x0412, 0x0412, 
0x0412, 0x0412, 0x0412, 0x0412, 
0x0412, 0x0412, 0x0412, 0x0412, 
0x0412, 0x0412, 0x0412, 0x0412, 
0x0412, 0x0412, 0x0412, 0x0412, 
0x0412, 0x0412, 0x0412, 0x0412, 
0x0412, 0x0412, 0x0412, 0x0412, 
0x0412, 0x0412, 0x0412, 0x0412, 
0xfbe1, 0xfbe1, 0xfbe1, 0xfbe1, 
0xfbe1, 0xfbe1, 0xfbe1, 0xfbe1, 
0xfbe1, 0xfbe1, 0xfbe1, 0xfbe1, 
0xfbe1, 0xfbe1, 0xfbe1, 0xfbe1, 
0xfbe1, 0xfbe1, 0xfbe1, 0xfbe1, 
0xfbe1, 0xfbe1, 0xfbe1, 0xfbe1, 
0xfbe1, 0xfbe1, 0xfbe1, 0xfbe1, 
0xfbe1, 0xfbe1, 0xfbe1, 0xfbe1, 
0xfbe1, 0xfbe1, 0xfbe1, 0xfbe1, 
0xfbe1, 0xfbe1, 0xfbe1, 0xfbe1, 
0xfbe1, 0xfbe1, 0xfbe1, 0xfbe1, 
0xfbe1, 0xfbe1, 0xfbe1, 0xfbe1, 
0xfbe1, 0xfbe1, 0xfbe1, 0xfbe1, 
0xfbe1, 0xfbe1, 0xfbe1, 0xfbe1, 
0xfbe1, 0xfbe1, 0xfbe1, 0xfbe1, 
0xfbe1, 0xfbe1, 0xfbe1, 0xfbe1, 
0x0011, 0x0011, 0x0011, 0x0011, 
0x0011, 0x0011, 0x0011, 0x0011, 
0x0011, 0x0011, 0x0011, 0x0011, 
0x0011, 0x0011, 0x0011, 0x0011, 
0x0011, 0x0011, 0x0011, 0x0011, 
0x0011, 0x0011, 0x0011, 0x0011, 
0x0011, 0x0011, 0x0011, 0x0011, 
0x0011, 0x0011, 0x0011, 0x0011, 
0x0011, 0x0011, 0x0011, 0x0011, 
0x0011, 0x0011, 0x0011, 0x0011, 
0x0011, 0x0011, 0x0011, 0x0011, 
0x0011, 0x0011, 0x0011, 0x0011, 
0x0011, 0x0011, 0x0011, 0x0011, 
0x0011, 0x0011, 0x0011, 0x0011, 
0x0011, 0x0011, 0x0011, 0x0011, 
0x0011, 0x0011, 0x0011, 0x0011, 
};

unsigned short int dct_coeff_first[256] = 
{
0xffff, 0xffff, 0xffff, 0xffff, 
0xf7d5, 0xf7d5, 0xf7d5, 0xf7d5, 
0x0826, 0x0826, 0x2416, 0x2416, 
0x0046, 0x0046, 0x2016, 0x2016, 
0x1c15, 0x1c15, 0x1c15, 0x1c15, 
0x1815, 0x1815, 0x1815, 0x1815, 
0x0425, 0x0425, 0x0425, 0x0425, 
0x1415, 0x1415, 0x1415, 0x1415, 
0x3417, 0x0067, 0x3017, 0x2c17, 
0x0c27, 0x0437, 0x0057, 0x2817, 
0x0034, 0x0034, 0x0034, 0x0034, 
0x0034, 0x0034, 0x0034, 0x0034, 
0x1014, 0x1014, 0x1014, 0x1014, 
0x1014, 0x1014, 0x1014, 0x1014, 
0x0c14, 0x0c14, 0x0c14, 0x0c14, 
0x0c14, 0x0c14, 0x0c14, 0x0c14, 
0x0023, 0x0023, 0x0023, 0x0023, 
0x0023, 0x0023, 0x0023, 0x0023, 
0x0023, 0x0023, 0x0023, 0x0023, 
0x0023, 0x0023, 0x0023, 0x0023, 
0x0813, 0x0813, 0x0813, 0x0813, 
0x0813, 0x0813, 0x0813, 0x0813, 
0x0813, 0x0813, 0x0813, 0x0813, 
0x0813, 0x0813, 0x0813, 0x0813, 
0x0412, 0x0412, 0x0412, 0x0412, 
0x0412, 0x0412, 0x0412, 0x0412, 
0x0412, 0x0412, 0x0412, 0x0412, 
0x0412, 0x0412, 0x0412, 0x0412, 
0x0412, 0x0412, 0x0412, 0x0412, 
0x0412, 0x0412, 0x0412, 0x0412, 
0x0412, 0x0412, 0x0412, 0x0412, 
0x0412, 0x0412, 0x0412, 0x0412, 
0x0010, 0x0010, 0x0010, 0x0010, 
0x0010, 0x0010, 0x0010, 0x0010, 
0x0010, 0x0010, 0x0010, 0x0010, 
0x0010, 0x0010, 0x0010, 0x0010, 
0x0010, 0x0010, 0x0010, 0x0010, 
0x0010, 0x0010, 0x0010, 0x0010, 
0x0010, 0x0010, 0x0010, 0x0010, 
0x0010, 0x0010, 0x0010, 0x0010, 
0x0010, 0x0010, 0x0010, 0x0010, 
0x0010, 0x0010, 0x0010, 0x0010, 
0x0010, 0x0010, 0x0010, 0x0010, 
0x0010, 0x0010, 0x0010, 0x0010, 
0x0010, 0x0010, 0x0010, 0x0010, 
0x0010, 0x0010, 0x0010, 0x0010, 
0x0010, 0x0010, 0x0010, 0x0010, 
0x0010, 0x0010, 0x0010, 0x0010, 
0x0010, 0x0010, 0x0010, 0x0010, 
0x0010, 0x0010, 0x0010, 0x0010, 
0x0010, 0x0010, 0x0010, 0x0010, 
0x0010, 0x0010, 0x0010, 0x0010, 
0x0010, 0x0010, 0x0010, 0x0010, 
0x0010, 0x0010, 0x0010, 0x0010, 
0x0010, 0x0010, 0x0010, 0x0010, 
0x0010, 0x0010, 0x0010, 0x0010, 
0x0010, 0x0010, 0x0010, 0x0010, 
0x0010, 0x0010, 0x0010, 0x0010, 
0x0010, 0x0010, 0x0010, 0x0010, 
0x0010, 0x0010, 0x0010, 0x0010, 
0x0010, 0x0010, 0x0010, 0x0010, 
0x0010, 0x0010, 0x0010, 0x0010, 
0x0010, 0x0010, 0x0010, 0x0010, 
0x0010, 0x0010, 0x0010, 0x0010, 
};


