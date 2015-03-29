//original name: plugin_example.c

/* Copyright (c) 2005, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include <stdlib.h>
#include <ctype.h>
#include <mysql/plugin.h>

#if !defined(__attribute__) && (defined(__cplusplus) || !defined(__GNUC__)  || __GNUC__ == 2 && __GNUC_MINOR__ < 8)
#define __attribute__(A)
#endif

#define K 10 // define k-mer length

static long number_of_calls= 0; /* for SHOW STATUS, see below */

/*
  Simple full-text parser plugin that acts as a replacement for the
  built-in full-text parser:
  - All non-whitespace characters are significant and are interpreted as
   "word characters."
  - Whitespace characters are space, tab, CR, LF.
  - There is no minimum word length.  Non-whitespace sequences of one
    character or longer are words.
  - Stopwords are used in non-boolean mode, not used in boolean mode.
*/

/*
  simple_parser interface functions:

  Plugin declaration functions:
  - simple_parser_plugin_init()
  - simple_parser_plugin_deinit()

  Parser descriptor functions:
  - simple_parser_parse()
  - simple_parser_init()
  - simple_parser_deinit()
*/


/*
  Initialize the parser plugin at server start or plugin installation.

  SYNOPSIS
    simple_parser_plugin_init()

  DESCRIPTION
    Does nothing.

  RETURN VALUE
    0                    success
    1                    failure (cannot happen)
*/

static int simple_parser_plugin_init(void *arg __attribute__((unused)))
{
  return(0);
}


/*
  Terminate the parser plugin at server shutdown or plugin deinstallation.

  SYNOPSIS
    simple_parser_plugin_deinit()
    Does nothing.

  RETURN VALUE
    0                    success
    1                    failure (cannot happen)

*/

static int simple_parser_plugin_deinit(void *arg __attribute__((unused)))
{
  return(0);
}


/*
  Initialize the parser on the first use in the query

  SYNOPSIS
    simple_parser_init()

  DESCRIPTION
    Does nothing.

  RETURN VALUE
    0                    success
    1                    failure (cannot happen)
*/

static int simple_parser_init(MYSQL_FTPARSER_PARAM *param
                              __attribute__((unused)))
{
  return(0);
}


/*
  Terminate the parser at the end of the query

  SYNOPSIS
    simple_parser_deinit()

  DESCRIPTION
    Does nothing.

  RETURN VALUE
    0                    success
    1                    failure (cannot happen)
*/

static int simple_parser_deinit(MYSQL_FTPARSER_PARAM *param
                                __attribute__((unused)))
{
  return(0);
}


/*
  Pass a word back to the server.

  SYNOPSIS
    add_word()
      param              parsing context of the plugin
      word               a word
      len                word length

  DESCRIPTION
    Fill in boolean metadata for the word (if parsing in boolean mode)
    and pass the word to the server.  The server adds the word to
    a full-text index when parsing for indexing, or adds the word to
    the list of search terms when parsing a search string.
*/

static void add_word(MYSQL_FTPARSER_PARAM *param, char *word, size_t len)
{
  MYSQL_FTPARSER_BOOLEAN_INFO bool_info=
    { FT_TOKEN_WORD, 0, 0, 0, 0, ' ', 0 };

    //The piece of code inserted below translates each word (k-mer) into a reduce amino acid alphabet
	//before passing it on to mysql_add_word()
	// amino acid groups of the reduced alphabet used by SIMAP:
	// W 		-> W
	// G 		-> G
	// H 		-> H
	// P 		-> P
	// C 		-> C
	// FY 		-> Y
	// AST 		-> A
	// RK 		-> R
	// ILVM 	-> L
	// DENQBZ 	-> D
	// XUOJ* 	-> X

	int i;
	char translation_table[256] = {[0 ... 255] = 88}; // The whole array is initialized to 'X'.
	// All characters other in the original k-mer than capital letters will be replaced by 'X'.
	
	translation_table[65] = 65; // A -> A ('A' in the original k-mer will be replaced by 'A')
	translation_table[66] = 68; // B -> D (...)
	translation_table[67] = 67; // C -> C
	translation_table[68] = 68; // D -> D
	translation_table[69] = 68; // E -> D
	translation_table[70] = 89; // F -> Y
	translation_table[71] = 71; // G -> G
	translation_table[72] = 72; // H -> H
	translation_table[73] = 76; // I -> L
	translation_table[74] = 88; // J -> X
	translation_table[75] = 82; // K -> R
	translation_table[76] = 76; // L -> L
	translation_table[77] = 76; // M -> L
	translation_table[78] = 68; // N -> D
	translation_table[79] = 88; // O -> X
	translation_table[80] = 80; // P -> P
	translation_table[81] = 68; // Q -> D
	translation_table[82] = 82; // R -> R
	translation_table[83] = 65; // S -> A
	translation_table[84] = 65; // T -> A
	translation_table[85] = 88; // U -> X
	translation_table[86] = 76; // V -> L
	translation_table[87] = 87; // W -> W
	translation_table[88] = 88; // X -> X
	translation_table[89] = 89; // Y -> Y
	
	// Each character (actually the corresponding integer) of a word is used as an index in
	// the array 'translation_table'.
	// The character is replaced by the value of 'translation_table' at this index.
	for (i = 0; i < len; i++) { // charcter of word used as index
		word[i] = translation_table[word[i]]; // replacement
	}
	// Inserted code end
	
  param->mysql_add_word(param, word, len, &bool_info);
}

