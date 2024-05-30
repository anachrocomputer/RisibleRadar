/* RisibleRadar --- radar game for the Nokia 1202 LCD       2012-05-20 */
/* Copyright (c) 2012 John Honniball */

/* Released under the GNU Public Licence (GPL) */

#include <math.h>
#include <SPI.h>
#include "font.h"

// Direct port I/O defines for Arduino with ATmega328
// Change these if running on Mega Arduino
#define LCDOUT PORTB
#define CS     0x02
#define SDA    0x08
#define SCLK   0x20

// Connections to Nokia 1202 LCD, via CD4050 level-shifter
#define slaveSelectPin 9
#define DACSelectPin 10
#define SDAPin 11
#define SCLKPin 13

// Size of Nokia 1202 LCD screen
#define MAXX 96
#define MAXY 68
#define MAXROWS 9  // 9 rows of bytes, last row only half used

// Co-ord of centre of screen
#define CENX (MAXX / 2)
#define CENY (MAXY / 2)

#define NTARGETS  10    // Number of randomly-placed radar targets on playfield
#define NECHOES 10      // Maximum number of echoes displayed

#define DEFGAMEDURATION 40
#define MAXGAMEDURATION 60

#define TIMERX  (MAXX - 9)
#define TIMERY   4

#define MAXPLAYX (MAXX * 2)
#define MAXPLAYY (MAXY * 2)

#define RADTODEG  57.29578

enum cardinalDirections {
  NORTH = 1,
  SOUTH,
  EAST,
  WEST
};

// The frame buffer, 864 bytes
unsigned char Frame[MAXROWS][MAXX];

// The current echoes
struct echo_t {
  unsigned int x;
  unsigned int y;
  int age;
  int rad;
};

struct echo_t Echo[NECHOES];

// The targets
struct target_t {
  unsigned int x;
  unsigned int y;
  float bearing;
  float range;
  unsigned char siz;
  int active:1;
  int rings:1;
  int axes:1;
  int time:1;
};

struct target_t Target[NTARGETS];

struct coord_t {
  unsigned int x;
  unsigned int y;
};

struct coord_t Player;

int Gather_y = 3;

int GameDuration = DEFGAMEDURATION;

int Sweeps = 0;

// Some attributes to "pick up" which are enhancements to the rather
// crude radar. Other things we could add here would be: longer echo
// persistence; faster sweep.
boolean Rings = false;
boolean Axes = false;


void setup (void)
{
  int i;
  
  Serial.begin (9600);
  
  Serial.println ("Risible Radar");
  Serial.println ("John Honniball, May 2012");
  Serial.println ("Ludum Dare MiniLD #34: Aspect");

  lcd1202_begin ();
  
  clrFrame ();
  
  circle (CENX, CENY, 33, 1, 0);
  
  // Targets scattered on playfield at random
  for (i = 0; i < NTARGETS; i++) {
    do {
      Target[i].x = random (0, MAXPLAYX);
      Target[i].y = random (0, MAXPLAYY);
      
      Target[i].active = true;
      Target[i].rings = false;
      Target[i].axes = false;
      Target[i].time = false;
      Target[i].siz = random (1, 3);
//Serial.print ("siz: ");
//Serial.println ((int)Target[i].siz);      
      // TODO: make sure no two targets are too close together
    } while (0);
  }
  
  // Place the 'bonus' targets somewhere
  i = random (0, NTARGETS - 1);
  Target[i].rings = true;
  
  i = random (0, NTARGETS - 1);
  Target[i].axes = true;
  
  i = random (0, NTARGETS - 1);
  Target[i].time = true;

  i = random (0, NTARGETS - 1);
  Target[i].time = true;
  
  // Start the player in centre of playfield
  Player.x = MAXPLAYX / 2;
  Player.y = MAXPLAYY / 2;
  
  reCalculateBearings ();
  
  drawBackground ();
  
  drawRadarScreen (true, true);
  
  fillRoundRect (CENX - (3 * 13) - 2, CENY - 8, CENX + (3 * 13) + 2, CENY + 12, 7);
  setText (CENX - (3 * 13), CENY, "Risible Radar");
  
  updscreen ();
  
  delay (2000);
  
  drawBackground ();
  
  drawRadarScreen (true, true);
  
  fillRoundRect (CENX - (3 * 5) - 2, CENY - 8, CENX + (3 * 5) + 2, CENY + 12, 7);
  setText (CENX - (3 * 5), CENY, "READY");
  
  updscreen ();
  
  delay (1000);
}


