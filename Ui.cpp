/*
    Bastet - tetris clone with embedded bastard block chooser
    (c) 2005-2009 Federico Poloni <f.polonithirtyseven@sns.it> minus 37

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Ui.hpp"
#include "BlockPosition.hpp"
#include "Config.hpp"
#include "BastetBlockChooser.hpp"
#include "BlockChooser.hpp"

#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>

using namespace std;
using namespace boost;

namespace Bastet{

  Score &operator +=(Score &a, const Score &b){
    a.first+=b.first;
    a.second+=b.second;
    return a;
  }

  void voidendwin(){
    endwin();
  }

  void PrepareUiGetch(){ ///gets ready for a getch() in the UI, i.e. empties the char buffer, sets blocking IO
    nodelay(stdscr,TRUE);
    while(getch()!=ERR);
    nodelay(stdscr,FALSE);
  }
  
  BorderedWindow::BorderedWindow(int height, int width, int y, int x){
    if(y==-1 || x==-1){
      int screen_h, screen_w;
      getmaxyx(stdscr,screen_h, screen_w);
      if(y==-1)
	y=(screen_h-height-2)/2-1;
      if(x==-1)
	x=(screen_w-width-2)/2-1;
    }
    _border=newwin(height+2,width+2,y,x);
    _window=derwin(_border,height,width,1,1); 
    //    wattrset(_border,COLOR_PAIR(21));
    RedrawBorder();
  }

  BorderedWindow::~BorderedWindow(){
    delwin(_window);
    delwin(_border);
  }
  
  void BorderedWindow::MoveTo(int y, int x){
    int h, w;
    getmaxyx(_window,h,w);
    delwin(_window); //a derived window does not follow its parent, so recreate it
    mvwin(_border,y<0?0:y,x<0?0:x);
    _window=derwin(_border,h,w,1,1);
  }

  void BorderedWindow::Resize(int height, int width){
    delwin(_window);
    werase(_border);
    wrefresh(_border);
    wresize(_border,height+2,width+2);
    _window=derwin(_border,height,width,1,1);
  }

  BorderedWindow::operator WINDOW*(){
    return _window;
  }

  void BorderedWindow::RedrawBorder(){
    box(_border,0,0);
    wrefresh(_border);
  }
 
  int BorderedWindow::GetMinX(){
    int x, y;
    getbegyx(_border,y,x);
    (void)(y); //silence warning about unused y
    return x;
  }

  int BorderedWindow::GetMinY(){
    int y, x;
    getbegyx(_border,y,x);
    return y;
  }

  int BorderedWindow::GetMaxX(){
    int x, y;
    getmaxyx(_border,y,x);
    (void)(y); //silence warning about unused y
    return GetMinX()+x;
  }

  int BorderedWindow::GetMaxY(){
    int y,x;
    getmaxyx(_border,y,x);
    return GetMinY()+y;
  }

  void BorderedWindow::DrawDot(const Dot &d, Color c){
    if(c==0 || COLORS<256){
      wattrset((WINDOW *)(*this),c);
      mvwaddch(*this,d.y,2*d.x,' ');
      mvwaddch(*this,d.y,2*d.x+1,' ');
      return;
    }
    //filled cell: solid colour with a thin black gap on its right edge,
    //so neighbouring squares are visibly separated
    wattrset((WINDOW *)(*this),c);
    mvwaddch(*this,d.y,2*d.x,' ');
    mvwaddstr(*this,d.y,2*d.x+1,"▕");
  }

  void BorderedWindow::DrawGrid(const Dot &d){
    wattrset((WINDOW *)(*this),COLOR_PAIR(23));
    mvwaddch(*this,d.y,2*d.x,ACS_BULLET);
    mvwaddch(*this,d.y,2*d.x+1,' ');
  }

  void BorderedWindow::DrawGhost(const Dot &d, Color c){
    wattrset((WINDOW *)(*this),c);
    mvwaddch(*this,d.y,2*d.x,'[');
    mvwaddch(*this,d.y,2*d.x+1,']');
  }

  Curses::Curses(){
    setlocale(LC_ALL,""); //needed for the unicode tile-gap glyph
    if(initscr()==NULL){
      fprintf(stderr,"dqtris: error while initializing graphics (ncurses library).\n");
      exit(1);
    }
    if(!has_colors()){
      endwin();
      fprintf(stderr,"dqtris: no color support, sorry. Ask the author for a black and white version.");
      exit(1);
    }
    
    /* Turn off cursor. */
    curs_set(0);
    atexit(voidendwin); /*make sure curses are properly stopped*/
    
    /* Setup keyboard. We'd like to get each and every character, but
       not to display them on the terminal. */
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    nonl();
    noecho();
    cbreak();
    
    start_color();
    /* 1 - 16 is for blocks */
    init_pair(1, COLOR_BLACK, COLOR_RED);
    init_pair(2, COLOR_BLACK, COLOR_YELLOW);
    init_pair(3, COLOR_BLACK, COLOR_GREEN);
    init_pair(4, COLOR_BLACK, COLOR_CYAN);
    init_pair(5, COLOR_BLACK, COLOR_MAGENTA);
    init_pair(6, COLOR_BLACK, COLOR_BLUE);
    init_pair(7, COLOR_BLACK, COLOR_WHITE);
    if(COLORS>=256){ //classic guideline colours
      //foreground (black) draws the gap between squares
      init_pair(1, COLOR_BLACK, 196); //Z red
      init_pair(2, COLOR_BLACK, 208); //L orange
      init_pair(3, COLOR_BLACK, 40);  //S green
      init_pair(4, COLOR_BLACK, 51);  //I cyan
      init_pair(5, COLOR_BLACK, 129); //T purple
      init_pair(6, COLOR_BLACK, 27);  //J blue
      init_pair(7, COLOR_BLACK, 220); //O yellow
    }
    init_pair(23, COLORS>=256 ? 238 : COLOR_WHITE, COLOR_BLACK); //background grid
    /* 31 - 37: ghost piece, block colour as foreground */
    for(short i=1;i<=7;++i){
      short fg,bg;
      pair_content(i,&fg,&bg);
      init_pair(i+30,bg,COLOR_BLACK);
    }

    
    /* 17 - ? is for other things */
    init_pair(17, COLOR_RED,    COLOR_BLACK); //points
    init_pair(18, COLOR_YELLOW, COLOR_BLACK); //number of lines
    init_pair(19, COLOR_GREEN,  COLOR_BLACK); //level
    init_pair(20, COLOR_YELLOW, COLOR_BLACK); //messages
    init_pair(21, COLOR_WHITE,  COLOR_BLACK); //window borders
    init_pair(22, COLOR_WHITE,  COLOR_BLACK); //end of line animation

    /* Set random seed. */
    srandom(time(NULL)+37);
  }

  Ui::Ui():
    _level(0),
    _wellWin(WellHeight,2*WellWidth),
    _nextWin(5,14,_wellWin.GetMinY(),_wellWin.GetMaxX()+1),
    _scoreWin(7,14,_nextWin.GetMaxY(),_nextWin.GetMinX())
  {
    BOOST_FOREACH(ColorWellLine &a, _colors)
      a.assign(0);
  }

  Dot BoundingRect(const std::string &message){ //returns x and y of the minimal rectangle containing the given string
    vector<string> splits;
    split(splits,message,is_any_of("\n"));
    size_t maxlen=0;
    BOOST_FOREACH(string &s, splits){
      maxlen=max(maxlen,s.size());
    }
    return (Dot){int(maxlen+1),int(splits.size())};
  }

  void Ui::MessageDialog(const std::string &message){
    Dot d=BoundingRect(message);
    int ch;
    do{ //redraw from scratch after every terminal resize
      RedrawStatic();
      BorderedWindow w(d.y,d.x);
      wattrset((WINDOW *)w,COLOR_PAIR(20));
      mvwprintw(w,0,0,message.c_str());
      w.RedrawBorder();
      wrefresh(w);
      PrepareUiGetch();
      do{
	ch=getch();
      }
      while(ch!=' ' && ch!=13 && ch!=KEY_RESIZE); //13=return key!=KEY_ENTER, it seems
      if(ch==KEY_RESIZE) Relayout();
    } while(ch==KEY_RESIZE);
  }

  std::string Ui::InputDialog(const std::string &message){
    RedrawStatic();
    Dot d=BoundingRect(message);
    d.y+=3;
    BorderedWindow w(d.y,d.x);
    wattrset((WINDOW *)w,COLOR_PAIR(20));
    mvwprintw(w,0,0,message.c_str());
    w.RedrawBorder();
    wrefresh(w);
    PrepareUiGetch();
    //nocbreak();
    echo();
    curs_set(1);
    char buf[51];
    mvwgetnstr(w,d.y-2,1,buf,50);
    curs_set(0);
    noecho();
    return string(buf);
  }
  
  int Ui::KeyDialog(const std::string &message){
    RedrawStatic();

    Dot d=BoundingRect(message);

    BorderedWindow w(d.y,d.x);
    wattrset((WINDOW *)w,COLOR_PAIR(20));
    mvwprintw(w,0,0,message.c_str());
    w.RedrawBorder();
    wrefresh(w);
    PrepareUiGetch();
    return getch();
  }
  
  int Ui::MenuDialog(const vector<string> &choices){
    RedrawStatic();
    size_t width=0;
    BOOST_FOREACH(const string &s, choices){
      width=max(width,s.size());
    }

    Dot d={int(width+5),int(choices.size())};
    size_t chosen=0;
    int ch;
    bool done=false;
    while(true){ //redraw from scratch after every terminal resize
    RedrawStatic();
    BorderedWindow w(d.y,d.x);
    wattrset((WINDOW *)w,COLOR_PAIR(20));
    for(size_t i=0;i<choices.size();++i){
      mvwprintw(w,i,4,choices[i].c_str());
    }
    w.RedrawBorder();
    wrefresh(w);
    PrepareUiGetch();
    bool resized=false;
    mvwprintw(w,chosen,1,"-> ");
    wrefresh(w);
    do{
      ch=getch();
      switch(ch){
      case KEY_UP:
	if(chosen==0) break;
	mvwprintw(w,chosen,1,"   ");
	chosen--;
	mvwprintw(w,chosen,1,"-> ");
	wrefresh(w);
	break;
      case KEY_DOWN:
	if(chosen==choices.size()-1) break;
	mvwprintw(w,chosen,1,"   ");
	chosen++;
	mvwprintw(w,chosen,1,"-> ");
	wrefresh(w);
	break;
      case 13: //ENTER
      case ' ':
	done=true;
	break;
      case KEY_RESIZE:
	resized=true;
	break;
      }
    } while(!done && !resized);
    if(done) return chosen;
    Relayout();
    }
  }
  
  void Ui::ChooseLevel(){
    RedrawStatic();
    int ch='0';
    format fmt("    Get ready!\n"
	       " \n"
	       " Starting level = %1% \n"
	       " 0-9 to change\n"
	       " <SPACE> to start\n");
    string msg;
    while(ch!=' '){
      msg=str(fmt % _level);
      PrepareUiGetch();
      Dot d=BoundingRect(msg );
      BorderedWindow w(d.y,d.x);
      wattrset((WINDOW *)w,COLOR_PAIR(20));
      mvwprintw(w,0,0,msg.c_str());
      w.RedrawBorder();
      ch=getch();
      switch(ch){
      case '0'...'9':
	_level=ch-'0';
      }
    }
    assert(_level>=0 && _level<=9);
  }

  void Ui::RedrawStatic(){
    erase();
    wrefresh(stdscr);
    _wellWin.RedrawBorder();
    _nextWin.RedrawBorder();
    _scoreWin.RedrawBorder();

    //title above the well, if the terminal is tall enough
    if(_wellWin.GetMinY()>0){
      static const char title[]="D Q T R I S";
      attrset(COLOR_PAIR(17)|A_BOLD);
      mvprintw(_wellWin.GetMinY()-1,(_wellWin.GetMinX()+_wellWin.GetMaxX()-int(sizeof(title)-1))/2,"%s",title);
      attrset(A_NORMAL);
      wrefresh(stdscr);
    }

    wattrset((WINDOW*)_nextWin,COLOR_PAIR(17));
    mvwprintw(_nextWin,0,0," Next:");
    wrefresh(_nextWin);

    wattrset((WINDOW*)_scoreWin,COLOR_PAIR(17));
    mvwprintw(_scoreWin,1,0,"Score:");
    wattrset((WINDOW*)_scoreWin,COLOR_PAIR(18));
    mvwprintw(_scoreWin,3,0,"Lines:");
    wattrset((WINDOW*)_scoreWin,COLOR_PAIR(19));
    mvwprintw(_scoreWin,5,0,"Level:");
    wrefresh(_scoreWin);
  }

  //must be <1E+06, because it should fit into a timeval usec field(see man select)
  static const boost::array<int,10> delay = {{999999, 770000, 593000, 457000, 352000, 271000, 208000, 160000, 124000, 95000}};

  void Ui::DropBlock(BlockType b, Well *w){
    fd_set in, tmp_in;
    struct timeval time;

    FD_ZERO(&in);
    FD_SET(0,&in); //adds stdin
    
    time.tv_sec=0;
    time.tv_usec=delay[_level];
    
    //assumes nodelay(stdscr,TRUE) has already been called
    BlockPosition p;

    RedrawWell(w,b,p);
    Keys *keys=config.GetKeys();

    while(1){ //break = tetromino locked
      tmp_in=in;
      int sel_ret=select(FD_SETSIZE,&tmp_in, NULL, NULL, &time);
      if(sel_ret==0){ //timeout
	if(!p.MoveIfPossible(Down,b,w))
	  break;
	time.tv_sec=0;
	time.tv_usec=delay[_level];
      }
      else{ //keypress
	int ch=getch();
	if(ch==keys->Left)
	  p.MoveIfPossible(Left,b,w);
	else if(ch==keys->Right)
	  p.MoveIfPossible(Right,b,w);
	else if(ch==keys->Down){
	  bool val=p.MoveIfPossible(Down,b,w);
	  if(val){
	    //_points++;
	    //RedrawScore();
	    time.tv_sec=0;
	    time.tv_usec=delay[_level];
	  }
	  else break;
	}
	else if(ch==keys->RotateCW)
	  p.MoveIfPossible(RotateCW,b,w);
	else if(ch==keys->RotateCCW)
	  p.MoveIfPossible(RotateCCW,b,w);
	else if(ch==keys->Drop){
	  p.Drop(b,w);
	  //_points+=2*fb.HardDrop(w);
	  //RedrawScore();
	  break;
	}
	else if(ch==KEY_RESIZE){
	  RedrawAfterResize();
	  RedrawWell(w,b,p);
	}
	else if(ch==keys->Pause){
	  MessageDialog("Press SPACE or ENTER to resume the game");
	  RedrawStatic();
	  RedrawWell(w,b,p);
	  nodelay(stdscr,TRUE);
	}
	else {} //default...

      } //keypress switch
      RedrawWell(w,b,p);
    } //while(1) 

    LinesCompleted lc=w->Lock(b,p);
    //locks also into _colors
    BOOST_FOREACH(const Dot &d, p.GetDots(b))
      if(d.y>=0)
	_colors[d.y+2][d.x]=GetColor(b);

    RedrawWell(w,b,p);
    if(lc._completed.any()){
      CompletedLinesAnimation(lc);
      w->ClearLines(lc);
      //clears also _colors
      ColorWell::reverse_iterator it=lc.Clear(_colors.rbegin(),_colors.rend());
      for(;it!=_colors.rend();++it){
	it->assign(0);
      }
      
      int newlines=lc._completed.count();
      if(( (_lines+newlines)/10 - _lines/10 !=0) && _level<9){
	_level++;
      }

      _lines+=newlines;
      switch(newlines){
      case 1:
	_points+=100;
	break;
      case 2:
	_points+=300;
	break;
      case 3:
	_points+=500;
	break;
      case 4:
	_points+=800;
	break;
      }
      RedrawScore();
    }
  }

  void Ui::RedrawWell(const Well *w, BlockType b, const BlockPosition &p){
    for(int i=0;i<WellWidth;++i)
      for(int j=0;j<WellHeight;++j){
	Dot d={i,j};
	if(_colors[j+2][i]==0)
	  _wellWin.DrawGrid(d);
	else
	  _wellWin.DrawDot(d,_colors[j+2][i]);
      }
    
    //ghost piece: where the falling block would land on a hard drop
    BlockPosition ghost=p;
    ghost.Drop(b,w);
    BOOST_FOREACH(const Dot &d, ghost.GetDots(b))
      if(d.y>=0)
	_wellWin.DrawGhost(d,COLOR_PAIR(PAIR_NUMBER(GetColor(b))+30));

    BOOST_FOREACH(const Dot &d, p.GetDots(b))
      _wellWin.DrawDot(d,GetColor(b));

    wrefresh(_wellWin);
  }

  void Ui::Relayout(){
    int screen_h, screen_w;
    getmaxyx(stdscr,screen_h,screen_w);
    _wellWin.MoveTo((screen_h-WellHeight-2)/2-1,(screen_w-2*WellWidth-2)/2-1);
    _nextWin.MoveTo(_wellWin.GetMinY(),_wellWin.GetMaxX()+1);
    _scoreWin.MoveTo(_nextWin.GetMaxY(),_nextWin.GetMinX());
  }

  void Ui::RedrawAfterResize(){
    Relayout();
    RedrawStatic();
    RedrawScore();
    RedrawNext(_next);
  }

  void Ui::ClearNext(){
    RedrawNext(Queue());
  }

  void Ui::RedrawNext(const Queue &next){
    _next=next;
    wmove((WINDOW*)_nextWin,1,0);
    wclrtobot((WINDOW*)_nextWin);

    int y=2;
    BOOST_FOREACH(BlockType b, next){
      BlockPosition p((Dot){2,y});
      BOOST_FOREACH(const Dot &d, p.GetDots(b))
        _nextWin.DrawDot(d,GetColor(b));
      y+=3;
    }
    wrefresh(_nextWin);
  }

  void Ui::RedrawScore(){
    wattrset((WINDOW*)_scoreWin,COLOR_PAIR(17));
    mvwprintw(_scoreWin,1,7,"%6d",_points);
    mvwprintw(_scoreWin,2,5,"%8s",str(format("(%1%)") % max(_best,_points)).c_str()); //best on this board, or this game once beaten
    wattrset((WINDOW*)_scoreWin,COLOR_PAIR(18));
    mvwprintw(_scoreWin,3,7,"%6d",_lines);
    wattrset((WINDOW*)_scoreWin,COLOR_PAIR(19));
    mvwprintw(_scoreWin,5,7,"%6s",str(format("%1%/%2%") % (_level+1) % int(delay.size())).c_str()); //levels 1 to 10
    wrefresh(_scoreWin);
    return;
  }

  void Ui::CompletedLinesAnimation(const LinesCompleted &completed){
    wattrset((WINDOW*)_wellWin,COLOR_PAIR(22));
    for(int i=0;i<2;++i){ //a single blink: flash, then blank
      for(int k=0;k<4;++k){
	if(completed._completed[k]){
	  wmove(_wellWin,completed._baseY+k,0);
	  whline(_wellWin, i%2?' ':':',WellWidth*2);
	}
      }
      wrefresh(_wellWin);
      usleep(100000);
    }
  }

  void Ui::Play(BlockChooser *bc, difficulty_t diff){
    HighScores *hs=config.GetHighScores(diff);
    _best=0;
    BOOST_FOREACH(const HighScore &h, *hs)
      _best=max(_best,h.Score);
    _level=0;
    _points=0;
    _lines=0;
    BOOST_FOREACH(ColorWellLine &a, _colors)
      a.assign(0);
    Queue q=bc->GetStartingQueue();
    //one 3-row slot per previewed block (at least one, so the layout matches bastet's)
    int slots=max<int>(1,q.size()-1);
    _nextWin.Resize(2+3*slots,14);
    Relayout();
    ClearNext();
    RedrawStatic();
    RedrawScore();
    Well w;
    nodelay(stdscr,TRUE);
    try{
      while(true){
	{ //ignores the keys pressed during the next block calculation, but not resizes
	  int c; bool resized=false;
	  while((c=getch())!=ERR) if(c==KEY_RESIZE) resized=true;
	  if(resized) RedrawAfterResize();
	}
	BlockType current=q.front();
	q.pop_front();
	RedrawNext(q);
	DropBlock(current,&w);
	q.push_back(bc->GetNext(&w,q));
      }
    } catch(GameOver &go){

    }
    return;
  }

  void Ui::HandleHighScores(difficulty_t diff){
    HighScores *hs=config.GetHighScores(diff);
    if(hs->Qualifies(_points)){
      string name=InputDialog(" Congratulations! You got a high score \n Please enter your name");
      hs->InsertHighScore(_points,name);
    }else{
      MessageDialog("You did not get into\n"
		    "the high score list!\n"
		    "\n"
		    "     Try again!\n"
		    );
    }
  }

  void Ui::ShowHighScores(difficulty_t diff){
    HighScores *hs=config.GetHighScores(diff);
    string allscores;
    allscores+=str(format("**%1%**\n") % DifficultyName(diff));
    format fmt("%-20.20s %8d\n");
    for(HighScores::reverse_iterator it=hs->rbegin();it!=hs->rend();++it){
      allscores+=str(fmt % it->Scorer % it->Score);
    }
    MessageDialog(allscores);
  }

  void Ui::CustomizeKeys(){
    Keys *keys=config.GetKeys();
    format fmt(
      "Press the key you wish to use for:\n\n"
      "%=1.34s\n\n");
    keys->Down=KeyDialog(str(fmt % "move tetromino DOWN (soft-drop)"));
    keys->Left=KeyDialog(str(fmt % "move tetromino LEFT"));
    keys->Right=KeyDialog(str(fmt % "move tetromino RIGHT"));
    keys->RotateCW=KeyDialog(str(fmt % "rotate tetromino CLOCKWISE"));
    keys->RotateCCW=KeyDialog(str(fmt % "rotate tetromino COUNTERCLOCKWISE"));
    keys->Drop=KeyDialog(str(fmt % "DROP tetromino (move down as much as possible immediately)"));
    keys->Pause=KeyDialog(str(fmt % "PAUSE the game"));
  }
}
