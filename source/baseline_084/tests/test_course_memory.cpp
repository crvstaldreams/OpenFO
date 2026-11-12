#include "course_ui.hpp"
#include <cstdlib>
#include <new>
#include <cassert>
#include <iostream>
static bool measure=false;static size_t allocations=0;
void*operator new(size_t n){if(measure)++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}void operator delete(void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}
int main(){using namespace ofo;Json j={{"points",{{0,0,0},{0,0,200},{200,0,200},{200,0,0}}}};CourseDirector r(j,1,2,0);CoursePose p;p.chassis[14]=80;measure=true;for(int i=0;i<1000;i++){auto g=r.guidance(0,p);assert(g.available&&std::abs(g.distance-20)<1e-6);}measure=false;std::cout<<"guidance allocations "<<allocations<<std::endl;assert(allocations==0);}