/*
  Parse a document or a search query.

  SYNOPSIS
    simple_parser_parse()
      param              parsing context

  DESCRIPTION
    This is the main plugin function which is called to parse
    a document or a search query. The call mode is set in
    param->mode.  This function simply splits the text into words
    and passes every word to the MySQL full-text indexing engine.
*/

static int simple_parser_parse(MYSQL_FTPARSER_PARAM *param)
{
  char *end, *start, *docend= param->doc + param->length;

  number_of_calls++;
  
// Original parsing: start and end of each word are set to the beginning of doc (text to be parsed).
// Then end is incremented until end is either a whitespace or the end of doc are encountered.
// In either case the word (address of start plus the length of the word (end - start)) is passed to
// the function add_word().

// k-mer parsing: 'start' and 'end' are placed at the beginning of doc (one sequence).
// 'end' is incremented until either the end of the sequence is encountered or 'end' equals (start+K).
// Then the address of 'start* plus the length of the word (in our case always K) is passed on to
// add_word().
// Finally, 'start' is incremented by one and the process is repeated for the next k-mer.


  for (end= start= param->doc;; end++)
  {
    if (end == docend)
    {
      if (end > start) // with the k-mer parsing, this statement could be omitted
        add_word(param, start, end - start);
      break;
    }
    else if ((start + K) == end) // if end is K characters away from start
    {
     	add_word(param, start, end - start); // pass another k-mer to add_word()
    	start++; // start is moved one residue further to mark the start of the next k-mer
    }
  }
  return(0);
}


/*
  Plugin type-specific descriptor
*/

static struct st_mysql_ftparser simple_parser_descriptor=
{
  MYSQL_FTPARSER_INTERFACE_VERSION, /* interface version      */
  simple_parser_parse,              /* parsing function       */
  simple_parser_init,               /* parser init function   */
  simple_parser_deinit              /* parser deinit function */
};

/*
  Plugin status variables for SHOW STATUS
*/

static struct st_mysql_show_var simple_status[]=
{
  {"static",     (char *)"just a static text",     SHOW_CHAR},
  {"called",     (char *)&number_of_calls, SHOW_LONG},
  {0,0,0}
};

/*
  Plugin system variables.
*/

static long     sysvar_one_value;
static char     *sysvar_two_value;

static MYSQL_SYSVAR_LONG(simple_sysvar_one, sysvar_one_value,
  PLUGIN_VAR_RQCMDARG,
  "Simple fulltext parser example system variable number one. Give a number.",
  NULL, NULL, 77L, 7L, 777L, 0);

static MYSQL_SYSVAR_STR(simple_sysvar_two, sysvar_two_value,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_MEMALLOC,
  "Simple fulltext parser example system variable number two. Give a string.",
  NULL, NULL, "simple sysvar two default");

static MYSQL_THDVAR_LONG(simple_thdvar_one,
  PLUGIN_VAR_RQCMDARG,
  "Simple fulltext parser example thread variable number one. Give a number.",
  NULL, NULL, 88L, 8L, 888L, 0);

static MYSQL_THDVAR_STR(simple_thdvar_two,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_MEMALLOC,
  "Simple fulltext parser example thread variable number two. Give a string.",
  NULL, NULL, "simple thdvar two default");

static struct st_mysql_sys_var* simple_system_variables[]= {
  MYSQL_SYSVAR(simple_sysvar_one),
  MYSQL_SYSVAR(simple_sysvar_two),
  MYSQL_SYSVAR(simple_thdvar_one),
  MYSQL_SYSVAR(simple_thdvar_two),
  NULL
};

/*
  Plugin library descriptor
*/

mysql_declare_plugin(ftexample)
{
  MYSQL_FTPARSER_PLUGIN,      /* type                            */
  &simple_parser_descriptor,  /* descriptor                      */
  "simple_parser",            /* name                            */
  "Oracle Corp",              /* author                          */
  "Simple Full-Text Parser",  /* description                     */
  PLUGIN_LICENSE_GPL,
  simple_parser_plugin_init,  /* init function (when loaded)     */
  simple_parser_plugin_deinit,/* deinit function (when unloaded) */
  0x0001,                     /* version                         */
  simple_status,              /* status variables                */
  simple_system_variables,    /* system variables                */
  NULL,
  0,
}
mysql_declare_plugin_end;

