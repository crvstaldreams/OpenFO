#include "ordered_batch.hpp"
#include <cassert>
#include <iostream>
#include <vector>
int main(){ofo::OrderedBatch<int,int> b(6);std::vector<int> keys,out;auto emit=[&](int k,const int*p,size_t n){keys.push_back(k);out.insert(out.end(),p,p+n);};int a[]={0,1,2,3,4,5,6,7,8};b.append(1,a,3,emit);b.append(1,a+3,3,emit);b.append(2,a+6,3,emit);b.append(1,a,9,emit);b.flush(emit);assert((keys==std::vector<int>{1,2,1,1}));assert((out==std::vector<int>{0,1,2,3,4,5,6,7,8,0,1,2,3,4,5,6,7,8}));auto n=out.size();b.flush(emit);assert(out.size()==n);bool bad=false;try{b.append(0,a,1,emit);}catch(...){bad=true;}assert(bad);std::cout<<"Ordered UI batches preserve painter order across texture, capacity and frame boundaries\n";}