void loop (void)
{
  int r;
  int dir;
  int e;
  long int start, now;
  int elapsed;
  
  for (r = 0; r < 360; r += 3) {
    // Record timer in milliseconds at start of frame cycle
    start = millis ();
    
    // Draw empty radar scope
    drawBackground ();

    drawRadarScreen (Rings, Axes);
    
    drawGatheredTargets ();

    if (Sweeps < GameDuration) {
      dir = getPlayerMove ();

      if (dir != 0) {
        movePlayer (dir);
        reCalculateBearings ();
      }
    }
    
    // Draw current scan vector
    drawRadarVector (r);

    // Do we have any new echoes for this scanner bearing?
    findNewEchoes (r, NTARGETS);
    
    // Add un-faded echoes
    for (e = 0; e < NECHOES; e++) {
      if (Echo[e].age > 0)
        circle (Echo[e].x, Echo[e].y, Echo[e].rad, 1, 1);
      
      Echo[e].age--;
    }
    
    if (r == 180)
      Sweeps++;
      
    if (Sweeps < GameDuration) {
      drawTimer ();
    }
    else {
      fillRoundRect (CENX - (3 * 9) - 2, CENY - 8, CENX + (3 * 9) + 2, CENY + 12, 7);
      setText (CENX - (3 * 9), CENY, "GAME OVER");
    }
    
    // Update LCD for this frame
    updscreen ();
    
    // Work out timing for this frame
    now = millis ();
    elapsed = now - start;
    
//    Serial.print (elapsed);
//    Serial.println ("ms.");
    
    if (elapsed < 40)
      delay (40 - elapsed);
  }
  
  Sweeps++;
}


/* drawBackground --- draw the screen background */

void drawBackground (void)
{
  int r, c;
  
  // clrFrame ();
  
  // Checkerboard background
  for (r = 0; r < MAXROWS; r++) {
    for (c = 0; c < MAXX; c += 2) {
      Frame[r][c] = 0x55;
      Frame[r][c + 1] = 0xaa;      
    }
  }
  
  // Four cases where the edge of the playing area is visible
  if (Player.x < CENX) {
    fillRect (0, 0, CENX - Player.x, MAXY - 1, 0, 0);
  }
  
  if (Player.y < CENY) {
    fillRect (0, 0, MAXX - 1, CENY - Player.y, 0, 0);
  }
  
  if ((MAXPLAYX - Player.x) < CENX) {
    fillRect ((MAXPLAYX - Player.x) + CENX, 0, MAXX - 1, MAXY - 1, 0, 0);
  }
  
  if ((MAXPLAYY - Player.y) < CENY) {
    fillRect (0, (MAXPLAYY - Player.y) + CENY, MAXX - 1, MAXY - 1, 0, 0);
  }  
}


/* drawRadarScreen --- draw the basic circular radar scope */

void drawRadarScreen (boolean rings, boolean axes)
{
//  unsigned long int before, after;

  // 1108us
//  before = micros ();
  circle (CENX, CENY, 33, 1, 0);
//  after = micros ();
  
//  Serial.print (after - before);
//  Serial.println ("us. circle 33");

  // Range rings
  if (rings) {
    circle (CENX, CENY, 11, 1, -1);
    circle (CENX, CENY, 22, 1, -1);
  }
  
  // Cardinal directions
  if (axes) {
    setVline (CENX, 0, MAXY);
    setHline (CENX - 33, CENX + 33, CENY);
  }
}


/* drawGatheredTargets --- draw the targets that we've walked over */

void drawGatheredTargets (void)
{
  int t;
  
  for (t = 0; t < NTARGETS; t++) {
    if (Target[t].active == false) {
      circle (6, Target[t].y, Target[t].siz, 1, 1);
      
      if (Target[t].rings)
        circle (12, Target[t].y, 2, 1, -1);
      
      if (Target[t].axes)
        setPixel (12, Target[t].y);
    }
  }
}


/* drawTimer --- visualise the game timer */

void drawTimer (void)
{
  // Very simple vertical bar on RHS of screen.
  int y;

  fillRect (MAXX - 10, TIMERY, MAXX - 1, MAXGAMEDURATION + TIMERY, 1, 0);
  
  for (y = 1; y <= GameDuration; y++)
    if (y < Sweeps)
      setHline (MAXX - 9, MAXX - 2, y + TIMERY);
    else
      clrHline (MAXX - 9, MAXX - 2, y + TIMERY);
  
  setHline (MAXX - 9, MAXX - 2, GameDuration + TIMERY);
}


