#include <curses.h>
#include <stdio.h>
#include "rogue.h"
#include "mach_dep.h"
#if defined(BSD4)
#include <signal.h>
#endif

#if !defined(flushout)
flushout()
{
	int out = 1;

	ioctl(_tty_ch, TIOCFLUSH, (char *)&out);
}
#endif

#if defined(BSD4)

static  struct  ltchars OriginalLtchars;
static  struct  ltchars GameLtchars;

static  struct  tchars  OriginalTchars;
static  struct  tchars  GameTchars;

static  struct  sgttyb  OriginalSgttyb;
static  struct  sgttyb  GameSgttyb;

myinitscr()
{

  /*
   * I am assuming that the game runs off of stdin (ie fd 0)
   */

  if( initscr() == ERR ) {
    return 0;
  }
  if( TtyGetGameChars() && TtyMapGameChars() ) {
    return 1;
  } else {
    (void) TtySetOriginalChars();
    return 0;
  }
}

myendwin()
{

  (void) endwin();
  return TtySetOriginalChars();
}

myraw()
{

  extern struct sgttyb GameSgttyb;

  GameSgttyb.sg_flags |= RAW;
  return raw();
}

mynoraw()
{

  extern struct sgttyb GameSgttyb;

  GameSgttyb.sg_flags &= ~RAW;
  return noraw();
}

mycrmode()
{

  extern struct sgttyb GameSgttyb;

  GameSgttyb.sg_flags |= CBREAK;
  return crmode();
}

mynocrmode()
{

  extern struct sgttyb GameSgttyb;

  GameSgttyb.sg_flags &= ~CBREAK;
  return nocrmode();
}

myecho()
{

  extern	struct  sgttyb  GameSgttyb;

  GameSgttyb.sg_flags |= ECHO;
  return echo();
}

mynoecho()
{

  extern	struct  sgttyb  GameSgttyb;

  GameSgttyb.sg_flags &= ~ECHO;
  return noecho();
}

mynl()
{

  extern	struct  sgttyb  GameSgttyb;

  GameSgttyb.sg_flags |= CRMOD;
  return nl();
}

mynonl()
{

  extern	struct  sgttyb  GameSgttyb;

  GameSgttyb.sg_flags &= ~CRMOD;
  return nonl();
}

mysavetty()
{

  /*
   * If all calls have gone though us,
   * we should not ever need to do anything at this point.
   */

  return savetty();
}

myresetty()
{

  /*
   * If everything has gone well,  we can ignore this,
   * and just pass it on.
   */

  resetty();
  return ;
}

TtyInit()
{

  if(
	!TtyGetOriginalChars() ||
	!TtyGetGameChars() ) {
	  return 0;
	}
  return 1;
}

TtyGetOriginalChars()
{
  extern	struct  ltchars OriginalLtchars;
  extern	struct  tchars  OriginalTchars;
  extern	struct  sgttyb  OriginalSgttyb;

  if(
	ioctl( 0, TIOCGLTC, (char *) &OriginalLtchars) == -1 ||
	ioctl( 0, TIOCGETC, (char *) &OriginalTchars) == -1 ||
	ioctl( 0, TIOCGETP, (char *) &OriginalSgttyb) == -1 ) {
	  return 0;
	}
  return 1;
}

TtySetOriginalChars()
{

  extern	struct  ltchars OriginalLtchars;
  extern	struct  tchars  OriginalTchars;
  extern	struct  sgttyb  OriginalSgttyb;

  if(
	ioctl( 0, TIOCSLTC, (char *) &OriginalLtchars) == -1 ||
	ioctl( 0, TIOCSETC, (char *) &OriginalTchars) == -1 ||
	ioctl( 0, TIOCSETP, (char *) &OriginalSgttyb) == -1 ) {
	  return 0;
	}
  return 1;
}

TtyGetGameChars()
{
  extern	struct  ltchars GameLtchars;
  extern	struct  tchars  GameTchars;
  extern	struct  sgttyb  GameSgttyb;

  if(
	ioctl( 0, TIOCGLTC, (char *) &GameLtchars) == -1 ||
	ioctl( 0, TIOCGETC, (char *) &GameTchars) == -1 ||
	ioctl( 0, TIOCGETP, (char *) &GameSgttyb) == -1 ) {
	  return 0;
	}
  return 1;
}


TtySetGameChars()
{

  extern	struct  ltchars GameLtchars;
  extern	struct  tchars  GameTchars;
  extern	struct  sgttyb  GameSgttyb;

  if(
	ioctl( 0, TIOCSLTC, (char *) &GameLtchars) == -1 ||
	ioctl( 0, TIOCSETC, (char *) &GameTchars) == -1 ||
	ioctl( 0, TIOCSETP, (char *) &GameSgttyb) == -1 ) {
	  return 0;
	}
  return 1;
}


TtyMapGameChars()
{

/*
 * Map all the special chars to non existant ones.
 * Since curses insists on at least CBREAK mode,
 * we can ignore sgttyb.sg_erase and sgttyb.sg_kill
 * (all of the sgttyb struct)
 *
 */

  extern	struct  ltchars GameLtchars;
  extern	struct  tchars  GameTchars;

#if !defined(CTRL)
#define CTRL(x) (('x') & (~'@'))
#endif

  GameLtchars.t_suspc = '&';
  GameLtchars.t_dsuspc = -1;
  GameLtchars.t_rprntc = -1;
  GameLtchars.t_flushc = -1;
  GameLtchars.t_werasc = -1;
  GameLtchars.t_lnextc = -1;

  GameTchars.t_quitc = CTRL(\\);
  GameTchars.t_startc = CTRL(Q);
  GameTchars.t_stopc = CTRL(S);
  GameTchars.t_eofc = -1;
  GameTchars.t_brkc = -1;

  return TtySetGameChars();
}

void
tstop()
{

  /*
   * Since we are running with job control,
   * we can assume that the signals remain set after an interupt.
   */


  int   (*SigCont)();

  /*
   * We are basically commited at this point,
   * so ignore return codes,
   */

  (void) mvcur( 0, COLS-1, LINES-1, 0);
  (void) TtySetOriginalChars();
  /*
   * Give us a fresh line to work with
   */
  putc( '\n', stdout );
  SigCont = signal( SIGCONT, SIG_DFL );
  (void) kill( 0, SIGSTOP );
  /*
   * This is the point that the process will stop at
   */

  /*
   * Lets restart the world.
   */
  (void) signal( SIGCONT, SigCont );
  (void) TtyGetOriginalChars();
  (void) TtySetGameChars();
  (void) wrefresh( curscr );
  return;
}
#endif /* BSD4 */
