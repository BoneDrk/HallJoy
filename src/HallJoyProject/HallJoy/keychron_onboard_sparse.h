// Coherent sparse full-state telemetry. Omitted slots are released, not unchanged.
#pragma once
#include "keychron_hj_protocol.h"
#include <string.h>
#define HJK4_SPARSE_RECORDS 5u
#define HJK4_SPARSE_PAGES ((HJK4_SLOTS+HJK4_SPARSE_RECORDS-1)/HJK4_SPARSE_RECORDS)
#define HJK4_SPARSE_LAST 2u
static inline uint16_t hjk4_sparse_crc(const uint8_t *p) {
    uint16_t crc=0xffff;
    for(unsigned i=0;i<30;++i) {
        crc^=(uint16_t)p[i]<<8;
        for(unsigned b=0;b<8;++b) crc=(uint16_t)((crc<<1)^((crc&0x8000)?0x1021:0));
    }
    return crc;
}
// Cursor belongs to one frozen snapshot; at most five nonzero records per page.
static inline void hjk4_sparse_encode(uint8_t *p,const uint16_t *depth,unsigned *cursor,
    uint8_t page,uint32_t session,uint32_t sequence,uint8_t calibrated) {
    memset(p,0,32);p[0]=0xa9;p[1]=0x7e;p[2]=1;p[3]=calibrated?1:0;
    hjk4_put32(p+4,session);hjk4_put32(p+8,sequence);p[12]=page;
    while(*cursor<HJK4_SLOTS && p[13]<HJK4_SPARSE_RECORDS) {
        const unsigned slot=(*cursor)++;
        if(depth[slot]) {
            uint8_t *entry=p+14+3*p[13]++;
            entry[0]=(uint8_t)slot;hjk4_put16(entry+1,depth[slot]);
        }
    }
    while(*cursor<HJK4_SLOTS && !depth[*cursor])++*cursor;
    if(*cursor==HJK4_SLOTS)p[3]|=HJK4_SPARSE_LAST;
    hjk4_put16(p+30,hjk4_sparse_crc(p));
}
typedef struct hjk4_sparse_assembly {
    uint16_t depth[HJK4_SLOTS];
    uint32_t session,sequence;
    unsigned next_page,next_slot;
    uint8_t complete;
} hjk4_sparse_assembly;
// -1 invalid, 0 incomplete, 1 complete. Discard assembler after any error.
// Caller publishes depth only at 1, after session/sequence validation.
static inline int hjk4_sparse_append(hjk4_sparse_assembly *a,const uint8_t *p) {
    if(a->complete || a->next_page>=HJK4_SPARSE_PAGES ||
        p[0]!=0xa9 || p[1]!=0x7e || p[2]!=1 || !(p[3]&1) || (p[3]&~3u) ||
        p[12]!=a->next_page || p[13]>HJK4_SPARSE_RECORDS || p[29] ||
        (!(p[3]&HJK4_SPARSE_LAST) && p[13]!=HJK4_SPARSE_RECORDS) ||
        (a->next_page && !p[13]) || !hjk4_u32(p+4) ||
        hjk4_u16(p+30)!=hjk4_sparse_crc(p))return -1;
    if(!a->next_page) {a->session=hjk4_u32(p+4);a->sequence=hjk4_u32(p+8);}
    if(a->session!=hjk4_u32(p+4) || a->sequence!=hjk4_u32(p+8))return -1;
    for(unsigned i=0;i<p[13];++i) {
        const uint8_t *entry=p+14+3*i;const unsigned slot=entry[0];
        if(slot<a->next_slot || slot>=HJK4_SLOTS || !hjk4_u16(entry+1))return -1;
        a->depth[slot]=hjk4_u16(entry+1);a->next_slot=slot+1;
    }
    for(unsigned i=14+3*p[13];i<29;++i)if(p[i])return -1;
    ++a->next_page;a->complete=(p[3]&HJK4_SPARSE_LAST)!=0;
    return a->complete?1:0;
}