/* drawRadarVector --- draw the radial line representing the current scan vector */

void drawRadarVector (int r)
{
  // This function draws the radial line three times to make it
  // appear more clearly on the rather slow LCD. A better way
  // would be to draw a narrow sector (pie-slice) so that the
  // pixels have time to fully darken before they get switched
  // back to white. But that would require an efficient sector
  // drawing routine, which we don't have (yet).
  int x, y;

  // 252us
  x = (33.0 * cos ((double)r / RADTODEG)) + 0.49;
  y = (33.0 * sin ((double)r / RADTODEG)) + 0.49;

  // 232us
  setLine (MAXX / 2, MAXY / 2, (MAXX / 2) + x, (MAXY / 2) + y);

  x = (33.0 * cos ((double)(r + 2) / RADTODEG)) + 0.49;
  y = (33.0 * sin ((double)(r + 2) / RADTODEG)) + 0.49;

  // 232us
  setLine (MAXX / 2, MAXY / 2, (MAXX / 2) + x, (MAXY / 2) + y);
  
  x = (33.0 * cos ((double)(r + 4) / RADTODEG)) + 0.49;
  y = (33.0 * sin ((double)(r + 4) / RADTODEG)) + 0.49;

  setLine (MAXX / 2, MAXY / 2, (MAXX / 2) + x, (MAXY / 2) + y);
}


/* findNewEchoes --- search the Target array for anything that will cause an echo */

void findNewEchoes (int r, int nt)
{
  // Targets have an 'active' flag so that we can make them disappear
  // after the user walks over them -- currently unimplemented.
  // Some potential additions here: targets that are close to the radar
  // appear larger; make some echoes fade more quickly; give echoes
  // shapes other than circles.
  int t;
  int e;
  
  for (t = 0; t < nt; t++) {
    if (Target[t].active) {                            // Currently active?
      if (abs (Target[t].bearing - (float)r) < 6.0) {  // In the right direction?
        if (Target[t].range < 33.0) {                  // Close enough?
          // Make a new echo
          e = findEchoSlot ();
          Echo[e].x = CENX + (Target[t].x - Player.x);  // Make player-relative co-ordinates
          Echo[e].y = CENY + (Target[t].y - Player.y);
          Echo[e].age = 90;                             // Echoes last 3/4 of a revolution
          Echo[e].rad = Target[t].siz;                  // Target size affects echo size
          
          if (Target[t].range < 11.0) {  // Pick it up?
            Target[t].active = false;
            Target[t].y = Gather_y;
            Gather_y += 6;
          }
        }
      }
      
      if (Target[t].range < 11.0) {  // Close enough for bonus (regardless of bearing)?
        if (Target[t].rings)
          Rings = true;      // Enable range rings
          
        if (Target[t].axes)
          Axes = true;       // Enable axes
          
        if (Target[t].time) {
          if (GameDuration < MAXGAMEDURATION)    // Give user more time
            GameDuration += 5;
          
          Target[t].time = false;  // Only trigger once!
        }
      }
    }
  }
}


/* findEchoSlot --- search the Echo array for an unused slot */

int findEchoSlot (void)
{
  int e;
  
  for (e = 0; e < NECHOES; e++) {
    if (Echo[e].age <= 0)
      return (e);
  }

  // We didn't find an empty slot, so overwrite slot 0  
  return (0);
}


/* getPlayerMove --- read the analog joystick */

int getPlayerMove (void)
{
  // The analog joystick is on Arduino analog pins 0 and 1 for
  // X and Y respectively. The range of an analog input is
  // 0-1023 (10 bits), so the middle position is about 512.
  // At present, only four movement directions are possible.
  int x, y;
  int dir = 0;
  
  x = analogRead (0);
  y = analogRead (1);

  if (x < (512 - 128))
    dir = WEST;
  else if (x > (512 + 128))
    dir = EAST;
  
  if (y < (512 - 128))
    dir = NORTH;
  else if (y > (512 + 128))
    dir = SOUTH;
    
//Serial.print ("Player: ");
//Serial.println (dir);
//Serial.print (x);
//Serial.print (",");
//Serial.println (y);

  switch (dir) {
  case 0:
    break;
  case NORTH:
    setText (0, 0, "North");
    break;
  case SOUTH:
    setText (0, 0, "South");
    break;
  case EAST:
    setText (0, 0, "East");
    break;
  case WEST:
    setText (0, 0, "West");
    break;
  };
  
  return (dir);
}


