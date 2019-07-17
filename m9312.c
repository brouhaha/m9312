/*
 * m9312 - Decode DEC boot PROMs as used on M9312 Bootstrap/Terminator
 *         module.
 * Copyright 2004, 2005, 2019 Eric Smith <spacewar@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.  Note that permission is
 * not granted to redistribute this program under the terms of any
 * other version of the General Public License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111  USA
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


char *progname;


void usage (void)
{
  fprintf (stderr, "usage:\n");
  fprintf (stderr, "  %s -u hexfile absfile     unscramble PROM hex file into DEC binary\n", progname);
  fprintf (stderr, "  %s -s absfile hexfile     scramble DEC binary file into PROM hex file\n", progname);
  fprintf (stderr, "  %s -d hexfile dumpfile    unscramble PROM hex file into octal dump\n", progname);
  exit (1);
}


bool get_hex_digit (char *buf, uint8_t *digit)
{
  int c = buf [0];
  if ((c >= '0') && (c <= '9'))
    *digit = c - '0';
  else if ((c >= 'a') && (c <= 'f'))
    *digit = 10 + c - 'a';
  else if ((c >= 'A') && (c <= 'F'))
    *digit = 10 + c - 'A';
  else
    return (false);
  return (true);
}

bool get_hex_byte (char *buf, uint8_t *byte)
{
  uint8_t d1;
  uint8_t d2;
  if (! get_hex_digit (buf, & d1))
    return (false);
  if (! get_hex_digit (buf + 1, & d2))
    return (false);
  *byte = (d1 << 4) + d2;
  return (true);
}

bool get_hex_word (char *buf, uint16_t *word)
{
  uint8_t b1;
  uint8_t b2;
  if (! get_hex_byte (buf, & b1))
    return (false);
  if (! get_hex_byte (buf + 2, & b2))
    return (false);
  *word = (b1 << 8) + b2;
  return (true);
}

bool parse_hex_record (char *rbuf, int max_bytes, uint8_t *buf, bool *end)
{
  int len = strlen (rbuf);
  uint8_t byte_count;
  uint16_t base_addr;
  uint8_t record_type;
  uint8_t data_byte;
  uint8_t checksum = 0;
  int i;

  // ignore lines that don't start with a colon
  if (rbuf [0] != ':')
    return (true);
  // at minimum, there must be a colon, two char byte count, four char
  // base address, two char record type, and two char checksum
  if (len < 11)
    {
      fprintf (stderr, "hex record shorter than 11 chars\n");
      return (false);
    }
  if (! get_hex_byte (& rbuf [1], & byte_count))
    {
      fprintf (stderr, "hex record has invalid char in byte count\n");
      return (false);
    }
  if (len < (2 * byte_count + 11))
    {
      fprintf (stderr, "hex record too short for byte count\n");
      return (false);
    }
  // verify checksum
  for (i = 0; i < byte_count + 5; i++)
    {
      if (! get_hex_byte (& rbuf [1 + 2 * i], & data_byte))
	{
	  fprintf (stderr, "hex record has invalid char\n");
	  return (false);
	}
      checksum += data_byte;
    }
  if (checksum != 0)
    {
      fprintf (stderr, "hex record has invalid checksum\n");
      return (false);
    }
  if (! get_hex_word (& rbuf [3], & base_addr))
    {
      fprintf (stderr, "hex record has invalid char in address\n");
      return (false);
    }
  if (! get_hex_byte (& rbuf [7], & record_type))
    {
      fprintf (stderr, "hex record has invalid char in record type\n");
      return (false);
    }
  switch (record_type)
    {
    case 0x00:
      break;          // data record, process normally
    case 0x01:
      *end = true;    // end record, set end flag
      return (true);  
    default:
      return (true);  // ignore unknown record types
    }
  for (i = 0; i < byte_count; i++)
    {
      if (! get_hex_byte (& rbuf [9 + 2 * i], & data_byte))
	{
	  fprintf (stderr, "hex record has invalid char\n");
	  return (false);
	}
      buf [base_addr + i] = data_byte;
    }
  return (true);
}

bool read_hex_file (FILE *inf, int max_bytes, uint8_t *buf)
{
  char rbuf [100];
  bool end = false;

  while ((! end) && fgets (rbuf, sizeof (rbuf), inf))
    {
      if (! parse_hex_record (rbuf, max_bytes, buf, & end))
	return (false);
    }

  return (true);
}


#define HEX_DATA_BYTES_PER_LINE 32

bool write_hex_file (FILE *outf, uint16_t addr, int max_bytes, uint8_t *buf)
{
  while (max_bytes)
    {
      int count = max_bytes;
      uint8_t checksum = 0;

      if (count > HEX_DATA_BYTES_PER_LINE)
	count = HEX_DATA_BYTES_PER_LINE;
      fprintf (outf, ":%02X%04X00", count, addr);
      checksum = count;
      checksum += (addr >> 8);
      checksum += (addr & 0xff);
      addr += count;
      max_bytes -= count;
      while (count--)
	{
	  fprintf (outf, "%02X", *buf);
	  checksum += *(buf++);
	}
      fprintf (outf, "%02X\n", (- checksum) & 0xff);
    }

  fprintf (outf, ":00000001FF\n");  // end record

  return (true);
}


bool read_le16_update_checksum (FILE *inf, uint16_t *val, uint8_t *checksum)
{
  int c;

  c = fgetc (inf);
  if (c < 0)
    return false;
  (*val) = c;
  (*checksum) += c;

  c = fgetc (inf);
  if (c < 0)
    return false;
  (*val) |= (c << 8);
  (*checksum) += c;

  return true;
}


bool read_dec_binary_file (FILE *inf, int max_words, uint16_t *buf)
{
  int c;
  uint16_t i;
  uint16_t byte_count;
  uint16_t addr, base_addr;
  uint8_t checksum;
  bool first_record = true;
  bool end_record = false;

  while (1)
    {
      i = 0;
      while (i != 1)
	{
	  c = fgetc (inf);
	  if (c < 0)
	    return false;
	  i = (i >> 8) | (((uint16_t) c) << 8);
	}
      checksum = 1;

      if (! read_le16_update_checksum (inf, & byte_count, & checksum))
	return false;
      
      if ((byte_count < 6) || (byte_count & 1))
	return (false);

      end_record = (byte_count == 6);
    
      if (! read_le16_update_checksum (inf, & addr, & checksum))
	return false;

      if (first_record)
	{
	  base_addr = addr;
	  first_record = false;
	}

      while (byte_count > 6)
	{
	  uint16_t word;
	  if (! read_le16_update_checksum (inf,
					   & word,
					   & checksum))
	    return false;
	  buf [(addr - base_addr)/2] = word;
	  byte_count -= 2;
	  addr += 2;
	}

      c = fgetc (inf);
      if (c < 0)
	return (false);
      checksum += c;

      if (checksum)
	return (false);

      if (end_record)
	return (true);
    }
}


#define DEC_BINARY_DATA_BYTES_PER_RECORD 32
#define DEC_BINARY_LEADER_NULLS 0
#define DEC_BINARY_INTERRECORD_NULLS 0
#define DEC_BINARY_TRAILER_NULLS 0

void write_nulls (FILE *outf, int count)
{
  while (count--)
    fputc (0, outf);
}


void write_byte_update_checksum (FILE *outf, uint8_t val,  uint8_t *checksum)
{
  (*checksum) += val;
  fputc (val, outf);
}


void write_le16_update_checksum (FILE *outf, uint16_t val, uint8_t *checksum)
{
  write_byte_update_checksum (outf, val & 0xff, checksum);
  write_byte_update_checksum (outf, val >> 8,   checksum);
}


bool write_dec_binary_file (FILE *outf, uint16_t addr, int max_words, uint16_t *buf)
{
  uint8_t checksum;
  uint16_t transfer = addr;  // $$$ is this a good idea? or should we use 0?

  write_nulls (outf, DEC_BINARY_LEADER_NULLS);
  while (max_words)
    {
      int word_count = max_words;
      checksum = 0;

      if (word_count > (DEC_BINARY_DATA_BYTES_PER_RECORD / 2))
	word_count = DEC_BINARY_DATA_BYTES_PER_RECORD / 2;

      write_le16_update_checksum (outf, 1,                  & checksum);
      write_le16_update_checksum (outf, word_count * 2 + 6, & checksum);
      write_le16_update_checksum (outf, addr,               & checksum);

      addr += word_count * 2;
      max_words -= word_count;

      while (word_count--)
	write_le16_update_checksum (outf, *(buf++), & checksum);

      fputc ((- checksum) & 0xff, outf);

      write_nulls (outf, DEC_BINARY_INTERRECORD_NULLS);
    }

  checksum = 0;
  write_le16_update_checksum (outf, 1,                      & checksum);
  write_le16_update_checksum (outf, 6,                      & checksum);
  write_le16_update_checksum (outf, transfer,               & checksum);
  fputc ((- checksum) & 0xff, outf);

  write_nulls (outf, DEC_BINARY_TRAILER_NULLS);
  return (true);
}


void unscramble (uint8_t *buf1, int max_words, uint16_t *buf2)
{
  while (max_words--)
    {
      uint16_t c, p;

      c = (buf1 [3] << 12) + (buf1 [2] << 8) + (buf1 [1] << 4) + buf1 [0];
      buf1 += 4;

      p = (c & 0xfefe) | ((c & 0x0001) << 8) | ((c & 0x0100) >> 8);
      p ^= 0x1c00;

      *(buf2++) = p;
    }
}


void scramble (uint16_t *buf1, int max_words, uint8_t *buf2)
{
  while (max_words--)
    {
      uint16_t p, c;

      p = *(buf1++);
      p ^= 0x1c00;
      c = (p & 0xfefe) | ((p & 0x0001) << 8) | ((p & 0x0100) >> 8);

      *(buf2++) = c & 0x0f;
      *(buf2++) = (c >> 4) & 0x0f;
      *(buf2++) = (c >> 8) & 0x0f;
      *(buf2++) = (c >> 12) & 0x0f;
    }
}


void dump_char (FILE *f, uint8_t c)
{
  if ((c >= ' ') && (c <= '~'))
    fprintf (f, "%c", c);
  else
    fprintf (f, ".");
}

#define WORDS_PER_LINE 8

void dump (FILE *f, uint16_t origin, int count, uint16_t *buf)
{
  int i, j;

  for (i = 0; i < count; i += WORDS_PER_LINE)
    {
      fprintf (f, "%06o:", origin + i);
      for (j = i; j < (i + WORDS_PER_LINE); j++)
	if (j < count)
	  fprintf (f, " %06o", buf [j]);
	else
	  fprintf (f, "       ");
      fprintf (f, " ");
      for (j = i; j < (i + WORDS_PER_LINE); j++)
	if (j < count)
	  {
	    dump_char (f, buf [j] >> 8);
	    dump_char (f, buf [j] & 0xff);
	  }
	else
	  fprintf (f, "  ");
      fprintf (f, "\n");
    }
}


#define MAX_PROM_WORDS 64

uint8_t buf1 [MAX_PROM_WORDS * 8];
// PROM hex file has only 4 bits per byte, and only first half of PROM is
// used

uint16_t buf2 [MAX_PROM_WORDS];


void unscramble_cmd (FILE *inf, FILE *outf)
{
  if (! read_hex_file (inf, sizeof (buf1), buf1))
    {
      fprintf (stderr, "error reading hex file\n");
      exit (2);
    }

  unscramble (buf1, MAX_PROM_WORDS, buf2);

  write_dec_binary_file (outf, 0173000, MAX_PROM_WORDS, buf2);
}


void scramble_cmd (FILE *inf, FILE *outf)
{
  if (! read_dec_binary_file (inf, sizeof (buf2), buf2))
    {
      fprintf (stderr, "error reading abs file\n");
      exit (2);
    }

  scramble (buf2, MAX_PROM_WORDS, buf1);

  write_hex_file (outf, 0, sizeof (buf1), buf1);
}


void dump_cmd (FILE *inf, FILE *outf)
{
  if (! read_hex_file (inf, sizeof (buf1), buf1))
    {
      fprintf (stderr, "error reading hex file\n");
      exit (2);
    }

  unscramble (buf1, MAX_PROM_WORDS, buf2);

  dump (outf, 0173000, MAX_PROM_WORDS, buf2);
}


typedef enum
{
  UNKNOWN,
  UNSCRAMBLE,
  SCRAMBLE,
  DUMP
} cmd_t;


int main (int argc, char *argv[])
{
  cmd_t cmd = UNKNOWN;
  char *infn = NULL;
  char *outfn = NULL;
  char *in_mode;
  char *out_mode;
  FILE *inf;
  FILE *outf;

  progname = argv [0];

  while (--argc)
    {
      argv++;
      if ((argv [0][0] == '-') && argv [0][1])
	{
	  if (! cmd)
	    {
	      if (strcmp (argv [0], "-u") == 0)
		cmd = UNSCRAMBLE;
	      else if (strcmp (argv [0], "-s") == 0)
		cmd = SCRAMBLE;
	      else if (strcmp (argv [0], "-d") == 0)
		cmd = DUMP;
	      else
		usage ();
	    }
	  else
	    usage ();
	}
      else
	{
	  if (! infn)
	    infn = argv [0];
	  else if (! outfn)
	    outfn = argv [0];
	  else
	    usage ();
	}
    }

  if ((! infn) || (! outfn))
    usage ();

  switch (cmd)
    {
    case UNSCRAMBLE:
      in_mode = "r";
      out_mode = "wb";
      break;
    case SCRAMBLE:
      in_mode = "rb";
      out_mode = "w";
      break;
    case DUMP:
      in_mode = "r";
      out_mode = "w";
      break;
    default:
      usage ();
      exit (1);
    }

  if (strcmp (infn, "-") == 0)
    inf = stdin;
  else
    inf = fopen (infn, in_mode);
  if (! inf)
    {
      fprintf (stderr, "error opening input file\n");
      exit (2);
    }

  if (strcmp (outfn, "-") == 0)
    outf = stdout;
  else
    outf = fopen (outfn, out_mode);
  if (! outf)
    {
      fprintf (stderr, "error opening output file\n");
      exit (2);
    }

  switch (cmd)
    {
    case UNSCRAMBLE:
      unscramble_cmd (inf, outf);
      break;
    case SCRAMBLE:
      scramble_cmd (inf, outf);
      break;
    case DUMP:
      dump_cmd (inf, outf);
      break;
    default:
      break;
    }

  fclose (inf);
  fclose (outf);
    
  exit (0);
}
