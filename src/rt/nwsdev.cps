% SCCSid "$SunId$ LBL"

%
% By Isaac Kuo
%

#include "newsconstants.h"

cdef cps_clear()
  textbackground setcolor clippath fill
cdef initcanvas(x,y,width,height,mb1key,mb2key,mb3key)
% a couple of definitions of commands in Sun NeWS but not in
% SiliconGraphics NeWS

currentdict /createcanvas known not % check if they're defined or not
 {
  /createcanvas
   {
    3 2 roll newcanvas /newcan exch def
    0 0 4 2 roll newpath rectpath
    newcan reshapecanvas newpath
    newcan
   } def
  /mapcanvas
   {
    /Mapped true put
   } def
 } if

% terrific, wasn't it?

  /Can framebuffer width height createcanvas def
  Can /Retained true put
  Can setcanvas x y movecanvas currentcanvas mapcanvas
  clippath pathbbox pop pop translate
  1.0 .75 .3 setrgbcolor clippath fill
  /scrollheight textareaheight fontheight sub def
  thefont findfont fontheight scalefont setfont
  /textbackground textbackgroundRED textbackgroundGREEN
   textbackgroundBLUE rgbcolor def
  /MB1key mb1key def
  /MB2key mb2key def
  /MB3key mb3key def

% scroll scrolls the text area
/scroll
 {
  newpath
  0 0 width scrollheight points2rect rectpath
  0 fontheight copyarea % Scroll the text area
  newpath textbackground setcolor
  0 0 width fontheight points2rect rectpath
  fill
  /textcursorposition 0 def
 } def
  scroll scroll

% myshow takes a string and prints it at the current cursor location
/myshow
 {
  textshadowgray setgray
  textcursorposition 1 sub textbottom 1 sub moveto dup show
  0 setgray
  textcursorposition textbottom moveto show
  /textcursorposition currentpoint pop def
 } def

% mydel takes a string and deletes it from the current cursor location
/mydel
 {
  textbackground setcolor
  dup stringwidth pop textcursorposition exch sub
  dup /textcursorposition exch def
  1 sub textbottom 1 sub moveto dup show textcursorposition %
  textbottom moveto show
 } def

/normalcursor
 {
  /xcurs /xcurs_m Can setstandardcursor
 } def
  normalcursor

 % get this canvas ready for input
  /buttonevent createevent def
  buttonevent /Name [/LeftMouseButton /MiddleMouseButton /RightMouseButton] put 
  buttonevent /Action /UpTransition put
  buttonevent /Canvas Can put
  /keyevent Can addkbdinterests aload /EVENTS exch
   def revokeinterest revokeinterest def
  /dumevent createevent def % dumevent is used by the input checker
  dumevent /Name 32 put     % to insure awaitevent returns an answer
  dumevent /Action 13 put   % immediately; if it is the first one
  dumevent /Canvas Can put  % returned, then no keyboard events
  dumevent expressinterest  % were waiting.
  /kdevent createevent def % kdevent is used by the input checker
  kdevent /Action 666 put  % to replace waiting keyboard events with
  kdevent /Canvas Can put  % something which acts interchangably with
  kdevent expressinterest  % a normal keyboard event.

cdef box(x1,y1,x2,y2,float r,float g,float b)
 % Draw a filled box at x,y in pixels with color r,g,b
  r g b setrgbcolor newpath
  x1 y1 x2 y2 points2rect rectpath fill
#define tag 1990
cdef cps_cleanup() => tag()
 % Clean up just enough stuff so the window will die quietly
  keyevent revokeinterest
  kdevent revokeinterest
  EVENTS Can revokekbdinterests
  dumevent revokeinterest
  /Can currentcanvas def
  Can /EventsConsumed /NoEvents put
  Can /Transparent true put
  Can /Mapped false put
  Can /Retained false put
  tag tagprint
cdef getthebox(X,Y,W,H) => tag(X,Y,W,H)
 % Get the coordinates of the box from the user

% While Sun NeWS coordinates default to pixels, Silicon Graphics NeWS
% defaults to "points", which are 4/3 the size of pixels in both directions.
% Silicon Graphics NeWS does not have "createcanvas" defined, so it is
% used to determine whether the coordinates should be translated.

  currentcanvas createoverlay setcanvas
  currentdict /createcanvas known not
   {
    .75 .75 scale
   } if
  getwholerect waitprocess
  aload pop /y1 exch def /x1 exch def /y0 exch def /x0 exch def
  x0 x1 gt { /x x1 def /w x0 x1 sub def }
           { /x x0 def /w x1 x0 sub def } ifelse
  y0 y1 gt { /y y1 def /h y0 y1 sub def }
           { /y y0 def /h y1 y0 sub def } ifelse
  h w y x tag tagprint typedprint typedprint typedprint typedprint
cdef printout(string message)
 % print message without scrolling the text "window" up
  message myshow
cdef linefeed(string message)
 % print message and scroll
  message myshow scroll
cdef getclick(x,y,key) => tag(key,y,x)
 % get a cursor position marked by click or key
  buttonevent expressinterest
  /beye_m /xhair_m Can setstandardcursor
  /theclick awaitevent def
  currentcursorlocation textareaheight sub
  normalcursor
  buttonevent revokeinterest

  theclick /Name get
 % Translate mouse clicks if necessary
  dup /LeftMouseButton eq
   {pop MB1key} if
  dup /MiddleMouseButton eq
   {pop MB2key} if
  dup /RightMouseButton eq
   {pop MB3key} if
  cvi 
  tag tagprint typedprint typedprint typedprint
cdef isready(keyread) => tag(keyread)
 % tells whether character input is ready
  0 % default output
  dumevent createevent copy sendevent
   {
    /theevent awaitevent def
    theevent /Action get 13 eq
     { exit } if
    pop 1
    /newevent kdevent createevent copy def
    newevent /Name theevent /Name get put
    newevent sendevent
   } loop
  tag tagprint typedprint
cdef startcomin()
 % get ready for execution of comin
  /nouse /nouse_m Can setstandardcursor
cdef endcomin()
 % get ready for normal execution
  normalcursor
cdef getkey(key) => tag(key)
 % get a keypress
  textcursor myshow awaitevent /Name get cvi
  textcursor mydel tag tagprint typedprint
cdef delete(string s)
 % delete the string s
  s mydel