/* movePlayer --- make player move and update bearings */

void movePlayer (int dir)
{
  switch (dir) {
  case NORTH:
    if (Player.y > 0)
      Player.y--;
      break;
  case SOUTH:
    if (Player.y < (MAXPLAYY - 1))
      Player.y++;
      break;
  case WEST:
    if (Player.x > 0)
      Player.x--;
      break;
  case EAST:
    if (Player.x < (MAXPLAYX - 1))
      Player.x++;
      break;
  }
  
//Serial.print ("New player pos: ");
//Serial.print (Player.x);
//Serial.print (", ");  
//Serial.println (Player.y);
}


/* reCalculateBearings --- update array of target bearings from new player position */

void reCalculateBearings (void)
{
  // We need to know the bearing from the player to each of the radar
  // targets, so that we can rapidly update the display as the "beam" rotates.
  // In this function, we update the array of bearings and ranges after the
  // player position has changed. 'atan2' computes the arctangent, giving a
  // bearing, without the risk of dividing by zero. The result is -180 to +180
  // degrees, so we add 360 to any negative bearings. Range is worked out by
  // Pythagoras' theorem.
  int i;
  int dx, dy;
  
  for (i = 0; i < NTARGETS; i++) {
    if (Target[i].active) {
      dx = Target[i].x - Player.x;
      dy = Target[i].y - Player.y;
      
      Target[i].bearing = atan2 (dy, dx) * RADTODEG;
      Target[i].range = sqrt ((dx * dx) + (dy * dy));
      
      if (Target[i].bearing < 0.0)
        Target[i].bearing += 360.0;
    }
  }
}


/* clrFrame --- clear the entire frame to white */

void clrFrame (void)
{
#ifdef SLOW_CLRFRAME
  // 452us
  int r, c;
  
  for (r = 0; r < MAXROWS; r++) {
    for (c = 0; c < MAXX; c++) {
      Frame[r][c] = 0x00;
    }
  }
#else
  // 332 us
  memset (Frame, 0, sizeof (Frame));
#endif
}


/* setText --- draw text into buffer using predefined font */

void setText (int x, int y, const char *str)
{
  // This function does not, as yet, allow for pixel row positioning of text.
  // The Y co-ordinate is rounded to the nearest row of display RAM bytes.
  // The font (475 bytes) is held in program memory (Flash) to reduce RAM
  // usage. The AVR is a Harvard architecture machine and needs a special
  // instruction to read program memory, which is implemented in C as the
  // 'pgm_read_byte_near' function.
  int row;
  int i;
  int d;
  
  row = y >> 3;
  
  for ( ; *str; str++) {
    d = (*str - ' ') * 5;
    
    for (i = 0; i < 5; i++, d++) {
      Frame[row][x++] = pgm_read_byte_near (font_data + d);
    }
    
    Frame[row][x++] = 0;
  }
}


/* setLine --- draw a line between any two absolute co-ords */

void setLine (int x1, int y1, int x2, int y2)
{
  // Bresenham's line drawing algorithm. Originally coded on the IBM PC
  // with EGA card in 1986.
  int dx, dy;
  int d;
  int i1, i2;
  int x, y;
  int xend, yend;
  int temp;
  int yinc, xinc;
   
  dx = abs (x2 - x1);
  dy = abs (y2 - y1);
  
  if (((y1 > y2) && (dx < dy)) || ((x1 > x2) && (dx > dy))) {
    temp = y1;
    y1 = y2;
    y2 = temp;

    temp = x1;
    x1 = x2;
    x2 = temp;
  }
  
  if (dy > dx) {
    d = (2 * dx) - dy;     /* Slope > 1 */
    i1 = 2 * dx;
    i2 = 2 * (dx - dy);
    
    if (y1 > y2) {
      x = x2;
      y = y2;
      yend = y1;
    }
    else {
      x = x1;
      y = y1;
      yend = y2;
    }
    
    if (x1 > x2)
      xinc = -1;
    else
      xinc = 1;
    
    setPixel (x, y);
    
    while (y < yend) {
      y++;    
      if (d < 0)
        d += i1;
      else {
        x += xinc;
        d += i2;
      }
      
      setPixel (x, y);
    }
  }
  else {          
    d = (2 * dy) - dx;  /* Slope < 1 */
    i1 = 2 * dy;
    i2 = 2 * (dy - dx);
    
    if (x1 > x2) {
      x = x2;
      y = y2;
      xend = x1;
    }
    else {
      x = x1;
      y = y1;
      xend = x2;
    }
    
    if (y1 > y2)
      yinc = -1;
    else
      yinc = 1;
    
    setPixel (x, y);
    
    while (x < xend) {
      x++;
      if (d < 0)
        d += i1;
      else {
        y += yinc;
        d += i2;
      }
      
      setPixel (x, y);
    }
  }
}


