#include "bounded_cpu_jobs.hpp"
#include <iostream>
#include <array>
#include <stdexcept>
using namespace ofo;
void need(bool b,const char*m){if(!b)throw std::runtime_error(m);}
int main(){try{
 need(bounded_cpu_for(0,[](size_t){throw std::runtime_error("empty");})==0,"empty does not run");
 std::vector<size_t>values(500);std::array<std::atomic<int>,500>visits{};std::atomic<int>active{0},peak{0};auto caller=std::this_thread::get_id();
 auto workers=bounded_cpu_for(values.size(),[&](size_t i){int n=++active,p=peak.load();while(p<n&&!peak.compare_exchange_weak(p,n)){}values[i]=i*i;++visits[i];--active;},3);
 need(workers<=3&&peak<=3,"worker count is bounded");for(size_t i=0;i<values.size();++i)need(values[i]==i*i&&visits[i]==1,"ordered disjoint output exactly once");
 bounded_cpu_for(5,[&](size_t){need(std::this_thread::get_id()==caller,"serial runs on caller");},1);
 bounded_cpu_for(8,[&](size_t){auto id=std::this_thread::get_id();need(bounded_cpu_for(16,[&](size_t){need(std::this_thread::get_id()==id,"nested calls stay serial");},8)==1,"nested worker count");},4);
 std::atomic<int>completed{0};bool failed=false;try{bounded_cpu_for(100,[&](size_t i){++completed;if(i==7||i==2)throw std::runtime_error(std::to_string(i));},4);}catch(const std::runtime_error&e){failed=std::string(e.what())=="2";}
 need(failed&&completed==100,"workers join and lowest-index exception reaches caller");
 completed=0;failed=false;try{bounded_cpu_for(10,[&](size_t i){++completed;if(i==2)throw std::runtime_error("serial failure");},1);}catch(const std::runtime_error&){failed=true;}need(failed&&completed==10,"serial fallback reports exceptions after completion");
 std::cout<<"bounded CPU jobs: limits, ordered outputs, serial/nested fallback and joined exceptions passed\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
