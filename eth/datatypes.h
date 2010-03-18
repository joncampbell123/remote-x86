
#ifndef DATATYPES_H
#define DATATYPES_H

#if defined(__i386__)
typedef unsigned char		uint8_t;
typedef signed char		int8_t;
typedef unsigned short		uint16_t;
typedef signed short		int16_t;
typedef unsigned int		uint32_t;
typedef signed int		int32_t;
typedef unsigned long long	uint64_t;
typedef signed long long	int64_t;
typedef uint32_t		size_t;
#elif defined(__amd64__)
#endif

/* code here needs "NULL" */
#define NULL ((void*)0)

/* in GCC the memcpy() function is built-in */
void *memcpy(void *dst,const void *src,size_t c);
void *memset(void *dst,int c,size_t l);

#endif