/* circle --- draw a circle with edge and fill colours */

void circle (int x0, int y0, int r, int ec, int fc)
{
  // Michener's circle algorithm. Originally coded on the IBM PC
  // with EGA card in 1986.
  int x, y;
  int d;

  x = 0;
  y = r;
  d = 3 - (2 * r);

  if (fc >= 0) {
    while (x < y) {
      cfill (x0, y0, x, y, fc);
      if (d < 0) {
        d += (4 * x) + 6;
      }
      else {
        d += (4 * (x - y)) + 10;
        y--;
      }
      x++;
    }
    
    if (x == y)
      cfill (x0, y0, x, y, fc);
  }
  
  x = 0;
  y = r;
  d = 3 - (2 * r);

  while (x < y) {
    cpts8 (x0, y0, x, y, ec);
    if (d < 0) {
      d += (4 * x) + 6;
    }
    else {
      d += (4 * (x - y)) + 10;
      y--;
    }
    x++;
  }
  
  if (x == y)
    cpts8 (x0, y0, x, y, ec);
}


/* cfill --- draw horizontal lines to fill a circle */

static void cfill (int x0, int y0, int x, int y, int fc)
{
  if (fc) {
    setHline (x0 - x, x0 + x, y0 + y);
    setHline (x0 - x, x0 + x, y0 - y);
    setHline (x0 - y, x0 + y, y0 + x);
    setHline (x0 - y, x0 + y, y0 - x);
  }
  else {
    clrHline (x0 - x, x0 + x, y0 + y);
    clrHline (x0 - x, x0 + x, y0 - y);
    clrHline (x0 - y, x0 + y, y0 + x);
    clrHline (x0 - y, x0 + y, y0 - x);
  }
}


/* cpts8 --- draw eight pixels to form the edge of a circle */

static void cpts8 (int x0, int y0, int x, int y, int ec)
{
  cpts4 (x0, y0, x, y, ec);

// if (x != y)
    cpts4 (x0, y0, y, x, ec);
}


/* cpts4 --- draw four pixels to form the edge of a circle */

static void cpts4 (int x0, int y0, int x, int y, int ec)
{
  if (ec) {
    setPixel (x0 + x, y0 + y);

//  if (x != 0)
      setPixel (x0 - x, y0 + y);

//  if (y != 0)  
      setPixel (x0 + x, y0 - y);

//  if ((x != 0) && (y != 0))
      setPixel (x0 - x, y0 - y);
  }
  else {
    clrPixel (x0 + x, y0 + y);

//  if (x != 0)
      clrPixel (x0 - x, y0 + y);

//  if (y != 0)
      clrPixel (x0 + x, y0 - y);

//  if ((x != 0) && (y != 0))
      clrPixel (x0 - x, y0 - y);
  }
}


/* drawRoundRect --- draw a rounded rectangle */

void drawRoundRect (int x0, int y0, int x1, int y1, int r)
{
  setHline (x0 + r, x1 - r, y0);
  setHline (x0 + r, x1 - r, y1);
  setVline (x0, y0 + r, y1 - r);
  setVline (x1, y0 + r, y1 - r);
  
  drawSplitCircle (x0 + r, y0 + r, x1 - r, y1 - r, r, 1, -1);
}


/* fillRoundRect --- fill a rounded rectangle */

void fillRoundRect (int x0, int y0, int x1, int y1, int r)
{
  int y;
  
  drawSplitCircle (x0 + r, y0 + r, x1 - r, y1 - r, r, 1, 0);
  
  setHline (x0 + r, x1 - r, y0);
  setHline (x0 + r, x1 - r, y1);
  setVline (x0, y0 + r, y1 - r);
  setVline (x1, y0 + r, y1 - r);
  
  for (y = y0 + r; y < (y1 - r); y++)
    clrHline (x0 + 1, x1 - 1, y);
}


