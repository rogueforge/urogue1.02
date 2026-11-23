/*
 * save and restore routines
 *
 * @(#)save.c	3.9 (Berkeley) 6/16/81
 */

#include "curses.h"
#include "mach_dep.h"
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <signal.h>
#include "rogue.h"
#include "mach_dep.h"

typedef struct stat STAT;

extern char version[], *strerror();
#include <errno.h>

extern char __executable_start[], _end[];
extern char rg_bss[], *const rg_edata, *const rg_ebss;
extern int playit(), rg_restored();
long save_size();

long save_stamp[2];			/* tell one build from another */
char save_win[3][MAXLINES][MAXCOLS];	/* stdscr, mw and cw */
int save_lines, save_cols;		/* and how big they were */

#ifdef __SANITIZE_ADDRESS__
#define NOCHECK	__attribute__((no_sanitize_address))
#define read	rg_read
#else
#define NOCHECK
#endif

STAT sbuf;

int sigdie = -1;

/*
 * save_ok:
 *	A saved game is the game's variables from version to rg_edata
 *	and from rg_bss to rg_ebss.  See that they are where that takes
 *	them to be, by one variable of each kind, and that save_win[]
 *	has room for the screen.
 */
save_ok()
{
    return ((char *) &level >= version && (char *) &level < rg_edata
	&& (char *) &save_lines >= rg_bss && (char *) &save_lines < rg_ebss
	&& LINES <= MAXLINES && COLS <= MAXCOLS);
}

/*
 * save_wins:
 *	Copy what is in the windows that hold the level to save_win[],
 *	or back from it into the windows
 */
save_wins(back)
bool back;
{
    register int i, y, x;
    register WINDOW *win;

    for (i = 0; i < 3; i++)
    {
	win = (i == 0 ? stdscr : i == 1 ? mw : cw);
	for (y = 0; y < LINES; y++)
	    for (x = 0; x < COLS; x++)
		if (back)
		    mvwaddch(win, y, x, (unsigned char) save_win[i][y][x]);
		else
		    save_win[i][y][x] = mvwinch(win, y, x) & A_CHARTEXT;
    }
}

save_game()
{
    register FILE *savef;
    register int c;
    char buf[LINELEN], oname[LINELEN];

    if (!save_ok())
    {
	msg("This rogue cannot save a game");
	return FALSE;
    }
    /*
     * get file name
     */
    mpos = 0;
    strcpy(oname, file_name);
    if (file_name[0] != '\0')
    {
	msg("Save file (%s)? ", file_name);
	do
	{
	    c = readchar();
	    if (c == ESCAPE) return(0);
	} while (c != 'n' && c != 'N' && c != 'y' && c != 'Y');
	mpos = 0;
	if (c == 'y' || c == 'Y')
	{
	    msg("File name: %s", file_name);
	    goto gotfile;
	}
    }

    do
    {
	msg("File name: ");
	mpos = 0;
	buf[0] = '\0';
	if (get_str(buf, cw) == QUIT)
	{
	    msg("");
	    strcpy(file_name, oname);
	    return FALSE;
	}
	strcpy(file_name, buf);
gotfile:
	if (stat(file_name, &sbuf) >= 0 && (sbuf.st_mode & S_IFMT) == S_IFREG)
	    unlink(file_name);
	c = open(file_name, O_WRONLY|O_CREAT|O_EXCL, 0400);
	if (c >= 0 && fstat(c, &sbuf) == 0
	    && (sbuf.st_uid != getuid() || (sbuf.st_mode & 022) != 0))
	{
	    close(c);
	    unlink(file_name);
	    c = -1;
	    errno = EPERM;
	}
	if ((savef = c < 0 ? NULL : fdopen(c, "w")) == NULL)
	    msg(strerror(errno));	/* fake perror() */
    } while (savef == NULL);

    /*
     * write out encrpyted file (after a stat)
     * The fwrite is to force allocation of the buffer before the write
     */
    if (save_file(savef))
	return TRUE;
    c = errno;
    unlink(file_name);
    msg(strerror(c));
    strcpy(file_name, oname);
    return FALSE;
}

/*
 * automatically save a file.  This is used if a HUP signal is
 * recieved
 */
auto_save(sig)
int sig;
{
    register FILE *savef;
    FILE *fopen(),*ferrsig;
    register int i;

    /*
     * Code added by Bruce Dautrich on 4/11/84 to
     * increase probability of saving game if two 
     * signals are sent to game. Requires define
     * in rogue.h of a variable called sigdie
     */

    if (sigdie == -1) {
	/*This is first signal*/
	sigdie=sig;
	signal(sig,SIG_IGN);
	goto got1sig;
    } else {
	/*This is second signal*/
	signal(sig,SIG_IGN);
    }

got1sig:
    if (pstats.s_hpt <= 0)
	file_name[0] = '\0';
    if (!save_ok())
	exit(1);
    if (stat(file_name, &sbuf) >= 0 && (sbuf.st_mode & S_IFMT) == S_IFREG)
	unlink(file_name);
    if (file_name[0] != '\0'
	&& (i = open(file_name, O_WRONLY|O_CREAT|O_EXCL, 0400)) >= 0
	&& (savef = fdopen(i, "w")) != NULL
	&& !save_file(savef))
	    unlink(file_name);
#if defined(BSD4)
    (void) TtySetOriginalChars();
#endif
    exit(1);
}

