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

#include "Config.hpp"

#include <boost/program_options.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/assign.hpp>
#include <curses.h>
#include <cstdlib>
#include <fstream>

//DBG
#include <iostream>

using namespace std;
using namespace boost;
namespace po=boost::program_options;

namespace Bastet{
  const size_t HowManyHighScores=10;
  const std::string RcFileName="/.dqtrisrc";
  const std::string LocalHighScoresFileName="/.dqtrisscores";
  const std::string LegacyRcFileName="/.bastetrc";
  const std::string LegacyHighScoresFileName="/.bastetscores";

  const char *DifficultyName(int difficulty){
    switch(difficulty){
    case difficulty_fair: return "Fair";
    case difficulty_normal: return "Bastard";
    case difficulty_hard: return "Bastard, no preview";
    }
    return "?";
  }

  //opens the dqtris file, or bastet's old one if dqtris has never written its own
  static void OpenWithFallback(ifstream &ifs, const string &name, const string &legacy){
    ifs.open(name.c_str());
    if(ifs.fail()){
      ifs.clear();
      ifs.open((string(getenv("HOME"))+legacy).c_str());
    }
  }

  bool HighScores::Qualifies(int score){
    stable_sort(begin(),end());
    return begin()->Score < score;
  }

  int HighScores::InsertHighScore(int score, const std::string &scorer){
    if(!Qualifies(score)) return -1;
    HighScore hs={score,scorer};
    insert(begin(),hs); //we insert at the beginning to resolve ties
    stable_sort(begin(),end()); //the dumbest way to do it
    erase(begin());
    iterator it=find(begin(),end(),hs); //the dumbest way to find the position
    return end()-it+1;
  }

  Config config; //singleton instance

  std::string Config::GetConfigFileName() const{
    return string(getenv("HOME"))+RcFileName;
  }

  std::string Config::GetHighScoresFileName() const{
    return string(getenv("HOME"))+LocalHighScoresFileName;
  }

  Config::Config(){
    po::options_description keyMappingOpts("Key mappings");
    keyMappingOpts.add_options()
      ("Down",po::value<int>()->default_value(KEY_DOWN),"Down key")
      ("Left",po::value<int>()->default_value(KEY_LEFT),"Left key")
      ("Right",po::value<int>()->default_value(KEY_RIGHT),"Right key")
      ("RotateCW",po::value<int>()->default_value(' '),"Clockwise turn key")
      ("RotateCCW",po::value<int>()->default_value(KEY_UP),"Counterclockwise turn key")
      ("Drop",po::value<int>()->default_value(KEY_ENTER),"Drop tetromino key")
      ("Pause",po::value<int>()->default_value('p'),"Pause key")
      ;

    po::options_description highScoresOpts("High scores");
    boost::format scorer("Scorer%02d%02d");
    boost::format score("Score%02d%02d");
    for(int difficulty=0;difficulty<num_difficulties;++difficulty)
      for(size_t i=0;i<HowManyHighScores;i++){
	highScoresOpts.add_options()
	  (str(scorer % difficulty % i).c_str(),po::value<string>()->default_value("No one played yet"),"Name of high scorer")
	  (str(score % difficulty % i).c_str(),po::value<int>()->default_value(0),"High score (points)")
	  ;
      }
    
    boost::program_options::variables_map _options;
    boost::program_options::variables_map _highScores;

    ifstream ifs;
    OpenWithFallback(ifs,GetConfigFileName(),LegacyRcFileName);
    po::store(po::parse_config_file(ifs,keyMappingOpts),_options);

    _keys.Down=_options["Down"].as<int>();
    _keys.Left=_options["Left"].as<int>();
    _keys.Right=_options["Right"].as<int>();
    _keys.RotateCW=_options["RotateCW"].as<int>();
    _keys.RotateCCW=_options["RotateCCW"].as<int>();
    _keys.Drop=_options["Drop"].as<int>();
    _keys.Pause=_options["Pause"].as<int>();

    ifstream ifs2;
    OpenWithFallback(ifs2,GetHighScoresFileName(),LegacyHighScoresFileName);
    po::store(po::parse_config_file(ifs2,highScoresOpts),_highScores);
    
    for(int difficulty=0;difficulty<num_difficulties;difficulty++){
      for(size_t i=0;i<HowManyHighScores;++i){
	string name=_highScores[str(scorer % difficulty % i)].as<string>();
	if(name.size()>=2 && name[0]=='"' && name[name.size()-1]=='"') //old files saved names with literal quotes
	  name=name.substr(1,name.size()-2);
	_hs[difficulty].push_back((HighScore){_highScores[str(score % difficulty % i)].as<int>(),name});
      }
    stable_sort(_hs[difficulty].begin(),_hs[difficulty].end()); //should not be needed but...
    }
  }
  
  Keys *Config::GetKeys(){
    return &_keys;
  }
  
  HighScores *Config::GetHighScores(int difficulty){
    assert(difficulty>=0 && difficulty<num_difficulties);
    return &(_hs[difficulty]);
  }
  
  Config::~Config(){
    /**
       The config and highscore files are written down at each game, even if it is not needed, but who cares for now
     */
    boost::program_options::variables_map _options;

    _options.insert(make_pair("Down",po::variable_value(_keys.Down,false)));
    _options.insert(make_pair("Left",po::variable_value(_keys.Left,false)));
    _options.insert(make_pair("Right",po::variable_value(_keys.Right,false)));
    _options.insert(make_pair("RotateCW",po::variable_value(_keys.RotateCW,false)));
    _options.insert(make_pair("RotateCCW",po::variable_value(_keys.RotateCCW,false)));
    _options.insert(make_pair("Drop",po::variable_value(_keys.Drop,false)));
    _options.insert(make_pair("Pause",po::variable_value(_keys.Pause,false)));

    ofstream ofs(GetConfigFileName().c_str());
    ofs<<"# Automatically regenerated by the program at each run, edit at your own risk\n";
    BOOST_FOREACH(const po::variables_map::value_type &pair, _options){
      ofs<<pair.first<<" = "<<pair.second.as<int>()<<"\n";
    }

    ofstream ofs2(GetHighScoresFileName().c_str());
    ofs2<<"# Do not edit this file, dqtris sees you\n";

    boost::format scorer("Scorer%02d%02d");
    boost::format score("Score%02d%02d");

    for(int difficulty=0;difficulty<num_difficulties;++difficulty){
      int i=0;
      BOOST_FOREACH(const HighScore &hs, _hs[difficulty]){
	ofs2<<str(scorer % difficulty % i) << " = " << hs.Scorer << "\n";
	ofs2<<str(score % difficulty % i) << " = " << hs.Score <<"\n";
	i++;
      }
    }
  }
}