/* drawSplitCircle --- draw a split circle with edge and fill colours */

void drawSplitCircle (int x0, int y0, int x1, int y1, int r, int ec, int fc)
{
  // Michener's circle algorithm. Originally coded on the IBM PC
  // with EGA card in 1986.
  int x, y;
  int d;

  x = 0;
  y = r;
  d = 3 - (2 * r);

  if (fc >= 0) {
    while (x < y) {
      splitcfill (x0, y0, x1, y1, x, y, fc);
      if (d < 0) {
        d += (4 * x) + 6;
      }
      else {
        d += (4 * (x - y)) + 10;
        y--;
      }
      x++;
    }
    
    if (x == y)
      splitcfill (x0, y0, x1, y1, x, y, fc);
  }
  
  x = 0;
  y = r;
  d = 3 - (2 * r);

  while (x < y) {
    splitcpts8 (x0, y0, x1, y1, x, y, ec);
    if (d < 0) {
      d += (4 * x) + 6;
    }
    else {
      d += (4 * (x - y)) + 10;
      y--;
    }
    x++;
  }
  
  if (x == y)
    splitcpts8 (x0, y0, x1, y1, x, y, ec);
}


/* cfill --- draw horizontal lines to fill a circle */

static void splitcfill (int x0, int y0, int x1, int y1, int x, int y, int fc)
{
  if (fc) {
    setHline (x0 - x, x1 + x, y1 + y);
    setHline (x0 - x, x1 + x, y0 - y);
    setHline (x0 - y, x1 + y, y1 + x);
    setHline (x0 - y, x1 + y, y0 - x);
  }
  else {
    clrHline (x0 - x, x1 + x, y1 + y);
    clrHline (x0 - x, x1 + x, y0 - y);
    clrHline (x0 - y, x1 + y, y1 + x);
    clrHline (x0 - y, x1 + y, y0 - x);
  }
}


/* splitcpts8 --- draw eight pixels to form the edge of a split circle */

static void splitcpts8 (int x0, int y0, int x1, int y1, int x, int y, int ec)
{
  splitcpts4 (x0, y0, x1, y1, x, y, ec);

// if (x != y)
    splitcpts4 (x0, y0, x1, y1, y, x, ec);
}


/* splitcpts4 --- draw four pixels to form the edge of a split circle */

static void splitcpts4 (int x0, int y0, int x1, int y1, int x, int y, int ec)
{
  if (ec) {
    setPixel (x1 + x, y1 + y);

//  if (x != 0)
      setPixel (x0 - x, y1 + y);

//  if (y != 0)  
      setPixel (x1 + x, y0 - y);

//  if ((x != 0) && (y != 0))
      setPixel (x0 - x, y0 - y);
  }
  else {
    clrPixel (x1 + x, y1 + y);

//  if (x != 0)
      clrPixel (x0 - x, y1 + y);

//  if (y != 0)
      clrPixel (x1 + x, y0 - y);

//  if ((x != 0) && (y != 0))
      clrPixel (x0 - x, y0 - y);
  }
}


/* setVline --- draw vertical line */

void setVline (unsigned int x, unsigned int y1, unsigned int y2)
{
  unsigned int y;
  
  for (y = y1; y <= y2; y++)
    setPixel (x, y);
}


/* clrVline --- draw vertical line */

void clrVline (unsigned int x, unsigned int y1, unsigned int y2)
{
  unsigned int y;
  
  for (y = y1; y <= y2; y++)
    clrPixel (x, y);
}

/* setHline --- set pixels in a horizontal line */

void setHline (unsigned int x1, unsigned int x2, unsigned int y)
{
  unsigned int x;
  unsigned int row;
  unsigned char b;
  
  row = y / 8;
  b = 1 << (y  & 7);
  
  for (x = x1; x <= x2; x++)
    Frame[row][x] |= b;
}


/* clrHline --- clear pixels in a horizontal line */

void clrHline (unsigned int x1, unsigned int x2, unsigned int y)
{
  unsigned int x;
  unsigned int row;
  unsigned char b;
  
  row = y / 8;
  b = ~(1 << (y  & 7));
  
  for (x = x1; x <= x2; x++)
    Frame[row][x] &= b;
}