/*
 * write the saved game on the file
 */
save_file(savef)
register FILE *savef;
{
    register int i;

    wmove(cw, LINES-1, 0);
    draw(cw);
    fstat(fileno(savef), &sbuf);
    fwrite("junk", 1, 5, savef);
    fseek(savef, 0L, 0);
    save_lines = LINES;
    save_cols = COLS;
    save_wins(FALSE);
    save_stamp[0] = (long) playit - (long) version;
    save_stamp[1] = (long) rg_restored - (long) __executable_start;
    encwrite(version, rg_edata - version, savef);
    encwrite(rg_bss, rg_ebss - rg_bss, savef);
    i = ferror(savef);
    if (fclose(savef) == EOF || i)
	return FALSE;
    return TRUE;
}

/*This is only until all have used this rogue version 1.2 */
restore(file, envp)
register char *file;
char **envp;
{
    register int inf;
    extern char **environ;
    char buf[LINELEN], name[LINELEN];
    STAT sbuf2;
    int y, x, owiz;

    makesure();

    /*
     * Reset the effective uid & gid to the real ones.
     */
    seteuid(getuid());
    setegid(getgid());

    if (strcmp(file, "-r") == 0)
	file = strcpy(name, file_name);
    if ((inf = open(file, O_RDONLY|O_NONBLOCK)) < 0
	|| fstat(inf, &sbuf2) < 0)
    {
	perror(file);
	return FALSE;
    }
    if (sbuf2.st_uid != getuid())
    {
	printf("Sorry, saved game is not yours.\n");
	return FALSE;
    }
    if ((sbuf2.st_mode & 022) != 0)
    {
	printf("Sorry, saved game can be written by others.\n");
	return FALSE;
    }

    fflush(stdout);
    buf[0] = '\0';
    if (encread(buf, strlen(version) + 1, inf) != strlen(version) + 1
	|| strcmp(buf, version) != 0)
    {
	buf[strlen(version)] = '\0';
	printf("Save Game Version: %s\nReal Game Version: %s\n", buf, version);
	printf("Sorry, saved game is out of date.\n");
	return FALSE;
    }

    if (sbuf2.st_size != save_size())
    {
	printf("Sorry, saved game is out of date.\n");
	return FALSE;
    }
    fflush(stdout);
    lseek(inf, 0L, 0);
    owiz = wizard;
    if (encread(version, rg_edata - version, inf) != rg_edata - version
	|| strcmp(buf, version) != 0
	|| encread(rg_bss, rg_ebss - rg_bss, inf) != rg_ebss - rg_bss)
    {
	printf("Sorry, saved game is damaged.\n");
	return FALSE;
    }
    if (save_stamp[0] != (long) playit - (long) version
	|| save_stamp[1] != (long) rg_restored - (long) __executable_start)
    {
	printf("Sorry, saved game is out of date.\n");
	return FALSE;
    }
    rg_rebase(__executable_start, _end, version, rg_edata, rg_bss, rg_ebss);
    if (!rg_restored())
    {
	printf("Sorry, saved game is damaged.\n");
	return FALSE;
    }
    if ((wizard = owiz) != FALSE)
	waswizard = TRUE;
    /*
     * we do not close the file so that we will have a hold of the
     * inode for as long as possible
     */

    if (!owiz) {
	if (sbuf2.st_ino != sbuf.st_ino) {
	    printf("Sorry, saved game is not in the same file.\n");
	    return FALSE;
	}
	else if (sbuf2.st_ctime - sbuf.st_ctime > 1800) {
	    printf("(%ld): Sorry, file has been touched.\n"
		,(long) (sbuf2.st_ctime - sbuf.st_ctime));
	    return FALSE;
	}
    }
    initscr();
    getmaxyx(stdscr, y, x);
    if (y < save_lines || x < save_cols)
    {
	endwin();
	printf("Sorry, this saved game needs a terminal with %d lines and %d columns.\n", save_lines, save_cols);
	return FALSE;
    }
    LINES = save_lines;
    COLS = save_cols;
    cw = newwin(LINES, COLS, 0, 0);
    mw = newwin(LINES, COLS, 0, 0);
    hw = newwin(LINES, COLS, 0, 0);
    save_wins(TRUE);
    mpos = 0;
    mvwprintw(cw, 0, 0, "%s: %s", file, ctime(&sbuf2.st_mtime));

    /*
     * defeat multiple restarting from the same place
     */
    if (!owiz) {
	if (sbuf2.st_nlink != 1) {
	    endwin();
	    printf("Cannot restore from a linked file\n");
	    return FALSE;
	}
	else if (unlink(file) < 0) {
	    endwin();
	    printf("Cannot unlink file\n");
	    return FALSE;
	}
    }

#if defined(BSD4)
    (void) TtyInit();
    (void) TtyGetGameChars();
    (void) TtyMapGameChars();
#endif

    environ = envp;
    strncpy(file_name, file, LINELEN - 1);
    file_name[LINELEN - 1] = '\0';
    setup();
    clearok(curscr, TRUE);
    touchwin(cw);
    srand48(getpid());
    playit();
    /*NOTREACHED*/
}

