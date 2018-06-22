/* Utility functions.
 */

#include "util.h"

#include <linux/kernel.h>
#include <linux/stringify.h>

int str_scan_word(const char **buf, char *word)
{
	// NOTE: linux's vsscanf() currently contains a bug where conversion
	// specification 'n' does not take into account length modifiers (such
	// as in "%zn") when assigning to the corresponding pointer, so we must
	// use an int in conjunction with "%n" to store the number of scanned
	// characters
	int scanned;
	int ret = sscanf(*buf, "%" __stringify(WORD_LEN_MAX) "s%n",
	                 word, &scanned);
	*buf += scanned;
	// NOTE: linux's vsscanf() currently contains a bug where conversion
	// specification 's' accepts a buffer with only whitespace in it and
	// parses it as the empty string; we have to check that the parsed word
	// is not empty
	return ret != 1 || word[0] == '\0';
}