/* setRect --- set pixels in a (non-filled) rectangle */

void setRect (int x1, int y1, int x2, int y2)
{
  setHline (x1, x2, y1);
  setVline (x2, y1, y2);
  setHline (x1, x2, y2);
  setVline (x1, y1, y2);
}


/* fillRect --- set pixels in a filled rectangle */

void fillRect (int x1, int y1, int x2, int y2, int ec, int fc)
{
  int y;
  
  for (y = y1; y <= y2; y++)
    if (fc == 0)
      clrHline (x1, x2, y);
    else if (fc == 1)
      setHline (x1, x2, y);
  
  if (ec == 1) {
    setHline (x1, x2, y1);
    setVline (x2, y1, y2);
    setHline (x1, x2, y2);
    setVline (x1, y1, y2);
  }
  else if (ec == 0) {
    clrHline (x1, x2, y1);
    clrVline (x2, y1, y2);
    clrHline (x1, x2, y2);
    clrVline (x1, y1, y2);
  }
}


/* setPixel --- set a single pixel */

void setPixel (unsigned int x, unsigned int y)
{
  if ((x < MAXX) && (y < MAXY))
    Frame[y / 8][x] |= 1 << (y & 7);
  else {
//  Serial.print ("setPixel(");
//  Serial.print (x);
//  Serial.print (",");    
//  Serial.print (y);
//  Serial.println (")");    
  }
}


/* clrPixel --- clear a single pixel */

void clrPixel (unsigned int x, unsigned int y)
{
  if ((x < MAXX) && (y < MAXY))
    Frame[y / 8][x] &= ~(1 << (y & 7));
  else {
//  Serial.print ("clrPixel(");
//  Serial.print (x);
//  Serial.print (",");
//  Serial.print (y);
//  Serial.println (")");    
  }
}


/* updscreen --- update the physical screen from the buffer */

void updscreen (void)
{
  // This function contains an eight-way unrolled loop. In the Arduino
  // IDE, the default GCC optimisation switch is -Os, which optimises
  // for space. No automatic loop unrolling is done by the compiler, so
  // we do it explicitly here to save a few microseconds.
//  long int before, after;
//  unsigned char r, c;
  unsigned char *p;
  int i;
  
//lcdGotoRC (0, 0);
  lcdSpi (0xB0); // Set page address to 'r'
  lcdSpi (0x10);   // Sets DDRAM column addr - upper 3-bit
  lcdSpi (0x00); // lower 4-bit
  
//  before = micros ();
  
  p = &Frame[0][0];
  
  for (i = 0; i < ((MAXROWS * MAXX) / 8); i++) {
    lcdData (*p++);
    lcdData (*p++);
    lcdData (*p++);
    lcdData (*p++);
    lcdData (*p++);
    lcdData (*p++);
    lcdData (*p++);
    lcdData (*p++);
  }

/*
  The slow way...
  for (r = 0; r < MAXROWS; r++) {
    for (c = 0; c < MAXX; c++) {
      lcdData (Frame[r][c]);
    }
  }
*/

//  after = micros ();
  
//  Serial.print (after - before);
//  Serial.println ("us updscreen");
}


/* lcd1202_begin --- initialise the Nokia 1202 LCD */

