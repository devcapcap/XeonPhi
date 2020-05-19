#ifndef _COMMON_H_
#define _COMMON_H_

#define ACCESS_ONCE(x) (*(volatile typeof(x)*)&(x))

#endif