/*****************************************************************
 *
 *           encwrite, encread: encoded read/write routines
 *
 * The encwrite/encread routines implement a synchronous stream Vernam
 * cipher using a linear congruential random number generator to
 * simulate a one-time pad.  The random seed is encoded and stored in
 * the file using data diffusion (putword,getword).
 *
 * Usage:
 *	encwrite (start, size, outf);
 *	char *start; int size; FILE *outf;
 *
 *	encread (start, size, infd);
 *	char *start; int size; int infd;
 *
 * HISTORY
 * 03-Mar-85  Michael Mauldin (mlm) at Carnegie-Mellon University
 *	Modified for UltraRogue.  Allowed multiple encwrites to a file by
 *	removing lseek calls and computing checksum while reading.
 *
 * 20-Dec-82  Michael Mauldin (mlm) at Carnegie-Mellon University
 *	Created as a replacement for the encwrite/encread in
 *	Rogue 5.2, which are very easily broken.
 *
 *****************************************************************/

# include <stdio.h>

/* Constants for key generation */
# define OFFSET		667818
# define MODULUS	894871
# define MULT		2399
# define ENCHAR		(((seed= ((seed*MULT+OFFSET)%MODULUS)) >> 10) & 0xff)

/* Constants for checksumming */
# define INITCK		1232531
# define CKMOD		6506347

struct wb { int w1, w2, w3, w4; };

/*****************************************************************
 * Encwrite: write a buffer to a file using data encryption
 *****************************************************************/

NOCHECK
encwrite (start, size, outf)
register char *start;
int size;
register FILE *outf;
{ register int cksum = INITCK, seed, ch;

  srand (time (0) + getpid () + getuid ());	/* Build a random seed */
  seed = (rand () & 0x7fff);
  
  putword (seed, outf);				/* Write the random seed */
  putword (size, outf);				/* Write the file length */

# ifdef DEBUG
  fprintf (stderr, "Encwrite: size %ld, seed %ld.\n", size, seed);
# endif

  /* Now checksum, encrypt, and write out the buffer. */
  while (size--)
  { ch = *start++ & 0xff;			/* Get the next char */
    cksum = ((cksum << 8) + ch) % CKMOD;	/* Checksum clear text */
    putc (ch ^ ENCHAR, outf);			/* Write ciphertext */
  }

  putword (cksum, outf);			/* Write out the checksum */
  fflush (outf);				/* Flush the output */

# ifdef DEBUG
  fprintf (stderr, "Checksum is %ld.\n", cksum);
# endif
}

/*****************************************************************
 * Encread: read a block of encrypted text from a file descriptor
 *****************************************************************/

NOCHECK
encread (start, size, infd)
register char *start;
int size;
int infd;
{ register int cksum = INITCK, seed, length, cnt, ch;
  int stored, getword();

  seed = getword (infd); 
  stored = getword (infd);  

# ifdef DEBUG
  fprintf (stderr, "Encread: size %ld, seed %ld, stored %ld.\n", 
	   size, seed, stored);
# endif
  
  if ((length = read (infd, start, (int) min (size, stored))) > 0)
  { for (cnt = length; cnt--; ) 
    { ch = (*start++ ^= ENCHAR) & 0xff;
      cksum = ((cksum << 8) + ch) % CKMOD;
    }
  
    if ((length == stored) && (getword (infd) != cksum))
    {
# ifdef DEBUG
      fprintf (stderr, "Computed checksum %ld is wrong.\n", cksum);
# else
      fprintf (stderr, "Sorry, file has been touched.\n");
# endif
      while (length--) *--start = '\0';		/* Zero the buffer */
    }
  }
  
  return (length);
}

/*****************************************************************
 * putword: Write out an encoded int word
 *****************************************************************/

putword (word, file)
int word;
FILE *file;
{ struct wb w;

  w.w1 = rand ();  
  w.w2 = rand ();  
  w.w3 = rand ();  
  w.w4 = w.w1 ^ w.w2 ^ w.w3 ^ word;
  
  return (fwrite ((char *) &w, sizeof (struct wb), 1, file));
}

/*****************************************************************
 * getword: Read in an encoded int word
 *****************************************************************/

getword (fd)
int fd;
{ struct wb w;
  
  if (read (fd, (char *) &w, sizeof (struct wb)) == sizeof (struct wb))
    return (w.w1 ^ w.w2 ^ w.w3 ^ w.w4);
  else
    return (0);
}

/*
 * save_size:
 *	How long a game saved by this build is: the two pieces of the
 *	game's data, each with the seed, length and checksum encwrite()
 *	puts around it
 */
long
save_size()
{
    return (rg_edata - version) + (rg_ebss - rg_bss) + 6 * sizeof (struct wb);
}