void lcd1202_begin (void)
{
  // LCD initialisation code from Greeeg of the Dangerous Prototypes forum.
  // The chip on the Nokia 1202 LCD is an ST Microelectronics STE2007.
  // Greeeg also designed the 5x7 pixel font that's used by 'setText'.
  /* Configure I/O pins on Arduino */
  pinMode (slaveSelectPin, OUTPUT);
  pinMode (SDAPin, OUTPUT);
  pinMode (SCLKPin, OUTPUT);
  
  digitalWrite (slaveSelectPin, HIGH);
  digitalWrite (SDAPin, HIGH);
  digitalWrite (SCLKPin, HIGH);

  SPI.begin ();
  // The following line fails on arduino-0021 due to a bug in the SPI library
  // Compile with arduino-0022 or later
  SPI.setClockDivider (SPI_CLOCK_DIV4);
  SPI.setBitOrder (MSBFIRST);
  SPI.setDataMode (SPI_MODE3);
  
  // For the moment, disable SPI and return the pins to normal I/O use
  SPCR &= ~(1<<SPE);
  
  /* Start configuring the STE2007 LCD controller */
// LCDOUT |= RESET; // Hard reset
   
   lcdSpi (0xE2); // SW reset.
   
// lcdSpi (0xA5); // Power saver ON, display all pixels ON
   lcdSpi (0xA4); // Power saver OFF
   lcdSpi (0x2F); // Power control set
// lcdSpi (0xAE); // Display OFF, blank
   lcdSpi (0xAF); // Display ON
  
// These next two commands (A0/A1) don't work on my 1202 LCD
// lcdSpi (0xA0);   // Display not flipped
// lcdSpi (0xA1);   // Display flipped

// 'Normal' makes the bit-mapped display work top/left to
// bottom/right when the LCD is oriented with the connector
// at the top edge. I don't know which way up the LCD is
// supposed to be mounted in the phone, but this setup
// makes more sense to me on the breadboard.
   lcdSpi (0xC0);   // Display common driver normal
// lcdSpi (0xC8);   // Display common driver flipped
   
   lcdSpi (0x80 | 16); // Electronic volume to 16
   
   lcdClr ();

   // v--- Likely these aren't needed...And might not be working :P ---v

   lcdSpi (0xef);   // Set refresh rate
   lcdSpi (3);         // 65 Hz
   
   lcdSpi (0x3d);   // Set Charge Pump multiply factor
   lcdSpi (0);         // 5x
   
   lcdSpi (0x36); // Bias ratio 1/4
   
   lcdSpi (0xad);   // set contrast
   lcdSpi (0x20 | 20);      // 20/32
   
   lcdSpi (0xe1);
   lcdSpi (0);
   
   lcdSpi (0xa6);   // Display normal
// lcdSpi (0xA7);   // Display reversed (inverse video)
}


void lcdClr (void)
{
   int i;

   lcdGotoRC (0, 0);

   for (i = 0; i < 16 * 6 * 9; i++) {
      lcdData (0x00); // fill DDRAM with Zeros       
   }
}


void lcdGotoRC (int r, int c)
{
   lcdSpi (0xB0 | (r & 0x0F)); // Set page address to 'r'
   lcdSpi (0x10 | (c >> 4));   // Sets DDRAM column addr - upper 3-bit
   lcdSpi (0x00 | (c & 0x0F)); // lower 4-bit
}


/* lcdData --- send a data byte to the LCD by fast hardware SPI */

inline void lcdData (unsigned char d)
{
  // Data bytes are distinguished from command bytes by an initial
  // '1' bit (followed by 8 data bits). AVR SPI hardware cannot do 9-bit
  // transfers, so we do the initial bit by bit-banging, then switch on
  // the SPI hardware and send a byte directly from the SPDR register.
  // This method is about twice as fast as the software method and
  // gives us a complete screen update in under 4 milliseconds.
  char i;
   
  LCDOUT &= ~CS;
  
  LCDOUT |= SDA;     // Leading '1' bit for LCD data
  LCDOUT &= ~SCLK;   // Toggle the clock
  LCDOUT |= SCLK;
  
#ifdef SOFTWARE_SPI
  for (i = 0; i < 8; i++) {
    if (d & 0x80)
      LCDOUT |= SDA;
    else
      LCDOUT &= ~SDA;

    LCDOUT &= ~SCLK;  // Toggle the clock
    LCDOUT |= SCLK;

    d <<= 1;
  }
#else
  SPCR |= 1 << SPE;
  
  SPDR = d;
  
  while (!(SPSR & (1 << SPIF)))
    ;
    
  SPCR &= ~(1 << SPE);
#endif
   
  LCDOUT |= CS;
}


/* lcdSpi --- send a command byte to the LCD by bit-banging SPI */

inline void lcdSpi (int d)
{
  char i;
   
  LCDOUT &= ~CS;
  // digitalWrite (slaveSelectPin, LOW);
   
  for (i = 0; i < 9; i++) {
    if (d & 0x100)
      LCDOUT |= SDA;
      // digitalWrite (SDAPin, HIGH);
    else
      LCDOUT &= ~SDA;
      // digitalWrite (SDAPin, LOW);

    LCDOUT &= ~SCLK;
    // digitalWrite (SCLKPin, LOW);
    LCDOUT |= SCLK;
    // digitalWrite (SCLKPin, HIGH);

    d <<= 1;
  }
   
  LCDOUT |= CS;
  // digitalWrite (slaveSelectPin, HIGH);
}
