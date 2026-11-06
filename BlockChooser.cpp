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

#include "BlockChooser.hpp"

#include <cstdlib> //random
#include <algorithm>
#include <random>

namespace Bastet{

  BlockChooser::BlockChooser(){}
  BlockChooser::~BlockChooser(){}

  RandomBlockChooser::RandomBlockChooser(){}
  RandomBlockChooser::~RandomBlockChooser(){}
  
  Queue RandomBlockChooser::GetStartingQueue(){
    Queue q;
    q.push_back(BlockType(random()%nBlockTypes));
    q.push_back(BlockType(random()%nBlockTypes));
    return q;
  }
  
  BlockType RandomBlockChooser::GetNext(const Well *well, const Queue &q){
    return BlockType(random()%nBlockTypes);
  }

  BagBlockChooser::BagBlockChooser(){}
  BagBlockChooser::~BagBlockChooser(){}

  BlockType BagBlockChooser::Draw(){
    if(_bag.empty()){
      static std::mt19937 rng(random());
      for(int i=0;i<int(nBlockTypes);++i)
        _bag.push_back(BlockType(i));
      std::shuffle(_bag.begin(),_bag.end(),rng);
    }
    BlockType b=_bag.front();
    _bag.pop_front();
    return b;
  }

  Queue BagBlockChooser::GetStartingQueue(){
    Queue q;
    for(size_t i=0;i<=Previews;++i) //the current block plus the previews
      q.push_back(Draw());
    return q;
  }

  BlockType BagBlockChooser::GetNext(const Well *well, const Queue &q){
    return Draw();
  }

}
