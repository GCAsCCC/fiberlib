#ifndef COMM_H
#define COMM_H

#include<iostream>

#define FIBER_STACK_SIZE_DEFAULT 1024*1024 //128字节

/// 协程总数
uint64_t s_fiber_count=0;
/// 当前已用协程id号
uint64_t s_fiber_id=0;

#endif